// Copyright (c) 2024-2026 Project Beatrice and Contributors

#ifndef BEATRICE_COMMON_AUDIO_ENGINE_H_
#define BEATRICE_COMMON_AUDIO_ENGINE_H_

#include <istream>
#include <ostream>
#include <vector>

#include "common/error.h"
#include "common/parameter_schema.h"
#include "common/processor_proxy.h"

namespace beatrice::common {

// Host-independent description of one realtime audio block. The engine accepts
// mono or stereo input, always converts a mono voice signal, and can produce a
// stereo output when output_right is supplied.
struct AudioBlock {
  const float* input_left = nullptr;
  const float* input_right = nullptr;
  float* output_left = nullptr;
  float* output_right = nullptr;
  // Optional realtime tap immediately before model conversion.  This is
  // intentionally supplied by the caller so standalone recording can copy
  // the signal after input cleanup and input gain without changing the VST
  // signal path.  It is never allocated or owned by the engine.
  float* pre_conversion = nullptr;
  int num_samples = 0;
  bool input_silent = false;
};

struct AudioProcessResult {
  ErrorCode error = ErrorCode::kSuccess;
  bool output_silent = true;
};

// Shared realtime signal-path controller for the VST and future standalone
// clients. Device management, host parameter queues, recording, and file I/O
// deliberately remain outside this class.
class AudioEngine {
 public:
  explicit AudioEngine(const ParameterSchema& schema) : processor_(schema) {}

  // Must be called outside the realtime callback. All block-sized work buffers
  // owned by this class are allocated here.
  auto Prepare(double sample_rate, int max_samples_per_block) -> ErrorCode;

  auto Process(const AudioBlock& block) -> AudioProcessResult;

  template <typename T>
  auto SetParameter(const ParameterID id, const T& value) -> ErrorCode {
    return processor_.SetParameter(id, value);
  }
  [[nodiscard]] auto GetParameter(ParameterID id) const
      -> const ParameterState::Value& {
    return processor_.GetParameter(id);
  }
  auto ReadState(std::istream& stream) -> ErrorCode {
    return processor_.Read(stream);
  }
  auto WriteState(std::ostream& stream) const -> ErrorCode {
    return processor_.Write(stream);
  }

  [[nodiscard]] auto GetLatencySamples() const -> int;
  [[nodiscard]] auto IsLatencyReportingEnabled() const -> bool;
  auto ResetContext() -> ErrorCode;

 private:
  void ClearOutput(const AudioBlock& block) const noexcept;

  ProcessorProxy processor_;
  std::vector<float> dry_buffer_;
  std::vector<float> conversion_off_buffer_;
  float conversion_off_mix_ = 0.0F;
  float conversion_off_mix_step_ = 1.0F;
};

}  // namespace beatrice::common

#endif  // BEATRICE_COMMON_AUDIO_ENGINE_H_
