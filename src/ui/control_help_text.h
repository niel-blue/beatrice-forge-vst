// Copyright (c) 2026 Project Beatrice and Contributors

#ifndef BEATRICE_UI_CONTROL_HELP_TEXT_H_
#define BEATRICE_UI_CONTROL_HELP_TEXT_H_

#include <string_view>

#include "ui/control_help.h"

namespace beatrice::ui {

// Help is intentionally enabled only for Japanese locale environments. The
// implementation must not depend solely on the host process' C++ locale: a
// DAW can leave it unnamed or override it independently of the user's OS
// language.
[[nodiscard]] auto IsJapaneseEnvironment() noexcept -> bool;

// These are usage descriptions, not translations of the official control
// names.  Keep them separate from ParameterInfo so wording can evolve without
// changing VST parameter IDs, automation, or preset data.
[[nodiscard]] constexpr auto GetControlHelpText(const ControlHelpID id)
    -> std::string_view {
  switch (id) {
    case ControlHelpID::kModel:
      return "使用する変換モデルを選びます。";
    case ControlHelpID::kVoice:
      return "変換先の話者を選びます。";
    case ControlHelpID::kConversionBypass:
      return "音声変換を停止します。GAINとINPUT CLEANUPは適用されます。";
    case ControlHelpID::kInputGain:
      return "変換に入る前の声の大きさを調整します。";
    case ControlHelpID::kOutputGain:
      return "変換後の声の最終的な音量を調整します。";
    case ControlHelpID::kNoiseReductionBoost:
      return "一部のモデルで低く唸るノイズが出るとき、上げることでノイズが軽減される可能性があります。";
    case ControlHelpID::kLowCut:
      return "設定した周波数より低い成分を抑え、入力時の低いノイズを軽減します。";
    case ControlHelpID::kLightDenoise:
      return "エアコンや機材のような小さく一定のノイズを抑えます。";
    case ControlHelpID::kDeClick:
      return "リップノイズなどの短いクリック音を抑えます。強くすると発話に影響する場合があります。";
    case ControlHelpID::kPitchShift:
      return "変換前の入力音声の音程を上下します。";
    case ControlHelpID::kFormantShift:
      return "声質の太さ・響きの高さを変えます。";
    case ControlHelpID::kVQNeighborCount:
      return "声の発音らしさを決める調整です。通常は初期値のまま使用してください。";
    case ControlHelpID::kControl:
      return "Average Source PitchとPitch Shiftのどちらを基準に調整するか選びます。";
    case ControlHelpID::kAverageSourcePitch:
      return "入力声の普段の高さを設定します。話者を変えても、入力声の高さを基準に調整したい場合に使います。";
    case ControlHelpID::kMinSourcePitch:
      return "認識する入力音程の下限です。低い声や低音を使うときに下げます。";
    case ControlHelpID::kMaxSourcePitch:
      return "認識する入力音程の上限です。高い声や高音を使うときに上げます。";
    case ControlHelpID::kIntonationIntensity:
      return "入力音声のピッチの抑揚をどの程度反映するか調整します。";
    case ControlHelpID::kPitchCorrection:
      return "ピッチ補正の強さを調整します。";
    case ControlHelpID::kPitchCorrectionType:
      return "ピッチ補正の方式を選びます。";
    case ControlHelpID::kLatencyReporting:
      return "DAWへ報告する遅延の扱いを選びます。処理そのものの遅延は変わりません。";
    case ControlHelpID::kMorphPad:
      return "2D PAD上の位置で、複数の話者を混ぜます。";
    case ControlHelpID::kMorphSlider:
      return "各話者の影響量をスライダーで調整します。";
    case ControlHelpID::kMorphFalloff:
      return "モーフポイントから離れた位置で、声の影響が弱まる速さを調整します。";
    case ControlHelpID::kPresetBankAdd:
      return "新しいプリセットリストを作成します。";
    case ControlHelpID::kPresetBankDelete:
      return "選択中のプリセットリストを削除します。";
    case ControlHelpID::kSavePreset:
      return "現在の設定を新しいプリセットとして保存します。";
    case ControlHelpID::kNewEmptyPreset:
      return "初期状態の空のプリセットを作成します。";
    case ControlHelpID::kImportPresetList:
      return "保存済みのプリセットリストをファイルから読み込みます。";
    case ControlHelpID::kExportPresetList:
      return "選択中のプリセットリストをファイルに保存します。";
    case ControlHelpID::kDeMud:
      return "変換後の声に溜まった低中域を抑え、籠もりを軽減します。";
    case ControlHelpID::kPresence:
      return "変換後の声の高域を補い、輪郭を調整します。強くするとサ行やノイズが目立つ場合があります。";
    case ControlHelpID::kReverbMix:
      return "変換後の声に加える残響音の割合を調整します。";
    case ControlHelpID::kReverbDecay:
      return "残響音が消えるまでの長さを調整します。";
    case ControlHelpID::kReverbTone:
      return "残響音の明るさを調整します。上げるほど高域を残します。";
    case ControlHelpID::kNone:
    case ControlHelpID::kEnd:
      return {};
  }
  return {};
}

}  // namespace beatrice::ui

#endif  // BEATRICE_UI_CONTROL_HELP_TEXT_H_
