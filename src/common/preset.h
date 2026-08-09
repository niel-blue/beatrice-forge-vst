// Copyright (c) 2024-2026 Project Beatrice and Contributors

#ifndef BEATRICE_COMMON_PRESET_H_
#define BEATRICE_COMMON_PRESET_H_

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

#include "common/voice_morph_state.h"

namespace beatrice::common {

struct Preset {
  std::uint64_t id = 0;
  std::string name;
  std::u8string model_path;
  int voice = 0;
  double input_gain = 0.0;
  double output_gain = 0.0;
  double compensated_drive = 0.0;
  double de_mud = 0.0;
  double presence = 0.0;
  double reverb_mix = 0.0;
  double reverb_decay = 1.2;
  double reverb_tone = 50.0;
  double pitch_shift = 0.0;
  double formant_shift = 0.0;
  int vq_neighbor_count = 0;
  double average_source_pitch = 0.0;
  double min_source_pitch = 0.0;
  double max_source_pitch = 0.0;
  double intonation_intensity = 0.0;
  double pitch_correction = 0.0;
  int pitch_control = 1;
  int pitch_correction_type = 0;
  std::u8string simple_morph_weights = u8"1";
  VoiceMorphState advanced_morph_state;
  bool simple_morph_mode = true;
};

struct PresetBank {
  std::uint64_t id = 0;
  std::string name = "Default";
  std::vector<Preset> presets;
  int selected_preset = -1;
};

struct PresetWorkspace {
  std::vector<PresetBank> banks;
  int selected_bank = 0;
};

struct PresetImportReport {
  int loaded = 0;
  int skipped = 0;
};

[[nodiscard]] auto SerializePresetWorkspace(const PresetWorkspace& workspace)
    -> std::string;
[[nodiscard]] auto DeserializePresetWorkspace(
    const std::string& serialized, PresetWorkspace& workspace,
    std::string* error = nullptr, PresetImportReport* report = nullptr) -> bool;

[[nodiscard]] auto SerializePresets(const std::vector<Preset>& presets,
                                    int selected = -1)
    -> std::string;
[[nodiscard]] auto DeserializePresets(const std::string& serialized,
                                      std::vector<Preset>& presets,
                                      std::string* error = nullptr,
                                      int* selected = nullptr) -> bool;

class PresetStore {
 public:
  explicit PresetStore(std::filesystem::path path) : path_(std::move(path)) {}

  [[nodiscard]] auto Load(std::vector<Preset>& presets,
                          std::string* error = nullptr) const -> bool;
  [[nodiscard]] auto Save(const std::vector<Preset>& presets,
                          std::string* error = nullptr) const -> bool;
  [[nodiscard]] auto GetPath() const -> const std::filesystem::path& {
    return path_;
  }

 private:
  std::filesystem::path path_;
};

}  // namespace beatrice::common

#endif  // BEATRICE_COMMON_PRESET_H_
