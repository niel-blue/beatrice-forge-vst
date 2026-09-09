// Copyright (c) 2026 Project Beatrice and Contributors

#ifndef BEATRICE_VST_PLUGIN_STATE_H_
#define BEATRICE_VST_PLUGIN_STATE_H_

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <limits>
#include <optional>
#include <string>
#include <string_view>

#include "common/audio_recorder.h"
#include "common/input_source.h"
#include "common/stereo_delay_line.h"

namespace beatrice::vst {

// Settings which belong to the VST instance, but are not audio parameters.
// These values are serialized into the host-owned VST state stream.
struct PersistedPluginUiState {
  bool direct_wasapi_enabled = false;
  bool direct_wasapi_exclusive = false;
  std::string direct_wasapi_device_id;
  bool application_input_enabled = false;
  bool additional_input_enabled = false;
  // Stable executable identity. A process id is intentionally not persisted
  // because it changes every time an application is launched.
  std::string application_input_identity;
  // A separate process identity for the recording-only application input.
  std::string additional_application_input_identity;
  common::RecordingMode recording_mode = common::RecordingMode::kOutput;
  std::filesystem::path recording_path;
  std::int32_t voice_delay_ms = 0;
  // Recording-only gain for the additional application input.  It never
  // affects live output or monitoring.
  double additional_input_gain_db = common::kDefaultAdditionalInputGainDb;
  common::InputSource input_source = common::InputSource::kDawInput;
  common::InputSource additional_input_source = common::InputSource::kOff;
  std::filesystem::path input_file_path;
  std::filesystem::path additional_input_file_path;
  bool input_file_playing = false;
  bool input_file_loop = false;
  bool additional_file_playing = false;
  bool additional_file_loop = false;
  double input_file_volume = 1.0;
  double additional_file_volume = 1.0;
};

namespace plugin_state_detail {

inline constexpr std::array<char, 16> kMagic = {
    'B', 'E', 'A', 'T', 'R', 'I', 'C', 'E',
    'V', 'S', 'T', 'S', 'T', 'A', 'T', 'E'};
inline constexpr std::uint32_t kVersion = 8;
inline constexpr std::uint32_t kLegacyVersion = 1;
inline constexpr std::uint32_t kOlderVersion = 2;
inline constexpr std::uint32_t kPreviousVersion = 3;
inline constexpr std::uint32_t kVoiceDelayVersion = 4;
inline constexpr std::uint32_t kAdditionalInputGainVersion = 5;
inline constexpr std::uint32_t kCurrentAdditionalInputGainVersion = 6;
inline constexpr std::uint32_t kAudioFileVersion = 7;
inline constexpr std::uint32_t kBgmDelayVersion = 8;
inline constexpr double kLegacyMinAdditionalInputGainDb = -60.0;
inline constexpr double kLegacyMaxAdditionalInputGainDb = 6.0;
inline constexpr std::uint32_t kDirectWasapiEnabled = 1U << 0U;
inline constexpr std::uint32_t kDirectWasapiExclusive = 1U << 1U;
inline constexpr std::uint32_t kApplicationInputEnabled = 1U << 2U;
inline constexpr std::uint32_t kAdditionalInputEnabled = 1U << 3U;
inline constexpr std::size_t kLegacyHeaderSize =
    kMagic.size() + 6U * sizeof(std::uint32_t);
inline constexpr std::size_t kHeaderSize =
    kMagic.size() + 15U * sizeof(std::uint32_t) + 3U * sizeof(double);
inline constexpr std::size_t kMaxParameterStateSize = 256U * 1024U * 1024U;
inline constexpr std::size_t kMaxTextSize = 16U * 1024U * 1024U;

inline void AppendU32(std::string& output, const std::uint32_t value) {
  output.push_back(static_cast<char>(value & 0xffU));
  output.push_back(static_cast<char>((value >> 8U) & 0xffU));
  output.push_back(static_cast<char>((value >> 16U) & 0xffU));
  output.push_back(static_cast<char>((value >> 24U) & 0xffU));
}

inline auto ReadU32(std::string_view input, std::size_t& offset,
                    std::uint32_t& value) -> bool {
  if (input.size() - offset < sizeof(std::uint32_t)) {
    return false;
  }
  const auto byte = [&input](const std::size_t index) {
    return static_cast<std::uint32_t>(
        static_cast<unsigned char>(input[index]));
  };
  value = byte(offset) | (byte(offset + 1U) << 8U) |
          (byte(offset + 2U) << 16U) | (byte(offset + 3U) << 24U);
  offset += sizeof(std::uint32_t);
  return true;
}

inline auto ReadDouble(std::string_view input, std::size_t& offset,
                       double& value) -> bool {
  if (input.size() - offset < sizeof(value)) {
    return false;
  }
  std::memcpy(&value, input.data() + offset, sizeof(value));
  offset += sizeof(value);
  return true;
}

inline auto PathToUtf8(const std::filesystem::path& path) -> std::string {
  const auto utf8 = path.u8string();
  return std::string(reinterpret_cast<const char*>(utf8.data()), utf8.size());
}

inline auto PathFromUtf8(const std::string_view utf8)
    -> std::filesystem::path {
  const auto* const begin = reinterpret_cast<const char8_t*>(utf8.data());
  const auto* const end = begin + utf8.size();
  return std::filesystem::path(std::u8string(begin, end));
}

}  // namespace plugin_state_detail

// The old VST state was the raw ParameterState stream. New states retain
// that stream verbatim inside this envelope, so old projects remain readable.
inline auto EncodePluginState(
    const std::string_view parameter_state,
    const PersistedPluginUiState& ui_state) -> std::optional<std::string> {
  using namespace plugin_state_detail;
  const auto device_id = ui_state.direct_wasapi_enabled
                             ? ui_state.direct_wasapi_device_id
                             : std::string{};
  const auto application_identity = ui_state.application_input_identity;
  const auto additional_application_identity =
      ui_state.additional_application_input_identity;
  const auto recording_path = PathToUtf8(ui_state.recording_path);
  const auto input_file_path = PathToUtf8(ui_state.input_file_path);
  const auto additional_input_file_path =
      PathToUtf8(ui_state.additional_input_file_path);
  const auto mode = static_cast<std::uint32_t>(ui_state.recording_mode);
  const auto input_source = static_cast<std::uint32_t>(ui_state.input_source);
  const auto additional_input_source =
      static_cast<std::uint32_t>(ui_state.additional_input_source);
  if (mode > static_cast<std::uint32_t>(common::RecordingMode::kStereoInputOutput) ||
      parameter_state.size() > kMaxParameterStateSize ||
      device_id.size() > kMaxTextSize || recording_path.size() > kMaxTextSize ||
      application_identity.size() > kMaxTextSize ||
      additional_application_identity.size() > kMaxTextSize ||
      parameter_state.size() > std::numeric_limits<std::uint32_t>::max() ||
      device_id.size() > std::numeric_limits<std::uint32_t>::max() ||
      recording_path.size() > std::numeric_limits<std::uint32_t>::max() ||
      application_identity.size() > std::numeric_limits<std::uint32_t>::max() ||
      additional_application_identity.size() >
          std::numeric_limits<std::uint32_t>::max() ||
      input_file_path.size() > kMaxTextSize ||
      additional_input_file_path.size() > kMaxTextSize ||
      input_file_path.size() > std::numeric_limits<std::uint32_t>::max() ||
      additional_input_file_path.size() >
          std::numeric_limits<std::uint32_t>::max() ||
      input_source > static_cast<std::uint32_t>(common::InputSource::kAudioFile) ||
      additional_input_source >
          static_cast<std::uint32_t>(common::InputSource::kAudioFile) ||
      !std::isfinite(ui_state.additional_input_gain_db) ||
      ui_state.additional_input_gain_db <
          common::kMinAdditionalInputGainDb ||
      ui_state.additional_input_gain_db >
          common::kMaxAdditionalInputGainDb ||
      !std::isfinite(ui_state.input_file_volume) ||
      !std::isfinite(ui_state.additional_file_volume) ||
      ui_state.input_file_volume < 0.0 || ui_state.input_file_volume > 1.0 ||
      ui_state.additional_file_volume < 0.0 ||
      ui_state.additional_file_volume > 1.0) {
    return std::nullopt;
  }

  auto output = std::string{};
  output.reserve(kHeaderSize + parameter_state.size() + device_id.size() +
                 recording_path.size() + application_identity.size() +
                 additional_application_identity.size());
  output.append(kMagic.data(), kMagic.size());
  AppendU32(output, kVersion);
  AppendU32(output, static_cast<std::uint32_t>(parameter_state.size()));
  AppendU32(output, static_cast<std::uint32_t>(device_id.size()));
  AppendU32(output, static_cast<std::uint32_t>(recording_path.size()));
  AppendU32(output, static_cast<std::uint32_t>(application_identity.size()));
  AppendU32(output,
            static_cast<std::uint32_t>(additional_application_identity.size()));
  auto flags = std::uint32_t{0};
  if (ui_state.direct_wasapi_enabled && !device_id.empty()) {
    flags |= kDirectWasapiEnabled;
  }
  if (ui_state.application_input_enabled && !application_identity.empty()) {
    flags |= kApplicationInputEnabled;
  }
  if (ui_state.additional_input_enabled &&
      !additional_application_identity.empty()) {
    flags |= kAdditionalInputEnabled;
  }
  if (ui_state.voice_delay_ms < common::kMinBgmDelayMs ||
      ui_state.voice_delay_ms > common::kMaxBgmDelayMs) {
    return std::nullopt;
  }
  AppendU32(output, flags);
  AppendU32(output, mode);
  AppendU32(output, static_cast<std::uint32_t>(ui_state.voice_delay_ms));
  output.append(reinterpret_cast<const char*>(&ui_state.additional_input_gain_db),
                sizeof(ui_state.additional_input_gain_db));
  AppendU32(output, input_source);
  AppendU32(output, additional_input_source);
  AppendU32(output, static_cast<std::uint32_t>(input_file_path.size()));
  AppendU32(output,
            static_cast<std::uint32_t>(additional_input_file_path.size()));
  auto input_file_flags = std::uint32_t{0};
  if (ui_state.input_file_playing) input_file_flags |= 1U;
  if (ui_state.input_file_loop) input_file_flags |= 2U;
  auto additional_file_flags = std::uint32_t{0};
  if (ui_state.additional_file_playing) additional_file_flags |= 1U;
  if (ui_state.additional_file_loop) additional_file_flags |= 2U;
  AppendU32(output, input_file_flags);
  AppendU32(output, additional_file_flags);
  output.append(reinterpret_cast<const char*>(&ui_state.input_file_volume),
                sizeof(ui_state.input_file_volume));
  output.append(
      reinterpret_cast<const char*>(&ui_state.additional_file_volume),
      sizeof(ui_state.additional_file_volume));
  output.append(parameter_state.data(), parameter_state.size());
  output.append(device_id.data(), device_id.size());
  output.append(recording_path.data(), recording_path.size());
  output.append(application_identity.data(), application_identity.size());
  output.append(additional_application_identity.data(),
                additional_application_identity.size());
  output.append(input_file_path.data(), input_file_path.size());
  output.append(additional_input_file_path.data(),
                additional_input_file_path.size());
  return output;
}

inline auto DecodePluginState(const std::string_view input,
                              std::string& parameter_state,
                              PersistedPluginUiState& ui_state) -> bool {
  using namespace plugin_state_detail;
  ui_state = {};
  if (input.size() < kMagic.size() ||
      input.compare(0, kMagic.size(), kMagic.data(), kMagic.size()) != 0) {
    // Compatibility with states written before the envelope existed.
    parameter_state.assign(input.data(), input.size());
    return true;
  }
  if (input.size() < kLegacyHeaderSize) {
    return false;
  }

  auto offset = kMagic.size();
  std::uint32_t version = 0;
  std::uint32_t parameter_size = 0;
  std::uint32_t device_size = 0;
  std::uint32_t recording_path_size = 0;
  std::uint32_t application_identity_size = 0;
  std::uint32_t additional_application_identity_size = 0;
  std::uint32_t flags = 0;
  std::uint32_t recording_mode = 0;
  std::uint32_t voice_delay_raw = 0;
  auto additional_input_gain_db = common::kDefaultAdditionalInputGainDb;
  std::uint32_t input_source = static_cast<std::uint32_t>(
      common::InputSource::kDawInput);
  std::uint32_t additional_input_source = static_cast<std::uint32_t>(
      common::InputSource::kOff);
  std::uint32_t input_file_path_size = 0;
  std::uint32_t additional_input_file_path_size = 0;
  std::uint32_t input_file_flags = 0;
  std::uint32_t additional_file_flags = 0;
  auto input_file_volume = 1.0;
  auto additional_file_volume = 1.0;
  if (!ReadU32(input, offset, version) ||
      !ReadU32(input, offset, parameter_size) ||
      !ReadU32(input, offset, device_size) ||
      !ReadU32(input, offset, recording_path_size) ||
      (version >= kOlderVersion &&
       !ReadU32(input, offset, application_identity_size)) ||
      (version >= kPreviousVersion &&
       !ReadU32(input, offset, additional_application_identity_size)) ||
      !ReadU32(input, offset, flags) ||
      !ReadU32(input, offset, recording_mode) ||
      (version != kVersion && version != kAdditionalInputGainVersion &&
       version != kCurrentAdditionalInputGainVersion &&
       version != kAudioFileVersion &&
       version != kVoiceDelayVersion &&
       version != kPreviousVersion &&
       version != kOlderVersion && version != kLegacyVersion) ||
      (version >= kVoiceDelayVersion &&
       !ReadU32(input, offset, voice_delay_raw)) ||
      (version >= kAdditionalInputGainVersion &&
       !ReadDouble(input, offset, additional_input_gain_db)) ||
      (version >= kAudioFileVersion &&
       (!ReadU32(input, offset, input_source) ||
        !ReadU32(input, offset, additional_input_source) ||
        !ReadU32(input, offset, input_file_path_size) ||
        !ReadU32(input, offset, additional_input_file_path_size) ||
        !ReadU32(input, offset, input_file_flags) ||
        !ReadU32(input, offset, additional_file_flags) ||
        !ReadDouble(input, offset, input_file_volume) ||
        !ReadDouble(input, offset, additional_file_volume))) ||
      parameter_size > kMaxParameterStateSize || device_size > kMaxTextSize ||
      recording_path_size > kMaxTextSize ||
      application_identity_size > kMaxTextSize ||
      additional_application_identity_size > kMaxTextSize ||
      input_file_path_size > kMaxTextSize ||
      additional_input_file_path_size > kMaxTextSize ||
      input_source > static_cast<std::uint32_t>(common::InputSource::kAudioFile) ||
      additional_input_source >
          static_cast<std::uint32_t>(common::InputSource::kAudioFile) ||
      recording_mode > 3U) {
    return false;
  }
  auto voice_delay_ms = std::int32_t{0};
  if (version >= kBgmDelayVersion) {
    voice_delay_ms = static_cast<std::int32_t>(voice_delay_raw);
    if (voice_delay_ms < common::kMinBgmDelayMs ||
        voice_delay_ms > common::kMaxBgmDelayMs) {
      return false;
    }
  } else {
    if (voice_delay_raw > common::kMaxVoiceDelayMs) {
      return false;
    }
    // Older states stored a positive Voice Delay, which delayed the voice.
    // Preserve that audible behavior as a negative BGM-relative offset.
    voice_delay_ms = -static_cast<std::int32_t>(std::min(
        voice_delay_raw, static_cast<std::uint32_t>(common::kMaxBgmDelayMs)));
  }
  const auto gain_min = version >= kCurrentAdditionalInputGainVersion
                            ? common::kMinAdditionalInputGainDb
                            : kLegacyMinAdditionalInputGainDb;
  const auto gain_max = version >= kCurrentAdditionalInputGainVersion
                            ? common::kMaxAdditionalInputGainDb
                            : kLegacyMaxAdditionalInputGainDb;
  if (!std::isfinite(additional_input_gain_db) ||
      additional_input_gain_db < gain_min || additional_input_gain_db > gain_max) {
    return false;
  }
  if (!std::isfinite(input_file_volume) ||
      !std::isfinite(additional_file_volume) || input_file_volume < 0.0 ||
      input_file_volume > 1.0 || additional_file_volume < 0.0 ||
      additional_file_volume > 1.0) {
    return false;
  }
  const auto remaining = input.size() - offset;
  const auto payload_size = static_cast<std::uint64_t>(parameter_size) +
                            static_cast<std::uint64_t>(device_size) +
                            static_cast<std::uint64_t>(recording_path_size) +
                            static_cast<std::uint64_t>(
                                version < kOlderVersion
                                    ? 0U
                                    : application_identity_size) +
                            static_cast<std::uint64_t>(
                                version >= kPreviousVersion
                                    ? additional_application_identity_size
                                    : 0U) +
                            static_cast<std::uint64_t>(
                                version >= kAudioFileVersion
                                    ? input_file_path_size
                                    : 0U) +
                            static_cast<std::uint64_t>(
                                version >= kAudioFileVersion
                                    ? additional_input_file_path_size
                                    : 0U);
  if (payload_size != remaining) {
    return false;
  }

  parameter_state.assign(input.data() + offset, parameter_size);
  offset += parameter_size;
  const auto device_id = std::string_view(input.data() + offset, device_size);
  offset += device_size;
  const auto recording_path =
      std::string_view(input.data() + offset, recording_path_size);
  offset += recording_path_size;
  const auto application_identity =
      version >= kOlderVersion
          ? std::string_view(input.data() + offset, application_identity_size)
          : std::string_view{};
  if (version >= kOlderVersion) {
    offset += application_identity_size;
  }
  const auto additional_application_identity =
      version >= kPreviousVersion
          ? std::string_view(input.data() + offset,
                             additional_application_identity_size)
          : std::string_view{};
  if (version >= kPreviousVersion) {
    offset += additional_application_identity_size;
  }
  const auto input_file_path =
      version >= kAudioFileVersion
          ? std::string_view(input.data() + offset, input_file_path_size)
          : std::string_view{};
  if (version >= kAudioFileVersion) {
    offset += input_file_path_size;
  }
  const auto additional_input_file_path =
      version >= kAudioFileVersion
          ? std::string_view(input.data() + offset,
                             additional_input_file_path_size)
          : std::string_view{};

  ui_state.direct_wasapi_enabled =
      (flags & kDirectWasapiEnabled) != 0U && !device_id.empty();
  ui_state.direct_wasapi_exclusive =
      ui_state.direct_wasapi_enabled && (flags & kDirectWasapiExclusive) != 0U;
  if (ui_state.direct_wasapi_enabled) {
    ui_state.direct_wasapi_device_id.assign(device_id.data(), device_id.size());
  }
  ui_state.recording_mode = common::NormalizeRecordingMode(
      static_cast<common::RecordingMode>(recording_mode));
  if (!recording_path.empty()) {
    ui_state.recording_path = PathFromUtf8(recording_path);
  }
  ui_state.application_input_enabled =
      version >= kOlderVersion && (flags & kApplicationInputEnabled) != 0U &&
      !application_identity.empty();
  ui_state.additional_input_enabled =
      (version >= kPreviousVersion &&
       (flags & kAdditionalInputEnabled) != 0U &&
       !additional_application_identity.empty()) ||
      (version == kOlderVersion &&
       (flags & kAdditionalInputEnabled) != 0U &&
       !application_identity.empty());
  if (!application_identity.empty()) {
    ui_state.application_input_identity.assign(application_identity.data(),
                                               application_identity.size());
  }
  if (!additional_application_identity.empty()) {
    ui_state.additional_application_input_identity.assign(
        additional_application_identity.data(),
        additional_application_identity.size());
  } else if (version == kOlderVersion &&
             ui_state.additional_input_enabled) {
    // Version 2 used one identity for both roles. The old UI could only use
    // one role at a time, so migrate that identity to the recording role when
    // the additional-input flag was set.
    ui_state.additional_application_input_identity =
        ui_state.application_input_identity;
    ui_state.application_input_enabled = false;
  }
  ui_state.voice_delay_ms = voice_delay_ms;
  ui_state.additional_input_gain_db = std::clamp(
      additional_input_gain_db, common::kMinAdditionalInputGainDb,
      common::kMaxAdditionalInputGainDb);
  if (version >= kAudioFileVersion) {
    ui_state.input_source = static_cast<common::InputSource>(input_source);
    ui_state.additional_input_source =
        static_cast<common::InputSource>(additional_input_source);
    if (!input_file_path.empty()) {
      ui_state.input_file_path = PathFromUtf8(input_file_path);
    }
    if (!additional_input_file_path.empty()) {
      ui_state.additional_input_file_path =
          PathFromUtf8(additional_input_file_path);
    }
    ui_state.input_file_playing = (input_file_flags & 1U) != 0U;
    ui_state.input_file_loop = (input_file_flags & 2U) != 0U;
    ui_state.additional_file_playing = (additional_file_flags & 1U) != 0U;
    ui_state.additional_file_loop = (additional_file_flags & 2U) != 0U;
    ui_state.input_file_volume = input_file_volume;
    ui_state.additional_file_volume = additional_file_volume;
  } else {
    ui_state.input_source = ui_state.application_input_enabled
                                ? common::InputSource::kApplicationInput
                                : common::InputSource::kDawInput;
    ui_state.additional_input_source = ui_state.additional_input_enabled
                                           ? common::InputSource::kApplicationInput
                                           : common::InputSource::kOff;
  }
  // Keep the old wire fields readable, but never expose the removed Audio
  // Files mode to the running editor or processor.
  if (ui_state.input_source == common::InputSource::kAudioFile) {
    ui_state.input_source = common::InputSource::kDawInput;
    ui_state.input_file_path.clear();
    ui_state.input_file_playing = false;
    ui_state.input_file_loop = false;
  }
  if (ui_state.additional_input_source == common::InputSource::kAudioFile) {
    ui_state.additional_input_source = common::InputSource::kOff;
    ui_state.additional_input_file_path.clear();
    ui_state.additional_file_playing = false;
    ui_state.additional_file_loop = false;
  }
  return true;
}

}  // namespace beatrice::vst

#endif  // BEATRICE_VST_PLUGIN_STATE_H_
