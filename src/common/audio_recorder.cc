// Copyright (c) 2024-2026 Project Beatrice and Contributors

#include "common/audio_recorder.h"

#include "common/branding.h"

#include <algorithm>
#include <chrono>
#include <cstring>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <limits>
#include <sstream>
#include <system_error>
#include <utility>

namespace beatrice::common {

namespace {

constexpr std::uint32_t kFirstRecordingNumber = 1;
constexpr std::uint32_t kLastRecordingNumber = 999999;

auto CurrentRecordingDate() -> std::string {
  const auto now = std::chrono::system_clock::now();
  const auto time = std::chrono::system_clock::to_time_t(now);
  std::tm local_time{};
#if defined(_WIN32)
  localtime_s(&local_time, &time);
#else
  localtime_r(&time, &local_time);
#endif
  auto result = std::ostringstream{};
  result << std::put_time(&local_time, "%Y%m%d");
  return result.str();
}

auto RecordingPath(const std::filesystem::path& directory,
                   const std::string& date, const char* const kind,
                   const std::uint32_t number) -> std::filesystem::path {
  auto filename = std::ostringstream{};
  filename << kRecordingFilePrefix << '-' << date << '-' << std::setw(6)
           << std::setfill('0') << number << '-' << kind << ".wav";
  return directory / filename.str();
}

// Reserve numbers used by the previous product prefix and the two earlier
// naming schemes as well. This prevents a new Beatrice Forge file from
// reusing a number already present in the user's recording folder.
auto LegacyBeatriceRecordingPath(const std::filesystem::path& directory,
                                 const std::string& date,
                                 const char* const kind,
                                 const std::uint32_t number)
    -> std::filesystem::path {
  auto filename = std::ostringstream{};
  filename << kPreviousRecordingFilePrefix << '-' << date << '-'
           << std::setw(6) << std::setfill('0') << number << '-' << kind
           << ".wav";
  return directory / filename.str();
}

auto PreviousRecordingPath(const std::filesystem::path& directory,
                           const std::string& date, const char* const kind,
                           const std::uint32_t number)
    -> std::filesystem::path {
  auto filename = std::ostringstream{};
  filename << "Rec-" << date << '-' << std::setw(6) << std::setfill('0')
           << number << '-' << kind << ".wav";
  return directory / filename.str();
}

auto OlderRecordingPath(const std::filesystem::path& directory,
                        const std::string& date, const char* const kind,
                        const std::uint32_t number)
    -> std::filesystem::path {
  auto filename = std::ostringstream{};
  filename << "Rec-" << kind << '-' << date << '-' << std::setw(6)
           << std::setfill('0') << number << ".wav";
  return directory / filename.str();
}

auto NumberIsUsed(const std::filesystem::path& directory,
                  const std::string& date, const std::uint32_t number) -> bool {
  for (const auto* const kind : {"LR", "Input", "Output"}) {
    if (std::filesystem::exists(RecordingPath(directory, date, kind, number)) ||
        std::filesystem::exists(
            LegacyBeatriceRecordingPath(directory, date, kind, number)) ||
        std::filesystem::exists(
            PreviousRecordingPath(directory, date, kind, number)) ||
        std::filesystem::exists(OlderRecordingPath(directory, date, kind,
                                                    number))) {
      return true;
    }
  }
  return false;
}

auto ReservationPath(const std::filesystem::path& directory,
                     const std::string& date,
                     const std::uint32_t number) -> std::filesystem::path {
  auto filename = std::ostringstream{};
  filename << "." << kRecordingFilePrefix << '-' << date << '-' << std::setw(6)
           << std::setfill('0') << number << ".lock";
  return directory / filename.str();
}

void WriteU16(std::ofstream& stream, const std::uint16_t value) {
  const std::array<char, 2> bytes = {
      static_cast<char>(value & 0xffU),
      static_cast<char>((value >> 8U) & 0xffU)};
  stream.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
}

void WriteU32(std::ofstream& stream, const std::uint32_t value) {
  const std::array<char, 4> bytes = {
      static_cast<char>(value & 0xffU),
      static_cast<char>((value >> 8U) & 0xffU),
      static_cast<char>((value >> 16U) & 0xffU),
      static_cast<char>((value >> 24U) & 0xffU)};
  stream.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
}

}  // namespace

class AudioRecorder::WaveWriter {
 public:
  WaveWriter(const std::filesystem::path& path, const double sample_rate,
             const std::uint16_t channels)
      : path_(path),
        sample_rate_(static_cast<std::uint32_t>(std::max(1.0, sample_rate))),
        channels_(channels) {}

  auto Open() -> bool {
    stream_.open(path_, std::ios::binary | std::ios::trunc);
    if (!stream_) {
      return false;
    }
    stream_.write("RIFF", 4);
    riff_size_position_ = stream_.tellp();
    WriteU32(stream_, 0);
    stream_.write("WAVEfmt ", 8);
    WriteU32(stream_, 16);
    WriteU16(stream_, 3);  // IEEE float
    WriteU16(stream_, channels_);
    WriteU32(stream_, sample_rate_);
    const auto block_align = static_cast<std::uint16_t>(channels_ * 4U);
    WriteU32(stream_, sample_rate_ * block_align);
    WriteU16(stream_, block_align);
    WriteU16(stream_, 32);
    stream_.write("data", 4);
    data_size_position_ = stream_.tellp();
    WriteU32(stream_, 0);
    return static_cast<bool>(stream_);
  }

  void WriteMono(const float value) {
    stream_.write(reinterpret_cast<const char*>(&value), sizeof(value));
    data_bytes_ += sizeof(value);
  }

  void WriteStereo(const float left, const float right) {
    stream_.write(reinterpret_cast<const char*>(&left), sizeof(left));
    stream_.write(reinterpret_cast<const char*>(&right), sizeof(right));
    data_bytes_ += sizeof(left) + sizeof(right);
  }

  auto Finalize() -> bool {
    if (!stream_) {
      return false;
    }
    const auto end = stream_.tellp();
    stream_.seekp(riff_size_position_);
    const auto riff_size = static_cast<std::uint32_t>(
        std::min<std::uint64_t>(36U + data_bytes_,
                                std::numeric_limits<std::uint32_t>::max()));
    WriteU32(stream_, riff_size);
    stream_.seekp(data_size_position_);
    const auto data_size = static_cast<std::uint32_t>(std::min<std::uint64_t>(
        data_bytes_, std::numeric_limits<std::uint32_t>::max()));
    WriteU32(stream_, data_size);
    stream_.seekp(end);
    stream_.close();
    return true;
  }

 private:
  std::filesystem::path path_;
  std::uint32_t sample_rate_ = 48000;
  std::uint16_t channels_ = 1;
  std::ofstream stream_;
  std::streampos riff_size_position_{};
  std::streampos data_size_position_{};
  std::uint64_t data_bytes_ = 0;
};

AudioRecorder::AudioRecorder()
    : queue_(static_cast<std::size_t>(48000U * 8U) + 1U) {}

AudioRecorder::~AudioRecorder() { Stop(); }

void AudioRecorder::SetError(std::string error) {
  while (error_lock_.test_and_set(std::memory_order_acquire)) {
  }
  error_ = std::move(error);
  error_lock_.clear(std::memory_order_release);
}

auto AudioRecorder::ReserveRecordingNumber(
    const std::filesystem::path& directory, const std::string& date)
    -> std::optional<std::uint32_t> {
  // The lock is a directory because create_directory is an atomic
  // create-if-absent operation on the filesystems supported by Windows and
  // POSIX. This prevents two VST instances from selecting the same number.
  for (auto number = kFirstRecordingNumber; number <= kLastRecordingNumber;
       ++number) {
    if (NumberIsUsed(directory, date, number)) {
      continue;
    }
    const auto candidate = ReservationPath(directory, date, number);
    std::error_code filesystem_error;
    if (std::filesystem::create_directory(candidate, filesystem_error)) {
      reservation_path_ = candidate;
      return number;
    }
    if (filesystem_error &&
        filesystem_error != std::make_error_code(std::errc::file_exists)) {
      SetError("The recording number could not be reserved.");
      return std::nullopt;
    }
  }
  SetError("No unused recording number is available.");
  return std::nullopt;
}

void AudioRecorder::ReleaseRecordingNumber() {
  if (reservation_path_.empty()) {
    return;
  }
  std::error_code filesystem_error;
  std::filesystem::remove(reservation_path_, filesystem_error);
  reservation_path_.clear();
}

auto AudioRecorder::PrepareWriters(const RecordingSettings& settings) -> bool {
  if (settings.base_path.empty()) {
    SetError("Choose a recording location.");
    return false;
  }
  if (settings.sample_rate <= 0.0) {
    SetError("The host sample rate is not available yet.");
    return false;
  }
  std::error_code filesystem_error;
  const auto parent = settings.base_path.parent_path();
  if (!parent.empty()) {
    std::filesystem::create_directories(parent, filesystem_error);
  }
  if (filesystem_error) {
    SetError("The recording folder could not be created.");
    return false;
  }
  auto recording_directory = parent;
  if (recording_directory.empty()) {
    recording_directory = std::filesystem::current_path();
  }
  const auto date = CurrentRecordingDate();
  const auto number = ReserveRecordingNumber(recording_directory, date);
  if (!number.has_value()) {
    return false;
  }
  if (settings.mode == RecordingMode::kOutput) {
    output_path_ = RecordingPath(recording_directory, date, "Output", *number);
    output_writer_ = std::make_unique<WaveWriter>(output_path_,
                                                  settings.sample_rate, 2);
    if (!output_writer_->Open()) {
      SetError("The output recording file could not be opened.");
      return false;
    }
    return true;
  }
  if (settings.mode == RecordingMode::kSeparateInputOutput) {
    input_path_ = RecordingPath(recording_directory, date, "Input", *number);
    output_path_ = RecordingPath(recording_directory, date, "Output", *number);
    input_writer_ = std::make_unique<WaveWriter>(input_path_,
                                                 settings.sample_rate, 1);
    output_writer_ = std::make_unique<WaveWriter>(output_path_,
                                                  settings.sample_rate, 2);
    if (!input_writer_->Open() || !output_writer_->Open()) {
      SetError("The input/output recording files could not be opened.");
      return false;
    }
    return true;
  }
  if (settings.mode == RecordingMode::kStereoInputOutput) {
    stereo_path_ = RecordingPath(recording_directory, date, "LR", *number);
    stereo_writer_ = std::make_unique<WaveWriter>(stereo_path_,
                                                  settings.sample_rate, 2);
    if (!stereo_writer_->Open()) {
      SetError("The stereo recording file could not be opened.");
      return false;
    }
    return true;
  }
  return false;
}

auto AudioRecorder::Start(const RecordingSettings& settings) -> bool {
  Stop();
  settings_ = settings;
  SetError({});
  frame_count_.store(0, std::memory_order_relaxed);
  dropped_frames_.store(0, std::memory_order_relaxed);
  write_index_.store(0, std::memory_order_relaxed);
  read_index_.store(0, std::memory_order_relaxed);
  if (settings.mode == RecordingMode::kOff || !PrepareWriters(settings)) {
    FinalizeWriters();
    return false;
  }
  stopping_.store(false, std::memory_order_release);
  recording_.store(true, std::memory_order_release);
  writer_thread_ = std::thread(&AudioRecorder::WriterLoop, this);
  return true;
}

void AudioRecorder::Push(const float pre_conversion, const float output_left,
                         const float output_right) noexcept {
  if (!recording_.load(std::memory_order_acquire)) {
    return;
  }
  const auto write = write_index_.load(std::memory_order_relaxed);
  const auto next = (write + 1U) % queue_.size();
  if (next == read_index_.load(std::memory_order_acquire)) {
    dropped_frames_.fetch_add(1, std::memory_order_relaxed);
    return;
  }
  queue_[write] = {.pre_conversion = pre_conversion,
                   .output_left = output_left,
                   .output_right = output_right};
  write_index_.store(next, std::memory_order_release);
}

void AudioRecorder::WriterLoop() {
  while (recording_.load(std::memory_order_acquire) ||
         read_index_.load(std::memory_order_acquire) !=
             write_index_.load(std::memory_order_acquire)) {
    const auto read = read_index_.load(std::memory_order_relaxed);
    if (read == write_index_.load(std::memory_order_acquire)) {
      std::this_thread::sleep_for(std::chrono::milliseconds(2));
      continue;
    }
    const auto frame = queue_[read];
    read_index_.store((read + 1U) % queue_.size(), std::memory_order_release);
    if (settings_.mode == RecordingMode::kOutput) {
      output_writer_->WriteStereo(frame.output_left, frame.output_right);
    } else if (settings_.mode == RecordingMode::kSeparateInputOutput) {
      input_writer_->WriteMono(frame.pre_conversion);
      output_writer_->WriteStereo(frame.output_left, frame.output_right);
    } else if (settings_.mode == RecordingMode::kStereoInputOutput) {
      stereo_writer_->WriteStereo(frame.pre_conversion,
                                  (frame.output_left + frame.output_right) *
                                      0.5F);
    }
    frame_count_.fetch_add(1, std::memory_order_relaxed);
  }
}

void AudioRecorder::FinalizeWriters() {
  if (input_writer_) {
    static_cast<void>(input_writer_->Finalize());
    input_writer_.reset();
  }
  if (output_writer_) {
    static_cast<void>(output_writer_->Finalize());
    output_writer_.reset();
  }
  if (stereo_writer_) {
    static_cast<void>(stereo_writer_->Finalize());
    stereo_writer_.reset();
  }
  ReleaseRecordingNumber();
}

void AudioRecorder::Stop() {
  const auto was_recording = recording_.exchange(false, std::memory_order_acq_rel);
  if (was_recording && writer_thread_.joinable()) {
    writer_thread_.join();
  } else if (writer_thread_.joinable()) {
    writer_thread_.join();
  }
  stopping_.store(true, std::memory_order_release);
  FinalizeWriters();
  settings_ = {};
}

auto AudioRecorder::GetStatus() const -> RecordingStatus {
  while (error_lock_.test_and_set(std::memory_order_acquire)) {
  }
  auto error = error_;
  error_lock_.clear(std::memory_order_release);
  return {.recording = IsRecording(),
          .frames = frame_count_.load(std::memory_order_relaxed),
          .dropped_frames = dropped_frames_.load(std::memory_order_relaxed),
          .error = std::move(error)};
}

}  // namespace beatrice::common
