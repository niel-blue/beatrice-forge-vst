# Beatrice VST — 日本語向けフォーク

本リポジトリは、公式の [Beatrice VST](https://github.com/prj-beatrice/beatrice-vst) を基にしたフォーク版です。

公式版の項目名と基本的な操作の流れを維持しながら、個人の制作環境で必要となった機能と、初めて使う方にも扱いやすくするための機能を追加しています。

公式プロジェクトについては、[公式サイト](https://prj-beatrice.com) と [公式リポジトリ](https://github.com/prj-beatrice/beatrice-vst) を参照してください。

## 追加されている機能

- プリセットリストを複数のタブで管理
- プリセットリストとプリセット項目の名前変更・並び替え
- プリセットの保存、空のプリセットの作成
- 選択中のプリセットリストのインポート・エクスポート
- INPUT CLEANUP
  - Low Cut
  - Light Denoise
  - De-click
- Noise Reduction Boost
- TUNING内のLatency Reporting
- モーフ画面での話者名・ウェイト値の常時表示
- 日本語環境での項目ヘルプ表示

## 導入

ビルド済みのVST3を、使用するDAWのVST3プラグインフォルダへ配置してください。

モデルや関連データの導入方法は、公式プロジェクトの案内を確認してください。本リポジトリには、公式配布物に含まれるモデルそのものは同梱していません。

## 基本操作

1. `MODEL` で使用する変換モデルを選択します。
2. `VOICE` で変換先の話者を選択します。
3. `GAIN` と `INPUT CLEANUP` を必要に応じて調整します。
4. `VOICE SHAPE` と `TUNING` を調整します。
5. モーフを使う場合は、`2D PAD` または `SLIDER` を選択します。
6. 必要な状態を `SAVE PRESET` で保存します。

## 項目の説明

### GAIN

`Input Gain` は、変換に入る前の声の大きさを調整します。

`Output Gain` は、変換後の最終的な音量を調整します。

`Noise Reduction Boost` は、一部のモデルで低く唸るノイズが出る場合に使用します。値を上げることでノイズが軽減される可能性があります。

### INPUT CLEANUP

`Low Cut` は、設定した周波数より低い成分を抑え、入力時の低いノイズを軽減します。

`Light Denoise` は、エアコンや機材のような小さく一定のノイズを抑えます。

`De-click` は、リップノイズなどの短いクリック音を抑えます。強くすると発話に影響する場合があります。

### VOICE SHAPE

`Pitch Shift` は、変換前の入力音声の音程を上下します。

`Formant Shift` は、声質の太さや響きの高さを変えます。

`VQ Neighbor Count` は、声の発音らしさを決める調整です。通常は初期値のまま使用してください。

`CONTROL` は、`Average Source Pitch` と `Pitch Shift` のどちらを基準に調整するかを選びます。

`Average Source Pitch` は、入力声の普段の高さを設定します。話者を変えても、入力声の高さを基準に調整したい場合に使用します。

`Min Source Pitch` と `Max Source Pitch` は、入力音程を認識する範囲の下限と上限です。

### TUNING

`Intonation Intensity` は、入力音声のピッチの抑揚をどの程度反映するかを調整します。

`Pitch Correction` は、ピッチ補正の強さを調整します。

`Pitch Correction Type` は、ピッチ補正の方式を選びます。

`Latency Reporting` は、DAWへ報告する遅延の扱いを選びます。処理そのものの遅延は変わりません。

### VOICE CONVERSION

`VOICE CONVERSION` をオフにすると音声変換だけが停止します。`GAIN` と `INPUT CLEANUP` はそのまま適用されます。

## モーフ

`2D PAD` では、パッド上の位置で複数の話者を混ぜます。

`SLIDER` では、各話者の影響量を個別に調整します。

`Morph Falloff` は、モーフポイントから離れた位置で声の影響が弱まる速さを調整します。

話者名とウェイト値は常に表示されます。

## プリセット

プリセットは、上部のリストタブごとに管理されます。リストタブとプリセット項目は右クリックから名前を変更できます。

`SAVE PRESET` は、現在の設定を新しいプリセットとして保存します。

`NEW EMPTY` は、初期状態の空のプリセットを作成します。

`IMPORT LIST` は、選択中のプリセットリストをファイルから読み込みます。

`EXPORT LIST` は、選択中のプリセットリストをファイルへ保存します。

プリセットリストはVSTホストの状態に保存されます。ファイルとして移動・共有する場合は、`EXPORT LIST` と `IMPORT LIST` を使用してください。

## 項目ヘルプ

項目の上にマウスを約800ms置くと、使用方法や効果の説明が表示されます。マウスを移動するかクリックすると自動的に消えます。

日本語の項目ヘルプは、日本語環境でのみ表示されます。その他の環境では、公式の英語表記と公式ドキュメントを使用してください。

## ライセンス

MIT License。詳細は [LICENSE.txt](LICENSE.txt) を参照してください。

## English Summary

This repository is a fork of the official [Beatrice VST](https://github.com/prj-beatrice/beatrice-vst) repository.

It keeps the upstream control names and basic workflow while adding preset list management, input cleanup controls, Noise Reduction Boost, Latency Reporting, always-visible morph labels, and Japanese usage tooltips.

For the original project and model setup, see the [official website](https://prj-beatrice.com) and [official repository](https://github.com/prj-beatrice/beatrice-vst).
