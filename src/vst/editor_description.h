// Copyright (c) 2024-2026 Project Beatrice and Contributors

#ifndef BEATRICE_VST_EDITOR_DESCRIPTION_H_
#define BEATRICE_VST_EDITOR_DESCRIPTION_H_

#include <algorithm>
#include <cstddef>
#include <functional>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "vst3sdk/vstgui4/vstgui/lib/cdrawdefs.h"
#include "vst3sdk/vstgui4/vstgui/lib/controls/cparamdisplay.h"
#include "vst3sdk/vstgui4/vstgui/lib/controls/cscrollbar.h"
#include "vst3sdk/vstgui4/vstgui/lib/controls/ctextlabel.h"
#include "vst3sdk/vstgui4/vstgui/lib/cscrollview.h"
#include "vst3sdk/vstgui4/vstgui/lib/cview.h"
#include "vst3sdk/vstgui4/vstgui/lib/cviewcontainer.h"
#include "vst3sdk/vstgui4/vstgui/lib/events.h"
#include "vst3sdk/vstgui4/vstgui/lib/vstguifwd.h"

// Beatrice
#include "vst/description_text_layout.h"
#include "vst/editor_layout.h"
#include "vst/editor_theme.h"
#include "vst/surface_texture.h"

namespace beatrice::vst {

using VSTGUI::CFontRef;
using VSTGUI::CHoriTxtAlign;
using VSTGUI::CMultiLineTextLabel;
using VSTGUI::CParamDisplay;
using VSTGUI::CScrollbar;
using VSTGUI::CScrollView;
using VSTGUI::CTextLabel;
using VSTGUI::CView;
using VSTGUI::kDrawFilled;

// Stable semantic identity for each description source.
enum class DescriptionTarget { kPortrait, kModel, kVoice };

class DescriptionTextLabel final : public CMultiLineTextLabel {
 public:
  using OpenUrlAction = std::function<void(const std::u8string&)>;
  using NonLinkClickAction = std::function<void()>;

  // DESCRIPTION 本文と URL のクリック処理を持つラベルを生成する。
  DescriptionTextLabel(const CRect& rect, OpenUrlAction open_url);

  // 折り返し済み本文と URL の表示範囲を設定する。
  void SetDescriptionLayout(DescriptionTextLayout layout);

  // 本文を描画した後、URL の下線を描画する。
  void drawRect(CDrawContext* context, const CRect& update_rect) override;

  // URL 上で左ボタンが押されたことを記録する。
  void onMouseDownEvent(VSTGUI::MouseDownEvent& event) override;

  // マウス位置に応じて URL 用カーソルを更新する。
  void onMouseMoveEvent(VSTGUI::MouseMoveEvent& event) override;

  // 同じ URL 上で左ボタンが離された場合に URL を開く。
  void onMouseUpEvent(VSTGUI::MouseUpEvent& event) override;

  // クリック追跡が中断された場合に押下状態を解除する。
  void onMouseCancelEvent(VSTGUI::MouseCancelEvent& event) override;

  // ラベルからマウスが出た場合にカーソルを戻す。
  void onMouseExitEvent(VSTGUI::MouseExitEvent& event) override;

  // Handle non-link clicks explicitly so an enclosing CScrollView cannot
  // swallow the event before the description pane receives it.
  void SetNonLinkClickAction(NonLinkClickAction action) {
    non_link_click_ = std::move(action);
  }

 private:
  struct LinkArea {
    CRect rect;
    std::size_t link_index = 0;
  };

  // URL の表示範囲を文字幅に対応するクリック領域へ変換する。
  void RebuildLinkAreas();

  // 指定位置にある URL の添字を返す。
  [[nodiscard]] auto LinkAt(const CPoint& position) const
      -> std::optional<std::size_t>;

  // URL 上かどうかに応じてカーソルを切り替える。
  void UpdateCursor(bool over_link);

  DescriptionTextLayout layout_;
  std::vector<LinkArea> link_areas_;
  std::optional<std::size_t> pressed_link_;
  OpenUrlAction open_url_;
  NonLinkClickAction non_link_click_;
};

// A transparent body hit target kept below the scroll view. If the text or
// scrollbar does not consume a click, this lets DescriptionPane handle it
// without changing the normal scroll/link behavior.
class DescriptionClickView final : public CView {
 public:
  using ClickAction = std::function<void()>;

  DescriptionClickView(const CRect& rect, ClickAction action)
      : CView(rect), action_(std::move(action)) {
    setMouseEnabled(true);
    setMouseableArea(rect);
    setTransparency(true);
  }

  void onMouseDownEvent(VSTGUI::MouseDownEvent& event) override {
    if (event.buttonState.isLeft() && action_) {
      action_();
      event.consumed = true;
      event.ignoreFollowUpMoveAndUpEvents(true);
      return;
    }
    CView::onMouseDownEvent(event);
  }

 private:
  ClickAction action_;
};

// OS の文字処理で DESCRIPTION 本文を折り返し、スクロール領域へ設定する。
void SetScrollableDescription(CScrollView* scroll, DescriptionTextLabel* label,
                              const std::u8string& text);

// DESCRIPTION のスクロールバーへ共通の配色を適用する。
inline void ApplyScrollbarTheme(CScrollView* const scroll,
                                const CCoord min_scroller_length = 18.0) {
  if (!scroll) {
    return;
  }
  // 1 本のスクロールバーへ配色と最小寸法を設定する。
  const auto apply = [min_scroller_length](CScrollbar* const bar) -> void {
    if (!bar) {
      return;
    }
    bar->setBackgroundColor(theme::kScrollbarBackground);
    bar->setFrameColor(theme::kScrollbarFrame);
    bar->setScrollerColor(theme::kScrollbarThumb);
    bar->setMinScrollerLength(min_scroller_length);
    bar->setDirty();
  };
  apply(scroll->getVerticalScrollbar());
  apply(scroll->getHorizontalScrollbar());
}

class DimOverlayView final : public CView {
 public:
  // 入力を受け取らない背景暗幕を生成する。
  explicit DimOverlayView(const CRect& rect) : CView(rect) {
    setMouseEnabled(false);
  }

  // ポップアップ背面の半透明な暗幕を描画する。
  void draw(CDrawContext* const context) override {
    const auto rect = getViewSize();
    context->saveGlobalState();
    context->setDrawMode(kAntiAliasing);
    context->setFillColor(theme::kDescriptionOverlay);
    context->drawRect(rect, kDrawFilled);
    context->restoreGlobalState();
    setDirty(false);
  }
};

class DescriptionPane final : public SurfacePanel {
 public:
  using ExpandAction = std::function<void(
      DescriptionTarget target, const char* title, const std::u8string& text,
      CRect target_rect)>;

  // タイトルとスクロール本文を持つ DESCRIPTION 欄を生成する。
  DescriptionPane(const CRect& rect,
                  const SharedPointer<SurfaceBitmap>& texture,
                  const CColor& border, CCoord radius, std::string title,
                  const CRect& title_rect, const CRect& scroll_rect,
                  CFontRef title_font, CFontRef body_font,
                  const CColor& title_color, const CColor& body_color,
                  const bool frame_body, const DescriptionTarget target,
                  CRect target_rect, ExpandAction expand_action,
                  DescriptionTextLabel::OpenUrlAction open_url)
      : SurfacePanel(rect, texture, border, radius),
        title_(std::move(title)),
        target_(target),
        target_rect_(target_rect),
        expand_action_(std::move(expand_action)) {
    title_label_ = new CTextLabel(title_rect, title_.c_str(), nullptr,
                                  CParamDisplay::kNoFrame);
    title_label_->setBackColor(kTransparentCColor);
    title_label_->setTransparency(true);
    title_label_->setFont(title_font);
    title_label_->setFontColor(title_color);
    title_label_->setHoriAlign(CHoriTxtAlign::kLeftText);
    addView(title_label_);

    // Only normal description cards own a text-body frame. The morph-control
    // container reuses this class for focus and layout, but has no description
    // body and must never draw a stray frame.
    if (frame_body) {
      auto* const body_frame = new SurfacePanel(
          scroll_rect, texture, theme::kDescriptionBorder, 2.0);
      body_frame->setMouseEnabled(false);
      addView(body_frame);
    }

    expand_click_view_ = new DescriptionClickView(scroll_rect, [this]() {
      if (expand_action_) {
        expand_action_(target_, title_.c_str(), text_, target_rect_);
      }
    });
    addView(expand_click_view_);

    scroll_ = new CScrollView(
        scroll_rect,
        CRect(0, 0, scroll_rect.getWidth(), scroll_rect.getHeight()),
        CScrollView::kVerticalScrollbar | CScrollView::kDontDrawFrame |
            CScrollView::kAutoHideScrollbars,
        layout::kVerticalScrollbarWidth);
    ApplyScrollbarTheme(scroll_);
    scroll_->setBackgroundColor(kTransparentCColor);
    scroll_->setTransparency(true);
    addView(scroll_);

    const auto label_width = std::max(
        20.0, scroll_rect.getWidth() - layout::kVerticalScrollbarWidth -
                  layout::kScrollbarContentGap);
    label_ = new DescriptionTextLabel(
        CRect(0, 0, label_width, scroll_rect.getHeight()), std::move(open_url));
    label_->setFont(body_font);
    label_->setFontColor(body_color);
    label_->setBackColor(kTransparentCColor);
    label_->setStyle(CParamDisplay::kNoFrame);
    label_->setHoriAlign(CHoriTxtAlign::kLeftText);
    label_->setLineLayout(CMultiLineTextLabel::LineLayout::clip);
    label_->SetNonLinkClickAction([this]() {
      if (expand_action_) {
        expand_action_(target_, title_.c_str(), text_, target_rect_);
      }
    });
    scroll_->addView(label_);
  }

  // DESCRIPTION 欄の左クリック時に拡大表示処理を呼ぶ。
  void onMouseDownEvent(VSTGUI::MouseDownEvent& event) override {
    SurfacePanel::onMouseDownEvent(event);
    if (!event.consumed && event.buttonState.isLeft()) {
      if (expand_action_) {
        expand_action_(target_, title_.c_str(), text_, target_rect_);
      }
      event.consumed = true;
      event.ignoreFollowUpMoveAndUpEvents(true);
    }
  }

  // DESCRIPTION のタイトルを更新する。
  void SetTitle(std::string title) {
    title_ = std::move(title);
    title_label_->setText(title_.c_str());
    title_label_->setDirty();
  }

  // 本文が変わった場合だけ折り返しを更新する。
  void SetText(const std::u8string& text) {
    if (text_ == text) {
      return;
    }
    text_ = text;
    SetScrollableDescription(scroll_, label_, text_);
    invalid();
  }

  // DESCRIPTION 本文とスクロール領域の表示状態を切り替える。
  void SetBodyVisible(const bool visible) {
    body_visible_ = visible;
    label_->setVisible(visible);
    label_->setDirty();
    scroll_->setVisible(visible);
    scroll_->setDirty();
    expand_click_view_->setVisible(visible);
    expand_click_view_->setDirty();
  }

  void SetTitleAlignment(const CHoriTxtAlign alignment) {
    title_label_->setHoriAlign(alignment);
    title_label_->setDirty();
  }

  void Expand() {
    if (expand_action_) {
      expand_action_(target_, title_.c_str(), text_, target_rect_);
    }
  }

 private:
  std::string title_;
  std::u8string text_;
  DescriptionTarget target_;
  CRect target_rect_;
  ExpandAction expand_action_;
  CTextLabel* title_label_ = nullptr;
  DescriptionClickView* expand_click_view_ = nullptr;
  CScrollView* scroll_ = nullptr;
  DescriptionTextLabel* label_ = nullptr;
  bool body_visible_ = true;
};

class DescriptionPopupView final : public CViewContainer {
 public:
  // DESCRIPTION の拡大表示に使うポップアップを生成する。
  DescriptionPopupView(const CRect& rect,
                       const SharedPointer<SurfaceBitmap>& panel_texture,
                       const CColor& border, CCoord radius, CFontRef title_font,
                       CFontRef body_font,
                       DescriptionTextLabel::OpenUrlAction open_url)
      : CViewContainer(rect) {
    setBackgroundColor(kTransparentCColor);
    setTransparency(true);

    panel_ = new SurfacePanel(CRect(0, 0, 1, 1), panel_texture, border, radius);
    addView(panel_);

    title_ = new CTextLabel(CRect(0, 0, 1, 1), "", nullptr,
                            CParamDisplay::kNoFrame);
    title_->setBackColor(kTransparentCColor);
    title_->setFont(title_font);
    title_->setFontColor(theme::kText);
    title_->setHoriAlign(CHoriTxtAlign::kCenterText);
    panel_->addView(title_);

    body_frame_ = new SurfacePanel(CRect(0, 0, 1, 1), panel_texture,
                                   theme::kDescriptionBorder, 2.0);
    body_frame_->setMouseEnabled(false);
    panel_->addView(body_frame_);

    scroll_ = new CScrollView(
        CRect(0, 0, 1, 1), CRect(0, 0, 1, 1),
        CScrollView::kVerticalScrollbar | CScrollView::kDontDrawFrame |
            CScrollView::kAutoHideScrollbars,
        layout::kVerticalScrollbarWidth);
    ApplyScrollbarTheme(scroll_);
    scroll_->setBackgroundColor(kTransparentCColor);
    scroll_->setTransparency(true);
    panel_->addView(scroll_);

    text_ = new DescriptionTextLabel(CRect(0, 0, 1, 1), std::move(open_url));
    text_->setFont(body_font);
    text_->setFontColor(theme::kTextSecondary);
    text_->setBackColor(kTransparentCColor);
    text_->setStyle(CParamDisplay::kNoFrame);
    text_->setHoriAlign(CHoriTxtAlign::kLeftText);
    text_->setLineLayout(CMultiLineTextLabel::LineLayout::clip);
    text_->SetNonLinkClickAction([this]() { Hide(); });
    scroll_->addView(text_);

    setVisible(false);
  }

  // パネルの外へマウスが移動した場合にポップアップを閉じる。
  void onMouseDownEvent(VSTGUI::MouseDownEvent& event) override {
    if (event.buttonState.isLeft() &&
        !panel_->getMouseableArea().pointInside(event.mousePosition)) {
      Hide();
      event.consumed = true;
      event.ignoreFollowUpMoveAndUpEvents(true);
      return;
    }
    CViewContainer::onMouseDownEvent(event);
    if (event.buttonState.isLeft() && !event.consumed) {
      Hide();
      event.consumed = true;
      event.ignoreFollowUpMoveAndUpEvents(true);
    }
  }

  // 表示領域からマウスが出た場合にポップアップを閉じる。
  // 指定したタイトルと本文でポップアップを表示する。
  void Show(const DescriptionTarget target, const char* const title,
            const std::u8string& text, const CRect& target_rect) {
    target_ = target;
    panel_->setViewSize(target_rect);
    panel_->setMouseableArea(target_rect);
    const auto size = CRect(0, 0, target_rect.getWidth(),
                            target_rect.getHeight());
    title_->setViewSize(CRect(layout::kPanelContentInset,
                              layout::kDescriptionTitleTop,
                              size.getWidth() - layout::kPanelContentInset,
                              layout::kDescriptionTitleBottom));
    title_->setMouseableArea(title_->getViewSize());
    title_->setText(title);
    const auto scroll_rect = CRect(
        layout::kPanelContentInset, layout::kDescriptionBodyTop,
        size.getWidth() - layout::kPanelContentInset,
        size.getHeight() -
            (layout::kDescriptionPaneHeight - layout::kDescriptionBodyBottom));
    body_frame_->setViewSize(scroll_rect);
    body_frame_->setMouseableArea(scroll_rect);
    scroll_->setViewSize(scroll_rect);
    scroll_->setMouseableArea(scroll_rect);
    const auto content_width = std::max(
        20.0, scroll_rect.getWidth() - layout::kVerticalScrollbarWidth -
                  layout::kScrollbarContentGap);
    text_->setViewSize(CRect(0, 0, content_width, scroll_rect.getHeight()));
    text_->setMouseableArea(text_->getViewSize());
    SetScrollableDescription(scroll_, text_, text);
    setVisible(true);
    invalid();
  }

  [[nodiscard]] auto IsShowing(const DescriptionTarget target) const -> bool {
    return isVisible() && target_ == target;
  }

  // DESCRIPTION ポップアップを非表示にする。
  void Hide() {
    setVisible(false);
    invalid();
  }

 private:
  SurfacePanel* panel_ = nullptr;
  SurfacePanel* body_frame_ = nullptr;
  CTextLabel* title_ = nullptr;
  CScrollView* scroll_ = nullptr;
  DescriptionTextLabel* text_ = nullptr;
  DescriptionTarget target_ = DescriptionTarget::kModel;
};

}  // namespace beatrice::vst

#endif  // BEATRICE_VST_EDITOR_DESCRIPTION_H_
