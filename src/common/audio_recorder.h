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

inline constexpr std::array<const char*, 4> kRecordingModeLabels = {
    "OFF", "Output", "Input/Output Separate", "Input/Output L-R"};

struct RecordingSettings {
  RecordingMode mode = RecordingMode::kOff;
  // The selected file name is used as a location hint. Recordings are written
  // beside it using the automatic Beatrice-Forge-Rec-* naming rule.
  std::filesystem::path base_path;
  double sample_rate = 48000.0;
};

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

  void Push(float pre_conversion, float output_left,
            float output_right) noexcept;

  [[nodiscard]] auto IsRecording() const -> bool {
    return recording_.load(std::memory_order_acquire);
  }
  [[nodiscard]] auto GetStatus() const -> RecordingStatus;

 private:
  struct Frame {
    float pre_conversion = 0.0F;
    float output_left = 0.0F;
    float output_right = 0.0F;
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
  mutable std::atomic_flag error_lock_ = ATOMIC_FLAG_INIT;
  std::string error_;
};

}  // namespace beatrice::common

#endif  // BEATRICE_COMMON_AUDIO_RECORDER_H_
