// Copyright (c) 2024-2026 Project Beatrice and Contributors

#ifndef BEATRICE_VST_CONTROLLER_H_
#define BEATRICE_VST_CONTROLLER_H_

#include <atomic>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <istream>
#include <mutex>
#include <string>
#include <vector>

#include "vst3sdk/pluginterfaces/base/ftypes.h"
#include "vst3sdk/pluginterfaces/base/ibstream.h"
#include "vst3sdk/public.sdk/source/vst/vsteditcontroller.h"

// Beatrice
#include "common/controller_core.h"
#include "common/audio_recorder.h"
#include "vst/direct_wasapi_output.h"
#include "vst/editor.h"

namespace beatrice::vst {

class Controller : public Steinberg::Vst::EditController {
  using tresult = Steinberg::tresult;
  using IBStream = Steinberg::IBStream;
  using IPlugView = Steinberg::IPlugView;
  using IMessage = Steinberg::Vst::IMessage;
  using EditorView = Steinberg::Vst::EditorView;
  using ParamID = Steinberg::Vst::ParamID;
  using ParamValue = Steinberg::Vst::ParamValue;
  using IMessage = Steinberg::Vst::IMessage;

 public:
  using ParameterSink = std::function<common::ErrorCode(
      common::ParameterID, const common::ParameterState::Value&)>;

  ~Controller() override;

  // NOLINTNEXTLINE(readability-identifier-naming)
  static auto createInstance(void*) -> FUnknown* {
    return static_cast<IEditController*>(new Controller());
  }

  // from IPluginBase
  auto PLUGIN_API initialize(FUnknown* context) -> tresult SMTG_OVERRIDE;

  // from EditController
  auto PLUGIN_API setComponentState(IBStream* state) -> tresult SMTG_OVERRIDE;
  auto PLUGIN_API notify(IMessage* message) -> tresult SMTG_OVERRIDE;

  auto PLUGIN_API createView(const char* name) -> IPlugView* SMTG_OVERRIDE;

  void editorDestroyed(EditorView* editorView) SMTG_OVERRIDE;

  auto PLUGIN_API setParamNormalized(ParamID param_id, ParamValue value)
      -> tresult SMTG_OVERRIDE;

  auto InitializeStandalone(ParameterSink sink) -> bool;
  auto ReadStandaloneState(std::istream& stream) -> common::ErrorCode;
  auto CreateStandaloneEditor() -> Editor*;
  [[nodiscard]] auto GetCore() -> common::ControllerCore& { return core_; }
  void GetAudioLevels(float& input_peak, float& output_peak) const noexcept {
    input_peak = input_peak_.load(std::memory_order_relaxed);
    output_peak = output_peak_.load(std::memory_order_relaxed);
  }
  void SetDirectWasapiSelection(std::string device_id, bool exclusive);
  void ClearDirectWasapiSelection();
  void GetDirectWasapiSelection(std::string& device_id,
                                bool& exclusive) const;
  void GetDirectWasapiStatus(DirectWasapiStatus& status,
                             std::string& error) const;
  void SetRecordingSelection(common::RecordingMode mode,
                             std::filesystem::path path);
  void GetRecordingSelection(common::RecordingMode& mode,
                             std::filesystem::path& path) const;
  void GetRecordingStatus(common::RecordingStatus& status) const;

 private:
  common::ControllerCore core_;
  std::vector<Editor*> editors_;
  ParameterSink parameter_sink_;
  bool parameters_initialized_ = false;
  std::atomic<float> input_peak_{0.0F};
  std::atomic<float> output_peak_{0.0F};
  mutable std::mutex direct_wasapi_mutex_;
  std::string direct_wasapi_device_id_;
  bool direct_wasapi_exclusive_ = false;
  std::atomic<DirectWasapiStatus> direct_wasapi_status_{
      DirectWasapiStatus::kOff};
  std::string direct_wasapi_error_;

  mutable std::mutex recording_mutex_;
  common::RecordingMode recording_mode_ = common::RecordingMode::kOff;
  std::filesystem::path recording_path_;
  std::atomic_bool recording_{false};
  std::atomic<std::uint64_t> recording_frames_{0};
  std::atomic<std::uint64_t> recording_dropped_frames_{0};
  std::string recording_error_;

  void InitializeParameters();
  auto SetStringParameter(ParamID, const std::u8string&,
                          const Editor* source_editor = nullptr)
      -> common::ErrorCode;
  friend Editor;
};

}  // namespace beatrice::vst

#endif  // BEATRICE_VST_CONTROLLER_H_
