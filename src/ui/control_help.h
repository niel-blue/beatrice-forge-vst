// Copyright (c) 2026 Project Beatrice and Contributors

#ifndef BEATRICE_UI_CONTROL_HELP_H_
#define BEATRICE_UI_CONTROL_HELP_H_

#include <array>
#include <cstdint>
#include <string_view>

#include "common/parameter_schema.h"

namespace beatrice::ui {

// ControlHelpID is deliberately separate from ParameterID.  Help is a UI
// concern: it is not saved in a preset, exposed to a VST host, or used for
// automation.  The numeric ranges leave room for future entries without
// changing the identity of existing help items.
enum class ControlHelpID : std::uint16_t {
  kNone = 0x0000,

  // General controls.
  kModel = 0x0100,
  kVoice = 0x0101,
  kConversionBypass = 0x0102,

  // GAIN.
  kInputGain = 0x0200,
  kOutputGain = 0x0201,
  kNoiseReductionBoost = 0x0202,

  // INPUT CLEANUP.
  kLowCut = 0x0300,
  kLightDenoise = 0x0301,
  kDeClick = 0x0302,

  // VOICE SHAPE.
  kPitchShift = 0x0400,
  kFormantShift = 0x0401,
  kVQNeighborCount = 0x0402,
  kControl = 0x0403,
  kAverageSourcePitch = 0x0404,
  kMinSourcePitch = 0x0405,
  kMaxSourcePitch = 0x0406,

  // TUNING.
  kIntonationIntensity = 0x0500,
  kPitchCorrection = 0x0501,
  kPitchCorrectionType = 0x0502,
  kLatencyReporting = 0x0503,

  // Voice morphing views.
  kMorphPad = 0x0600,
  kMorphSlider = 0x0601,
  kMorphFalloff = 0x0602,

  // PRESETS actions.
  kPresetBankAdd = 0x0700,
  kPresetBankDelete = 0x0701,
  kSavePreset = 0x0702,
  kNewEmptyPreset = 0x0703,
  kImportPresetList = 0x0704,
  kExportPresetList = 0x0705,

  // EFFECTS.
  kDeMud = 0x0800,
  kPresence = 0x0801,
  kReverbMix = 0x0810,
  kReverbDecay = 0x0811,
  kReverbTone = 0x0812,

  // Reserved end marker.  Do not use this as an item ID.
  kEnd = 0x0900,
};

enum class ControlHelpSection : std::uint8_t {
  kGeneral,
  kGain,
  kInputCleanup,
  kVoiceShape,
  kTuning,
  kMorph,
  kPresets,
  kEffects,
};

struct ControlHelpDefinition {
  ControlHelpID id;
  ControlHelpSection section;
  std::string_view key;
};

// The key is stable source-facing identity.  Localized text is intentionally
// kept out of this table so that wording can be revised without changing an
// ID or touching VST parameter metadata.
inline constexpr auto kControlHelpDefinitions =
    std::to_array<ControlHelpDefinition>({
        {ControlHelpID::kModel, ControlHelpSection::kGeneral, "model"},
        {ControlHelpID::kVoice, ControlHelpSection::kGeneral, "voice"},
        {ControlHelpID::kConversionBypass, ControlHelpSection::kGeneral,
         "conversion_bypass"},
        {ControlHelpID::kInputGain, ControlHelpSection::kGain, "input_gain"},
        {ControlHelpID::kOutputGain, ControlHelpSection::kGain,
         "output_gain"},
        {ControlHelpID::kNoiseReductionBoost, ControlHelpSection::kGain,
         "noise_reduction_boost"},
        {ControlHelpID::kLowCut, ControlHelpSection::kInputCleanup,
         "low_cut"},
        {ControlHelpID::kLightDenoise, ControlHelpSection::kInputCleanup,
         "light_denoise"},
        {ControlHelpID::kDeClick, ControlHelpSection::kInputCleanup,
         "de_click"},
        {ControlHelpID::kPitchShift, ControlHelpSection::kVoiceShape,
         "pitch_shift"},
        {ControlHelpID::kFormantShift, ControlHelpSection::kVoiceShape,
         "formant_shift"},
        {ControlHelpID::kVQNeighborCount, ControlHelpSection::kVoiceShape,
         "vq_neighbor_count"},
        {ControlHelpID::kControl, ControlHelpSection::kVoiceShape, "control"},
        {ControlHelpID::kAverageSourcePitch, ControlHelpSection::kVoiceShape,
         "average_source_pitch"},
        {ControlHelpID::kMinSourcePitch, ControlHelpSection::kVoiceShape,
         "min_source_pitch"},
        {ControlHelpID::kMaxSourcePitch, ControlHelpSection::kVoiceShape,
         "max_source_pitch"},
        {ControlHelpID::kIntonationIntensity, ControlHelpSection::kTuning,
         "intonation_intensity"},
        {ControlHelpID::kPitchCorrection, ControlHelpSection::kTuning,
         "pitch_correction"},
        {ControlHelpID::kPitchCorrectionType, ControlHelpSection::kTuning,
         "pitch_correction_type"},
        {ControlHelpID::kLatencyReporting, ControlHelpSection::kTuning,
         "latency_reporting"},
        {ControlHelpID::kMorphPad, ControlHelpSection::kMorph, "morph_pad"},
        {ControlHelpID::kMorphSlider, ControlHelpSection::kMorph,
         "morph_slider"},
        {ControlHelpID::kMorphFalloff, ControlHelpSection::kMorph,
         "morph_falloff"},
        {ControlHelpID::kPresetBankAdd, ControlHelpSection::kPresets,
         "preset_bank_add"},
        {ControlHelpID::kPresetBankDelete, ControlHelpSection::kPresets,
         "preset_bank_delete"},
        {ControlHelpID::kSavePreset, ControlHelpSection::kPresets,
         "save_preset"},
        {ControlHelpID::kNewEmptyPreset, ControlHelpSection::kPresets,
         "new_empty_preset"},
        {ControlHelpID::kImportPresetList, ControlHelpSection::kPresets,
         "import_preset_list"},
        {ControlHelpID::kExportPresetList, ControlHelpSection::kPresets,
         "export_preset_list"},
        {ControlHelpID::kDeMud, ControlHelpSection::kEffects, "de_mud"},
        {ControlHelpID::kPresence, ControlHelpSection::kEffects, "presence"},
        {ControlHelpID::kReverbMix, ControlHelpSection::kEffects,
         "reverb_mix"},
        {ControlHelpID::kReverbDecay, ControlHelpSection::kEffects,
         "reverb_decay"},
        {ControlHelpID::kReverbTone, ControlHelpSection::kEffects,
         "reverb_tone"},
    });

[[nodiscard]] constexpr auto IsControlHelpID(const ControlHelpID id) -> bool {
  const auto value = static_cast<std::uint16_t>(id);
  return value != static_cast<std::uint16_t>(ControlHelpID::kNone) &&
         value < static_cast<std::uint16_t>(ControlHelpID::kEnd);
}

[[nodiscard]] constexpr auto FindControlHelpDefinition(
    const ControlHelpID id) -> const ControlHelpDefinition* {
  for (const auto& definition : kControlHelpDefinitions) {
    if (definition.id == id) {
      return &definition;
    }
  }
  return nullptr;
}

// Maps parameter-backed controls to the separate help namespace.  Parameters
// that are internal serialization data or host-only implementation details do
// not receive a help item.  Morph cursor/marker parameters are represented by
// the visible morph pad rather than by individual hidden controls.
[[nodiscard]] constexpr auto ControlHelpForParameter(
    const common::ParameterID parameter_id) -> ControlHelpID {
  using common::ParameterID;

  // The morph pad is one visible control even though its state is represented
  // by several cursor/marker parameters.  Keep every hidden morph parameter
  // attached to that one help item, while keeping Morph Falloff separate.
  if (parameter_id == ParameterID::kVoiceMorphFalloff) {
    return ControlHelpID::kMorphFalloff;
  }
  const auto parameter_value = static_cast<int>(parameter_id);
  const auto morph_last = static_cast<int>(ParameterID::kVoiceMorphMarkerYBase) +
                          common::kMaxNVoiceMorphMarkers;
  if (parameter_value >= static_cast<int>(ParameterID::kVoiceMorphCursorX) &&
      parameter_value < morph_last) {
    return ControlHelpID::kMorphPad;
  }

  switch (parameter_id) {
    case ParameterID::kModel:
      return ControlHelpID::kModel;
    case ParameterID::kVoice:
      return ControlHelpID::kVoice;
    case ParameterID::kBypass:
      return ControlHelpID::kConversionBypass;
    case ParameterID::kInputGain:
      return ControlHelpID::kInputGain;
    case ParameterID::kOutputGain:
      return ControlHelpID::kOutputGain;
    case ParameterID::kCompensatedDrive:
      return ControlHelpID::kNoiseReductionBoost;
    case ParameterID::kLowCutHz:
      return ControlHelpID::kLowCut;
    case ParameterID::kLightDenoise:
      return ControlHelpID::kLightDenoise;
    case ParameterID::kDeClick:
      return ControlHelpID::kDeClick;
    case ParameterID::kDeMud:
      return ControlHelpID::kDeMud;
    case ParameterID::kPresence:
      return ControlHelpID::kPresence;
    case ParameterID::kReverbMix:
      return ControlHelpID::kReverbMix;
    case ParameterID::kReverbDecay:
      return ControlHelpID::kReverbDecay;
    case ParameterID::kReverbTone:
      return ControlHelpID::kReverbTone;
    case ParameterID::kPitchShift:
      return ControlHelpID::kPitchShift;
    case ParameterID::kFormantShift:
      return ControlHelpID::kFormantShift;
    case ParameterID::kVQNumNeighbors:
      return ControlHelpID::kVQNeighborCount;
    case ParameterID::kLock:
      return ControlHelpID::kControl;
    case ParameterID::kAverageSourcePitch:
      return ControlHelpID::kAverageSourcePitch;
    case ParameterID::kMinSourcePitch:
      return ControlHelpID::kMinSourcePitch;
    case ParameterID::kMaxSourcePitch:
      return ControlHelpID::kMaxSourcePitch;
    case ParameterID::kIntonationIntensity:
      return ControlHelpID::kIntonationIntensity;
    case ParameterID::kPitchCorrection:
      return ControlHelpID::kPitchCorrection;
    case ParameterID::kPitchCorrectionType:
      return ControlHelpID::kPitchCorrectionType;
    case ParameterID::kLatencyReporting:
      return ControlHelpID::kLatencyReporting;
    case ParameterID::kSimpleMorphWeights:
      return ControlHelpID::kMorphSlider;
    default:
      return ControlHelpID::kNone;
  }
}

}  // namespace beatrice::ui

#endif  // BEATRICE_UI_CONTROL_HELP_H_
