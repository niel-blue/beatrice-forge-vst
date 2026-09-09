// Copyright (c) 2026 Project Beatrice and Contributors

#include "common/application_input.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <cctype>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <future>
#include <map>
#include <mutex>
#include <ranges>
#include <thread>
#include <utility>

#include "common/stereo_audio_fifo.h"

#ifdef _WIN32

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include <Audioclient.h>
#include <audioclientactivationparams.h>
#include <audiopolicy.h>
#include <mmdeviceapi.h>
#include <wrl/client.h>
#include <wrl/implements.h>

#endif

namespace beatrice::common {

namespace {

auto LowerAscii(std::string value) -> std::string {
  std::ranges::transform(value, value.begin(), [](const unsigned char value) {
    return static_cast<char>(std::tolower(value));
  });
  return value;
}

auto IsExcludedApplication(const ApplicationInputExclusions& exclusions,
                          const std::uint32_t process_id,
                          const std::string_view identity) -> bool {
  if (std::ranges::find(exclusions.process_ids, process_id) !=
      exclusions.process_ids.end()) {
    return true;
  }
  const auto normalized_identity = LowerAscii(std::string(identity));
  return std::ranges::any_of(
      exclusions.identities, [&normalized_identity](const auto& excluded) {
        return !normalized_identity.empty() &&
               normalized_identity == LowerAscii(excluded);
      });
}

#ifdef _WIN32

using Microsoft::WRL::ComPtr;

auto WideToUtf8(const std::wstring& text) -> std::string {
  if (text.empty()) {
    return {};
  }
  const auto size = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS,
                                        text.data(),
                                        static_cast<int>(text.size()), nullptr,
                                        0, nullptr, nullptr);
  if (size <= 0) {
    return {};
  }
  auto result = std::string(static_cast<std::size_t>(size), '\0');
  WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, text.data(),
                      static_cast<int>(text.size()), result.data(), size,
                      nullptr, nullptr);
  return result;
}

auto HResultText(const char* const operation, const HRESULT result)
    -> std::string {
  return std::string(operation) + " failed (0x" +
         [&]() {
           char buffer[16] = {};
           std::snprintf(buffer, sizeof(buffer), "%08lX",
                         static_cast<unsigned long>(result));
           return std::string(buffer);
         }() + ").";
}

auto ProcessImagePath(const DWORD process_id) -> std::wstring {
  const auto process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE,
                                   process_id);
  if (process == nullptr) {
    return {};
  }
  auto path = std::wstring(32768, L'\0');
  auto size = static_cast<DWORD>(path.size());
  const auto success = QueryFullProcessImageNameW(process, 0, path.data(),
                                                   &size) != FALSE;
  CloseHandle(process);
  if (!success) {
    return {};
  }
  path.resize(size);
  return path;
}

auto ProcessDisplayName(const std::wstring& image_path,
                        const std::string& fallback) -> std::string {
  if (!image_path.empty()) {
    const auto name = std::filesystem::path(image_path).filename().wstring();
    const auto utf8 = WideToUtf8(name);
    if (!utf8.empty()) {
      return utf8;
    }
  }
  return fallback.empty() ? "Unknown application" : fallback;
}

#endif

}  // namespace

auto CurrentApplicationInputIdentity() -> std::string {
#ifdef _WIN32
  return LowerAscii(WideToUtf8(ProcessImagePath(GetCurrentProcessId())));
#else
  return {};
#endif
}

auto CurrentApplicationInputProcessId() -> std::uint32_t {
#ifdef _WIN32
  return static_cast<std::uint32_t>(GetCurrentProcessId());
#else
  return 0;
#endif
}

auto CurrentApplicationInputExclusions() -> ApplicationInputExclusions {
  auto exclusions = ApplicationInputExclusions{};
  const auto process_id = CurrentApplicationInputProcessId();
  if (process_id != 0) {
    exclusions.process_ids.push_back(process_id);
  }
  const auto identity = CurrentApplicationInputIdentity();
  if (!identity.empty()) {
    exclusions.identities.push_back(identity);
  }
  return exclusions;
}

auto EnumerateApplicationInputs(const ApplicationInputExclusions& exclusions)
    -> ApplicationInputSnapshot {
#ifdef _WIN32
  const auto com_result = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
  // A host UI thread may already be initialized as an STA. In that case
  // COM reports RPC_E_CHANGED_MODE even though the thread is still a valid
  // COM caller. Respect the host's apartment and do not uninitialize it.
  if (FAILED(com_result) && com_result != RPC_E_CHANGED_MODE) {
    return {.error = HResultText("Initializing Windows audio", com_result)};
  }
  const auto uninitialize = [should_uninitialize = SUCCEEDED(com_result)]() {
    if (should_uninitialize) {
      CoUninitialize();
    }
  };

  auto result = ApplicationInputSnapshot{};
  ComPtr<IMMDeviceEnumerator> enumerator;
  auto hresult = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr,
                                  CLSCTX_ALL, IID_PPV_ARGS(&enumerator));
  if (FAILED(hresult)) {
    uninitialize();
    result.error = HResultText("Creating the audio device registry", hresult);
    return result;
  }

  ComPtr<IMMDeviceCollection> devices;
  hresult = enumerator->EnumAudioEndpoints(eRender, DEVICE_STATE_ACTIVE,
                                           &devices);
  if (FAILED(hresult)) {
    uninitialize();
    result.error = HResultText("Enumerating audio output devices", hresult);
    return result;
  }

  std::map<DWORD, ApplicationInputInfo> processes;
  UINT device_count = 0;
  static_cast<void>(devices->GetCount(&device_count));
  for (UINT device_index = 0; device_index < device_count; ++device_index) {
    ComPtr<IMMDevice> device;
    if (FAILED(devices->Item(device_index, &device))) {
      continue;
    }
    ComPtr<IAudioSessionManager2> manager;
    if (FAILED(device->Activate(__uuidof(IAudioSessionManager2), CLSCTX_ALL,
                               nullptr,
                               reinterpret_cast<void**>(manager.GetAddressOf())))) {
      continue;
    }
    ComPtr<IAudioSessionEnumerator> sessions;
    if (FAILED(manager->GetSessionEnumerator(&sessions))) {
      continue;
    }
    int session_count = 0;
    if (FAILED(sessions->GetCount(&session_count))) {
      continue;
    }
    for (int session_index = 0; session_index < session_count;
         ++session_index) {
      ComPtr<IAudioSessionControl> control;
      if (FAILED(sessions->GetSession(session_index, &control))) {
        continue;
      }
      AudioSessionState state = AudioSessionStateInactive;
      if (FAILED(control->GetState(&state)) ||
          state == AudioSessionStateExpired) {
        continue;
      }
      ComPtr<IAudioSessionControl2> control2;
      if (FAILED(control.As(&control2))) {
        continue;
      }
      DWORD process_id = 0;
      if (FAILED(control2->GetProcessId(&process_id)) || process_id == 0) {
        continue;
      }
      const auto image_path = ProcessImagePath(process_id);
      std::string session_name;
      LPWSTR display_name = nullptr;
      if (SUCCEEDED(control->GetDisplayName(&display_name)) &&
          display_name != nullptr) {
        session_name = WideToUtf8(display_name);
        CoTaskMemFree(display_name);
      }
      const auto display = ProcessDisplayName(image_path, session_name);
      auto identity = LowerAscii(WideToUtf8(image_path));
      if (identity.empty()) {
        identity = LowerAscii(display);
      }
      if (IsExcludedApplication(exclusions, process_id, identity)) {
        continue;
      }
      const auto active = state == AudioSessionStateActive;
      auto [entry, inserted] = processes.emplace(
          process_id,
          ApplicationInputInfo{identity, display, process_id, active});
      if (!inserted && entry->second.display_name == "Unknown application" &&
          display != "Unknown application") {
        entry->second.identity = identity;
        entry->second.display_name = display;
      }
      if (!inserted) {
        entry->second.active = entry->second.active || active;
      }
    }
  }
  uninitialize();
  for (auto& [process_id, info] : processes) {
    static_cast<void>(process_id);
    result.applications.push_back(std::move(info));
  }
  std::ranges::sort(result.applications,
                    [](const auto& left, const auto& right) {
                      return LowerAscii(left.display_name) <
                             LowerAscii(right.display_name);
                    });
  return result;
#else
  return {.error = "Application Input is available on Windows only."};
#endif
}

auto MergeRememberedApplicationInput(
    ApplicationInputSnapshot snapshot, const std::string_view identity,
    const ApplicationInputExclusions& exclusions) -> ApplicationInputSnapshot {
  if (identity.empty()) {
    return snapshot;
  }
  const auto normalized_identity = LowerAscii(std::string(identity));
  if (IsExcludedApplication(exclusions, 0, normalized_identity) ||
      std::ranges::any_of(snapshot.applications, [&normalized_identity](
                              const auto& application) {
        return LowerAscii(application.identity) == normalized_identity;
      })) {
    return snapshot;
  }
  auto display_name = normalized_identity;
  if (const auto separator = display_name.find_last_of("\\/");
      separator != std::string::npos && separator + 1U < display_name.size()) {
    display_name.erase(0, separator + 1U);
  }
  snapshot.applications.push_back(
      {.identity = normalized_identity,
       .display_name = std::move(display_name),
       .process_id = 0,
       .active = false});
  std::ranges::sort(snapshot.applications,
                    [](const auto& left, const auto& right) {
                      return LowerAscii(left.display_name) <
                             LowerAscii(right.display_name);
                    });
  return snapshot;
}

auto FindApplicationInput(const ApplicationInputSnapshot& snapshot,
                          const std::string& identity)
    -> std::optional<ApplicationInputInfo> {
  if (identity.empty()) {
    return std::nullopt;
  }
  const auto normalized = LowerAscii(identity);
  for (const auto& application : snapshot.applications) {
    if (LowerAscii(application.identity) == normalized) {
      return application;
    }
  }
  return std::nullopt;
}

class ApplicationInput::Impl final {
 public:
  explicit Impl(StatusCallback callback) : callback_(std::move(callback)) {}
  ~Impl() { Stop(); }

  auto Start(const std::uint32_t process_id, const double output_sample_rate)
      -> bool {
    Stop();
    if (process_id == 0 || !std::isfinite(output_sample_rate) ||
        output_sample_rate <= 0.0) {
      SetUnavailable("Select an active audio application.");
      return false;
    }
#ifdef _WIN32
    stop_event_ = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    if (stop_event_ == nullptr) {
      SetUnavailable("The Application Input stop event could not be created.");
      return false;
    }
    fifo_.DiscardAll();
    fifo_.ResetStats();
    resampler_ready_ = false;
    recovery_pending_ = false;
    control_initialized_ = false;
    control_frame_accumulator_ = 0;
    ratio_integral_ = 0.0;
    nominal_resampler_ratio_ = 1.0;
    resampler_ratio_ = 1.0;
    target_fifo_frames_ = 0;
    playback_started_ = false;
    recovery_armed_ = false;
    handled_underflow_count_ = 0;
    handled_overflow_count_ = 0;
    handled_discontinuity_count_ = 0;
    discontinuity_count_.store(0, std::memory_order_release);
    target_sample_rate_ = output_sample_rate;
    SetStatus(ApplicationInputStatus::kStarting, {});
    auto promise = std::promise<bool>{};
    auto future = promise.get_future();
    worker_ = std::thread(&Impl::Run, this, process_id, std::move(promise));
    const auto success = future.get();
    if (!success) {
      if (worker_.joinable()) {
        worker_.join();
      }
      CloseHandle(stop_event_);
      stop_event_ = nullptr;
      return false;
    }
    return true;
#else
    static_cast<void>(output_sample_rate);
    SetUnavailable("Application Input is available on Windows only.");
    return false;
#endif
  }

  void Stop() {
#ifdef _WIN32
    if (stop_event_ != nullptr) {
      SetEvent(stop_event_);
    }
    if (worker_.joinable()) {
      worker_.join();
    }
    if (stop_event_ != nullptr) {
      CloseHandle(stop_event_);
      stop_event_ = nullptr;
    }
#endif
    fifo_.DiscardAll();
    resampler_ready_ = false;
    recovery_pending_ = false;
    control_initialized_ = false;
    control_frame_accumulator_ = 0;
    ratio_integral_ = 0.0;
    nominal_resampler_ratio_ = 1.0;
    resampler_ratio_ = 1.0;
    target_fifo_frames_ = 0;
    playback_started_ = false;
    recovery_armed_ = false;
    handled_underflow_count_ = 0;
    handled_overflow_count_ = 0;
    handled_discontinuity_count_ = 0;
    SetStatus(ApplicationInputStatus::kOff, {});
  }

  auto Read(float* const left, float* const right, const std::size_t count)
      noexcept -> std::size_t {
    if (left == nullptr || count == 0) {
      return 0;
    }
    const auto active = GetStatus() == ApplicationInputStatus::kActive;
    if (active && recovery_pending_) {
      if (fifo_.Size() >= target_fifo_frames_) {
        recovery_pending_ = false;
        resampler_ready_ = false;
        resampler_position_ = 0.0;
        ratio_integral_ = 0.0;
        resampler_ratio_ = nominal_resampler_ratio_;
        smoothed_fifo_frames_ = static_cast<double>(fifo_.Size());
        control_frame_accumulator_ = 0;
      } else {
        std::fill_n(left, count, 0.0F);
        if (right != nullptr) {
          std::fill_n(right, count, 0.0F);
        }
        return 0;
      }
    }
    auto received = std::size_t{0};
    for (auto index = std::size_t{0}; index < count; ++index) {
      auto value_left = 0.0F;
      auto value_right = 0.0F;
      if (active && ReadResampled(value_left, value_right)) {
        ++received;
      }
      left[index] = value_left;
      if (right != nullptr) {
        right[index] = value_right;
      }
    }
    if (received != 0) {
      playback_started_ = true;
    }
    if (active) {
      UpdateClockControl(count);
      const auto underflows = fifo_.UnderrunCount();
      const auto overflows = fifo_.OverflowCount();
      const auto discontinuities =
          discontinuity_count_.load(std::memory_order_acquire);
      // The first empty reads are expected while the process-loopback stream
      // is opening. They must not trigger the normal recovery path, otherwise
      // the recovery target would reintroduce an artificial startup delay.
      if (!recovery_armed_ && target_fifo_frames_ != 0 &&
          fifo_.Size() >= target_fifo_frames_) {
        recovery_armed_ = true;
        handled_underflow_count_ = underflows;
        handled_overflow_count_ = overflows;
        handled_discontinuity_count_ = discontinuities;
      }
      if (playback_started_ && recovery_armed_ &&
          (underflows != handled_underflow_count_ ||
           overflows != handled_overflow_count_ ||
           discontinuities != handled_discontinuity_count_)) {
        BeginRecovery(overflows != handled_overflow_count_ ||
                      discontinuities != handled_discontinuity_count_);
        handled_underflow_count_ = underflows;
        handled_overflow_count_ = overflows;
        handled_discontinuity_count_ = discontinuities;
      }
    }
    return received;
  }

  [[nodiscard]] auto GetStatus() const noexcept -> ApplicationInputStatus {
    return status_.load(std::memory_order_acquire);
  }

  [[nodiscard]] auto GetLastError() const -> std::string {
    const auto lock = std::lock_guard(error_mutex_);
    return error_;
  }

 private:
  void SetStatus(const ApplicationInputStatus status, std::string error) {
    {
      const auto lock = std::lock_guard(error_mutex_);
      error_ = std::move(error);
    }
    status_.store(status, std::memory_order_release);
    if (callback_) {
      callback_(status, GetLastError());
    }
  }

  void SetUnavailable(std::string error) {
    SetStatus(ApplicationInputStatus::kUnavailable, std::move(error));
  }

  void BeginRecovery(const bool discard_fifo) noexcept {
    if (discard_fifo) {
      fifo_.DiscardAll();
    }
    resampler_ready_ = false;
    resampler_position_ = 0.0;
    ratio_integral_ = 0.0;
    resampler_ratio_ = nominal_resampler_ratio_;
    smoothed_fifo_frames_ = static_cast<double>(fifo_.Size());
    control_frame_accumulator_ = 0;
    recovery_pending_ = true;
  }

  void UpdateClockControl(const std::size_t output_frames) noexcept {
    if (!control_initialized_) {
      smoothed_fifo_frames_ = static_cast<double>(fifo_.Size());
      control_initialized_ = true;
    }
    if (target_sample_rate_ <= 0.0 || source_sample_rate_ <= 0.0 ||
        target_fifo_frames_ == 0) {
      return;
    }
    control_frame_accumulator_ += output_frames;
    const auto interval_frames = static_cast<std::size_t>(std::max(
        1.0, std::round(target_sample_rate_ * kClockControlIntervalSeconds)));
    if (control_frame_accumulator_ < interval_frames) {
      return;
    }
    const auto elapsed_frames = control_frame_accumulator_;
    control_frame_accumulator_ = 0;
    const auto elapsed_seconds =
        static_cast<double>(elapsed_frames) / target_sample_rate_;
    const auto occupancy = static_cast<double>(fifo_.Size());
    const auto smoothing = elapsed_seconds /
                           (kFifoSmoothingTimeSeconds + elapsed_seconds);
    smoothed_fifo_frames_ +=
        std::clamp(smoothing, 0.01, 1.0) *
        (occupancy - smoothed_fifo_frames_);
    const auto error_seconds =
        (smoothed_fifo_frames_ - static_cast<double>(target_fifo_frames_)) /
        source_sample_rate_;
    const auto proportional = kClockProportionalGain * error_seconds;
    const auto integral_step =
        kClockIntegralGain * error_seconds * elapsed_seconds;
    const auto candidate_integral = std::clamp(
        ratio_integral_ + integral_step, -kMaxIntegralCorrection,
        kMaxIntegralCorrection);
    const auto candidate_correction = proportional + candidate_integral;
    const auto saturated_high = candidate_correction > kMaxCorrection &&
                                error_seconds > 0.0;
    const auto saturated_low = candidate_correction < -kMaxCorrection &&
                               error_seconds < 0.0;
    if (!saturated_high && !saturated_low) {
      ratio_integral_ = candidate_integral;
    }
    const auto correction = std::clamp(
        proportional + ratio_integral_, -kMaxCorrection, kMaxCorrection);
    resampler_ratio_ = nominal_resampler_ratio_ * (1.0 + correction);
  }

  auto ReadResampled(float& left, float& right) noexcept -> bool {
    if (!resampler_ready_) {
      if (!fifo_.Pop(resampler_current_left_, resampler_current_right_)) {
        return false;
      }
      if (!fifo_.Pop(resampler_next_left_, resampler_next_right_)) {
        resampler_next_left_ = resampler_current_left_;
        resampler_next_right_ = resampler_current_right_;
      }
      resampler_position_ = 0.0;
      resampler_ready_ = true;
    }
    left = static_cast<float>(resampler_current_left_ +
                              resampler_position_ *
                                  (resampler_next_left_ -
                                   resampler_current_left_));
    right = static_cast<float>(resampler_current_right_ +
                               resampler_position_ *
                                   (resampler_next_right_ -
                                    resampler_current_right_));
    resampler_position_ += resampler_ratio_;
    while (resampler_position_ >= 1.0) {
      resampler_current_left_ = resampler_next_left_;
      resampler_current_right_ = resampler_next_right_;
      if (!fifo_.Pop(resampler_next_left_, resampler_next_right_)) {
        resampler_next_left_ = resampler_current_left_;
        resampler_next_right_ = resampler_current_right_;
      }
      resampler_position_ -= 1.0;
    }
    return true;
  }

#ifdef _WIN32
  struct ActivationState {
    std::mutex mutex;
    std::condition_variable condition;
    bool complete = false;
    HRESULT result = E_FAIL;
    ComPtr<IAudioClient> client;
  };

  class ActivationHandler final
      : public Microsoft::WRL::RuntimeClass<
            Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>,
            Microsoft::WRL::FtmBase,
            IActivateAudioInterfaceCompletionHandler> {
   public:
    explicit ActivationHandler(std::shared_ptr<ActivationState> state)
        : state_(std::move(state)) {}

    auto STDMETHODCALLTYPE ActivateCompleted(
        IActivateAudioInterfaceAsyncOperation* const operation) -> HRESULT
        override {
      HRESULT result = E_FAIL;
      HRESULT activation_result = E_FAIL;
      ComPtr<IUnknown> activated;
      if (operation != nullptr) {
        result = operation->GetActivateResult(&activation_result, &activated);
        if (SUCCEEDED(result)) {
          result = activation_result;
        }
      }
      ComPtr<IAudioClient> client;
      if (SUCCEEDED(result) && activated != nullptr) {
        result = activated.As(&client);
      }
      {
        const auto lock = std::lock_guard(state_->mutex);
        state_->result = result;
        state_->client = std::move(client);
        state_->complete = true;
      }
      state_->condition.notify_one();
      return S_OK;
    }

   private:
    std::shared_ptr<ActivationState> state_;
  };

  static auto IsFloatFormat(const WAVEFORMATEX& format) -> bool {
    if (format.wFormatTag == WAVE_FORMAT_IEEE_FLOAT) {
      return format.wBitsPerSample == 32;
    }
    if (format.wFormatTag != WAVE_FORMAT_EXTENSIBLE ||
        format.cbSize < sizeof(WAVEFORMATEXTENSIBLE) - sizeof(WAVEFORMATEX)) {
      return false;
    }
    const auto& extensible = reinterpret_cast<const WAVEFORMATEXTENSIBLE&>(format);
    return extensible.SubFormat == KSDATAFORMAT_SUBTYPE_IEEE_FLOAT &&
           format.wBitsPerSample == 32;
  }

  static auto ReadSourceSample(const BYTE* data, const UINT32 frame,
                               const UINT32 channel,
                               const WAVEFORMATEX& format) noexcept -> float {
    if (data == nullptr || channel >= format.nChannels ||
        format.nBlockAlign == 0) {
      return 0.0F;
    }
    const auto bytes = static_cast<std::size_t>(format.wBitsPerSample / 8U);
    const auto* sample = data + static_cast<std::size_t>(frame) *
                                   format.nBlockAlign + channel * bytes;
    if (IsFloatFormat(format)) {
      float value = 0.0F;
      std::memcpy(&value, sample, sizeof(value));
      return value;
    }
    if (format.wBitsPerSample == 16) {
      std::int16_t value = 0;
      std::memcpy(&value, sample, sizeof(value));
      return static_cast<float>(value) / 32768.0F;
    }
    if (format.wBitsPerSample == 24) {
      const auto value = static_cast<std::int32_t>(sample[0]) |
                         (static_cast<std::int32_t>(sample[1]) << 8) |
                         (static_cast<std::int32_t>(sample[2]) << 16);
      const auto signed_value = (value & 0x00800000) != 0
                                    ? value | static_cast<std::int32_t>(0xff000000)
                                    : value;
      return static_cast<float>(signed_value) / 8388608.0F;
    }
    std::int32_t value = 0;
    std::memcpy(&value, sample, sizeof(value));
    return static_cast<float>(value) / 2147483648.0F;
  }

  void Run(const std::uint32_t process_id, std::promise<bool> started) {
    const auto com_result = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    if (FAILED(com_result)) {
      SetUnavailable(HResultText("Initializing Application Input", com_result));
      started.set_value(false);
      return;
    }
    auto client = ComPtr<IAudioClient>{};
    auto format = std::unique_ptr<WAVEFORMATEX, void (*)(WAVEFORMATEX*)>(
        nullptr, [](WAVEFORMATEX* value) { CoTaskMemFree(value); });
    auto capture = ComPtr<IAudioCaptureClient>{};
    auto audio_event = HANDLE{};
    const auto finish = [&]() {
      if (audio_event != nullptr) {
        CloseHandle(audio_event);
        audio_event = nullptr;
      }
      CoUninitialize();
    };

    const auto activation = std::make_shared<ActivationState>();
    auto handler = Microsoft::WRL::Make<ActivationHandler>(activation);
    if (handler == nullptr) {
      SetUnavailable("The Application Input activation handler could not be created.");
      started.set_value(false);
      finish();
      return;
    }
    AUDIOCLIENT_ACTIVATION_PARAMS params{};
    params.ActivationType = AUDIOCLIENT_ACTIVATION_TYPE_PROCESS_LOOPBACK;
    params.ProcessLoopbackParams.ProcessLoopbackMode =
        PROCESS_LOOPBACK_MODE_INCLUDE_TARGET_PROCESS_TREE;
    params.ProcessLoopbackParams.TargetProcessId = process_id;
    PROPVARIANT activation_params{};
    activation_params.vt = VT_BLOB;
    activation_params.blob.cbSize = sizeof(params);
    activation_params.blob.pBlobData = reinterpret_cast<BYTE*>(&params);
    ComPtr<IActivateAudioInterfaceAsyncOperation> operation;
    auto result = ActivateAudioInterfaceAsync(
        VIRTUAL_AUDIO_DEVICE_PROCESS_LOOPBACK, __uuidof(IAudioClient),
        &activation_params, handler.Get(), &operation);
    if (FAILED(result)) {
      SetUnavailable(HResultText("Starting Application Input", result));
      started.set_value(false);
      finish();
      return;
    }
    {
      auto lock = std::unique_lock(activation->mutex);
      if (!activation->condition.wait_for(
              lock, std::chrono::seconds(5),
              [&]() { return activation->complete; })) {
        SetUnavailable("Application Input activation timed out.");
        started.set_value(false);
        finish();
        return;
      }
      if (FAILED(activation->result) || activation->client == nullptr) {
        const auto error = HResultText("Activating Application Input",
                                       activation->result);
        started.set_value(false);
        finish();
        SetUnavailable(error);
        return;
      }
      client = activation->client;
    }
    // The process-loopback endpoint is a virtual mixer rather than a physical
    // device.  Its GetMixFormat implementation is unavailable or unreliable
    // on some Windows versions.  Use the format from Microsoft's process
    // loopback sample and let AUTOCONVERTPCM perform the engine conversion.
    auto* const raw_format = static_cast<WAVEFORMATEX*>(
        CoTaskMemAlloc(sizeof(WAVEFORMATEX)));
    if (raw_format == nullptr) {
      SetUnavailable("The Application Input format could not be allocated.");
      started.set_value(false);
      finish();
      return;
    }
    *raw_format = {};
    raw_format->wFormatTag = WAVE_FORMAT_PCM;
    raw_format->nChannels = 2;
    raw_format->nSamplesPerSec = 44100;
    raw_format->wBitsPerSample = 16;
    raw_format->nBlockAlign = static_cast<WORD>(
        raw_format->nChannels * raw_format->wBitsPerSample / 8U);
    raw_format->nAvgBytesPerSec =
        raw_format->nSamplesPerSec * raw_format->nBlockAlign;
    format.reset(raw_format);
    constexpr auto stream_flags = AUDCLNT_STREAMFLAGS_LOOPBACK |
                                  AUDCLNT_STREAMFLAGS_EVENTCALLBACK |
                                  AUDCLNT_STREAMFLAGS_AUTOCONVERTPCM;
    if (FAILED(result = client->Initialize(AUDCLNT_SHAREMODE_SHARED,
                                            stream_flags, 0, 0, format.get(),
                                            nullptr))) {
      SetUnavailable(HResultText("Initializing Application Input", result));
      started.set_value(false);
      finish();
      return;
    }
    audio_event = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    if (audio_event == nullptr) {
      SetUnavailable("The Application Input audio event could not be created.");
      started.set_value(false);
      finish();
      return;
    }
    // Keep the setup order aligned with Microsoft's process-loopback sample:
    // acquire the capture service before registering the event callback.
    if (FAILED(result = client->GetService(IID_PPV_ARGS(&capture))) ||
        FAILED(result = client->SetEventHandle(audio_event))) {
      SetUnavailable(HResultText("Preparing Application Input", result));
      started.set_value(false);
      finish();
      return;
    }
    UINT32 buffer_frames = 0;
    if (FAILED(result = client->GetBufferSize(&buffer_frames))) {
      SetUnavailable(HResultText("Reading Application Input buffer", result));
      started.set_value(false);
      finish();
      return;
    }
    source_sample_rate_ = static_cast<double>(format->nSamplesPerSec);
    nominal_resampler_ratio_ = source_sample_rate_ / target_sample_rate_;
    resampler_ratio_ = nominal_resampler_ratio_;
    target_fifo_frames_ = static_cast<std::size_t>(std::max(
        2.0, std::round(source_sample_rate_ * kTargetBufferSeconds)));
    if (!std::isfinite(nominal_resampler_ratio_) ||
        nominal_resampler_ratio_ <= 0.0) {
      SetUnavailable("The Application Input sample rate is invalid.");
      started.set_value(false);
      finish();
      return;
    }
    // Process-loopback clients can report zero here even though packets will
    // be delivered after Start(). Keep a small initial workspace and grow it
    // to the actual packet size below instead of treating zero as failure.
    const auto initial_buffer_frames = std::max<UINT32>(buffer_frames, 4096U);
    capture_left_.assign(initial_buffer_frames, 0.0F);
    capture_right_.assign(initial_buffer_frames, 0.0F);
    if (FAILED(result = client->Start())) {
      SetUnavailable(HResultText("Starting Application Input", result));
      started.set_value(false);
      finish();
      return;
    }
    // Do not wait for the steady-state FIFO target here. The first audible
    // sample should start as soon as the process-loopback stream can provide
    // it; the target is only used by the clock-recovery controller afterwards.
    SetStatus(ApplicationInputStatus::kActive, {});
    started.set_value(true);
    auto handles = std::array<HANDLE, 2>{stop_event_, audio_event};
    while (true) {
      const auto wait = WaitForMultipleObjects(
          static_cast<DWORD>(handles.size()), handles.data(), FALSE, 10);
      if (wait == WAIT_OBJECT_0) {
        break;
      }
      // The normal path is the audio event. Some Windows audio endpoints
      // activate process-loopback successfully but do not reliably signal
      // that event until a later state change. Polling the capture queue at a
      // short interval keeps the input usable on those endpoints while the
      // event remains the low-overhead notification path when it works.
      if (wait != WAIT_OBJECT_0 + 1U && wait != WAIT_TIMEOUT) {
        SetUnavailable("The Application Input wait failed.");
        break;
      }
      UINT32 packet_frames = 0;
      result = capture->GetNextPacketSize(&packet_frames);
      while (SUCCEEDED(result) && packet_frames > 0) {
        BYTE* data = nullptr;
        UINT32 frames = 0;
        DWORD flags = 0;
        [[maybe_unused]] UINT64 device_position = 0;
        [[maybe_unused]] UINT64 qpc_position = 0;
        result = capture->GetBuffer(&data, &frames, &flags,
                                    &device_position, &qpc_position);
        if (FAILED(result)) {
          break;
        }
        // ReleaseBuffer must receive the complete packet size returned by
        // GetBuffer, even when the temporary conversion buffer is smaller.
        // Keeping the two counts separate prevents a partial release from
        // leaving the same packet at the head of the capture stream.
        const auto packet_frame_count = frames;
        if ((flags & AUDCLNT_BUFFERFLAGS_DATA_DISCONTINUITY) != 0) {
          discontinuity_count_.fetch_add(1, std::memory_order_release);
        }
        if (capture_left_.size() < packet_frame_count) {
          capture_left_.resize(packet_frame_count);
          capture_right_.resize(packet_frame_count);
        }
        const auto frames_to_copy = packet_frame_count;
        const auto silent = (flags & AUDCLNT_BUFFERFLAGS_SILENT) != 0;
        for (UINT32 frame = 0; frame < frames_to_copy; ++frame) {
          capture_left_[frame] = silent
                                     ? 0.0F
                                     : ReadSourceSample(data, frame, 0, *format);
          capture_right_[frame] = silent
                                      ? 0.0F
                                      : ReadSourceSample(data, frame,
                                                         format->nChannels > 1
                                                             ? 1U
                                                             : 0U,
                                                         *format);
        }
        fifo_.PushBlock(capture_left_.data(), capture_right_.data(),
                        frames_to_copy);
        result = capture->ReleaseBuffer(packet_frame_count);
        if (FAILED(result)) {
          break;
        }
        result = capture->GetNextPacketSize(&packet_frames);
      }
      if (FAILED(result)) {
        SetUnavailable(HResultText("Reading Application Input", result));
        break;
      }
    }
    client->Stop();
    if (GetStatus() == ApplicationInputStatus::kActive) {
      SetStatus(ApplicationInputStatus::kOff, {});
    }
    finish();
  }
#endif

  StatusCallback callback_;
  StereoAudioFifo fifo_{48000U * 4U};
  std::atomic<ApplicationInputStatus> status_ = ApplicationInputStatus::kOff;
  mutable std::mutex error_mutex_;
  std::string error_;
#ifdef _WIN32
  std::thread worker_;
  HANDLE stop_event_ = nullptr;
  std::vector<float> capture_left_;
  std::vector<float> capture_right_;
#endif
  double target_sample_rate_ = 48000.0;
  double source_sample_rate_ = 48000.0;
  // Keep the startup and steady-state queue deliberately small.  This is a
  // real-time input path, so a large target would turn clock recovery into a
  // permanent extra delay before the first audible sample.
  static constexpr auto kTargetBufferSeconds = 0.010;
  static constexpr auto kClockControlIntervalSeconds = 0.010;
  static constexpr auto kFifoSmoothingTimeSeconds = 0.250;
  static constexpr auto kClockProportionalGain = 0.12;
  static constexpr auto kClockIntegralGain = 0.02;
  static constexpr auto kMaxCorrection = 0.005;
  static constexpr auto kMaxIntegralCorrection = 0.004;
  std::size_t target_fifo_frames_ = 0;
  std::size_t control_frame_accumulator_ = 0;
  double smoothed_fifo_frames_ = 0.0;
  double nominal_resampler_ratio_ = 1.0;
  double resampler_ratio_ = 1.0;
  double ratio_integral_ = 0.0;
  bool resampler_ready_ = false;
  bool control_initialized_ = false;
  bool recovery_pending_ = false;
  bool playback_started_ = false;
  bool recovery_armed_ = false;
  std::uint64_t handled_underflow_count_ = 0;
  std::uint64_t handled_overflow_count_ = 0;
  std::uint64_t handled_discontinuity_count_ = 0;
  std::atomic<std::uint64_t> discontinuity_count_ = 0;
  double resampler_position_ = 0.0;
  float resampler_current_left_ = 0.0F;
  float resampler_current_right_ = 0.0F;
  float resampler_next_left_ = 0.0F;
  float resampler_next_right_ = 0.0F;
};

ApplicationInput::ApplicationInput(StatusCallback callback)
    : impl_(std::make_unique<Impl>(std::move(callback))) {}
ApplicationInput::~ApplicationInput() = default;
auto ApplicationInput::Start(const std::uint32_t process_id,
                             const double output_sample_rate) -> bool {
  return impl_->Start(process_id, output_sample_rate);
}
void ApplicationInput::Stop() { impl_->Stop(); }
auto ApplicationInput::Read(float* const left, float* const right,
                            const std::size_t count) noexcept -> std::size_t {
  return impl_->Read(left, right, count);
}
auto ApplicationInput::GetStatus() const noexcept -> ApplicationInputStatus {
  return impl_->GetStatus();
}
auto ApplicationInput::GetLastError() const -> std::string {
  return impl_->GetLastError();
}

}  // namespace beatrice::common
