// Copyright (c) 2026 Project Beatrice and Contributors

#ifndef BEATRICE_VST_EDITOR_TYPOGRAPHY_H_
#define BEATRICE_VST_EDITOR_TYPOGRAPHY_H_

#include "vst3sdk/vstgui4/vstgui/lib/cfont.h"

namespace beatrice::vst::typography {

// All editor fonts are named by their semantic role.  Keeping the values
// here prevents a title, tab, or description size from being changed in only
// one construction site.
inline constexpr auto kUiFontFamily = "Segoe UI";
inline constexpr auto kDescriptionFontFamily = "Meiryo UI";
inline constexpr auto kUiFontSize = 14.0;
inline constexpr auto kDescriptionFontSize = 11.0;
inline constexpr auto kSmallFontSize = 11.0;
inline constexpr auto kTabFontSize = 12.0;
inline constexpr auto kHeadingFontSize = 13.0;
inline constexpr auto kStrongFontSize = 16.0;

inline auto MakeUiFont(const bool bold = false) -> VSTGUI::CFontDesc* {
  return new VSTGUI::CFontDesc(kUiFontFamily, kUiFontSize,
                               bold ? VSTGUI::kBoldFace : 0);
}

inline auto MakeDescriptionFont() -> VSTGUI::CFontDesc* {
  return new VSTGUI::CFontDesc(kDescriptionFontFamily,
                               kDescriptionFontSize);
}

inline auto MakeSmallFont(const bool bold = false) -> VSTGUI::CFontDesc* {
  return new VSTGUI::CFontDesc(kUiFontFamily, kSmallFontSize,
                               bold ? VSTGUI::kBoldFace : 0);
}

inline auto MakeTabFont() -> VSTGUI::CFontDesc* {
  return new VSTGUI::CFontDesc(kUiFontFamily, kTabFontSize);
}

inline auto MakeHeadingFont() -> VSTGUI::CFontDesc* {
  return new VSTGUI::CFontDesc(kUiFontFamily, kHeadingFontSize,
                               VSTGUI::kBoldFace);
}

inline auto MakeStrongFont() -> VSTGUI::CFontDesc* {
  return new VSTGUI::CFontDesc(kUiFontFamily, kStrongFontSize,
                               VSTGUI::kBoldFace);
}

}  // namespace beatrice::vst::typography

#endif  // BEATRICE_VST_EDITOR_TYPOGRAPHY_H_
