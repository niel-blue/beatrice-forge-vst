# Beatrice Forge

<img width="962" height="721" alt="スクリーンショット 2026-08-22 025501" src="https://github.com/user-attachments/assets/b8533872-587d-47a8-b721-ce1927f2f685" />


Beatrice Forge は、Project Beatrice の音声変換 VST3 プラグインを基にした、Windows 向けの改修版です。

本リポジトリでは、公式版の基本的な音声変換処理と画面構成を維持しながら、プリセット管理、入力音声の前処理、音量表示、外部出力、録音、説明表示などを追加しています。モデル本体や非公開の推論ライブラリは、このリポジトリには同梱していません。

製品バージョンは `0.9.0` です。現在のForgeは、Project Beatrice `2.0.0-rc.3` を基盤としていますが、公式版のバージョン番号とForgeの製品バージョンは別に管理します。

公式プロジェクトの仕様・モデル・ライセンスについては、次の一次情報を確認してください。

- [Project Beatrice 公式サイト](https://prj-beatrice.com)
- [Project Beatrice / beatrice-vst](https://github.com/prj-beatrice/beatrice-vst)


## 追加した主な機能

### 音声の補正と効果

- **Noise Reduction Boost** — 一部のモデルで発生する低いノイズを、必要に応じて抑えます。
- **INPUT CLEANUP** — Low Cut で低域の不要な成分を抑え、Light Denoise で一定した環境ノイズを軽減します。De-click は短いクリック音やリップノイズを抑えます。
- **EFFECTS** — De-Mud でこもりを抑え、Presence で明瞭さを加えます。Reverb では残響の量、長さ、音色を調整できます。
- **入力・変換後の音量インジケーター** — 入力音と変換後の音が入っているか、音量がどの程度かを画面上で確認できます。
- **レガシースタイルのモーフ画面** — 従来の操作感を残したモーフ画面で、話者名とウェイト値を確認しながら調整できます。


### プリセット

プリセットリストを複数のタブに分けて管理できます。リストと項目の名前変更、並べ替え、空のプリセットの作成、保存、選択中リストのインポート・エクスポートに対応しています。

プリセットとプラグインの設定は、VST3 の状態としてホストへ渡されます。したがって、保存先や復元のタイミングは DAW や VST ホストに依存します。複数の環境でプリセットリストを移動する場合は、EXPORT LIST と IMPORT LIST を使用してください。

### VST 独自の外部出力

IN/OUT タブの OUTPUT DEVICE から、処理後の音声をホストの出力とは別に WASAPI へ送れます。

- 初期状態は OFF です。
- 選択できる出力は WASAPI デバイスだけです。メニューには識別のため [WASAPI] を付けます。
- WASAPI EXCLUSIVE を選ぶと排他モードを使用します。通常モードでは共有モードを使用します。
- ホストの入力・出力設定は変更しません。
- ホスト側の出力と同じ音を選んだデバイスからも出すと、二重に聞こえる場合があります。
- ASIO 出力をこの機能から直接選択することはできません。

これは DAW やホストの出力を置き換える機能ではなく、VST から外部デバイスへ音を出す追加機能です。対応するホストやオーディオ環境によっては使用できないため、通常は OFF のまま使用してください。

### 録音

IN/OUT タブの RECORDING から、次のモードを選べます。

| モード | 保存内容 |
| --- | --- |
| OFF | 録音しない |
| Output | 変換後の出力を保存 |
| Input/Output Separate | 入力と出力を別ファイルに保存 |
| Input/Output L-R | 入力を左、出力を右にまとめて保存 |

保存先の初期値は、Windows のユーザー Music フォルダです。BROWSE で保存先・ベースファイル名を変更できます。実際に保存されるファイル名は、モードに応じて次の形式になります。

~~~~text
Beatrice-Forge-Rec-YYYYMMDD-XXXXXX-Output.wav
Beatrice-Forge-Rec-YYYYMMDD-XXXXXX-Input.wav
Beatrice-Forge-Rec-YYYYMMDD-XXXXXX-LR.wav
~~~~

XXXXXX は同一日の既存ファイルを確認しながら割り当てる番号です。日付が変わると番号は再利用されますが、既存ファイルを上書きしないように保存先のファイル名を確認します。録音中のファイルは、メニューを変更せず STOP で終了してください。

録音設定と保存先は VST ホストへ渡すプラグイン状態に保存されます。録音中かどうかや動作中の WASAPI ストリームは保存・自動再開しません。

## 基本操作

1. MODEL で使用する変換モデルを選択します。
2. VOICE で変換先の話者を選択します。
3. GAIN と INPUT CLEANUP を必要に応じて調整します。
4. VOICE SHAPE、TUNING、EFFECTS を調整します。
5. 必要であれば IN/OUT で外部出力または録音を設定します。
6. プリセットとして残す場合は SAVE PRESET を使用します。

各項目にマウスを一定時間置くと、使用方法を説明するツールチップが表示されます。日本語環境では日本語の説明を表示し、それ以外の環境では公式の英語表記を使用します。

## 状態の保存

プラグインの値や設定は、Windows レジストリには保存しません。パラメーター、プリセット、独自のWASAPI出力設定、録音設定は、VST3 の状態データとして DAW や VST ホストに保存されます。

そのため、次の動作はホスト側の実装に依存します。

- プロジェクト保存時に状態を保存するか
- プラグインのプリセットとして状態を保存するか
- プロジェクトを開いたときに状態を復元するか
- 外部デバイスが存在しない場合に保存済み設定を無効化するか

外部デバイスが変更・切断された場合は、IN/OUT タブでデバイスを再選択してください。

## ビルド（開発者向け・任意）

通常の利用者は、公開されているVST3配布物を使用するため、ビルドは必要ありません。

このリポジトリでビルドする場合だけ、Visual Studio 2022、CMake、サブモジュール、およびProject Beatriceから個別に許諾を受けた`beatrice.lib`が必要です。`beatrice.lib`は公開物に含まれず、自動取得もされません。

詳細な開発手順ではなく、最小限の流れだけを示します。

~~~~powershell
git submodule update --init --recursive
cmake -S . -B build/vs3 -G "Visual Studio 17 2022" -A x64
cmake --build build/vs3 --config Release
~~~~

VST3は`build/vs3/VST3/Release/`に生成されます。DAWのVSTフォルダへの自動コピーは行いません。

## 既知の制限

- 64-bit のプラグインホストを主な対象としています。
- 推論ライブラリ beatrice.lib は自動取得しません。モデル本体や補助データも、このリポジトリには同梱していません。
- リリース構成では、CMakeに設定された公式配布URLからVST配布用の補助データを取得する場合があります。
- VST独自の外部出力は WASAPI のみで、ASIO は対象外です。
- 外部出力を有効にしても、ホストの入出力ルーティングは変わりません。
- デバイスの占有状態、サンプルレート、ホストのスレッド運用によっては外部出力や録音を開始できない場合があります。

## ライセンス

Beatrice Forge のVST3プラグイン本体は MIT License です。著作権表示と許諾文は [LICENSE.txt](LICENSE.txt) を参照してください。

## English summary

Beatrice Forge is a Windows-oriented VST3 build based on Project Beatrice, with Noise Reduction Boost, input cleanup, output effects, input/output level meters, a legacy-style morph screen, optional direct WASAPI output, and recording.

The Forge product version is 0.9.0. This build is based on Project Beatrice 2.0.0-rc.3; the upstream base version and the Forge product version are managed separately.

The host input/output routing is not modified. Direct output is disabled by default, uses WASAPI only, and is intended as an optional extra output path. Plugin state is stored by the VST3 host rather than in the Windows registry.

Models and the non-public Beatrice inference library are not bundled.
