// Copyright (c) 2026 Project Beatrice and Contributors

#ifndef BEATRICE_VST_EDITOR_UI_LIMITS_H_
#define BEATRICE_VST_EDITOR_UI_LIMITS_H_

#include <algorithm>
#include <cmath>
#include <optional>

#include "common/parameter_schema.h"

namespace beatrice::vst::ui_limits {

struct Range {
  float minimum;
  float maximum;
};

struct SliderSpec {
  int precision;
  float wheel_increment;
  float fine_wheel_increment;
  float keyboard_increment;
  float keyboard_fine_increment;
};

// These are deliberately UI-only ranges. The common parameter schema keeps
// the original engine/host ranges for upstream compatibility and future use.
// Values restored by a host outside these ranges remain in controller and
// processor state; only their editor representation is clamped.
[[nodiscard]] inline auto GetSliderRange(const common::ParameterID id)
    -> std::optional<Range> {
  switch (id) {
    case common::ParameterID::kInputGain:
    case common::ParameterID::kOutputGain:
      return Range{-20.0f, 20.0f};
    case common::ParameterID::kPitchShift:
      return Range{-18.0f, 18.0f};
    case common::ParameterID::kCompensatedDrive:
      return Range{0.0f, 20.0f};
    case common::ParameterID::kLowCutHz:
      return Range{0.0f, 500.0f};
    case common::ParameterID::kDeClick:
    case common::ParameterID::kDeMud:
    case common::ParameterID::kPresence:
    case common::ParameterID::kReverbMix:
    case common::ParameterID::kReverbTone:
      return Range{0.0f, 100.0f};
    case common::ParameterID::kReverbDecay:
      return Range{0.2f, 5.0f};
    case common::ParameterID::kAverageSourcePitch:
    case common::ParameterID::kMinSourcePitch:
    case common::ParameterID::kMaxSourcePitch:
      return Range{0.0f, 128.0f};
    default:
      return std::nullopt;
  }
}

[[nodiscard]] inline auto GetSliderSpec(const common::ParameterID id)
    -> SliderSpec {
  switch (id) {
    case common::ParameterID::kInputGain:
    case common::ParameterID::kOutputGain:
      return {1, 0.5f, 0.1f, 1.0f, 0.1f};
    case common::ParameterID::kCompensatedDrive:
      return {1, 0.1f, 0.1f, 1.0f, 0.1f};
    case common::ParameterID::kLowCutHz:
      return {0, 1.0f, 1.0f, 1.0f, 1.0f};
    case common::ParameterID::kDeClick:
    case common::ParameterID::kDeMud:
    case common::ParameterID::kPresence:
    case common::ParameterID::kReverbMix:
    case common::ParameterID::kReverbTone:
      return {0, 1.0f, 0.1f, 1.0f, 0.1f};
    case common::ParameterID::kReverbDecay:
      return {1, 0.1f, 0.1f, 0.5f, 0.1f};
    case common::ParameterID::kPitchShift:
      return {1, 0.5f, 0.1f, 1.0f, 0.1f};
    case common::ParameterID::kFormantShift:
      return {1, 0.5f, 0.5f, 0.5f, 0.5f};
    case common::ParameterID::kVQNumNeighbors:
      return {0, 1.0f, 1.0f, 1.0f, 1.0f};
    case common::ParameterID::kAverageSourcePitch:
    case common::ParameterID::kMinSourcePitch:
    case common::ParameterID::kMaxSourcePitch:
      return {1, 0.1f, 0.1f, 1.0f, 0.1f};
    case common::ParameterID::kIntonationIntensity:
    case common::ParameterID::kPitchCorrection:
      return {1, 0.1f, 0.1f, 0.5f, 0.1f};
    case common::ParameterID::kVoiceMorphFalloff:
      return {1, common::kVoiceMorphFalloffStep,
              common::kVoiceMorphFalloffStep, 1.0f,
              common::kVoiceMorphFalloffStep};
    default:
      return {1, 1.0f, 0.1f, 1.0f, 0.1f};
  }
}

// Quantization belongs to the editor layer only.  The common parameter
// schema keeps the original engine resolution, while the editor presents the
// intentionally coarser controls requested by the UI.  This also makes the
// rule explicit instead of duplicating parameter IDs and scales in editor.cc.
[[nodiscard]] inline auto QuantizeSliderValue(const common::ParameterID id,
                                               const double value,
                                               const bool fine) -> double {
  auto step = 0.0;
  switch (id) {
    case common::ParameterID::kInputGain:
    case common::ParameterID::kOutputGain:
    case common::ParameterID::kPitchShift:
    case common::ParameterID::kCompensatedDrive:
      step = fine ? 0.1 : 0.5;
      break;
    case common::ParameterID::kFormantShift:
      // Formant Shift is always a half-step UI control, including fine mode.
      step = 0.5;
      break;
    default:
      return value;
  }
  return std::round(value / step) * step;
}

[[nodiscard]] inline auto ClampForDisplay(const common::ParameterID id,
                                          const float value) -> float {
  if (const auto range = GetSliderRange(id)) {
    return std::clamp(value, range->minimum, range->maximum);
  }
  return value;
}

}  // namespace beatrice::vst::ui_limits

#endif  // BEATRICE_VST_EDITOR_UI_LIMITS_H_
