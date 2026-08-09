// Copyright (c) 2024-2026 Project Beatrice and Contributors

#include "vst/editor.h"

#include <algorithm>
#include <array>
#include <cassert>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <ctime>
#include <cstring>
#include <exception>
#include <filesystem>
#include <fstream>
#include <initializer_list>
#include <ios>
#include <iomanip>
#include <iterator>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <system_error>
#include <variant>
#include <vector>

#include "beatricelib/beatrice.h"
#include "toml11/single_include/toml.hpp"
#include "vst3sdk/pluginterfaces/base/fplatform.h"
#include "vst3sdk/pluginterfaces/base/fstrdefs.h"
#include "vst3sdk/pluginterfaces/base/ftypes.h"
#include "vst3sdk/pluginterfaces/base/funknown.h"
#include "vst3sdk/pluginterfaces/base/smartpointer.h"
#include "vst3sdk/pluginterfaces/gui/iplugview.h"
#include "vst3sdk/pluginterfaces/vst/vsttypes.h"
#include "vst3sdk/public.sdk/source/common/openurl.h"
#include "vst3sdk/public.sdk/source/vst/utility/stringconvert.h"
#include "vst3sdk/public.sdk/source/vst/vstguieditor.h"
#include "vst3sdk/public.sdk/source/vst/vstparameters.h"
#include "vst3sdk/vstgui4/vstgui/lib/cbitmap.h"
#include "vst3sdk/vstgui4/vstgui/lib/cbitmapfilter.h"
#include "vst3sdk/vstgui4/vstgui/lib/cfont.h"
#include "vst3sdk/vstgui4/vstgui/lib/cframe.h"
#include "vst3sdk/vstgui4/vstgui/lib/controls/coptionmenu.h"
#include "vst3sdk/vstgui4/vstgui/lib/cviewcontainer.h"
#include "vst3sdk/vstgui4/vstgui/lib/platform/platformfactory.h"
#include "vst3sdk/vstgui4/vstgui/lib/vstguibase.h"
#include "vst3sdk/vstgui4/vstgui/lib/vstguifwd.h"

// Beatrice
#include "common/error.h"
#include "common/model_config.h"
#include "common/parameter_schema.h"
#include "common/voice_morph_parameter.h"
#include "common/voice_morph_state.h"
#include "ui/control_help.h"
#include "vst/controller.h"
#include "vst/control_help_tooltip.h"
#include "vst/controls.h"
#include "vst/description_url.h"
#include "vst/editor_description.h"
#include "vst/editor_morph.h"
#include "vst/editor_morph_controller.h"
#include "vst/editor_preset.h"
#include "vst/editor_simple_morph.h"
#include "vst/editor_theme.h"
#include "vst/editor_ui_limits.h"
#include "vst/editor_views.h"
#include "vst/editor_voice_selector.h"
#include "vst/parameter.h"
#include "vst/surface_texture.h"

#ifdef BEATRICE_ONLY_FOR_LINTER_DO_NOT_COMPILE_WITH_THIS
#include "vst/metadata.h.in"
#else
#include "metadata.h"  // NOLINT(build/include_subdir)
#endif

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>  // NOLINT(misc-include-cleaner)
#else
#include <cstdlib>
#endif

namespace beatrice::vst {

namespace {

class ColumnFocusOptionMenu final : public VSTGUI::COptionMenu {
 public:
  ColumnFocusOptionMenu(const VSTGUI::CRect& size,
                        VSTGUI::IControlListener* listener, int32_t tag,
                        VSTGUI::CBitmap* background,
                        std::function<void()> focus_action)
      : VSTGUI::COptionMenu(size, listener, tag, background),
        focus_action_(std::move(focus_action)) {}

  void onMouseDownEvent(VSTGUI::MouseDownEvent& event) override {
    if (event.buttonState.isLeft() && focus_action_) {
      focus_action_();
    }
    VSTGUI::COptionMenu::onMouseDownEvent(event);
  }

 private:
  std::function<void()> focus_action_;
};

class ClickableImageView final : public VSTGUI::CView {
 public:
  ClickableImageView(const VSTGUI::CRect& size,
                     std::function<void()> click_action)
      : CView(size), click_action_(std::move(click_action)) {}

  void onMouseDownEvent(VSTGUI::MouseDownEvent& event) override {
    if (event.buttonState.isLeft() && click_action_) {
      click_action_();
      event.consumed = true;
      event.ignoreFollowUpMoveAndUpEvents(true);
      return;
    }
    CView::onMouseDownEvent(event);
  }

 private:
  std::function<void()> click_action_;
};

}  // namespace

using common::ParameterID;
using Steinberg::ViewRect;
using Steinberg::Vst::String128;
using Steinberg::Vst::StringListParameter;
using VSTGUI::CFrame;
using VSTGUI::COptionMenu;
using VSTGUI::CView;
using VSTGUI::CViewContainer;
using VSTGUI::getPlatformFactory;

// NOLINTNEXTLINE(readability-identifier-naming)
namespace BitmapFilter = VSTGUI::BitmapFilter;

namespace {

// DESCRIPTION URL を検証し、既定のブラウザで開く。
void OpenDescriptionUrl(const std::u8string& url) {
  if (!IsSafeDescriptionUrl(url)) {
    return;
  }
  static_cast<void>(Steinberg::openURLInDefaultApplication(
      reinterpret_cast<const char*>(url.c_str())));
}

auto HasEnvironmentVariable(const char* const name) -> bool {
#if defined(_WIN32)
  // NOLINTNEXTLINE(misc-include-cleaner)
  return GetEnvironmentVariableA(name, nullptr, 0) > 0;
#else
  return std::getenv(name) != nullptr;
#endif
}

auto GetScreenshotModelPath() -> std::u8string {
#if defined(_WIN32)
  const auto size =
      // NOLINTNEXTLINE(misc-include-cleaner)
      GetEnvironmentVariableW(L"BEATRICE_SCREENSHOT_MODEL_PATH", nullptr, 0);
  if (size == 0) {
    return {};
  }
  auto value = std::wstring(size, L'\0');
  const auto copied =
      // NOLINTNEXTLINE(misc-include-cleaner)
      GetEnvironmentVariableW(L"BEATRICE_SCREENSHOT_MODEL_PATH", value.data(),
                              size);
  if (copied == 0) {
    return {};
  }
  value.resize(copied);
  const auto value_utf8 =
      Steinberg::Vst::StringConvert::convert(Steinberg::wscast(value.c_str()));
#else
  const auto* const value = std::getenv("BEATRICE_SCREENSHOT_MODEL_PATH");
  if (!value || value[0] == '\0') {
    return {};
  }
  const auto value_utf8 = std::string(value);
#endif
  return {value_utf8.begin(), value_utf8.end()};
}

auto ScaleBitmap(CBitmap* const bitmap, const int width, const int height)
    -> SharedPointer<CBitmap> {
  const auto scale =
      VSTGUI::owned(BitmapFilter::Factory::getInstance().createFilter(
          BitmapFilter::Standard::kScaleBilinear));
  scale->setProperty(BitmapFilter::Standard::Property::kInputBitmap, bitmap);
  scale->setProperty(BitmapFilter::Standard::Property::kOutputRect,
                     CRect(0, 0, width, height));
  if (!scale->run()) {
    return nullptr;
  }
  auto* const scaled_bitmap_obj =
      scale->getProperty(BitmapFilter::Standard::Property::kOutputBitmap)
          .getObject();
  auto* const scaled_bitmap = dynamic_cast<CBitmap*>(scaled_bitmap_obj);
  if (!scaled_bitmap) {
    return nullptr;
  }
  return VSTGUI::shared(scaled_bitmap);
}

void ApplyRoundedMask(CBitmap* const bitmap, const double radius) {
  if (!bitmap || radius <= 0.0) {
    return;
  }
  auto access = VSTGUI::owned(CBitmapPixelAccess::create(bitmap, false));
  if (!access) {
    return;
  }
  const auto width = static_cast<double>(access->getBitmapWidth());
  const auto height = static_cast<double>(access->getBitmapHeight());
  const auto r = std::min(radius, std::min(width, height) / 2.0);
  for (uint32_t y = 0; y < access->getBitmapHeight(); ++y) {
    for (uint32_t x = 0; x < access->getBitmapWidth(); ++x) {
      const auto px = static_cast<double>(x) + 0.5;
      const auto py = static_cast<double>(y) + 0.5;
      auto coverage = 1.0;
      auto cx = px;
      auto cy = py;
      auto in_corner = false;
      if (px < r && py < r) {
        cx = r;
        cy = r;
        in_corner = true;
      } else if (px > width - r && py < r) {
        cx = width - r;
        cy = r;
        in_corner = true;
      } else if (px < r && py > height - r) {
        cx = r;
        cy = height - r;
        in_corner = true;
      } else if (px > width - r && py > height - r) {
        cx = width - r;
        cy = height - r;
        in_corner = true;
      }
      if (in_corner) {
        const auto distance = std::hypot(px - cx, py - cy);
        coverage = std::clamp(r + 0.5 - distance, 0.0, 1.0);
      }
      if (coverage < 1.0) {
        access->setPosition(x, y);
        CColor color;
        access->getColor(color);
        color.alpha = static_cast<uint8_t>(
            std::clamp(std::round(static_cast<double>(color.alpha) * coverage),
                       0.0, 255.0));
        access->setColor(color);
      }
    }
  }
}

auto MakeRoundedBitmap(CBitmap* const source, const int width, const int height,
                       const double radius) -> SharedPointer<CBitmap> {
  auto bitmap = ScaleBitmap(source, width, height);
  if (bitmap) {
    ApplyRoundedMask(bitmap.get(), radius);
  }
  return bitmap;
}

}  // namespace

Editor::Editor(void* const controller)
    : VSTGUIEditor(controller),
      font_(typography::MakeUiFont()),
      font_bold_(typography::MakeUiFont(true)),
      font_description_(typography::MakeDescriptionFont()),
      font_small_(typography::MakeSmallFont()),
      font_small_bold_(typography::MakeSmallFont(true)),
      font_tab_(typography::MakeTabFont()),
      font_heading_(typography::MakeHeadingFont()),
      font_strong_(typography::MakeStrongFont()),
      page_views_(),
      page_tabs_() {
  setRect(ViewRect(0, 0, layout::kWindowWidth, layout::kWindowHeight));
}

Editor::~Editor() {
  font_->forget();
  font_bold_->forget();
  font_description_->forget();
  font_small_->forget();
  font_small_bold_->forget();
  font_tab_->forget();
  font_heading_->forget();
  font_strong_->forget();
}

auto PLUGIN_API Editor::open(void* const parent,
                             const PlatformType& /*platformType*/) -> bool {
  if (frame) {
    return false;
  }
  frame = new CFrame(layout::WindowRect(), this);
  if (!frame) {
    return false;
  }
  japanese_tooltips_ = ui::IsJapaneseEnvironment();
  const auto japanese_tooltips = japanese_tooltips_;
  frame->enableTooltips(japanese_tooltips_ && control_help_enabled_, 800);
  auto* const beatrice_controller = static_cast<Controller*>(getController());

  // テクスチャ設定
  const auto frame_texture = SurfaceTextureParams{
      .base = theme::kBackground,
      .low_frequency_strength = 0.9,
      .fine_grain_strength = 0.9,
      .baked_grain_strength = 0.5,
  };
  const auto header_texture = SurfaceTextureParams{
      .base = theme::kHeader,
      .low_frequency_strength = 5.9,
      .fine_grain_strength = 0.95,
      .baked_grain_strength = 1.08,
  };
  const auto tab_texture = SurfaceTextureParams{
      .base = theme::kTabSelected,
      .low_frequency_strength = 17.6,
      .fine_grain_strength = 0.95,
      .baked_grain_strength = 1.08,
  };
  const auto page_texture = SurfaceTextureParams{
      .base = theme::kBackground,
      .low_frequency_strength = 1.0,
      .fine_grain_strength = 0.7,
      .baked_grain_strength = 0.45,
  };
  const auto panel_texture = SurfaceTextureParams{
      .base = theme::kSurface,
      .low_frequency_strength = 4.7,
      .fine_grain_strength = 0.70,
      .baked_grain_strength = 0.86,
  };
  const auto control_texture = SurfaceTextureParams{
      .base = theme::kDropdown,
      .low_frequency_strength = 5.9,
      .fine_grain_strength = 0.95,
      .baked_grain_strength = 1.08,
  };
  const auto surface_noise = SurfaceNoiseParams{};
  const auto surface_noise_maps =
      std::make_shared<SurfaceNoiseMaps>(surface_noise);
  const auto frame_surface =
      VSTGUI::owned(new SurfaceBitmap(frame_texture, surface_noise_maps));
  const auto header_surface = VSTGUI::owned(
      new SurfaceBitmap(header_texture, surface_noise_maps, 512, 82));
  const auto tab_surface = VSTGUI::owned(
      new SurfaceBitmap(tab_texture, surface_noise_maps, 512, 52));
  const auto page_surface =
      VSTGUI::owned(new SurfaceBitmap(page_texture, surface_noise_maps));
  const auto panel_surface = VSTGUI::owned(
      new SurfaceBitmap(panel_texture, surface_noise_maps, 512, 480));
  const auto control_surface = VSTGUI::owned(
      new SurfaceBitmap(control_texture, surface_noise_maps, 512, 48));

  // ルートビュー
  auto* const root = new SurfacePanel(layout::WindowRect(),
                                      frame_surface, kTransparentCColor, 0.0);
  frame->addView(root);

  // UI 生成ヘルパー
  const auto make_label =
      [&](CViewContainer* parent, const CRect& rect, const char* text,
          CFontRef font, const CColor& color,
          CHoriTxtAlign align = CHoriTxtAlign::kLeftText) -> CTextLabel* {
    auto* label = new CTextLabel(rect, text, nullptr, CParamDisplay::kNoFrame);
    label->setBackColor(kTransparentCColor);
    label->setFont(font);
    label->setFontColor(color);
    label->setHoriAlign(align);
    parent->addView(label);
    return label;
  };
  const auto register_control = [&](const ParamID param_id,
                                    CControl* const control) -> void {
    const auto [it, inserted] = controls_.emplace(param_id, control);
    assert(inserted);
    if (!inserted) {
      it->second = control;
    }
  };
  const auto add_title = [&](CViewContainer* parent, const CRect& rect,
                             const char* text) -> CTextLabel* {
    return make_label(parent, rect, text, font_heading_,
                      theme::kText, CHoriTxtAlign::kLeftText);
  };
  const auto add_panel = [&](CViewContainer* parent,
                             const CRect& rect) -> SurfacePanel* {
    auto* panel = new SurfacePanel(rect, panel_surface,
                                   kTransparentCColor, 2.0);
    parent->addView(panel);
    if (const auto column = ColumnForView(parent);
        column != FocusColumn::kNone) {
      panel->SetFocusAction(
          [this, column]() { FocusColumnRoot(column); });
    }
    return panel;
  };
  const auto add_action =
      [&](CViewContainer* parent, const CRect& rect, const char* text,
          std::function<void()> action,
          const theme::ActionRole role = theme::ActionRole::kSwitch,
          const ui::ControlHelpID help_id = ui::ControlHelpID::kNone)
          -> GlowingActionLabel* {
    auto* label = new GlowingActionLabel(
        rect, text, [this, parent, action = std::move(action)]() {
          FocusColumnRoot(ColumnForView(parent));
          action();
        });
    label->setBackColor(role == theme::ActionRole::kSwitch
                            ? theme::kSwitch
                            : theme::kActionButton);
    label->setFont(font_small_);
    label->SetStateFonts(font_small_, font_small_bold_);
    label->setFontColor(theme::kAccent);
    label->setHoriAlign(CHoriTxtAlign::kCenterText);
    label->setStyle(CParamDisplay::kNoFrame);
    ApplyControlHelpTooltip(label, help_id, japanese_tooltips);
    parent->addView(label);
    return label;
  };
  const auto reset_parameters =
      [this](const std::initializer_list<ParamID> param_ids) -> void {
    for (const auto param_id : param_ids) {
      const auto it = controls_.find(param_id);
      if (it == controls_.end()) {
        continue;
      }
      auto* const control = it->second;
      const auto& parameter = common::kSchema.GetParameter(
          static_cast<ParameterID>(param_id));
      auto default_value = 0.0f;
      if (const auto* const number =
              std::get_if<common::NumberParameter>(&parameter)) {
        default_value = static_cast<float>(number->GetDefaultValue());
      } else if (const auto* const list =
                     std::get_if<common::ListParameter>(&parameter)) {
        default_value = static_cast<float>(list->GetDefaultValue());
      } else {
        continue;
      }
      control->beginEdit();
      control->setValue(default_value);
      control->valueChanged();
      control->endEdit();
      control->invalid();
    }
  };
  const auto add_slider = [&](CViewContainer* parent, ParamID param_id,
                              const CRect& rect) -> Slider* {
    const auto slider_spec = ui_limits::GetSliderSpec(
        static_cast<ParameterID>(param_id));
    auto* const param =
        static_cast<LinearParameter*>(controller->getParameterObject(param_id));
    auto* const slider_bmp = new MonotoneBitmap(
        static_cast<int>(rect.getWidth()), static_cast<int>(rect.getHeight()),
        kTransparentCColor, kTransparentCColor);
    auto* const handle_bmp = new MonotoneBitmap(
        kSliderKnobWidth, 22, theme::kSliderHandle, kTransparentCColor);
    const auto title =
        Steinberg::Vst::StringConvert::convert(param->getInfo().title);
    auto* slider = new Slider(
        rect, this, static_cast<int>(param_id), static_cast<int>(rect.left),
        static_cast<int>(rect.right - handle_bmp->getWidth()), handle_bmp,
        slider_bmp,
        Steinberg::Vst::StringConvert::convert(param->getInfo().units),
        font_small_, font_bold_, title, slider_spec.precision);
    slider->setMin(param->GetMinPlain());
    slider->setMax(param->GetMaxPlain());
    if (const auto ui_range = ui_limits::GetSliderRange(
            static_cast<ParameterID>(param_id))) {
      slider->setMin(ui_range->minimum);
      slider->setMax(ui_range->maximum);
    }
    slider->setWheelInc(slider_spec.wheel_increment);
    slider->setFineWheelInc(slider_spec.fine_wheel_increment);
    slider->SetKeyboardInc(slider_spec.keyboard_increment);
    slider->SetKeyboardFineInc(slider_spec.keyboard_fine_increment);
    slider->SetWheelEditingEnabled(true);
    slider->setDefaultValue(
        param->toPlain(param->getInfo().defaultNormalizedValue));
    slider->setValue(ui_limits::ClampForDisplay(
        static_cast<ParameterID>(param_id),
        static_cast<float>(param->toPlain(param->getNormalized()))));
    slider->SetFocusChangedAction(
        [this, slider]() { SetFocusedColumn(ColumnForView(slider)); });
    slider->SetDragFinishedAction(
        [this]() { FlushDeferredPresetSave(); });
    ApplyControlHelpTooltip(
        slider,
        ui::ControlHelpForParameter(static_cast<ParameterID>(param_id)),
        japanese_tooltips);
    parent->addView(slider);
    register_control(param_id, slider);
    slider_bmp->forget();
    handle_bmp->forget();
    return slider;
  };
  const auto add_list_slider =
      [&](CViewContainer* parent, const ParamID param_id, const CRect& rect,
          std::vector<std::string> labels) -> Slider* {
    auto* const param =
        static_cast<StringListParameter*>(controller->getParameterObject(param_id));
    auto* const slider_bmp = new MonotoneBitmap(
        static_cast<int>(rect.getWidth()), static_cast<int>(rect.getHeight()),
        kTransparentCColor, kTransparentCColor);
    auto* const handle_bmp = new MonotoneBitmap(
        kSliderKnobWidth, 22, theme::kSliderHandle, kTransparentCColor);
    const auto title =
        Steinberg::Vst::StringConvert::convert(param->getInfo().title);
    auto* const slider = new Slider(
        rect, this, static_cast<int>(param_id), static_cast<int>(rect.left),
        static_cast<int>(rect.right - handle_bmp->getWidth()), handle_bmp,
        slider_bmp, "", font_small_, font_bold_, title, 0);
    slider->setMin(0.0f);
    slider->setMax(static_cast<float>(labels.size() - 1));
    slider->setWheelInc(1.0f);
    slider->setFineWheelInc(1.0f);
    slider->SetWheelEditingEnabled(true);
    slider->SetValueLabels(std::move(labels));
    slider->setDefaultValue(
        static_cast<float>(param->toPlain(param->getInfo().defaultNormalizedValue)));
    slider->setValue(static_cast<float>(param->toPlain(param->getNormalized())));
    slider->SetFocusChangedAction(
        [this, slider]() { SetFocusedColumn(ColumnForView(slider)); });
    slider->SetDragFinishedAction(
        [this]() { FlushDeferredPresetSave(); });
    ApplyControlHelpTooltip(
        slider,
        ui::ControlHelpForParameter(static_cast<ParameterID>(param_id)),
        japanese_tooltips);
    parent->addView(slider);
    register_control(param_id, slider);
    slider_bmp->forget();
    handle_bmp->forget();
    return slider;
  };
  const auto add_option_menu =
      [&](CViewContainer* parent, ParamID param_id, const CRect& rect,
          const CColor& frame_color = theme::kPanelBorder,
          const CColor& back_color = theme::kDropdown) -> COptionMenu* {
    auto* const param = static_cast<StringListParameter*>(
        controller->getParameterObject(param_id));
    auto* const bmp = new MonotoneBitmap(static_cast<int>(rect.getWidth()),
                                         static_cast<int>(rect.getHeight()),
                                         back_color, frame_color, 3.0);
    const auto column = ColumnForView(parent);
    auto* const control = new ColumnFocusOptionMenu(
        rect, this, static_cast<int>(param_id), bmp,
        [this, column]() { FocusColumnRoot(column); });
    bmp->forget();
    for (auto i = 0; i <= param->getInfo().stepCount; ++i) {
      String128 tmp_string128;
      param->toString(param->toNormalized(i), tmp_string128);
      const auto name = Steinberg::Vst::StringConvert::convert(tmp_string128);
      control->addEntry(name.c_str());
    }
    control->setValue(static_cast<float>(
        param->toPlain(controller->getParamNormalized(param_id))));
    control->setFont(font_);
    control->setFontColor(theme::kWarmText);
    ApplyControlHelpTooltip(
        control,
        ui::ControlHelpForParameter(static_cast<ParameterID>(param_id)),
        japanese_tooltips);
    parent->addView(control);
    auto* const chevron = new ChevronView(layout::DropdownChevronRect(rect),
                                          theme::kAccent);
    chevron->setMouseEnabled(false);
    parent->addView(chevron);
    register_control(param_id, control);
    return control;
  };

  // ヘッダー
  auto* const header =
      new SurfacePanel(layout::HeaderPanelRect(), header_surface,
                       theme::kHeaderOverlay, 0.0);
  root->addView(header);
  auto* const logo_view = new CView(layout::kHeaderLogoRect);
  auto* const logo_bmp = new CBitmap("logo.png");
  logo_view->setBackground(logo_bmp);
  header->addView(logo_view);
  logo_bmp->forget();
  conversion_status_label_ =
      make_label(header, layout::kHeaderVoiceConversionRect,
                 "VOICE CONVERSION", font_small_, theme::kTextAccent);
  bypass_button_ = new GlowingActionLabel(
      layout::kHeaderBypassRect, "", [this]() {
        auto* const controller = static_cast<Controller*>(getController());
        const auto param_id = static_cast<ParamID>(ParameterID::kBypass);
        const auto current = controller->getParamNormalized(param_id);
        SendParameterEdit(param_id, current > 0.5 ? 0.0 : 1.0);
      });
  bypass_button_->setBackColor(kTransparentCColor);
  bypass_button_->setFont(font_small_);
  bypass_button_->SetStateFonts(font_small_, font_small_bold_);
  bypass_button_->setFontColor(theme::kBypassOffText);
  bypass_button_->SetActiveColor(theme::kBypassOnBackground);
  bypass_button_->SetInactiveColor(theme::kBypassOffBackground);
  bypass_button_->SetIcon(ActionIcon::kPower);
  bypass_button_->setHoriAlign(CHoriTxtAlign::kCenterText);
  bypass_button_->setStyle(CParamDisplay::kNoFrame);
  ApplyControlHelpTooltip(bypass_button_, ui::ControlHelpID::kConversionBypass,
                          japanese_tooltips);
  header->addView(bypass_button_);
  UpdateBypassUi(beatrice_controller->getParamNormalized(
                     static_cast<ParamID>(ParameterID::kBypass)) > 0.5);

  auto* const model_panel =
      new SurfacePanel(layout::HeaderModelRect(), control_surface,
                       theme::kPanelBorder, 3.0);
  header->addView(model_panel);
  make_label(model_panel, layout::kHeaderModelLabelRect, "MODEL", font_small_,
             theme::kTextAccent);
  auto* const model_selector =
      new FileSelector(layout::HeaderModelSelectorRect(), this,
                       static_cast<int>(ParameterID::kModel), nullptr);
  model_selector->setBackColor(kTransparentCColor);
  model_selector->setStyle(CParamDisplay::kNoFrame);
  model_selector->setFont(font_strong_);
  model_selector->setFontColor(theme::kTextPrimary);
  model_selector->setHoriAlign(CHoriTxtAlign::kCenterText);
  model_panel->addView(model_selector);
  ApplyControlHelpTooltip(model_selector, ui::ControlHelpID::kModel,
                          japanese_tooltips);
  register_control(static_cast<ParamID>(ParameterID::kModel), model_selector);
  model_name_label_ = model_selector;

  // タブ
  // The development version string is longer than a release version. Give the
  // right-aligned label enough room so its leading "V" is not clipped.
  make_label(header, layout::kHeaderVersionRect,
             (UTF8String(layout::kHeaderVersionPrefix) + FULL_VERSION_STR).data(),
             font_small_,
             theme::kTextMuted, CHoriTxtAlign::kRightText);

  // ページコンテナ
  auto* const main_page = new SurfacePanel(
      layout::MainPageRect(), page_surface,
      kTransparentCColor, 0.0);
  root->addView(main_page);
  page_views_[0] = main_page;

  // The columns are layout/background containers, not visual cards. Keeping
  // their border transparent prevents it from overlapping the full-width
  // child-card borders at every vertical gap.
  const auto add_column = [&](const CRect& rect) -> SurfacePanel* {
    auto* const column = new SurfacePanel(rect, SharedPointer<SurfaceBitmap>{},
                                          kTransparentCColor, 0.0);
    main_page->addView(column);
    return column;
  };
  // Display order is independent from focus ownership. Each column keeps its
  // semantic role even if its visual position changes again in the future.
  auto* const control_column =
      add_column(layout::ColumnRect(layout::ColumnRole::kSettings));
  auto* const voice_column =
      add_column(layout::ColumnRect(layout::ColumnRole::kVoice));
  auto* const preset_column =
      add_column(layout::ColumnRect(layout::ColumnRole::kPresets));
  RegisterFocusColumn(FocusColumn::kSettings, control_column);
  RegisterFocusColumn(FocusColumn::kVoice, voice_column);
  RegisterFocusColumn(FocusColumn::kPresets, preset_column);

  // Main ページ左
  auto* gain_panel = add_panel(
      control_column, layout::PanelRect(0, layout::kSettingsTopPanelHeight));
  auto* const levels_panel =
      add_panel(gain_panel, layout::PanelRect(layout::kSettingsTabBodyTop,
                                               layout::kSettingsTopPanelHeight));
  // The tab above already identifies this section; keep the title slot empty
  // so the established vertical rhythm remains unchanged.
  add_slider(levels_panel, static_cast<ParamID>(ParameterID::kInputGain),
             layout::SettingsSliderRect(0));
  add_slider(levels_panel, static_cast<ParamID>(ParameterID::kOutputGain),
             layout::SettingsSliderRect(1));
  add_slider(
      levels_panel, static_cast<ParamID>(ParameterID::kCompensatedDrive),
      layout::SettingsSliderRect(2));
  add_action(levels_panel, layout::ResetButtonRect(), "RESET",
             [reset_parameters]() {
               reset_parameters(
                   {static_cast<ParamID>(ParameterID::kInputGain),
                    static_cast<ParamID>(ParameterID::kOutputGain),
                    static_cast<ParamID>(ParameterID::kCompensatedDrive)});
             }, theme::ActionRole::kAction);
  auto* const cleanup_panel =
      add_panel(gain_panel, layout::PanelRect(layout::kSettingsTabBodyTop,
                                               layout::kSettingsTopPanelHeight));
  add_slider(cleanup_panel, static_cast<ParamID>(ParameterID::kLowCutHz),
             layout::SettingsSliderRect(0));
  add_list_slider(cleanup_panel,
                  static_cast<ParamID>(ParameterID::kLightDenoise),
                  layout::SettingsSliderRect(1),
                  {"OFF", "LIGHT", "STANDARD"});
  add_slider(cleanup_panel, static_cast<ParamID>(ParameterID::kDeClick),
             layout::SettingsSliderRect(2));
  add_action(cleanup_panel, layout::ResetButtonRect(), "RESET",
             [reset_parameters]() {
                reset_parameters(
                    {static_cast<ParamID>(ParameterID::kLowCutHz),
                     static_cast<ParamID>(ParameterID::kLightDenoise),
                     static_cast<ParamID>(ParameterID::kDeClick)});
             }, theme::ActionRole::kAction);
  cleanup_panel->setVisible(false);
  auto* const levels_tab = add_action(
      gain_panel, layout::SplitTabRect(0), "GAIN",
      [levels_panel, cleanup_panel]() {
        levels_panel->setVisible(true);
        cleanup_panel->setVisible(false);
        levels_panel->setDirty();
        cleanup_panel->setDirty();
      });
  auto* const cleanup_tab = add_action(
      gain_panel, layout::SplitTabRect(1),
      "INPUT CLEANUP", [levels_panel, cleanup_panel]() {
        levels_panel->setVisible(false);
        cleanup_panel->setVisible(true);
        levels_panel->setDirty();
        cleanup_panel->setDirty();
      });
  levels_tab->SetAction([levels_panel, cleanup_panel, levels_tab, cleanup_tab]() {
    levels_panel->setVisible(true);
    cleanup_panel->setVisible(false);
    levels_tab->SetActive(true);
    cleanup_tab->SetActive(false);
    levels_panel->setDirty();
    cleanup_panel->setDirty();
  });
  cleanup_tab->SetAction([levels_panel, cleanup_panel, levels_tab, cleanup_tab]() {
    levels_panel->setVisible(false);
    cleanup_panel->setVisible(true);
    levels_tab->SetActive(false);
    cleanup_tab->SetActive(true);
    levels_panel->setDirty();
    cleanup_panel->setDirty();
  });
  levels_tab->SetActive(true);
  cleanup_tab->SetActive(false);
  for (auto* const tab : {levels_tab, cleanup_tab}) {
    tab->SetStateFonts(font_tab_, font_heading_);
    tab->setFontColor(theme::kText);
  }

  auto* const shape_tabs_panel = add_panel(
      control_column,
      layout::PanelRect(layout::kSettingsBottomPanelTop,
                        layout::kColumnContentHeight));
  auto* const pitch_panel =
      add_panel(shape_tabs_panel,
                layout::PanelRect(layout::kSettingsTabBodyTop,
                                  layout::kSettingsTabBodyBottom));
  add_slider(pitch_panel, static_cast<ParamID>(ParameterID::kPitchShift),
             layout::SettingsSliderRect(0));
  add_slider(pitch_panel, static_cast<ParamID>(ParameterID::kFormantShift),
             layout::SettingsSliderRect(1));
  add_slider(pitch_panel, static_cast<ParamID>(ParameterID::kVQNumNeighbors),
             layout::SettingsSliderRect(2));
  make_label(pitch_panel,
             layout::ControlLabelRect(layout::kControlLabelTop,
                                      layout::kControlLabelBottom),
             "CONTROL", font_small_,
             theme::kSecondaryText);
  add_option_menu(pitch_panel, static_cast<ParamID>(ParameterID::kLock),
                  layout::ControlMenuRect(layout::kControlMenuTop,
                                          layout::kControlMenuBottom));
  add_action(pitch_panel, layout::ResetButtonRect(), "RESET",
             [reset_parameters]() {
               reset_parameters(
                   {static_cast<ParamID>(ParameterID::kPitchShift),
                    static_cast<ParamID>(ParameterID::kFormantShift)});
             }, theme::ActionRole::kAction);

  source_pitch_panel_ = add_panel(
      pitch_panel,
      layout::PanelRect(layout::kSourcePitchTop, layout::kSourcePitchBottom));
  add_action(source_pitch_panel_, layout::ResetButtonRect(), "RESET",
             [reset_parameters]() {
               reset_parameters(
                   {static_cast<ParamID>(ParameterID::kMinSourcePitch),
                    static_cast<ParamID>(ParameterID::kMaxSourcePitch)});
             }, theme::ActionRole::kAction);
  add_slider(source_pitch_panel_,
             static_cast<ParamID>(ParameterID::kAverageSourcePitch),
             layout::SettingsSliderRect(0));
  add_slider(source_pitch_panel_,
             static_cast<ParamID>(ParameterID::kMinSourcePitch),
             layout::SettingsSliderRect(1));
  add_slider(source_pitch_panel_,
             static_cast<ParamID>(ParameterID::kMaxSourcePitch),
             layout::SettingsSliderRect(2));

  auto* const tuning_panel = add_panel(
      shape_tabs_panel,
      layout::PanelRect(layout::kSettingsTabBodyTop,
                        layout::kSettingsTabBodyBottom));
  add_slider(tuning_panel,
             static_cast<ParamID>(ParameterID::kIntonationIntensity),
             layout::SettingsSliderRect(0));
  add_slider(tuning_panel, static_cast<ParamID>(ParameterID::kPitchCorrection),
             layout::SettingsSliderRect(1));
  make_label(tuning_panel,
             layout::ControlLabelRect(layout::kTuningControlLabelTop,
                                      layout::kTuningControlLabelBottom),
             "Pitch Correction Type",
             font_small_, theme::kSecondaryText);
  add_option_menu(tuning_panel,
                  static_cast<ParamID>(ParameterID::kPitchCorrectionType),
                  layout::ContentRect(layout::kTuningControlMenuTop,
                                      layout::kTuningControlMenuBottom));
  make_label(tuning_panel,
             layout::ControlLabelRect(layout::kTuningLatencyLabelTop,
                                      layout::kTuningLatencyLabelBottom),
             "Latency Resporting", font_small_, theme::kSecondaryText);
  add_option_menu(
      tuning_panel, static_cast<ParamID>(ParameterID::kLatencyReporting),
      layout::ContentRect(layout::kTuningLatencyMenuTop,
                          layout::kTuningLatencyMenuBottom));
  add_action(tuning_panel, layout::ResetButtonRect(), "RESET",
             [reset_parameters]() {
               reset_parameters(
                   {static_cast<ParamID>(ParameterID::kIntonationIntensity),
                    static_cast<ParamID>(ParameterID::kPitchCorrection),
                    static_cast<ParamID>(
                        ParameterID::kPitchCorrectionType),
                    static_cast<ParamID>(ParameterID::kLatencyReporting)});
             }, theme::ActionRole::kAction);
  tuning_panel->setVisible(false);
  auto* const source_tab = add_action(
      shape_tabs_panel, layout::SplitTabRect(0), "VOICE SHAPE",
      [pitch_panel, tuning_panel]() {
        pitch_panel->setVisible(true);
        tuning_panel->setVisible(false);
        pitch_panel->setDirty();
        tuning_panel->setDirty();
      });
  auto* const tuning_tab = add_action(
      shape_tabs_panel,
      layout::SplitTabRect(1), "TUNING",
      [pitch_panel, tuning_panel]() {
        pitch_panel->setVisible(false);
        tuning_panel->setVisible(true);
        pitch_panel->setDirty();
        tuning_panel->setDirty();
      });
  source_tab->SetAction(
      [pitch_panel, tuning_panel, source_tab, tuning_tab]() {
        pitch_panel->setVisible(true);
        tuning_panel->setVisible(false);
        source_tab->SetActive(true);
        tuning_tab->SetActive(false);
        pitch_panel->setDirty();
        tuning_panel->setDirty();
      });
  tuning_tab->SetAction(
      [pitch_panel, tuning_panel, source_tab, tuning_tab]() {
        pitch_panel->setVisible(false);
        tuning_panel->setVisible(true);
        source_tab->SetActive(false);
        tuning_tab->SetActive(true);
        pitch_panel->setDirty();
        tuning_panel->setDirty();
      });
  source_tab->SetActive(true);
  tuning_tab->SetActive(false);
  for (auto* const tab : {source_tab, tuning_tab}) {
    tab->SetStateFonts(font_tab_, font_heading_);
    tab->setFontColor(theme::kText);
  }
  UpdateSourcePitchVisibility();
  UpdateCompensatedDriveUi();

  // Main ページ中央
  const auto normal_voice_geometry = layout::NormalVoiceModeGeometry();
  auto* portrait_panel = new SurfacePanel(
      normal_voice_geometry.panel, panel_surface, kTransparentCColor, 0.0);
  portrait_panel_ = portrait_panel;
  voice_column->addView(portrait_panel);
  portrait_view_ = new ClickableImageView(
      layout::PortraitImageRect(), [this]() {
        FocusColumnRoot(FocusColumn::kVoice);
        if (portrait_description_pane_) {
          portrait_description_pane_->Expand();
        }
      });
  portrait_panel->addView(portrait_view_);
  unloaded_logo_view_ = new CView(layout::kPortraitLogoRect);
  auto* const unloaded_logo = new CBitmap("logo.png");
  unloaded_logo_view_->setBackground(unloaded_logo);
  unloaded_logo_view_->setVisible(false);
  portrait_panel->addView(unloaded_logo_view_);
  unloaded_logo->forget();

  morph_pad_controller_ = std::make_unique<MorphPadController>(
      beatrice_controller->core_, *beatrice_controller, [this]() {
        UpdateSelectedPresetFromCurrentState(
            static_cast<ParamID>(ParameterID::kVoiceMorphFalloff), true);
      });
  auto* const morph_pad =
      new MorphPadView(layout::PortraitImageRect(),
                       morph_pad_controller_.get(),
                       font_bold_, font_small_);
  morph_pad_view_ = morph_pad;
  ApplyControlHelpTooltip(morph_pad_view_, ui::ControlHelpID::kMorphPad,
                          japanese_tooltips);
  morph_pad_view_->setVisible(false);
  portrait_panel->addView(morph_pad_view_);

  portrait_description_pane_ = new DescriptionPane(
      layout::PanelRect(layout::kMorphDetailsTop,
                        layout::kColumnContentHeight),
      panel_surface, kTransparentCColor,
      2.0, "PORTRAIT DESCRIPTION", layout::kMorphDescriptionTitleRect,
      layout::kMorphDescriptionBodyRect, font_heading_, font_description_,
      theme::kText, theme::kBodyText,
      false, DescriptionTarget::kPortrait,
      layout::DescriptionPopupRect(),
      [this](const DescriptionTarget target, const char* const title,
             const std::u8string& text, const CRect target_rect) -> void {
        ShowDescriptionPopup(target, title, text, target_rect);
      },
      OpenDescriptionUrl);
  voice_column->addView(portrait_description_pane_);
  portrait_description_pane_->SetBodyVisible(false);
  portrait_description_pane_->setVisible(false);

  morph_mode_switch_ =
      new SurfacePanel(layout::MorphSwitchRect(), panel_surface,
                       kTransparentCColor, 0.0);
  voice_column->addView(morph_mode_switch_);
  simple_morph_tab_ =
      add_action(morph_mode_switch_, layout::SplitTabRect(1),
                 "SLIDER",
                 [this]() -> void { SetSimpleMorphMode(true); });
  advanced_morph_tab_ =
      add_action(morph_mode_switch_, layout::SplitTabRect(0),
                 "2D PAD",
                 [this]() -> void { SetSimpleMorphMode(false); });
  // Morph mode tabs follow the same inactive/active typography as the
  // GAIN/INPUT CLEANUP and VOICE SHAPE/TUNING tabs.
  for (auto* const label : {simple_morph_tab_, advanced_morph_tab_}) {
    auto* const tab = static_cast<GlowingActionLabel*>(label);
    tab->SetStateFonts(font_tab_, font_heading_);
    tab->setFontColor(theme::kText);
  }
  simple_morph_panel_ = new SimpleMorphPanel(
      layout::PanelRect(layout::kMorphPortraitTop,
                        layout::kColumnContentHeight),
      panel_surface, font_small_, font_bold_,
      [this](const common::SimpleMorphWeights& weights,
             const int voice_count, const bool save_now) -> void {
        SetFocusedColumn(FocusColumn::kVoice);
        ApplySimpleMorphWeights(weights, voice_count, save_now);
      },
      [this]() { FlushDeferredPresetSave(); },
      [this]() { ScheduleDeferredPresetSave(); },
      [this]() { SetFocusedColumn(FocusColumn::kVoice); });
  simple_morph_panel_->SetFocusAction(
      [this]() { FocusColumnRoot(FocusColumn::kVoice); });
  ApplyControlHelpTooltip(simple_morph_panel_, ui::ControlHelpID::kMorphSlider,
                          japanese_tooltips);
  voice_column->addView(simple_morph_panel_);
  morph_mode_switch_->setVisible(false);
  simple_morph_panel_->setVisible(false);
  morph_reset_button_ = add_action(
      voice_column, layout::ResetButtonRect(layout::kMorphResetTop), "RESET",
      [this]() { ResetMorph(simple_morph_mode_); },
      theme::ActionRole::kAction);
  morph_reset_button_->setVisible(false);

  const auto falloff_rect = layout::kMorphFalloffRect;
  auto* const falloff_slider_bmp =
      new MonotoneBitmap(static_cast<int>(falloff_rect.getWidth()),
                         static_cast<int>(falloff_rect.getHeight()),
                         kTransparentCColor, kTransparentCColor);
  auto* const falloff_handle_bmp = new MonotoneBitmap(
      kSliderKnobWidth, 22, theme::kSliderHandle, kTransparentCColor);
  auto* const morph_falloff = new MorphFalloffSlider(
      falloff_rect, this, static_cast<int32_t>(ParameterID::kVoiceMorphFalloff),
      falloff_handle_bmp, falloff_slider_bmp, font_small_, font_bold_);
  morph_falloff->SetFocusChangedAction(
      [this]() { SetFocusedColumn(FocusColumn::kVoice); });
  morph_falloff->SetDragFinishedAction(
      [this]() { FlushDeferredPresetSave(); });
  morph_falloff_slider_ = morph_falloff;
  ApplyControlHelpTooltip(morph_falloff_slider_,
                          ui::ControlHelpID::kMorphFalloff,
                          japanese_tooltips);
  morph_falloff_slider_->setVisible(false);
  portrait_description_pane_->addView(morph_falloff_slider_);
  register_control(static_cast<ParamID>(ParameterID::kVoiceMorphFalloff),
                   morph_falloff);
  falloff_slider_bmp->forget();
  falloff_handle_bmp->forget();

  // Main ページ右
  auto* const voice_panel = add_panel(
      voice_column, layout::PanelRect(0, layout::kVoicePanelHeight));
  auto* const voice_title =
      add_title(voice_panel, layout::VoiceTitleRect(), "VOICE");
  voice_title->setHoriAlign(CHoriTxtAlign::kCenterText);
  auto* const voice_menu =
      add_option_menu(voice_panel, static_cast<ParamID>(ParameterID::kVoice),
                      layout::ContentRect(layout::kVoiceSelectorTop,
                                          layout::kVoiceSelectorBottom, true),
                      theme::kPanelBorder,
                      theme::kDropdown);
  voice_menu->setMouseEnabled(false);
  voice_menu->setFontColor(kTransparentCColor);
  voice_selector_ = new VoiceSelectorView(
      layout::ContentRect(layout::kVoiceSelectorTop,
                          layout::kVoiceSelectorBottom, true),
      [this]() -> void { ToggleVoiceMenu(); },
      font_bold_);
  ApplyControlHelpTooltip(voice_selector_, ui::ControlHelpID::kVoice,
                          japanese_tooltips);
  voice_panel->addView(voice_selector_);
  model_description_pane_ = new DescriptionPane(
      layout::PanelRect(layout::kDescriptionNormalTop,
                        layout::kModelDescriptionBottom), panel_surface,
      kTransparentCColor,
      2.0, "MODEL DESCRIPTION",
      layout::ContentRect(layout::kDescriptionTitleTop,
                          layout::kDescriptionTitleBottom),
      layout::ContentRect(layout::kDescriptionBodyTop,
                          layout::kDescriptionBodyBottom),
      font_heading_, font_description_,
      theme::kText, theme::kBodyText,
      true, DescriptionTarget::kModel,
      layout::DescriptionPopupRect(),
      [this](const DescriptionTarget target, const char* const title,
             const std::u8string& text, const CRect target_rect) -> void {
        ShowDescriptionPopup(target, title, text, target_rect);
      },
      OpenDescriptionUrl);
  voice_column->addView(model_description_pane_);
  model_description_pane_->SetTitleAlignment(CHoriTxtAlign::kCenterText);
  model_description_pane_->SetBodyVisible(true);

  voice_description_pane_ = new DescriptionPane(
      layout::PanelRect(layout::kVoiceDescriptionTop,
                        layout::kColumnContentHeight), panel_surface,
      kTransparentCColor,
      2.0, "VOICE DESCRIPTION",
      layout::ContentRect(layout::kDescriptionTitleTop,
                          layout::kDescriptionTitleBottom),
      layout::ContentRect(layout::kDescriptionBodyTop,
                          layout::kDescriptionBodyBottom),
      font_heading_, font_description_,
      theme::kText, theme::kBodyText,
      true, DescriptionTarget::kVoice,
      layout::DescriptionPopupRect(),
      [this](const DescriptionTarget target, const char* const title,
             const std::u8string& text, const CRect target_rect) -> void {
        ShowDescriptionPopup(target, title, text, target_rect);
      },
      OpenDescriptionUrl);
  voice_column->addView(voice_description_pane_);
  voice_description_pane_->SetTitleAlignment(CHoriTxtAlign::kCenterText);
  voice_description_pane_->SetBodyVisible(true);

  // Tuning ページ
  const auto& preset_state = *std::get<std::unique_ptr<std::u8string>>(
      beatrice_controller->core_.parameter_state_.GetValue(
          ParameterID::kPresetData));
  if (!preset_state.empty()) {
    const auto serialized = std::string(
        reinterpret_cast<const char*>(preset_state.data()),
        preset_state.size());
    static_cast<void>(common::DeserializePresetWorkspace(
        serialized, preset_workspace_));
    LoadCurrentPresetBank();
  }
  if (preset_workspace_.banks.empty()) {
    preset_workspace_.banks.push_back(
        common::PresetBank{.id = 1, .name = "Default"});
    preset_workspace_.selected_bank = 0;
    LoadCurrentPresetBank();
  }
  preset_panel_ = new PresetPanel(
      layout::PanelRect(0, layout::kColumnContentHeight), panel_surface,
      font_small_, font_small_bold_,
      [this]() -> void { AddCurrentPreset(); },
      [this]() -> void { CreateNewPreset(); },
      [this](const int index) -> void { ApplyPreset(index); },
      [this](const int index, const std::string& name) -> void {
        RenamePreset(index, name);
      },
      [this](const int index, const int destination) -> void {
        MovePreset(index, destination);
      },
      [this](const int index) -> void { DeletePreset(index); },
      [this]() -> void { AddPresetBank(); },
      [this](const int index) -> void { SelectPresetBank(index); },
      [this](const int index, const std::string& name) -> void {
        RenamePresetBank(index, name);
      },
      [this](const int index, const int destination) -> void {
        MovePresetBank(index, destination);
      },
      [this](const int index) -> void { DeletePresetBank(index); },
      [this](const int mode) -> void { ImportPresets(mode); },
      [this](const bool all_banks) -> void { ExportPresets(all_banks); },
      [this]() { FocusColumnRoot(FocusColumn::kPresets); });
  preset_column->addView(preset_panel_);

  // Shared right-column page for post-conversion effects. Controls are added
  // here as their DSP and parameter contracts are fixed.
  effects_panel_ = new SurfacePanel(
      layout::PanelRect(0, layout::kColumnContentHeight), panel_surface,
      kTransparentCColor, 2.0);
  effects_panel_->SetFocusAction(
      [this]() { FocusColumnRoot(FocusColumn::kPresets); });
  preset_column->addView(effects_panel_);
  auto* const clarity_panel = add_panel(
      effects_panel_,
      layout::PanelRect(layout::kEffectsClarityPanelTop,
                        layout::kEffectsClarityPanelBottom));
  add_title(clarity_panel, layout::PanelTitleRect(180.0), "CLARITY");
  add_slider(clarity_panel, static_cast<ParamID>(ParameterID::kDeMud),
             layout::SettingsSliderRect(0));
  add_slider(clarity_panel, static_cast<ParamID>(ParameterID::kPresence),
             layout::SettingsSliderRect(1));
  add_action(clarity_panel, layout::ResetButtonRect(), "RESET",
             [reset_parameters]() {
               reset_parameters({
                   static_cast<ParamID>(ParameterID::kDeMud),
                   static_cast<ParamID>(ParameterID::kPresence),
               });
             });

  auto* const reverb_panel = add_panel(
      effects_panel_,
      layout::PanelRect(layout::kEffectsReverbPanelTop,
                        layout::kEffectsReverbPanelBottom));
  add_title(reverb_panel, layout::PanelTitleRect(180.0), "REVERB");
  add_slider(reverb_panel, static_cast<ParamID>(ParameterID::kReverbMix),
             layout::SettingsSliderRect(0));
  add_slider(reverb_panel, static_cast<ParamID>(ParameterID::kReverbDecay),
             layout::SettingsSliderRect(1));
  add_slider(reverb_panel, static_cast<ParamID>(ParameterID::kReverbTone),
             layout::SettingsSliderRect(2));
  add_action(reverb_panel, layout::ResetButtonRect(), "RESET",
             [reset_parameters]() {
               reset_parameters({
                   static_cast<ParamID>(ParameterID::kReverbMix),
                   static_cast<ParamID>(ParameterID::kReverbDecay),
                   static_cast<ParamID>(ParameterID::kReverbTone),
               });
             });

  presets_tab_ = add_action(
      preset_column, layout::SplitTabRect(0), "PRESETS",
      [this]() { SelectRightPanel(false); });
  effects_tab_ = add_action(
      preset_column, layout::SplitTabRect(1), "EFFECTS",
      [this]() { SelectRightPanel(true); });
  for (auto* const tab : {presets_tab_, effects_tab_}) {
    tab->SetStateFonts(font_tab_, font_heading_);
    tab->setFontColor(theme::kText);
  }
  SelectRightPanel(false);
  RefreshPresetPanel();

  // Voice 選択メニュー
  voice_menu_overlay_ = new VoiceMenuOverlayView(
      layout::FullWindowRect(), panel_surface, font_,
      [this](const int voice_id) -> void { SelectVoice(voice_id); });
  root->addView(voice_menu_overlay_);

  // Description の拡大表示
  description_popup_ = new DescriptionPopupView(
      layout::PanelRect(0, layout::kColumnContentHeight), panel_surface,
      kTransparentCColor, 0.0, font_heading_, font_description_,
      OpenDescriptionUrl);
  voice_column->addView(description_popup_);

  if (!frame->open(parent)) {
    close();
    return false;
  }

  if (const auto* value = std::get_if<std::unique_ptr<std::u8string>>(
          &beatrice_controller->core_.parameter_state_.GetValue(
              ParameterID::kModel));
      value && *value) {
    static_cast<FileSelector*>(
        controls_.at(static_cast<ParamID>(ParameterID::kModel)))
        ->SetPath(**value);
  }
  SyncModelDescription();
  SyncSourcePitchRange();
  SyncParameterAvailability();
  UpdateSourcePitchVisibility();

  // デバッグ用
  if (const auto model_path = GetScreenshotModelPath(); !model_path.empty()) {
    auto* const model_control = static_cast<FileSelector*>(
        controls_.at(static_cast<ParamID>(ParameterID::kModel)));
    model_control->SetPath(model_path);
    valueChanged(model_control);
  }
  if (HasEnvironmentVariable("BEATRICE_SCREENSHOT_VOICE_MORPH") &&
      model_config_.has_value() && common::GetVoiceCount(*model_config_) > 1) {
    auto* const voice_control = static_cast<COptionMenu*>(
        controls_.at(static_cast<ParamID>(ParameterID::kVoice)));
    voice_control->setValue(voice_control->getMax());
    valueChanged(voice_control);
  }

  return true;
}
void PLUGIN_API Editor::close() {
  if (frame) {
    FlushDeferredPresetSave();
    if (morph_pad_view_ && morph_pad_view_->isEditing()) {
      morph_pad_view_->endEdit();
    }
    frame->forget();
    frame = nullptr;
    japanese_tooltips_ = false;
    controls_.clear();
    portraits_.clear();
    portrait_menu_thumbnails_.clear();
    portrait_marker_thumbnails_.clear();
    portrait_view_ = nullptr;
    portrait_panel_ = nullptr;
    unloaded_logo_view_ = nullptr;
    morph_pad_controller_.reset();
    morph_pad_view_ = nullptr;
    simple_morph_panel_ = nullptr;
    source_pitch_panel_ = nullptr;
    morph_mode_switch_ = nullptr;
    simple_morph_tab_ = nullptr;
    advanced_morph_tab_ = nullptr;
    simple_morph_mode_ = true;
    preset_panel_ = nullptr;
    effects_panel_ = nullptr;
    presets_tab_ = nullptr;
    effects_tab_ = nullptr;
    portrait_description_pane_ = nullptr;
    morph_falloff_slider_ = nullptr;
    morph_reset_button_ = nullptr;
    model_description_pane_ = nullptr;
    voice_description_pane_ = nullptr;
    voice_selector_ = nullptr;
    voice_menu_overlay_ = nullptr;
    description_popup_ = nullptr;
    model_name_label_ = nullptr;
    page_views_ = {};
    page_tabs_ = {};
    tab_indicator_ = nullptr;
    focus_columns_.clear();
    focused_column_ = FocusColumn::kNone;
    voice_morph_state_ = {};
  }
}

void Editor::SelectPage(const int page) {
  const auto page_count = static_cast<int>(page_views_.size());
  for (auto i = 0; i < page_count; ++i) {
    if (page_views_[i]) {
      page_views_[i]->setVisible(i == page);
      page_views_[i]->setDirty();
    }
    if (page_tabs_[i]) {
      const auto selected = i == page;
      page_tabs_[i]->setFontColor(selected ? theme::kTextAccent
                                           : theme::kTextMuted);
      page_tabs_[i]->setStyle(CParamDisplay::kNoFrame);
      if (auto* const tab = dynamic_cast<GlowingActionLabel*>(page_tabs_[i])) {
        tab->SetActive(selected);
      }
      page_tabs_[i]->setDirty();
    }
  }
  if (tab_indicator_ && page >= 0 && page < page_count) {
    const auto indicator_rect = layout::PageIndicatorRect(page);
    tab_indicator_->setViewSize(indicator_rect);
    tab_indicator_->setMouseableArea(indicator_rect);
    tab_indicator_->setDirty();
  }
  HideVoiceMenu();
  HideDescriptionPopup();
}

void Editor::SetPortraitDescriptionText(const std::u8string& text) {
  if (portrait_description_pane_) {
    portrait_description_pane_->SetText(text);
  }
}

void Editor::SetModelDescriptionText(const std::u8string& text) {
  if (model_description_pane_) {
    model_description_pane_->SetText(text);
  }
}

void Editor::SetVoiceDescriptionText(const std::u8string& text) {
  if (voice_description_pane_) {
    voice_description_pane_->SetText(text);
  }
}

void Editor::SetPortraitDescriptionMode(const bool morphing) {
  if (portrait_description_pane_) {
    portrait_description_pane_->SetTitle(morphing ? ""
                                                  : "PORTRAIT DESCRIPTION");
    portrait_description_pane_->SetBodyVisible(false);
    portrait_description_pane_->setVisible(morphing);
  }
  if (morph_falloff_slider_) {
    morph_falloff_slider_->setVisible(morphing && !simple_morph_mode_);
    morph_falloff_slider_->setDirty();
  }
}

void Editor::SetVoiceSelectorDisplay(const int voice_id) {
  if (voice_selector_) {
    voice_selector_->SetDisplay(model_config_, portrait_menu_thumbnails_,
                                voice_id);
  }
}

void Editor::ToggleVoiceMenu() {
  SetFocusedColumn(FocusColumn::kVoice);
  auto* const voice_control = GetVoiceControl();
  if (!voice_menu_overlay_ || !voice_control) {
    return;
  }
  const auto selected_voice_id =
      static_cast<int>(std::round(voice_control->getValue()));
  voice_menu_overlay_->ToggleMenu(model_config_, portrait_menu_thumbnails_,
                                  selected_voice_id);
}

void Editor::HideVoiceMenu() {
  if (voice_menu_overlay_) {
    voice_menu_overlay_->HideMenu();
  }
}

void Editor::RebuildVoiceMenu() {
  auto* const voice_control = GetVoiceControl();
  if (!voice_menu_overlay_ || !voice_control) {
    return;
  }
  const auto selected_voice_id =
      static_cast<int>(std::round(voice_control->getValue()));
  voice_menu_overlay_->RebuildMenu(model_config_, portrait_menu_thumbnails_,
                                   selected_voice_id);
}

void Editor::SelectVoice(const int voice_id) {
  auto* const voice_control = GetVoiceControl();
  if (!voice_control) {
    return;
  }
  voice_control->beginEdit();
  voice_control->setValue(static_cast<float>(voice_id));
  voice_control->valueChanged();
  voice_control->endEdit();
}

auto Editor::GetVoiceControl() const -> COptionMenu* {
  const auto voice_param_id = static_cast<ParamID>(ParameterID::kVoice);
  const auto it = controls_.find(voice_param_id);
  if (it == controls_.end()) {
    return nullptr;
  }
  return static_cast<COptionMenu*>(it->second);
}

void Editor::ShowDescriptionPopup(const DescriptionTarget target,
                                  const char* const title,
                                  const std::u8string& text,
                                  const CRect target_rect) {
  if (morphing_active_) {
    return;
  }
  HideVoiceMenu();
  if (description_popup_) {
    if (description_popup_->IsShowing(target)) {
      description_popup_->Hide();
    } else {
      description_popup_->Show(target, title, text, target_rect);
    }
  }
}

void Editor::HideDescriptionPopup() {
  if (description_popup_) {
    description_popup_->Hide();
  }
}

void Editor::ApplyVoiceMorphState(const common::VoiceMorphState& state) {
  voice_morph_state_ = state;
  if (model_config_.has_value()) {
    const auto voice_count = common::GetVoiceCount(*model_config_);
    assert(voice_count > 0);
    if (voice_count > 0) {
      for (auto i = 0; i < voice_morph_state_.marker_count; ++i) {
        voice_morph_state_.markers[i].voice_id = std::clamp(
            voice_morph_state_.markers[i].voice_id, 0, voice_count - 1);
      }
    }
  }

  if (morph_pad_view_) {
    morph_pad_view_->SetState(voice_morph_state_);
  }
  if (morph_falloff_slider_) {
    morph_falloff_slider_->SetValue(voice_morph_state_.falloff);
  }
  UpdateVoiceMorphingDescription();
}

auto Editor::ColumnForView(const CView* const view) const -> FocusColumn {
  for (auto* current = view; current; current = current->getParentView()) {
    for (const auto& entry : focus_columns_) {
      if (current == entry.panel) {
        return entry.role;
      }
    }
  }
  return FocusColumn::kNone;
}

void Editor::SetFocusedColumn(const FocusColumn column) {
  focused_column_ = column;
}

void Editor::SelectRightPanel(const bool effects) {
  if (preset_panel_) {
    preset_panel_->setVisible(!effects);
    preset_panel_->setDirty();
  }
  if (effects_panel_) {
    effects_panel_->setVisible(effects);
    effects_panel_->setDirty();
  }
  if (presets_tab_) {
    presets_tab_->SetActive(!effects);
  }
  if (effects_tab_) {
    effects_tab_->SetActive(effects);
  }
  if (!frame || !frame->isAttached()) {
    return;
  }
  if (effects && effects_panel_) {
    frame->setFocusView(effects_panel_);
  } else if (!effects && preset_panel_) {
    frame->setFocusView(preset_panel_);
  }
}

void Editor::SetControlHelpEnabled(const bool enabled) {
  control_help_enabled_ = enabled;
  if (frame) {
    frame->enableTooltips(japanese_tooltips_ && control_help_enabled_, 800);
  }
}

void Editor::RegisterFocusColumn(const FocusColumn role,
                                 SurfacePanel* const panel) {
  if (!panel || role == FocusColumn::kNone) {
    return;
  }
  focus_columns_.push_back({role, panel});
  panel->SetFocusAction([this, role]() { FocusColumnRoot(role); });
}

void Editor::FocusColumnRoot(const FocusColumn column) {
  if (column == FocusColumn::kNone) {
    return;
  }
  SetFocusedColumn(column);
  if (!frame) {
    return;
  }
  if (column == FocusColumn::kPresets) {
    if (effects_panel_ && effects_panel_->isVisible()) {
      frame->setFocusView(effects_panel_);
      return;
    }
    if (preset_panel_) {
      frame->setFocusView(preset_panel_);
      return;
    }
  }
  for (const auto& entry : focus_columns_) {
    if (entry.role == column && entry.panel) {
      frame->setFocusView(entry.panel);
      return;
    }
  }
}

void Editor::UpdateSourcePitchVisibility() {
  if (!source_pitch_panel_) {
    return;
  }
  const auto it = controls_.find(static_cast<ParamID>(ParameterID::kLock));
  if (it == controls_.end()) {
    return;
  }
  const auto average_source_pitch =
      static_cast<int>(std::round(it->second->getValue())) == 0;
  source_pitch_panel_->setVisible(average_source_pitch);
  source_pitch_panel_->setDirty();
}

void Editor::UpdateCompensatedDriveUi() {
  const auto drive_it = controls_.find(
      static_cast<ParamID>(ParameterID::kCompensatedDrive));
  const auto input_it = controls_.find(
      static_cast<ParamID>(ParameterID::kInputGain));
  if (drive_it == controls_.end() || input_it == controls_.end()) {
    return;
  }
  const auto drive = static_cast<Slider*>(drive_it->second);
  auto* const input = static_cast<Slider*>(input_it->second);
  const auto active = drive->getValue() > 0.0f;
  input->SetEnabled(!active);
  input->invalid();
}

void Editor::UpdateBypassUi(const bool bypassed) {
  if (conversion_status_label_ == nullptr || bypass_button_ == nullptr) {
    return;
  }
  conversion_status_label_->setText(bypassed ? "CONVERSION OFF"
                                              : "VOICE CONVERSION");
  conversion_status_label_->setFontColor(bypassed ? theme::kBypassOffText
                                                   : theme::kTextAccent);
  bypass_button_->SetActive(!bypassed);
  bypass_button_->setBackColor(kTransparentCColor);
  bypass_button_->SetActiveColor(theme::kBypassOnBackground);
  bypass_button_->SetInactiveColor(theme::kBypassOffBackground);
  bypass_button_->setFontColor(bypassed ? theme::kBypassOffText
                                         : theme::kBypassOnText);
  conversion_status_label_->invalid();
  bypass_button_->invalid();
}

void Editor::SetSimpleMorphMode(const bool simple) {
  simple_morph_mode_ = simple;
  if (simple_morph_tab_) {
    static_cast<GlowingActionLabel*>(simple_morph_tab_)->SetActive(simple);
  }
  if (advanced_morph_tab_) {
    static_cast<GlowingActionLabel*>(advanced_morph_tab_)
        ->SetActive(!simple);
  }
  const auto* const voice_control = GetVoiceControl();
  const auto morphing =
      model_config_.has_value() && voice_control &&
      static_cast<int>(std::round(voice_control->getValue())) >=
          common::GetVoiceCount(*model_config_);
  UpdateMorphUiVisibility(morphing);
  if (!simple && morphing) {
    for (const auto [param_id, value] :
         common::GetVoiceMorphParameterValues(voice_morph_state_)) {
      const auto& parameter = std::get<common::NumberParameter>(
          common::kSchema.GetParameter(param_id));
      SendParameterEdit(static_cast<ParamID>(param_id),
                        Normalize(parameter, value));
    }
  } else if (simple && morphing && simple_morph_panel_ &&
             model_config_.has_value()) {
    ApplySimpleMorphWeights(simple_morph_panel_->GetWeights(),
                            common::GetVoiceCount(*model_config_), false);
  }
  UpdateSelectedPresetFromCurrentState(
      static_cast<ParamID>(ParameterID::kSimpleMorphWeights), true);
}

void Editor::UpdateMorphUiVisibility(const bool morphing) {
  morphing_active_ = morphing;
  if (morphing) {
    HideDescriptionPopup();
  }
  if (portrait_panel_) {
    portrait_panel_->setVisible(false);
    const auto geometry = morphing ? layout::MorphVoiceModeGeometry()
                                   : layout::NormalVoiceModeGeometry();
    portrait_panel_->setViewSize(geometry.panel);
    portrait_panel_->setMouseableArea(geometry.panel);
    portrait_panel_->setVisible(true);
    if (auto* const parent = portrait_panel_->getParentView()) {
      parent->invalid();
    }
    portrait_panel_->setDirty();
  }
  if (morph_reset_button_) {
    morph_reset_button_->setVisible(morphing);
    const auto reset_top = simple_morph_mode_
                               ? layout::kMorphResetTop
                               : layout::kMorphDetailsTop + 4.0;
    morph_reset_button_->setViewSize(layout::ResetButtonRect(reset_top));
    morph_reset_button_->setMouseableArea(layout::ResetButtonRect(reset_top));
    morph_reset_button_->setDirty();
  }
  if (model_description_pane_) {
    model_description_pane_->setVisible(!morphing);
  }
  if (voice_description_pane_) {
    voice_description_pane_->setVisible(!morphing);
  }
  if (portrait_description_pane_) {
    portrait_description_pane_->setVisible(morphing && !simple_morph_mode_);
  }
  if (morph_mode_switch_) {
    morph_mode_switch_->setVisible(morphing);
    morph_mode_switch_->setDirty();
  }
  if (simple_morph_panel_) {
    simple_morph_panel_->setVisible(morphing && simple_morph_mode_);
    simple_morph_panel_->setDirty();
  }
  if (morph_pad_view_) {
    morph_pad_view_->setViewSize(layout::PortraitImageRect());
    morph_pad_view_->setMouseableArea(morph_pad_view_->getViewSize());
    morph_pad_view_->setVisible(morphing && !simple_morph_mode_);
    morph_pad_view_->setDirty();
  }
  if (portrait_view_) {
    portrait_view_->setViewSize(
        layout::PortraitImageRect());
    portrait_view_->setMouseableArea(portrait_view_->getViewSize());
  }
  if (morph_falloff_slider_) {
    morph_falloff_slider_->setVisible(morphing && !simple_morph_mode_);
    morph_falloff_slider_->setDirty();
  }
  if (morphing && portrait_view_) {
    portrait_view_->setVisible(false);
  }
  if (simple_morph_tab_) {
    static_cast<GlowingActionLabel*>(simple_morph_tab_)
        ->SetActive(simple_morph_mode_);
  }
  if (advanced_morph_tab_) {
    static_cast<GlowingActionLabel*>(advanced_morph_tab_)
        ->SetActive(!simple_morph_mode_);
  }
}

void Editor::ResetMorph(const bool simple) {
  if (!model_config_.has_value()) {
    return;
  }
  const auto voice_count = common::GetVoiceCount(*model_config_);
  if (simple) {
    auto weights = common::SimpleMorphWeights{};
    weights[0] = 1.0f;
    if (simple_morph_panel_) {
      simple_morph_panel_->SetWeights(weights);
    }
    ApplySimpleMorphWeights(weights, voice_count);
    return;
  }
  auto state = common::VoiceMorphState{};
  for (auto i = 0; i < state.marker_count; ++i) {
    state.markers[i].voice_id = i % std::max(1, voice_count);
  }
  ApplyVoiceMorphState(state);
  for (const auto [parameter_id, value] :
       common::GetVoiceMorphParameterValues(state)) {
    const auto& parameter = std::get<common::NumberParameter>(
        common::kSchema.GetParameter(parameter_id));
    SendParameterEdit(static_cast<ParamID>(parameter_id),
                      Normalize(parameter, value));
  }
  UpdateSelectedPresetFromCurrentState(
      static_cast<ParamID>(ParameterID::kVoiceMorphFalloff), true);
}

void Editor::ApplySimpleMorphWeights(
    const common::SimpleMorphWeights& weights, const int voice_count,
    const bool save_now) {
  auto* const controller = static_cast<Controller*>(getController());
  const auto serialized =
      common::SerializeSimpleMorphWeights(weights, voice_count);
  controller->core_.parameter_state_.SetValue(
      ParameterID::kSimpleMorphWeights, serialized);
  controller->SetStringParameter(
      static_cast<ParamID>(ParameterID::kSimpleMorphWeights), serialized);
  UpdateSelectedPresetFromCurrentState(
      static_cast<ParamID>(ParameterID::kSimpleMorphWeights), save_now);

  if (const auto message = Steinberg::owned(controller->allocateMessage())) {
    const auto vst_param_id =
        static_cast<ParamID>(ParameterID::kSimpleMorphWeights);
    message->setMessageID("param_change");
    message->getAttributes()->setBinary("param_id", &vst_param_id,
                                        sizeof(vst_param_id));
    message->getAttributes()->setBinary(
        "data", serialized.data(),
        static_cast<Steinberg::uint32>(serialized.size()));
    controller->sendMessage(message);
  }
}

void Editor::AddCurrentPreset() {
  if (!model_config_.has_value()) {
    return;
  }
  auto* const controller = static_cast<Controller*>(getController());
  const auto& state = controller->core_.parameter_state_;
  const auto get_double = [&state](const ParameterID id) -> double {
    return std::get<double>(state.GetValue(id));
  };
  const auto voice = std::get<int>(state.GetValue(ParameterID::kVoice));

  auto name = CurrentModelVoicePresetName();
  if (name.empty()) {
    name = "Preset " + std::to_string(presets_.size() + 1);
  }

  auto id = static_cast<std::uint64_t>(
      std::chrono::system_clock::now().time_since_epoch().count());
  for (const auto& preset : presets_) {
    id = std::max(id, preset.id + 1);
  }
  const auto& model_value =
      *std::get<std::unique_ptr<std::u8string>>(
          state.GetValue(ParameterID::kModel));
  const auto& weights =
      *std::get<std::unique_ptr<std::u8string>>(
          state.GetValue(ParameterID::kSimpleMorphWeights));
  presets_.push_back(common::Preset{
      .id = id,
      .name = std::move(name),
      .model_path = model_value,
      .voice = voice,
       .input_gain = get_double(ParameterID::kInputGain),
       .output_gain = get_double(ParameterID::kOutputGain),
       .compensated_drive = get_double(ParameterID::kCompensatedDrive),
       .de_mud = get_double(ParameterID::kDeMud),
       .presence = get_double(ParameterID::kPresence),
       .reverb_mix = get_double(ParameterID::kReverbMix),
       .reverb_decay = get_double(ParameterID::kReverbDecay),
       .reverb_tone = get_double(ParameterID::kReverbTone),
       .pitch_shift = get_double(ParameterID::kPitchShift),
      .formant_shift = get_double(ParameterID::kFormantShift),
      .vq_neighbor_count = static_cast<int>(
          std::round(get_double(ParameterID::kVQNumNeighbors))),
      .average_source_pitch = get_double(ParameterID::kAverageSourcePitch),
      .min_source_pitch = get_double(ParameterID::kMinSourcePitch),
      .max_source_pitch = get_double(ParameterID::kMaxSourcePitch),
      .intonation_intensity = get_double(ParameterID::kIntonationIntensity),
      .pitch_correction = get_double(ParameterID::kPitchCorrection),
      .pitch_control = std::get<int>(state.GetValue(ParameterID::kLock)),
      .pitch_correction_type =
          std::get<int>(state.GetValue(ParameterID::kPitchCorrectionType)),
      .simple_morph_weights = weights,
      .advanced_morph_state = voice_morph_state_,
      .simple_morph_mode = simple_morph_mode_,
  });
  selected_preset_ = static_cast<int>(presets_.size()) - 1;
  active_preset_bank_ = preset_workspace_.selected_bank;
  SavePresets();
  RefreshPresetPanel(selected_preset_);
}

void Editor::CreateNewPreset() {
  const auto number_default = [](const ParameterID id) -> double {
    return std::get<common::NumberParameter>(common::kSchema.GetParameter(id))
        .GetDefaultValue();
  };
  const auto list_default = [](const ParameterID id) -> int {
    return std::get<common::ListParameter>(common::kSchema.GetParameter(id))
        .GetDefaultValue();
  };
  const auto& default_weights = std::get<common::StringParameter>(
      common::kSchema.GetParameter(ParameterID::kSimpleMorphWeights));

  auto id = static_cast<std::uint64_t>(
      std::chrono::system_clock::now().time_since_epoch().count());
  for (const auto& preset : presets_) {
    id = std::max(id, preset.id + 1);
  }
  presets_.push_back(common::Preset{
      .id = id,
      .name = "New Preset",
      .model_path = u8"",
      .voice = list_default(ParameterID::kVoice),
       .input_gain = number_default(ParameterID::kInputGain),
       .output_gain = number_default(ParameterID::kOutputGain),
       .compensated_drive = number_default(ParameterID::kCompensatedDrive),
       .de_mud = number_default(ParameterID::kDeMud),
       .presence = number_default(ParameterID::kPresence),
       .reverb_mix = number_default(ParameterID::kReverbMix),
       .reverb_decay = number_default(ParameterID::kReverbDecay),
       .reverb_tone = number_default(ParameterID::kReverbTone),
       .pitch_shift = number_default(ParameterID::kPitchShift),
      .formant_shift = number_default(ParameterID::kFormantShift),
      .vq_neighbor_count = static_cast<int>(
          std::round(number_default(ParameterID::kVQNumNeighbors))),
      .average_source_pitch = number_default(ParameterID::kAverageSourcePitch),
      .min_source_pitch = number_default(ParameterID::kMinSourcePitch),
      .max_source_pitch = number_default(ParameterID::kMaxSourcePitch),
      .intonation_intensity =
          number_default(ParameterID::kIntonationIntensity),
      .pitch_correction = number_default(ParameterID::kPitchCorrection),
      .pitch_control = list_default(ParameterID::kLock),
      .pitch_correction_type = list_default(ParameterID::kPitchCorrectionType),
      .simple_morph_weights = default_weights.GetDefaultValue(),
  });
  selected_preset_ = static_cast<int>(presets_.size()) - 1;
  active_preset_bank_ = preset_workspace_.selected_bank;

  // The selected blank preset must match the live plug-in state. Unload the
  // previous model first: source-pitch updates from a loaded model can
  // otherwise recalculate Pitch Shift while the remaining controls reset.
  applying_preset_ = true;
  auto* const model_control = static_cast<FileSelector*>(
      controls_.at(static_cast<ParamID>(ParameterID::kModel)));
  model_control->SetPath({});
  valueChanged(model_control);
  const auto reset_control = [this](const ParameterID id) -> void {
    const auto it = controls_.find(static_cast<ParamID>(id));
    if (it == controls_.end()) {
      return;
    }
    const auto& parameter = common::kSchema.GetParameter(id);
    auto* const control = it->second;
    if (const auto* const number =
            std::get_if<common::NumberParameter>(&parameter)) {
      control->setValue(static_cast<float>(number->GetDefaultValue()));
    } else if (const auto* const list =
                   std::get_if<common::ListParameter>(&parameter)) {
      control->setValue(static_cast<float>(list->GetDefaultValue()));
    } else {
      return;
    }
    control->beginEdit();
    valueChanged(control);
    control->endEdit();
    control->invalid();
  };
  for (const auto id_to_reset : {
           ParameterID::kVoice,
            ParameterID::kInputGain,
            ParameterID::kOutputGain,
            ParameterID::kCompensatedDrive,
            ParameterID::kDeMud,
            ParameterID::kPresence,
            ParameterID::kReverbMix,
            ParameterID::kReverbDecay,
            ParameterID::kReverbTone,
            ParameterID::kFormantShift,
           ParameterID::kVQNumNeighbors,
           ParameterID::kAverageSourcePitch,
           ParameterID::kMinSourcePitch,
           ParameterID::kMaxSourcePitch,
           ParameterID::kIntonationIntensity,
           ParameterID::kPitchCorrection,
           ParameterID::kLock,
           ParameterID::kPitchCorrectionType,
         }) {
    reset_control(id_to_reset);
  }
  // Keep Pitch Shift last. Average Source Pitch and formant control can
  // derive a temporary pitch from the previous model state while resetting.
  reset_control(ParameterID::kPitchShift);
  auto* const controller = static_cast<Controller*>(getController());
  controller->SetStringParameter(
      static_cast<ParamID>(ParameterID::kSimpleMorphWeights),
      default_weights.GetDefaultValue());
  applying_preset_ = false;

  SavePresets();
  RefreshPresetPanel(selected_preset_);
}

auto Editor::CurrentModelVoicePresetName() -> std::string {
  if (!model_config_.has_value()) {
    return {};
  }
  auto* const controller = static_cast<Controller*>(getController());
  const auto voice = std::get<int>(controller->core_.parameter_state_.GetValue(
      ParameterID::kVoice));
  const auto voice_count = common::GetVoiceCount(*model_config_);
  auto name = std::string(
      reinterpret_cast<const char*>(model_config_->model.name.data()),
      model_config_->model.name.size());
  name += " / ";
  if (voice >= 0 && voice < voice_count) {
    const auto& voice_name = model_config_->voices[voice].name;
    name.append(reinterpret_cast<const char*>(voice_name.data()),
                voice_name.size());
  } else {
    name += "Morph";
  }
  return name == " / " ? std::string{} : name;
}

auto Editor::RenameSelectedPresetFromCurrentModelVoice() -> bool {
  if (applying_preset_ || selected_preset_ < 0 ||
      selected_preset_ >= static_cast<int>(presets_.size())) {
    return false;
  }
  auto name = CurrentModelVoicePresetName();
  if (name.empty() || presets_[selected_preset_].name == name) {
    return false;
  }
  presets_[selected_preset_].name = std::move(name);
  return true;
}

void Editor::ApplyPreset(const int index) {
  if (index < 0 || index >= static_cast<int>(presets_.size())) {
    return;
  }
  const auto preset = presets_[index];
  applying_preset_ = true;
  selected_preset_ = index;
  active_preset_bank_ = preset_workspace_.selected_bank;
  auto* const model_control = static_cast<FileSelector*>(
      controls_.at(static_cast<ParamID>(ParameterID::kModel)));
  if (model_control->GetPath().u8string() != preset.model_path) {
    model_control->SetPath(preset.model_path);
    valueChanged(model_control);
  }
  if (!model_config_.has_value()) {
    applying_preset_ = false;
    SavePresets();
    RefreshPresetPanel(index);
    return;
  }

  const auto set_control = [this](const ParameterID id,
                                  const float value) -> void {
    const auto it = controls_.find(static_cast<ParamID>(id));
    if (it == controls_.end()) {
      return;
    }
    auto* const control = it->second;
    control->beginEdit();
    control->setValue(value);
    valueChanged(control);
    control->endEdit();
    control->invalid();
  };
  const auto voice_count = common::GetVoiceCount(*model_config_);
  set_control(ParameterID::kVoice,
              static_cast<float>(std::clamp(preset.voice, 0, voice_count)));
  set_control(ParameterID::kInputGain,
              static_cast<float>(preset.input_gain));
  set_control(ParameterID::kOutputGain,
              static_cast<float>(preset.output_gain));
  set_control(ParameterID::kCompensatedDrive,
              static_cast<float>(preset.compensated_drive));
  set_control(ParameterID::kDeMud, static_cast<float>(preset.de_mud));
  set_control(ParameterID::kPresence, static_cast<float>(preset.presence));
  set_control(ParameterID::kReverbMix,
              static_cast<float>(preset.reverb_mix));
  set_control(ParameterID::kReverbDecay,
              static_cast<float>(preset.reverb_decay));
  set_control(ParameterID::kReverbTone,
              static_cast<float>(preset.reverb_tone));
  set_control(ParameterID::kPitchShift,
              static_cast<float>(preset.pitch_shift));
  set_control(ParameterID::kFormantShift,
              static_cast<float>(preset.formant_shift));
  set_control(ParameterID::kVQNumNeighbors,
              static_cast<float>(preset.vq_neighbor_count));
  set_control(ParameterID::kAverageSourcePitch,
              static_cast<float>(preset.average_source_pitch));
  set_control(ParameterID::kMinSourcePitch,
              static_cast<float>(preset.min_source_pitch));
  set_control(ParameterID::kMaxSourcePitch,
              static_cast<float>(preset.max_source_pitch));
  set_control(ParameterID::kIntonationIntensity,
              static_cast<float>(preset.intonation_intensity));
  set_control(ParameterID::kPitchCorrection,
              static_cast<float>(preset.pitch_correction));
  set_control(ParameterID::kLock, static_cast<float>(preset.pitch_control));
  set_control(ParameterID::kPitchCorrectionType,
              static_cast<float>(preset.pitch_correction_type));

  if (preset.voice >= voice_count && voice_count > 1) {
    auto weights =
        common::ParseSimpleMorphWeights(preset.simple_morph_weights);
    weights = common::NormalizeSimpleMorphWeights(weights, voice_count);
    if (simple_morph_panel_) {
      simple_morph_panel_->SetWeights(weights);
    }
    ApplyVoiceMorphState(preset.advanced_morph_state);
    SetSimpleMorphMode(preset.simple_morph_mode);
    if (preset.simple_morph_mode) {
      ApplySimpleMorphWeights(weights, voice_count);
    }
  }
  applying_preset_ = false;
  UpdateCompensatedDriveUi();
  SavePresets();
  RefreshPresetPanel(index);
}

void Editor::RenamePreset(const int index, const std::string& name) {
  if (index < 0 || index >= static_cast<int>(presets_.size()) ||
      name.empty()) {
    return;
  }
  presets_[index].name = name;
  SavePresets();
  RefreshPresetPanel(index);
}

void Editor::MovePreset(const int index, const int destination) {
  if (index < 0 || index >= static_cast<int>(presets_.size()) ||
      destination < 0 || destination >= static_cast<int>(presets_.size()) ||
      index == destination) {
    return;
  }
  auto preset = std::move(presets_[index]);
  presets_.erase(presets_.begin() + index);
  presets_.insert(presets_.begin() + destination, std::move(preset));
  if (selected_preset_ == index) {
    selected_preset_ = destination;
  } else if (index < selected_preset_ && selected_preset_ <= destination) {
    --selected_preset_;
  } else if (destination <= selected_preset_ && selected_preset_ < index) {
    ++selected_preset_;
  }
  SavePresets();
  RefreshPresetPanel(selected_preset_);
}

void Editor::DeletePreset(const int index) {
  if (index < 0 || index >= static_cast<int>(presets_.size())) {
    return;
  }
  presets_.erase(presets_.begin() + index);
  if (presets_.empty()) {
    selected_preset_ = -1;
  } else if (selected_preset_ == index) {
    selected_preset_ = std::max(0, index - 1);
  } else if (selected_preset_ > index) {
    --selected_preset_;
  }
  SavePresets();
  if (selected_preset_ >= 0) {
    ApplyPreset(selected_preset_);
  } else {
    RefreshPresetPanel();
  }
}

void Editor::RefreshPresetPanel(const int selected) {
  if (preset_panel_) {
    preset_panel_->SetBanks(preset_workspace_.banks,
                            preset_workspace_.selected_bank);
    auto thumbnails = std::vector<SharedPointer<CBitmap>>(presets_.size());
    if (model_config_.has_value()) {
      auto* const controller = static_cast<Controller*>(getController());
      const auto& model_value = *std::get<std::unique_ptr<std::u8string>>(
          controller->core_.parameter_state_.GetValue(ParameterID::kModel));
      const auto voice_count = common::GetVoiceCount(*model_config_);
      for (auto i = 0; i < static_cast<int>(presets_.size()); ++i) {
        if (presets_[i].model_path != model_value || presets_[i].voice < 0 ||
            presets_[i].voice >= voice_count) {
          continue;
        }
        const auto& portrait =
            model_config_->voices[presets_[i].voice].portrait.path;
        if (const auto it = portrait_menu_thumbnails_.find(portrait);
            it != portrait_menu_thumbnails_.end()) {
          thumbnails[i] = it->second;
        }
      }
    }
    const auto visible_selected =
        active_preset_bank_ == preset_workspace_.selected_bank ? selected : -1;
    preset_panel_->SetPresets(presets_, visible_selected,
                              std::move(thumbnails));
  }
}

void Editor::SavePresets() {
  if (preset_save_timer_) {
    preset_save_timer_->stop();
    preset_save_timer_ = nullptr;
  }
  preset_save_pending_ = false;
  SyncCurrentPresetBank();
  const auto serialized =
      common::SerializePresetWorkspace(preset_workspace_);
  const auto value = std::u8string(serialized.begin(), serialized.end());
  auto* const controller = static_cast<Controller*>(getController());
  const auto vst_param_id = static_cast<ParamID>(ParameterID::kPresetData);
  controller->SetStringParameter(vst_param_id, value);
  if (const auto message = Steinberg::owned(controller->allocateMessage())) {
    message->setMessageID("param_change");
    message->getAttributes()->setBinary("param_id", &vst_param_id,
                                        sizeof(vst_param_id));
    message->getAttributes()->setBinary(
        "data", value.data(), static_cast<Steinberg::uint32>(value.size()));
    controller->sendMessage(message);
  }
  controller->setDirty(true);
}

void Editor::ImportPresets(const int mode) {
  auto* const selector =
      CNewFileSelector::create(getFrame(), CNewFileSelector::kSelectFile);
  if (!selector) {
    return;
  }
  selector->setTitle("Import Beatrice Presets");
  selector->addFileExtension(
      CFileExtension("Beatrice Presets", "beatrice-presets"));
  selector->addFileExtension(CFileExtension("TOML", "toml"));
  if (selector->runModal() && selector->getNumSelectedFiles() > 0) {
    if (const auto* const selected = selector->getSelectedFile(0)) {
      auto imported = std::vector<common::Preset>{};
      const auto path = std::filesystem::path(std::u8string(
          reinterpret_cast<const char8_t*>(selected)));
      auto imported_workspace = common::PresetWorkspace{};
      auto input = std::ifstream(path, std::ios::binary);
      const auto serialized = std::string(
          std::istreambuf_iterator<char>(input),
          std::istreambuf_iterator<char>());
      auto import_error = std::string{};
      auto import_report = common::PresetImportReport{};
      if ((input.good() || input.eof()) &&
          common::DeserializePresetWorkspace(serialized,
                                             imported_workspace, &import_error,
                                             &import_report)) {
        for (const auto& bank : imported_workspace.banks) {
          imported.insert(imported.end(), bank.presets.begin(),
                          bank.presets.end());
        }
        if (mode == 0) {
          presets_.insert(presets_.end(), imported.begin(), imported.end());
          // Imports target the selected tab. Restore the exported tab name as
          // part of that tab's data, rather than retaining the destination
          // name.
          if (!imported_workspace.banks.empty() &&
              !imported_workspace.banks.front().name.empty() &&
              preset_workspace_.selected_bank >= 0 &&
              preset_workspace_.selected_bank <
                  static_cast<int>(preset_workspace_.banks.size())) {
            preset_workspace_.banks[preset_workspace_.selected_bank].name =
                imported_workspace.banks.front().name;
          }
        } else if (mode == 1) {
          presets_ = std::move(imported);
          selected_preset_ = -1;
        } else {
          SyncCurrentPresetBank();
          auto bank = imported_workspace.banks.empty()
                          ? common::PresetBank{}
                          : imported_workspace.banks.front();
          if (bank.name.empty()) {
            bank.name = path.stem().string();
          }
          bank.id = static_cast<std::uint64_t>(
              std::chrono::steady_clock::now().time_since_epoch().count());
          preset_workspace_.banks.push_back(std::move(bank));
          preset_workspace_.selected_bank =
              static_cast<int>(preset_workspace_.banks.size()) - 1;
          LoadCurrentPresetBank();
        }
        SavePresets();
        RefreshPresetPanel();
      }
    }
  }
  selector->forget();
}

void Editor::ExportPresets(const bool all_banks) {
  SyncCurrentPresetBank();
  auto exported_workspace = common::PresetWorkspace{};
  if (!all_banks && preset_workspace_.selected_bank >= 0 &&
      preset_workspace_.selected_bank <
          static_cast<int>(preset_workspace_.banks.size())) {
    exported_workspace.banks.push_back(
        preset_workspace_.banks[preset_workspace_.selected_bank]);
    exported_workspace.selected_bank = 0;
  }

  auto tab_name = std::string("Presets");
  if (!exported_workspace.banks.empty() &&
      !exported_workspace.banks.front().name.empty()) {
    tab_name = exported_workspace.banks.front().name;
  }
  for (auto& ch : tab_name) {
    if (ch == '<' || ch == '>' || ch == ':' || ch == '"' || ch == '/' ||
        ch == '\\' || ch == '|' || ch == '?' || ch == '*') {
      ch = '_';
    }
  }
  while (!tab_name.empty() &&
         (tab_name.back() == ' ' || tab_name.back() == '.')) {
    tab_name.pop_back();
  }
  if (tab_name.empty()) {
    tab_name = "Presets";
  }

  const auto now = std::chrono::system_clock::to_time_t(
      std::chrono::system_clock::now());
  auto local_time = std::tm{};
#if defined(_WIN32)
  localtime_s(&local_time, &now);
#else
  localtime_r(&now, &local_time);
#endif
  auto default_name = std::ostringstream{};
  default_name << std::put_time(&local_time, "%Y-%m-%d") << '-'
               << tab_name << ".beatrice-presets";

  auto* const selector = CNewFileSelector::create(
      getFrame(), CNewFileSelector::kSelectSaveFile);
  if (!selector) {
    return;
  }
  const auto extension =
      CFileExtension("Beatrice Presets", "beatrice-presets");
  selector->setTitle("Export Beatrice Presets");
  selector->setDefaultExtension(extension);
  selector->addFileExtension(extension);
  const auto default_name_string = default_name.str();
  selector->setDefaultSaveName(default_name_string.c_str());
  if (selector->runModal() && selector->getNumSelectedFiles() > 0) {
    if (const auto* const selected = selector->getSelectedFile(0)) {
      const auto path = std::filesystem::path(std::u8string(
          reinterpret_cast<const char8_t*>(selected)));
      auto output = std::ofstream(path, std::ios::binary);
      if (output) {
        if (all_banks) {
          output << common::SerializePresetWorkspace(preset_workspace_);
        } else {
          // Use workspace serialization even for one tab so its name and
          // selected preset are preserved during transfer to another instance.
          output << common::SerializePresetWorkspace(exported_workspace);
        }
      }
      if (preset_panel_) preset_panel_->SetExportFailed(!output);
    }
  }
  selector->forget();
}

void Editor::SyncCurrentPresetBank() {
  if (preset_workspace_.selected_bank < 0 ||
      preset_workspace_.selected_bank >=
          static_cast<int>(preset_workspace_.banks.size())) {
    return;
  }
  auto& bank = preset_workspace_.banks[preset_workspace_.selected_bank];
  bank.presets = presets_;
  bank.selected_preset = selected_preset_;
}

void Editor::LoadCurrentPresetBank() {
  if (preset_workspace_.banks.empty()) {
    preset_workspace_.banks.push_back(common::PresetBank{});
  }
  preset_workspace_.selected_bank = std::clamp(
      preset_workspace_.selected_bank, 0,
      static_cast<int>(preset_workspace_.banks.size()) - 1);
  const auto& bank = preset_workspace_.banks[preset_workspace_.selected_bank];
  presets_ = bank.presets;
  selected_preset_ = bank.selected_preset >= 0 &&
                             bank.selected_preset <
                                 static_cast<int>(presets_.size())
                         ? bank.selected_preset
                         : -1;
  if (active_preset_bank_ < 0) {
    active_preset_bank_ = preset_workspace_.selected_bank;
  }
}

void Editor::AddPresetBank() {
  SyncCurrentPresetBank();
  const auto number = preset_workspace_.banks.size() + 1;
  preset_workspace_.banks.push_back(common::PresetBank{
      .id = static_cast<std::uint64_t>(
          std::chrono::steady_clock::now().time_since_epoch().count()),
      .name = "Preset " + std::to_string(number),
  });
  preset_workspace_.selected_bank =
      static_cast<int>(preset_workspace_.banks.size()) - 1;
  LoadCurrentPresetBank();
  SavePresets();
  if (preset_panel_) {
    preset_panel_->RevealNewBankOnNextRebuild();
  }
  RefreshPresetPanel();
}

void Editor::SelectPresetBank(const int index) {
  if (index < 0 || index >= static_cast<int>(preset_workspace_.banks.size()) ||
      index == preset_workspace_.selected_bank) {
    return;
  }
  SyncCurrentPresetBank();
  preset_workspace_.selected_bank = index;
  LoadCurrentPresetBank();
  SavePresets();
  // Changing the bank only changes which list is being viewed.  A remembered
  // row is shown only when returning to the bank that owns the active preset;
  // a different bank remains visually unselected until the user clicks a row.
  RefreshPresetPanel(selected_preset_);
}

void Editor::RenamePresetBank(const int index, const std::string& name) {
  if (index < 0 || index >= static_cast<int>(preset_workspace_.banks.size()) ||
      name.empty()) {
    return;
  }
  preset_workspace_.banks[index].name = name;
  SavePresets();
  RefreshPresetPanel(selected_preset_);
}

void Editor::MovePresetBank(const int index, const int destination) {
  auto& banks = preset_workspace_.banks;
  if (index < 0 || index >= static_cast<int>(banks.size()) ||
      destination < 0 || destination >= static_cast<int>(banks.size()) ||
      index == destination) {
    return;
  }
  SyncCurrentPresetBank();
  auto bank = std::move(banks[index]);
  banks.erase(banks.begin() + index);
  banks.insert(banks.begin() + destination, std::move(bank));
  auto& selected = preset_workspace_.selected_bank;
  if (selected == index) {
    selected = destination;
  } else if (index < selected && selected <= destination) {
    --selected;
  } else if (destination <= selected && selected < index) {
    ++selected;
  }
  LoadCurrentPresetBank();
  SavePresets();
  RefreshPresetPanel(selected_preset_);
}

void Editor::DeletePresetBank(const int index) {
  if (preset_workspace_.banks.size() <= 1 || index < 0 ||
      index >= static_cast<int>(preset_workspace_.banks.size())) {
    return;
  }
  SyncCurrentPresetBank();
  preset_workspace_.banks.erase(preset_workspace_.banks.begin() + index);
  preset_workspace_.selected_bank = std::clamp(
      index - 1, 0, static_cast<int>(preset_workspace_.banks.size()) - 1);
  LoadCurrentPresetBank();
  SavePresets();
  RefreshPresetPanel(selected_preset_);
}

void Editor::UpdateSelectedPresetFromCurrentState(
    const ParamID changed_parameter_id, const bool save_now) {
  if (applying_preset_ || selected_preset_ < 0 ||
      selected_preset_ >= static_cast<int>(presets_.size())) {
    return;
  }
  if (common::IsInputCleanupParameter(
          static_cast<ParameterID>(changed_parameter_id))) {
    return;
  }
  auto& preset = presets_[selected_preset_];
  auto* const controller = static_cast<Controller*>(getController());
  const auto& state = controller->core_.parameter_state_;
  const auto value = [&state](const ParameterID id) {
    return std::get<double>(state.GetValue(id));
  };
  const auto changed_parameter = static_cast<ParameterID>(changed_parameter_id);
  const auto is_advanced_morph = common::IsVoiceMorphParameter(changed_parameter);
  switch (changed_parameter) {
    case ParameterID::kModel:
    case ParameterID::kVoice:
    case ParameterID::kInputGain:
    case ParameterID::kOutputGain:
    case ParameterID::kCompensatedDrive:
    case ParameterID::kDeMud:
    case ParameterID::kPresence:
    case ParameterID::kReverbMix:
    case ParameterID::kReverbDecay:
    case ParameterID::kReverbTone:
    case ParameterID::kPitchShift:
    case ParameterID::kFormantShift:
    case ParameterID::kVQNumNeighbors:
    case ParameterID::kAverageSourcePitch:
    case ParameterID::kMinSourcePitch:
    case ParameterID::kMaxSourcePitch:
    case ParameterID::kIntonationIntensity:
    case ParameterID::kPitchCorrection:
    case ParameterID::kLock:
    case ParameterID::kPitchCorrectionType:
    case ParameterID::kSimpleMorphWeights:
      break;
    default:
      if (!is_advanced_morph) return;
  }
  const auto& model = *std::get<std::unique_ptr<std::u8string>>(
      state.GetValue(ParameterID::kModel));
  const auto& weights = *std::get<std::unique_ptr<std::u8string>>(
      state.GetValue(ParameterID::kSimpleMorphWeights));
  preset.model_path = model;
  preset.voice = std::get<int>(state.GetValue(ParameterID::kVoice));
  preset.input_gain = value(ParameterID::kInputGain);
  preset.output_gain = value(ParameterID::kOutputGain);
  preset.compensated_drive = value(ParameterID::kCompensatedDrive);
  preset.de_mud = value(ParameterID::kDeMud);
  preset.presence = value(ParameterID::kPresence);
  preset.reverb_mix = value(ParameterID::kReverbMix);
  preset.reverb_decay = value(ParameterID::kReverbDecay);
  preset.reverb_tone = value(ParameterID::kReverbTone);
  preset.pitch_shift = value(ParameterID::kPitchShift);
  preset.formant_shift = value(ParameterID::kFormantShift);
  preset.vq_neighbor_count =
      static_cast<int>(std::round(value(ParameterID::kVQNumNeighbors)));
  preset.average_source_pitch = value(ParameterID::kAverageSourcePitch);
  preset.min_source_pitch = value(ParameterID::kMinSourcePitch);
  preset.max_source_pitch = value(ParameterID::kMaxSourcePitch);
  preset.intonation_intensity = value(ParameterID::kIntonationIntensity);
  preset.pitch_correction = value(ParameterID::kPitchCorrection);
  preset.pitch_control = std::get<int>(state.GetValue(ParameterID::kLock));
  preset.pitch_correction_type =
      std::get<int>(state.GetValue(ParameterID::kPitchCorrectionType));
  preset.simple_morph_weights = weights;
  preset.advanced_morph_state = voice_morph_state_;
  preset.simple_morph_mode = simple_morph_mode_;
  auto renamed = false;
  if (changed_parameter == ParameterID::kVoice) {
    renamed = RenameSelectedPresetFromCurrentModelVoice();
  }
  if (save_now) {
    SavePresets();
  } else {
    preset_save_pending_ = true;
  }
  if (renamed) {
    RefreshPresetPanel(selected_preset_);
  }
}

void Editor::ScheduleDeferredPresetSave() {
  if (!preset_save_pending_) {
    return;
  }
  if (preset_save_timer_) {
    preset_save_timer_->stop();
    preset_save_timer_ = nullptr;
  }
  preset_save_timer_ = VSTGUI::owned(new VSTGUI::CVSTGUITimer(
      [this](VSTGUI::CVSTGUITimer* const timer) {
        timer->stop();
        preset_save_timer_ = nullptr;
        FlushDeferredPresetSave();
      },
      200, true));
}

void Editor::FlushDeferredPresetSave() {
  if (!preset_save_pending_) {
    return;
  }
  preset_save_pending_ = false;
  SavePresets();
}

void Editor::PerformParameterEdit(const ParamID param_id,
                                  const ParamValue normalized_value) {
  auto* const controller = static_cast<Controller*>(getController());
  if (controller->setParamNormalized(param_id, normalized_value) ==
      Steinberg::kResultTrue) {
    controller->performEdit(param_id, controller->getParamNormalized(param_id));
  }
}

void Editor::SendParameterEdit(const ParamID param_id,
                               const ParamValue normalized_value) {
  auto* const controller = static_cast<Controller*>(getController());
  controller->beginEdit(param_id);
  PerformParameterEdit(param_id, normalized_value);
  controller->endEdit(param_id);
}

// DAW 側から GUI にパラメータ変更を伝える。
// Voice など、controller と editor で最大値が異なるパラメータがあるため、
// controller->setValueNormalized は使わない。
// valueChanged からも controller を介して間接的に呼ばれる。
// 引数じゃなくて core から値を取った方が良い？
void Editor::SyncValue(const ParamID param_id, const float plain_value) {
  const auto parameter_id = static_cast<ParameterID>(param_id);
  if (parameter_id == ParameterID::kBypass) {
    UpdateBypassUi(plain_value > 0.5F);
    return;
  }
  if (common::IsVoiceMorphParameter(parameter_id)) {
    auto* const controller = static_cast<Controller*>(getController());
    ApplyVoiceMorphState(
        common::GetVoiceMorphState(controller->core_.parameter_state_));
    return;
  }

  if (!frame || !controls_.contains(param_id)) {
    return;
  }
  auto* const control = controls_.at(param_id);
  // Voice は色々ややこしいので特別扱いする
  if (param_id == static_cast<ParamID>(ParameterID::kVoice)) {
    const auto voice_id = static_cast<int>(std::round(plain_value));
    control->setValue(plain_value);
    if (!model_config_.has_value()) {
      portrait_view_->setBackground(nullptr);
      portrait_view_->setVisible(false);
      unloaded_logo_view_->setVisible(true);
      morph_pad_view_->setVisible(false);
      UpdateMorphUiVisibility(false);
      SetPortraitDescriptionMode(false);
      SetPortraitDescriptionText(u8"");
      SetVoiceDescriptionText(u8"");
      SetVoiceSelectorDisplay(-1);
    } else if (voice_id == 0 ||
               voice_id < static_cast<int>(control->getMax())) {
      portrait_view_->setBackground(
          portraits_.at(model_config_->voices[voice_id].portrait.path).get());
      portrait_view_->setVisible(true);
      unloaded_logo_view_->setVisible(false);
      morph_pad_view_->setVisible(false);
      UpdateMorphUiVisibility(false);
      SetPortraitDescriptionMode(false);
      SetVoiceSelectorDisplay(voice_id);
      SetPortraitDescriptionText(
          model_config_->voices[voice_id].portrait.description);
      SetVoiceDescriptionText(model_config_->voices[voice_id].description);
    } else {
      portrait_view_->setBackground(nullptr);
      portrait_view_->setVisible(false);
      unloaded_logo_view_->setVisible(false);
      morph_pad_view_->setVisible(true);
      UpdateMorphUiVisibility(true);
      SetPortraitDescriptionMode(true);
      SetPortraitDescriptionText(u8"");
      SetVoiceSelectorDisplay(-2);
      UpdateVoiceMorphingDescription();
    }
    if (voice_menu_overlay_ && voice_menu_overlay_->IsMenuVisible()) {
      RebuildVoiceMenu();
    }
  } else {
    // Host/upstream values may legitimately exceed this editor's compact
    // operating range. Keep the authoritative value in controller/processor
    // state and clamp only the visual control.
    control->setValue(ui_limits::ClampForDisplay(parameter_id, plain_value));
  }
  if (param_id == static_cast<ParamID>(ParameterID::kLock)) {
    UpdateSourcePitchVisibility();
  }
  if (param_id == static_cast<ParamID>(ParameterID::kCompensatedDrive) ||
      param_id == static_cast<ParamID>(ParameterID::kInputGain)) {
    UpdateCompensatedDriveUi();
  }
  control->setDirty();
}

void Editor::SyncStringValue(const ParamID param_id,
                             const std::u8string& value) {
  if (param_id == static_cast<ParamID>(ParameterID::kPresetData)) {
    const auto serialized = std::string(
        reinterpret_cast<const char*>(value.data()), value.size());
    auto restored = common::PresetWorkspace{};
    if (common::DeserializePresetWorkspace(serialized, restored)) {
      preset_workspace_ = std::move(restored);
      LoadCurrentPresetBank();
      RefreshPresetPanel(selected_preset_);
    }
    return;
  }
  if (!frame || !controls_.contains(param_id)) {
    return;
  }
  auto* const control = static_cast<CTextLabel*>(controls_.at(param_id));
  if (param_id == static_cast<ParamID>(ParameterID::kModel)) {
    auto* const model_selector = static_cast<FileSelector*>(control);
    model_selector->SetPath(value);
    SyncModelDescription();
    SyncSourcePitchRange();
    SyncParameterAvailability();
    if (rename_selected_preset_after_model_load_) {
      rename_selected_preset_after_model_load_ = false;
      if (RenameSelectedPresetFromCurrentModelVoice()) {
        SavePresets();
        RefreshPresetPanel(selected_preset_);
      }
    }
  } else {
    control->setText(reinterpret_cast<const char*>(value.c_str()));
  }
}

// 現在読み込まれているモデルをもとに
// min_source_pitch, max_source_pitch の範囲を更新する。
void Editor::SyncSourcePitchRange() {
  auto* const min_source_pitch_slider = static_cast<Slider*>(
      controls_.at(static_cast<ParamID>(ParameterID::kMinSourcePitch)));
  auto* const max_source_pitch_slider = static_cast<Slider*>(
      controls_.at(static_cast<ParamID>(ParameterID::kMaxSourcePitch)));
  // MIDI ノートナンバー
  const auto range = ui_limits::GetSliderRange(
      ParameterID::kMinSourcePitch);
  if (range) {
    min_source_pitch_slider->setMin(range->minimum);
    min_source_pitch_slider->setMax(range->maximum);
    max_source_pitch_slider->setMin(range->minimum);
    max_source_pitch_slider->setMax(range->maximum);
  }
  min_source_pitch_slider->setDirty();
  max_source_pitch_slider->setDirty();
}

// 現在読み込まれているモデルをもとに
// パラメータの有効/無効を更新する。
void Editor::SyncParameterAvailability() {
  if (!model_config_.has_value() || model_config_->model.VersionInt() < 0) {
    return;
  }
  auto* const vq_num_neighbors_slider = static_cast<Slider*>(
      controls_.at(static_cast<ParamID>(ParameterID::kVQNumNeighbors)));
  vq_num_neighbors_slider->SetEnabled(model_config_->model.VersionInt() >= 2);
  vq_num_neighbors_slider->setDirty();
}

// model_selector->getPath() をもとに
// 話者リスト等を更新する
void Editor::SyncModelDescription() {
  auto* const controller = static_cast<Controller*>(getController());
  auto* const model_selector = static_cast<FileSelector*>(
      controls_.at(static_cast<ParamID>(ParameterID::kModel)));
  auto* const voice_menu = static_cast<COptionMenu*>(
      controls_.at(static_cast<ParamID>(ParameterID::kVoice)));
  if (voice_menu_overlay_) {
    voice_menu_overlay_->ClearRememberedScroll();
  }
  const auto file = model_selector->GetPath();
  model_selector->setText("<unloaded>");
  voice_menu->removeAllEntry();
  SetModelDescriptionText(u8"");
  SetVoiceDescriptionText(u8"");
  SetPortraitDescriptionText(u8"");
  if (portrait_view_) {
    portrait_view_->setBackground(nullptr);
    portrait_view_->setVisible(false);
  }
  SetPortraitDescriptionMode(false);
  if (unloaded_logo_view_) {
    unloaded_logo_view_->setVisible(true);
  }
  if (morph_pad_view_) {
    morph_pad_view_->setVisible(false);
  }
  UpdateMorphUiVisibility(false);
  model_config_ = std::nullopt;
  SetVoiceSelectorDisplay(-1);
  RebuildVoiceMenu();
  portraits_.clear();
  portrait_menu_thumbnails_.clear();
  portrait_marker_thumbnails_.clear();
  if (morph_pad_view_) {
    morph_pad_view_->SetVoices({}, {});
  }
  if (file.empty()) {
    // 初期状態
    return;
  }
  auto file_error = std::error_code{};
  auto stream = std::ifstream{};
  if (std::filesystem::is_regular_file(file, file_error)) {
    stream.open(file, std::ios::binary);
  }
  if (!stream.is_open()) {
    // ファイルが移動して読み込めない場合の分岐だが、
    // モデルを読み込んだ後に GUI を閉じモデルファイルを移動して
    // 再び GUI を開いた場合などには
    // Processor のみ読み込まれている可能性がある。
    model_selector->setText("<failed to load>");
    SetModelDescriptionText(
        u8"Error: The model could not be loaded due to a file move or another "
        u8"issue. Please reload a valid model.");
    return;
  }
  try {
    const auto source_path = file.u8string();
    const auto source_name = std::string(
        reinterpret_cast<const char*>(source_path.data()), source_path.size());
    const auto toml_data = toml::parse(stream, source_name);
    model_config_ = toml::get<common::ModelConfig>(toml_data);
    if (model_config_->model.VersionInt() == -1) {
      SetModelDescriptionText(u8"Error: Unknown model version.");
      return;
    }
    model_selector->setText(
        reinterpret_cast<const char*>(model_config_->model.name.c_str()));
    const auto voice_count = common::GetVoiceCount(*model_config_);
    // 話者のリストを読み込む。
    // また、予め portrait を読み込んで、必要に応じてリサイズしておく。
    for (auto i = 0; i < voice_count; ++i) {
      const auto& voice = model_config_->voices[i];
      voice_menu->addEntry(reinterpret_cast<const char*>(voice.name.c_str()));

      // portrait
      {
        if (portraits_.contains(voice.portrait.path)) {
          goto load_portrait_succeeded;
        }
        const auto portrait_file = file.parent_path() / voice.portrait.path;
        auto portrait_error = std::error_code{};
        if (!std::filesystem::is_regular_file(portrait_file, portrait_error)) {
          goto load_portrait_failed;
        }
        const auto platform_bitmap = getPlatformFactory().createBitmapFromPath(
            reinterpret_cast<const char*>(portrait_file.u8string().c_str()));
        if (!platform_bitmap) {
          goto load_portrait_failed;
        }
        const auto original_bitmap =
            VSTGUI::owned(new CBitmap(platform_bitmap));
        auto rounded_portrait = MakeRoundedBitmap(
            original_bitmap.get(), kPortraitWidth, kPortraitHeight, 4.0);
        auto menu_thumbnail = ScaleBitmap(original_bitmap.get(), 42, 42);
        auto circular_thumbnail =
            MakeRoundedBitmap(original_bitmap.get(), 58, 58, 29.0);
        if (!rounded_portrait || !menu_thumbnail || !circular_thumbnail) {
          goto load_portrait_failed;
        }
        portraits_.insert({voice.portrait.path, rounded_portrait});
        portrait_menu_thumbnails_.insert({voice.portrait.path, menu_thumbnail});
        portrait_marker_thumbnails_.insert(
            {voice.portrait.path, circular_thumbnail});
        goto load_portrait_succeeded;
      }
      assert(false);
    load_portrait_failed:
      portraits_.insert({voice.portrait.path, nullptr});
      portrait_menu_thumbnails_.insert({voice.portrait.path, nullptr});
      portrait_marker_thumbnails_.insert({voice.portrait.path, nullptr});
    load_portrait_succeeded: {}
    }

    if (voice_count > 1) {
      const auto flags = model_config_->model.VersionInt() <= 2
                             ? VSTGUI::CMenuItem::kNoFlags
                             : VSTGUI::CMenuItem::kDisabled;
      voice_menu->addEntry("Voice Morphing Mode", -1, flags);
      portraits_.insert({u8"", nullptr});
      portrait_menu_thumbnails_.insert({u8"", nullptr});
      portrait_marker_thumbnails_.insert({u8"", nullptr});
    }

    if (morph_pad_view_) {
      std::vector<SharedPointer<CBitmap>> marker_bitmaps;
      std::vector<std::string> voice_names;
      marker_bitmaps.reserve(static_cast<size_t>(voice_count));
      voice_names.reserve(static_cast<size_t>(voice_count));
      for (auto i = 0; i < voice_count; ++i) {
        const auto& path = model_config_->voices[i].portrait.path;
        if (const auto it = portrait_marker_thumbnails_.find(path);
            it != portrait_marker_thumbnails_.end()) {
          marker_bitmaps.push_back(it->second);
        } else {
          marker_bitmaps.emplace_back(nullptr);
        }
        const auto& name = model_config_->voices[i].name;
        voice_names.emplace_back(name.begin(), name.end());
      }
      morph_pad_view_->SetVoices(marker_bitmaps, voice_names);
    }
    if (simple_morph_panel_) {
      const auto& stored = controller->core_.parameter_state_.GetValue(
          ParameterID::kSimpleMorphWeights);
      const auto& serialized =
          *std::get<std::unique_ptr<std::u8string>>(stored);
      auto weights = common::ParseSimpleMorphWeights(serialized);
      weights = common::NormalizeSimpleMorphWeights(weights, voice_count);
      simple_morph_panel_->SetVoices(*model_config_, weights);
    }

    voice_menu->setDirty();
    ApplyVoiceMorphState(
        common::GetVoiceMorphState(controller->core_.parameter_state_));
    auto voice_id =
        Denormalize(std::get<common::ListParameter>(
                        common::kSchema.GetParameter(ParameterID::kVoice)),
                    controller->getParamNormalized(
                        static_cast<ParamID>(ParameterID::kVoice)));
    if (voice_id < voice_count) {
      const auto& voice = model_config_->voices[voice_id];
      portrait_view_->setBackground(portraits_.at(voice.portrait.path).get());
      portrait_view_->setVisible(true);
      unloaded_logo_view_->setVisible(false);
      morph_pad_view_->setVisible(false);
      UpdateMorphUiVisibility(false);
      SetPortraitDescriptionMode(false);
      SetPortraitDescriptionText(voice.portrait.description);
      SetVoiceDescriptionText(voice.description);
      SetVoiceSelectorDisplay(voice_id);
    } else {
      portrait_view_->setBackground(nullptr);
      portrait_view_->setVisible(false);
      unloaded_logo_view_->setVisible(false);
      morph_pad_view_->setVisible(true);
      UpdateMorphUiVisibility(true);
      SetPortraitDescriptionMode(true);
      SetPortraitDescriptionText(u8"");
      SetVoiceSelectorDisplay(-2);
      UpdateVoiceMorphingDescription();
    }
    SetModelDescriptionText(model_config_->model.description);
    RebuildVoiceMenu();

    portrait_view_->setDirty();
    if (portrait_description_pane_) {
      portrait_description_pane_->setDirty();
    }
    unloaded_logo_view_->setDirty();
    morph_pad_view_->setDirty();
  } catch (const std::exception& e) {
    model_selector->setText("<failed to load>");
    SetModelDescriptionText(
        u8"Error:\n" +
        std::u8string(e.what(), e.what() + std::strlen(e.what())));
    return;
  }
}

// GUI でパラメータに変更があったときに、DAW に伝える。
// あとダブルクリックでデフォルトに戻したい。
void Editor::valueChanged(CControl* const pControl) {
  assert(pControl);
  if (!pControl) {
    return;
  }
  if (const auto column = ColumnForView(pControl);
      column != FocusColumn::kNone) {
    SetFocusedColumn(column);
  }
  const auto vst_param_id = pControl->getTag();
  const auto param_id = static_cast<ParameterID>(vst_param_id);
  const auto& param = common::kSchema.GetParameter(param_id);
  auto* const controller = static_cast<Controller*>(getController());
  auto& core = controller->core_;
  // 各々の Control でやるべきという感じも
  if (auto* const control = dynamic_cast<Slider*>(pControl)) {
    // SendParameterEdit 含めて controller の中に処理書いた方が明快？
    if (const auto* const num_param =
            std::get_if<common::NumberParameter>(&param)) {
      auto requested_value = static_cast<double>(control->getValue());
      requested_value = ui_limits::QuantizeSliderValue(
          param_id, requested_value, control->IsFineAdjustment());
      const auto normalized_value = Normalize(*num_param, requested_value);
      const auto plain_value = Denormalize(*num_param, normalized_value);
      const auto control_value = static_cast<float>(plain_value);
      if (control->getValue() != control_value) {
        control->setValue(control_value);
        control->invalid();
      }
      if (plain_value ==
          std::get<double>(core.parameter_state_.GetValue(param_id))) {
        return;
      }
      core.parameter_state_.SetValue(param_id, plain_value);
      [[maybe_unused]] const auto error_code =
          num_param->ControllerSetValue(core, plain_value);
      assert(error_code == common::ErrorCode::kSuccess);
      PerformParameterEdit(vst_param_id, normalized_value);
      if (param_id == ParameterID::kCompensatedDrive) {
        UpdateCompensatedDriveUi();
      }
    } else if (const auto* const list_param =
                   std::get_if<common::ListParameter>(&param)) {
      const auto plain_value = std::clamp(
          static_cast<int>(std::round(control->getValue())), 0,
          static_cast<int>(list_param->GetValues().size()) - 1);
      if (control->getValue() != static_cast<float>(plain_value)) {
        control->setValue(static_cast<float>(plain_value));
        control->invalid();
      }
      if (plain_value ==
          std::get<int>(core.parameter_state_.GetValue(param_id))) {
        return;
      }
      const auto normalized_value = Normalize(*list_param, plain_value);
      const auto error_code = list_param->ControllerSetValue(core, plain_value);
      assert(error_code == common::ErrorCode::kSuccess);
      static_cast<void>(error_code);
      PerformParameterEdit(vst_param_id, normalized_value);
    } else {
      assert(false);
      return;
    }
  } else if (auto* const control = dynamic_cast<COptionMenu*>(pControl)) {
    const auto* const list_param = std::get_if<common::ListParameter>(&param);
    assert(list_param);
    const auto plain_value = static_cast<int>(control->getValue());
    if (plain_value ==
        std::get<int>(core.parameter_state_.GetValue(param_id))) {
      return;
    }
    const auto normalized_value = Normalize(*list_param, plain_value);
    const auto error_code = list_param->ControllerSetValue(core, plain_value);
    if (error_code == common::ErrorCode::kSpeakerIDOutOfRange) {
      // これが表示されることは無いはず
      SetVoiceDescriptionText(u8"Error: Speaker ID out of range.");
    }
    assert(error_code == common::ErrorCode::kSuccess);
    PerformParameterEdit(vst_param_id, normalized_value);
    if (param_id == ParameterID::kLatencyReporting) {
      if (const auto message = Steinberg::owned(controller->allocateMessage())) {
        message->setMessageID("param_change");
        message->getAttributes()->setBinary("param_id", &vst_param_id,
                                            sizeof(vst_param_id));
        message->getAttributes()->setInt("data", plain_value);
        controller->sendMessage(message);
      }
    }
  } else if (auto* const control = dynamic_cast<FileSelector*>(pControl)) {
    const auto* const str_param = std::get_if<common::StringParameter>(&param);
    assert(str_param);
    const auto file = control->GetPath().u8string();
    auto error_code = str_param->ControllerSetValue(core, file);
    if (error_code == common::ErrorCode::kFileOpenError ||
        error_code == common::ErrorCode::kTOMLSyntaxError ||
        error_code == common::ErrorCode::kInvalidModelConfig) {
      // Controller とは別に Editor::SyncModelDescription でも改めて
      // ファイルを読み込もうとして失敗するので、ここではエラー処理しない
      error_code = common::ErrorCode::kSuccess;
    }
    assert(error_code == common::ErrorCode::kSuccess);
    if (error_code != common::ErrorCode::kSuccess) {
      return;
    }
    if (param_id == ParameterID::kModel && !applying_preset_ &&
        selected_preset_ >= 0 &&
        selected_preset_ < static_cast<int>(presets_.size())) {
      // SetStringParameter synchronously reloads model metadata in every open
      // editor. Arm the rename before entering that call.
      rename_selected_preset_after_model_load_ = true;
    }
    controller->SetStringParameter(vst_param_id, file);
    // processor に通知
    if (const auto msg = Steinberg::owned(controller->allocateMessage())) {
      msg->setMessageID("param_change");
      msg->getAttributes()->setBinary("param_id", &vst_param_id,
                                      sizeof(vst_param_id));
      msg->getAttributes()->setBinary(
          "data", file.c_str(), static_cast<Steinberg::uint32>(file.size()));
      controller->sendMessage(msg);
    }
  } else {
    assert(false);
  }

  // 連動するパラメータの処理
  for (const auto& param_id : core.updated_parameters_) {
    const auto vst_param_id = static_cast<ParamID>(param_id);
    const auto& value = core.parameter_state_.GetValue(param_id);
    const auto& param = common::kSchema.GetParameter(param_id);
    if (const auto* const num_param =
            std::get_if<common::NumberParameter>(&param)) {
      const auto normalized_value =
          Normalize(*num_param, std::get<double>(value));
      SendParameterEdit(vst_param_id, normalized_value);
    } else if (const auto* const list_param =
                   std::get_if<common::ListParameter>(&param)) {
      const auto normalized_value =
          Normalize(*list_param, std::get<int>(value));
      SendParameterEdit(vst_param_id, normalized_value);
    } else if (std::get_if<common::StringParameter>(&param)) {
      // 現状何かに連動して StringParameter が変化することはない
      assert(false);
    } else {
      assert(false);
    }
  }
  core.updated_parameters_.clear();
  const auto slider = dynamic_cast<Slider*>(pControl);
  const auto defer_preset_save =
      Slider::IsAnyMouseDragEditing() || (slider && slider->IsWheelEditing());
  UpdateSelectedPresetFromCurrentState(static_cast<ParamID>(param_id),
                                       !defer_preset_save);
  if (slider && slider->IsWheelEditing()) {
    ScheduleDeferredPresetSave();
  }
}

void Editor::beginEdit(const Steinberg::int32 index) {
  if (index >= 0) {
    Steinberg::Vst::VSTGUIEditor::beginEdit(index);
  }
}

void Editor::endEdit(const Steinberg::int32 index) {
  if (index >= 0) {
    Steinberg::Vst::VSTGUIEditor::endEdit(index);
  }
}

void Editor::UpdateVoiceMorphingDescription() {
  if (!model_config_.has_value() || !morph_pad_view_ ||
      !morph_pad_view_->isVisible()) {
    return;
  }
  std::u8string str;

  str += u8"[注意 / Caution]";
  str += u8"\n";
  str +=
      u8"Voice Morphing Mode では、未選択の Voice の学習データが\n"
      u8"変換結果に影響を与えやすくなる可能性があります。\n"
      u8"意図せぬ声質の類似や権利侵害にご注意ください。\n";
  str +=
      u8"In Voice Morphing Mode, the training data of unselected Voices could "
      u8"be more prone to influencing the conversion results. Please be "
      u8"mindful of unintended similarities in timbre and possible rights "
      u8"infringement.\n";
  str += u8"\n";
  const auto voice_count = common::GetVoiceCount(*model_config_);
  const auto weights = voice_morph_state_.CalculateWeights();
  for (auto i = 0; i < voice_count; ++i) {
    if (weights[i] >= common::kVoiceMorphWeightThreshold) {
      str += model_config_->voices[i].name;
      str += u8"\n";
      str += model_config_->voices[i].description;
      str += u8"\n";
    }
  }
  SetVoiceDescriptionText(str);
}

}  // namespace beatrice::vst
