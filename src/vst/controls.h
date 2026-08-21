// Copyright (c) 2024-2026 Project Beatrice and Contributors

#ifndef BEATRICE_VST_CONTROLS_H_
#define BEATRICE_VST_CONTROLS_H_

#include <algorithm>
#include <charconv>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "vst3sdk/vstgui4/vstgui/lib/cbitmap.h"
#include "vst3sdk/vstgui4/vstgui/lib/ccolor.h"
#include "vst3sdk/vstgui4/vstgui/lib/cdrawcontext.h"
#include "vst3sdk/vstgui4/vstgui/lib/cdrawdefs.h"
#include "vst3sdk/vstgui4/vstgui/lib/cdrawmethods.h"
#include "vst3sdk/vstgui4/vstgui/lib/cfileselector.h"
#include "vst3sdk/vstgui4/vstgui/lib/cframe.h"
#include "vst3sdk/vstgui4/vstgui/lib/cfont.h"
#include "vst3sdk/vstgui4/vstgui/lib/cgraphicspath.h"
#include "vst3sdk/vstgui4/vstgui/lib/clinestyle.h"
#include "vst3sdk/vstgui4/vstgui/lib/controls/coptionmenu.h"
#include "vst3sdk/vstgui4/vstgui/lib/controls/cscrollbar.h"
#include "vst3sdk/vstgui4/vstgui/lib/controls/cparamdisplay.h"
#include "vst3sdk/vstgui4/vstgui/lib/controls/cslider.h"
#include "vst3sdk/vstgui4/vstgui/lib/cscrollview.h"
#include "vst3sdk/vstgui4/vstgui/lib/controls/ctextlabel.h"
#include "vst3sdk/vstgui4/vstgui/lib/cpoint.h"
#include "vst3sdk/vstgui4/vstgui/lib/cstring.h"
#include "vst3sdk/vstgui4/vstgui/lib/events.h"
#include "vst3sdk/vstgui4/vstgui/lib/vstguibase.h"
#include "vst3sdk/vstgui4/vstgui/lib/vstguifwd.h"
#include "vst/editor_layout.h"
#include "vst/editor_theme.h"

namespace beatrice::vst {

using VSTGUI::CBitmap;
using VSTGUI::CButtonState;
using VSTGUI::CColor;
using VSTGUI::CControl;
using VSTGUI::CCoord;
using VSTGUI::CDrawContext;
using VSTGUI::CFileExtension;
using VSTGUI::CFontRef;
using VSTGUI::CGraphicsPath;
using VSTGUI::CHoriTxtAlign;
using VSTGUI::CHorizontalSlider;
using VSTGUI::CMessageResult;
using VSTGUI::CMouseEventResult;
using VSTGUI::CNewFileSelector;
using VSTGUI::COptionMenu;
using VSTGUI::CParamDisplay;
using VSTGUI::CPoint;
using VSTGUI::CRect;
using VSTGUI::CTextLabel;

inline constexpr int kSliderKnobWidth = 6;
using VSTGUI::CView;
using VSTGUI::IControlListener;
using VSTGUI::kAntiAliasing;
using VSTGUI::kDrawFilledAndStroked;
using VSTGUI::kLineSolid;
using VSTGUI::kMessageNotified;
using VSTGUI::kTransparentCColor;
using VSTGUI::SharedPointer;
using VSTGUI::UTF8String;

// COptionMenu clips an overlong current entry at the view boundary. All
// Beatrice menus reserve the passive chevron at the right edge and use the
// same tail-truncation rule so device and model names remain readable while
// the full titles are retained in the popup entries.
class TruncatingOptionMenu : public COptionMenu {
 public:
  TruncatingOptionMenu(const CRect& size, IControlListener* listener,
                       const int32_t tag, CBitmap* background = nullptr)
      : COptionMenu(size, listener, tag, background) {
    setHoriAlign(CHoriTxtAlign::kLeftText);
    setTextInset(CPoint(layout::kDropdownTextInset, 0.0));
  }

  void draw(CDrawContext* const context) override {
    const auto* const item = getCurrent();
    drawBack(context);
    if (item != nullptr) {
      const auto available_width = std::max(
          0.0, getViewSize().getWidth() - layout::kDropdownChevronInset -
                   layout::kDropdownChevronWidth);
      const auto display_text = VSTGUI::CDrawMethods::createTruncatedText(
          VSTGUI::CDrawMethods::kTextTruncateTail, item->getTitle(),
          getFont(), available_width, getTextInset());
      drawPlatformText(context, display_text);
    }
    setDirty(false);
  }
};

class ChevronView final : public CView {
 public:
  ChevronView(const CRect& size, const CColor& color)
      : CView(size), color_(color) {
    setMouseEnabled(false);
  }

  void draw(CDrawContext* const context) override {
    auto rect = getViewSize();
    context->saveGlobalState();
    context->setDrawMode(kAntiAliasing);
    context->setLineStyle(kLineSolid);
    context->setLineWidth(1.5);
    context->setFrameColor(color_);
    const auto center_y = rect.getCenter().y + 1.0;
    const auto center_x = rect.getCenter().x;
    context->drawLine(CPoint(center_x - 4.0, center_y - 2.0),
                      CPoint(center_x, center_y + 2.0));
    context->drawLine(CPoint(center_x, center_y + 2.0),
                      CPoint(center_x + 4.0, center_y - 2.0));
    context->restoreGlobalState();
    setDirty(false);
  }

 private:
  CColor color_;
};

class ActionLabel : public CTextLabel {
 public:
  ActionLabel(const CRect& size, const UTF8String& text,
              std::function<void()> action)
      : CTextLabel(size, text, nullptr, CParamDisplay::kNoFrame),
        action_(std::move(action)) {}

  void SetAction(std::function<void()> action) { action_ = std::move(action); }

  auto onMouseDown(CPoint& where, const CButtonState& buttons)
      -> CMouseEventResult override {
    if (buttons.isLeftButton() && action_) {
      action_();
      return VSTGUI::kMouseEventHandled;
    }
    return CTextLabel::onMouseDown(where, buttons);
  }

 private:
  std::function<void()> action_;
};

class MonotoneBitmap : public CBitmap {
 public:
  MonotoneBitmap(const int width, const int height, const CColor& back_color,
                 const CColor& frame_color, CCoord radius = 0.0)
      : CBitmap(width, height),
        back_color_(back_color),
        frame_color_(frame_color),
        radius_(radius) {}

  void draw(  // NOLINT(google-default-arguments)
      CDrawContext* const context, const CRect& rect,
      const CPoint& offset = CPoint(0, 0),
      const float /*alpha*/ = 1.0f) override {
    context->setFillColor(back_color_);
    const SharedPointer<CGraphicsPath> path =
        VSTGUI::owned(context->createGraphicsPath());
    auto frame_rect = rect;
    frame_rect.offset(offset);
    auto stroke_path = true;
    if (path) {
      if (stroke_path) {
        if (frame_color_ == kTransparentCColor) {
          frame_rect.inset(1.0, 1.0);
          stroke_path = false;
        } else if (frame_color_ == back_color_) {
          stroke_path = false;
        } else {
          frame_rect.inset(0.5, 0.5);
        }
      }
      if (radius_ > 0.0) {
        path->addRoundRect(frame_rect, radius_);
      } else {
        path->addRect(frame_rect);
      }
      context->drawGraphicsPath(path, CDrawContext::kPathFilled);
      if (stroke_path) {
        context->setLineStyle(kLineSolid);
        context->setLineWidth(1);
        context->setFrameColor(frame_color_);
        context->drawGraphicsPath(path, CDrawContext::kPathStroked);
      }
    }
  }

 private:
  CColor back_color_;
  CColor frame_color_;
  CCoord radius_;
};

class Slider : public CHorizontalSlider {
 public:
  Slider(const CRect& size, IControlListener* const listener, const int32_t tag,
         const int min_pos, const int max_pos, CBitmap* const handle,
         CBitmap* const background, std::string units, CFontRef font_ref,
         CFontRef value_font_ref, std::string title, const int precision = 1)
      : CHorizontalSlider(size, listener, tag, min_pos, max_pos, handle,
                          background, CPoint(0, 0), CSliderBase::kLeft),
        units_(std::move(units)),
        font_ref_(font_ref),
        value_font_ref_(value_font_ref),
        title_(std::move(title)),
        precision_(precision) {
    font_ref_->remember();
    value_font_ref_->remember();
    instances_.push_back(this);
  }

  ~Slider() override {
    std::erase(instances_, this);
    font_ref_->forget();
    value_font_ref_->forget();
  }

  // NOLINTNEXTLINE(readability-identifier-naming)
  void setFineWheelInc(const float fine_wheel_inc) {
    fine_wheel_inc_ = fine_wheel_inc;
  }

  // NOLINTNEXTLINE(readability-identifier-naming)
  [[nodiscard]] auto getFineWheelInc() const -> float {
    return fine_wheel_inc_;
  }

  void SetWheelEditingEnabled(const bool enabled) {
    wheel_editing_enabled_ = enabled;
  }

  void SetValueLabels(std::vector<std::string> labels) {
    value_labels_ = std::move(labels);
  }

  void onMouseDownEvent(VSTGUI::MouseDownEvent& event) override {
    if (event.buttonState.isLeft()) {
      // CSlider normally requests focus through its parent container after a
      // handled mouse event.  Establish it here as well so the explicit
      // wheel-focus rule also works for double-click reset and sliders inside
      // a scroll view.
      if (auto* const frame = getFrame()) {
        frame->setFocusView(this);
      }
    }
    if (event.buttonState.isLeft() && event.clickCount == 2) {
      if (getValue() != getDefaultValue()) {
        beginEdit();
        setValue(getDefaultValue());
        valueChanged();
        endEdit();
        setDirty();
      }
      event.consumed = true;
      event.ignoreFollowUpMoveAndUpEvents(true);
      return;
    }
    if (event.buttonState.isLeft()) {
      mouse_drag_editing_ = true;
    }
    CHorizontalSlider::onMouseDownEvent(event);
  }

  auto onMouseMoved(CPoint& where, const CButtonState& buttons)
      -> CMouseEventResult override {
    fine_adjustment_ = (buttons & kZoomModifier) != 0;
    return CHorizontalSlider::onMouseMoved(where, buttons);
  }

  auto onMouseUp(CPoint& where, const CButtonState& buttons)
      -> CMouseEventResult override {
    const auto result = CHorizontalSlider::onMouseUp(where, buttons);
    fine_adjustment_ = false;
    if (mouse_drag_editing_) {
      mouse_drag_editing_ = false;
      if (drag_finished_action_) {
        drag_finished_action_();
      }
    }
    return result;
  }

  auto onMouseCancel() -> CMouseEventResult override {
    const auto result = CHorizontalSlider::onMouseCancel();
    fine_adjustment_ = false;
    if (mouse_drag_editing_) {
      mouse_drag_editing_ = false;
      if (drag_finished_action_) {
        drag_finished_action_();
      }
    }
    return result;
  }

  void onMouseWheelEvent(VSTGUI::MouseWheelEvent& event) override {
    // The wheel belongs to the nearest vertical page scroll surface.  A
    // slider must not steal it merely because the pointer happens to be over
    // a horizontal control; drag and keyboard editing remain available.
    if (!wheel_editing_enabled_) {
      ForwardWheelToScrollableAncestor(event);
      return;
    }
    // A normal mouse reports deltaY even when the control itself is a
    // horizontal slider. Use deltaX only for devices which emit no vertical
    // wheel component.
    auto distance = event.deltaY != 0.0 ? event.deltaY : event.deltaX;
    if (distance == 0.0) {
      return;
    }

    // The editor supplies a wheel increment per parameter. Holding Shift uses
    // that parameter's fine increment; this keeps low-cut, pitch and morph
    // controls independent instead of applying one global wheel step.
    fine_adjustment_ =
        (buttonStateFromEventModifiers(event.modifiers) & kZoomModifier) != 0;
    if (isInverseStyle()) {
      distance *= -1.0;
    }

    auto plain_value = getValue();
    // Device wheel deltas vary (fractional, 1, 2, or larger). Only use the
    // direction so one event always produces one predictable step. Integer
    // controls such as VQ Neighbor Count cannot represent tenths.
    const auto wheel_increment = fine_adjustment_ ? fine_wheel_inc_
                                                  : getWheelInc();
    plain_value += std::copysign(wheel_increment,
                                static_cast<float>(distance));
    wheel_editing_ = true;
    beginEdit();
    setValue(plain_value);
    if (isDirty()) {
      invalid();
      valueChanged();
    }
    endEdit();
    wheel_editing_ = false;
    fine_adjustment_ = false;
    event.consumed = true;
  }

 private:
  void ForwardWheelToScrollableAncestor(VSTGUI::MouseWheelEvent& event) {
    for (auto* parent = getParentView(); parent != nullptr;
         parent = parent->getParentView()) {
      auto* const scroll = dynamic_cast<VSTGUI::CScrollView*>(parent);
      if (!scroll) {
        continue;
      }
      if (event.deltaY != 0.0) {
        if (auto* const scrollbar = scroll->getVerticalScrollbar()) {
          scrollbar->onMouseWheelEvent(event);
          if (event.consumed) {
            return;
          }
        }
      }
      if (event.deltaX != 0.0) {
        if (auto* const scrollbar = scroll->getHorizontalScrollbar()) {
          scrollbar->onMouseWheelEvent(event);
          if (event.consumed) {
            return;
          }
        }
      }
    }
  }

 public:
  // CSliderBase::onKeyboardEvent がベース
  void onKeyboardEvent(VSTGUI::KeyboardEvent& event) override {
    using VSTGUI::VirtualKey;
    if (event.type != VSTGUI::EventType::KeyDown) {
      return;
    }
    switch (event.virt) {
      case VirtualKey::Up:
        MoveFocus(-1);
        event.consumed = true;
        break;
      case VirtualKey::Down:
        MoveFocus(1);
        event.consumed = true;
        break;
      case VirtualKey::Right:
        [[fallthrough]];
      case VirtualKey::Left: {
        auto distance = 1.0f;
        const auto is_inverse = isInverseStyle();
        if ((event.virt == VirtualKey::Left && !is_inverse) ||
            (event.virt == VirtualKey::Right && is_inverse)) {
          distance = -distance;
        }

        auto plain_value = getValue();
        fine_adjustment_ =
            (buttonStateFromEventModifiers(event.modifiers) & kZoomModifier) !=
            0;
        const auto increment =
            keyboard_inc_ > 0.0f
                ? (fine_adjustment_ ? keyboard_fine_inc_ : keyboard_inc_)
                : ((buttonStateFromEventModifiers(event.modifiers) &
                    kZoomModifier)
                       ? getFineWheelInc()
                       : getWheelInc());
        plain_value += distance * increment;
        setValue(plain_value);

        if (isDirty()) {
          invalid();
          beginEdit();
          valueChanged();
          endEdit();
        }
        fine_adjustment_ = false;
        event.consumed = true;
        break;
      }
      case VirtualKey::Escape: {
        if (isEditing()) {
          onMouseCancel();
          event.consumed = true;
        }
        break;
      }
      default:
        break;
    }
  }

  void SetFocusChangedAction(std::function<void()> action) {
    focus_changed_action_ = std::move(action);
  }

  void SetDragFinishedAction(std::function<void()> action) {
    drag_finished_action_ = std::move(action);
  }

  [[nodiscard]] static auto IsAnyMouseDragEditing() -> bool {
    return std::ranges::any_of(instances_, [](const auto* const slider) {
      return slider->mouse_drag_editing_;
    });
  }

  void SetKeyboardInc(const float increment) { keyboard_inc_ = increment; }
  void SetKeyboardFineInc(const float increment) {
    keyboard_fine_inc_ = increment;
  }
  [[nodiscard]] auto IsFineAdjustment() const -> bool {
    return fine_adjustment_;
  }
  [[nodiscard]] auto IsWheelEditing() const -> bool {
    return wheel_editing_;
  }

  void takeFocus() override {
    CView::takeFocus();
    focused_ = true;
    if (focus_changed_action_) {
      focus_changed_action_();
    }
    invalid();
  }

  void looseFocus() override {
    CView::looseFocus();
    focused_ = false;
    invalid();
  }

  void draw(CDrawContext* const context) override {
    // 値を文字で表示
    auto value_string = std::string();
    if (IsEnabled()) {
      if (!value_labels_.empty()) {
        const auto label_index = std::clamp(
            static_cast<int>(std::round(getValue() - getMin())), 0,
            static_cast<int>(value_labels_.size()) - 1);
        value_string = value_labels_[label_index];
      } else {
        value_string.resize(10);
        const auto result =
            std::to_chars(std::to_address(value_string.begin()),
                          std::to_address(value_string.end()), getValue(),
                          std::chars_format::fixed, precision_);
        value_string.resize(result.ptr - std::to_address(value_string.begin()));
        if (!units_.empty()) {
          value_string += " ";
          value_string += units_;
        }
      }
    } else {
      value_string = "Disabled";
    }

    auto rect = getViewSize();

    context->saveGlobalState();
    context->setDrawMode(kAntiAliasing);
    context->setFont(font_ref_);
    context->setFontColor(IsEnabled() ? theme::kTextSliderTitle
                                      : theme::kTextDisabled);
    // getPlatformString の結果を直接渡さないと
    // use-after-free になるので注意
    context->drawString(
        UTF8String(title_).getPlatformString(),
        CRect(rect.left, rect.top, rect.left + rect.getWidth() * 0.60,
              rect.top + 18),
        CHoriTxtAlign::kLeftText, true);
    context->setFont(value_font_ref_);
    context->setFontColor(IsEnabled() ? theme::kTextSliderValue
                                      : theme::kTextDisabled);
    context->drawString(UTF8String(value_string).getPlatformString(),
                        CRect(rect.left + rect.getWidth() * 0.56, rect.top,
                              rect.right, rect.top + 18),
                        CHoriTxtAlign::kRightText, true);

    const auto track_y = rect.top + GetTrackYOffset();
    const auto left = rect.left;
    const auto right = rect.right;
    context->setLineStyle(kLineSolid);
    context->setLineWidth(3);
    context->setFrameColor(theme::kSliderFrame);
    context->drawLine(CPoint(left, track_y), CPoint(right, track_y));
    const auto active_track = theme::kSliderTrackActive;
    const auto inactive_track = theme::kSliderTrackInactive;
    context->setFrameColor(IsEnabled() ? (focused_ ? active_track
                                                   : inactive_track)
                                       : theme::kSliderDisabled);
    const auto norm = (getMax() == getMin())
                          ? 0.0
                          : (getValue() - getMin()) / (getMax() - getMin());
    constexpr auto kKnobHalfWidth = kSliderKnobWidth / 2.0;
    constexpr auto kKnobFrameInset = 0.5;
    const auto knob_x = std::clamp(left + (right - left) * norm,
                                   left + kKnobHalfWidth + kKnobFrameInset,
                                   right - kKnobHalfWidth - kKnobFrameInset);
    context->drawLine(CPoint(left, track_y), CPoint(knob_x, track_y));
    context->setFillColor(IsEnabled() ? theme::kSliderHandle
                                      : theme::kSliderDisabled);
    auto knob_rect = CRect(knob_x - kKnobHalfWidth, track_y - 8.0,
                           knob_x + kKnobHalfWidth, track_y + 7.0);
    context->setFillColor(theme::WithAlpha(
        theme::kSliderShadow, IsEnabled() ? 0x5c : 0x2c));
    auto shadow_rect = knob_rect;
    shadow_rect.offset(0.0, 2.0);
    if (auto path = VSTGUI::owned(
            context->createRoundRectGraphicsPath(shadow_rect, 2.0))) {
      context->drawGraphicsPath(path, CDrawContext::kPathFilled);
    }
    context->setFillColor(IsEnabled() ? (focused_ ? active_track
                                                  : inactive_track)
                                      : theme::kSliderDisabled);
    context->setFrameColor(
        IsEnabled()
            ? (focused_ ? theme::kSliderHandleFocused
                        : theme::kSliderHandleUnfocused)
            : theme::kSliderDisabled);
    context->setLineWidth(1);
    if (auto path = VSTGUI::owned(
            context->createRoundRectGraphicsPath(knob_rect, 2.0))) {
      context->drawGraphicsPath(path, CDrawContext::kPathFilled);
      context->drawGraphicsPath(path, CDrawContext::kPathStroked);
    } else {
      context->drawRect(knob_rect, kDrawFilledAndStroked);
    }
    context->restoreGlobalState();
    setDirty(false);
  }

  void SetEnabled(const bool enabled) {
    if (enabled_ == enabled) {
      return;
    }
    enabled_ = enabled;
    if (enabled_) {
      setMouseEnabled(true);
      setWantsFocus(true);
      setAlphaValue(1.0);
    } else {
      setMouseEnabled(false);
      setWantsFocus(false);
      setAlphaValue(0.3);
    }
  }

  [[nodiscard]] auto IsEnabled() const -> bool { return enabled_; }

 protected:
  [[nodiscard]] virtual auto GetTrackYOffset() const -> CCoord { return 33.0; }

 private:
  [[nodiscard]] auto IsEffectivelyVisible() const -> bool {
    for (auto* view = static_cast<const CView*>(this); view;
         view = view->getParentView()) {
      if (!view->isVisible()) {
        return false;
      }
    }
    return true;
  }

  void MoveFocus(const int direction) {
    auto* const current_frame = getFrame();
    if (!current_frame) {
      return;
    }
    const auto current_center =
        translateToGlobal(CRect(0, 0, getViewSize().getWidth(),
                                getViewSize().getHeight())).getCenter();
    auto candidates = std::vector<std::pair<double, Slider*>>{};
    for (auto* const slider : instances_) {
      if (slider->getFrame() != current_frame || !slider->IsEnabled() ||
          !slider->IsEffectivelyVisible()) {
        continue;
      }
      const auto center = slider->translateToGlobal(
          CRect(0, 0, slider->getViewSize().getWidth(),
                slider->getViewSize().getHeight())).getCenter();
      if (std::abs(center.x - current_center.x) < 120.0) {
        candidates.emplace_back(center.y, slider);
      }
    }
    std::ranges::sort(candidates, {}, &std::pair<double, Slider*>::first);
    const auto it = std::ranges::find_if(
        candidates, [this](const auto& item) { return item.second == this; });
    if (it == candidates.end()) {
      return;
    }
    const auto index = static_cast<int>(it - candidates.begin());
    const auto next = std::clamp(index + direction, 0,
                                 static_cast<int>(candidates.size()) - 1);
    if (next != index) {
      current_frame->setFocusView(candidates[next].second);
    }
  }

  inline static std::vector<Slider*> instances_{};
  std::string units_;
  CFontRef font_ref_;
  CFontRef value_font_ref_;
  std::string title_;
  std::vector<std::string> value_labels_;
  int precision_;
  float fine_wheel_inc_ = 0.1f;
  bool enabled_ = true;
  bool focused_ = false;
  std::function<void()> focus_changed_action_;
  std::function<void()> drag_finished_action_;
  float keyboard_inc_ = 0.0f;
  float keyboard_fine_inc_ = 0.1f;
  bool fine_adjustment_ = false;
  bool wheel_editing_enabled_ = false;
  bool wheel_editing_ = false;
  bool mouse_drag_editing_ = false;
};

class FileSelector : public CTextLabel {
 public:
  explicit FileSelector(const CRect& size,
                        IControlListener* listener_ = nullptr, int32_t tag_ = 0,
                        CBitmap* background = nullptr)
      : CTextLabel(size, "", background) {
    setTag(tag_);
    setListener(listener_);
  }
  explicit FileSelector(const CRect& size, const UTF8String& text = "")
      : CTextLabel(size, text) {}

  auto onMouseDown(CPoint& where, const CButtonState& buttons)
      -> CMouseEventResult override {
    if (buttons.isLeftButton()) {
      pressed_ = true;
      invalid();
      if (auto* const frame = getFrame()) {
        frame->doAfterEventProcessing([self = VSTGUI::shared(this)]() {
          self->pressed_ = false;
          self->invalid();
        });
      }
      auto* const selector =
          CNewFileSelector::create(getFrame(), CNewFileSelector::kSelectFile);
      if (selector) {
        selector->addFileExtension(CFileExtension("TOML", "toml"));
        selector->run(
            [self = VSTGUI::shared(this)](CNewFileSelector* sender) -> void {
              self->notify(sender, CNewFileSelector::kSelectEndMessage);
            });
        selector->forget();
      }
      return VSTGUI::kMouseEventHandled;
    }
    return CTextLabel::onMouseDown(where, buttons);
  }

  void draw(CDrawContext* const context) override {
    CTextLabel::draw(context);
    if (pressed_) {
      context->setFillColor(theme::kSliderHoverOverlay);
      context->drawRect(getViewSize(), VSTGUI::kDrawFilled);
    }
  }

  auto notify(CBaseObject* sender, const char* message)
      -> CMessageResult override {
    if (std::strcmp(message, CNewFileSelector::kSelectEndMessage) == 0) {
      // ファイルパスを取得
      auto* const selector = static_cast<CNewFileSelector*>(sender);
      if (const auto* const file_char = selector->getSelectedFile(0)) {
        const auto file_u8string =
            std::u8string(file_char, file_char + std::strlen(file_char));
        const auto file = std::filesystem::path(file_u8string);
        if (std::filesystem::exists(file) &&
            std::filesystem::is_regular_file(file)) {
          SetPath(file);
          // Editor に通知
          valueChanged();
        }
      }
      return kMessageNotified;
    }
    return CTextLabel::notify(sender, message);
  }

  void SetPath(const std::filesystem::path& file) {
    if (file == file_) {
      return;
    }
    file_ = file;
  }

  [[nodiscard]] auto GetPath() const -> const std::filesystem::path& {
    return file_;
  }

 private:
  std::filesystem::path file_;
  bool pressed_ = false;
};

}  // namespace beatrice::vst

#endif  // BEATRICE_VST_CONTROLS_H_
