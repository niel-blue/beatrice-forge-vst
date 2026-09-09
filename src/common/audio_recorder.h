// Copyright (c) 2024-2026 Project Beatrice and Contributors

#ifndef BEATRICE_COMMON_AUDIO_RECORDER_H_
#define BEATRICE_COMMON_AUDIO_RECORDER_H_

#include <atomic>
#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <thread>
#include <vector>

namespace beatrice::common {

enum class RecordingMode {
  kOff,
  kOutput,
  kSeparateInputOutput,
  kStereoInputOutput,
};

// kOff is retained as a hidden compatibility value for states written by
// older versions.  It is intentionally not exposed in either UI.
inline constexpr std::array<const char*, 3> kRecordingModeLabels = {
    "Output", "Input/Output Separate", "Input/Output L-R"};

inline auto NormalizeRecordingMode(const RecordingMode mode) noexcept
    -> RecordingMode {
  switch (mode) {
    case RecordingMode::kOutput:
    case RecordingMode::kSeparateInputOutput:
    case RecordingMode::kStereoInputOutput:
      return mode;
    case RecordingMode::kOff:
    default:
      // Older saved states used kOff as the default.  Recording now always
      // starts from the Output mode, so migrate that value on read.
      return RecordingMode::kOutput;
  }
}

inline auto RecordingModeFromMenuIndex(const std::int32_t index) noexcept
    -> RecordingMode {
  switch (index) {
    case 1:
      return RecordingMode::kSeparateInputOutput;
    case 2:
      return RecordingMode::kStereoInputOutput;
    case 0:
    default:
      return RecordingMode::kOutput;
  }
}

inline auto RecordingModeToMenuIndex(const RecordingMode mode) noexcept
    -> std::int32_t {
  switch (NormalizeRecordingMode(mode)) {
    case RecordingMode::kSeparateInputOutput:
      return 1;
    case RecordingMode::kStereoInputOutput:
      return 2;
    case RecordingMode::kOutput:
    case RecordingMode::kOff:
    default:
      return 0;
  }
}

// Gain applied to the additional input in the recording mix. The live
// standalone output applies the same gain when routing ADD BGM, while the
// converted voice output gain remains independent.
inline constexpr double kMinAdditionalInputGainDb = -20.0;
inline constexpr double kMaxAdditionalInputGainDb = 20.0;
inline constexpr double kDefaultAdditionalInputGainDb = 0.0;

struct RecordingSettings {
  RecordingMode mode = RecordingMode::kOutput;
  // The selected file name is used as a location hint. Recordings are written
  // beside it using the automatic Beatrice-Forge-Rec-* naming rule.
  std::filesystem::path base_path;
  double sample_rate = 48000.0;
  // When enabled, the additional input is mixed into the recorded output
  // channels only.  It never changes the host or monitor output path.
  bool additional_input_enabled = false;
  double additional_input_gain_db = kDefaultAdditionalInputGainDb;
  // Legacy direct-recorder delay. Realtime host/standalone streams apply the
  // same delay before their external-output/recording fan-out and pass zero
  // here to avoid applying it twice.
  std::uint32_t voice_delay_ms = 0;
};

inline constexpr std::uint32_t kMaxVoiceDelayMs = 200U;

struct RecordingStatus {
  bool recording = false;
  std::uint64_t frames = 0;
  std::uint64_t dropped_frames = 0;
  std::string error;
};

// Host-independent realtime recorder. The audio side only copies into a
// fixed single-producer/single-consumer ring; WAV writes are performed by a
// worker thread. The same implementation is used by the standalone client
// and by the optional VST external-output recorder.
class AudioRecorder {
 public:
  AudioRecorder();
  ~AudioRecorder();

  AudioRecorder(const AudioRecorder&) = delete;
  auto operator=(const AudioRecorder&) -> AudioRecorder& = delete;

  auto Start(const RecordingSettings& settings) -> bool;
  void Stop();
  // Updates the additional-input mix used by the recording writer. This is
  // safe while recording so the BGM gain control can remain live.
  void SetAdditionalInputGainDb(double gain_db);

  void Push(float pre_conversion, float output_left, float output_right,
            float additional_input_left = 0.0F,
            float additional_input_right = 0.0F) noexcept;

  [[nodiscard]] auto IsRecording() const -> bool {
    return recording_.load(std::memory_order_acquire);
  }
  [[nodiscard]] auto GetStatus() const -> RecordingStatus;

 private:
  struct Frame {
    float pre_conversion = 0.0F;
    float output_left = 0.0F;
    float output_right = 0.0F;
    float additional_input_left = 0.0F;
    float additional_input_right = 0.0F;
  };
  class WaveWriter;

  void WriterLoop();
  void SetError(std::string error);
  auto PrepareWriters(const RecordingSettings& settings) -> bool;
  auto ReserveRecordingNumber(const std::filesystem::path& directory,
                              const std::string& date)
      -> std::optional<std::uint32_t>;
  void ReleaseRecordingNumber();
  void FinalizeWriters();

  std::vector<Frame> queue_;
  std::atomic<std::size_t> write_index_{0};
  std::atomic<std::size_t> read_index_{0};
  std::atomic_bool recording_{false};
  std::atomic_bool stopping_{false};
  std::atomic<std::uint64_t> frame_count_{0};
  std::atomic<std::uint64_t> dropped_frames_{0};
  std::thread writer_thread_;

  RecordingSettings settings_;
  std::filesystem::path input_path_;
  std::filesystem::path output_path_;
  std::filesystem::path stereo_path_;
  std::filesystem::path reservation_path_;
  std::unique_ptr<WaveWriter> input_writer_;
  std::unique_ptr<WaveWriter> output_writer_;
  std::unique_ptr<WaveWriter> stereo_writer_;
  std::atomic<float> additional_input_gain_ = 0.0F;
  std::uint32_t voice_delay_frames_ = 0;
  mutable std::atomic_flag error_lock_ = ATOMIC_FLAG_INIT;
  std::string error_;
};

}  // namespace beatrice::common

#endif  // BEATRICE_COMMON_AUDIO_RECORDER_H_
