// Copyright (c) 2024-2026 Project Beatrice and Contributors

#ifndef BEATRICE_VST_EDITOR_VOICE_SELECTOR_H_
#define BEATRICE_VST_EDITOR_VOICE_SELECTOR_H_

#include <algorithm>
#include <functional>
#include <map>
#include <optional>
#include <string>
#include <utility>

#include "vst3sdk/vstgui4/vstgui/lib/cscrollview.h"
#include "vst3sdk/vstgui4/vstgui/lib/cview.h"
#include "vst3sdk/vstgui4/vstgui/lib/cviewcontainer.h"
#include "vst3sdk/vstgui4/vstgui/lib/vstguifwd.h"

// Beatrice
#include "common/model_config.h"
#include "vst/controls.h"
#include "vst/editor_description.h"
#include "vst/editor_layout.h"
#include "vst/editor_theme.h"
#include "vst/editor_views.h"
#include "vst/surface_texture.h"

namespace beatrice::vst {

namespace voice_selector_detail {

[[nodiscard]] inline auto GetVoiceDisplayName(
    const common::ModelConfig::Voice& voice, const int voice_id)
    -> std::string {
  if (voice.name.empty()) {
    return "Voice " + std::to_string(voice_id + 1);
  }
  return {reinterpret_cast<const char*>(voice.name.data()), voice.name.size()};
}

}  // namespace voice_selector_detail

class VoiceSelectorView final : public CViewContainer {
 public:
  using ThumbnailMap = std::map<std::u8string, SharedPointer<CBitmap>>;
  using ToggleMenuAction = std::function<void()>;

  VoiceSelectorView(const CRect& rect, ToggleMenuAction toggle_menu_action,
                    CFontRef font_bold)
      : CViewContainer(rect),
        toggle_menu_action_(std::move(toggle_menu_action)) {
    setBackgroundColor(kTransparentCColor);
    setTransparency(true);

    portrait_ = new CView(layout::kVoiceSelectorIconRect);
    portrait_->setMouseEnabled(false);
    portrait_->setVisible(false);
    addView(portrait_);

    morph_icon_ = new MorphSelectorIconView(layout::kVoiceSelectorIconRect);
    morph_icon_->setMouseEnabled(false);
    morph_icon_->setVisible(false);
    addView(morph_icon_);

    name_ = new CTextLabel(layout::kVoiceSelectorNameRect, "", nullptr,
                           CParamDisplay::kNoFrame);
    name_->setBackColor(kTransparentCColor);
    name_->setFont(font_bold);
    name_->setFontColor(theme::kTextPrimary);
    name_->setHoriAlign(CHoriTxtAlign::kLeftText);
    name_->setMouseEnabled(false);
    addView(name_);
  }

  auto onMouseDown(CPoint& where, const CButtonState& buttons)
      -> CMouseEventResult override {
    if (buttons.isLeftButton() && toggle_menu_action_) {
      toggle_menu_action_();
      return VSTGUI::kMouseEventHandled;
    }
    return CViewContainer::onMouseDown(where, buttons);
  }

  void draw(CDrawContext* const context) override {
    CViewContainer::draw(context);
    context->setFrameColor(theme::kPanelBorder);
    context->setLineWidth(1.0);
    context->drawRect(getViewSize(), VSTGUI::kDrawStroked);
  }

  void SetDisplay(const std::optional<common::ModelConfig>& model_config,
                  const ThumbnailMap& thumbnails, const int voice_id) {
    if (!name_ || !portrait_ || !morph_icon_) {
      return;
    }

    thumbnail_ = nullptr;
    portrait_->setBackground(nullptr);
    portrait_->setVisible(false);
    morph_icon_->setVisible(false);

    if (!model_config.has_value()) {
      name_->setText("");
      SetDisplayDirty();
      return;
    }

    const auto voice_count = common::GetVoiceCount(*model_config);
    if (voice_id < 0 || voice_id >= voice_count) {
      name_->setText("Voice Morphing Mode");
      if (voice_id < -1) {
        morph_icon_->setVisible(true);
      }
      SetDisplayDirty();
      return;
    }

    const auto& voice = model_config->voices[voice_id];
    const auto display_name =
        voice_selector_detail::GetVoiceDisplayName(voice, voice_id);
    name_->setText(display_name.c_str());
    if (const auto it = thumbnails.find(voice.portrait.path);
        it != thumbnails.end() && it->second) {
      thumbnail_ = it->second;
      portrait_->setBackground(thumbnail_.get());
      portrait_->setVisible(true);
    }
    SetDisplayDirty();
  }

 private:
  void SetDisplayDirty() {
    name_->setDirty();
    portrait_->setDirty();
    morph_icon_->setDirty();
  }

  ToggleMenuAction toggle_menu_action_;
  CView* portrait_ = nullptr;
  CView* morph_icon_ = nullptr;
  CTextLabel* name_ = nullptr;
  SharedPointer<CBitmap> thumbnail_;
};

class VoiceMenuOverlayView final : public CViewContainer {
 public:
  using ThumbnailMap = std::map<std::u8string, SharedPointer<CBitmap>>;
  using SelectVoiceAction = std::function<void(int)>;

  VoiceMenuOverlayView(const CRect& rect,
                       const SharedPointer<SurfaceBitmap>& panel_surface,
                       CFontRef font, SelectVoiceAction select_voice_action)
      : CViewContainer(rect),
        font_(font),
        select_voice_action_(std::move(select_voice_action)) {
    setBackgroundColor(kTransparentCColor);

    auto* const dismiss_overlay =
        new DismissOverlayView(CRect(0, 0, rect.getWidth(), rect.getHeight()),
                               [this]() -> void { HideMenu(); });
    addView(dismiss_overlay);

    const auto voice_column_left =
        layout::kColumnOuterMargin + layout::kColumnWidth + layout::kColumnGap;
    menu_panel_ = new SurfacePanel(
        CRect(voice_column_left, layout::kVoiceMenuTop,
              voice_column_left + layout::kColumnWidth,
              layout::kVoiceMenuBottom),
        panel_surface, theme::kPanelBorder, 2.0);
    addView(menu_panel_);

    const auto viewport_width =
        layout::kColumnWidth - 2.0 * layout::kVoiceMenuInset;
    menu_scroll_ = new VSTGUI::CScrollView(
        CRect(layout::kVoiceMenuInset, layout::kVoiceMenuInset,
              layout::kVoiceMenuInset + viewport_width, 60),
        CRect(0, 0, viewport_width, layout::kVoiceMenuItemHeight),
        VSTGUI::CScrollView::kVerticalScrollbar |
            VSTGUI::CScrollView::kDontDrawFrame |
            VSTGUI::CScrollView::kOverlayScrollbars |
            VSTGUI::CScrollView::kAutoHideScrollbars,
        layout::kVerticalScrollbarWidth);
    ApplyScrollbarTheme(menu_scroll_, layout::kVoiceMenuMinScrollerLength);
    menu_scroll_->setBackgroundColor(kTransparentCColor);
    menu_scroll_->setTransparency(true);
    menu_panel_->addView(menu_scroll_);

    setVisible(false);
  }

  void ToggleMenu(const std::optional<common::ModelConfig>& model_config,
                  const ThumbnailMap& thumbnails, const int selected_voice_id) {
    if (isVisible()) {
      HideMenu();
      return;
    }
    if (!BuildMenu(model_config, thumbnails, selected_voice_id)) {
      return;
    }
    setVisible(true);
  }

  void HideMenu() {
    if (isVisible() && menu_scroll_) {
      remembered_scroll_offset_ = menu_scroll_->getScrollOffset();
    }
    setVisible(false);
  }

  void ClearRememberedScroll() { remembered_scroll_offset_ = CPoint{}; }

  void RebuildMenu(const std::optional<common::ModelConfig>& model_config,
                   const ThumbnailMap& thumbnails,
                   const int selected_voice_id) {
    static_cast<void>(BuildMenu(model_config, thumbnails, selected_voice_id));
  }

  [[nodiscard]] auto IsMenuVisible() const -> bool { return isVisible(); }

 private:
  auto BuildMenu(const std::optional<common::ModelConfig>& model_config,
                 const ThumbnailMap& thumbnails, const int selected_voice_id)
      -> bool {
    if (!menu_panel_ || !menu_scroll_) {
      return false;
    }
    if (isVisible()) {
      remembered_scroll_offset_ = menu_scroll_->getScrollOffset();
    }
    menu_scroll_->resetScrollOffset();
    menu_scroll_->removeAll(true);
    if (!model_config.has_value()) {
      HideMenu();
      return false;
    }

    const auto voice_count = common::GetVoiceCount(*model_config);
    const auto has_morph = voice_count > 1;
    const auto entry_count = voice_count + (has_morph ? 1 : 0);
    if (entry_count == 0) {
      HideMenu();
      return false;
    }

    const auto viewport_width =
        layout::kColumnWidth - 2.0 * layout::kVoiceMenuInset;
    const auto item_width = viewport_width - layout::kVerticalScrollbarWidth -
                            layout::kScrollbarContentGap;
    const auto content_height =
        entry_count * layout::kVoiceMenuItemHeight;
    const auto panel_height =
        layout::kVoiceMenuBottom - layout::kVoiceMenuTop;
    const auto viewport_height = panel_height -
                                 2.0 * layout::kVoiceMenuInset;
    const auto panel_left =
        layout::kColumnOuterMargin + layout::kColumnWidth + layout::kColumnGap;
    const auto panel_rect =
        CRect(panel_left, layout::kVoiceMenuTop,
              panel_left + layout::kColumnWidth,
              layout::kVoiceMenuBottom);
    menu_panel_->setViewSize(panel_rect);
    menu_panel_->setMouseableArea(panel_rect);
    menu_scroll_->setViewSize(
        CRect(layout::kVoiceMenuInset, layout::kVoiceMenuInset,
              layout::kVoiceMenuInset + viewport_width,
              layout::kVoiceMenuInset + viewport_height));
    menu_scroll_->setMouseableArea(menu_scroll_->getViewSize());
    menu_scroll_->setContainerSize(
        CRect(0, 0, viewport_width,
              std::max(content_height, viewport_height)));
    if (auto* const scrollbar = menu_scroll_->getVerticalScrollbar()) {
      const auto scrollable_height = content_height - viewport_height;
      const auto wheel_increment =
          scrollable_height > 0.0
              ? std::min(1.0,
                         layout::kVoiceMenuItemHeight / scrollable_height)
              : 1.0;
      scrollbar->setWheelInc(static_cast<float>(wheel_increment));
    }
    const auto add_item =
        [&](const int entry_index, const int voice_id, const std::string& label,
            SharedPointer<CBitmap> thumbnail, const bool morph_item) -> void {
      const auto y = entry_index * layout::kVoiceMenuItemHeight;
      auto* const item = new VoiceMenuItemView(
          CRect(0, y, item_width,
                y + layout::kVoiceMenuItemSurfaceHeight), label,
          std::move(thumbnail),
          morph_item, selected_voice_id == voice_id, font_,
          [this, voice_id]() -> void {
            HideMenu();
            if (select_voice_action_) {
              select_voice_action_(voice_id);
            }
          });
      menu_scroll_->addView(item);
    };

    auto entry_index = 0;
    for (auto i = 0; i < voice_count; ++i) {
      const auto& voice = model_config->voices[i];
      SharedPointer<CBitmap> thumbnail = nullptr;
      if (const auto it = thumbnails.find(voice.portrait.path);
          it != thumbnails.end() && it->second) {
        thumbnail = it->second;
      }
      add_item(entry_index++, i,
               voice_selector_detail::GetVoiceDisplayName(voice, i), thumbnail,
               false);
    }
    if (has_morph) {
      add_item(entry_index, voice_count, "Voice Morphing Mode", nullptr, true);
    }
    menu_scroll_->setScrollOffset(remembered_scroll_offset_);
    menu_scroll_->invalid();
    menu_panel_->invalid();
    return true;
  }

  CFontRef font_;
  SelectVoiceAction select_voice_action_;
  SurfacePanel* menu_panel_ = nullptr;
  VSTGUI::CScrollView* menu_scroll_ = nullptr;
  CPoint remembered_scroll_offset_;
};

}  // namespace beatrice::vst

#endif  // BEATRICE_VST_EDITOR_VOICE_SELECTOR_H_
