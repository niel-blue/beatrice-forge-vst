// Copyright (c) 2024-2026 Project Beatrice and Contributors

#ifndef BEATRICE_VST_EDITOR_THEME_H_
#define BEATRICE_VST_EDITOR_THEME_H_

#include <cstdint>

#include "vst3sdk/vstgui4/vstgui/lib/ccolor.h"

namespace beatrice::vst::theme {

enum class ActionRole { kSwitch, kAction, kToggle };

inline auto WithAlpha(const VSTGUI::CColor& color, const uint8_t alpha)
    -> VSTGUI::CColor {
  return {color.red, color.green, color.blue, alpha};
}

// Named palettes. Components below use the role aliases at the end of this
// file, so changing the active palette never requires editing UI code.
namespace blue {
inline constexpr auto kBackground = VSTGUI::CColor(0x13, 0x1a, 0x20);
inline constexpr auto kSurface = VSTGUI::CColor(0x1e, 0x32, 0x41);
inline constexpr auto kPanel = VSTGUI::CColor(0x1e, 0x32, 0x41);
inline constexpr auto kHeader = kSurface;
inline constexpr auto kTabSelected = VSTGUI::CColor(0x1e, 0x32, 0x41);
inline constexpr auto kTabUnselected = VSTGUI::CColor(0x19, 0x26, 0x32);
inline constexpr auto kSwitch = kTabUnselected;
inline constexpr auto kPresetTabSelected = VSTGUI::CColor(0x31, 0x48, 0x5b);
inline constexpr auto kPresetTabUnselected = VSTGUI::CColor(0x25, 0x3a, 0x4c);
inline constexpr auto kPresetSelected = VSTGUI::CColor(0x46, 0x72, 0x91);
inline constexpr auto kPresetUnselected = VSTGUI::CColor(0x31, 0x48, 0x5b);
inline constexpr auto kActionButton = VSTGUI::CColor(0x11, 0x47, 0x6b);
inline constexpr auto kActionPressed = VSTGUI::CColor(0x46, 0x72, 0x91);
inline constexpr auto kDeleteConfirm = VSTGUI::CColor(0x2d, 0x5c, 0x78);
inline constexpr auto kMorphGridBackground = VSTGUI::CColor(0x10, 0x19, 0x20);
inline constexpr auto kMorphGridFrame = VSTGUI::CColor(0x62, 0x9a, 0xbd, 0x60);
inline constexpr auto kMorphGridLine = VSTGUI::CColor(0x62, 0x9a, 0xbd, 0x3e);
inline constexpr auto kMorphMarkerBackground = VSTGUI::CColor(0x19, 0x2b, 0x38);
inline constexpr auto kMorphMarkerFrame = VSTGUI::CColor(0x9a, 0xd2, 0xf2, 0x84);
inline constexpr auto kMorphCursor = VSTGUI::CColor(0x76, 0xb7, 0xe1);
inline constexpr auto kMorphCursorFrame = VSTGUI::CColor(0x10, 0x20, 0x2c);
inline constexpr auto kMorphLabelBackground = VSTGUI::CColor(0x0d, 0x18, 0x22, 0xd8);
inline constexpr auto kMorphLabelFrame = VSTGUI::CColor(0x9a, 0xd2, 0xf2, 0x70);
inline constexpr auto kVoiceMenuHover = VSTGUI::CColor(0x58, 0x83, 0x9c, 0x22);
inline constexpr auto kVoiceMenuFrame = VSTGUI::CColor(0x76, 0xb7, 0xe1, 0x80);
inline constexpr auto kVoiceMenuPlaceholder = VSTGUI::CColor(0x24, 0x3d, 0x4d);
inline constexpr auto kSliderFrame = VSTGUI::CColor(0x05, 0x05, 0x05);
inline constexpr auto kSliderShadow = VSTGUI::CColor(0x00, 0x00, 0x00, 0x5c);
inline constexpr auto kSliderHoverOverlay = VSTGUI::CColor(0x66, 0xb8, 0xee, 0x28);
inline constexpr auto kDescriptionOverlay = VSTGUI::CColor(0x00, 0x00, 0x00, 0x2a);
inline constexpr auto kDropdown = VSTGUI::CColor(0x18, 0x22, 0x2d);
inline constexpr auto kDropdownBorder = VSTGUI::CColor(0x62, 0x9a, 0xbd, 0x88);
inline constexpr auto kScrollbarBackground = VSTGUI::CColor(0x12, 0x1b, 0x25, 0xd0);
inline constexpr auto kScrollbarFrame = VSTGUI::CColor(0x66, 0xb8, 0xee, 0x48);
inline constexpr auto kScrollbarThumb = VSTGUI::CColor(0x58, 0xb7, 0xf2, 0xe8);
inline constexpr auto kTextPrimary = VSTGUI::CColor(0xf0, 0xf5, 0xf9);
inline constexpr auto kTextSecondary = VSTGUI::CColor(0xc9, 0xde, 0xec);
inline constexpr auto kTextMuted = VSTGUI::CColor(0x9a, 0xb5, 0xc8);
inline constexpr auto kTextValue = VSTGUI::CColor(0xe3, 0xf0, 0xf8);
inline constexpr auto kTextAccent = VSTGUI::CColor(0x9a, 0xd2, 0xf2);
inline constexpr auto kTextDisabled = VSTGUI::CColor(0x78, 0x92, 0xa3);
inline constexpr auto kTextOnWarm = VSTGUI::CColor(0x10, 0x10, 0x0f);
inline constexpr auto kSliderHandle = VSTGUI::CColor(0x58, 0xb7, 0xf2);
inline constexpr auto kSliderTrackActive = VSTGUI::CColor(0x76, 0xb7, 0xe1);
inline constexpr auto kSliderTrackInactive = VSTGUI::CColor(0x3f, 0x70, 0x8d);
inline constexpr auto kSliderHandleFocused = VSTGUI::CColor(0x9a, 0xd2, 0xf2);
inline constexpr auto kSliderHandleUnfocused = VSTGUI::CColor(0x4d, 0x86, 0xa8);
inline constexpr auto kSliderDisabled = VSTGUI::CColor(0x4a, 0x5d, 0x6a);
inline constexpr auto kPanelOverlay = VSTGUI::CColor(0xff, 0xff, 0xff, 0x0a);
inline constexpr auto kControlOverlay = VSTGUI::CColor(0xff, 0xff, 0xff, 0x0e);
inline constexpr auto kHeaderOverlay = VSTGUI::CColor(0xff, 0xff, 0xff, 0x10);
inline constexpr auto kPanelBorder = VSTGUI::CColor(0x62, 0x9a, 0xbd, 0x88);
inline constexpr auto kPanelBorderSubtle = VSTGUI::CColor(0x62, 0x9a, 0xbd, 0x38);
inline constexpr auto kActionFlash = VSTGUI::CColor(0x9a, 0xd2, 0xf2, 0x24);
inline constexpr auto kActionGlowNear = VSTGUI::CColor(0x9a, 0xd2, 0xf2, 0x48);
inline constexpr auto kActionGlowMid = VSTGUI::CColor(0x9a, 0xd2, 0xf2, 0x2c);
inline constexpr auto kActionGlowFar = VSTGUI::CColor(0x9a, 0xd2, 0xf2, 0x18);
inline constexpr auto kBypassOnText = VSTGUI::CColor(0x9a, 0xe6, 0xa7);
inline constexpr auto kBypassOffText = VSTGUI::CColor(0x8a, 0x8d, 0x91);
inline constexpr auto kBypassOnBackground = VSTGUI::CColor(0x2f, 0x75, 0x51);
inline constexpr auto kBypassOffBackground = kDropdown;
}  // namespace blue

namespace original {
// Exact role values recovered from the upstream editor, with selected roles
// kept deliberately brighter so the active tab/control is unambiguous.
inline constexpr auto kBackground = VSTGUI::CColor(0x09, 0x09, 0x09);
inline constexpr auto kSurface = VSTGUI::CColor(0x22, 0x20, 0x1e);
inline constexpr auto kPanel = VSTGUI::CColor(0x22, 0x20, 0x1e);
inline constexpr auto kHeader = VSTGUI::CColor(0x13, 0x11, 0x0f);
// State colors are intentionally separated from the upstream base colors.
// The upstream charcoal/gold character is retained, while the active state
// is made visibly brighter than its inactive counterpart.
inline constexpr auto kTabSelected = VSTGUI::CColor(0x22, 0x20, 0x1e);
inline constexpr auto kTabUnselected = VSTGUI::CColor(0x16, 0x14, 0x11);
inline constexpr auto kSwitch = VSTGUI::CColor(0x16, 0x14, 0x11);
inline constexpr auto kPresetTabSelected = VSTGUI::CColor(0x34, 0x2f, 0x26);
inline constexpr auto kPresetTabUnselected = VSTGUI::CColor(0x19, 0x17, 0x14);
inline constexpr auto kPresetSelected = VSTGUI::CColor(0x62, 0x56, 0x46);
inline constexpr auto kPresetUnselected = VSTGUI::CColor(0x34, 0x2f, 0x26);
inline constexpr auto kActionButton = VSTGUI::CColor(0x44, 0x38, 0x22);
inline constexpr auto kActionPressed = VSTGUI::CColor(0x62, 0x56, 0x46);
inline constexpr auto kDeleteConfirm = VSTGUI::CColor(0x72, 0x32, 0x32);
inline constexpr auto kMorphGridBackground = VSTGUI::CColor(0x10, 0x0f, 0x0e);
inline constexpr auto kMorphGridFrame = VSTGUI::CColor(0xd6, 0xa8, 0x57, 0x60);
inline constexpr auto kMorphGridLine = VSTGUI::CColor(0xd6, 0xa8, 0x57, 0x3e);
inline constexpr auto kMorphMarkerBackground = VSTGUI::CColor(0x1d, 0x19, 0x14);
inline constexpr auto kMorphMarkerFrame = VSTGUI::CColor(0xeb, 0xca, 0x89, 0x84);
inline constexpr auto kMorphCursor = VSTGUI::CColor(0xd8, 0xb3, 0x6c);
inline constexpr auto kMorphCursorFrame = VSTGUI::CColor(0x15, 0x11, 0x0a, 0xa0);
inline constexpr auto kMorphLabelBackground = VSTGUI::CColor(0x07, 0x06, 0x05, 0xd8);
inline constexpr auto kMorphLabelFrame = VSTGUI::CColor(0xe0, 0xb8, 0x74, 0x70);
inline constexpr auto kVoiceMenuHover = VSTGUI::CColor(0xd7, 0xac, 0x62, 0x22);
inline constexpr auto kVoiceMenuFrame = VSTGUI::CColor(0xd8, 0xb3, 0x6d, 0x80);
inline constexpr auto kVoiceMenuPlaceholder = VSTGUI::CColor(0x27, 0x23, 0x1f);
inline constexpr auto kSliderFrame = VSTGUI::CColor(0x05, 0x05, 0x05);
inline constexpr auto kSliderShadow = VSTGUI::CColor(0x00, 0x00, 0x00, 0x5c);
inline constexpr auto kSliderHoverOverlay = VSTGUI::CColor(0xd6, 0xa8, 0x57, 0x28);
inline constexpr auto kDescriptionOverlay = VSTGUI::CColor(0x00, 0x00, 0x00, 0x2a);
inline constexpr auto kDropdown = VSTGUI::CColor(0x1b, 0x1a, 0x19);
inline constexpr auto kDropdownBorder = VSTGUI::CColor(0xd6, 0xa8, 0x57, 0x64);
inline constexpr auto kScrollbarBackground = VSTGUI::CColor(0x10, 0x10, 0x0f, 0xd0);
inline constexpr auto kScrollbarFrame = VSTGUI::CColor(0xd6, 0xa8, 0x57, 0x48);
inline constexpr auto kScrollbarThumb = VSTGUI::CColor(0xc3, 0xa0, 0x66, 0xe8);
inline constexpr auto kTextPrimary = VSTGUI::CColor(0xca, 0xc7, 0xc1);
inline constexpr auto kTextSecondary = VSTGUI::CColor(0xbb, 0xb8, 0xb2);
inline constexpr auto kTextMuted = VSTGUI::CColor(0xb8, 0xb5, 0xaf);
inline constexpr auto kTextValue = VSTGUI::CColor(0xca, 0xc7, 0xc1);
inline constexpr auto kTextAccent = VSTGUI::CColor(0xf0, 0xcf, 0x90);
inline constexpr auto kTextDisabled = VSTGUI::CColor(0x76, 0x73, 0x6d);
inline constexpr auto kTextOnWarm = VSTGUI::CColor(0x10, 0x10, 0x0f);
inline constexpr auto kSliderHandle = VSTGUI::CColor(0xc3, 0xa0, 0x66);
inline constexpr auto kSliderTrackActive = VSTGUI::CColor(0xe2, 0xba, 0x79);
inline constexpr auto kSliderTrackInactive = VSTGUI::CColor(0x8a, 0x59, 0x2c);
inline constexpr auto kSliderHandleFocused = VSTGUI::CColor(0xf4, 0xd7, 0x9e);
inline constexpr auto kSliderHandleUnfocused = VSTGUI::CColor(0xb0, 0x75, 0x38);
inline constexpr auto kSliderDisabled = VSTGUI::CColor(0x5b, 0x54, 0x49);
inline constexpr auto kPanelOverlay = VSTGUI::CColor(0xff, 0xff, 0xff, 0x0a);
inline constexpr auto kControlOverlay = VSTGUI::CColor(0xff, 0xff, 0xff, 0x0e);
inline constexpr auto kHeaderOverlay = VSTGUI::CColor(0xff, 0xff, 0xff, 0x10);
inline constexpr auto kPanelBorder = VSTGUI::CColor(0xb1, 0x9b, 0x7c, 0x88);
inline constexpr auto kPanelBorderSubtle = VSTGUI::CColor(0xb1, 0x9b, 0x7c, 0x38);
inline constexpr auto kActionFlash = VSTGUI::CColor(0xf0, 0xcf, 0x90, 0x24);
inline constexpr auto kActionGlowNear = VSTGUI::CColor(0xf0, 0xcf, 0x90, 0x48);
inline constexpr auto kActionGlowMid = VSTGUI::CColor(0xf0, 0xcf, 0x90, 0x2c);
inline constexpr auto kActionGlowFar = VSTGUI::CColor(0xf0, 0xcf, 0x90, 0x18);
inline constexpr auto kBypassOnText = VSTGUI::CColor(0x9a, 0xe6, 0xa7);
inline constexpr auto kBypassOffText = kTextDisabled;
inline constexpr auto kBypassOnBackground = VSTGUI::CColor(0x3b, 0x6f, 0x46);
inline constexpr auto kBypassOffBackground = kDropdown;
}  // namespace original

// Active palette. Keep this one line as the only switch between visual sets.
namespace active = original;

inline constexpr auto kBackground = active::kBackground;
inline constexpr auto kSurface = active::kSurface;
inline constexpr auto kPanel = active::kPanel;
inline constexpr auto kHeader = active::kHeader;
inline constexpr auto kTabSelected = active::kTabSelected;
inline constexpr auto kTabUnselected = active::kTabUnselected;
inline constexpr auto kSwitch = active::kSwitch;
inline constexpr auto kPresetTabSelected = active::kPresetTabSelected;
inline constexpr auto kPresetTabUnselected = active::kPresetTabUnselected;
inline constexpr auto kPresetSelected = active::kPresetSelected;
inline constexpr auto kPresetUnselected = active::kPresetUnselected;
inline constexpr auto kActionButton = active::kActionButton;
inline constexpr auto kActionPressed = active::kActionPressed;
inline constexpr auto kDeleteConfirm = active::kDeleteConfirm;
inline constexpr auto kMorphGridBackground = active::kMorphGridBackground;
inline constexpr auto kMorphGridFrame = active::kMorphGridFrame;
inline constexpr auto kMorphGridLine = active::kMorphGridLine;
inline constexpr auto kMorphMarkerBackground = active::kMorphMarkerBackground;
inline constexpr auto kMorphMarkerFrame = active::kMorphMarkerFrame;
inline constexpr auto kMorphCursor = active::kMorphCursor;
inline constexpr auto kMorphCursorFrame = active::kMorphCursorFrame;
inline constexpr auto kMorphLabelBackground = active::kMorphLabelBackground;
inline constexpr auto kMorphLabelFrame = active::kMorphLabelFrame;
inline constexpr auto kVoiceMenuHover = active::kVoiceMenuHover;
inline constexpr auto kVoiceMenuFrame = active::kVoiceMenuFrame;
inline constexpr auto kVoiceMenuPlaceholder = active::kVoiceMenuPlaceholder;
inline constexpr auto kSliderFrame = active::kSliderFrame;
inline constexpr auto kSliderShadow = active::kSliderShadow;
inline constexpr auto kSliderHoverOverlay = active::kSliderHoverOverlay;
inline constexpr auto kDescriptionOverlay = active::kDescriptionOverlay;
inline constexpr auto kDropdown = active::kDropdown;
inline constexpr auto kDropdownBorder = active::kDropdownBorder;
inline constexpr auto kScrollbarBackground = active::kScrollbarBackground;
inline constexpr auto kScrollbarFrame = active::kScrollbarFrame;
inline constexpr auto kScrollbarThumb = active::kScrollbarThumb;
inline constexpr auto kTextPrimary = active::kTextPrimary;
inline constexpr auto kTextSecondary = active::kTextSecondary;
inline constexpr auto kTextMuted = active::kTextMuted;
inline constexpr auto kTextValue = active::kTextValue;
inline constexpr auto kTextAccent = active::kTextAccent;
inline constexpr auto kTextDisabled = active::kTextDisabled;
inline constexpr auto kTextOnWarm = active::kTextOnWarm;
inline constexpr auto kTextSliderTitle = kTextSecondary;
inline constexpr auto kTextSliderValue = kTextValue;
inline constexpr auto kTextMorphName = kTextSecondary;
inline constexpr auto kTextMorphValue = kTextValue;
inline constexpr auto kTextMutedAlpha = VSTGUI::CColor(
    kTextMuted.red, kTextMuted.green, kTextMuted.blue, 0xa0);
inline constexpr auto kText = kTextPrimary;
inline constexpr auto kAccent = kTextAccent;
inline constexpr auto kSecondaryText = kTextSecondary;
inline constexpr auto kBodyText = kTextSecondary;
inline constexpr auto kWarmText = kTextPrimary;
inline constexpr auto kHeaderAccent = kTextAccent;
inline constexpr auto kSliderHandle = active::kSliderHandle;
inline constexpr auto kSliderTrackActive = active::kSliderTrackActive;
inline constexpr auto kSliderTrackInactive = active::kSliderTrackInactive;
inline constexpr auto kSliderHandleFocused = active::kSliderHandleFocused;
inline constexpr auto kSliderHandleUnfocused = active::kSliderHandleUnfocused;
inline constexpr auto kSliderDisabled = active::kSliderDisabled;
inline constexpr auto kPanelOverlay = active::kPanelOverlay;
inline constexpr auto kControlOverlay = active::kControlOverlay;
inline constexpr auto kHeaderOverlay = active::kHeaderOverlay;
inline constexpr auto kPanelBorder = active::kPanelBorder;
inline constexpr auto kDescriptionBorder = kPanelBorder;
inline constexpr auto kPanelBorderSubtle = active::kPanelBorderSubtle;
inline constexpr auto kActionFlash = active::kActionFlash;
inline constexpr auto kActionGlowNear = active::kActionGlowNear;
inline constexpr auto kActionGlowMid = active::kActionGlowMid;
inline constexpr auto kActionGlowFar = active::kActionGlowFar;
inline constexpr auto kBypassOnText = active::kBypassOnText;
inline constexpr auto kBypassOffText = active::kBypassOffText;
inline constexpr auto kBypassOnBackground = active::kBypassOnBackground;
inline constexpr auto kBypassOffBackground = active::kBypassOffBackground;

// Compatibility name used by older custom views.
inline constexpr auto kSelected = kTabSelected;

}  // namespace beatrice::vst::theme

#endif  // BEATRICE_VST_EDITOR_THEME_H_
