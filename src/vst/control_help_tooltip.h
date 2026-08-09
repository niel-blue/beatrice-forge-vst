// Copyright (c) 2026 Project Beatrice and Contributors

#ifndef BEATRICE_VST_CONTROL_HELP_TOOLTIP_H_
#define BEATRICE_VST_CONTROL_HELP_TOOLTIP_H_

#include "ui/control_help_text.h"
#include "vst3sdk/vstgui4/vstgui/lib/cview.h"

namespace beatrice::vst {

inline void ApplyControlHelpTooltip(VSTGUI::CView* const view,
                                    const ui::ControlHelpID help_id,
                                    const bool japanese_locale) {
  if (!view || !japanese_locale) {
    return;
  }
  const auto text = ui::GetControlHelpText(help_id);
  if (!text.empty()) {
    view->setTooltipText(text.data());
  }
}

}  // namespace beatrice::vst

#endif  // BEATRICE_VST_CONTROL_HELP_TOOLTIP_H_
