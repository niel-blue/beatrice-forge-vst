// Copyright (c) 2026 Project Beatrice and Contributors

#ifndef BEATRICE_COMMON_OUTPUT_EFFECTS_H_
#define BEATRICE_COMMON_OUTPUT_EFFECTS_H_

#include <array>
#include <cstdint>
#include <vector>

namespace beatrice::common {

// Shared post-conversion processing for the VST and future standalone client.
// It is called after model conversion and before Output Gain. Every model-core
// generation therefore uses the same effect implementation and saved values.
class OutputEffects {
 public:
  explicit OutputEffects(double sample_rate = 0.0);

  void SetSampleRate(double sample_rate);
  void SetDeMud(double amount) noexcept;
  void SetPresence(double amount) noexcept;
  void SetReverbMix(double mix) noexcept;
  void SetReverbDecay(double seconds) noexcept;
  void SetReverbTone(double tone) noexcept;

  void Reset() noexcept;
  void DiscardReverbTail() noexcept;
  [[nodiscard]] auto HasReverbTail() const noexcept -> bool;

  // With a stereo destination, left and right receive decorrelated reverb.
  // Passing nullptr for right keeps the same processing usable by a mono
  // client without maintaining a separate DSP implementation.
  void Process(float* left, float* right, int n_samples) noexcept;

 private:
  static constexpr auto kNCombs = 4;
  static constexpr auto kNAllPasses = 2;

  struct CombFilter {
    std::vector<float> buffer;
    std::size_t index = 0;
    double delay_seconds = 0.0;
    double filter_store = 0.0;
    double feedback = 0.0;
    double feedback_target = 0.0;

    void Configure(double delay, double sample_rate);
    void Reset() noexcept;
    [[nodiscard]] auto Process(float input, double damping,
                               double smoothing_step) noexcept -> float;
  };

  struct AllPassFilter {
    std::vector<float> buffer;
    std::size_t index = 0;

    void Configure(double delay, double sample_rate);
    void Reset() noexcept;
    [[nodiscard]] auto Process(float input) noexcept -> float;
  };

  [[nodiscard]] auto LowPassCoefficient(double frequency) const -> double;
  void UpdateConfiguration();
  void UpdateReverbFeedbackTargets() noexcept;
  void ProcessClarity(float* samples, int n_samples) noexcept;
  void ProcessReverb(float* left, float* right, int n_samples) noexcept;

  double sample_rate_ = 0.0;
  bool coefficients_valid_ = false;

  double de_mud_target_ = 0.0;
  double de_mud_mix_ = 0.0;
  double presence_target_ = 0.0;
  double presence_mix_ = 0.0;
  double control_smoothing_step_ = 1.0;
  double low_180_coefficient_ = 0.0;
  double low_700_coefficient_ = 0.0;
  double low_2500_coefficient_ = 0.0;
  double low_180_ = 0.0;
  double low_700_ = 0.0;
  double low_2500_ = 0.0;

  double reverb_mix_target_ = 0.0;
  double reverb_mix_ = 0.0;
  double reverb_decay_seconds_ = 1.2;
  double reverb_tone_target_ = 0.5;
  double reverb_tone_ = 0.5;
  std::int64_t tail_samples_remaining_ = 0;
  std::int64_t tail_duration_samples_ = 0;
  bool reverb_cleared_ = true;
  std::array<CombFilter, kNCombs> left_combs_;
  std::array<CombFilter, kNCombs> right_combs_;
  std::array<AllPassFilter, kNAllPasses> left_all_passes_;
  std::array<AllPassFilter, kNAllPasses> right_all_passes_;
};

}  // namespace beatrice::common

#endif  // BEATRICE_COMMON_OUTPUT_EFFECTS_H_
