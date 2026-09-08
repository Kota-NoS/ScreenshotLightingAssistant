# Screenshot Lighting Assistant 0.1.29

Skyrim SE/AEのスクリーンショット撮影向けに、対象Actorの周囲へ3灯の撮影ライトと独立した白色フェイスライトを配置するSKSEプラグインです。

## 必要環境

- Skyrim Special Edition 1.5.97 または Anniversary Edition 1.6.x
- SKSE64
- Address Library for SKSE Plugins
- SKSE Menu Framework 3
- Microsoft Visual C++ 2015–2022 Redistributable（x64）

VRには対応していません。ESP、Papyrusスクリプト、ゲームINIの変更はありません。

## 導入

1. Skyrimを終了します。
2. MO2で `ScreenshotLightingAssistant-0.1.29-MO2.zip` をインストールします。
3. 旧版を無効にし、ScreenshotLightingAssistant.dllが二重にならないようにします。
4. SKSE経由で起動し、Mod Control Panelから `Screenshot Lighting Assistant > Lighting` を開きます。

Source ZIPとSymbols ZIPはMO2へ導入しません。

## 主な機能

- 対象とカメラを基準にした8方向・最大3灯の撮影ライト
- ライトごとの種類、色、強さ、範囲、距離、高さ、微調整、影、ON/OFF
- 選択中ライトの調整値だけを既定へ戻し、種類・色・影方式・点灯状態を保持する「初期値」
- カメラ基準／顔の向き基準を切り替えられる独立フェイスライト
- プレイヤーまたは周囲の読み込み済み人型NPCを撮影対象として選択
- 戻る／進む、起動中だけ保持する栞
- 登録ライト、お気に入り、プリセット、内蔵サンプル
- 日本語／英語UI。設定がない新規環境は英語で開始

ライトはメニューを閉じても維持されます。ロード、ニューゲーム、メインメニュー、セル変更、対象3Dの消失では安全のため停止し、自動再点灯はしません。

## 登録データ

登録ライト・プリセット・言語・一覧設定はゲームのセーブデータには保存しません。仮想Data内の次の場所へ保存します。

`SKSE/Plugins/ScreenshotLightingAssistant/`

MO2では通常、実ファイルが次へ出力されます。

`Overwrite/SKSE/Plugins/ScreenshotLightingAssistant/`

画面下の「登録の保存先 (?)」へマウスを載せると、実際に使用しているパスを確認できます。必要に応じてフォルダー全体をバックアップするか、MO2の専用出力MODへ変換してください。

0.1.27以前のMy Games側に登録がある場合、初回だけ新しい保存先へコピーします。旧ファイルは削除しません。

## プリセットと安全性

プリセットは照明設定だけを保存し、NPC、ActorHandle、カメラ座標、ゲームセーブ、全体の開始状態は保存しません。削除操作は可能な限り `Archived` フォルダーへ退避します。

他のフェイスライト・撮影ライトMODと同時に使うと照明が重なるため、比較時は不要なライトを消してください。このMODはActorのAI、ポーズ、スケジュール、向き、NPCレコードを変更しません。

## 0.1.29について

RC1の新規導入・再起動後の保存・旧版移行と、RC2で追加した「初期値」はゲーム内確認済みです。「初期値」は強さ1.00、範囲4.0m、距離2.20m、高さと3軸微調整0.00m、影の補正1へ戻し、配置方向・種類・色・影方式・点灯状態を保持します。「戻る」で取り消せます。確認済みのRC2と同じDLLを正式版として収録しています。

## ライセンスとソース

著作者：kota (@kotaSkyrim)  
Copyright © 2026 kota (@kotaSkyrim)  
本プロジェクトはGPL-3.0-or-laterです。詳細は `LICENSE` と `THIRD_PARTY_NOTICES.md` を参照してください。対応するソースは同じバージョンのSource ZIPおよび公開GitHubリポジトリで提供します。
