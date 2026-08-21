// Copyright (c) 2026 Project Beatrice and Contributors

#ifndef BEATRICE_VST_PLUGIN_STATE_H_
#define BEATRICE_VST_PLUGIN_STATE_H_

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <limits>
#include <optional>
#include <string>
#include <string_view>

#include "common/audio_recorder.h"

namespace beatrice::vst {

// Settings which belong to the VST instance, but are not audio parameters.
// These values are serialized into the host-owned VST state stream.
struct PersistedPluginUiState {
  bool direct_wasapi_enabled = false;
  bool direct_wasapi_exclusive = false;
  std::string direct_wasapi_device_id;
  common::RecordingMode recording_mode = common::RecordingMode::kOff;
  std::filesystem::path recording_path;
};

namespace plugin_state_detail {

inline constexpr std::array<char, 16> kMagic = {
    'B', 'E', 'A', 'T', 'R', 'I', 'C', 'E',
    'V', 'S', 'T', 'S', 'T', 'A', 'T', 'E'};
inline constexpr std::uint32_t kVersion = 1;
inline constexpr std::uint32_t kDirectWasapiEnabled = 1U << 0U;
inline constexpr std::uint32_t kDirectWasapiExclusive = 1U << 1U;
inline constexpr std::size_t kHeaderSize = kMagic.size() + 6U * sizeof(std::uint32_t);
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
  const auto recording_path = PathToUtf8(ui_state.recording_path);
  const auto mode = static_cast<std::uint32_t>(ui_state.recording_mode);
  if (mode > static_cast<std::uint32_t>(common::RecordingMode::kStereoInputOutput) ||
      parameter_state.size() > kMaxParameterStateSize ||
      device_id.size() > kMaxTextSize || recording_path.size() > kMaxTextSize ||
      parameter_state.size() > std::numeric_limits<std::uint32_t>::max() ||
      device_id.size() > std::numeric_limits<std::uint32_t>::max() ||
      recording_path.size() > std::numeric_limits<std::uint32_t>::max()) {
    return std::nullopt;
  }

  auto output = std::string{};
  output.reserve(kHeaderSize + parameter_state.size() + device_id.size() +
                 recording_path.size());
  output.append(kMagic.data(), kMagic.size());
  AppendU32(output, kVersion);
  AppendU32(output, static_cast<std::uint32_t>(parameter_state.size()));
  AppendU32(output, static_cast<std::uint32_t>(device_id.size()));
  AppendU32(output, static_cast<std::uint32_t>(recording_path.size()));
  auto flags = std::uint32_t{0};
  if (ui_state.direct_wasapi_enabled && !device_id.empty()) {
    flags |= kDirectWasapiEnabled;
  }
  if (ui_state.direct_wasapi_exclusive && (flags & kDirectWasapiEnabled) != 0U) {
    flags |= kDirectWasapiExclusive;
  }
  AppendU32(output, flags);
  AppendU32(output, mode);
  output.append(parameter_state.data(), parameter_state.size());
  output.append(device_id.data(), device_id.size());
  output.append(recording_path.data(), recording_path.size());
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
  if (input.size() < kHeaderSize) {
    return false;
  }

  auto offset = kMagic.size();
  std::uint32_t version = 0;
  std::uint32_t parameter_size = 0;
  std::uint32_t device_size = 0;
  std::uint32_t recording_path_size = 0;
  std::uint32_t flags = 0;
  std::uint32_t recording_mode = 0;
  if (!ReadU32(input, offset, version) ||
      !ReadU32(input, offset, parameter_size) ||
      !ReadU32(input, offset, device_size) ||
      !ReadU32(input, offset, recording_path_size) ||
      !ReadU32(input, offset, flags) ||
      !ReadU32(input, offset, recording_mode) || version != kVersion ||
      parameter_size > kMaxParameterStateSize || device_size > kMaxTextSize ||
      recording_path_size > kMaxTextSize || recording_mode > 3U) {
    return false;
  }
  const auto remaining = input.size() - offset;
  const auto payload_size = static_cast<std::uint64_t>(parameter_size) +
                            static_cast<std::uint64_t>(device_size) +
                            static_cast<std::uint64_t>(recording_path_size);
  if (payload_size != remaining) {
    return false;
  }

  parameter_state.assign(input.data() + offset, parameter_size);
  offset += parameter_size;
  const auto device_id = std::string_view(input.data() + offset, device_size);
  offset += device_size;
  const auto recording_path =
      std::string_view(input.data() + offset, recording_path_size);

  ui_state.direct_wasapi_enabled =
      (flags & kDirectWasapiEnabled) != 0U && !device_id.empty();
  ui_state.direct_wasapi_exclusive =
      ui_state.direct_wasapi_enabled && (flags & kDirectWasapiExclusive) != 0U;
  if (ui_state.direct_wasapi_enabled) {
    ui_state.direct_wasapi_device_id.assign(device_id.data(), device_id.size());
  }
  ui_state.recording_mode =
      static_cast<common::RecordingMode>(recording_mode);
  if (!recording_path.empty()) {
    ui_state.recording_path = PathFromUtf8(recording_path);
  }
  return true;
}

}  // namespace beatrice::vst

#endif  // BEATRICE_VST_PLUGIN_STATE_H_
