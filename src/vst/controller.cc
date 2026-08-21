// Copyright (c) 2024-2026 Project Beatrice and Contributors

#include "vst/controller.h"

#include <algorithm>
#include <cassert>
#include <cstring>
#include <ios>
#include <memory>
#include <sstream>
#include <string>
#include <utility>
#include <variant>

#include "vst3sdk/pluginterfaces/base/fplatform.h"
#include "vst3sdk/pluginterfaces/base/funknown.h"
#include "vst3sdk/pluginterfaces/vst/ivsteditcontroller.h"
#include "vst3sdk/pluginterfaces/vst/ivstunits.h"
#include "vst3sdk/pluginterfaces/vst/ivsteditcontroller.h"
#include "vst3sdk/public.sdk/source/vst/utility/stringconvert.h"
#include "vst3sdk/public.sdk/source/vst/vsteditcontroller.h"
#include "vst3sdk/public.sdk/source/vst/vstparameters.h"

// Beatrice
#include "common/parameter_schema.h"
#include "common/parameter_state.h"
#include "vst/editor.h"
#include "vst/parameter.h"
#include "vst/plugin_state.h"

namespace beatrice::vst {

using Steinberg::kResultFalse;
using Steinberg::kResultTrue;
using Steinberg::Vst::kRootUnitId;
using Steinberg::Vst::StringListParameter;

Controller::~Controller() = default;

auto PLUGIN_API Controller::initialize(FUnknown* const context) -> tresult {
  const tresult result = EditController::initialize(context);
  if (result != kResultTrue) {
    return kResultTrue;  // ???
  }

  InitializeParameters();
  return kResultTrue;
}

void Controller::InitializeParameters() {
  if (parameters_initialized_) {
    return;
  }
  parameters_initialized_ = true;
  for (const auto& [param_id, param] : common::kSchema) {
    const auto vst_param_id = static_cast<ParamID>(param_id);
    if (const auto* const num_param =
            std::get_if<common::NumberParameter>(&param)) {
      parameters.addParameter(new LinearParameter(
          Steinberg::Vst::StringConvert::convert(
              reinterpret_cast<const char*>(num_param->GetName().c_str()))
              .c_str(),
          vst_param_id,
          Steinberg::Vst::StringConvert::convert(
              reinterpret_cast<const char*>(num_param->GetUnits().c_str()))
              .c_str(),
          num_param->GetMinValue(), num_param->GetMaxValue(),
          num_param->GetDefaultValue(), num_param->GetDivisions(),
          num_param->GetFlags(), kRootUnitId,
          Steinberg::Vst::StringConvert::convert(
              reinterpret_cast<const char*>(num_param->GetShortName().c_str()))
              .c_str()));
    } else if (const auto* const list_param =
                   std::get_if<common::ListParameter>(&param)) {
      auto* const param = new StringListParameter(
          Steinberg::Vst::StringConvert::convert(
              reinterpret_cast<const char*>(list_param->GetName().c_str()))
              .c_str(),
          vst_param_id, nullptr, list_param->GetFlags(), kRootUnitId,
          Steinberg::Vst::StringConvert::convert(
              reinterpret_cast<const char*>(list_param->GetShortName().c_str()))
              .c_str());
      for (const auto& value : list_param->GetValues()) {
        param->appendString(Steinberg::Vst::StringConvert::convert(
                                reinterpret_cast<const char*>(value.c_str()))
                                .c_str());
      }
      const auto default_normalized =
          param->toNormalized(list_param->GetDefaultValue());
      param->getInfo().defaultNormalizedValue = default_normalized;
      param->setNormalized(default_normalized);
      parameters.addParameter(param);
    } else if (std::get_if<common::StringParameter>(&param)) {
    } else {
      assert(false);
    }
  }
}

// 状態を読み出す。
// Host 側から初期化時やプリセットロード時に呼ばれる。
// 不正なファイルパスなども構わず読み込む。
auto PLUGIN_API Controller::setComponentState(IBStream* const state)
    -> tresult {
  if (state == nullptr) {
    return kResultFalse;
  }
  int siz;
  if (state->read(&siz, sizeof(siz)) != kResultTrue) {
    return kResultFalse;
  }
  auto state_string = std::string();
  state_string.resize(siz);
  if (state->read(std::to_address(state_string.begin()), siz) != kResultTrue) {
    return kResultFalse;
  }
  auto parameter_state_string = std::string{};
  auto ui_state = PersistedPluginUiState{};
  if (!DecodePluginState(state_string, parameter_state_string, ui_state)) {
    return kResultFalse;
  }
  auto iss = std::istringstream(parameter_state_string, std::ios::binary);
  common::ParameterState tmp_parameter_state;
  // Processor 側との整合性を維持するため、
  // Host から与えられた値はなるべくそのまま保持する。
  [[maybe_unused]] const auto error_code =
      tmp_parameter_state.ReadOrSetDefault(iss, common::kSchema);
  for (const auto& [param_id, param] : common::kSchema) {
    const auto vst_param_id = static_cast<ParamID>(param_id);
    const auto& value = tmp_parameter_state.GetValue(param_id);
    if (const auto* const num_param =
            std::get_if<common::NumberParameter>(&param)) {
      const auto normalized_value =
          Normalize(*num_param, std::get<double>(value));
      setParamNormalized(vst_param_id, normalized_value);
    } else if (const auto* const list_param =
                   std::get_if<common::ListParameter>(&param)) {
      const auto normalized_value =
          Normalize(*list_param, std::get<int>(value));
      setParamNormalized(vst_param_id, normalized_value);
    } else if (std::get_if<common::StringParameter>(&param)) {
      static_cast<void>(SetStringParameter(
          vst_param_id, *std::get<std::unique_ptr<std::u8string>>(value)));
    } else {
      assert(false);
    }
  }
  if (ui_state.direct_wasapi_enabled &&
      !ui_state.direct_wasapi_device_id.empty()) {
    SetDirectWasapiSelection(ui_state.direct_wasapi_device_id,
                             ui_state.direct_wasapi_exclusive);
  } else {
    ClearDirectWasapiSelection();
  }
  SetRecordingSelection(ui_state.recording_mode, ui_state.recording_path);
  for (auto* const editor : editors_) {
    if (editor != nullptr) {
      editor->SyncVstExternalState();
    }
  }
  if (componentHandler) {
    componentHandler->restartComponent(Steinberg::Vst::kLatencyChanged);
  }
  state->seek(0, IBStream::IStreamSeekMode::kIBSeekSet);
  return kResultTrue;
}

auto Controller::InitializeStandalone(ParameterSink sink) -> bool {
  parameter_sink_ = std::move(sink);
  InitializeParameters();
  return parameters_initialized_;
}

auto Controller::ReadStandaloneState(std::istream& stream)
    -> common::ErrorCode {
  auto state = common::ParameterState{};
  const auto read_error =
      state.ReadOrSetDefault(stream, common::kSchema);
  if (read_error != common::ErrorCode::kSuccess) {
    return read_error;
  }
  for (const auto& [param_id, parameter] : common::kSchema) {
    const auto vst_param_id = static_cast<ParamID>(param_id);
    const auto& value = state.GetValue(param_id);
    if (const auto* const number =
            std::get_if<common::NumberParameter>(&parameter)) {
      if (setParamNormalized(
              vst_param_id,
              Normalize(*number, std::get<double>(value))) != kResultTrue) {
        return common::ErrorCode::kUnknownError;
      }
    } else if (const auto* const list =
                   std::get_if<common::ListParameter>(&parameter)) {
      if (setParamNormalized(vst_param_id,
                             Normalize(*list, std::get<int>(value))) !=
          kResultTrue) {
        return common::ErrorCode::kUnknownError;
      }
    } else if (std::get_if<common::StringParameter>(&parameter)) {
      const auto& string_value =
          std::get<std::unique_ptr<std::u8string>>(value);
      if (string_value == nullptr ||
          SetStringParameter(vst_param_id, *string_value) !=
              common::ErrorCode::kSuccess) {
        return common::ErrorCode::kUnknownError;
      }
    } else {
      return common::ErrorCode::kUnknownError;
    }
  }
  return common::ErrorCode::kSuccess;
}

auto Controller::CreateStandaloneEditor() -> Editor* {
  auto* const editor = new Editor(this);
  editors_.push_back(editor);
  return editor;
}

auto PLUGIN_API Controller::createView(const char* const name) -> IPlugView* {
  if (std::strcmp(name, "editor") == 0) {
    auto* const editor = new Editor(this);
    editors_.push_back(editor);
    return editor;
  }
  return nullptr;
}

void Controller::editorDestroyed(EditorView* const editor) {
  const auto itr = std::ranges::find(editors_, editor);
  if (itr == editors_.end()) {
    return;
  }
  *itr = editors_.back();
  editors_.pop_back();
}

// Host や Editor から呼ばれる
// ここで performEdit を呼ぶと param_id が異なっても
// DAW に拒否されるようなので、DAW からオートメーションなどで
// パラメータが操作された場合の他のパラメータとの連携には
// 定期的な同期が必要になりそうで、面倒なので諦める
auto PLUGIN_API Controller::setParamNormalized(
    const ParamID vst_param_id, const ParamValue normalized_value) -> tresult {
  const auto param_id = static_cast<common::ParameterID>(vst_param_id);
  const auto& param = common::kSchema.GetParameter(param_id);
  float plain_value_for_editor;
  if (const auto* const num_param =
          std::get_if<common::NumberParameter>(&param)) {
    const auto plain_value = Denormalize(*num_param, normalized_value);
    core_.parameter_state_.SetValue(param_id, plain_value);
    plain_value_for_editor = static_cast<float>(plain_value);
  } else if (const auto* const list_param =
                 std::get_if<common::ListParameter>(&param)) {
    const auto plain_value = Denormalize(*list_param, normalized_value);
    core_.parameter_state_.SetValue(param_id, plain_value);
    plain_value_for_editor = static_cast<float>(plain_value);
  } else if (std::get_if<common::StringParameter>(&param)) {
    return kResultFalse;
  } else {
    assert(false);
    return kResultFalse;
  }

  const auto result =
      EditController::setParamNormalized(vst_param_id, normalized_value);
  if (result != kResultTrue) {
    return result;
  }
  if (parameter_sink_ &&
      parameter_sink_(param_id, core_.parameter_state_.GetValue(param_id)) !=
          common::ErrorCode::kSuccess) {
    return kResultFalse;
  }
  for (auto&& editor : editors_) {
    editor->SyncValue(vst_param_id, plain_value_for_editor);
  }

  return kResultTrue;
}

// setParamNormalized の文字列パラメータ版で、Editor から呼ばれる他、
// Host 側からも初期化時やプリセットロード時に
// setComponentState を通して呼ばれる。
auto Controller::SetStringParameter(const ParamID vst_param_id,
                                    const std::u8string& value,
                                    const Editor* const source_editor)
    -> common::ErrorCode {
  const auto param_id = static_cast<common::ParameterID>(vst_param_id);
  core_.parameter_state_.SetValue(param_id, value);

  if (parameter_sink_) {
    const auto error =
        parameter_sink_(param_id, core_.parameter_state_.GetValue(param_id));
    if (error != common::ErrorCode::kSuccess) {
      return error;
    }
  }

  for (auto&& editor : editors_) {
    // The editor which produced preset data already owns the authoritative
    // in-memory workspace. Feeding the serialized data straight back into it
    // would parse the whole workspace and rebuild every preset row on each
    // parameter edit.
    if (editor == source_editor) {
      continue;
    }
    editor->SyncStringValue(vst_param_id, value);
  }
  return common::ErrorCode::kSuccess;
}

auto PLUGIN_API Controller::notify(IMessage* const message) -> tresult {
  if (std::strcmp(message->getMessageID(), "direct_wasapi_status") == 0) {
    auto* const attributes = message->getAttributes();
    Steinberg::int64 status = 0;
    if (attributes == nullptr ||
        attributes->getInt("status", status) != kResultTrue) {
      return kResultFalse;
    }
    status = std::clamp<Steinberg::int64>(status, 0, 3);
    auto error = std::string{};
    Steinberg::uint32 size = 0;
    const void* data = nullptr;
    if (attributes->getBinary("error", data, size) == kResultTrue &&
        data != nullptr && size != 0) {
      error.assign(static_cast<const char*>(data), size);
    }
    direct_wasapi_status_.store(static_cast<DirectWasapiStatus>(status),
                                std::memory_order_release);
    {
      std::lock_guard<std::mutex> lock(direct_wasapi_mutex_);
      direct_wasapi_error_ = std::move(error);
    }
    return kResultTrue;
  }
  if (std::strcmp(message->getMessageID(), "audio_levels") == 0) {
    double input_peak = 0.0;
    double output_peak = 0.0;
    auto* const attributes = message->getAttributes();
    if (attributes == nullptr ||
        attributes->getFloat("input_peak", input_peak) != kResultTrue ||
        attributes->getFloat("output_peak", output_peak) != kResultTrue) {
      return kResultFalse;
    }
    input_peak_.store(static_cast<float>(std::clamp(input_peak, 0.0, 2.0)),
                      std::memory_order_relaxed);
    output_peak_.store(static_cast<float>(std::clamp(output_peak, 0.0, 2.0)),
                       std::memory_order_relaxed);
    return kResultTrue;
  }
  if (std::strcmp(message->getMessageID(), "recording_status") == 0) {
    Steinberg::int64 recording = 0;
    Steinberg::int64 frames = 0;
    Steinberg::int64 dropped_frames = 0;
    auto* const attributes = message->getAttributes();
    if (attributes == nullptr ||
        attributes->getInt("recording", recording) != kResultTrue ||
        attributes->getInt("frames", frames) != kResultTrue ||
        attributes->getInt("dropped_frames", dropped_frames) !=
            kResultTrue) {
      return kResultFalse;
    }
    auto error = std::string{};
    Steinberg::uint32 size = 0;
    const void* data = nullptr;
    if (attributes->getBinary("error", data, size) == kResultTrue &&
        data != nullptr && size != 0) {
      error.assign(static_cast<const char*>(data), size);
    }
    recording_.store(recording != 0, std::memory_order_release);
    recording_frames_.store(
        static_cast<std::uint64_t>(std::max<Steinberg::int64>(0, frames)),
        std::memory_order_relaxed);
    recording_dropped_frames_.store(
        static_cast<std::uint64_t>(
            std::max<Steinberg::int64>(0, dropped_frames)),
        std::memory_order_relaxed);
    {
      std::lock_guard<std::mutex> lock(recording_mutex_);
      recording_error_ = std::move(error);
    }
    return kResultTrue;
  }
  if (std::strcmp(message->getMessageID(), "latency_changed") == 0) {
    if (componentHandler) {
      return componentHandler->restartComponent(
          Steinberg::Vst::kLatencyChanged);
    }
    return kResultTrue;
  }
  return EditController::notify(message);
}

void Controller::SetDirectWasapiSelection(std::string device_id,
                                           const bool exclusive) {
  std::lock_guard<std::mutex> lock(direct_wasapi_mutex_);
  direct_wasapi_device_id_ = std::move(device_id);
  direct_wasapi_exclusive_ = exclusive;
}

void Controller::ClearDirectWasapiSelection() {
  std::lock_guard<std::mutex> lock(direct_wasapi_mutex_);
  direct_wasapi_device_id_.clear();
  direct_wasapi_exclusive_ = false;
}

void Controller::GetDirectWasapiSelection(std::string& device_id,
                                          bool& exclusive) const {
  std::lock_guard<std::mutex> lock(direct_wasapi_mutex_);
  device_id = direct_wasapi_device_id_;
  exclusive = direct_wasapi_exclusive_;
}

void Controller::GetDirectWasapiStatus(DirectWasapiStatus& status,
                                       std::string& error) const {
  status = direct_wasapi_status_.load(std::memory_order_acquire);
  std::lock_guard<std::mutex> lock(direct_wasapi_mutex_);
  error = direct_wasapi_error_;
}

void Controller::SetRecordingSelection(const common::RecordingMode mode,
                                       std::filesystem::path path) {
  std::lock_guard<std::mutex> lock(recording_mutex_);
  recording_mode_ = mode;
  recording_path_ = std::move(path);
}

void Controller::GetRecordingSelection(common::RecordingMode& mode,
                                       std::filesystem::path& path) const {
  std::lock_guard<std::mutex> lock(recording_mutex_);
  mode = recording_mode_;
  path = recording_path_;
}

void Controller::GetRecordingStatus(common::RecordingStatus& status) const {
  status.recording = recording_.load(std::memory_order_acquire);
  status.frames = recording_frames_.load(std::memory_order_relaxed);
  status.dropped_frames =
      recording_dropped_frames_.load(std::memory_order_relaxed);
  std::lock_guard<std::mutex> lock(recording_mutex_);
  status.error = recording_error_;
}

auto PLUGIN_API Controller::notify(IMessage* const message) -> tresult {
  if (std::strcmp(message->getMessageID(), "latency_changed") == 0) {
    if (componentHandler) {
      return componentHandler->restartComponent(
          Steinberg::Vst::kLatencyChanged);
    }
    return kResultTrue;
  }
  return EditController::notify(message);
}

}  // namespace beatrice::vst
