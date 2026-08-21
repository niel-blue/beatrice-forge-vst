// Copyright (c) 2026 Project Beatrice and Contributors

#include "common/output_effects.h"

#include <algorithm>
#include <cmath>
#include <cstddef>

namespace beatrice::common {
namespace {

// The controls are intended to remain gentle in their lower range while the
// upper range must be clearly audible on an already-converted voice. At 100,
// De-Mud cuts the centre of the 180-700 Hz band by roughly 12 dB and Presence
// raises the upper voice band by roughly 5.5 dB at 48 kHz.
constexpr auto kMudReduction = 1.30;
constexpr auto kPresenceBoost = 1.15;
constexpr auto kReverbWetGain = 0.70;
constexpr auto kAllPassFeedback = 0.5F;
constexpr auto kSignalThreshold = 1.0e-7;

constexpr std::array kLeftCombDelays = {0.0297, 0.0371, 0.0411, 0.0437};
constexpr std::array kRightCombDelays = {0.0307, 0.0361, 0.0403, 0.0449};
constexpr std::array kLeftAllPassDelays = {0.0050, 0.0017};
constexpr std::array kRightAllPassDelays = {0.0055, 0.0021};

}  // namespace

void OutputEffects::CombFilter::Configure(const double delay,
                                           const double sample_rate) {
  delay_seconds = delay;
  const auto size = std::max<std::size_t>(
      1, static_cast<std::size_t>(std::round(delay * sample_rate)));
  buffer.assign(size, 0.0F);
  index = 0;
  filter_store = 0.0;
  feedback = feedback_target;
}

void OutputEffects::CombFilter::Reset() noexcept {
  std::fill(buffer.begin(), buffer.end(), 0.0F);
  index = 0;
  filter_store = 0.0;
  feedback = feedback_target;
}

auto OutputEffects::CombFilter::Process(const float input,
                                        const double damping,
                                        const double smoothing_step) noexcept
    -> float {
  if (buffer.empty()) {
    return 0.0F;
  }
  feedback += std::clamp(feedback_target - feedback, -smoothing_step,
                         smoothing_step);
  const auto output = static_cast<double>(buffer[index]);
  filter_store = output * (1.0 - damping) + filter_store * damping;
  buffer[index] = static_cast<float>(input + filter_store * feedback);
  ++index;
  if (index == buffer.size()) {
    index = 0;
  }
  return static_cast<float>(output);
}

void OutputEffects::AllPassFilter::Configure(const double delay,
                                              const double sample_rate) {
  const auto size = std::max<std::size_t>(
      1, static_cast<std::size_t>(std::round(delay * sample_rate)));
  buffer.assign(size, 0.0F);
  index = 0;
}

void OutputEffects::AllPassFilter::Reset() noexcept {
  std::fill(buffer.begin(), buffer.end(), 0.0F);
  index = 0;
}

auto OutputEffects::AllPassFilter::Process(const float input) noexcept
    -> float {
  if (buffer.empty()) {
    return input;
  }
  const auto delayed = buffer[index];
  const auto output = -input + delayed;
  buffer[index] = input + delayed * kAllPassFeedback;
  ++index;
  if (index == buffer.size()) {
    index = 0;
  }
  return output;
}

OutputEffects::OutputEffects(const double sample_rate)
    : sample_rate_(sample_rate) {
  UpdateConfiguration();
}

void OutputEffects::SetSampleRate(const double sample_rate) {
  sample_rate_ = sample_rate;
  UpdateConfiguration();
}

void OutputEffects::SetDeMud(const double amount) noexcept {
  de_mud_target_ = std::clamp(amount / 100.0, 0.0, 1.0);
}

void OutputEffects::SetPresence(const double amount) noexcept {
  presence_target_ = std::clamp(amount / 100.0, 0.0, 1.0);
}

void OutputEffects::SetReverbMix(const double mix) noexcept {
  reverb_mix_target_ = std::clamp(mix / 100.0, 0.0, 1.0);
}

void OutputEffects::SetReverbDecay(const double seconds) noexcept {
  reverb_decay_seconds_ = std::clamp(seconds, 0.2, 5.0);
  UpdateReverbFeedbackTargets();
}

void OutputEffects::SetReverbTone(const double tone) noexcept {
  reverb_tone_target_ = std::clamp(tone / 100.0, 0.0, 1.0);
}

void OutputEffects::Reset() noexcept {
  low_180_ = 0.0;
  low_700_ = 0.0;
  low_2500_ = 0.0;
  de_mud_mix_ = de_mud_target_;
  presence_mix_ = presence_target_;
  reverb_mix_ = reverb_mix_target_;
  reverb_tone_ = reverb_tone_target_;
  DiscardReverbTail();
}

void OutputEffects::DiscardReverbTail() noexcept {
  for (auto& comb : left_combs_) comb.Reset();
  for (auto& comb : right_combs_) comb.Reset();
  for (auto& all_pass : left_all_passes_) all_pass.Reset();
  for (auto& all_pass : right_all_passes_) all_pass.Reset();
  tail_samples_remaining_ = 0;
  reverb_cleared_ = true;
}

auto OutputEffects::HasReverbTail() const noexcept -> bool {
  return tail_samples_remaining_ > 0 &&
         (reverb_mix_target_ > 0.0 || reverb_mix_ > 0.0);
}

auto OutputEffects::LowPassCoefficient(const double frequency) const
    -> double {
  constexpr auto kTwoPi = 6.28318530717958647692;
  return 1.0 - std::exp(-kTwoPi * frequency / sample_rate_);
}

void OutputEffects::UpdateConfiguration() {
  coefficients_valid_ = std::isfinite(sample_rate_) && sample_rate_ >= 8000.0;
  if (!coefficients_valid_) {
    low_180_coefficient_ = 0.0;
    low_700_coefficient_ = 0.0;
    low_2500_coefficient_ = 0.0;
    control_smoothing_step_ = 1.0;
    left_combs_ = {};
    right_combs_ = {};
    left_all_passes_ = {};
    right_all_passes_ = {};
    Reset();
    return;
  }

  low_180_coefficient_ = LowPassCoefficient(180.0);
  low_700_coefficient_ = LowPassCoefficient(700.0);
  low_2500_coefficient_ = LowPassCoefficient(2500.0);
  control_smoothing_step_ = 1.0 / (sample_rate_ * 0.020);
  for (auto i = std::size_t{0}; i < kNCombs; ++i) {
    left_combs_[i].Configure(kLeftCombDelays[i], sample_rate_);
    right_combs_[i].Configure(kRightCombDelays[i], sample_rate_);
  }
  for (auto i = std::size_t{0}; i < kNAllPasses; ++i) {
    left_all_passes_[i].Configure(kLeftAllPassDelays[i], sample_rate_);
    right_all_passes_[i].Configure(kRightAllPassDelays[i], sample_rate_);
  }
  UpdateReverbFeedbackTargets();
  Reset();
}

void OutputEffects::UpdateReverbFeedbackTargets() noexcept {
  if (!coefficients_valid_) {
    return;
  }
  const auto set_feedback = [this](auto& combs) {
    for (auto& comb : combs) {
      // Feedback is derived from RT60: after the selected Decay time the
      // repeated signal is 60 dB below its starting level.
      comb.feedback_target =
          std::pow(0.001, comb.delay_seconds / reverb_decay_seconds_);
    }
  };
  set_feedback(left_combs_);
  set_feedback(right_combs_);
  tail_duration_samples_ = static_cast<std::int64_t>(
      std::ceil((reverb_decay_seconds_ + 0.25) * sample_rate_));
}

void OutputEffects::Process(float* const left, float* const right,
                            const int n_samples) noexcept {
  if (left == nullptr || n_samples <= 0) {
    return;
  }
  if (!coefficients_valid_) {
    if (right != nullptr) {
      std::copy_n(left, n_samples, right);
    }
    return;
  }
  ProcessClarity(left, n_samples);
  ProcessReverb(left, right, n_samples);
}

void OutputEffects::ProcessClarity(float* const samples,
                                   const int n_samples) noexcept {
  if (de_mud_target_ == 0.0 && de_mud_mix_ == 0.0 &&
      presence_target_ == 0.0 && presence_mix_ == 0.0) {
    low_180_ = 0.0;
    low_700_ = 0.0;
    low_2500_ = 0.0;
    return;
  }

  for (auto i = 0; i < n_samples; ++i) {
    de_mud_mix_ += std::clamp(de_mud_target_ - de_mud_mix_,
                              -control_smoothing_step_,
                              control_smoothing_step_);
    presence_mix_ += std::clamp(presence_target_ - presence_mix_,
                                -control_smoothing_step_,
                                control_smoothing_step_);
    const auto dry = static_cast<double>(samples[i]);
    low_180_ += low_180_coefficient_ * (dry - low_180_);
    low_700_ += low_700_coefficient_ * (dry - low_700_);
    low_2500_ += low_2500_coefficient_ * (dry - low_2500_);
    const auto muddy_band = low_700_ - low_180_;
    const auto presence = dry - low_2500_;
    samples[i] = static_cast<float>(
        dry - de_mud_mix_ * kMudReduction * muddy_band +
        presence_mix_ * kPresenceBoost * presence);
  }
}

void OutputEffects::ProcessReverb(float* const left, float* const right,
                                  const int n_samples) noexcept {
  if (reverb_mix_target_ == 0.0 && reverb_mix_ == 0.0) {
    if (!reverb_cleared_) {
      DiscardReverbTail();
    }
    if (right != nullptr) {
      std::copy_n(left, n_samples, right);
    }
    return;
  }

  reverb_cleared_ = false;
  for (auto i = 0; i < n_samples; ++i) {
    reverb_mix_ += std::clamp(reverb_mix_target_ - reverb_mix_,
                              -control_smoothing_step_,
                              control_smoothing_step_);
    reverb_tone_ += std::clamp(reverb_tone_target_ - reverb_tone_,
                               -control_smoothing_step_,
                               control_smoothing_step_);
    const auto damping = 0.75 - 0.70 * reverb_tone_;
    const auto dry = left[i];
    if (std::abs(static_cast<double>(dry)) > kSignalThreshold) {
      tail_samples_remaining_ = tail_duration_samples_;
    } else if (tail_samples_remaining_ > 0) {
      --tail_samples_remaining_;
    }

    auto wet_left = 0.0F;
    auto wet_right = 0.0F;
    for (auto& comb : left_combs_) {
      wet_left += comb.Process(dry, damping, control_smoothing_step_);
    }
    for (auto& comb : right_combs_) {
      wet_right += comb.Process(dry, damping, control_smoothing_step_);
    }
    wet_left /= static_cast<float>(kNCombs);
    wet_right /= static_cast<float>(kNCombs);
    for (auto& all_pass : left_all_passes_) {
      wet_left = all_pass.Process(wet_left);
    }
    for (auto& all_pass : right_all_passes_) {
      wet_right = all_pass.Process(wet_right);
    }
    wet_left = static_cast<float>(wet_left * kReverbWetGain);
    wet_right = static_cast<float>(wet_right * kReverbWetGain);

    const auto dry_mix = 1.0 - reverb_mix_;
    if (right != nullptr) {
      left[i] = static_cast<float>(dry * dry_mix + wet_left * reverb_mix_);
      right[i] = static_cast<float>(dry * dry_mix + wet_right * reverb_mix_);
    } else {
      const auto wet_mono = 0.5F * (wet_left + wet_right);
      left[i] = static_cast<float>(dry * dry_mix + wet_mono * reverb_mix_);
    }
  }

  if (reverb_mix_target_ == 0.0 && reverb_mix_ == 0.0) {
    DiscardReverbTail();
  }
}

}  // namespace beatrice::common
