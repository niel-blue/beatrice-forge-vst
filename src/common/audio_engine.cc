// Copyright (c) 2024-2026 Project Beatrice and Contributors

#include "common/audio_engine.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <variant>

namespace beatrice::common {

auto AudioEngine::Prepare(const double sample_rate,
                          const int max_samples_per_block) -> ErrorCode {
  const auto error = processor_.SetSampleRate(sample_rate);
  if (error != ErrorCode::kSuccess) {
    return error;
  }

  const auto buffer_size = std::max(0, max_samples_per_block);
  dry_buffer_.assign(buffer_size, 0.0F);
  conversion_off_buffer_.assign(buffer_size, 0.0F);
  const auto fade_samples = std::max(1.0, sample_rate * 0.04);
  conversion_off_mix_step_ = static_cast<float>(1.0 / fade_samples);
  conversion_off_mix_ = 0.0F;
  return ErrorCode::kSuccess;
}

void AudioEngine::ClearOutput(const AudioBlock& block) const noexcept {
  if (block.output_left != nullptr && block.num_samples > 0) {
    std::memset(block.output_left, 0,
                static_cast<std::size_t>(block.num_samples) * sizeof(float));
  }
  if (block.output_right != nullptr && block.num_samples > 0) {
    std::memset(block.output_right, 0,
                static_cast<std::size_t>(block.num_samples) * sizeof(float));
  }
}

auto AudioEngine::Process(const AudioBlock& block) -> AudioProcessResult {
  if (block.num_samples <= 0) {
    return {};
  }
  if (block.input_left == nullptr || block.output_left == nullptr ||
      static_cast<int>(dry_buffer_.size()) < block.num_samples ||
      static_cast<int>(conversion_off_buffer_.size()) < block.num_samples) {
    ClearOutput(block);
    return {.error = ErrorCode::kUnknownError, .output_silent = true};
  }

  for (auto i = 0; i < block.num_samples; ++i) {
    auto mono = block.input_silent ? 0.0F : block.input_left[i];
    if (!block.input_silent && block.input_right != nullptr) {
      mono = (mono + block.input_right[i]) * 0.5F;
    }
    dry_buffer_[i] = mono;
    block.output_left[i] = mono;
  }

  auto input_silent = block.input_silent;
  if (!input_silent) {
    input_silent = true;
    for (auto i = 0; i < block.num_samples; ++i) {
      if (block.output_left[i] != 0.0F) {
        input_silent = false;
        break;
      }
    }
  }

  auto result = AudioProcessResult{};
  const auto conversion_off =
      std::get<int>(processor_.GetParameter(ParameterID::kBypass)) != 0;
  const auto needs_converted =
      !conversion_off || conversion_off_mix_ < 0.999F;
  if (conversion_off) {
    std::memcpy(conversion_off_buffer_.data(), dry_buffer_.data(),
                static_cast<std::size_t>(block.num_samples) * sizeof(float));
    if (!input_silent) {
      result.error = processor_.GetCore()->ProcessWithoutConversion(
          conversion_off_buffer_.data(), conversion_off_buffer_.data(),
          block.num_samples, block.pre_conversion);
    }
  }

  auto converted_active = false;
  if (needs_converted) {
    if (!input_silent) {
      result.error = processor_.GetCore()->Process(
          block.output_left, block.output_left, block.num_samples,
          block.output_right, block.pre_conversion);
      converted_active = true;
    } else if (processor_.GetCore()->HasOutputEffectsTail()) {
      result.error = processor_.GetCore()->ProcessOutputEffectsTail(
          block.output_left, block.output_right, block.num_samples);
      converted_active = true;
    } else {
      ClearOutput(block);
    }
  }

  if (!needs_converted) {
    processor_.GetCore()->DiscardOutputEffectsTail();
    std::memcpy(block.output_left, conversion_off_buffer_.data(),
                static_cast<std::size_t>(block.num_samples) * sizeof(float));
    if (block.output_right != nullptr) {
      std::memcpy(block.output_right, conversion_off_buffer_.data(),
                  static_cast<std::size_t>(block.num_samples) * sizeof(float));
    }
  }

  const auto target_mix = conversion_off ? 1.0F : 0.0F;
  for (auto i = 0; i < block.num_samples; ++i) {
    conversion_off_mix_ +=
        std::clamp(target_mix - conversion_off_mix_,
                   -conversion_off_mix_step_, conversion_off_mix_step_);
    const auto converted_left =
        converted_active ? block.output_left[i] : 0.0F;
    const auto converted_right =
        block.output_right != nullptr
            ? (converted_active ? block.output_right[i] : 0.0F)
            : converted_left;
    const auto dry = conversion_off ? conversion_off_buffer_[i]
                                    : dry_buffer_[i];
    block.output_left[i] =
        converted_left * (1.0F - conversion_off_mix_) +
        dry * conversion_off_mix_;
    if (block.output_right != nullptr) {
      block.output_right[i] =
          converted_right * (1.0F - conversion_off_mix_) +
          dry * conversion_off_mix_;
    }
  }

  result.output_silent = true;
  for (auto i = 0; i < block.num_samples; ++i) {
    if (block.output_left[i] != 0.0F ||
        (block.output_right != nullptr && block.output_right[i] != 0.0F)) {
      result.output_silent = false;
      break;
    }
  }
  return result;
}

auto AudioEngine::GetLatencySamples() const -> int {
  return processor_.GetCore()->GetLatencySamples();
}

auto AudioEngine::IsLatencyReportingEnabled() const -> bool {
  return std::get<int>(
             processor_.GetParameter(ParameterID::kLatencyReporting)) != 0;
}

auto AudioEngine::ResetContext() -> ErrorCode {
  return processor_.GetCore()->ResetContext();
}

}  // namespace beatrice::common
