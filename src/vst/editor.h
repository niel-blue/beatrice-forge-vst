// Copyright (c) 2024-2026 Project Beatrice and Contributors

#ifndef BEATRICE_VST_EDITOR_H_
#define BEATRICE_VST_EDITOR_H_

#include <map>
#include <memory>
#include <optional>
#include <filesystem>
#include <string>
#include <vector>

#include "vst3sdk/pluginterfaces/base/fplatform.h"
#include "vst3sdk/pluginterfaces/vst/vsttypes.h"
#include "vst3sdk/public.sdk/source/vst/vstguieditor.h"
#include "vst3sdk/vstgui4/vstgui/lib/cview.h"
#include "vst3sdk/vstgui4/vstgui/lib/cviewcontainer.h"
#include "vst3sdk/vstgui4/vstgui/lib/cvstguitimer.h"
#include "vst3sdk/vstgui4/vstgui/lib/vstguifwd.h"

// Beatrice
#include "common/model_config.h"
#include "common/audio_recorder.h"
#include "common/recording_paths.h"
#include "common/preset.h"
#include "common/simple_morph.h"
#include "common/voice_morph_state.h"
#include "vst/controls.h"
#include "vst/editor_layout.h"
#include "vst/editor_theme.h"
#include "vst/editor_typography.h"

namespace beatrice::vst {

static constexpr auto kWindowWidth = static_cast<int>(layout::kWindowWidth);
static constexpr auto kWindowHeight = static_cast<int>(layout::kWindowHeight);

class DescriptionPane;
class DescriptionPopupView;
enum class DescriptionTarget;
class MorphFalloffSlider;
class MorphPadController;
class MorphPadView;
class SimpleMorphPanel;
class PresetPanel;
class PortraitPopupView;
class VoiceMenuOverlayView;
class VoiceSelectorView;
class SurfacePanel;
class VerticalScrollView;
class GlowingActionLabel;
class LevelIndicator;

// NOLINTNEXTLINE(misc-multiple-inheritance)
class Editor : public Steinberg::Vst::VSTGUIEditor, public IControlListener {
  using ParamID = Steinberg::Vst::ParamID;
  using ParamValue = Steinberg::Vst::ParamValue;
  using PlatformType = VSTGUI::PlatformType;
  using CView = VSTGUI::CView;
  using CViewContainer = VSTGUI::CViewContainer;

 public:
  explicit Editor(void* controller);
  ~Editor() SMTG_OVERRIDE;
  auto PLUGIN_API open(void* parent, const PlatformType& platformType)
      -> bool SMTG_OVERRIDE;
  auto CreateStandaloneFrame(VSTGUI::CCoord width)
      -> VSTGUI::SharedPointer<VSTGUI::CFrame>;
  [[nodiscard]] auto GetStandaloneInOutPage() const -> SurfacePanel* {
    return standalone_inout_panel_;
  }
  void PLUGIN_API close() SMTG_OVERRIDE;
  void beginEdit(Steinberg::int32 index) SMTG_OVERRIDE;
  void endEdit(Steinberg::int32 index) SMTG_OVERRIDE;
  void SyncValue(ParamID param_id, float plain_value);
  void SyncStringValue(ParamID param_id, const std::u8string& value);
  void SetAudioLevels(float input_peak, float output_peak);
  void SyncVstExternalState();
  void valueChanged(CControl* pControl) SMTG_OVERRIDE;
  // auto notify(CBaseObject* sender,
  //                       const char* message) -> CMessageResult SMTG_OVERRIDE;

 private:
  auto BuildFrame(void* parent, bool attach_to_platform,
                  VSTGUI::CCoord width) -> bool;
  enum class FocusColumn { kNone, kSettings, kVoice, kPresets };
  static constexpr auto kPortraitWidth = static_cast<int>(layout::kPortraitSize);
  static constexpr auto kPortraitHeight = kPortraitWidth;
  void SyncSourcePitchRange();
  void UpdateSourcePitchVisibility();
  void UpdateCompensatedDriveUi();
  void UpdateBypassUi(bool bypassed);
  void SyncModelDescription();
  // Rebuild the model-dependent controls from the controller state after a
  // preset has finished applying.  Some controls legitimately skip their
  // listener when the requested value already matches the stored value, but
  // the model-dependent presentation still needs to be refreshed.
  void RefreshModelUiFromControllerState();
  void SyncParameterAvailability();
  void SetPortraitDescriptionText(const std::u8string& text);
  void SetModelDescriptionText(const std::u8string& text);
  void SetVoiceDescriptionText(const std::u8string& text);
  void SetPortraitDescriptionMode(bool morphing);
  void ShowPortraitPopup();
  void HidePortraitPopup();
  void SetVoiceSelectorDisplay(int voice_id);
  void ToggleVoiceMenu();
  void HideVoiceMenu();
  void RebuildVoiceMenu();
  void SelectVoice(int voice_id);
  [[nodiscard]] auto GetVoiceControl() const -> VSTGUI::COptionMenu*;
  void ShowDescriptionPopup(DescriptionTarget target, const char* title,
                            const std::u8string& text, CRect target_rect);
  void HideDescriptionPopup();
  void UpdateVoiceMorphingDescription();
  void ApplyVoiceMorphState(const common::VoiceMorphState& state);
  void ApplyMorphViewMode(bool simple);
  void SetSimpleMorphMode(bool simple);
  void RestoreMorphViewModeFromCurrentPreset();
  void UpdateMorphUiVisibility(bool morphing);
  void ResetMorph(bool simple);
  void ApplySimpleMorphWeights(const common::SimpleMorphWeights& weights,
                               int voice_count, bool save_now = true);
  void AddCurrentPreset();
  // reset_state is false only for the first blank preset created when a new
  // editor has no persisted workspace.  That seed row must not clear a model
  // which the host has already supplied.
  void CreateNewPreset(bool reset_state = true);
  [[nodiscard]] auto CurrentModelVoicePresetName() -> std::string;
  auto RenameSelectedPresetFromCurrentModelVoice() -> bool;
  void ApplyPreset(int index);
  void RenamePreset(int index, const std::string& name);
  void MovePreset(int index, int destination);
  void DeletePreset(int index);
  void RefreshPresetPanel(int selected = -1);
  void SelectRightPanel(int page);
  void SavePresets();
  void ImportPresets(int mode);
  void ExportPresets(bool all_banks);
  void AddPresetBank();
  void SelectPresetBank(int index);
  void RenamePresetBank(int index, const std::string& name);
  void MovePresetBank(int index, int destination);
  void DeletePresetBank(int index);
  void SyncCurrentPresetBank();
  void LoadCurrentPresetBank();
  void UpdateSelectedPresetFromCurrentState(ParamID changed_parameter,
                                            bool save_now);
  void ScheduleDeferredPresetSave();
  void FlushDeferredPresetSave();
  void PerformParameterEdit(ParamID param_id, ParamValue normalized_value);
  void SendParameterEdit(ParamID param_id, ParamValue normalized_value);
  void SetFocusedColumn(FocusColumn column);
  void FocusColumnRoot(FocusColumn column);
  void RegisterFocusColumn(FocusColumn role, SurfacePanel* panel);
  [[nodiscard]] auto ColumnForView(const CView* view) const -> FocusColumn;
  // Reserved for a future Options/help switch. This is a UI preference, not
  // a VST parameter or preset value.
  void SetControlHelpEnabled(bool enabled);
  void PollAudioLevels();
  void PollVstRecordingStatus();
  void ChooseVstRecordingPath();
  void ToggleVstRecording();
  void StartVstRecording();
  void StopVstRecording();
  void UpdateVstRecordingControls();
  void SendVstRecordingStart();
  void SendVstRecordingSelection();
  void SendVstRecordingStop();
  void RefreshVstWasapiDevices();
  void SendVstDirectWasapiSelection();
  void SendVstDirectWasapiOff();
  void UpdateVstDirectWasapiControls();

  std::map<ParamID, CControl*> controls_;
  CFontRef font_, font_bold_, font_description_, font_small_, font_small_bold_,
      font_tab_, font_tab_bold_;
  CFontRef font_heading_, font_strong_;
  bool control_help_enabled_ = true;
  bool japanese_tooltips_ = false;
  bool standalone_frame_ = false;
  std::optional<common::ModelConfig> model_config_;
  CViewContainer* source_pitch_panel_ = nullptr;

  // Portrait / morph
  CView* portrait_view_ = nullptr;
  CViewContainer* portrait_panel_ = nullptr;
  PortraitPopupView* portrait_popup_view_ = nullptr;
  CView* unloaded_logo_view_ = nullptr;
  std::unique_ptr<MorphPadController> morph_pad_controller_;
  MorphPadView* morph_pad_view_ = nullptr;
  SimpleMorphPanel* simple_morph_panel_ = nullptr;
  CViewContainer* morph_mode_switch_ = nullptr;
  CTextLabel* simple_morph_tab_ = nullptr;
  CTextLabel* advanced_morph_tab_ = nullptr;
  // Entering Voice Morphing Mode starts on the 2D PAD.  A preset can still
  // explicitly restore SLIDER mode when it contains that choice.
  bool simple_morph_mode_ = false;
  bool morphing_active_ = false;

  // Presets shared with the future standalone client.
  std::vector<common::Preset> presets_;
  int selected_preset_ = -1;
  // The preset whose settings are currently active.  This is kept separate
  // from the bank currently being viewed so changing banks does not make a
  // remembered row appear selected in the new bank.
  int active_preset_bank_ = -1;
  common::PresetWorkspace preset_workspace_;
  bool applying_preset_ = false;
  bool preset_save_pending_ = false;
  VSTGUI::SharedPointer<VSTGUI::CVSTGUITimer> preset_save_timer_;
  VSTGUI::SharedPointer<VSTGUI::CVSTGUITimer> audio_level_timer_;
  bool rename_selected_preset_after_model_load_ = false;
  PresetPanel* preset_panel_ = nullptr;
  VerticalScrollView* effects_scroll_ = nullptr;
  SurfacePanel* effects_panel_ = nullptr;
  VerticalScrollView* standalone_inout_scroll_ = nullptr;
  SurfacePanel* standalone_inout_panel_ = nullptr;
  VerticalScrollView* vst_inout_scroll_ = nullptr;
  SurfacePanel* vst_inout_panel_ = nullptr;
  VSTGUI::COptionMenu* vst_output_device_menu_ = nullptr;
  VSTGUI::CCheckBox* vst_exclusive_checkbox_ = nullptr;
  LevelIndicator* vst_output_level_ = nullptr;
  std::vector<std::string> vst_output_device_ids_;
  VSTGUI::COptionMenu* vst_recording_mode_menu_ = nullptr;
  GlowingActionLabel* vst_record_button_ = nullptr;
  GlowingActionLabel* vst_record_path_button_ = nullptr;
  VSTGUI::CTextLabel* vst_recording_status_label_ = nullptr;
  common::RecordingMode vst_recording_mode_ = common::RecordingMode::kOff;
  std::filesystem::path vst_recording_path_;
  GlowingActionLabel* presets_tab_ = nullptr;
  GlowingActionLabel* effects_tab_ = nullptr;
  GlowingActionLabel* standalone_inout_tab_ = nullptr;
  GlowingActionLabel* vst_inout_tab_ = nullptr;
  LevelIndicator* input_level_ = nullptr;
  LevelIndicator* output_level_ = nullptr;
  DescriptionPane* portrait_description_pane_ = nullptr;
  MorphFalloffSlider* morph_falloff_slider_ = nullptr;
  CTextLabel* morph_reset_button_ = nullptr;

  // Model / Voice Description
  DescriptionPane* model_description_pane_ = nullptr;
  DescriptionPane* voice_description_pane_ = nullptr;

  // Voice 選択
  VoiceSelectorView* voice_selector_ = nullptr;
  VoiceMenuOverlayView* voice_menu_overlay_ = nullptr;

  // Description popup
  DescriptionPopupView* description_popup_ = nullptr;

  // Header
  CTextLabel* conversion_status_label_ = nullptr;
  GlowingActionLabel* bypass_button_ = nullptr;
  CTextLabel* model_name_label_ = nullptr;
  struct FocusColumnEntry {
    FocusColumn role;
    SurfacePanel* panel;
  };
  // Roles are independent from display order, allowing columns to be moved or
  // a fourth one to be added without changing focus ownership.
  std::vector<FocusColumnEntry> focus_columns_;
  FocusColumn focused_column_ = FocusColumn::kNone;

  // Portrait bitmap cache
  std::map<std::u8string, SharedPointer<CBitmap>> portraits_;
  std::map<std::u8string, SharedPointer<CBitmap>> portrait_popup_bitmaps_;
  std::map<std::u8string, SharedPointer<CBitmap>> portrait_menu_thumbnails_;
  std::map<std::u8string, SharedPointer<CBitmap>> portrait_marker_thumbnails_;

  // Morphing parameters
  common::VoiceMorphState voice_morph_state_;
};

}  // namespace beatrice::vst

#endif  // BEATRICE_VST_EDITOR_H_
