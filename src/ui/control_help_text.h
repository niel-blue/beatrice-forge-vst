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
      return "使用する変換モデルを読み込みます。";
    case ControlHelpID::kVoice:
      return "変換先の話者を選択します。";
    case ControlHelpID::kConversionBypass:
      return "音声変換のON/OFFができます。（GAINとINPUT CLEANUPは適用される）";
    case ControlHelpID::kInputGain:
      return "変換前の音量を調整します。";
    case ControlHelpID::kOutputGain:
      return "変換後の最終的な音量を調整します。";
    case ControlHelpID::kNoiseReductionBoost:
      return "一部のモデルで低く唸るノイズが出るとき、値を上げることでノイズを軽減します。";
    case ControlHelpID::kLowCut:
      return "設定した周波数より低い成分を抑え、入力時の低いノイズを軽減します。";
    case ControlHelpID::kLightDenoise:
      return "エアコンや機材のような小さく一定のノイズを軽減します。";
    case ControlHelpID::kDeClick:
      return "リップノイズなどの短いクリック音を軽減します。強くすると発話に影響する場合があります。";
    case ControlHelpID::kPitchShift:
      return "変換前の入力音声の音程を調整します。";
    case ControlHelpID::kFormantShift:
      return "声質の[太さ-柔らかさ]を調整します。";
    case ControlHelpID::kVQNeighborCount:
      return "kNN-VCで使用する近傍数を設定します。値を上げると話者らしさが向上する場合がありますが、滑舌が悪化する場合があります。0で無効です。";
    case ControlHelpID::kControl:
      return "VoiceやFormant Shiftの変更時に、Pitch ShiftとAverage Source Pitchのどちらを維持するかを選びます。";
    case ControlHelpID::kAverageSourcePitch:
      return "VoiceごとのAverage Target Pitchとの差からピッチシフト量を決めるため、入力声の平均的な高さを設定します。（MIDIノート番号）";
    case ControlHelpID::kMinSourcePitch:
      return "認識する入力音程の下限です。低い声や低音を使うときに下げます。";
    case ControlHelpID::kMaxSourcePitch:
      return "認識する入力音程の上限です。高い声や高音を使うときに上げます。";
    case ControlHelpID::kIntonationIntensity:
      return "入力音声の抑揚を強調する倍率を調整します。";
    case ControlHelpID::kPitchCorrection:
      return "ピッチ補正の強さを調整します。";
    case ControlHelpID::kPitchCorrectionType:
      return "ピッチ補正の方式を選びます。";
    case ControlHelpID::kLatencyReporting:
      return "DAWへ報告する遅延の扱いを選びます。処理そのものの遅延は変わりません。";
    case ControlHelpID::kMorphPad:
      return "話者同士をモーフィングする時の影響量を2DPAD上で調整します。";
    case ControlHelpID::kMorphSlider:
      return "話者同士をモーフィングする時の影響量をスライダーで調整します。";
    case ControlHelpID::kMorphFalloff:
      return "Voice Morphing Modeで、距離の近いマーカーを優先する度合いを調整します。";
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
      return "変換後の声の溜まった低中域を抑えて籠もりを軽減します。";
    case ControlHelpID::kPresence:
      return "変換後の声の高域を補い、輪郭を調整します。強くするとサ行やノイズが目立つ場合があります。";
    case ControlHelpID::kReverbMix:
      return "変換後の声に残響音（エコー）を加えます。";
    case ControlHelpID::kReverbDecay:
      return "残響音が消えるまでの長さを調整します。";
    case ControlHelpID::kReverbTone:
      return "残響音の明るさを調整します。上げるほど高域を残します。";
    case ControlHelpID::kInputSource:
      return "マイクなどの音声入力と、音声ファイル入力を切り替えます。";
    case ControlHelpID::kInputDevice:
      return "変換前の音声を受け取る入力機器を選びます。";
    case ControlHelpID::kOutputDevice:
      return "変換後の音声を出力する機器を選びます。";
    case ControlHelpID::kMonitorDevice:
      return "変換後の音声を確認する機器を選びます。";
    case ControlHelpID::kAudioFileBrowse:
      return "変換に使用する音声ファイルを選びます。";
    case ControlHelpID::kAudioFilePlayback:
      return "音声ファイルの再生、一時停止、停止、ループを操作します。";
    case ControlHelpID::kAudioFileVolume:
      return "音声ファイルの音量を調整します。";
    case ControlHelpID::kAudioFileSeek:
      return "音声ファイルの再生位置を移動します。";
    case ControlHelpID::kRecordingMode:
      return "変換後の音声の録音モードを選びます。";
    case ControlHelpID::kRecording:
      return "選んだ録音モードで録音を開始・停止します。";
    case ControlHelpID::kRecordingBrowse:
      return "録音ファイルの保存先と名前を指定します。";
    case ControlHelpID::kVstOutputDevice:
      return "変換後の音声を、選択したWASAPIデバイスへ追加出力します。ホスト側の出力は変更しません。\nホスト側でも同じ音声を出力している場合、二重に聞こえることがあります。";
    case ControlHelpID::kVstWasapiExclusive:
      return "選択したWASAPI出力デバイスを排他モードで使用します。使用中は、他のアプリケーションから同じデバイスを使用できない場合があります。";
    case ControlHelpID::kNone:
    case ControlHelpID::kEnd:
      return {};
  }
  return {};
}

}  // namespace beatrice::ui

#endif  // BEATRICE_UI_CONTROL_HELP_TEXT_H_
