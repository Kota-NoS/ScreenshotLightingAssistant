# Screenshot Lighting Assistant 0.3.0 導入・公開確認

## 導入

1. Skyrimを終了し、旧版MODと `SKSE/Plugins/ScreenshotLightingAssistant/` の登録データをバックアップします。
2. MO2へ `ScreenshotLightingAssistant-0.3.0-MO2.zip` を導入します。旧版と同時に有効化せず、DLLが一つだけになるようにしてください。
3. 使用ランタイムに合うSKSE、Address Library、SKSE Menu Frameworkを導入します。Skyrim 1.7.104ではSKSE 2.3.1とAddress Library All in One v13以降が必要です。
4. SKSE経由で起動し、画面下が `0.3.0`、`ScreenshotLightingAssistant.log` に `Screenshot Lighting Assistant 0.3.0 loading` があることを確認します。

## 最小動作確認

1. Mod Control PanelのLightingを開き、3灯、撮影用／常用フェイスライト、撮影エフェクト、画面演出、登録の保存先を確認します。
2. 3灯とフェイスライトを一度点灯し、メニューを閉じても維持されることを確認します。
3. 撮影停止後に常用フェイスライトへ戻ること、ロードやセル移動後に残留・二重点灯がないことを確認します。
4. 撮影エフェクトを一つ、画面演出を一つ表示し、通常のスクリーンショットへ写ることを確認します。
5. 旧登録データがある場合は、登録ライト、栞、セット、常用フェイス設定がそのまま読み込まれることを確認します。

## 公開物

- 利用者向け：`ScreenshotLightingAssistant-0.3.0-MO2.zip`
- ソース公開用：`ScreenshotLightingAssistant-0.3.0-source.zip`
- クラッシュ解析用：`ScreenshotLightingAssistant-0.3.0-symbols.zip`

Source ZIPとSymbols ZIPはMO2へ導入しません。異常時は `skse64.log` と `ScreenshotLightingAssistant.log` を添えて報告してください。
