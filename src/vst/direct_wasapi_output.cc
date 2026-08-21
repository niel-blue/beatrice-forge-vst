// Copyright (c) 2026 Project Beatrice and Contributors

#include "vst/direct_wasapi_output.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
#include <utility>

#include "common/stereo_audio_fifo.h"

#ifdef _WIN32

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include <Audioclient.h>
#include <ksmedia.h>
#include <mmdeviceapi.h>
#include <wrl/client.h>

namespace beatrice::vst {

namespace {

using Microsoft::WRL::ComPtr;

constexpr auto kFifoCapacity = std::size_t{1U << 20};
constexpr auto kStreamFlags = AUDCLNT_STREAMFLAGS_EVENTCALLBACK;

struct CoTaskMemDeleter {
  void operator()(WAVEFORMATEX* const value) const noexcept {
    CoTaskMemFree(value);
  }
};
using WaveFormatPtr = std::unique_ptr<WAVEFORMATEX, CoTaskMemDeleter>;

auto Utf8ToWide(const std::string& text) -> std::wstring {
  if (text.empty()) {
    return {};
  }
  const auto size = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
                                        text.data(),
                                        static_cast<int>(text.size()), nullptr,
                                        0);
  if (size <= 0) {
    return {};
  }
  auto result = std::wstring(static_cast<std::size_t>(size), L'\0');
  MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(),
                      static_cast<int>(text.size()), result.data(), size);
  return result;
}

auto HResultText(const char* const operation, const HRESULT result)
    -> std::string {
  auto stream = std::ostringstream{};
  stream << operation << " failed (0x" << std::uppercase << std::hex
         << static_cast<unsigned long>(result) << ").";
  return stream.str();
}

auto IsFloatFormat(const WAVEFORMATEX& format) -> bool {
  if (format.wFormatTag == WAVE_FORMAT_IEEE_FLOAT) {
    return format.wBitsPerSample == 32;
  }
  if (format.wFormatTag != WAVE_FORMAT_EXTENSIBLE ||
      format.cbSize < sizeof(WAVEFORMATEXTENSIBLE) - sizeof(WAVEFORMATEX)) {
    return false;
  }
  const auto& extensible =
      reinterpret_cast<const WAVEFORMATEXTENSIBLE&>(format);
  return extensible.SubFormat == KSDATAFORMAT_SUBTYPE_IEEE_FLOAT &&
         format.wBitsPerSample == 32;
}

auto IsPcm16Format(const WAVEFORMATEX& format) -> bool {
  if (format.wFormatTag == WAVE_FORMAT_PCM) {
    return format.wBitsPerSample == 16;
  }
  if (format.wFormatTag != WAVE_FORMAT_EXTENSIBLE ||
      format.cbSize < sizeof(WAVEFORMATEXTENSIBLE) - sizeof(WAVEFORMATEX)) {
    return false;
  }
  const auto& extensible =
      reinterpret_cast<const WAVEFORMATEXTENSIBLE&>(format);
  return extensible.SubFormat == KSDATAFORMAT_SUBTYPE_PCM &&
         format.wBitsPerSample == 16;
}

auto SupportsFormat(IAudioClient* const client, const AUDCLNT_SHAREMODE mode,
                    WAVEFORMATEX& format) -> bool {
  WAVEFORMATEX* closest = nullptr;
  const auto result = client->IsFormatSupported(mode, &format, &closest);
  CoTaskMemFree(closest);
  return result == S_OK;
}

auto InitializeSharedClient(IAudioClient* const client,
                            WAVEFORMATEX& format, UINT32& period_frames)
    -> HRESULT {
  ComPtr<IAudioClient3> low_latency_client;
  if (SUCCEEDED(client->QueryInterface(IID_PPV_ARGS(&low_latency_client)))) {
    UINT32 default_period = 0;
    UINT32 fundamental_period = 0;
    UINT32 minimum_period = 0;
    UINT32 maximum_period = 0;
    auto result = low_latency_client->GetSharedModeEnginePeriod(
        &format, &default_period, &fundamental_period, &minimum_period,
        &maximum_period);
    if (FAILED(result)) {
      return result;
    }
    period_frames = fundamental_period == 0
                        ? minimum_period
                        : ((minimum_period + fundamental_period - 1) /
                           fundamental_period) * fundamental_period;
    if (period_frames == 0 || period_frames > maximum_period) {
      period_frames = default_period;
    }
    return low_latency_client->InitializeSharedAudioStream(
        kStreamFlags, period_frames, &format, nullptr);
  }
  const auto result = client->Initialize(AUDCLNT_SHAREMODE_SHARED,
                                         kStreamFlags, 0, 0, &format, nullptr);
  if (SUCCEEDED(result)) {
    REFERENCE_TIME device_period = 0;
    if (SUCCEEDED(client->GetDevicePeriod(&device_period, nullptr))) {
      period_frames = static_cast<UINT32>(std::max<REFERENCE_TIME>(
          1, (device_period * format.nSamplesPerSec + 9999999) / 10000000));
    }
  }
  return result;
}

auto InitializeExclusiveClient(IAudioClient* const client,
                               WAVEFORMATEX& format, UINT32& period_frames)
    -> HRESULT {
  REFERENCE_TIME default_period = 0;
  REFERENCE_TIME minimum_period = 0;
  auto result = client->GetDevicePeriod(&default_period, &minimum_period);
  if (FAILED(result)) {
    return result;
  }
  auto period = minimum_period > 0 ? minimum_period : default_period;
  if (period <= 0 || format.nSamplesPerSec == 0) {
    return AUDCLNT_E_BUFDURATION_PERIOD_NOT_EQUAL;
  }
  period_frames = static_cast<UINT32>(std::max<REFERENCE_TIME>(
      1, (period * format.nSamplesPerSec + 9999999) / 10000000));
  result = client->Initialize(AUDCLNT_SHAREMODE_EXCLUSIVE, kStreamFlags,
                              period, period, &format, nullptr);
  if (FAILED(result) && default_period > 0 && default_period != period) {
    period = default_period;
    period_frames = static_cast<UINT32>(std::max<REFERENCE_TIME>(
        1, (period * format.nSamplesPerSec + 9999999) / 10000000));
    result = client->Initialize(AUDCLNT_SHAREMODE_EXCLUSIVE, kStreamFlags,
                                period, period, &format, nullptr);
  }
  return result;
}

void WriteFrame(BYTE* const data, const UINT32 frame,
                const WAVEFORMATEX& format, const float left,
                const float right) noexcept {
  if (data == nullptr || format.nChannels == 0) {
    return;
  }
  const auto mono = (left + right) * 0.5F;
  if (IsFloatFormat(format)) {
    auto* const samples = reinterpret_cast<float*>(data);
    const auto offset = static_cast<std::size_t>(frame) * format.nChannels;
    for (auto channel = 0U; channel < format.nChannels; ++channel) {
      samples[offset + channel] = format.nChannels == 1
                                      ? mono
                                      : (channel == 0 ? left
                                                      : (channel == 1 ? right
                                                                      : 0.0F));
    }
    return;
  }
  auto* const samples = reinterpret_cast<std::int16_t*>(data);
  const auto offset = static_cast<std::size_t>(frame) * format.nChannels;
  for (auto channel = 0U; channel < format.nChannels; ++channel) {
    const auto value = format.nChannels == 1
                           ? mono
                           : (channel == 0 ? left
                                           : (channel == 1 ? right : 0.0F));
    samples[offset + channel] = static_cast<std::int16_t>(std::lrint(
        std::clamp(value, -1.0F, 1.0F) * 32767.0F));
  }
}

class SourceResampler final {
 public:
  SourceResampler(common::StereoAudioFifo& fifo, const double source_rate,
                  const double output_rate)
      : fifo_(fifo), ratio_(source_rate > 0.0 && output_rate > 0.0
                                ? source_rate / output_rate
                                : 1.0) {}

  void Next(float& left, float& right) noexcept {
    if (!initialized_) {
      static_cast<void>(fifo_.Pop(previous_left_, previous_right_));
      static_cast<void>(fifo_.Pop(next_left_, next_right_));
      initialized_ = true;
    }
    left = previous_left_ +
           static_cast<float>(phase_) * (next_left_ - previous_left_);
    right = previous_right_ +
            static_cast<float>(phase_) * (next_right_ - previous_right_);
    phase_ += ratio_;
    while (phase_ >= 1.0) {
      previous_left_ = next_left_;
      previous_right_ = next_right_;
      static_cast<void>(fifo_.Pop(next_left_, next_right_));
      phase_ -= 1.0;
    }
  }

 private:
  common::StereoAudioFifo& fifo_;
  double ratio_ = 1.0;
  double phase_ = 0.0;
  float previous_left_ = 0.0F;
  float previous_right_ = 0.0F;
  float next_left_ = 0.0F;
  float next_right_ = 0.0F;
  bool initialized_ = false;
};

}  // namespace

class DirectWasapiOutput::Impl final {
 public:
  explicit Impl(StatusCallback status_callback)
      : status_callback_(std::move(status_callback)), fifo_(kFifoCapacity) {}

  ~Impl() { Stop(); }

  void Start(const DirectWasapiConfig& config) {
    Stop();
    fifo_.DiscardAll();
    config_ = config;
    status_.store(DirectWasapiStatus::kStarting, std::memory_order_release);
    Notify(DirectWasapiStatus::kStarting, {});
    stop_event_ = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    audio_event_ = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    if (stop_event_ == nullptr || audio_event_ == nullptr) {
      CloseEvents();
      SetUnavailable("WASAPI event creation failed.");
      return;
    }
    thread_ = std::thread([this]() { Run(); });
  }

  void Stop() noexcept {
    if (stop_event_ != nullptr) {
      SetEvent(stop_event_);
    }
    if (thread_.joinable()) {
      thread_.join();
    }
    CloseEvents();
    fifo_.DiscardAll();
    if (status_.load(std::memory_order_acquire) != DirectWasapiStatus::kOff) {
      status_.store(DirectWasapiStatus::kOff, std::memory_order_release);
      Notify(DirectWasapiStatus::kOff, {});
    }
  }

  void PushBlock(const float* const left, const float* const right,
                 const std::size_t count) noexcept {
    if (status_.load(std::memory_order_relaxed) == DirectWasapiStatus::kOff) {
      return;
    }
    fifo_.PushBlock(left, right, count);
  }

 private:
  void CloseEvents() noexcept {
    if (stop_event_ != nullptr) {
      CloseHandle(stop_event_);
      stop_event_ = nullptr;
    }
    if (audio_event_ != nullptr) {
      CloseHandle(audio_event_);
      audio_event_ = nullptr;
    }
  }

  void Notify(const DirectWasapiStatus status, const std::string& error) {
    if (status_callback_) {
      status_callback_(status, error);
    }
  }

  void SetUnavailable(const std::string& error) {
    status_.store(DirectWasapiStatus::kUnavailable,
                  std::memory_order_release);
    Notify(DirectWasapiStatus::kUnavailable, error);
  }

  void Run() {
    const auto com_result = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    const auto uninitialize = SUCCEEDED(com_result);
    if (FAILED(com_result) && com_result != RPC_E_CHANGED_MODE) {
      SetUnavailable("WASAPI COM initialization failed.");
      return;
    }

    ComPtr<IMMDeviceEnumerator> enumerator;
    auto result = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr,
                                   CLSCTX_ALL, IID_PPV_ARGS(&enumerator));
    if (FAILED(result)) {
      SetUnavailable(HResultText("WASAPI device enumeration", result));
      if (uninitialize) {
        CoUninitialize();
      }
      return;
    }
    const auto device_id = Utf8ToWide(config_.device_id);
    if (device_id.empty()) {
      SetUnavailable("No WASAPI output device was selected.");
      if (uninitialize) {
        CoUninitialize();
      }
      return;
    }
    ComPtr<IMMDevice> device;
    result = enumerator->GetDevice(device_id.c_str(), &device);
    if (FAILED(result)) {
      SetUnavailable(HResultText("WASAPI output device lookup", result));
      if (uninitialize) {
        CoUninitialize();
      }
      return;
    }

    ComPtr<IAudioClient> client;
    result = device->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr,
                              reinterpret_cast<void**>(client.GetAddressOf()));
    if (FAILED(result)) {
      SetUnavailable(HResultText("WASAPI audio client activation", result));
      if (uninitialize) {
        CoUninitialize();
      }
      return;
    }
    WAVEFORMATEX* raw_format = nullptr;
    result = client->GetMixFormat(&raw_format);
    WaveFormatPtr format(raw_format);
    if (FAILED(result) || format == nullptr) {
      SetUnavailable(HResultText("WASAPI mix format", result));
      if (uninitialize) {
        CoUninitialize();
      }
      return;
    }
    if (!IsFloatFormat(*format) && !IsPcm16Format(*format)) {
      SetUnavailable("The selected WASAPI device uses an unsupported format.");
      if (uninitialize) {
        CoUninitialize();
      }
      return;
    }
    if (!SupportsFormat(client.Get(),
                        config_.mode == DirectWasapiMode::kExclusive
                            ? AUDCLNT_SHAREMODE_EXCLUSIVE
                            : AUDCLNT_SHAREMODE_SHARED,
                        *format)) {
      SetUnavailable("The selected WASAPI device rejected its mix format.");
      if (uninitialize) {
        CoUninitialize();
      }
      return;
    }

    UINT32 period_frames = 0;
    if (config_.mode == DirectWasapiMode::kExclusive) {
      result = InitializeExclusiveClient(client.Get(), *format, period_frames);
    } else {
      result = InitializeSharedClient(client.Get(), *format, period_frames);
    }
    if (FAILED(result)) {
      SetUnavailable(HResultText("WASAPI stream initialization", result));
      if (uninitialize) {
        CoUninitialize();
      }
      return;
    }
    result = client->SetEventHandle(audio_event_);
    if (FAILED(result)) {
      SetUnavailable(HResultText("WASAPI event setup", result));
      if (uninitialize) {
        CoUninitialize();
      }
      return;
    }
    UINT32 buffer_frames = 0;
    result = client->GetBufferSize(&buffer_frames);
    if (FAILED(result) || buffer_frames == 0) {
      SetUnavailable(HResultText("WASAPI buffer query", result));
      if (uninitialize) {
        CoUninitialize();
      }
      return;
    }
    ComPtr<IAudioRenderClient> render_client;
    result = client->GetService(IID_PPV_ARGS(&render_client));
    if (FAILED(result)) {
      SetUnavailable(HResultText("WASAPI render client", result));
      if (uninitialize) {
        CoUninitialize();
      }
      return;
    }

    SourceResampler source(fifo_, config_.source_sample_rate,
                           static_cast<double>(format->nSamplesPerSec));
    auto fill = [&](const UINT32 frames) -> HRESULT {
      BYTE* data = nullptr;
      auto fill_result = render_client->GetBuffer(frames, &data);
      if (FAILED(fill_result)) {
        return fill_result;
      }
      for (UINT32 frame = 0; frame < frames; ++frame) {
        auto left = 0.0F;
        auto right = 0.0F;
        source.Next(left, right);
        WriteFrame(data, frame, *format, left, right);
      }
      return render_client->ReleaseBuffer(frames, 0);
    };

    result = fill(buffer_frames);
    if (FAILED(result)) {
      SetUnavailable(HResultText("WASAPI initial buffer", result));
      if (uninitialize) {
        CoUninitialize();
      }
      return;
    }
    result = client->Start();
    if (FAILED(result)) {
      SetUnavailable(HResultText("WASAPI stream start", result));
      if (uninitialize) {
        CoUninitialize();
      }
      return;
    }
    status_.store(DirectWasapiStatus::kActive, std::memory_order_release);
    Notify(DirectWasapiStatus::kActive, {});

    const HANDLE events[] = {stop_event_, audio_event_};
    auto running = true;
    while (running) {
      const auto wait_result = WaitForMultipleObjects(2, events, FALSE,
                                                      INFINITE);
      if (wait_result == WAIT_OBJECT_0) {
        running = false;
        continue;
      }
      if (wait_result != WAIT_OBJECT_0 + 1) {
        SetUnavailable("WASAPI event wait failed.");
        running = false;
        continue;
      }
      UINT32 padding = 0;
      if (FAILED(client->GetCurrentPadding(&padding))) {
        SetUnavailable("WASAPI buffer padding query failed.");
        running = false;
        continue;
      }
      const auto available = padding < buffer_frames ? buffer_frames - padding
                                                      : 0U;
      if (available > 0) {
        result = fill(std::min(available, buffer_frames));
        if (FAILED(result)) {
          SetUnavailable(HResultText("WASAPI render buffer", result));
          running = false;
        }
      }
    }
    static_cast<void>(client->Stop());
    if (uninitialize) {
      CoUninitialize();
    }
  }

  StatusCallback status_callback_;
  common::StereoAudioFifo fifo_;
  DirectWasapiConfig config_;
  std::atomic<DirectWasapiStatus> status_{DirectWasapiStatus::kOff};
  HANDLE stop_event_ = nullptr;
  HANDLE audio_event_ = nullptr;
  std::thread thread_;
};

DirectWasapiOutput::DirectWasapiOutput(StatusCallback status_callback)
    : impl_(std::make_unique<Impl>(std::move(status_callback))) {}

DirectWasapiOutput::~DirectWasapiOutput() = default;

void DirectWasapiOutput::Start(const DirectWasapiConfig& config) {
  impl_->Start(config);
}

void DirectWasapiOutput::Stop() noexcept { impl_->Stop(); }

void DirectWasapiOutput::PushBlock(const float* const left,
                                   const float* const right,
                                   const std::size_t count) noexcept {
  impl_->PushBlock(left, right, count);
}

}  // namespace beatrice::vst

#else

namespace beatrice::vst {

class DirectWasapiOutput::Impl {
 public:
  explicit Impl(StatusCallback) {}
  void Start(const DirectWasapiConfig&) {}
  void Stop() noexcept {}
  void PushBlock(const float*, const float*, std::size_t) noexcept {}
};

DirectWasapiOutput::DirectWasapiOutput(StatusCallback status_callback)
    : impl_(std::make_unique<Impl>(std::move(status_callback))) {}
DirectWasapiOutput::~DirectWasapiOutput() = default;
void DirectWasapiOutput::Start(const DirectWasapiConfig& config) {
  impl_->Start(config);
}
void DirectWasapiOutput::Stop() noexcept { impl_->Stop(); }
void DirectWasapiOutput::PushBlock(const float* const left,
                                   const float* const right,
                                   const std::size_t count) noexcept {
  impl_->PushBlock(left, right, count);
}

}  // namespace beatrice::vst

#endif
