// Copyright (c) 2024-2026 Project Beatrice and Contributors

#ifndef BEATRICE_VST_EDITOR_LAYOUT_H_
#define BEATRICE_VST_EDITOR_LAYOUT_H_

#include <algorithm>
#include <array>
#include <cmath>
#include <iterator>

#include "vst3sdk/vstgui4/vstgui/lib/crect.h"

namespace beatrice::vst::layout {

// Window and semantic column grid.
inline constexpr auto kWindowWidth = 960.0;
inline constexpr auto kWindowHeight = 720.0;
inline constexpr auto kHeaderHeight = 58.0;
inline constexpr auto kMainContentTop = kHeaderHeight;
inline constexpr auto kColumnOuterMargin = 8.0;
inline constexpr auto kColumnTop = 4.0;
inline constexpr auto kColumnBottomMargin = 8.0;
inline constexpr auto kColumnGap = 10.0;
enum class ColumnRole { kSettings, kVoice, kPresets };
// Display order is deliberately separate from the roles. Reordering the
// columns therefore changes this list only; focus and child-view ownership
// continue to use semantic roles rather than numeric positions.
inline constexpr std::array kColumnOrder = {
    ColumnRole::kSettings, ColumnRole::kVoice, ColumnRole::kPresets};
inline constexpr auto kColumnCount =
    static_cast<double>(kColumnOrder.size());
inline constexpr auto kColumnWidth =
    (kWindowWidth - 2.0 * kColumnOuterMargin -
     (kColumnCount - 1.0) * kColumnGap) /
    kColumnCount;
// The column content viewport is 650 px at the fixed 960x720 editor size.
inline constexpr auto kColumnContentHeight =
    kWindowHeight - kHeaderHeight - kColumnTop - kColumnBottomMargin;

// Header geometry. These values preserve the current 960x720 appearance,
// while keeping every header child tied to the same semantic layout source.
// logo_forge.png is a combined Beatrice + Forge mark. The asset itself is
// prepared at 50% of the supplied source size and is drawn at native size
// from the header's top-left corner.
inline constexpr auto kHeaderLogoOrigin = VSTGUI::CPoint(0, 0);
// Keep the conversion label in the space between the logo and the model
// selector.  Its right edge deliberately stops before the selector starts.
inline constexpr auto kHeaderVoiceConversionRect =
    VSTGUI::CRect(160, 20, 300, 40);
inline constexpr auto kHeaderBypassRect = VSTGUI::CRect(284, 15, 312, 43);
// The header's model selector is the same width as the semantic VOICE column.
// Deriving these coordinates from the column grid keeps the two regions in
// lockstep when the window or column spacing is adjusted.
inline constexpr auto kHeaderModelLabelRect =
    VSTGUI::CRect(10, 15, 65, 35);
inline constexpr auto kHeaderVersionRect = VSTGUI::CRect(790, 20, 945, 40);
// Keep the editable part of the version label in one named setting.  The
// version number itself remains generated from CMake/Git metadata.
inline constexpr auto kHeaderVersionPrefix = "Ver. ";

// Shared component metrics.
inline constexpr auto kPanelGap = 8.0;
inline constexpr auto kPanelContentInset = 12.0;
inline constexpr auto kCompactContentInset = 10.0;
inline constexpr auto kControlRight = kColumnWidth - kPanelContentInset;
inline constexpr auto kCompactRight = kColumnWidth - kCompactContentInset;
inline constexpr auto kPanelTitleTop = 3.0;
inline constexpr auto kPanelTitleBottom = 22.0;
inline constexpr auto kResetButtonWidth = 80.0;
inline constexpr auto kResetButtonHeight = 20.0;
inline constexpr auto kSwitchHeight = 28.0;
inline constexpr auto kControlLabelHeight = 20.0;
inline constexpr auto kControlMenuHeight = 34.0;
inline constexpr auto kSplitTabWidth = kColumnWidth / 2.0;
inline constexpr auto kSplitTabHeight = 30.0;
// Keep a small, shared breathing space between every tab row and its page
// surface.  Non-PRESETS pages add their own common offset below, so the
// complete tab content (titles, RESET actions and controls) moves together
// without per-panel coordinate drift.
inline constexpr auto kTabContentGap = 3.0;
inline constexpr auto kPanelSectionGap = 4.0;
// One adjustment point for the complete standalone IN/OUT body.  The title,
// source selector, meters and file player all move together when this value
// is changed; the tab row itself remains fixed.  The title rectangle below
// compensates for the extra content-only shift so AUDIO DEVICES stays put.
// Align the standalone IN/OUT heading and its first controls with the
// compact, top-anchored titles used by the EFFECTS panels.  All controls in
// the page are translated together so the file-player and device sections
// cannot drift apart when this value is adjusted later.
inline constexpr auto kStandaloneInOutBodyOffsetY = -11.0;
inline constexpr auto kStandaloneInOutRecordingOffsetY = 5.0;
inline constexpr auto kDropdownChevronInset = 8.0;
inline constexpr auto kDropdownChevronWidth = 16.0;
inline constexpr auto kDropdownCornerRadius = 3.0;
inline constexpr auto kDropdownTextInset = 9.0;

// Shared peak-meter drawing and polling metrics. The standalone aliases below
// keep existing layout references source-compatible while the actual meter is
// now shared by both editor variants.
inline constexpr auto kLevelPeakDecay = 0.78;
inline constexpr auto kLevelClipThreshold = 0.98;
inline constexpr auto kLevelIndicatorHeight = 6.0;
inline constexpr auto kStandaloneLevelPeakDecay = kLevelPeakDecay;
inline constexpr auto kStandaloneLevelClipThreshold = kLevelClipThreshold;
inline constexpr auto kAudioLevelPollMilliseconds = 50;
inline constexpr auto kStandalonePlayerHandleRadius = 4.0;
inline constexpr auto kStandaloneAudioStatusPollMilliseconds =
    kAudioLevelPollMilliseconds;

// Recording is a shared UI block. The page-specific code supplies only the
// top anchor; all field order, heights and horizontal grouping stay identical
// in the standalone and VST IN/OUT pages.
struct RecordingBlockGeometry {
  VSTGUI::CRect title;
  VSTGUI::CRect mode_label;
  VSTGUI::CRect mode_menu;
  VSTGUI::CRect record_button;
  VSTGUI::CRect browse_button;
  VSTGUI::CRect status;
};

inline auto RecordingBlockGeometryAt(const double top)
    -> RecordingBlockGeometry {
  const auto title_height = kPanelTitleBottom - kPanelTitleTop;
  const auto title = VSTGUI::CRect(kPanelContentInset, top, kControlRight,
                                   top + title_height);
  const auto mode_label_top = title.bottom + kPanelSectionGap;
  const auto mode_label = VSTGUI::CRect(
      kPanelContentInset, mode_label_top, kControlRight,
      mode_label_top + kControlLabelHeight);
  const auto mode_menu = VSTGUI::CRect(
      kPanelContentInset, mode_label.bottom, kControlRight,
      mode_label.bottom + kControlMenuHeight);
  const auto action_top = mode_menu.bottom + kPanelGap;
  const auto action_bottom = action_top + kResetButtonHeight;
  const auto record_button = VSTGUI::CRect(
      kPanelContentInset, action_top,
      kPanelContentInset + kResetButtonWidth, action_bottom);
  const auto browse_button = VSTGUI::CRect(
      record_button.right + kPanelSectionGap, action_top,
      record_button.right + kPanelSectionGap + kResetButtonWidth,
      action_bottom);
  const auto status = VSTGUI::CRect(
      browse_button.right + kPanelSectionGap, action_top, kControlRight,
      action_bottom);
  return {title, mode_label, mode_menu, record_button, browse_button, status};
}

// Standalone IN/OUT geometry.  These are deliberately named by their UI
// role instead of being repeated CRects in the application layer.  New input
// sources, meters or recording controls should extend this block and pass
// the resulting rectangle through StandaloneInOutRect so the shared body
// offset remains the only adjustment point.
inline constexpr auto kStandaloneInOutTitleRect =
    VSTGUI::CRect(kPanelContentInset, 18.0, kControlRight, 37.0);
inline constexpr auto kStandaloneInOutInputSourceLabelRect =
    VSTGUI::CRect(kPanelContentInset, 44.0, kControlRight, 64.0);
inline constexpr auto kStandaloneInOutInputSourceMenuRect =
    VSTGUI::CRect(kPanelContentInset, 64.0, 160.0, 98.0);
inline constexpr auto kStandaloneInOutInputLabelRect =
    VSTGUI::CRect(kPanelContentInset, 108.0, kControlRight, 128.0);
inline constexpr auto kStandaloneInOutInputMenuRect =
    VSTGUI::CRect(kPanelContentInset, 128.0, kControlRight, 162.0);
inline constexpr auto kStandaloneInOutInputMeterRect =
    VSTGUI::CRect(kPanelContentInset, 168.0, kControlRight, 174.0);
inline constexpr auto kStandaloneInOutOutputLabelRect =
    VSTGUI::CRect(kPanelContentInset, 180.0, kControlRight, 200.0);
inline constexpr auto kStandaloneInOutOutputMenuRect =
    VSTGUI::CRect(kPanelContentInset, 200.0, kControlRight, 234.0);
inline constexpr auto kStandaloneInOutOutputMeterRect =
    VSTGUI::CRect(kPanelContentInset, 240.0, kControlRight, 246.0);
inline constexpr auto kStandaloneInOutMonitorLabelRect =
    VSTGUI::CRect(kPanelContentInset, 252.0, kControlRight, 272.0);
inline constexpr auto kStandaloneInOutMonitorMenuRect =
    VSTGUI::CRect(kPanelContentInset, 272.0, kControlRight, 306.0);
inline constexpr auto kStandaloneInOutMonitorMeterRect =
    VSTGUI::CRect(kPanelContentInset, 312.0, kControlRight, 318.0);
inline constexpr auto kStandaloneInOutBrowseRect =
    VSTGUI::CRect(192.0, 71.0, 272.0, 91.0);
inline constexpr auto kStandaloneInOutFilePlayerGap = 5.0;
inline constexpr auto kStandaloneInOutFileControlsRect = VSTGUI::CRect(
    0.0, 108.0, kColumnWidth, 174.0 + kStandaloneInOutFilePlayerGap);
inline constexpr auto kStandaloneInOutFileNameRect =
    VSTGUI::CRect(12.0, 0.0, kControlRight, 18.0);
inline constexpr auto kStandaloneInOutFilePlaybackRect = VSTGUI::CRect(
    0.0, 18.0 + kStandaloneInOutFilePlayerGap, kColumnWidth,
    66.0 + kStandaloneInOutFilePlayerGap);
// File-player child controls share the same local coordinate system.  Keeping
// these rectangles here means a later recording or transport control can be
// added without reintroducing one-off offsets in the standalone window code.
inline constexpr auto kStandaloneInOutPlayButtonRect =
    VSTGUI::CRect(12.0, 2.0, 32.0, 22.0);
inline constexpr auto kStandaloneInOutPauseButtonRect =
    VSTGUI::CRect(36.0, 2.0, 56.0, 22.0);
inline constexpr auto kStandaloneInOutStopButtonRect =
    VSTGUI::CRect(60.0, 2.0, 80.0, 22.0);
inline constexpr auto kStandaloneInOutLoopButtonRect =
    VSTGUI::CRect(84.0, 2.0, 104.0, 22.0);
inline constexpr auto kStandaloneInOutVolumeLabelRect =
    VSTGUI::CRect(112.0, 2.0, 142.0, 22.0);
// Both player bars share one right edge.  Keep the value/time labels in the
// remaining right-hand region so extending the bars cannot make their text
// overlap the controls.
inline constexpr auto kStandaloneInOutPlayerBarRight = 232.0;
inline constexpr auto kStandaloneInOutPlayerValueLeft =
    kStandaloneInOutPlayerBarRight + 4.0;
inline constexpr auto kStandaloneInOutVolumeViewRect =
    VSTGUI::CRect(144.0, 8.0, kStandaloneInOutPlayerBarRight, 18.0);
inline constexpr auto kStandaloneInOutVolumeValueRect =
    VSTGUI::CRect(kStandaloneInOutPlayerValueLeft - 10.0, 2.0, 286.0, 22.0);
inline constexpr auto kStandaloneInOutProgressRect =
    VSTGUI::CRect(12.0, 29.0, kStandaloneInOutPlayerBarRight, 39.0);
inline constexpr auto kStandaloneInOutTimeRect =
    VSTGUI::CRect(kStandaloneInOutPlayerValueLeft, 25.0, 296.0, 45.0);
// Reserve the status area before starting the recording section.  The status
// label itself is single-line so an error message cannot leave punctuation on
// a separate line.
inline constexpr auto kStandaloneInOutStatusRect =
    VSTGUI::CRect(kPanelContentInset, 326.0, kControlRight, 354.0);
// Standalone recording controls follow the device status. These aliases keep
// existing callers source-compatible while deriving every rectangle from the
// same block factory used by the VST IN/OUT page.
inline const auto kStandaloneInOutRecordingGeometry = RecordingBlockGeometryAt(
    366.0 + kStandaloneInOutRecordingOffsetY);
inline const auto& kStandaloneInOutRecordingTitleRect =
    kStandaloneInOutRecordingGeometry.title;
inline const auto& kStandaloneInOutRecordingModeLabelRect =
    kStandaloneInOutRecordingGeometry.mode_label;
inline const auto& kStandaloneInOutRecordingModeMenuRect =
    kStandaloneInOutRecordingGeometry.mode_menu;
inline const auto& kStandaloneInOutRecordButtonRect =
    kStandaloneInOutRecordingGeometry.record_button;
inline const auto& kStandaloneInOutRecordPathButtonRect =
    kStandaloneInOutRecordingGeometry.browse_button;
inline const auto& kStandaloneInOutRecordingStatusRect =
    kStandaloneInOutRecordingGeometry.status;
// The whole IN/OUT body fits inside the same 650px viewport as the other
// columns.  Keeping the content rect equal to the viewport is intentional:
// the recording controls must not introduce a scrollbar by themselves.
inline constexpr auto kStandaloneInOutContentHeight = kColumnContentHeight;

// Keep all scrollbar metrics together so popup and panel content rectangles
// use the same reserved gutter without depending on declaration order.
inline constexpr auto kVerticalScrollbarWidth = 6.0;
inline constexpr auto kScrollbarContentGap = 4.0;
inline constexpr auto kPopupRect = VSTGUI::CRect(280, 70, 680, 370);
inline constexpr auto kPopupInset = 15.0;
inline constexpr auto kPopupTitleTop = 15.0;
inline constexpr auto kPopupTitleBottom = 40.0;
inline constexpr auto kPopupScrollTop = 50.0;
inline constexpr auto kPopupScrollBottomInset = 15.0;
inline constexpr auto kPopupTitleRect =
    VSTGUI::CRect(kPopupInset, kPopupTitleTop, kPopupRect.getWidth() -
                                             kPopupInset, kPopupTitleBottom);
inline constexpr auto kPopupScrollRect = VSTGUI::CRect(
    kPopupInset, kPopupScrollTop, kPopupRect.getWidth() - kPopupInset,
    kPopupRect.getHeight() - kPopupScrollBottomInset);
inline constexpr auto kPopupScrollContentRect = VSTGUI::CRect(
    0, 0, kPopupScrollRect.getWidth() - kVerticalScrollbarWidth -
                kScrollbarContentGap,
    kPopupScrollRect.getHeight() - kPopupScrollTop);
inline constexpr auto kPopupTextRect = kPopupScrollContentRect;
inline constexpr auto kPortraitImageInset = 4.0;
inline constexpr auto kVoiceSelectorIconRect = VSTGUI::CRect(6, 4, 44, 42);
inline constexpr auto kVoiceSelectorNameRect = VSTGUI::CRect(52, 8, 276, 38);

// Settings column. Child controls use one repeated vertical rhythm.
// Three level controls end at y=194. Keep 12px beneath the final control so
// the GAIN panel does not look cramped against its lower edge.
inline constexpr auto kSettingsTopPanelHeight = 206.0;
// Keep the same narrow separation used by the VOICE selector and its
// following mode panel. These panels must remain visually distinct.
inline constexpr auto kSettingsBottomPanelTop =
    kSettingsTopPanelHeight + kPanelSectionGap;
inline constexpr auto kSettingsTabBodyTop = kSplitTabHeight + kTabContentGap;
// The PRESETS page owns its own compact list geometry.  All other tab pages
// use this second body origin so their complete contents (titles, RESET
// actions and controls) can be moved together without changing the tab row.
inline constexpr auto kNonPresetTabBodyOffsetY = 4.0;
inline constexpr auto kNonPresetTabBodyTop =
    kSettingsTabBodyTop + kNonPresetTabBodyOffsetY;
inline constexpr auto kSettingsSliderLeft = kPanelContentInset;
inline constexpr auto kSettingsSliderRight = kControlRight;
inline constexpr auto kSettingsSliderTop = 28.0;
inline constexpr auto kSettingsSliderHeight = 40.0;
inline constexpr auto kSettingsSliderStride = 46.0;
inline constexpr auto kControlLabelTop = 164.0;
inline constexpr auto kControlLabelBottom = 184.0;
inline constexpr auto kControlLabelRight = 180.0;
inline constexpr auto kControlMenuTop = 186.0;
inline constexpr auto kControlMenuBottom = 220.0;
inline constexpr auto kTuningControlLabelTop = 122.0;
inline constexpr auto kTuningControlLabelBottom = 142.0;
inline constexpr auto kTuningControlMenuTop = 144.0;
inline constexpr auto kTuningControlMenuBottom = 178.0;
inline constexpr auto kTuningControlLabelRight = kControlLabelRight;
inline constexpr auto kTuningLatencySectionExtraGap = 4.0;
// Match the section separation used between the EFFECTS panels.  The
// Pitch Correction menu ends at 178px; LATENCY therefore starts after the
// shared 8px panel gap instead of using a one-off 4px spacing.
inline constexpr auto kTuningLatencyTitleTop =
    kTuningControlMenuBottom + kPanelGap + kTuningLatencySectionExtraGap;
inline constexpr auto kTuningLatencyTitleBottom =
    kTuningLatencyTitleTop + (kPanelTitleBottom - kPanelTitleTop);
inline constexpr auto kTuningLatencyLabelTop =
    kTuningLatencyTitleBottom + kTabContentGap;
inline constexpr auto kTuningLatencyLabelBottom =
    kTuningLatencyLabelTop + 20.0;
inline constexpr auto kTuningLatencyMenuTop =
    kTuningLatencyLabelBottom + 2.0;
inline constexpr auto kTuningLatencyMenuBottom =
    kTuningLatencyMenuTop + 34.0;
inline constexpr auto kSourcePitchTop = 224.0;
inline constexpr auto kSourcePitchBottom = 392.0;
inline constexpr auto kSettingsTabBodyBottom = 432.0;

// Effects column. Each group has its own surface and reset action. The panel
// heights are derived from the shared slider rhythm so future controls cannot
// overlap the PRESETS/EFFECTS tabs or one another.
inline constexpr auto kEffectsClarityPanelTop = kNonPresetTabBodyTop;
inline constexpr auto kEffectsClarityPanelBottom =
    kEffectsClarityPanelTop + kSettingsSliderTop +
    2.0 * kSettingsSliderStride + 6.0;
inline constexpr auto kEffectsReverbPanelTop =
    kEffectsClarityPanelBottom + kPanelGap;
inline constexpr auto kEffectsReverbPanelBottom =
    kEffectsReverbPanelTop + kSettingsSliderTop +
    3.0 * kSettingsSliderStride + 6.0;

// Voice column.
inline constexpr auto kPortraitSize = 300.0;
inline constexpr auto kPortraitLeft = (kColumnWidth - kPortraitSize) / 2.0;
inline constexpr auto kPortraitPopupMaxSize = 480.0;
inline constexpr auto kPortraitPopupDescriptionGap = 4.0;
// Shared VOICE header geometry. The normal portrait surface begins at the
// selector's lower edge, while morph mode retains the full panel bottom gap.
inline constexpr auto kVoiceSelectorTop = kSplitTabHeight + 2.0;
inline constexpr auto kVoiceSelectorBottom = 78.0;
// Normal portrait view is one continuous voice column: selector, portrait,
// model description and voice description meet without vertical gaps.
inline constexpr auto kVoicePanelHeight = 86.0;
inline constexpr auto kPortraitNormalTop = kVoiceSelectorBottom;
// Portrait image is 300x300, centered inside a 308x308 panel surface.
inline constexpr auto kPortraitNormalBottom = kPortraitNormalTop + kColumnWidth;
inline constexpr auto kDescriptionNormalTop = kPortraitNormalBottom;
inline constexpr auto kDescriptionPaneHeight =
    (kColumnContentHeight - kDescriptionNormalTop) / 2.0;
inline constexpr auto kModelDescriptionBottom =
    kDescriptionNormalTop + kDescriptionPaneHeight;
inline constexpr auto kVoiceDescriptionTop = kModelDescriptionBottom;
inline constexpr auto kMorphSwitchTop = kVoicePanelHeight + 4.0;
inline constexpr auto kMorphResetTop = kMorphSwitchTop + kSplitTabHeight + 4.0;
inline constexpr auto kMorphPortraitTop = kMorphSwitchTop + kSplitTabHeight;
// The morph surface fills the voice column; the 300px pad is centered inside.
inline constexpr auto kMorphPortraitBottom = kMorphPortraitTop + kColumnWidth;
// Morph controls are visually connected to the pad/slider surface.
inline constexpr auto kMorphDetailsTop = kMorphPortraitBottom;
inline constexpr auto kDescriptionTitleTop = 7.0;
inline constexpr auto kDescriptionTitleBottom = 28.0;
inline constexpr auto kDescriptionBodyTop = 32.0;
// The description card is 132px tall. Give its readable, scrollable body the
// remaining space instead of leaving an oversized unused lower area.
inline constexpr auto kDescriptionBodyBottom = 124.0;
inline constexpr auto kPortraitLogoRect = VSTGUI::CRect(24, 96, 226, 154);
inline constexpr auto kMorphFalloffRect = VSTGUI::CRect(12, 38, 296, 94);
// Keep the morphing VOICE DESCRIPTION title on the same baseline as the
// normal voice description.  The falloff control ends at this boundary, so
// the description can reuse the normal pane geometry without overlap.
inline constexpr auto kMorphVoiceDescriptionTop = kVoiceDescriptionTop;
inline constexpr auto kMorphDescriptionTitleRect =
    VSTGUI::CRect(12, 8, 296, 30);
inline constexpr auto kMorphDescriptionBodyRect =
    VSTGUI::CRect(12, 36, 296, 302);
// Morph drawing metrics are kept with the rest of the layout so the pad can
// be resized without scattering pixel constants through the renderer.
inline constexpr auto kMorphMarkerRadius = 29.0;
inline constexpr auto kMorphBadgeInset = 10.0;
inline constexpr auto kMorphBadgeSize = 18.0;
inline constexpr auto kMorphCursorRadius = 14.0;
inline constexpr auto kMorphAxisHalfLength = 26.0;
inline constexpr auto kMorphWeightLabelHalfWidth = 22.0;
inline constexpr auto kMorphWeightLabelHalfHeight = 10.0;
inline constexpr auto kMorphWeightLabelRadius = 4.0;
inline constexpr auto kMorphNameLabelWidth = 120.0;
inline constexpr auto kMorphNameLabelHeight = 17.0;
inline constexpr auto kMorphNameLabelGap = 33.0;
inline constexpr auto kVoiceMenuTop = 134.0;
inline constexpr auto kVoiceMenuBottom = kWindowHeight - kColumnOuterMargin;
inline constexpr auto kVoiceMenuItemHeight = 52.0;
inline constexpr auto kVoiceMenuItemSurfaceHeight = 48.0;
inline constexpr auto kVoiceMenuMinScrollerLength = 48.0;
inline constexpr auto kVoiceMenuInset = 8.0;

// Scrollbars and presets.
inline constexpr auto kPresetTabWidth = 90.0;
inline constexpr auto kPresetRowHeight = 42.0;
inline constexpr auto kPresetRowDeleteWidth = 34.0;
inline constexpr auto kPresetTabScrollbarHeight = 6.0;
inline constexpr auto kBottomButtonWidth = 141.0;
inline constexpr auto kBottomButtonGap = 6.0;
inline constexpr auto kIconTextGap = 20.0;
inline constexpr auto kPresetSaveLeft = 104.0;
inline constexpr auto kPresetSaveRight = 200.0;
inline constexpr auto kPresetNewLeft = 204.0;
// PRESETS keeps its own compact body geometry.  Move every item in that body
// together, while leaving the PRESETS/EFFECTS/IN-OUT tab row itself fixed.
inline constexpr auto kPresetBodyOffsetY = -3.0;
inline constexpr auto kPresetTabsTop = 38.0 + kPresetBodyOffsetY;
inline constexpr auto kPresetTabActionTop = 44.0 + kPresetBodyOffsetY;
inline constexpr auto kPresetTabActionGap = 4.0;
// The bank tabs and their add/delete actions intentionally use the original
// 28px rule; this is independent from the 20px RESET action height.
inline constexpr auto kPresetTabActionSize = 28.0;
inline constexpr auto kPresetTabActionBottom =
    kPresetTabActionTop + kPresetTabActionSize;
// The outer scroll view includes the 28px tab row and its 6px scrollbar.
inline constexpr auto kPresetTabsBottom =
    kPresetTabActionBottom + kPresetTabScrollbarHeight;
inline constexpr auto kPresetTabsRight = 234.0;
inline constexpr auto kPresetDeleteRight = kCompactRight;
inline constexpr auto kPresetDeleteLeft =
    kPresetDeleteRight - kPresetTabActionSize;
inline constexpr auto kPresetAddRight =
    kPresetDeleteLeft - kPresetTabActionGap;
inline constexpr auto kPresetAddLeft =
    kPresetAddRight - kPresetTabActionSize;
inline constexpr auto kPresetTabHitBottom = kPresetTabsBottom -
                                             kPresetTabScrollbarHeight;
inline constexpr auto kPresetTabItemTop = 6.0;
inline constexpr auto kPresetTabItemBottom =
    kPresetTabItemTop + kPresetTabActionSize;
inline constexpr auto kPresetTabItemGap = 4.0;
inline constexpr auto kPresetTabContainerHeight = kPresetTabItemBottom;
// Preset rows are placed on a 42 px rhythm (38 px surface plus 4 px gap).
// Keep the viewport an exact number of rows high. If its height ends mid-row,
// the first scroll offset is fractional and VSTGUI rounds it, leaving the
// row gap exposed at the top exactly when the list first overflows.
// Keep the first preset row separated from the 28px bank row by the same
// 4px section margin used by the other stacked controls.  Deriving this from
// the bank action bottom prevents the old 28px-row spacing from returning.
inline constexpr auto kPresetListTop =
    kPresetTabActionBottom + kPanelSectionGap;
inline constexpr auto kPresetBottomControlsHeight = 64.0;
inline constexpr auto kPresetListBottomGap = 6.0;
inline constexpr auto kPresetRowBottomGap = 4.0;
inline constexpr auto kPresetPanelInset = 10.0;
inline constexpr auto kPresetBottomRowHeight = 28.0;
inline constexpr auto kPresetBottomRowGap = 4.0;
inline constexpr auto kPresetBottomInset = 4.0;
inline constexpr auto kPresetPanelRight = kCompactRight;

inline auto PresetListViewportHeight(const VSTGUI::CCoord panel_height)
    -> VSTGUI::CCoord {
  const auto available_height = std::max(
      0.0, panel_height - kPresetBottomControlsHeight -
               kPresetListBottomGap - kPresetListTop);
  const auto complete_rows = static_cast<int>(available_height / kPresetRowHeight);
  return static_cast<VSTGUI::CCoord>(complete_rows) * kPresetRowHeight;
}

inline auto WindowRect() -> VSTGUI::CRect {
  return {0, 0, kWindowWidth, kWindowHeight};
}

inline auto MainPageRect() -> VSTGUI::CRect {
  return {0, kMainContentTop, kWindowWidth, kWindowHeight};
}

inline auto ColumnRect(const int display_index) -> VSTGUI::CRect {
  const auto left = kColumnOuterMargin +
                    display_index * (kColumnWidth + kColumnGap);
  return {left, kColumnTop, left + kColumnWidth,
          kColumnTop + kColumnContentHeight};
}

inline auto ColumnRect(const ColumnRole role) -> VSTGUI::CRect {
  const auto it = std::find(kColumnOrder.begin(), kColumnOrder.end(), role);
  const auto display_index =
      it == kColumnOrder.end()
          ? 0
          : static_cast<int>(std::distance(kColumnOrder.begin(), it));
  return ColumnRect(display_index);
}

inline auto HeaderModelRect() -> VSTGUI::CRect {
  const auto voice_column = ColumnRect(ColumnRole::kVoice);
  return {voice_column.left, 5.0, voice_column.right, 55.0};
}

inline auto HeaderModelSelectorRect() -> VSTGUI::CRect {
  // Keep the selector's original width, but center that region in the full
  // VOICE column.  This puts the unloaded placeholder at the actual second
  // column center instead of centering it in the right-hand remainder after
  // the MODEL label.
  const auto column_width = ColumnRect(ColumnRole::kVoice).getWidth();
  const auto selector_width = column_width - 65.0;
  const auto left = (column_width - selector_width) / 2.0;
  return {left, 0.0, left + selector_width, 50.0};
}

inline auto PanelRect(const double top, const double bottom) -> VSTGUI::CRect {
  return {0, top, kColumnWidth, bottom};
}

// The description popup always replaces the complete normal description
// stack. Keeping this rectangle semantic avoids accidentally making model and
// voice popups identical to their source panes.
inline auto DescriptionPopupRect() -> VSTGUI::CRect {
  return PanelRect(kDescriptionNormalTop, kColumnContentHeight);
}

inline auto TabRect(const int count, const int index) -> VSTGUI::CRect {
  // A three-way split is 102.666... px at the fixed 308 px column width.
  // Use floor/ceil at shared boundaries so adjacent tabs overlap by one
  // raster pixel where fractional rounding would otherwise create a seam.
  const auto boundary = [count](const int position) {
    return kColumnWidth * static_cast<double>(position) /
           static_cast<double>(count);
  };
  const auto left = index == 0 ? 0.0 : std::floor(boundary(index));
  const auto right = index + 1 == count
                         ? kColumnWidth
                         : std::ceil(boundary(index + 1));
  return {left, 0, right, kSplitTabHeight};
}

inline auto StandaloneInOutRect(const VSTGUI::CRect& rect)
    -> VSTGUI::CRect {
  auto moved = rect;
  moved.offset(0.0, kStandaloneInOutBodyOffsetY);
  return moved;
}

inline auto SplitTabRect(const int index) -> VSTGUI::CRect {
  return TabRect(2, index);
}

inline auto TripleTabRect(const int index) -> VSTGUI::CRect {
  return TabRect(3, index);
}

inline auto SettingsSliderRect(const int row) -> VSTGUI::CRect {
  const auto top = kSettingsSliderTop + row * kSettingsSliderStride;
  return {kSettingsSliderLeft, top, kSettingsSliderRight,
          top + kSettingsSliderHeight};
}

// Leave the slider title and value readable while placing the compact meter
// in the open centre of the same row, matching the standalone IN/OUT meter.
inline auto SettingsMeterRect(const int row) -> VSTGUI::CRect {
  const auto slider = SettingsSliderRect(row);
  const auto top = slider.top + 5.0;
  return {slider.left + 75.0, top, slider.right - 60.0,
          top + kLevelIndicatorHeight};
}

inline auto ControlLabelRect(const double top, const double bottom)
    -> VSTGUI::CRect {
  return {kPanelContentInset, top, kControlLabelRight, bottom};
}

inline auto ControlMenuRect(const double top, const double bottom)
    -> VSTGUI::CRect {
  return {kPanelContentInset, top, kControlRight, bottom};
}

inline auto DropdownChevronRect(const VSTGUI::CRect& rect) -> VSTGUI::CRect {
  return {rect.right - kDropdownChevronInset - kDropdownChevronWidth,
          rect.top + kDropdownChevronInset,
          rect.right - kDropdownChevronInset,
          rect.bottom - kDropdownChevronInset};
}

inline auto VoiceTitleRect() -> VSTGUI::CRect {
  return {0, 0, kColumnWidth, kSplitTabHeight};
}

inline auto MorphSwitchRect() -> VSTGUI::CRect {
  return {0, kMorphSwitchTop, kColumnWidth,
          kMorphSwitchTop + kSplitTabHeight};
}

inline auto FullWindowRect() -> VSTGUI::CRect {
  return {0, 0, kWindowWidth, kWindowHeight};
}

inline auto PanelTitleRect(const double right) -> VSTGUI::CRect {
  return {kPanelContentInset, kPanelTitleTop, right, kPanelTitleBottom};
}

// Section headings inside a multi-section page use the same font, color and
// left inset as the heading at the top of a panel.  Keeping the vertical
// position as an argument lets later sections (such as LATENCY) retain the
// established control rhythm without introducing a one-off label style.
inline auto PanelSectionTitleRect(const double top, const double right)
    -> VSTGUI::CRect {
  return {kPanelContentInset, top, right,
          top + (kPanelTitleBottom - kPanelTitleTop)};
}

inline auto ContentRect(const double top, const double bottom,
                        const bool compact = false) -> VSTGUI::CRect {
  const auto inset = compact ? kCompactContentInset : kPanelContentInset;
  const auto right = compact ? kCompactRight : kControlRight;
  return {inset, top, right, bottom};
}

// VST-only direct output page. Its geometry is derived from the same panel
// title, first-control, menu, meter and switch metrics used by the other
// pages. Keeping the dependency chain here prevents a later tab adjustment
// from leaving IN/OUT controls at an unrelated hand-tuned position.
inline auto VstInOutTitleRect() -> VSTGUI::CRect {
  return PanelTitleRect(kControlRight);
}

inline auto VstInOutOutputLabelRect() -> VSTGUI::CRect {
  const auto top = kSettingsSliderTop;
  return ControlLabelRect(top,
                          top + (kControlLabelBottom - kControlLabelTop));
}

inline auto VstInOutOutputMenuRect() -> VSTGUI::CRect {
  const auto label = VstInOutOutputLabelRect();
  const auto height = kControlMenuBottom - kControlMenuTop;
  return ContentRect(label.bottom, label.bottom + height);
}

inline auto VstInOutOutputMeterRect() -> VSTGUI::CRect {
  const auto menu = VstInOutOutputMenuRect();
  const auto top = menu.bottom + kPanelSectionGap;
  return ContentRect(top, top + kLevelIndicatorHeight);
}

inline auto VstInOutExclusiveRect() -> VSTGUI::CRect {
  const auto meter = VstInOutOutputMeterRect();
  const auto top = meter.bottom + kPanelGap;
  return ContentRect(top, top + kSwitchHeight);
}

inline auto VstInOutRecordingGeometry() -> RecordingBlockGeometry {
  const auto exclusive = VstInOutExclusiveRect();
  return RecordingBlockGeometryAt(exclusive.bottom + kPanelGap);
}

inline auto ResetButtonRect(const double top = kPanelTitleTop)
    -> VSTGUI::CRect {
  return {kCompactRight - kResetButtonWidth, top, kCompactRight,
          top + kResetButtonHeight};
}

inline auto HeaderPanelRect() -> VSTGUI::CRect {
  return {0, 0, kWindowWidth, kHeaderHeight};
}

// Keep the version label at the same right inset when the frame width is
// extended by a host or another standalone shell.
inline auto HeaderVersionRect(const VSTGUI::CCoord window_width)
    -> VSTGUI::CRect {
  auto rect = kHeaderVersionRect;
  rect.offset(window_width - kWindowWidth, 0.0);
  return rect;
}

inline auto PortraitImageRect() -> VSTGUI::CRect {
  return {kPortraitImageInset, kPortraitImageInset,
          kPortraitImageInset + kPortraitSize,
          kPortraitImageInset + kPortraitSize};
}

// The enlarged portrait keeps the exact centre of the normal 300px image.
// Its description begins immediately below the actual displayed bitmap, so
// portraits smaller than the 480px maximum do not leave a fixed empty area.
inline constexpr auto kPortraitPopupVerticalOffset = 8.0;

inline auto PortraitPopupRect(const double width, const double height)
    -> VSTGUI::CRect {
  const auto voice_column = ColumnRect(ColumnRole::kVoice);
  const auto center_x = (voice_column.left + voice_column.right) / 2.0;
  const auto center_y = kMainContentTop + kColumnTop + kPortraitNormalTop +
                        kPortraitImageInset + kPortraitSize / 2.0 +
                        kPortraitPopupVerticalOffset;
  return {center_x - width / 2.0, center_y - height / 2.0,
          center_x + width / 2.0, center_y + height / 2.0};
}

inline auto PortraitPopupDescriptionRect(
    const VSTGUI::CRect& portrait_rect) -> VSTGUI::CRect {
  const auto voice_column_top = kMainContentTop + kColumnTop;
  // The portrait is intentionally lowered to cover the sliver of the
  // underlying text. Keep the description panel at its original anchor so it
  // does not move together with that visual-only adjustment.
  const auto top = portrait_rect.bottom - kPortraitPopupVerticalOffset -
                   voice_column_top +
                   kPortraitPopupDescriptionGap;
  return PanelRect(top, kColumnContentHeight);
}

// The voice column has two mutually exclusive layouts. Keeping the
// rectangles together prevents a normal portrait and a morph pad from being
// resized by separate, later patches that can accidentally overlap each other.
struct VoiceModeGeometry {
  VSTGUI::CRect panel;
  VSTGUI::CRect content;
};

inline auto NormalVoiceModeGeometry() -> VoiceModeGeometry {
  return {PanelRect(kPortraitNormalTop, kPortraitNormalBottom),
          PortraitImageRect()};
}

inline auto MorphVoiceModeGeometry() -> VoiceModeGeometry {
  return {PanelRect(kMorphPortraitTop, kMorphPortraitBottom),
          PortraitImageRect()};
}

inline auto MorphVoiceDescriptionRect() -> VSTGUI::CRect {
  return PanelRect(kMorphVoiceDescriptionTop, kColumnContentHeight);
}

inline auto PopupRect() -> VSTGUI::CRect { return kPopupRect; }

inline auto PopupScrollRectForSize(const VSTGUI::CRect& size)
    -> VSTGUI::CRect {
  return {kPopupInset, kPopupScrollTop, size.getWidth() - kPopupInset,
          size.getHeight() - kPopupScrollBottomInset};
}

inline auto PresetBottomButtonRect(const int row, const int column,
                                   const double panel_height)
    -> VSTGUI::CRect {
  const auto left = kPresetPanelInset +
                    column * (kBottomButtonWidth + kBottomButtonGap);
  const auto top = panel_height - kPresetBottomInset -
                   kPresetBottomRowHeight -
                   row * (kPresetBottomRowHeight + kPresetBottomRowGap) +
                   kPresetBodyOffsetY;
  return {left, top, left + kBottomButtonWidth,
          top + kPresetBottomRowHeight};
}

inline auto PresetTabHitRect() -> VSTGUI::CRect {
  return {kCompactContentInset, kPresetTabsTop, kPresetTabsRight,
          kPresetTabHitBottom};
}

inline auto PresetBankTabRect(const int index, const double tab_width)
    -> VSTGUI::CRect {
  const auto left = index * tab_width;
  return {left, kPresetTabItemTop,
          (index + 1) * tab_width - kPresetTabItemGap,
          kPresetTabItemBottom};
}

inline auto PresetBankAddButtonRect() -> VSTGUI::CRect {
  return {kPresetAddLeft, kPresetTabActionTop, kPresetAddRight,
          kPresetTabActionBottom};
}

inline auto PresetBankDeleteButtonRect() -> VSTGUI::CRect {
  return {kPresetDeleteLeft, kPresetTabActionTop, kPresetDeleteRight,
          kPresetTabActionBottom};
}

}  // namespace beatrice::vst::layout

#endif  // BEATRICE_VST_EDITOR_LAYOUT_H_
