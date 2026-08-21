// Copyright (c) 2024-2026 Project Beatrice and Contributors

#include "common/preset.h"

#include <algorithm>
#include <fstream>
#include <sstream>
#include <system_error>

#include "toml11/single_include/toml.hpp"

namespace beatrice::common {
namespace {

auto ToString(const std::u8string& value) -> std::string {
  return {reinterpret_cast<const char*>(value.data()), value.size()};
}

auto ToU8String(const std::string& value) -> std::u8string {
  return {value.begin(), value.end()};
}

auto MakePresetDocument(const std::vector<Preset>& presets,
                        const int selected = -1) -> toml::value {
  auto entries = toml::array{};
  entries.reserve(presets.size());
  for (const auto& preset : presets) {
    auto entry = toml::table{
        {"id", static_cast<std::int64_t>(preset.id)},
        {"name", preset.name},
        {"model_path", ToString(preset.model_path)},
        {"voice", preset.voice},
        {"input_gain", preset.input_gain},
        {"output_gain", preset.output_gain},
        {"compensated_drive", preset.compensated_drive},
        {"de_mud", preset.de_mud},
        {"presence", preset.presence},
        {"reverb_mix", preset.reverb_mix},
        {"reverb_decay", preset.reverb_decay},
        {"reverb_tone", preset.reverb_tone},
        {"pitch_shift", preset.pitch_shift},
        {"formant_shift", preset.formant_shift},
        {"vq_neighbor_count", preset.vq_neighbor_count},
        {"average_source_pitch", preset.average_source_pitch},
        {"min_source_pitch", preset.min_source_pitch},
        {"max_source_pitch", preset.max_source_pitch},
        {"intonation_intensity", preset.intonation_intensity},
        {"pitch_correction", preset.pitch_correction},
        {"pitch_control", preset.pitch_control},
        {"pitch_correction_type", preset.pitch_correction_type},
        {"simple_morph_weights", ToString(preset.simple_morph_weights)},
        {"simple_morph_mode", preset.simple_morph_mode},
        {"morph_cursor_x", preset.advanced_morph_state.cursor_x},
        {"morph_cursor_y", preset.advanced_morph_state.cursor_y},
        {"morph_falloff", preset.advanced_morph_state.falloff},
        {"morph_marker_count", preset.advanced_morph_state.marker_count},
    };
    for (auto i = 0; i < kMaxNVoiceMorphMarkers; ++i) {
      const auto prefix = "morph_marker_" + std::to_string(i) + "_";
      const auto& marker = preset.advanced_morph_state.markers[i];
      entry[prefix + "voice"] = marker.voice_id;
      entry[prefix + "x"] = marker.x;
      entry[prefix + "y"] = marker.y;
    }
    entries.emplace_back(std::move(entry));
  }
  return toml::value(toml::table{{"version", 2},
                                 {"selected", selected},
                                 {"preset", std::move(entries)}});
}

void ReadPresetDocument(const toml::value& root, std::vector<Preset>& presets,
                        PresetImportReport* const report = nullptr) {
  const auto& entries = toml::find<toml::array>(root, "preset");
  presets.clear();
  presets.reserve(entries.size());
  for (const auto& entry : entries) {
    try {
      const auto default_id = static_cast<std::int64_t>(presets.size() + 1);
      const auto default_name = "Preset " + std::to_string(presets.size() + 1);
      auto preset = Preset{
          .id = static_cast<std::uint64_t>(
              toml::find_or<std::int64_t>(entry, "id", default_id)),
          .name = toml::find_or<std::string>(entry, "name", default_name),
          .model_path = ToU8String(
              toml::find_or<std::string>(entry, "model_path", "")),
          .voice = toml::find_or<int>(entry, "voice", 0),
          .input_gain = toml::find_or<double>(entry, "input_gain", 0.0),
          .output_gain = toml::find_or<double>(entry, "output_gain", 0.0),
          .compensated_drive =
              toml::find_or<double>(entry, "compensated_drive", 0.0),
          .de_mud = toml::find_or<double>(
              entry, "de_mud",
              toml::find_or<double>(entry, "clarity", 0.0)),
          .presence = toml::find_or<double>(entry, "presence", 0.0),
          .reverb_mix = toml::find_or<double>(entry, "reverb_mix", 0.0),
          .reverb_decay =
              toml::find_or<double>(entry, "reverb_decay", 1.2),
          .reverb_tone =
              toml::find_or<double>(entry, "reverb_tone", 50.0),
          .pitch_shift = toml::find_or<double>(entry, "pitch_shift", 0.0),
          .formant_shift =
              toml::find_or<double>(entry, "formant_shift", 0.0),
          .vq_neighbor_count =
              toml::find_or<int>(entry, "vq_neighbor_count", 0),
          .average_source_pitch =
              toml::find_or<double>(entry, "average_source_pitch", 52.0),
          .min_source_pitch =
              toml::find_or<double>(entry, "min_source_pitch", 33.125),
          .max_source_pitch =
              toml::find_or<double>(entry, "max_source_pitch", 80.875),
          .intonation_intensity =
              toml::find_or<double>(entry, "intonation_intensity", 1.0),
          .pitch_correction =
              toml::find_or<double>(entry, "pitch_correction", 0.0),
          .pitch_control = toml::find_or<int>(entry, "pitch_control", 1),
          .pitch_correction_type =
              toml::find_or<int>(entry, "pitch_correction_type", 0),
          .simple_morph_weights = ToU8String(toml::find_or<std::string>(
              entry, "simple_morph_weights", "1")),
          .simple_morph_mode =
              toml::find_or<bool>(entry, "simple_morph_mode", false),
      };
      auto& morph = preset.advanced_morph_state;
      morph.cursor_x = toml::find_or<float>(entry, "morph_cursor_x", morph.cursor_x);
      morph.cursor_y = toml::find_or<float>(entry, "morph_cursor_y", morph.cursor_y);
      morph.falloff = toml::find_or<float>(entry, "morph_falloff", morph.falloff);
      morph.marker_count = std::clamp(toml::find_or<int>(
          entry, "morph_marker_count", morph.marker_count), 1,
          kMaxNVoiceMorphMarkers);
      for (auto i = 0; i < kMaxNVoiceMorphMarkers; ++i) {
        const auto prefix = "morph_marker_" + std::to_string(i) + "_";
        auto& marker = morph.markers[i];
        marker.voice_id = toml::find_or<int>(entry, prefix + "voice", marker.voice_id);
        marker.x = std::clamp(toml::find_or<float>(entry, prefix + "x", marker.x), 0.0f, 1.0f);
        marker.y = std::clamp(toml::find_or<float>(entry, prefix + "y", marker.y), 0.0f, 1.0f);
      }
      presets.push_back(std::move(preset));
      if (report) {
        ++report->loaded;
      }
    } catch (const std::exception&) {
      if (report) {
        ++report->skipped;
      }
    }
  }
}

auto MakeBankDocument(const PresetBank& bank) -> toml::value {
  auto document = MakePresetDocument(bank.presets, bank.selected_preset);
  document["id"] = static_cast<std::int64_t>(bank.id);
  document["name"] = bank.name;
  return document;
}

auto ReadBankDocument(const toml::value& document,
                      PresetImportReport* const report = nullptr) -> PresetBank {
  auto bank = PresetBank{
      .id = static_cast<std::uint64_t>(
          toml::find_or<std::int64_t>(document, "id", 0)),
      .name = toml::find_or<std::string>(document, "name", "Default"),
      .selected_preset = toml::find_or<int>(document, "selected", -1),
  };
  ReadPresetDocument(document, bank.presets, report);
  return bank;
}

}  // namespace

auto SerializePresetWorkspace(const PresetWorkspace& workspace)
    -> std::string {
  auto banks = toml::array{};
  banks.reserve(workspace.banks.size());
  for (const auto& bank : workspace.banks) {
    banks.emplace_back(MakeBankDocument(bank));
  }
  return toml::format(toml::value(toml::table{
      {"version", 2},
      {"selected_bank", workspace.selected_bank},
      {"bank", std::move(banks)},
  }));
}

auto DeserializePresetWorkspace(const std::string& serialized,
                                PresetWorkspace& workspace,
                                std::string* const error,
                                PresetImportReport* const report) -> bool {
  workspace = {};
  if (report) {
    *report = {};
  }
  if (serialized.empty()) {
    workspace.banks.push_back(PresetBank{});
    return true;
  }
  try {
    auto stream = std::istringstream(serialized);
    const auto root = toml::parse(stream, "Beatrice preset workspace");
    if (root.contains("bank")) {
      const auto& banks = toml::find<toml::array>(root, "bank");
      workspace.banks.reserve(banks.size());
      for (const auto& bank : banks) {
        try {
          workspace.banks.push_back(ReadBankDocument(bank, report));
        } catch (const std::exception&) {
          if (report) {
            ++report->skipped;
          }
        }
      }
      workspace.selected_bank =
          toml::find_or<int>(root, "selected_bank", 0);
    } else {
      auto legacy = ReadBankDocument(root, report);
      legacy.name = "Default";
      workspace.banks.push_back(std::move(legacy));
      workspace.selected_bank = 0;
    }
    if (workspace.banks.empty()) {
      workspace.banks.push_back(PresetBank{});
    }
    workspace.selected_bank =
        std::clamp(workspace.selected_bank, 0,
                   static_cast<int>(workspace.banks.size()) - 1);
    return true;
  } catch (const std::exception& exception) {
    if (error) {
      *error = exception.what();
    }
    return false;
  }
}

auto SerializePresets(const std::vector<Preset>& presets, const int selected)
    -> std::string {
  return toml::format(MakePresetDocument(presets, selected));
}

auto DeserializePresets(const std::string& serialized,
                        std::vector<Preset>& presets,
                        std::string* const error, int* const selected) -> bool {
  if (serialized.empty()) {
    presets.clear();
    return true;
  }
  try {
    auto stream = std::istringstream(serialized);
    const auto root = toml::parse(stream, "Beatrice preset state");
    ReadPresetDocument(root, presets);
    if (selected) {
      *selected = toml::find_or<int>(root, "selected", -1);
    }
    return true;
  } catch (const std::exception& exception) {
    if (error) {
      *error = exception.what();
    }
    return false;
  }
}

auto PresetStore::Load(std::vector<Preset>& presets,
                       std::string* const error) const -> bool {
  presets.clear();
  if (!std::filesystem::exists(path_)) {
    return true;
  }
  try {
    ReadPresetDocument(toml::parse(path_), presets);
    return true;
  } catch (const std::exception& exception) {
    if (error) {
      *error = exception.what();
    }
    return false;
  }
}

auto PresetStore::Save(const std::vector<Preset>& presets,
                       std::string* const error) const -> bool {
  try {
    auto root = MakePresetDocument(presets);

    std::filesystem::create_directories(path_.parent_path());
    auto temporary = path_;
    temporary += ".tmp";
    {
      auto stream = std::ofstream(temporary, std::ios::binary);
      if (!stream.is_open()) {
        throw std::runtime_error("could not open preset file for writing");
      }
      stream << toml::format(root);
      stream.flush();
      if (!stream.good()) {
        throw std::runtime_error("could not write preset file");
      }
    }
    auto filesystem_error = std::error_code{};
    std::filesystem::remove(path_, filesystem_error);
    filesystem_error.clear();
    std::filesystem::rename(temporary, path_, filesystem_error);
    if (filesystem_error) {
      throw std::filesystem::filesystem_error(
          "could not replace preset file", temporary, path_, filesystem_error);
    }
    return true;
  } catch (const std::exception& exception) {
    if (error) {
      *error = exception.what();
    }
    return false;
  }
}

}  // namespace beatrice::common
