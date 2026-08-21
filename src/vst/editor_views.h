// Copyright (c) 2024-2026 Project Beatrice and Contributors

#ifndef BEATRICE_VST_EDITOR_VIEWS_H_
#define BEATRICE_VST_EDITOR_VIEWS_H_

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <functional>
#include <string>
#include <utility>

#include "vst3sdk/vstgui4/vstgui/lib/cdrawdefs.h"
#include "vst3sdk/vstgui4/vstgui/lib/controls/cscrollbar.h"
#include "vst3sdk/vstgui4/vstgui/lib/cscrollview.h"
#include "vst3sdk/vstgui4/vstgui/lib/cview.h"
#include "vst3sdk/vstgui4/vstgui/lib/cvstguitimer.h"
#include "vst3sdk/vstgui4/vstgui/lib/vstguibase.h"
#include "vst3sdk/vstgui4/vstgui/lib/vstguifwd.h"

// Beatrice
#include "vst/controls.h"
#include "vst/editor_layout.h"
#include "vst/editor_theme.h"

namespace beatrice::vst {

using VSTGUI::CView;
using VSTGUI::kDrawFilled;
using VSTGUI::kDrawStroked;

// A shared page-level vertical scroll surface.  The scrollbar is an overlay
// and is only enabled when the content exceeds the viewport, so adding future
// controls does not change the current column widths or reserve a permanent
// gutter in the design.
class VerticalScrollView final : public VSTGUI::CScrollView {
 public:
  VerticalScrollView(const VSTGUI::CRect& viewport,
                     const VSTGUI::CRect& content)
      : CScrollView(
            viewport, content,
            CScrollView::kVerticalScrollbar |
                CScrollView::kDontDrawFrame |
                CScrollView::kOverlayScrollbars |
                CScrollView::kAutoHideScrollbars,
            layout::kVerticalScrollbarWidth) {
    setBackgroundColor(VSTGUI::kTransparentCColor);
    setTransparency(true);
    UpdateScrollbar();
  }

  void setContainerSize(
      const VSTGUI::CRect& size,
      const bool keep_visible_area = false) override {
    CScrollView::setContainerSize(size, keep_visible_area);
    UpdateScrollbar();
  }

  void setViewSize(const VSTGUI::CRect& rect,
                   const bool invalid = true) override {
    CScrollView::setViewSize(rect, invalid);
    UpdateScrollbar();
  }

  bool attached(VSTGUI::CView* const parent) override {
    const auto result = CScrollView::attached(parent);
    UpdateScrollbar();
    return result;
  }

 private:
  void UpdateScrollbar() {
    auto* const scrollbar = getVerticalScrollbar();
    if (!scrollbar) {
      return;
    }
    scrollbar->setBackgroundColor(theme::kScrollbarBackground);
    scrollbar->setFrameColor(theme::kScrollbarFrame);
    scrollbar->setScrollerColor(theme::kScrollbarThumb);
    scrollbar->setMinScrollerLength(18.0);
    const auto needed = getContainerSize().getHeight() >
                        getVisibleClientRect().getHeight() + 0.5;
    scrollbar->setVisible(needed);
    scrollbar->setMouseEnabled(needed);
    scrollbar->setDirty();
  }
};

class TabAccentView final : public CView {
 public:
  explicit TabAccentView(const CRect& size) : CView(size) {}

  void draw(CDrawContext* const context) override {
    auto rect = getViewSize();
    context->saveGlobalState();
    context->setDrawMode(kAntiAliasing);
    const auto line_left = rect.left + 24.0;
    const auto line_right = rect.right - 24.0;
    const auto line_top = rect.bottom - 3.0;
    const auto line_bottom = rect.bottom;
    static constexpr auto kSegments = 64;
    for (auto i = 0; i < kSegments; ++i) {
      const auto t0 = static_cast<double>(i) / kSegments;
      const auto t1 = static_cast<double>(i + 1) / kSegments;
      const auto mid = (t0 + t1) * 0.5;
      const auto alpha = static_cast<uint8_t>(
          std::clamp(255.0 * (1.0 - std::abs(mid - 0.5) * 2.0), 0.0, 255.0));
      context->setFillColor(theme::WithAlpha(theme::kTextAccent, alpha));
      context->drawRect(
          CRect(line_left + (line_right - line_left) * t0, line_top,
                line_left + (line_right - line_left) * t1 + 0.35, line_bottom),
          kDrawFilled);
    }
    context->restoreGlobalState();
    setDirty(false);
  }
};

inline void DrawMorphGridIcon(CDrawContext* const context, CRect rect) {
  context->setFillColor(theme::kMorphGridBackground);
  context->setFrameColor(theme::kMorphGridFrame);
  if (auto path =
          VSTGUI::owned(context->createRoundRectGraphicsPath(rect, 3.0))) {
    context->drawGraphicsPath(path, CDrawContext::kPathFilled);
    context->drawGraphicsPath(path, CDrawContext::kPathStroked);
  } else {
    context->drawRect(rect, kDrawFilledAndStroked);
  }
  rect.inset(5.0, 5.0);
  context->setFrameColor(theme::WithAlpha(theme::kTextAccent, 0x64));
  context->setLineWidth(1);
  context->drawRect(rect, kDrawStroked);
  context->setFrameColor(theme::kMorphGridLine);
  context->drawLine(CPoint(rect.left + rect.getWidth() / 2.0, rect.top),
                    CPoint(rect.left + rect.getWidth() / 2.0, rect.bottom));
  context->drawLine(CPoint(rect.left, rect.top + rect.getHeight() / 2.0),
                    CPoint(rect.right, rect.top + rect.getHeight() / 2.0));
}

class MorphSelectorIconView final : public CView {
 public:
  explicit MorphSelectorIconView(const CRect& rect) : CView(rect) {}

  void draw(CDrawContext* const context) override {
    auto rect = getViewSize();
    context->saveGlobalState();
    context->setDrawMode(kAntiAliasing);
    DrawMorphGridIcon(context, rect);
    context->restoreGlobalState();
    setDirty(false);
  }
};

class DismissOverlayView final : public CView {
 public:
  DismissOverlayView(const CRect& rect, std::function<void()> action)
      : CView(rect), action_(std::move(action)) {}

  auto onMouseDown(CPoint& where, const CButtonState& buttons)
      -> CMouseEventResult override {
    if (buttons.isLeftButton() && action_) {
      action_();
      return VSTGUI::kMouseEventHandled;
    }
    return CView::onMouseDown(where, buttons);
  }

  void draw(CDrawContext* const /*context*/) override { setDirty(false); }

 private:
  std::function<void()> action_;
};

enum class ActionIcon {
  kNone,
  kImport,
  kExport,
  kTrash,
  kPencil,
  kPlus,
  kUp,
  kDown,
  kSave,
  kPower,
  kPlay,
  kPause,
  kStop,
  kLoop,
  kRecord,
};

class GlowingActionLabel final : public ActionLabel {
 public:
  GlowingActionLabel(const CRect& size, const UTF8String& text,
                     std::function<void()> action)
      : ActionLabel(size, text, action), action_(std::move(action)) {}

  void SetActive(const bool active) {
    if (active_ == active) {
      return;
    }
    active_ = active;
    if (regular_font_ && bold_font_) {
      setFont(active_ ? bold_font_ : regular_font_);
    }
    if (state_text_colors_configured_) {
      setFontColor(active_ ? active_text_color_ : inactive_text_color_);
    }
    invalid();
  }

  void SetActiveColor(const CColor& color) { active_color_ = color; }

  void SetInactiveColor(const CColor& color) { inactive_color_ = color; }

  // Action buttons can opt into the shared warm outline without changing the
  // appearance of tabs, preset rows or other labels that use this view.
  void SetOutlined(const bool outlined, const CCoord radius = 3.0) {
    outlined_ = outlined;
    outline_radius_ = radius;
    invalid();
  }

  void SetStateFonts(CFontRef regular_font, CFontRef bold_font) {
    regular_font_ = regular_font;
    bold_font_ = bold_font;
    setFont(active_ ? bold_font_ : regular_font_);
  }

  void SetStateTextColors(const CColor& active, const CColor& inactive) {
    active_text_color_ = active;
    inactive_text_color_ = inactive;
    state_text_colors_configured_ = true;
    setFontColor(active_ ? active_text_color_ : inactive_text_color_);
  }

  void SetIcon(const ActionIcon icon) {
    icon_ = icon;
    setTextInset(CPoint{});
    invalid();
  }

  // Compact action buttons such as the standalone REC/STOP control can keep
  // their icon inside a narrow 80px button without changing the shared gap
  // used by preset-management actions.
  void SetIconTextGap(const CCoord gap) {
    icon_text_gap_ = std::max(0.0, gap);
    invalid();
  }

  void SetRightClickAction(std::function<void()> action) {
    right_click_action_ = std::move(action);
  }

  // Drag direction is explicit so the same control supports vertical preset
  // rows and horizontal preset-bank tabs.
  void SetDragFinishedAction(std::function<void(CCoord)> action) {
    drag_finished_action_ = std::move(action);
    drag_axis_ = DragAxis::kVertical;
  }

  void SetHorizontalDragFinishedAction(std::function<void(CCoord)> action) {
    drag_finished_action_ = std::move(action);
    drag_axis_ = DragAxis::kHorizontal;
  }

  auto onMouseDown(CPoint& where, const CButtonState& buttons)
      -> CMouseEventResult override {
    if (buttons.isRightButton() && right_click_action_) {
      right_click_action_();
      return VSTGUI::kMouseEventHandled;
    }
    if (buttons.isLeftButton()) {
      drag_candidate_ = static_cast<bool>(drag_finished_action_);
      dragging_ = false;
      drag_start_ = DragCoordinate(where);
      flash_ = true;
      invalid();
      VSTGUI::Call::later(
          [self = VSTGUI::shared(this)]() {
            self->flash_ = false;
            self->invalid();
          },
          120);
      // A draggable row must remain alive until mouse-up. Its normal click
      // action rebuilds the preset list, so defer it until we know that this
      // was a click rather than a drag.
      if (drag_candidate_) {
        return VSTGUI::kMouseEventHandled;
      }
    }
    return ActionLabel::onMouseDown(where, buttons);
  }

  auto onMouseMoved(CPoint& where, const CButtonState& buttons)
      -> CMouseEventResult override {
    if (drag_candidate_ && buttons.isLeftButton()) {
      dragging_ =
          dragging_ || std::abs(DragCoordinate(where) - drag_start_) >= 4.0;
      return VSTGUI::kMouseEventHandled;
    }
    return ActionLabel::onMouseMoved(where, buttons);
  }

  auto onMouseUp(CPoint& where, const CButtonState& buttons)
      -> CMouseEventResult override {
    if (drag_candidate_) {
      const auto distance = DragCoordinate(where) - drag_start_;
      const auto was_dragging = dragging_;
      drag_candidate_ = false;
      dragging_ = false;
      if (was_dragging && drag_finished_action_) {
        drag_finished_action_(distance);
      } else if (!was_dragging && action_) {
        action_();
      }
      return VSTGUI::kMouseEventHandled;
    }
    return ActionLabel::onMouseUp(where, buttons);
  }

  auto onMouseCancel() -> CMouseEventResult override {
    drag_candidate_ = false;
    dragging_ = false;
    return ActionLabel::onMouseCancel();
  }

  void draw(CDrawContext* const context) override {
    ActionLabel::draw(context);
    const auto rect = getViewSize();
    const auto text = getText().getPlatformString();
    context->saveGlobalState();
    context->setDrawMode(kAntiAliasing);
    if (icon_ == ActionIcon::kPower) {
      auto power_rect = rect;
      power_rect.inset(1.5, 1.5);
      context->setFillColor(active_ ? active_color_ : inactive_color_);
      context->drawEllipse(power_rect, kDrawFilled);
      if (flash_) {
        context->setFillColor(theme::kActionFlash);
        context->drawEllipse(power_rect, kDrawFilled);
      }
    } else if (active_ || flash_) {
      context->setFillColor(active_ ? active_color_ : theme::kActionFlash);
      context->drawRect(rect, kDrawFilled);
      context->setFont(getFont());
      for (const auto& pass : std::array{
             std::pair{theme::kActionGlowNear, 2.0},
             std::pair{theme::kActionGlowMid, 5.0},
             std::pair{theme::kActionGlowFar, 8.0},
           }) {
        context->setFontColor(pass.first);
        context->drawString(text,
                            CRect(rect.left - pass.second, rect.top,
                                  rect.right + pass.second, rect.bottom),
                            CHoriTxtAlign::kCenterText, true);
        context->drawString(text,
                            CRect(rect.left, rect.top - pass.second, rect.right,
                                  rect.bottom + pass.second),
                            CHoriTxtAlign::kCenterText, true);
      }
      context->setFontColor(getFontColor());
      context->drawString(text, rect, CHoriTxtAlign::kCenterText, true);
    }
    DrawIcon(context, rect);
    if (outlined_) {
      const auto path = VSTGUI::owned(context->createGraphicsPath());
      if (path) {
        auto frame_rect = rect;
        frame_rect.inset(0.5, 0.5);
        path->addRoundRect(frame_rect, outline_radius_);
        context->setLineStyle(kLineSolid);
        context->setLineWidth(1.0);
        context->setFrameColor(theme::kActionOutline);
        context->drawGraphicsPath(path, CDrawContext::kPathStroked);
      }
    }
    context->restoreGlobalState();
    setDirty(false);
  }

 private:
  enum class DragAxis { kVertical, kHorizontal };

  [[nodiscard]] auto DragCoordinate(const CPoint& point) const -> CCoord {
    return drag_axis_ == DragAxis::kHorizontal ? point.x : point.y;
  }

  void DrawIcon(CDrawContext* const context, const CRect& rect) const {
    if (icon_ == ActionIcon::kNone) return;
    // Icons and text use one common baseline.  The icon's right edge is kept
    // 20 px from the text's left edge; measuring after setting the label font
    // avoids the previous overlap caused by the context's stale font.
    context->setFont(getFont());
    constexpr auto kIconHalfWidth = 6.0;
    const auto x = getText().getString().empty()
                       ? rect.getCenter().x
                       : rect.getCenter().x -
                             context->getStringWidth(
                                 getText().getPlatformString()) /
                             2.0 -
                             icon_text_gap_ - kIconHalfWidth;
    const auto y = rect.top + rect.getHeight() / 2.0 -
                   (icon_ == ActionIcon::kPower ? 1.0 : 0.0);
    context->setFrameColor(getFontColor());
    context->setFillColor(getFontColor());
    context->setLineWidth(1.5);
    const auto line = [context](double x1, double y1, double x2, double y2) {
      context->drawLine(CPoint(x1, y1), CPoint(x2, y2));
    };
    if (icon_ == ActionIcon::kPlus) {
      line(x - 4, y, x + 4, y); line(x, y - 4, x, y + 4);
    } else if (icon_ == ActionIcon::kUp || icon_ == ActionIcon::kDown) {
      const auto d = icon_ == ActionIcon::kUp ? -1.0 : 1.0;
      line(x - 4, y - 2 * d, x, y + 2 * d);
      line(x, y + 2 * d, x + 4, y - 2 * d);
    } else if (icon_ == ActionIcon::kTrash) {
      context->drawRect(CRect(x - 4, y - 3, x + 4, y + 5), kDrawStroked);
      line(x - 5, y - 5, x + 5, y - 5); line(x - 2, y - 7, x + 2, y - 7);
    } else if (icon_ == ActionIcon::kPencil) {
      line(x - 4, y + 4, x + 4, y - 4); line(x - 4, y + 4, x - 5, y + 5);
      line(x + 2, y - 5, x + 5, y - 2);
    } else if (icon_ == ActionIcon::kSave) {
      context->drawRect(CRect(x - 6, y - 7, x + 6, y + 7), kDrawStroked);
      context->drawRect(CRect(x - 3, y - 6, x + 3, y - 1), kDrawFilled);
      context->drawRect(CRect(x - 3, y + 1, x + 3, y + 6), kDrawStroked);
    } else if (icon_ == ActionIcon::kPower) {
      // Compact power glyph for the header bypass switch.  The open top is
      // represented by the vertical stem and a stroked circular body.
      context->drawEllipse(CRect(x - 7, y - 6, x + 7, y + 8), kDrawStroked);
      line(x, y - 8, x, y + 1);
    } else if (icon_ == ActionIcon::kImport ||
               icon_ == ActionIcon::kExport) {
      const auto up = icon_ == ActionIcon::kExport;
      const auto tip_y = up ? y - 6 : y + 2;
      const auto tail_y = up ? y + 2 : y - 6;
      line(x, tail_y, x, tip_y);
      line(x - 3, tip_y + (up ? 3 : -3), x, tip_y);
      line(x, tip_y, x + 3, tip_y + (up ? 3 : -3));
      line(x - 6, y + 4, x - 6, y + 7);
      line(x - 6, y + 7, x + 6, y + 7);
      line(x + 6, y + 7, x + 6, y + 4);
    } else if (icon_ == ActionIcon::kPlay) {
      const auto points = CDrawContext::PointList{
          CPoint(x - 4.0, y - 6.0), CPoint(x + 5.0, y),
          CPoint(x - 4.0, y + 6.0)};
      context->drawPolygon(points, kDrawFilled);
    } else if (icon_ == ActionIcon::kPause) {
      context->drawRect(CRect(x - 5.0, y - 6.0, x - 1.0, y + 6.0),
                        kDrawFilled);
      context->drawRect(CRect(x + 1.0, y - 6.0, x + 5.0, y + 6.0),
                        kDrawFilled);
    } else if (icon_ == ActionIcon::kStop) {
      context->drawRect(CRect(x - 5.0, y - 5.0, x + 5.0, y + 5.0),
                        kDrawFilled);
    } else if (icon_ == ActionIcon::kLoop) {
      // Keep the repeat glyph legible at the compact 20 px transport-button
      // size.  A complete ring plus two arrowheads is more reliable across
      // VSTGUI backends than two partially overlapping arcs.
      context->drawEllipse(CRect(x - 6.0, y - 5.0, x + 6.0, y + 5.0),
                           kDrawStroked);
      line(x + 2.0, y - 5.0, x + 7.0, y - 5.0);
      line(x + 7.0, y - 5.0, x + 5.0, y - 3.0);
      line(x - 2.0, y + 5.0, x - 7.0, y + 5.0);
      line(x - 7.0, y + 5.0, x - 5.0, y + 3.0);
    } else if (icon_ == ActionIcon::kRecord) {
      context->drawEllipse(CRect(x - 5.0, y - 5.0, x + 5.0, y + 5.0),
                           kDrawFilled);
    }
  }

  ActionIcon icon_ = ActionIcon::kNone;
  CCoord icon_text_gap_ = layout::kIconTextGap;
  std::function<void()> action_;
  std::function<void()> right_click_action_;
  std::function<void(CCoord)> drag_finished_action_;
  CCoord drag_start_ = 0.0;
  DragAxis drag_axis_ = DragAxis::kVertical;
  bool drag_candidate_ = false;
  bool dragging_ = false;
  bool active_ = false;
  bool state_text_colors_configured_ = false;
  CColor active_text_color_;
  CColor inactive_text_color_;
  CColor active_color_ = theme::kSelected;
  CColor inactive_color_ = theme::kDropdown;
  bool outlined_ = false;
  CCoord outline_radius_ = 3.0;
  bool flash_ = false;
  CFontRef regular_font_ = nullptr;
  CFontRef bold_font_ = nullptr;
};

class VoiceMenuItemView final : public CView {
 public:
  VoiceMenuItemView(const CRect& rect, std::string label,
                    SharedPointer<CBitmap> thumbnail, bool morph_item,
                    bool selected, CFontRef font, std::function<void()> action)
      : CView(rect),
        label_(std::move(label)),
        thumbnail_(std::move(thumbnail)),
        morph_item_(morph_item),
        selected_(selected),
        font_(font),
        action_(std::move(action)) {
    if (font_) {
      font_->remember();
    }
  }

  ~VoiceMenuItemView() override {
    if (font_) {
      font_->forget();
    }
  }

  auto onMouseDown(CPoint& where, const CButtonState& buttons)
      -> CMouseEventResult override {
    if (buttons.isLeftButton() && action_) {
      action_();
      return VSTGUI::kMouseEventHandled;
    }
    return CView::onMouseDown(where, buttons);
  }

  auto onMouseEntered(CPoint& where, const CButtonState& buttons)
      -> CMouseEventResult override {
    hovered_ = true;
    invalid();
    return CView::onMouseEntered(where, buttons);
  }

  auto onMouseExited(CPoint& where, const CButtonState& buttons)
      -> CMouseEventResult override {
    hovered_ = false;
    invalid();
    return CView::onMouseExited(where, buttons);
  }

  void draw(CDrawContext* const context) override {
    auto rect = getViewSize();
    context->saveGlobalState();
    context->setDrawMode(kAntiAliasing);
    if (selected_) {
      context->setFillColor(hovered_ ? theme::kVoiceMenuHover
                                     : theme::WithAlpha(theme::kVoiceMenuHover,
                                                         0x17));
      context->setFrameColor(hovered_ ? theme::kVoiceMenuFrame
                                      : theme::WithAlpha(theme::kVoiceMenuFrame,
                                                          0x6b));
    } else if (hovered_) {
      context->setFillColor(theme::WithAlpha(theme::kVoiceMenuHover, 0x0f));
      context->setFrameColor(kTransparentCColor);
    } else {
      context->setFillColor(kTransparentCColor);
      context->setFrameColor(kTransparentCColor);
    }
    auto item_rect = rect;
    item_rect.inset(0.5, 0.5);
    if (auto path = VSTGUI::owned(
            context->createRoundRectGraphicsPath(item_rect, 2.0))) {
      context->drawGraphicsPath(path, CDrawContext::kPathFilled);
      context->drawGraphicsPath(path, CDrawContext::kPathStroked);
    }

    constexpr auto kThumbSize = 42.0;
    const auto thumb_top =
        rect.top + std::floor((rect.getHeight() - kThumbSize) / 2.0);
    auto thumb = CRect(rect.left + 6, thumb_top, rect.left + 6 + kThumbSize,
                       thumb_top + kThumbSize);
    if (thumbnail_) {
      auto clip = thumb;
      clip.extend(1.0, 1.0);
      thumbnail_->draw(context, clip,
                       CPoint(clip.left - thumb.left, clip.top - thumb.top));
      auto thumb_frame = thumb;
      thumb_frame.inset(0.5, 0.5);
      context->setFrameColor(theme::WithAlpha(theme::kVoiceMenuPlaceholder,
                                              0x58));
      if (auto path = VSTGUI::owned(
              context->createRoundRectGraphicsPath(thumb_frame, 2.0))) {
        context->drawGraphicsPath(path, CDrawContext::kPathStroked);
      }
    } else {
      context->setFillColor(theme::kVoiceMenuPlaceholder);
      context->setFrameColor(theme::WithAlpha(theme::kTextAccent, 0x38));
      if (auto path =
              VSTGUI::owned(context->createRoundRectGraphicsPath(thumb, 2.0))) {
        context->drawGraphicsPath(path, CDrawContext::kPathFilled);
        context->drawGraphicsPath(path, CDrawContext::kPathStroked);
      }
      if (morph_item_) {
        DrawMorphGridIcon(context, thumb);
      }
    }

    context->setFont(font_);
    context->setFontColor(theme::kTextPrimary);
    constexpr auto kTextHeight = 18.0;
    const auto text_top =
        rect.top + std::floor((rect.getHeight() - kTextHeight) / 2.0);
    context->drawString(
        UTF8String(label_).getPlatformString(),
        CRect(rect.left + 58, text_top, rect.right - 8, text_top + kTextHeight),
        CHoriTxtAlign::kLeftText, true);
    context->restoreGlobalState();
    setDirty(false);
  }

 private:
  std::string label_;
  SharedPointer<CBitmap> thumbnail_;
  bool morph_item_;
  bool selected_;
  bool hovered_ = false;
  CFontRef font_;
  std::function<void()> action_;
};

}  // namespace beatrice::vst

#endif  // BEATRICE_VST_EDITOR_VIEWS_H_
