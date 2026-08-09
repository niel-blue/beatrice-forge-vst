// Copyright (c) 2024-2026 Project Beatrice and Contributors

#ifndef BEATRICE_VST_EDITOR_PRESET_H_
#define BEATRICE_VST_EDITOR_PRESET_H_

#include <functional>
#include <cstring>
#include <string>
#include <utility>
#include <vector>

#include "vst3sdk/vstgui4/vstgui/lib/controls/ctextedit.h"
#include "vst3sdk/vstgui4/vstgui/lib/cscrollview.h"

#include "common/preset.h"
#include "vst/controls.h"
#include "vst/control_help_tooltip.h"
#include "vst/editor_description.h"
#include "vst/editor_layout.h"
#include "vst/editor_theme.h"
#include "vst/editor_views.h"
#include "vst/surface_texture.h"

namespace beatrice::vst {

// CScrollView normally reserves its horizontal bar at the bottom. Preset
// tabs place that full-width bar above the tab row.
class PresetTabScrollView final : public VSTGUI::CScrollView {
 public:
  PresetTabScrollView(const CRect& rect, const CRect& container_size)
      : CScrollView(rect, container_size,
                    CScrollView::kHorizontalScrollbar |
                        CScrollView::kDontDrawFrame |
                        CScrollView::kAutoHideScrollbars,
                    layout::kPresetTabScrollbarHeight) {
    LayoutParts();
  }

  void setContainerSize(const CRect& size,
                        const bool keep_visible_area = false) override {
    CScrollView::setContainerSize(size, keep_visible_area);
    LayoutParts();
  }

  bool attached(CView* const parent) override {
    const auto result = CScrollView::attached(parent);
    // CScrollView performs its own scrollbar layout while attaching to a
    // freshly opened editor. Reapply our top-bar layout after that pass.
    LayoutParts();
    return result;
  }

  void setViewSize(const CRect& rect, const bool invalid = true) override {
    CScrollView::setViewSize(rect, invalid);
    LayoutParts();
  }

  void onMouseWheelEvent(VSTGUI::MouseWheelEvent& event) override {
    // A regular mouse wheel reports deltaY, while VSTGUI's horizontal
    // scrollbar only consumes deltaX. Never forward deltaY: doing so lets the
    // scroll container adjust its vertical offset while tabs move sideways.
    const auto original_delta_x = event.deltaX;
    const auto original_delta_y = event.deltaY;
    if (event.deltaX == 0.0 && event.deltaY != 0.0) {
      // Horizontal tab scrolling otherwise advances only a very small amount
      // per wheel notch. Keep it precise, but make each notch useful.
      event.deltaX = event.deltaY * 3.0;
    }
    event.deltaY = 0.0;
    CScrollView::onMouseWheelEvent(event);
    event.deltaX = original_delta_x;
    event.deltaY = original_delta_y;
  }

  void SetStableScrollRatio(const float ratio) {
    auto apply = [this](const float value) {
      if (auto* const bar = getHorizontalScrollbar()) {
        bar->setValueNormalized(value);
        bar->bounceValue();
        bar->onVisualChange();
        valueChanged(bar);
        bar->invalid();
      }
    };
    apply(ratio);
    VSTGUI::Call::later([self = VSTGUI::shared(this), ratio]() {
      self->SetStableScrollRatioImmediate(ratio);
    });
  }

 private:
  void SetStableScrollRatioImmediate(const float ratio) {
    if (auto* const bar = getHorizontalScrollbar()) {
      bar->setValueNormalized(ratio);
      bar->bounceValue();
      bar->onVisualChange();
      valueChanged(bar);
      bar->invalid();
    }
  }

  void LayoutParts() {
    const auto width = getViewSize().getWidth();
    if (auto* const bar = getHorizontalScrollbar()) {
      // CScrollView can retain the scrollbar's visible state after a previous
      // overflow. Recompute it whenever the tab container is rebuilt so two
      // tabs do not leave an unnecessary scrollbar strip behind.
      const auto content_width = getContainerSize().getWidth();
      bar->setVisible(content_width > width + 0.5);
      bar->setViewSize(
          CRect(0, 0, width, layout::kPresetTabScrollbarHeight), false);
      bar->setMouseableArea(bar->getViewSize());
    }
  }
};

// Keep a fixed client viewport for the preset rows. VSTGUI's auto-hide mode
// changes the internal grid when the vertical scrollbar first appears; that
// makes the entire list jump by a few pixels. Reserve the narrow scrollbar
// gutter at all times, then only hide/show the scrollbar control itself.
class PresetListScrollView final : public VSTGUI::CScrollView {
 public:
  PresetListScrollView(const CRect& rect, const CRect& container_size)
      : CScrollView(rect, container_size,
                    CScrollView::kVerticalScrollbar |
                        CScrollView::kDontDrawFrame,
                    layout::kVerticalScrollbarWidth) {
    UpdateScrollbarVisibility();
  }

  void setContainerSize(const CRect& size,
                        const bool keep_visible_area = false) override {
    CScrollView::setContainerSize(size, keep_visible_area);
    UpdateScrollbarVisibility();
  }

  bool attached(CView* const parent) override {
    const auto result = CScrollView::attached(parent);
    UpdateScrollbarVisibility();
    return result;
  }

  void setViewSize(const CRect& rect, const bool invalid = true) override {
    CScrollView::setViewSize(rect, invalid);
    UpdateScrollbarVisibility();
  }

  // CScrollView may normalize its offset once more when the new container is
  // laid out. Apply the requested position again after that pass so adding a
  // row never leaves the newest row underneath the viewport edge.
  void SetStableScrollOffset(const CPoint& offset) {
    setScrollOffset(offset);
    VSTGUI::Call::later([self = VSTGUI::shared(this), offset]() {
      self->setScrollOffset(offset);
      self->invalid();
    });
  }

 private:
  void UpdateScrollbarVisibility() {
    if (auto* const bar = getVerticalScrollbar()) {
      const auto viewport_height = getVisibleClientRect().getHeight();
      const auto needed =
          getContainerSize().getHeight() > viewport_height + 0.5;
      bar->setVisible(needed);
      bar->setMouseEnabled(needed);
    }
  }
};

class PresetRenameTextEdit final : public VSTGUI::CTextEdit {
 public:
 using VSTGUI::CTextEdit::CTextEdit;

 protected:
  void platformOnKeyboardEvent(VSTGUI::KeyboardEvent& event) override {
    // The Windows native text control handles its own copy/paste. Do not
    // forward these four commands to the plug-in frame, where the host can
    // interpret Ctrl+C/V as preset-state copy/paste instead.
    if (event.type == VSTGUI::EventType::KeyDown &&
        event.modifiers.is(VSTGUI::ModifierKey::Control) &&
        (event.character == 'a' || event.character == 'c' ||
         event.character == 'v' || event.character == 'x')) {
      return;
    }
    VSTGUI::CTextEdit::platformOnKeyboardEvent(event);
  }
};

class PresetPanel final : public SurfacePanel, public IControlListener {
 public:
  using IndexAction = std::function<void(int)>;
  using ReorderAction = std::function<void(int, int)>;
  using RenameAction = std::function<void(int, const std::string&)>;
  using ImportAction = std::function<void(int)>;
  using ExportAction = std::function<void(bool)>;

  PresetPanel(const CRect& rect, const SharedPointer<SurfaceBitmap>& texture,
              CFontRef font, CFontRef bold_font,
              std::function<void()> add_current, std::function<void()> new_preset,
              IndexAction apply,
              RenameAction rename, ReorderAction reorder,
              IndexAction remove, std::function<void()> add_bank,
              IndexAction select_bank, RenameAction rename_bank,
              ReorderAction reorder_bank, IndexAction delete_bank,
              ImportAction import_presets,
              ExportAction export_presets,
              std::function<void()> focus_column)
      : SurfacePanel(rect, texture, kTransparentCColor, 2.0),
        font_(font),
        bold_font_(bold_font),
        add_current_(std::move(add_current)),
        new_preset_(std::move(new_preset)),
        apply_(std::move(apply)),
        rename_(std::move(rename)),
        reorder_(std::move(reorder)),
        remove_(std::move(remove)),
        add_bank_(std::move(add_bank)),
        select_bank_(std::move(select_bank)),
        rename_bank_(std::move(rename_bank)),
        reorder_bank_(std::move(reorder_bank)),
        delete_bank_(std::move(delete_bank)),
        import_presets_(std::move(import_presets)),
        export_presets_(std::move(export_presets)),
        focus_column_(std::move(focus_column)) {
    // Stop four pixels before the add button. With 90 px tabs this leaves a
    // sliver of the next tab visible when three or more banks exist.
    bank_tabs_ = new PresetTabScrollView(
        CRect(layout::kCompactContentInset, layout::kPresetTabsTop,
              layout::kPresetTabsRight, layout::kPresetTabsBottom),
        CRect(0, 0,
              layout::kPresetTabsRight - layout::kCompactContentInset,
              layout::kSwitchHeight));
    ApplyScrollbarTheme(bank_tabs_);
    bank_tabs_->setBackgroundColor(kTransparentCColor);
    bank_tabs_->setTransparency(true);
    // The extra six pixels are reserved internally by CScrollView for its
    // horizontal bar. Keep that empty strip out of hit-testing so the preset
    // list can begin at y=82.5 without input overlap.
    bank_tabs_->setMouseableArea(layout::PresetTabHitRect());
    addView(bank_tabs_);
    addView(MakeAction(CRect(layout::kPresetAddLeft,
                             layout::kPresetTabActionTop,
                             layout::kPresetAddRight,
                             layout::kPresetTabActionBottom), "", [this]() {
      if (add_bank_) {
        add_bank_();
      }
    }, ActionIcon::kPlus, true, ui::ControlHelpID::kPresetBankAdd));
    delete_bank_button_ = MakeAction(
        CRect(layout::kPresetDeleteLeft, layout::kPresetTabActionTop,
              layout::kCompactRight, layout::kPresetTabActionBottom), "", [this]() {
          if (banks_.size() <= 1 || selected_bank_ < 0) return;
          if (delete_armed_bank_ != selected_bank_) {
            delete_armed_bank_ = selected_bank_;
            delete_bank_button_->setBackColor(theme::kDeleteConfirm);
            delete_bank_button_->invalid();
            return;
          }
          const auto index = selected_bank_;
          delete_armed_bank_ = -1;
          if (delete_bank_) delete_bank_(index);
        }, ActionIcon::kTrash, true, ui::ControlHelpID::kPresetBankDelete);
    addView(delete_bank_button_);

    // The visible tab row ends at y=72. Keep only half of the previous 9 px
    // visual gap and give the recovered space to the preset list.
    const auto preset_list_height =
        layout::PresetListViewportHeight(rect.getHeight());
    scroll_ = new PresetListScrollView(
        CRect(layout::kCompactContentInset, layout::kPresetListTop,
              layout::kCompactRight,
              layout::kPresetListTop + preset_list_height),
        CRect(0, 0, layout::kCompactRight - layout::kCompactContentInset,
              preset_list_height));
    ApplyScrollbarTheme(scroll_);
    scroll_->setBackgroundColor(kTransparentCColor);
    scroll_->setTransparency(true);
    addView(scroll_);

    export_status_ = new CTextLabel(
        CRect(layout::kCompactContentInset, layout::kPresetListTop + 4.0,
              layout::kCompactRight, layout::kPresetListTop + 28.0),
        "EXPORT FAILED", nullptr, CParamDisplay::kNoFrame);
    export_status_->setFont(bold_font_);
    export_status_->setFontColor(theme::kTextPrimary);
    export_status_->setBackColor(theme::kDeleteConfirm);
    export_status_->setHoriAlign(CHoriTxtAlign::kCenterText);
    export_status_->setVisible(false);
    addView(export_status_);

    addView(MakeAction(
        layout::PresetBottomButtonRect(1, 0, rect.getHeight()),
        "SAVE PRESET", [this]() {
          if (add_current_) add_current_();
        }, ActionIcon::kSave, true, ui::ControlHelpID::kSavePreset));
    addView(MakeAction(
        layout::PresetBottomButtonRect(1, 1, rect.getHeight()),
        "NEW EMPTY", [this]() {
          if (new_preset_) new_preset_();
        }, ActionIcon::kPlus, true, ui::ControlHelpID::kNewEmptyPreset));

    addView(MakeAction(
        layout::PresetBottomButtonRect(0, 0, rect.getHeight()),
        "IMPORT LIST", [this]() {
          if (import_presets_) import_presets_(0);
        }, ActionIcon::kImport, true, ui::ControlHelpID::kImportPresetList));
    addView(MakeAction(
        layout::PresetBottomButtonRect(0, 1, rect.getHeight()),
        "EXPORT LIST", [this]() {
          if (export_presets_) export_presets_(false);
        }, ActionIcon::kExport, true, ui::ControlHelpID::kExportPresetList));
  }

  void SetPresets(const std::vector<common::Preset>& presets,
                  const int selected = -1,
                  std::vector<SharedPointer<CBitmap>> thumbnails = {}) {
    presets_ = presets;
    if (delete_armed_preset_ >= static_cast<int>(presets_.size())) {
      delete_armed_preset_ = -1;
    }
    thumbnails_ = std::move(thumbnails);
    const auto next_selected =
        selected >= 0 && selected < static_cast<int>(presets_.size())
            ? selected
            : -1;
    const auto selection_changed = next_selected != selected_;
    selected_ = next_selected;
    editing_ = -1;
    Rebuild(selection_changed);
  }

  void SetExportFailed(const bool failed) {
    if (!export_status_) return;
    export_status_->setVisible(failed);
    export_status_->invalid();
  }

  // A newly created bank is appended after the currently visible tabs. Keep
  // the requested reveal separate from ordinary tab selection so selecting an
  // existing tab never changes the user's horizontal scroll position.
  void RevealNewBankOnNextRebuild() { reveal_new_bank_on_next_rebuild_ = true; }

  void SetBanks(const std::vector<common::PresetBank>& banks,
                const int selected_bank) {
    banks_ = banks;
    selected_bank_ = selected_bank;
    delete_armed_bank_ = -1;
    if (delete_bank_button_) {
      // The bank delete control is an operation button, just like the add
      // control beside it. Keep its idle color in the same role category.
      delete_bank_button_->setBackColor(theme::kActionButton);
      delete_bank_button_->invalid();
    }
    RebuildBankTabs();
  }

  void valueChanged(CControl* control) override {
    auto* const edit = dynamic_cast<VSTGUI::CTextEdit*>(control);
    const auto index = control ? control->getTag() : -1;
    if (edit && index >= 0 && index < static_cast<int>(presets_.size()) &&
        rename_) {
      editing_ = -1;
      const auto name = std::string(edit->getText().getString());
      if (auto* const frame = getFrame()) {
        frame->doAfterEventProcessing(
            [this, index, name]() { rename_(index, name); });
      }
      return;
    }
    if (edit && index <= -1000 && rename_bank_) {
      const auto bank_index = -1000 - index;
      editing_bank_ = -1;
      const auto name = std::string(edit->getText().getString());
      if (auto* const frame = getFrame()) {
        frame->doAfterEventProcessing(
            [this, bank_index, name]() { rename_bank_(bank_index, name); });
      }
    }
  }

  CMessageResult notify(CBaseObject* sender,
                        VSTGUI::IdStringPtr message) override {
    if (message == VSTGUI::kMsgLooseFocus &&
        dynamic_cast<VSTGUI::CTextEdit*>(sender)) {
      editing_ = -1;
      editing_bank_ = -1;
      if (auto* const frame = getFrame()) {
        frame->doAfterEventProcessing([this]() {
          Rebuild();
          RebuildBankTabs();
        });
      }
      return kMessageNotified;
    }
    return SurfacePanel::notify(sender, message);
  }

  bool wantsFocus() const override { return true; }

  void onKeyboardEvent(VSTGUI::KeyboardEvent& event) override {
    if (event.type != VSTGUI::EventType::KeyDown || presets_.empty()) {
      SurfacePanel::onKeyboardEvent(event);
      return;
    }
    auto next = selected_;
    if (event.virt == VSTGUI::VirtualKey::Up) {
      next = std::max(0, selected_ - 1);
    } else if (event.virt == VSTGUI::VirtualKey::Down) {
      next = std::min(static_cast<int>(presets_.size()) - 1, selected_ + 1);
    } else {
      SurfacePanel::onKeyboardEvent(event);
      return;
    }
    if (next != selected_ && apply_) {
      selected_ = next;
      apply_(next);
    }
    event.consumed = true;
  }

 private:
  auto MakeAction(const CRect& rect, const char* text,
                  std::function<void()> action,
                  const ActionIcon icon = ActionIcon::kNone,
                  const bool emphasized = false,
                  const ui::ControlHelpID help_id = ui::ControlHelpID::kNone)
      -> GlowingActionLabel* {
    auto* label =
        new GlowingActionLabel(rect, text,
                               [this, action = std::move(action)]() {
                                 if (focus_column_) {
                                   focus_column_();
                                 }
                                 if (auto* const frame = getFrame()) {
                                   frame->doAfterEventProcessing(action);
                                 } else {
                                   action();
                                 }
                               });
    label->setBackColor(emphasized ? theme::kActionButton
                                   : theme::kPresetTabUnselected);
    label->setFont(font_);
    label->SetStateFonts(font_, bold_font_);
    label->setFontColor(theme::kAccent);
    label->setHoriAlign(CHoriTxtAlign::kCenterText);
    label->setStyle(CParamDisplay::kNoFrame);
    label->SetIcon(icon);
    ApplyControlHelpTooltip(label, help_id, ui::IsJapaneseEnvironment());
    return label;
  }

  void Rebuild(const bool reveal_selected = false) {
    const auto previous_offset = scroll_->getScrollOffset();
    // CScrollView applies its offset directly to child rectangles. New
    // children are created in local coordinates, so clear the old offset
    // before removing/recreating rows or they can appear shifted/overlapped.
    scroll_->resetScrollOffset();
    scroll_->removeAll(true);
    constexpr auto kRowHeight = layout::kPresetRowHeight;
    const auto row_width = scroll_->getViewSize().getWidth();
    for (auto i = 0; i < static_cast<int>(presets_.size()); ++i) {
      if (i == editing_) {
        auto* const edit = new PresetRenameTextEdit(
            CRect(0, i * kRowHeight, row_width,
                  i * kRowHeight + kRowHeight - layout::kPresetRowBottomGap),
            this, i, presets_[i].name.c_str(), nullptr,
            CParamDisplay::kNoFrame);
        edit->setBackColor(theme::kPresetTabSelected);
        edit->setFont(bold_font_);
        edit->setFontColor(theme::kTextPrimary);
        edit->setHoriAlign(CHoriTxtAlign::kCenterText);
        scroll_->addView(edit);
        VSTGUI::Call::later([self = VSTGUI::shared(edit)]() {
          if (auto* const frame = self->getFrame()) {
            frame->setFocusView(self);
            // Explicitly enter the platform text-edit state.  Without this
            // call a newly rebuilt rename row can still leave keyboard
            // shortcuts routed to the host/plugin instead of the text field.
            self->takeFocus();
          }
        });
        continue;
      }
      const auto delete_armed = delete_armed_preset_ == i;
      auto* const row = MakeAction(
          CRect(0, i * kRowHeight, row_width - layout::kPresetRowDeleteWidth,
                i * kRowHeight + kRowHeight - layout::kPresetRowBottomGap),
          presets_[i].name.c_str(), [this, i]() {
            const auto was_armed = delete_armed_preset_ != -1;
            if (delete_armed_preset_ != i) {
              delete_armed_preset_ = -1;
            }
            selected_ = i;
            if (auto* const frame = getFrame()) {
              frame->setFocusView(this);
            }
            // MakeAction already invokes this after the mouse event. Applying
            // another deferral here leaves the selection pending until the
            // next UI event and can overwrite a subsequent slider change.
            if (apply_ && i >= 0 && i < static_cast<int>(presets_.size())) {
              apply_(i);
            }
            if (was_armed) {
              // This action is already dispatched after mouse processing by
              // MakeAction. Rebuild directly: scheduling a second deferred
              // task can leave the old blue selected overlay on screen until
              // the next host UI event.
              Rebuild();
            }
          });
      row->SetRightClickAction([this, i]() {
        delete_armed_preset_ = -1;
        editing_ = i;
        if (auto* const frame = getFrame()) {
          frame->doAfterEventProcessing([this]() { Rebuild(); });
        }
      });
      row->SetDragFinishedAction([this, i](const CCoord distance) {
        delete_armed_preset_ = -1;
        const auto offset = static_cast<int>(std::round(distance / kRowHeight));
        const auto destination = std::clamp(
            i + offset, 0, static_cast<int>(presets_.size()) - 1);
        if (destination == i || !reorder_) return;
        if (auto* const frame = getFrame()) {
          frame->doAfterEventProcessing(
              [this, i, destination]() { reorder_(i, destination); });
        } else {
          reorder_(i, destination);
        }
      });
      row->SetStateFonts(font_, bold_font_);
      row->SetActiveColor(theme::kPresetSelected);
      row->setFontColor(delete_armed || i == selected_
                            ? theme::kTextPrimary
                            : theme::kTextSecondary);
      row->setBackColor(delete_armed
                            ? theme::kDeleteConfirm
                            : (i == selected_ ? theme::kPresetSelected
                                              : theme::kPresetUnselected));
      row->setHoriAlign(CHoriTxtAlign::kCenterText);
      // Active rows draw their blue selection fill after the normal
      // background. A pending deletion must therefore be non-active, or the
      // selected overlay would hide its red confirmation state.
      row->SetActive(!delete_armed && i == selected_);
      scroll_->addView(row);
      auto* const remove = MakeAction(
          CRect(row_width - layout::kPresetRowDeleteWidth, i * kRowHeight,
                row_width,
                i * kRowHeight + kRowHeight - layout::kPresetRowBottomGap),
          "", [this, i]() {
            if (delete_armed_preset_ != i) {
              delete_armed_preset_ = i;
              // MakeAction already runs after the current mouse event.
              // Rebuild now so the red confirmation state is visible without
              // requiring another click or host redraw.
              Rebuild();
              return;
            }
            delete_armed_preset_ = -1;
            if (remove_) remove_(i);
          }, ActionIcon::kTrash);
      remove->setBackColor(delete_armed
                                ? theme::kDeleteConfirm
                                : (i == selected_ ? theme::kPresetSelected
                                                  : theme::kPresetUnselected));
      scroll_->addView(remove);
    }
    const auto content_height =
        std::max(scroll_->getViewSize().getHeight(),
                 static_cast<double>(presets_.size()) * kRowHeight);
    scroll_->setContainerSize(CRect(0, 0, row_width, content_height));
    if (reveal_selected && selected_ >= 0 &&
        selected_ < static_cast<int>(presets_.size())) {
      // The list view is rebuilt after selection changes. Explicitly place
      // the selected row inside the viewport; CScrollView does not
      // automatically reveal newly added children.
      const auto viewport_height = scroll_->getViewSize().getHeight();
      const auto max_offset = std::max(0.0, content_height - viewport_height);
      const auto row_top = selected_ * kRowHeight;
      const auto row_bottom = row_top + kRowHeight;
      const auto current_offset = std::clamp(row_top, 0.0, max_offset);
      const auto visible_bottom = current_offset + viewport_height;
      // When the selected row is the newest row, use the true maximum
      // offset. This keeps the newest preset at the bottom even when the
      // row is only partially beyond the viewport.
      const auto target_offset =
          selected_ == static_cast<int>(presets_.size()) - 1
              ? max_offset
              : (row_bottom > visible_bottom
                     ? std::min(max_offset, row_bottom - viewport_height)
                     : current_offset);
      scroll_->SetStableScrollOffset(CPoint(0, target_offset));
    } else {
      const auto max_offset = std::max(
          0.0, content_height - scroll_->getViewSize().getHeight());
      scroll_->SetStableScrollOffset(
          CPoint(0, std::clamp(previous_offset.y, 0.0, max_offset)));
    }
    scroll_->invalid();
  }

  void RebuildBankTabs() {
    if (!bank_tabs_) {
      return;
    }
    // Save only the scrollbar ratio. Resetting before adding new child views
    // is essential: otherwise fresh tabs are created at logical x=0 while
    // inheriting the old container offset, separating them from the bar.
    auto scroll_ratio = 0.0f;
    if (auto* const bar = bank_tabs_->getHorizontalScrollbar()) {
      scroll_ratio = bar->getValueNormalized();
    }
    bank_tabs_->resetScrollOffset();
    bank_tabs_->removeAll(true);
    constexpr auto kTabWidth = layout::kPresetTabWidth;
    for (auto i = 0; i < static_cast<int>(banks_.size()); ++i) {
      const auto rect = layout::PresetBankTabRect(i, kTabWidth);
      if (i == editing_bank_) {
        auto* const edit = new PresetRenameTextEdit(
            rect, this, -1000 - i, banks_[i].name.c_str(), nullptr,
            CParamDisplay::kNoFrame);
        edit->setFont(bold_font_);
        edit->setBackColor(theme::kPresetTabSelected);
        edit->setFontColor(theme::kText);
        bank_tabs_->addView(edit);
        if (auto* const frame = getFrame()) {
          frame->setFocusView(edit);
          // Keep tab-name editing on the native text editor so Ctrl+C/Ctrl+V
          // operates on the tab name rather than any host parameter data.
          edit->takeFocus();
        }
      } else {
        auto* const tab = MakeAction(rect, banks_[i].name.c_str(), [this, i]() {
          delete_armed_bank_ = -1;
          if (select_bank_) {
            select_bank_(i);
          }
        });
        tab->setBackColor(theme::kPresetTabUnselected);
        tab->SetActiveColor(theme::kPresetTabSelected);
        tab->SetRightClickAction([this, i]() {
          editing_bank_ = i;
          if (auto* const frame = getFrame()) {
            frame->doAfterEventProcessing([this]() { RebuildBankTabs(); });
          }
        });
        tab->SetHorizontalDragFinishedAction([this, i](const CCoord distance) {
          const auto offset = static_cast<int>(std::round(distance / kTabWidth));
          const auto destination = std::clamp(
              i + offset, 0, static_cast<int>(banks_.size()) - 1);
          if (destination == i || !reorder_bank_) return;
          if (auto* const frame = getFrame()) {
            frame->doAfterEventProcessing(
                [this, i, destination]() { reorder_bank_(i, destination); });
          } else {
            reorder_bank_(i, destination);
          }
        });
        tab->SetActive(i == selected_bank_);
        bank_tabs_->addView(tab);
      }
    }
    const auto width = std::max(bank_tabs_->getViewSize().getWidth(),
                                banks_.size() * kTabWidth);
    bank_tabs_->setContainerSize(
        CRect(0, 0, width, layout::kPresetTabContainerHeight), false);
    if (auto* const bar = bank_tabs_->getHorizontalScrollbar()) {
      const auto target_ratio = reveal_new_bank_on_next_rebuild_ ? 1.0f
                                                                  : scroll_ratio;
      reveal_new_bank_on_next_rebuild_ = false;
      static_cast<PresetTabScrollView*>(bank_tabs_)
          ->SetStableScrollRatio(target_ratio);
    }
    bank_tabs_->invalid();
  }

  CFontRef font_;
  CFontRef bold_font_;
  std::function<void()> add_current_;
  std::function<void()> new_preset_;
  IndexAction apply_;
  RenameAction rename_;
  ReorderAction reorder_;
  IndexAction remove_;
  std::function<void()> add_bank_;
  IndexAction select_bank_;
  RenameAction rename_bank_;
  ReorderAction reorder_bank_;
  IndexAction delete_bank_;
  ImportAction import_presets_;
  ExportAction export_presets_;
  std::function<void()> focus_column_;
  PresetListScrollView* scroll_ = nullptr;
  PresetTabScrollView* bank_tabs_ = nullptr;
  GlowingActionLabel* delete_bank_button_ = nullptr;
  CTextLabel* export_status_ = nullptr;
  std::vector<common::PresetBank> banks_;
  int selected_bank_ = 0;
  bool reveal_new_bank_on_next_rebuild_ = false;
  int editing_bank_ = -1;
  int delete_armed_bank_ = -1;
  std::vector<common::Preset> presets_;
  std::vector<SharedPointer<CBitmap>> thumbnails_;
  int selected_ = -1;
  int delete_armed_preset_ = -1;
  int editing_ = -1;
};

}  // namespace beatrice::vst

#endif  // BEATRICE_VST_EDITOR_PRESET_H_
