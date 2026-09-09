// Copyright (c) 2024-2026 Project Beatrice and Contributors

#ifndef BEATRICE_VST_PROCESSOR_H_
#define BEATRICE_VST_PROCESSOR_H_

#include <atomic>
#include <cstdint>
#include <filesystem>
#include <memory>
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
#include "common/application_input.h"
#include "common/audio_recorder.h"
#include "common/input_source.h"
#include "common/stereo_delay_line.h"
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
  void SendApplicationInputStatusMessage(common::ApplicationInputStatus status,
                                          const std::string& error);
  auto StartApplicationInput(common::ApplicationInput& capture,
                             std::uint32_t& process_id,
                             const std::string& identity) -> bool;

  std::mutex mtx_;
  common::AudioEngine audio_engine_;
  common::AudioRecorder recorder_;
  DirectWasapiOutput direct_wasapi_output_;
  common::ApplicationInput application_input_;
  common::ApplicationInput additional_application_input_;
  bool application_input_enabled_ = false;
  bool additional_input_enabled_ = false;
  common::InputSource input_source_ = common::InputSource::kDawInput;
  common::InputSource additional_input_source_ = common::InputSource::kOff;
  std::string application_input_identity_;
  std::uint32_t application_input_process_id_ = 0;
  std::string additional_application_input_identity_;
  std::uint32_t additional_application_input_process_id_ = 0;
  std::filesystem::path input_file_path_;
  std::filesystem::path additional_input_file_path_;
  bool input_file_playing_ = false;
  bool input_file_loop_ = false;
  bool additional_file_playing_ = false;
  bool additional_file_loop_ = false;
  // The editor can change file volume while the realtime callback is active.
  // Atomics keep that update lock-free and prevent a slider drag from making
  // the callback observe a torn value.
  std::atomic<double> input_file_volume_{1.0};
  std::atomic<double> additional_file_volume_{1.0};
  std::optional<DirectWasapiConfig> direct_wasapi_config_;
  common::RecordingMode recording_mode_ = common::RecordingMode::kOutput;
  std::filesystem::path recording_path_;
  // Signed relative offset for the final BGM mix. Positive values delay BGM;
  // negative values delay the converted voice.
  std::int32_t recording_voice_delay_ms_ = 0;
  double recording_additional_input_gain_db_ =
      common::kDefaultAdditionalInputGainDb;
  // メモリ確保が挟まるのが望ましくないが……
  std::map<ParamID, ParamValue> unreflected_params_;
  double meter_sample_rate_ = 0.0;
  std::int64_t meter_frames_ = 0;
  float meter_input_peak_ = 0.0F;
  float meter_output_peak_ = 0.0F;
  float meter_external_output_peak_ = 0.0F;
  float meter_additional_input_peak_ = 0.0F;
  std::vector<float> input_meter_buffer_;
  // ADD BGM is routed to the external output/recorder, not the host monitor
  // output. Keep a separate block buffer for that post-conversion mix.
  std::vector<float> mixed_output_left_;
  std::vector<float> mixed_output_right_;
  common::StereoDelayLine voice_delay_line_;
  common::StereoDelayLine bgm_delay_line_;
  bool voice_delay_active_ = false;
  std::int32_t active_bgm_delay_ms_ = 0;
  std::vector<float> application_input_buffer_;
  std::vector<float> application_input_right_buffer_;
  std::vector<float> additional_application_input_buffer_;
  std::vector<float> additional_application_input_right_buffer_;
  std::vector<float> delayed_additional_input_buffer_;
  std::vector<float> delayed_additional_input_right_buffer_;
};

}  // namespace beatrice::vst

#endif  // BEATRICE_VST_PROCESSOR_H_
