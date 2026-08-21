// Copyright (c) 2024-2026 Project Beatrice and Contributors

#ifndef BEATRICE_COMMON_STEREO_AUDIO_FIFO_H_
#define BEATRICE_COMMON_STEREO_AUDIO_FIFO_H_

#include <algorithm>
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace beatrice::common {

// A bounded single-producer/single-consumer FIFO for a secondary audio path.
// The VST process callback is the producer and the WASAPI worker is the
// consumer. Push/Pop never allocate or lock; a complete block is dropped when
// there is not enough room, keeping the producer's timing deterministic.
class StereoAudioFifo final {
 public:
  explicit StereoAudioFifo(const std::size_t capacity)
      : frames_(std::max<std::size_t>(capacity, 1)) {}

  void PushBlock(const float* const left, const float* const right,
                 const std::size_t count) noexcept {
    if (left == nullptr || count == 0) {
      return;
    }
    const auto write = write_.load(std::memory_order_relaxed);
    const auto read = read_.load(std::memory_order_acquire);
    const auto used = write > read ? std::min(write - read, frames_.size()) : 0;
    if (count > frames_.size() - used) {
      overflow_count_.fetch_add(count, std::memory_order_relaxed);
      return;
    }
    for (auto index = std::size_t{0}; index < count; ++index) {
      frames_[(write + index) % frames_.size()] = {
          left[index], right == nullptr ? left[index] : right[index]};
    }
    write_.store(write + count, std::memory_order_release);
  }

  auto Pop(float& left, float& right) noexcept -> bool {
    const auto read = read_.load(std::memory_order_relaxed);
    const auto write = write_.load(std::memory_order_acquire);
    if (read == write) {
      left = 0.0F;
      right = 0.0F;
      underrun_count_.fetch_add(1, std::memory_order_relaxed);
      return false;
    }
    const auto& frame = frames_[read % frames_.size()];
    left = frame[0];
    right = frame[1];
    read_.store(read + 1, std::memory_order_release);
    return true;
  }

  [[nodiscard]] auto Size() const noexcept -> std::size_t {
    const auto write = write_.load(std::memory_order_acquire);
    const auto read = read_.load(std::memory_order_acquire);
    return write > read ? std::min(write - read, frames_.size()) : 0;
  }

  void DiscardAll() noexcept {
    read_.store(write_.load(std::memory_order_acquire),
                std::memory_order_release);
  }

 private:
  std::vector<std::array<float, 2>> frames_;
  std::atomic<std::size_t> read_ = 0;
  std::atomic<std::size_t> write_ = 0;
  std::atomic<std::uint64_t> underrun_count_ = 0;
  std::atomic<std::uint64_t> overflow_count_ = 0;
};

}  // namespace beatrice::common

#endif  // BEATRICE_COMMON_STEREO_AUDIO_FIFO_H_
