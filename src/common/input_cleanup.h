// Copyright (c) 2026 Project Beatrice and Contributors

#ifndef BEATRICE_COMMON_INPUT_CLEANUP_H_
#define BEATRICE_COMMON_INPUT_CLEANUP_H_

#include <algorithm>
#include <cmath>
#include <numbers>

namespace beatrice::common {

// Lightweight, allocation-free input conditioning used before voice
// conversion. It intentionally avoids look-ahead and does not add latency.
class InputCleanup {
 public:
  explicit InputCleanup(const double sample_rate = 48000.0) {
    SetSampleRate(sample_rate);
  }

  void SetSampleRate(const double sample_rate) {
    sample_rate_ = std::max(sample_rate, 1.0);
    UpdateHighPassCoefficient();
    // A 12 ms detector follows syllables without reacting to individual
    // waveform cycles. This is deliberately independent of the low-cut
    // filter, which remains a zero-latency frequency filter.
    envelope_coefficient_ = static_cast<float>(
        std::exp(-1.0 / (0.012 * sample_rate_)));
    gain_attack_coefficient_ = static_cast<float>(
        std::exp(-1.0 / (0.008 * sample_rate_)));
    gain_release_coefficient_ = static_cast<float>(
        std::exp(-1.0 / (0.120 * sample_rate_)));
    declick_envelope_coefficient_ = static_cast<float>(
        std::exp(-1.0 / (0.002 * sample_rate_)));
    declick_recovery_coefficient_ = static_cast<float>(
        std::exp(-1.0 / (0.006 * sample_rate_)));
  }

  void SetLowCutHz(const double frequency) {
    low_cut_hz_ = std::clamp(frequency, 0.0, 500.0);
    UpdateHighPassCoefficient();
  }

  void SetDenoiseMode(const int mode) {
    denoise_mode_ = std::clamp(mode, 0, 2);
  }

  void SetDeClickStrength(const double strength) {
    declick_strength_ = static_cast<float>(
        std::clamp(strength, 0.0, 100.0) * 0.01);
  }

  void Reset() {
    previous_input_ = previous_output_ = 0.0f;
    envelope_power_ = 0.0f;
    denoise_gain_ = 1.0f;
    declick_previous_sample_ = 0.0f;
    declick_previous_delta_ = 0.0f;
    declick_envelope_ = 0.0f;
    declick_gain_ = 1.0f;
  }

  void Process(const float* const input, float* const output,
               const int n_samples) {
    // This is a voice-safe downward expander, not a sample-by-sample gate.
    // A previous implementation multiplied every individual sample according
    // to its magnitude, which also cut each zero crossing of normal speech.
    // The detector below follows short-term power and the gain is smoothed.
    const auto threshold = denoise_mode_ == 1 ? 0.0040f
                           : denoise_mode_ == 2 ? 0.0080f
                                                : 0.0f;
    const auto minimum_gain = denoise_mode_ == 1 ? 0.65f
                              : denoise_mode_ == 2 ? 0.32f
                                                   : 1.0f;
    for (auto i = 0; i < n_samples; ++i) {
      const auto x = input[i];
      auto y = x;
      if (low_cut_hz_ > 0.0) {
        y = high_pass_coefficient_ * (previous_output_ + x - previous_input_);
        previous_input_ = x;
        previous_output_ = y;
      }
      if (declick_strength_ > 0.0f) {
        const auto delta = y - declick_previous_sample_;
        const auto curvature = std::abs(delta - declick_previous_delta_);
        declick_envelope_ =
            declick_envelope_coefficient_ * declick_envelope_ +
            (1.0f - declick_envelope_coefficient_) * std::abs(y);
        const auto transient = std::abs(delta) + 0.5f * curvature;
        const auto relative_transient =
            transient / (0.01f + declick_envelope_);
        const auto threshold = 0.50f - 0.45f * declick_strength_;
        const auto hit = std::clamp(
            (relative_transient - threshold) /
                std::max(threshold, 0.01f),
            0.0f, 1.0f);
        const auto target_gain =
            1.0f - 0.95f * declick_strength_ * hit;
        if (target_gain < declick_gain_) {
          declick_gain_ = target_gain;
        } else {
          declick_gain_ =
              declick_recovery_coefficient_ * declick_gain_ +
              (1.0f - declick_recovery_coefficient_);
        }
        y *= declick_gain_;
        declick_previous_delta_ = delta;
        declick_previous_sample_ = y;
      } else {
        declick_previous_sample_ = y;
        declick_previous_delta_ = 0.0f;
        declick_envelope_ = 0.0f;
        declick_gain_ = 1.0f;
      }
      if (threshold > 0.0f) {
        envelope_power_ = envelope_coefficient_ * envelope_power_ +
                          (1.0f - envelope_coefficient_) * y * y;
        const auto envelope = std::sqrt(std::max(envelope_power_, 0.0f));
        const auto openness = std::clamp(envelope / threshold, 0.0f, 1.0f);
        const auto target_gain =
            minimum_gain + (1.0f - minimum_gain) * openness;
        const auto coefficient = target_gain > denoise_gain_
                                     ? gain_attack_coefficient_
                                     : gain_release_coefficient_;
        denoise_gain_ = coefficient * denoise_gain_ +
                         (1.0f - coefficient) * target_gain;
        y *= denoise_gain_;
      } else {
        envelope_power_ = 0.0f;
        denoise_gain_ = 1.0f;
      }
      output[i] = y;
    }
  }

 private:
  void UpdateHighPassCoefficient() {
    high_pass_coefficient_ = low_cut_hz_ <= 0.0
                                 ? 0.0f
                                 : static_cast<float>(std::exp(
                                       -2.0 * std::numbers::pi * low_cut_hz_ /
                                       sample_rate_));
  }

  double sample_rate_ = 48000.0;
  double low_cut_hz_ = 0.0;
  int denoise_mode_ = 0;
  float high_pass_coefficient_ = 0.0f;
  float previous_input_ = 0.0f;
  float previous_output_ = 0.0f;
  float envelope_power_ = 0.0f;
  float denoise_gain_ = 1.0f;
  float envelope_coefficient_ = 0.0f;
  float gain_attack_coefficient_ = 0.0f;
  float gain_release_coefficient_ = 0.0f;
  float declick_strength_ = 0.0f;
  float declick_previous_sample_ = 0.0f;
  float declick_previous_delta_ = 0.0f;
  float declick_envelope_ = 0.0f;
  float declick_gain_ = 1.0f;
  float declick_envelope_coefficient_ = 0.0f;
  float declick_recovery_coefficient_ = 0.0f;
};

}  // namespace beatrice::common

#endif  // BEATRICE_COMMON_INPUT_CLEANUP_H_
