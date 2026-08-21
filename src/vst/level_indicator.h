// Copyright (c) 2024-2026 Project Beatrice and Contributors

#ifndef BEATRICE_VST_LEVEL_INDICATOR_H_
#define BEATRICE_VST_LEVEL_INDICATOR_H_

#include <algorithm>
#include <cmath>

#include "vst3sdk/vstgui4/vstgui/lib/cdrawcontext.h"
#include "vst3sdk/vstgui4/vstgui/lib/cview.h"

#include "vst/editor_layout.h"
#include "vst/editor_theme.h"

namespace beatrice::vst {

// The compact horizontal peak meter shared by the VST and standalone editors.
// Its input is a linear peak value; the visual scale is -60 dB to 0 dB.
class LevelIndicator final : public VSTGUI::CView {
 public:
  explicit LevelIndicator(const VSTGUI::CRect& rect) : CView(rect) {
    setMouseEnabled(false);
  }

  void SetEnabled(const bool enabled) {
    if (enabled_ == enabled) {
      return;
    }
    enabled_ = enabled;
    if (!enabled_) {
      level_ = 0.0F;
    }
    invalid();
  }

  void SetPeak(const float peak) {
    if (!enabled_) {
      return;
    }
    const auto next = std::max(
        std::clamp(peak, 0.0F, 2.0F),
        level_ * static_cast<float>(layout::kLevelPeakDecay));
    if (std::abs(next - level_) < 0.0001F) {
      return;
    }
    level_ = next;
    invalid();
  }

  void draw(VSTGUI::CDrawContext* const context) override {
    auto rect = getViewSize();
    context->setDrawMode(VSTGUI::kAntiAliasing);
    context->setFillColor(enabled_ ? theme::kSliderFrame : theme::kPanel);
    context->drawRect(rect, VSTGUI::kDrawFilled);
    rect.inset(1.0, 1.0);
    context->setFillColor(enabled_ ? theme::kMenuFill : theme::kPanel);
    context->drawRect(rect, VSTGUI::kDrawFilled);

    const auto decibels = level_ > 0.000001F
                              ? 20.0F * std::log10(level_)
                              : -60.0F;
    const auto normalized = std::clamp((decibels + 60.0F) / 60.0F,
                                       0.0F, 1.0F);
    if (enabled_ && normalized > 0.0F) {
      auto active = rect;
      active.right = active.left + active.getWidth() * normalized;
      context->setFillColor(
          level_ >= static_cast<float>(layout::kLevelClipThreshold)
              ? theme::kDeleteConfirm
              : theme::kSliderTrackActive);
      context->drawRect(active, VSTGUI::kDrawFilled);
    }
    setDirty(false);
  }

 private:
  bool enabled_ = true;
  float level_ = 0.0F;
};

}  // namespace beatrice::vst

#endif  // BEATRICE_VST_LEVEL_INDICATOR_H_
