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
      }) {
  // 対応するコントローラクラスを設定する
  setControllerClass(kControllerUID);
}

Processor::~Processor() {
  recorder_.Stop();
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

  // In/Out バスの生成
  addAudioInput(STR16("AudioInput"), SpeakerArr::kMono);
  addAudioOutput(STR16("AudioOutput"), SpeakerArr::kMono);

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
  // 入力バス・出力バスの数はいずれも 1
  if (numIns == 1 && numOuts == 1 &&
      (inputs[0] == SpeakerArr::kMono || inputs[0] == SpeakerArr::kStereo) &&
      (outputs[0] == SpeakerArr::kMono || outputs[0] == SpeakerArr::kStereo)) {
    return AudioEffect::setBusArrangements(inputs, numIns, outputs, numOuts);
  }

  return kResultFalse;
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
  input_meter_buffer_.assign(
      static_cast<std::size_t>(std::max(0, setup.maxSamplesPerBlock)), 0.0F);
  if (direct_wasapi_config_.has_value()) {
    direct_wasapi_config_->source_sample_rate = setup.sampleRate;
    direct_wasapi_output_.Start(*direct_wasapi_config_);
  }
  return AudioEffect::setupProcessing(setup);
}

auto PLUGIN_API Processor::setActive(const TBool state) -> tresult {
  if (state) {
    if (direct_wasapi_config_.has_value() && meter_sample_rate_ > 0.0) {
      direct_wasapi_config_->source_sample_rate = meter_sample_rate_;
      direct_wasapi_output_.Start(*direct_wasapi_config_);
    }
  } else {
    recorder_.Stop();
    SendRecordingStatusMessage();
    direct_wasapi_output_.Stop();
    // メモリの解放など
    std::lock_guard<std::mutex> lock(mtx_);
    const auto error_code = audio_engine_.ResetContext();
    assert(error_code == common::ErrorCode::kSuccess);
    meter_frames_ = 0;
    meter_input_peak_ = 0.0F;
    meter_output_peak_ = 0.0F;
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

// TODO(bug): tail を設定する

auto PLUGIN_API Processor::getLatencySamples() -> uint32 {
  const std::scoped_lock lock(mtx_);
  const auto latency_reporting =
      std::get<int>(vc_core_.GetParameterState().GetValue(
          common::ParameterID::kLatencyReporting));
  if (latency_reporting == 0) {
    return 0;
  }
  return static_cast<uint32>(vc_core_.GetCore()->GetLatencySamples());
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

  if (data.numInputs == 0 || data.numOutputs == 0 || data.numSamples == 0) {
    // 何もしない
    return kResultOk;
  }

  // double は処理しない
  if (data.symbolicSampleSize == Steinberg::Vst::kSample64) {
    return kResultOk;
  }

  // チャンネル数を確認
  if (data.inputs[0].numChannels < 1) {
    return kResultOk;
  }
  if (data.outputs[0].numChannels < 1) {
    return kResultOk;
  }

  // 出力バス 0 のチャンネル 0 に入力をコピー
  const float* const in0 = data.inputs[0].channelBuffers32[0];
  float* const out0 = data.outputs[0].channelBuffers32[0];
  float* const out1 = data.outputs[0].numChannels >= 2
                          ? data.outputs[0].channelBuffers32[1]
                          : nullptr;
  const float* const in1 = data.inputs[0].numChannels >= 2
                               ? data.inputs[0].channelBuffers32[1]
                               : nullptr;
  const auto host_input_silent = data.inputs[0].silenceFlags != 0;
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

  auto block_output_peak = 0.0F;
  for (auto i = 0; i < data.numSamples; ++i) {
    block_output_peak = std::max(block_output_peak, std::abs(out0[i]));
    if (out1 != nullptr) {
      block_output_peak = std::max(block_output_peak, std::abs(out1[i]));
    }
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
  // The optional WASAPI path receives a copy of the processed signal. Host
  // output buffers remain untouched and continue to be the normal VST output.
  direct_wasapi_output_.PushBlock(
      out0, out1, static_cast<std::size_t>(data.numSamples));
  if (recorder_.IsRecording()) {
    for (auto i = 0; i < data.numSamples; ++i) {
      recorder_.Push(input_meter_buffer_[i], out0[i],
                     out1 != nullptr ? out1[i] : out0[i]);
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
    recording_mode_ = ui_state.recording_mode;
    recording_path_ = ui_state.recording_path;
    direct_wasapi_config_.reset();
    if (ui_state.direct_wasapi_enabled &&
        !ui_state.direct_wasapi_device_id.empty()) {
      DirectWasapiConfig config;
      config.device_id = ui_state.direct_wasapi_device_id;
      config.mode = ui_state.direct_wasapi_exclusive
                        ? DirectWasapiMode::kExclusive
                        : DirectWasapiMode::kShared;
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
      ui_state.direct_wasapi_exclusive =
          direct_wasapi_config_->mode == DirectWasapiMode::kExclusive;
      ui_state.direct_wasapi_device_id = direct_wasapi_config_->device_id;
    }
    ui_state.recording_mode = recording_mode_;
    ui_state.recording_path = recording_path_;
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
    Steinberg::uint32 path_size = 0;
    const void* path_data = nullptr;
    if (attributes->getInt("mode", mode_value) != kResultTrue ||
        attributes->getBinary("base_path", path_data, path_size) !=
            kResultTrue ||
        path_data == nullptr || path_size == 0 || mode_value < 1 ||
        mode_value > 3) {
      return kResultFalse;
    }
    auto sample_rate = 0.0;
    {
      std::lock_guard<std::mutex> lock(mtx_);
      sample_rate = meter_sample_rate_;
    }
    common::RecordingSettings settings;
    settings.mode = static_cast<common::RecordingMode>(mode_value);
    const auto* const path_begin = static_cast<const char*>(path_data);
    const auto path_utf8 = std::u8string(
        reinterpret_cast<const char8_t*>(path_begin),
        reinterpret_cast<const char8_t*>(path_begin + path_size));
    settings.base_path = std::filesystem::path(path_utf8);
    settings.sample_rate = sample_rate;
    {
      std::lock_guard<std::mutex> lock(mtx_);
      recording_mode_ = settings.mode;
      recording_path_ = settings.base_path;
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
    if (attributes->getInt("mode", mode_value) != kResultTrue ||
        mode_value < 0 || mode_value > 3) {
      return kResultFalse;
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
    {
      std::lock_guard<std::mutex> lock(mtx_);
      recording_mode_ = static_cast<common::RecordingMode>(mode_value);
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
    config.mode = exclusive != 0 ? DirectWasapiMode::kExclusive
                                 : DirectWasapiMode::kShared;
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
