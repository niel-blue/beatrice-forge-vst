// Copyright (c) 2026 Project Beatrice and Contributors

#ifndef BEATRICE_COMMON_STEREO_DELAY_LINE_H_
#define BEATRICE_COMMON_STEREO_DELAY_LINE_H_

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace beatrice::common {

// BGM Delay is a signed relative offset.  Positive values delay the added
// BGM, while negative values delay the converted voice by the same amount.
inline constexpr std::int32_t kMinBgmDelayMs = -100;
inline constexpr std::int32_t kMaxBgmDelayMs = 100;

inline auto ClampBgmDelayMs(const std::int32_t delay_ms) noexcept
    -> std::int32_t {
  return std::clamp(delay_ms, kMinBgmDelayMs, kMaxBgmDelayMs);
}

// A preallocated, realtime-safe stereo delay used for the final output mix.
// Prepare() is called outside the audio callback. Reset() only clears the
// already allocated storage, and ProcessBlock() performs indexed reads/writes
// without allocating or locking.
class StereoDelayLine final {
 public:
  void Prepare(const double sample_rate, const std::uint32_t max_delay_ms) {
    const auto max_delay = static_cast<std::size_t>(std::max(
        0LL, static_cast<long long>(std::llround(
                 std::max(0.0, sample_rate) * max_delay_ms / 1000.0))));
    left_.assign(max_delay + 1U, 0.0F);
    right_.assign(max_delay + 1U, 0.0F);
    write_index_ = 0;
  }

  void Reset() noexcept {
    std::fill(left_.begin(), left_.end(), 0.0F);
    std::fill(right_.begin(), right_.end(), 0.0F);
    write_index_ = 0;
  }

  void ProcessBlock(const float* const input_left,
                   const float* const input_right, float* const output_left,
                   float* const output_right, const std::size_t count,
                   const std::size_t delay_samples) noexcept {
    if (input_left == nullptr || output_left == nullptr || count == 0 ||
        left_.empty()) {
      return;
    }
    const auto capacity = left_.size();
    const auto delay = std::min(delay_samples, capacity - 1U);
    for (auto frame = std::size_t{0}; frame < count; ++frame) {
      const auto write = write_index_;
      left_[write] = input_left[frame];
      right_[write] = input_right == nullptr ? input_left[frame]
                                              : input_right[frame];
      const auto read = (write + capacity - delay) % capacity;
      output_left[frame] = left_[read];
      if (output_right != nullptr) {
        output_right[frame] = right_[read];
      }
      write_index_ = (write + 1U) % capacity;
    }
  }

 private:
  std::vector<float> left_;
  std::vector<float> right_;
  std::size_t write_index_ = 0;
};

inline auto DelaySamples(const double sample_rate,
                         const std::uint32_t delay_ms) noexcept
    -> std::size_t {
  return static_cast<std::size_t>(std::max(
      0LL, static_cast<long long>(std::llround(
                 std::max(0.0, sample_rate) * delay_ms / 1000.0))));
}

inline auto SignedDelaySamples(const double sample_rate,
                               const std::int32_t delay_ms) noexcept
    -> std::size_t {
  const auto magnitude = delay_ms < 0
                             ? static_cast<std::uint32_t>(
                                   -static_cast<std::int64_t>(delay_ms))
                             : static_cast<std::uint32_t>(delay_ms);
  return DelaySamples(sample_rate, magnitude);
}

}  // namespace beatrice::common

#endif  // BEATRICE_COMMON_STEREO_DELAY_LINE_H_
