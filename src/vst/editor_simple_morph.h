// Copyright (c) 2024-2026 Project Beatrice and Contributors

#ifndef BEATRICE_VST_EDITOR_SIMPLE_MORPH_H_
#define BEATRICE_VST_EDITOR_SIMPLE_MORPH_H_

#include <algorithm>
#include <cmath>
#include <functional>
#include <string>
#include <utility>
#include <vector>

#include "vst3sdk/vstgui4/vstgui/lib/cscrollview.h"

#include "common/model_config.h"
#include "common/simple_morph.h"
#include "vst/controls.h"
#include "vst/control_help_tooltip.h"
#include "vst/editor_description.h"
#include "vst/editor_layout.h"
#include "vst/surface_texture.h"

namespace beatrice::vst {

class SimpleMorphPanel final : public SurfacePanel, public IControlListener {
 public:
  using WeightsChangedAction =
      std::function<void(const common::SimpleMorphWeights&, int, bool)>;

  SimpleMorphPanel(const CRect& rect,
                   const SharedPointer<SurfaceBitmap>& texture,
                   CFontRef label_font, CFontRef value_font,
                   WeightsChangedAction weights_changed,
                   std::function<void()> edit_finished,
                   std::function<void()> wheel_edited,
                   std::function<void()> focus_changed)
      : SurfacePanel(rect, texture, kTransparentCColor, 0.0),
        label_font_(label_font),
        value_font_(value_font),
        weights_changed_(std::move(weights_changed)),
        edit_finished_(std::move(edit_finished)),
        wheel_edited_(std::move(wheel_edited)),
        focus_changed_(std::move(focus_changed)) {
    scroll_ = new VSTGUI::CScrollView(
        CRect(8, 40, rect.getWidth() - 8, rect.getHeight() - 8),
        CRect(0, 0, rect.getWidth() - 16, rect.getHeight() - 48),
        VSTGUI::CScrollView::kVerticalScrollbar |
            VSTGUI::CScrollView::kDontDrawFrame |
            VSTGUI::CScrollView::kAutoHideScrollbars,
        layout::kVerticalScrollbarWidth);
    ApplyScrollbarTheme(scroll_);
    scroll_->setBackgroundColor(kTransparentCColor);
    scroll_->setTransparency(true);
    addView(scroll_);
  }

  void SetVoices(const common::ModelConfig& config,
                 common::SimpleMorphWeights weights) {
    const auto voice_count = common::GetVoiceCount(config);
    weights_ = common::NormalizeSimpleMorphWeights(weights, voice_count);
    voice_count_ = voice_count;
    scroll_->removeAll(true);
    sliders_.clear();

    constexpr auto kRowHeight = 48.0;
    const auto content_width =
        scroll_->getViewSize().getWidth() - layout::kVerticalScrollbarWidth -
        layout::kScrollbarContentGap;
    for (auto i = 0; i < voice_count_; ++i) {
      const auto& voice = config.voices[i];
      auto title = voice.name.empty()
                       ? "Voice " + std::to_string(i + 1)
                       : std::string(
                             reinterpret_cast<const char*>(voice.name.data()),
                             voice.name.size());
      auto* const background = new MonotoneBitmap(
          static_cast<int>(content_width),
          static_cast<int>(kRowHeight - 5.0), kTransparentCColor,
          kTransparentCColor);
      auto* const handle = new MonotoneBitmap(
          kSliderKnobWidth, 18, theme::kSliderHandle,
          kTransparentCColor);
      auto* const slider = new Slider(
          CRect(0, i * kRowHeight, content_width,
                i * kRowHeight + kRowHeight - 5.0),
          this, i, 0, static_cast<int>(content_width - handle->getWidth()),
          handle, background, "%", label_font_, value_font_, std::move(title),
          0);
      slider->setMin(0.0f);
      slider->setMax(100.0f);
      slider->setWheelInc(1.0f);
      slider->setFineWheelInc(1.0f);
      slider->SetWheelEditingEnabled(false);
      slider->setValue(weights_[i] * 100.0f);
      slider->SetFocusChangedAction(focus_changed_);
      slider->SetDragFinishedAction(edit_finished_);
      ApplyControlHelpTooltip(slider, ui::ControlHelpID::kMorphSlider,
                              ui::IsJapaneseEnvironment());
      scroll_->addView(slider);
      sliders_.push_back(slider);
      background->forget();
      handle->forget();
    }
    const auto content_height =
        std::max(scroll_->getViewSize().getHeight(),
                 static_cast<double>(voice_count_) * kRowHeight);
    scroll_->setContainerSize(CRect(0, 0, content_width, content_height));
    scroll_->resetScrollOffset();
    scroll_->invalid();
  }

  void SetWeights(common::SimpleMorphWeights weights) {
    weights_ = common::NormalizeSimpleMorphWeights(weights, voice_count_);
    SyncSliders();
  }

  [[nodiscard]] auto GetWeights() const -> const common::SimpleMorphWeights& {
    return weights_;
  }

  void valueChanged(CControl* control) override {
    if (!control) {
      return;
    }
    const auto index = control->getTag();
    weights_ = common::AdjustSimpleMorphWeight(
        weights_, voice_count_, index,
        static_cast<float>(std::round(control->getValue())) / 100.0f);
    SyncSliders();
    const auto* const slider = dynamic_cast<Slider*>(control);
    const auto defer_save = Slider::IsAnyMouseDragEditing() ||
                            (slider && slider->IsWheelEditing());
    if (weights_changed_) {
      weights_changed_(weights_, voice_count_, !defer_save);
    }
    if (slider && slider->IsWheelEditing() && wheel_edited_) {
      wheel_edited_();
    }
  }

 private:
  void SyncSliders() {
    for (auto i = 0; i < static_cast<int>(sliders_.size()); ++i) {
      sliders_[i]->setValue(weights_[i] * 100.0f);
      sliders_[i]->invalid();
    }
  }

  CFontRef label_font_;
  CFontRef value_font_;
  WeightsChangedAction weights_changed_;
  std::function<void()> edit_finished_;
  std::function<void()> wheel_edited_;
  std::function<void()> focus_changed_;
  VSTGUI::CScrollView* scroll_ = nullptr;
  std::vector<Slider*> sliders_;
  common::SimpleMorphWeights weights_{};
  int voice_count_ = 0;
};

}  // namespace beatrice::vst

#endif  // BEATRICE_VST_EDITOR_SIMPLE_MORPH_H_
