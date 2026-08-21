// Copyright (c) 2024-2026 Project Beatrice and Contributors

#ifndef BEATRICE_VST_PROCESSOR_H_
#define BEATRICE_VST_PROCESSOR_H_

#include <cstdint>
#include <filesystem>
#include <map>
#include <mutex>  // NOLINT(build/c++11)
#include <optional>
#include <string>
#include <vector>

#include "vst3sdk/pluginterfaces/base/ibstream.h"
#include "vst3sdk/pluginterfaces/vst/ivstaudioprocessor.h"
#include "vst3sdk/pluginterfaces/vst/vsttypes.h"
#include "vst3sdk/public.sdk/source/vst/vstaudioeffect.h"

// Beatrice
#include "common/audio_engine.h"
#include "common/audio_recorder.h"
#include "vst/direct_wasapi_output.h"

namespace beatrice::vst {

class Processor : public Steinberg::Vst::AudioEffect {
  using tresult = Steinberg::tresult;
  using int32 = Steinberg::int32;
  using uint32 = Steinberg::uint32;
  using TBool = Steinberg::TBool;
  using IBStream = Steinberg::IBStream;
  using SpeakerArrangement = Steinberg::Vst::SpeakerArrangement;
  using ProcessSetup = Steinberg::Vst::ProcessSetup;
  using ProcessData = Steinberg::Vst::ProcessData;
  using IMessage = Steinberg::Vst::IMessage;
  using IAudioProcessor = Steinberg::Vst::IAudioProcessor;
  using ParamID = Steinberg::Vst::ParamID;
  using ParamValue = Steinberg::Vst::ParamValue;

 public:
  Processor();
  ~Processor() override;

  auto PLUGIN_API initialize(FUnknown* context) -> tresult SMTG_OVERRIDE;
  auto PLUGIN_API setBusArrangements(SpeakerArrangement* inputs, int32 numIns,
                                     SpeakerArrangement* outputs, int32 numOuts)
      -> tresult SMTG_OVERRIDE;

  auto PLUGIN_API setupProcessing(ProcessSetup& setup) -> tresult SMTG_OVERRIDE;
  auto PLUGIN_API setActive(TBool state) -> tresult SMTG_OVERRIDE;
  auto PLUGIN_API getLatencySamples() -> uint32 SMTG_OVERRIDE;
  auto PLUGIN_API process(ProcessData& data) -> tresult SMTG_OVERRIDE;

  auto PLUGIN_API setState(IBStream* state) -> tresult SMTG_OVERRIDE;
  auto PLUGIN_API getState(IBStream* state) -> tresult SMTG_OVERRIDE;

  auto PLUGIN_API notify(IMessage* message) -> tresult SMTG_OVERRIDE;

  // NOLINTNEXTLINE(readability-identifier-naming)
  static auto createInstance(void* /*context*/) -> FUnknown* {
    return static_cast<IAudioProcessor*>(new Processor());
  }

 private:
  void SendAudioLevelMessage();
  void SendRecordingStatusMessage();
  void SendDirectWasapiStatusMessage(DirectWasapiStatus status,
                                     const std::string& error);

  std::mutex mtx_;
  common::AudioEngine audio_engine_;
  common::AudioRecorder recorder_;
  DirectWasapiOutput direct_wasapi_output_;
  std::optional<DirectWasapiConfig> direct_wasapi_config_;
  common::RecordingMode recording_mode_ = common::RecordingMode::kOff;
  std::filesystem::path recording_path_;
  // メモリ確保が挟まるのが望ましくないが……
  std::map<ParamID, ParamValue> unreflected_params_;
  double meter_sample_rate_ = 0.0;
  std::int64_t meter_frames_ = 0;
  float meter_input_peak_ = 0.0F;
  float meter_output_peak_ = 0.0F;
  std::vector<float> input_meter_buffer_;
};

}  // namespace beatrice::vst

#endif  // BEATRICE_VST_PROCESSOR_H_
