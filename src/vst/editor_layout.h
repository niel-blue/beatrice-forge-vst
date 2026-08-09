// Copyright (c) 2024-2026 Project Beatrice and Contributors

#ifndef BEATRICE_VST_EDITOR_LAYOUT_H_
#define BEATRICE_VST_EDITOR_LAYOUT_H_

#include <algorithm>
#include <array>
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
inline constexpr auto kColumnContentHeight =
    kWindowHeight - kHeaderHeight - kColumnTop - kColumnBottomMargin;

// Header geometry. These values preserve the current 960x720 appearance,
// while keeping every header child tied to the same semantic layout source.
inline constexpr auto kHeaderLogoRect = VSTGUI::CRect(12, 7, 144, 51);
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
inline constexpr auto kPageIndicatorLeft = 288.0;
inline constexpr auto kPageIndicatorWidth = 112.0;
inline constexpr auto kPageIndicatorHeight = 46.0;

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
inline constexpr auto kSplitTabWidth = kColumnWidth / 2.0;
inline constexpr auto kSplitTabHeight = 30.0;
inline constexpr auto kTabContentGap = 4.0;
inline constexpr auto kDropdownChevronInset = 8.0;
inline constexpr auto kDropdownChevronWidth = 16.0;
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
    kSettingsTopPanelHeight + kTabContentGap;
inline constexpr auto kSettingsTabBodyTop = kSplitTabHeight + kTabContentGap;
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
inline constexpr auto kTuningLatencyLabelTop = 182.0;
inline constexpr auto kTuningLatencyLabelBottom = 202.0;
inline constexpr auto kTuningLatencyMenuTop = 204.0;
inline constexpr auto kTuningLatencyMenuBottom = 238.0;
inline constexpr auto kSourcePitchTop = 224.0;
inline constexpr auto kSourcePitchBottom = 392.0;
inline constexpr auto kSettingsTabBodyBottom = 432.0;

// Effects column. Each group has its own surface and reset action. The panel
// heights are derived from the shared slider rhythm so future controls cannot
// overlap the PRESETS/EFFECTS tabs or one another.
inline constexpr auto kEffectsClarityPanelTop = kSettingsTabBodyTop;
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
inline constexpr auto kPresetHeaderButtonTop = 4.0;
inline constexpr auto kPresetHeaderButtonBottom = 28.0;
inline constexpr auto kPresetSaveLeft = 104.0;
inline constexpr auto kPresetSaveRight = 200.0;
inline constexpr auto kPresetNewLeft = 204.0;
inline constexpr auto kPresetTabsTop = 38.0;
inline constexpr auto kPresetTabsBottom = 78.0;
inline constexpr auto kPresetTabsRight = 234.0;
inline constexpr auto kPresetTabActionTop = 44.0;
inline constexpr auto kPresetTabActionBottom = 72.0;
inline constexpr auto kPresetAddLeft = 238.0;
inline constexpr auto kPresetAddRight = 266.0;
inline constexpr auto kPresetDeleteLeft = 270.0;
inline constexpr auto kPresetTabHitBottom = kPresetTabsBottom -
                                             kPresetTabScrollbarHeight;
inline constexpr auto kPresetTabItemTop = 6.0;
inline constexpr auto kPresetTabItemBottom = 34.0;
inline constexpr auto kPresetTabItemGap = 4.0;
inline constexpr auto kPresetTabContainerHeight = kPresetTabItemBottom;
// Preset rows are placed on a 42 px rhythm (38 px surface plus 4 px gap).
// Keep the viewport an exact number of rows high. If its height ends mid-row,
// the first scroll offset is fractional and VSTGUI rounds it, leaving the
// row gap exposed at the top exactly when the list first overflows.
inline constexpr auto kPresetListTop = 76.0;
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
  return {65.0, 0.0, ColumnRect(ColumnRole::kVoice).getWidth(), 50.0};
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

inline auto SplitTabRect(const int index) -> VSTGUI::CRect {
  const auto left = index * kSplitTabWidth;
  return {left, 0, left + kSplitTabWidth, kSplitTabHeight};
}

inline auto SettingsSliderRect(const int row) -> VSTGUI::CRect {
  const auto top = kSettingsSliderTop + row * kSettingsSliderStride;
  return {kSettingsSliderLeft, top, kSettingsSliderRight,
          top + kSettingsSliderHeight};
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

inline auto PageIndicatorRect(const int index) -> VSTGUI::CRect {
  const auto left = kPageIndicatorLeft + index * kPageIndicatorWidth;
  return {left, 0, left + kPageIndicatorWidth, kPageIndicatorHeight};
}

inline auto PanelTitleRect(const double right) -> VSTGUI::CRect {
  return {kPanelContentInset, kPanelTitleTop, right, kPanelTitleBottom};
}

inline auto ContentRect(const double top, const double bottom,
                        const bool compact = false) -> VSTGUI::CRect {
  const auto inset = compact ? kCompactContentInset : kPanelContentInset;
  const auto right = compact ? kCompactRight : kControlRight;
  return {inset, top, right, bottom};
}

inline auto ResetButtonRect(const double top = kPanelTitleTop)
    -> VSTGUI::CRect {
  return {kCompactRight - kResetButtonWidth, top, kCompactRight,
          top + kResetButtonHeight};
}

inline auto HeaderPanelRect() -> VSTGUI::CRect {
  return {0, 0, kWindowWidth, kHeaderHeight};
}

inline auto PortraitImageRect() -> VSTGUI::CRect {
  return {kPortraitImageInset, kPortraitImageInset,
          kPortraitImageInset + kPortraitSize,
          kPortraitImageInset + kPortraitSize};
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
                   row * (kPresetBottomRowHeight + kPresetBottomRowGap);
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

}  // namespace beatrice::vst::layout

#endif  // BEATRICE_VST_EDITOR_LAYOUT_H_
