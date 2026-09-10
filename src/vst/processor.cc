// Copyright (c) 2024-2026 Project Beatrice and Contributors

#include "vst/processor.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <ios>
#include <memory>
#include <mutex>  // NOLINT(build/c++11)
#include <sstream>
#include <string>
#include <variant>

#include "vst3sdk/pluginterfaces/base/fplatform.h"
#include "vst3sdk/pluginterfaces/base/fstrdefs.h"
#include "vst3sdk/pluginterfaces/base/ftypes.h"
#include "vst3sdk/pluginterfaces/base/funknown.h"
#include "vst3sdk/pluginterfaces/vst/ivstaudioprocessor.h"
#include "vst3sdk/pluginterfaces/vst/ivstparameterchanges.h"
#include "vst3sdk/pluginterfaces/vst/vstspeaker.h"
#include "vst3sdk/pluginterfaces/base/smartpointer.h"

// Beatrice
#include "common/error.h"
#include "common/parameter_schema.h"
#include "vst/parameter.h"
#include "vst/plugin_state.h"

#ifdef BEATRICE_ONLY_FOR_LINTER_DO_NOT_COMPILE_WITH_THIS
#include "vst/metadata.h.in"
#else
#include "metadata.h"  // NOLINT(build/include_subdir)
#endif

namespace beatrice::vst {

using Steinberg::kResultFalse;
using Steinberg::kResultOk;
using Steinberg::kResultTrue;
// NOLINTNEXTLINE(readability-identifier-naming)
namespace SpeakerArr = Steinberg::Vst::SpeakerArr;

// コンストラクタ
Processor::Processor()
    : audio_engine_(common::kSchema),
      direct_wasapi_output_([this](const DirectWasapiStatus status,
                                   const std::string& error) {
        SendDirectWasapiStatusMessage(status, error);
      }),
      application_input_([this](const common::ApplicationInputStatus status,
                                const std::string& error) {
        SendApplicationInputStatusMessage(status, error);
      }),
      additional_application_input_([this](
          const common::ApplicationInputStatus status,
          const std::string& error) {
        SendApplicationInputStatusMessage(status, error);
      }) {
  // 対応するコントローラクラスを設定する
  setControllerClass(kControllerUID);
}

Processor::~Processor() {
  recorder_.Stop();
  application_input_.Stop();
  direct_wasapi_output_.Stop();
}

// "Initialized" の状態に遷移する
// チャンネル数の指定など
auto PLUGIN_API Processor::initialize(FUnknown* const context) -> tresult {
  // 親クラスの初期化
  const tresult result = AudioEffect::initialize(context);
  if (result != kResultTrue) {
    return kResultFalse;
  }

  // Voice conversion itself is mono, but post-conversion effects can produce
  // stereo ambience.  Keep the input mono and expose a stereo output so the
  // host does not collapse that ambience back to mono.
  addAudioInput(STR16("AudioInput"), SpeakerArr::kMono);
  addAudioOutput(STR16("AudioOutput"), SpeakerArr::kStereo);

  return kResultTrue;
}

// バスの設定
// "Initialized" または "Setup Done" の時に呼ばれる
// VST の起動時にも呼ばれて勝手にチャンネル数変更しようとして来たりするので
// ちゃんと防ぐ
auto PLUGIN_API Processor::setBusArrangements(SpeakerArrangement* const inputs,
                                              const int32 numIns,
                                              SpeakerArrangement* const outputs,
                                              const int32 numOuts) -> tresult {
  if (numIns != 1 || numOuts != 1) {
    return kResultFalse;
  }

  // The conversion input may be mono or stereo (stereo is downmixed before
  // conversion), while the post-effects output must remain stereo.  When a
  // host asks for an unsupported arrangement, leave the buses in the closest
  // supported layout so it can discover the mono-to-stereo configuration.
  const auto input_supported =
      inputs[0] == SpeakerArr::kMono || inputs[0] == SpeakerArr::kStereo;
  auto selected_input = input_supported ? inputs[0] : SpeakerArr::kMono;
  auto selected_output = SpeakerArr::kStereo;
  const auto result = AudioEffect::setBusArrangements(
      &selected_input, 1, &selected_output, 1);
  return input_supported && outputs[0] == SpeakerArr::kStereo
             ? result
             : kResultFalse;
}

// "Setup Done" の状態に遷移する
// 必ず setActive(false) の状態で呼ばれる
// setup.maxSamplesPerBlock  最大ブロックサイズ
// setup.sampleRate          サンプリング周波数
// setup.processMode         kRealtime or kPrefetch or kOffline
// setup.symbolicSampleSize  kSample32 or kSample64
auto PLUGIN_API Processor::setupProcessing(ProcessSetup& setup) -> tresult {
  const std::scoped_lock lock(mtx_);
  if (setup.symbolicSampleSize == Steinberg::Vst::kSample64) {
    return kResultFalse;
  }
  const auto error_code =
      audio_engine_.Prepare(setup.sampleRate, setup.maxSamplesPerBlock);
  assert(error_code == common::ErrorCode::kSuccess);
  meter_sample_rate_ = setup.sampleRate;
  meter_frames_ = 0;
  meter_input_peak_ = 0.0F;
  meter_output_peak_ = 0.0F;
  meter_external_output_peak_ = 0.0F;
  meter_additional_input_peak_ = 0.0F;
  input_meter_buffer_.assign(
      static_cast<std::size_t>(std::max(0, setup.maxSamplesPerBlock)), 0.0F);
  mixed_output_left_.assign(
      static_cast<std::size_t>(std::max(0, setup.maxSamplesPerBlock)), 0.0F);
  mixed_output_right_.assign(
      static_cast<std::size_t>(std::max(0, setup.maxSamplesPerBlock)), 0.0F);
  voice_delay_line_.Prepare(
      setup.sampleRate, static_cast<std::uint32_t>(common::kMaxBgmDelayMs));
  bgm_delay_line_.Prepare(
      setup.sampleRate, static_cast<std::uint32_t>(common::kMaxBgmDelayMs));
  voice_delay_active_ = false;
  active_bgm_delay_ms_ = 0;
  application_input_buffer_.assign(
      static_cast<std::size_t>(std::max(0, setup.maxSamplesPerBlock)), 0.0F);
  application_input_right_buffer_.assign(
      static_cast<std::size_t>(std::max(0, setup.maxSamplesPerBlock)), 0.0F);
  additional_application_input_buffer_.assign(
      static_cast<std::size_t>(std::max(0, setup.maxSamplesPerBlock)), 0.0F);
  additional_application_input_right_buffer_.assign(
      static_cast<std::size_t>(std::max(0, setup.maxSamplesPerBlock)), 0.0F);
  delayed_additional_input_buffer_.assign(
      static_cast<std::size_t>(std::max(0, setup.maxSamplesPerBlock)), 0.0F);
  delayed_additional_input_right_buffer_.assign(
      static_cast<std::size_t>(std::max(0, setup.maxSamplesPerBlock)), 0.0F);
  if (direct_wasapi_config_.has_value()) {
    direct_wasapi_config_->source_sample_rate = setup.sampleRate;
    direct_wasapi_output_.Start(*direct_wasapi_config_);
  }
  return AudioEffect::setupProcessing(setup);
}

auto PLUGIN_API Processor::setActive(const TBool state) -> tresult {
  if (state) {
    voice_delay_line_.Reset();
    bgm_delay_line_.Reset();
    voice_delay_active_ = false;
    active_bgm_delay_ms_ = 0;
    if (direct_wasapi_config_.has_value() && meter_sample_rate_ > 0.0) {
      direct_wasapi_config_->source_sample_rate = meter_sample_rate_;
      direct_wasapi_output_.Start(*direct_wasapi_config_);
    }
    if (application_input_enabled_ && !application_input_identity_.empty()) {
      StartApplicationInput(application_input_, application_input_process_id_,
                            application_input_identity_);
    }
    if (additional_input_enabled_ &&
        !additional_application_input_identity_.empty()) {
      StartApplicationInput(additional_application_input_,
                            additional_application_input_process_id_,
                            additional_application_input_identity_);
    }
  } else {
    recorder_.Stop();
    SendRecordingStatusMessage();
    application_input_.Stop();
    additional_application_input_.Stop();
    application_input_process_id_ = 0;
    additional_application_input_process_id_ = 0;
    direct_wasapi_output_.Stop();
    // メモリの解放など
    std::lock_guard<std::mutex> lock(mtx_);
    const auto error_code = audio_engine_.ResetContext();
    assert(error_code == common::ErrorCode::kSuccess);
    meter_frames_ = 0;
    meter_input_peak_ = 0.0F;
    meter_output_peak_ = 0.0F;
    meter_external_output_peak_ = 0.0F;
    meter_additional_input_peak_ = 0.0F;
    voice_delay_line_.Reset();
    bgm_delay_line_.Reset();
    voice_delay_active_ = false;
    active_bgm_delay_ms_ = 0;
  }
  return AudioEffect::setActive(state);
}

auto PLUGIN_API Processor::getLatencySamples() -> uint32 {
  std::lock_guard<std::mutex> lock(mtx_);
  if (!audio_engine_.IsLatencyReportingEnabled()) {
    return 0;
  }
  return static_cast<uint32>(audio_engine_.GetLatencySamples());
}

// メイン処理
auto PLUGIN_API Processor::process(ProcessData& data) -> tresult {
  // パラメータの変更があった場合
  if (data.inputParameterChanges != nullptr) {
    const auto n_parameter_changed =
        data.inputParameterChanges->getParameterCount();
    for (auto index = 0; index < n_parameter_changed; ++index) {
      // バッファの中で複数回同じパラメータの変更があることが考慮される
      auto* const param_queue =
          data.inputParameterChanges->getParameterData(index);
      if (param_queue == nullptr) {
        continue;
      }
      ParamValue value;
      int sample_offset;
      const auto n_points = param_queue->getPointCount();
      if (n_points <= 0) {
        continue;
      }
      if (param_queue->getPoint(n_points - 1, sample_offset, value) !=
          kResultTrue) {
        continue;
      }
      unreflected_params_[param_queue->getParameterId()] = value;
    }
  }

  const std::unique_lock<std::mutex> lock(mtx_, std::try_to_lock);
  // ファイルの読み込み中はパラメータ変更の処理を先送りにし、
  // 無音を出力する
  if (!lock.owns_lock()) {
    for (auto bus = 0; bus < data.numOutputs; ++bus) {
      for (auto ch = 0; ch < data.outputs[bus].numChannels; ++ch) {
        std::memset(data.outputs[bus].channelBuffers32[ch], 0,
                    data.numSamples * sizeof(float));
      }
      data.outputs[bus].silenceFlags = 1U;
    }
    return kResultTrue;
  }

  for (const auto [vst_param_id, value] : unreflected_params_) {
    const auto param_id = static_cast<common::ParameterID>(vst_param_id);
    const auto& param = common::kSchema.GetParameter(param_id);
    if (const auto* const num_param =
            std::get_if<common::NumberParameter>(&param)) {
      const auto denormalized_value = Denormalize(*num_param, value);
      const auto error_code =
          audio_engine_.SetParameter(param_id, denormalized_value);
      assert(error_code == common::ErrorCode::kSuccess);
      assert(denormalized_value ==
             std::get<double>(audio_engine_.GetParameter(param_id)));
    } else if (const auto* const list_param =
                   std::get_if<common::ListParameter>(&param)) {
      const auto denormalized_value = Denormalize(*list_param, value);
      const auto error_code =
          audio_engine_.SetParameter(param_id, denormalized_value);
      assert(error_code == common::ErrorCode::kSuccess);
    }
  }
  unreflected_params_.clear();

  const auto application_input_active =
      application_input_enabled_ || additional_input_enabled_;
  const auto host_input_available =
      data.numInputs > 0 && data.inputs[0].numChannels > 0 &&
      data.inputs[0].channelBuffers32 != nullptr;
  if (data.numOutputs == 0 || data.numSamples == 0 ||
      (!application_input_active && !host_input_available)) {
    // 何もしない
    return kResultOk;
  }

  // double は処理しない
  if (data.symbolicSampleSize == Steinberg::Vst::kSample64) {
    return kResultOk;
  }

  // チャンネル数を確認。Application Input はホスト入力を使わないため、
  // ホストが入力バスを持たない構成でも動作できる。
  if (!application_input_active && !host_input_available) {
    return kResultOk;
  }
  if (data.outputs[0].numChannels < 1) {
    return kResultOk;
  }

  // 出力バス 0 のチャンネル 0 に入力をコピー
  auto application_input_silent = true;
  const auto primary_application_active =
      input_source_ == common::InputSource::kApplicationInput &&
      application_input_enabled_;
  if (primary_application_active) {
    static_cast<void>(application_input_.Read(
        application_input_buffer_.data(), application_input_right_buffer_.data(),
        static_cast<std::size_t>(data.numSamples)));
    for (auto i = 0; i < data.numSamples; ++i) {
      application_input_silent =
          application_input_silent && application_input_buffer_[i] == 0.0F &&
          application_input_right_buffer_[i] == 0.0F;
    }
  }
  const auto additional_application_active =
      additional_input_source_ == common::InputSource::kApplicationInput &&
      additional_input_enabled_;
  if (additional_application_active) {
    static_cast<void>(additional_application_input_.Read(
        additional_application_input_buffer_.data(),
        additional_application_input_right_buffer_.data(),
        static_cast<std::size_t>(data.numSamples)));
    const auto gain = static_cast<float>(std::pow(
        10.0, recording_additional_input_gain_db_ / 20.0));
    for (auto i = 0; i < data.numSamples; ++i) {
      additional_application_input_buffer_[i] *= gain;
      additional_application_input_right_buffer_[i] *= gain;
    }
  }
  if (!primary_application_active && !host_input_available) {
    std::fill(input_meter_buffer_.begin(), input_meter_buffer_.end(), 0.0F);
  }
  const float* const in0 =
      primary_application_active
          ? application_input_buffer_.data()
          : host_input_available ? data.inputs[0].channelBuffers32[0]
                                 : input_meter_buffer_.data();
  float* const out0 = data.outputs[0].channelBuffers32[0];
  float* const out1 = data.outputs[0].numChannels >= 2
                          ? data.outputs[0].channelBuffers32[1]
                          : nullptr;
  const float* const in1 =
      primary_application_active
          ? application_input_right_buffer_.data()
          : (host_input_available && data.inputs[0].numChannels >= 2
                 ? data.inputs[0].channelBuffers32[1]
                 : nullptr);
  const auto host_input_silent =
      primary_application_active
          ? application_input_silent
          : !host_input_available || data.inputs[0].silenceFlags != 0;
  const auto process_result = audio_engine_.Process(
      {.input_left = in0,
       .input_right = in1,
       .output_left = out0,
       .output_right = out1,
       .pre_conversion = input_meter_buffer_.data(),
       .num_samples = data.numSamples,
       .input_silent = host_input_silent});
  data.outputs[0].silenceFlags =
      process_result.output_silent
          ? (static_cast<Steinberg::uint64>(1U)
             << data.outputs[0].numChannels) - 1U
          : 0U;

  // An application capture is already audible from its source application, so
  // ADD BGM stays out of the host/VST monitor and is added only to the external
  // output/recorder mix.
  auto block_output_peak = 0.0F;
  auto block_external_output_peak = 0.0F;
  const auto additional_bgm_active = additional_application_active;
  const auto bgm_delay_ms = additional_bgm_active
                                ? common::ClampBgmDelayMs(
                                      recording_voice_delay_ms_)
                                : 0;
  if (additional_bgm_active != voice_delay_active_ ||
      bgm_delay_ms != active_bgm_delay_ms_) {
    voice_delay_line_.Reset();
    bgm_delay_line_.Reset();
    voice_delay_active_ = additional_bgm_active;
    active_bgm_delay_ms_ = bgm_delay_ms;
  }
  if (additional_bgm_active && bgm_delay_ms < 0) {
    voice_delay_line_.ProcessBlock(
        out0, out1, mixed_output_left_.data(), mixed_output_right_.data(),
        static_cast<std::size_t>(data.numSamples),
        common::SignedDelaySamples(meter_sample_rate_, bgm_delay_ms));
  } else {
    std::copy_n(out0, data.numSamples, mixed_output_left_.data());
    std::copy_n(out1 != nullptr ? out1 : out0, data.numSamples,
                mixed_output_right_.data());
  }
  if (additional_bgm_active && bgm_delay_ms > 0) {
    bgm_delay_line_.ProcessBlock(
        additional_application_input_buffer_.data(),
        additional_application_input_right_buffer_.data(),
        delayed_additional_input_buffer_.data(),
        delayed_additional_input_right_buffer_.data(),
        static_cast<std::size_t>(data.numSamples),
        common::SignedDelaySamples(meter_sample_rate_, bgm_delay_ms));
  } else {
    std::copy_n(additional_application_input_buffer_.data(), data.numSamples,
                delayed_additional_input_buffer_.data());
    std::copy_n(additional_application_input_right_buffer_.data(),
                data.numSamples, delayed_additional_input_right_buffer_.data());
  }
  for (auto i = 0; i < data.numSamples; ++i) {
    const auto voice_left = out0[i];
    const auto voice_right = out1 != nullptr ? out1[i] : out0[i];
    // Keep the left GAIN meter voice-only; ADD BGM is external-only.
    block_output_peak = std::max(block_output_peak, std::abs(voice_left));
    block_output_peak = std::max(block_output_peak, std::abs(voice_right));

    if (additional_application_active) {
      mixed_output_left_[i] = std::clamp(
          mixed_output_left_[i] + delayed_additional_input_buffer_[i],
          -1.0F, 1.0F);
      mixed_output_right_[i] = std::clamp(
          mixed_output_right_[i] +
              delayed_additional_input_right_buffer_[i],
          -1.0F, 1.0F);
    }
    block_external_output_peak = std::max(
        block_external_output_peak, std::abs(mixed_output_left_[i]));
    block_external_output_peak = std::max(
        block_external_output_peak, std::abs(mixed_output_right_[i]));
  }
  auto block_input_peak = 0.0F;
  if (!host_input_silent &&
      process_result.error == common::ErrorCode::kSuccess) {
    for (auto i = 0; i < data.numSamples; ++i) {
      block_input_peak =
          std::max(block_input_peak, std::abs(input_meter_buffer_[i]));
    }
  }
  meter_input_peak_ = std::max(meter_input_peak_, block_input_peak);
  meter_output_peak_ = std::max(meter_output_peak_, block_output_peak);
  meter_external_output_peak_ = std::max(meter_external_output_peak_,
                                          block_external_output_peak);
  auto block_additional_input_peak = 0.0F;
  if (additional_application_active) {
    for (auto i = 0; i < data.numSamples; ++i) {
      block_additional_input_peak = std::max(
          block_additional_input_peak,
          std::max(std::abs(additional_application_input_buffer_[i]),
                   std::abs(additional_application_input_right_buffer_[i])));
    }
  }
  meter_additional_input_peak_ =
      std::max(meter_additional_input_peak_, block_additional_input_peak);
  // The optional WASAPI path receives the post-mix signal. Host output buffers
  // remain untouched and continue to be the normal VST voice-only output.
  direct_wasapi_output_.PushBlock(
      mixed_output_left_.data(), mixed_output_right_.data(),
      static_cast<std::size_t>(data.numSamples));
  if (recorder_.IsRecording()) {
    for (auto i = 0; i < data.numSamples; ++i) {
      recorder_.Push(input_meter_buffer_[i], mixed_output_left_[i],
                     mixed_output_right_[i], 0.0F, 0.0F);
    }
  }
  meter_frames_ += data.numSamples;
  const auto meter_interval = std::max<std::int64_t>(
      1, static_cast<std::int64_t>(std::llround(meter_sample_rate_ * 0.05)));
  if (meter_frames_ >= meter_interval) {
    SendAudioLevelMessage();
    SendRecordingStatusMessage();
    meter_frames_ = 0;
    meter_input_peak_ = 0.0F;
    meter_output_peak_ = 0.0F;
    meter_external_output_peak_ = 0.0F;
    meter_additional_input_peak_ = 0.0F;
  }

  return kResultOk;
}

void Processor::SendRecordingStatusMessage() {
  if (const auto message = Steinberg::owned(allocateMessage())) {
    message->setMessageID("recording_status");
    const auto status = recorder_.GetStatus();
    if (auto* const attributes = message->getAttributes();
        attributes != nullptr) {
      static_cast<void>(attributes->setInt(
          "recording", status.recording ? 1 : 0));
      static_cast<void>(attributes->setInt(
          "frames", static_cast<Steinberg::int64>(status.frames)));
      static_cast<void>(attributes->setInt(
          "dropped_frames",
          static_cast<Steinberg::int64>(status.dropped_frames)));
      if (!status.error.empty()) {
        static_cast<void>(attributes->setBinary(
            "error", status.error.data(),
            static_cast<Steinberg::uint32>(status.error.size())));
      }
      static_cast<void>(sendMessage(message));
    }
  }
}

void Processor::SendAudioLevelMessage() {
  if (const auto message = Steinberg::owned(allocateMessage())) {
    message->setMessageID("audio_levels");
    if (auto* const attributes = message->getAttributes();
        attributes != nullptr) {
      static_cast<void>(attributes->setFloat("input_peak", meter_input_peak_));
      static_cast<void>(
          attributes->setFloat("output_peak", meter_output_peak_));
      static_cast<void>(attributes->setFloat(
          "external_output_peak", meter_external_output_peak_));
      static_cast<void>(attributes->setFloat(
          "additional_input_peak", meter_additional_input_peak_));
      static_cast<void>(attributes->setFloat("input_file_position",
                                             0.0F));
      static_cast<void>(attributes->setFloat("input_file_length",
                                             0.0F));
      static_cast<void>(attributes->setFloat(
          "additional_file_position", 0.0F));
      static_cast<void>(attributes->setFloat(
          "additional_file_length", 0.0F));
      static_cast<void>(sendMessage(message));
    }
  }
}

void Processor::SendDirectWasapiStatusMessage(
    const DirectWasapiStatus status, const std::string& error) {
  if (const auto message = Steinberg::owned(allocateMessage())) {
    message->setMessageID("direct_wasapi_status");
    if (auto* const attributes = message->getAttributes();
        attributes != nullptr) {
      static_cast<void>(attributes->setInt(
          "status", static_cast<Steinberg::int64>(status)));
      if (!error.empty()) {
        static_cast<void>(attributes->setBinary(
            "error", error.data(), static_cast<Steinberg::uint32>(error.size())));
      }
      static_cast<void>(sendMessage(message));
    }
  }
}

void Processor::SendApplicationInputStatusMessage(
    const common::ApplicationInputStatus status, const std::string& error) {
  if (const auto message = Steinberg::owned(allocateMessage())) {
    message->setMessageID("application_input_status");
    if (auto* const attributes = message->getAttributes();
        attributes != nullptr) {
      static_cast<void>(attributes->setInt(
          "status", static_cast<Steinberg::int64>(status)));
      if (!error.empty()) {
        static_cast<void>(attributes->setBinary(
            "error", error.data(),
            static_cast<Steinberg::uint32>(error.size())));
      }
      static_cast<void>(sendMessage(message));
    }
  }
}

auto Processor::StartApplicationInput(
    common::ApplicationInput& capture, std::uint32_t& process_id,
    const std::string& identity) -> bool {
  capture.Stop();
  process_id = 0;
  const auto snapshot = common::EnumerateApplicationInputs(
      common::CurrentApplicationInputExclusions());
  const auto application = common::FindApplicationInput(snapshot, identity);
  if (!application.has_value()) {
    SendApplicationInputStatusMessage(
        common::ApplicationInputStatus::kUnavailable,
        snapshot.error.empty()
            ? "The selected application is not producing audio."
            : snapshot.error);
    return false;
  }
  if (meter_sample_rate_ <= 0.0 ||
      !capture.Start(application->process_id, meter_sample_rate_)) {
    return false;
  }
  process_id = application->process_id;
  return true;
}

// プロジェクトやプリセットをロードした時に呼ばれる。
// kResultFalse を返した場合、StudioRack などでは
// Controller::setComponentState が呼ばれなくなるため注意が必要。
auto PLUGIN_API Processor::setState(IBStream* const state) -> tresult {
  int siz;
  if (state->read(&siz, sizeof(siz)) != kResultTrue) {
    return kResultFalse;
  }
  auto state_string = std::string();
  state_string.resize(siz);
  if (state->read(std::to_address(state_string.begin()), siz) != kResultTrue) {
    return kResultFalse;
  }
  auto parameter_state = std::string{};
  auto ui_state = PersistedPluginUiState{};
  if (!DecodePluginState(state_string, parameter_state, ui_state)) {
    return kResultFalse;
  }

  auto restored_direct_wasapi = std::optional<DirectWasapiConfig>{};
  {
    std::lock_guard<std::mutex> lock(mtx_);
    auto iss = std::istringstream(parameter_state, std::ios::binary);
  // Controller 側の状態との整合性を維持するため、
  // Controller 側や Host から送られた設定値は、たとえ不正なものでも
  // なるべくそのまま保持する。
  [[maybe_unused]] const auto error_code =
      audio_engine_.ReadState(iss);
    recording_mode_ = common::NormalizeRecordingMode(ui_state.recording_mode);
    recording_path_ = ui_state.recording_path;
    recording_voice_delay_ms_ =
        common::ClampBgmDelayMs(ui_state.voice_delay_ms);
    recording_additional_input_gain_db_ = std::clamp(
        ui_state.additional_input_gain_db,
        common::kMinAdditionalInputGainDb,
        common::kMaxAdditionalInputGainDb);
    // Audio Files remains decodable only as a legacy state value.  It is not
    // exposed by either host anymore, so migrate old projects before any
    // runtime input is configured.
    input_source_ = ui_state.input_source == common::InputSource::kAudioFile
                        ? common::InputSource::kDawInput
                        : ui_state.input_source;
    additional_input_source_ =
        ui_state.additional_input_source == common::InputSource::kAudioFile
            ? common::InputSource::kOff
            : ui_state.additional_input_source;
    application_input_identity_ = ui_state.application_input_identity;
    additional_application_input_identity_ =
        ui_state.additional_application_input_identity;
    input_file_path_.clear();
    additional_input_file_path_.clear();
    input_file_playing_ = false;
    input_file_loop_ = false;
    additional_file_playing_ = false;
    additional_file_loop_ = false;
    input_file_volume_ = ui_state.input_file_volume;
    additional_file_volume_ = ui_state.additional_file_volume;
    application_input_enabled_ =
        input_source_ == common::InputSource::kApplicationInput &&
        !application_input_identity_.empty();
    additional_input_enabled_ =
        additional_input_source_ == common::InputSource::kApplicationInput &&
        !additional_application_input_identity_.empty();
    direct_wasapi_config_.reset();
    if (ui_state.direct_wasapi_enabled &&
        !ui_state.direct_wasapi_device_id.empty()) {
      DirectWasapiConfig config;
      config.device_id = ui_state.direct_wasapi_device_id;
      config.mode = DirectWasapiMode::kShared;
      config.source_sample_rate = meter_sample_rate_;
      direct_wasapi_config_ = config;
      restored_direct_wasapi = config;
    }
  }

  // Runtime activity is deliberately not part of the saved state. Loading a
  // project must never start recording by itself.
  recorder_.Stop();
  SendRecordingStatusMessage();
  if (restored_direct_wasapi.has_value() &&
      restored_direct_wasapi->source_sample_rate > 0.0) {
    direct_wasapi_output_.Start(*restored_direct_wasapi);
  } else {
    direct_wasapi_output_.Stop();
  }
  application_input_.Stop();
  additional_application_input_.Stop();
  application_input_process_id_ = 0;
  additional_application_input_process_id_ = 0;
  return kResultTrue;
}

auto PLUGIN_API Processor::getState(IBStream* const state) -> tresult {
  auto parameter_state = std::string{};
  auto ui_state = PersistedPluginUiState{};
  {
    std::lock_guard<std::mutex> lock(mtx_);
    auto oss = std::ostringstream(std::ios::binary);
    if (audio_engine_.WriteState(oss) != common::ErrorCode::kSuccess) {
      return kResultFalse;
    }
    parameter_state = oss.str();
    if (direct_wasapi_config_.has_value() &&
        !direct_wasapi_config_->device_id.empty()) {
      ui_state.direct_wasapi_enabled = true;
      ui_state.direct_wasapi_exclusive = false;
      ui_state.direct_wasapi_device_id = direct_wasapi_config_->device_id;
    }
    ui_state.recording_mode =
        common::NormalizeRecordingMode(recording_mode_);
    ui_state.recording_path = recording_path_;
    ui_state.voice_delay_ms = recording_voice_delay_ms_;
    ui_state.additional_input_gain_db = recording_additional_input_gain_db_;
    ui_state.application_input_enabled = application_input_enabled_ &&
                                         !application_input_identity_.empty();
    ui_state.additional_input_enabled = additional_input_enabled_ &&
                                        !additional_application_input_identity_.empty();
    ui_state.application_input_identity = application_input_identity_;
    ui_state.additional_application_input_identity =
        additional_application_input_identity_;
    ui_state.input_source = input_source_ == common::InputSource::kAudioFile
                                ? common::InputSource::kDawInput
                                : input_source_;
    ui_state.additional_input_source =
        additional_input_source_ == common::InputSource::kAudioFile
            ? common::InputSource::kOff
            : additional_input_source_;
    // File-player state is deliberately not written back.  Keeping the
    // fields in the wire format preserves compatibility with old projects,
    // while newly saved state cannot resurrect the removed feature.
  }

  auto state_string = EncodePluginState(parameter_state, ui_state);
  if (!state_string.has_value()) {
    return kResultFalse;
  }
  auto siz = static_cast<int>(state_string->size());
  if (state->write(&siz, sizeof(siz)) != kResultTrue) {
    return kResultFalse;
  }
  if (state->write(state_string->data(),
                   static_cast<int>(state_string->size())) != kResultTrue) {
    return kResultFalse;
  }
  return kResultTrue;
}

auto PLUGIN_API Processor::notify(IMessage* const message) -> tresult {
  const auto* const message_id = message->getMessageID();
  if (std::strcmp(message_id, "recording_stop") == 0) {
    recorder_.Stop();
    SendRecordingStatusMessage();
    return kResultTrue;
  }
  if (std::strcmp(message_id, "recording_start") == 0) {
    auto* const attributes = message->getAttributes();
    if (attributes == nullptr) {
      return kResultFalse;
    }
    Steinberg::int64 mode_value = 0;
    Steinberg::int64 voice_delay_ms_value =
        static_cast<Steinberg::int64>(common::kMinBgmDelayMs) - 1;
    Steinberg::uint32 path_size = 0;
    const void* path_data = nullptr;
    if (attributes->getInt("mode", mode_value) != kResultTrue ||
        attributes->getBinary("base_path", path_data, path_size) !=
            kResultTrue ||
        path_data == nullptr || path_size == 0 || mode_value < 1 ||
        mode_value > 3) {
      return kResultFalse;
    }
    if (attributes->getInt("voice_delay_ms", voice_delay_ms_value) !=
            kResultTrue ||
        voice_delay_ms_value <
            static_cast<Steinberg::int64>(common::kMinBgmDelayMs) ||
        voice_delay_ms_value >
            static_cast<Steinberg::int64>(common::kMaxBgmDelayMs)) {
      voice_delay_ms_value =
          static_cast<Steinberg::int64>(common::kMinBgmDelayMs) - 1;
    }
    double requested_gain_db = common::kDefaultAdditionalInputGainDb;
    const auto has_valid_gain =
        attributes->getFloat("additional_input_gain_db", requested_gain_db) ==
            kResultTrue &&
        std::isfinite(requested_gain_db) &&
        requested_gain_db >= common::kMinAdditionalInputGainDb &&
        requested_gain_db <= common::kMaxAdditionalInputGainDb;
    auto sample_rate = 0.0;
    {
      std::lock_guard<std::mutex> lock(mtx_);
      sample_rate = meter_sample_rate_;
    }
    common::RecordingSettings settings;
    settings.mode = common::NormalizeRecordingMode(
        static_cast<common::RecordingMode>(mode_value));
    const auto* const path_begin = static_cast<const char*>(path_data);
    const auto path_utf8 = std::u8string(
        reinterpret_cast<const char8_t*>(path_begin),
        reinterpret_cast<const char8_t*>(path_begin + path_size));
    settings.base_path = std::filesystem::path(path_utf8);
    settings.sample_rate = sample_rate;
    {
      std::lock_guard<std::mutex> lock(mtx_);
      recording_mode_ = common::NormalizeRecordingMode(settings.mode);
      recording_path_ = settings.base_path;
      if (voice_delay_ms_value >=
              static_cast<Steinberg::int64>(common::kMinBgmDelayMs) &&
          voice_delay_ms_value <=
              static_cast<Steinberg::int64>(common::kMaxBgmDelayMs)) {
        recording_voice_delay_ms_ = common::ClampBgmDelayMs(
            static_cast<std::int32_t>(voice_delay_ms_value));
      }
      if (!has_valid_gain) {
        requested_gain_db = recording_additional_input_gain_db_;
      }
      recording_additional_input_gain_db_ = requested_gain_db;
      // ADD BGM is mixed into the final block before the recorder receives
      // it. Keep the legacy recorder-side mix/delay disabled so neither
      // control is applied twice.
      settings.additional_input_enabled = false;
      settings.additional_input_gain_db = recording_additional_input_gain_db_;
      settings.voice_delay_ms = 0;
    }
    if (!recorder_.Start(settings)) {
      SendRecordingStatusMessage();
      return kResultTrue;
    }
    SendRecordingStatusMessage();
    return kResultTrue;
  }
  if (std::strcmp(message_id, "recording_selection") == 0) {
    auto* const attributes = message->getAttributes();
    if (attributes == nullptr) {
      return kResultFalse;
    }
    Steinberg::int64 mode_value = 0;
    Steinberg::int64 voice_delay_ms_value =
        static_cast<Steinberg::int64>(common::kMinBgmDelayMs) - 1;
    double additional_input_gain_db_value =
        common::kDefaultAdditionalInputGainDb;
    if (attributes->getInt("mode", mode_value) != kResultTrue ||
        mode_value < 0 || mode_value > 3) {
      return kResultFalse;
    }
    if (attributes->getInt("voice_delay_ms", voice_delay_ms_value) ==
            kResultTrue &&
        voice_delay_ms_value >=
            static_cast<Steinberg::int64>(common::kMinBgmDelayMs) &&
        voice_delay_ms_value <=
            static_cast<Steinberg::int64>(common::kMaxBgmDelayMs)) {
      std::lock_guard<std::mutex> lock(mtx_);
      recording_voice_delay_ms_ = static_cast<std::int32_t>(
          voice_delay_ms_value);
    }
    if (attributes->getFloat("additional_input_gain_db",
                             additional_input_gain_db_value) == kResultTrue &&
        std::isfinite(additional_input_gain_db_value) &&
        additional_input_gain_db_value >=
            common::kMinAdditionalInputGainDb &&
        additional_input_gain_db_value <=
            common::kMaxAdditionalInputGainDb) {
      std::lock_guard<std::mutex> lock(mtx_);
      recording_additional_input_gain_db_ = additional_input_gain_db_value;
    }
    auto path = std::filesystem::path{};
    Steinberg::uint32 path_size = 0;
    const void* path_data = nullptr;
    if (attributes->getBinary("base_path", path_data, path_size) ==
            kResultTrue &&
        path_size != 0) {
      if (path_data == nullptr) {
        return kResultFalse;
      }
      const auto* const path_begin = static_cast<const char*>(path_data);
      const auto path_utf8 = std::u8string(
          reinterpret_cast<const char8_t*>(path_begin),
          reinterpret_cast<const char8_t*>(path_begin + path_size));
      path = std::filesystem::path(path_utf8);
    }
    const auto requested_mode = common::NormalizeRecordingMode(
        static_cast<common::RecordingMode>(mode_value));
    {
      std::lock_guard<std::mutex> lock(mtx_);
      recording_mode_ = requested_mode;
      recording_path_ = std::move(path);
    }
    return kResultTrue;
  }
  if (std::strcmp(message_id, "direct_wasapi_off") == 0) {
    {
      std::lock_guard<std::mutex> lock(mtx_);
      direct_wasapi_config_.reset();
    }
    direct_wasapi_output_.Stop();
    return kResultTrue;
  }
  if (std::strcmp(message_id, "direct_wasapi_config") == 0) {
    auto* const attributes = message->getAttributes();
    if (attributes == nullptr) {
      return kResultFalse;
    }
    uint32 size = 0;
    const void* data = nullptr;
    Steinberg::int64 exclusive = 0;
    if (attributes->getBinary("device_id", data, size) != kResultTrue ||
        data == nullptr || size == 0 ||
        attributes->getInt("exclusive", exclusive) != kResultTrue) {
      return kResultFalse;
    }
    DirectWasapiConfig config;
    config.device_id.assign(static_cast<const char*>(data), size);
    static_cast<void>(exclusive);
    config.mode = DirectWasapiMode::kShared;
    {
      std::lock_guard<std::mutex> lock(mtx_);
      config.source_sample_rate = meter_sample_rate_;
      direct_wasapi_config_ = config;
    }
    if (config.source_sample_rate <= 0.0) {
      SendDirectWasapiStatusMessage(
          DirectWasapiStatus::kUnavailable,
          "The host sample rate is not available yet.");
      return kResultTrue;
    }
    direct_wasapi_output_.Start(config);
    return kResultTrue;
  }
  if (std::strcmp(message_id, "application_input_off") == 0) {
    {
      std::lock_guard<std::mutex> lock(mtx_);
      application_input_enabled_ = false;
      application_input_identity_.clear();
      application_input_process_id_ = 0;
      input_source_ = common::InputSource::kDawInput;
    }
    application_input_.Stop();
    return kResultTrue;
  }
  if (std::strcmp(message_id, "application_input_main_off") == 0) {
    {
      std::lock_guard<std::mutex> lock(mtx_);
      application_input_enabled_ = false;
      input_source_ = common::InputSource::kDawInput;
    }
    return kResultTrue;
  }
  if (std::strcmp(message_id, "application_input_clear") == 0) {
    {
      std::lock_guard<std::mutex> lock(mtx_);
      application_input_enabled_ = false;
      application_input_identity_.clear();
      application_input_process_id_ = 0;
      input_source_ = common::InputSource::kDawInput;
    }
    application_input_.Stop();
    return kResultTrue;
  }
  if (std::strcmp(message_id, "additional_input_off") == 0) {
    {
      std::lock_guard<std::mutex> lock(mtx_);
      additional_input_enabled_ = false;
      additional_application_input_identity_.clear();
      additional_application_input_process_id_ = 0;
      additional_input_source_ = common::InputSource::kOff;
    }
    additional_application_input_.Stop();
    return kResultTrue;
  }
  if (std::strcmp(message_id, "application_input_config") == 0) {
    auto* const attributes = message->getAttributes();
    if (attributes == nullptr) {
      return kResultFalse;
    }
    uint32 size = 0;
    const void* data = nullptr;
    if (attributes->getBinary("identity", data, size) != kResultTrue ||
        data == nullptr || size == 0) {
      return kResultFalse;
    }
    auto identity = std::string(static_cast<const char*>(data), size);
    {
      std::lock_guard<std::mutex> lock(mtx_);
      application_input_enabled_ = true;
      application_input_identity_ = identity;
      input_source_ = common::InputSource::kApplicationInput;
    }
    // Resolve the current PID from the identity again inside the processor.
    // The editor's menu can outlive an audio-session refresh, and a host may
    // have restarted the selected application in the meantime.
    if (meter_sample_rate_ <= 0.0 ||
        !StartApplicationInput(application_input_, application_input_process_id_,
                               identity)) {
      return kResultTrue;
    }
    return kResultTrue;
  }
  if (std::strcmp(message_id, "additional_input_config") == 0) {
    auto* const attributes = message->getAttributes();
    if (attributes == nullptr) {
      return kResultFalse;
    }
    uint32 size = 0;
    const void* data = nullptr;
    Steinberg::int64 enabled = 0;
    if (attributes->getBinary("identity", data, size) != kResultTrue ||
        data == nullptr || size == 0 ||
        attributes->getInt("enabled", enabled) != kResultTrue) {
      return kResultFalse;
    }
    auto identity = std::string(static_cast<const char*>(data), size);
    {
      std::lock_guard<std::mutex> lock(mtx_);
      additional_application_input_identity_ = identity;
      additional_input_enabled_ = enabled != 0;
      additional_input_source_ = enabled != 0
                                     ? common::InputSource::kApplicationInput
                                     : common::InputSource::kOff;
    }
    if (additional_input_enabled_ && meter_sample_rate_ > 0.0) {
      static_cast<void>(StartApplicationInput(
          additional_application_input_, additional_application_input_process_id_,
          identity));
    }
    return kResultTrue;
  }
  if (std::strcmp(message_id, "input_file_volume") == 0 ||
      std::strcmp(message_id, "additional_file_volume") == 0) {
    const auto additional =
        std::strcmp(message_id, "additional_file_volume") == 0;
    auto* const attributes = message->getAttributes();
    if (attributes == nullptr) {
      return kResultFalse;
    }
    double volume = 1.0;
    if (attributes->getFloat("volume", volume) != kResultTrue ||
        !std::isfinite(volume)) {
      return kResultFalse;
    }
    volume = std::clamp(volume, 0.0, 1.0);
    // This is intentionally lock-free: the editor sends this message for
    // every pointer movement, and taking the processing mutex here could make
    // the realtime callback return a silent block.
    if (additional) {
      additional_file_volume_.store(volume, std::memory_order_relaxed);
    } else {
      input_file_volume_.store(volume, std::memory_order_relaxed);
    }
    return kResultTrue;
  }
  if (std::strcmp(message_id, "input_source_config") == 0 ||
      std::strcmp(message_id, "additional_input_source_config") == 0) {
    const auto additional =
        std::strcmp(message_id, "additional_input_source_config") == 0;
    auto* const attributes = message->getAttributes();
    if (attributes == nullptr) return kResultFalse;
    Steinberg::int64 source_value = 0;
    if (attributes->getInt("source", source_value) != kResultTrue ||
        source_value < 0 || source_value > 3) return kResultFalse;
    const auto requested_source =
        static_cast<common::InputSource>(source_value);
    const auto source =
        requested_source == common::InputSource::kAudioFile
            ? (additional ? common::InputSource::kOff
                          : common::InputSource::kDawInput)
            : requested_source;
    auto previous_source = common::InputSource::kDawInput;
    {
      std::lock_guard<std::mutex> lock(mtx_);
      if (additional) {
        previous_source = additional_input_source_;
        additional_input_source_ = source;
        additional_input_file_path_.clear();
        additional_file_playing_ = false;
        additional_file_loop_ = false;
        additional_file_volume_.store(1.0, std::memory_order_relaxed);
        additional_input_enabled_ =
            source == common::InputSource::kApplicationInput &&
            !additional_application_input_identity_.empty();
      } else {
        previous_source = input_source_;
        input_source_ = source;
        input_file_path_.clear();
        input_file_playing_ = false;
        input_file_loop_ = false;
        input_file_volume_.store(1.0, std::memory_order_relaxed);
        application_input_enabled_ =
            source == common::InputSource::kApplicationInput &&
            !application_input_identity_.empty();
      }
    }
    if (additional) {
      if (previous_source == common::InputSource::kApplicationInput &&
          source != common::InputSource::kApplicationInput) {
        additional_application_input_.Stop();
      }
    } else {
      if (previous_source == common::InputSource::kApplicationInput &&
          source != common::InputSource::kApplicationInput) {
        application_input_.Stop();
      }
    }
    if (source == common::InputSource::kApplicationInput) {
      auto identity = std::string{};
      if (additional) {
        identity = additional_application_input_identity_;
      } else {
        identity = application_input_identity_;
      }
      if (!identity.empty() && meter_sample_rate_ > 0.0) {
        static_cast<void>(StartApplicationInput(
            additional ? additional_application_input_ : application_input_,
            additional ? additional_application_input_process_id_
                       : application_input_process_id_,
            identity));
      }
    }
    return kResultTrue;
  }
  if (std::strcmp(message_id, "param_change") == 0) {
    uint32 siz;
    const void* data;
    if (message->getAttributes()->getBinary("param_id", data, siz) !=
        kResultTrue) {
      return kResultFalse;
    }
    if (siz != sizeof(ParamID)) {
      return kResultFalse;
    }
    ParamID vst_param_id;
    std::memcpy(&vst_param_id, data, sizeof(vst_param_id));
    const auto param_id = static_cast<common::ParameterID>(vst_param_id);
    if (param_id == common::ParameterID::kLatencyReporting) {
      Steinberg::int64 value;
      if (message->getAttributes()->getInt("data", value) != kResultTrue) {
        return kResultFalse;
      }
      {
        std::lock_guard<std::mutex> lock(mtx_);
        [[maybe_unused]] const auto error_code =
            audio_engine_.SetParameter(param_id, static_cast<int>(value));
      }
      sendMessageID("latency_changed");
      return kResultTrue;
    }
    if (message->getAttributes()->getBinary("data", data, siz) != kResultTrue) {
      return kResultFalse;
    }
    auto value = std::u8string();
    value.resize(siz);
    std::memcpy(value.data(), data, siz);
    // Controller 側の状態との整合性を維持するため、
    // Controller 側や Host から送られた設定値は、たとえ不正なものでも
    // なるべくそのまま保持する。
    {
      std::lock_guard<std::mutex> lock(mtx_);
      [[maybe_unused]] const auto error_code =
          audio_engine_.SetParameter(param_id, value);
    }
    if (param_id == common::ParameterID::kModel) {
      sendMessageID("latency_changed");
    }
    return kResultTrue;
  }
  return AudioEffect::notify(message);
}

}  // namespace beatrice::vst
