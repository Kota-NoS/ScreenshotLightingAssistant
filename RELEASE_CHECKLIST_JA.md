# Screenshot Lighting Assistant 0.4.0 導入・公開確認

## 導入

1. Skyrimを終了し、旧版MODと `SKSE/Plugins/ScreenshotLightingAssistant/` の登録データをバックアップします。
2. MO2へ `ScreenshotLightingAssistant-0.4.0-MO2.zip` を導入します。旧版と同時に有効化せず、DLLが一つだけになるようにしてください。
3. 使用ランタイムに合うSKSE、Address Library、SKSE Menu Frameworkを導入します。Skyrim 1.7.104ではSKSE 2.3.1とAddress Library All in One v13以降が必要です。
4. SKSE経由で起動し、画面下が `0.4.0`、`ScreenshotLightingAssistant.log` に `Screenshot Lighting Assistant 0.4.0 loading` があることを確認します。

## 最小動作確認

1. Mod Control PanelのLightingを開き、既存3灯、フリーライト、撮影用／常用フェイスライト、撮影エフェクト、画面演出、登録の保存先を確認します。
2. カメラ型ボタンからフリーライトを配置し、配置小窓の強さ・範囲・距離・色・影が即時反映されることを確認します。
3. フリーライトを確定して再度カメラ型ボタンを押し、前回の画面位置と設定へ戻ることを確認します。新規作成時だけ範囲が6.0mになることも確認します。
4. 配置中のホイールが距離を0.03m、Shift＋ホイールが0.20mずつ変更し、左右端・上下端でもマーカーとライトが不自然に飛ばないことを確認します。
5. 3灯をすべて点灯するとフリーライトが消灯し、1灯を消して再点灯すると保持位置へ戻ることを確認します。
6. フリーライト込みのプリセットを登録・適用し、位置、強さ、範囲、距離、色、影、ON/OFFが再現されることを確認します。
7. 専用ホットキーと設定した修飾キーで単独ウィンドウを開閉し、右上の×でも閉じられることを確認します。
8. 撮影停止・再開、ロード、メインメニュー移行後に残留ライトや配置画面がなく、再開時だけ記憶値から戻ることを確認します。
9. 既存の3灯、両フェイスライト、撮影エフェクト、画面演出と旧登録データが従来どおり動くことを確認します。

## 公開物

- 利用者向け：`ScreenshotLightingAssistant-0.4.0-MO2.zip`
- ソース公開用：`ScreenshotLightingAssistant-0.4.0-source.zip`
- クラッシュ解析用：`ScreenshotLightingAssistant-0.4.0-symbols.zip`

Source ZIPとSymbols ZIPはMO2へ導入しません。異常時は `skse64.log` と `ScreenshotLightingAssistant.log` を添えて報告してください。
