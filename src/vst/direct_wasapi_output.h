// Copyright (c) 2026 Project Beatrice and Contributors

#ifndef BEATRICE_VST_DIRECT_WASAPI_OUTPUT_H_
#define BEATRICE_VST_DIRECT_WASAPI_OUTPUT_H_

#include <functional>
#include <memory>
#include <string>

namespace beatrice::vst {

enum class DirectWasapiMode {
  kShared,
  kExclusive,
};

enum class DirectWasapiStatus {
  kOff,
  kStarting,
  kActive,
  kUnavailable,
};

struct DirectWasapiConfig {
  std::string device_id;
  DirectWasapiMode mode = DirectWasapiMode::kShared;
  double source_sample_rate = 0.0;
};

// Optional secondary output owned by the VST processor. It never writes to
// the host's output buffers; it consumes a copy on a separate WASAPI worker.
class DirectWasapiOutput final {
 public:
  using StatusCallback =
      std::function<void(DirectWasapiStatus, const std::string&)>;

  explicit DirectWasapiOutput(StatusCallback status_callback);
  ~DirectWasapiOutput();

  DirectWasapiOutput(const DirectWasapiOutput&) = delete;
  auto operator=(const DirectWasapiOutput&) -> DirectWasapiOutput& = delete;

  void Start(const DirectWasapiConfig& config);
  void Stop() noexcept;
  void PushBlock(const float* left, const float* right,
                 std::size_t count) noexcept;

 private:
  class Impl;
  std::unique_ptr<Impl> impl_;
};

}  // namespace beatrice::vst

#endif  // BEATRICE_VST_DIRECT_WASAPI_OUTPUT_H_
