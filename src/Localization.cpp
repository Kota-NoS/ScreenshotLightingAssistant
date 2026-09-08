#include "Localization.h"
#include <Windows.h>
#include <fstream>
#include <unordered_map>

namespace ScreenshotLightingAssistant::Localization
{
    namespace
    {
        std::atomic<Language> language{Language::English};
        constexpr Translation translations[] = {
        { "ここを配置予定位置にする", "Plan placement here" },
        { "ここへ重ねて移動", "Move here (overlap)" },
        { "ここへ移動", "Move here" },
        { "配置位置", "position" },
        { "配置予定位置（まだ点灯しません）", "planned position (stays off)" },
        { "対象", "Target" },
        { "フェイスライト（影なし）", "Face light (no shadows)" },
        { "強さ##face", "Intensity##face" },
        { "光の範囲##face", "Range##face" },
        { "上下 (+上)##face", "Height (+up)##face" },
        { "微調整", "Fine position" },
        { "Light %d : %s / クリックで%s（削除せず設定を保持）", "Light %d: %s / Click to %s (keep settings)" },
        { "消灯", "switch off" },
        { "点灯", "switch on" },
        { "1. 位置", "1. Position" },
        { "Light %d を編集 (%s)", "Edit Light %d (%s)" },
        { "配置済み", "placed" },
        { "未配置", "not placed" },
        { "2. ライト選択", "2. Light type" },
        { "お気に入り", "Favorites" },
        { "管理##light_library", "Manage##light_library" },
        { " を削除", ": Remove" },
        { "管理", "Manage" },
        { "スタジオ白", "Studio White" },
        { "スタジオ暖色", "Studio Warm" },
        { "スタジオ青", "Studio Blue" },
        { "Light 3 を削除", "Light 3: Remove" },
        { "微調整パッド (?)", "Fine position pad" },
        { "前後 (+手前)", "Depth (+toward camera)" },
        { "左右 (+右)", "Horizontal (+right)" },
        { "上下 (+上)", "Vertical (+up)" },
        { "基準に戻す", "Reset offsets" },
        { "初期値", "Reset values" },
        { "カスタム", "Custom" },
        { "白", "White" },
        { "暖色", "Warm" },
        { "ろうそく", "Candle" },
        { "青", "Blue" },
        { "管理中はカードを押しても照明を変えません。", "Manage mode: cards do not change the lighting." },
        { "クリックで3灯とフェイスライトを適用します。", "Click to apply all three lights and the face light." },
        { "クリックで3灯を適用します。フェイス設定は保持します。", "Click to apply the three lights. Face light settings are kept." },
        { "（カスタム色）", " (custom color)" },
        { "名前変更", "Rename" },
        { "名前", "Name" },
        { "名前だけを変更します。照明・お気に入り・並び順は保持します。", "Change the name only. Lighting, favorites and list order are kept." },
        { "変更する", "Rename" },
        { "登録が変わりました。開き直してください。", "The entry changed. Please reopen this dialog." },
        { "キャンセル", "Cancel" },
        { "プリセット###sla_presets", "Presets###sla_presets" },
        { "整理中 / 照明は変わりません", "Managing / lighting stays unchanged" },
        { "カードを選んで適用", "Select a card to apply" },
        { "3: カスタム", "3: Custom" },
        { "3: ろうそく", "3: Candle" },
        { "登録したプリセット", "Saved presets" },
        { "フェイスライトを含む", "Includes face light" },
        { "通常3灯のみ", "Three lights only" },
        { "前へ", "Earlier" },
        { "後へ", "Later" },
        { "削除", "Delete" },
        { "削除する", "Delete" },
        { "サンプル", "Samples" },
        { "再表示", "Show" },
        { "非表示", "Hide" },
        { "サンプルは削除せず、通常の一覧での表示だけ切り替えます。", "Only hide/show the sample in the regular list; it is not deleted." },
        { "プリセットを削除しました（Archivedへ退避）。", "Preset removed (moved to Archived)." },
        { "登録ライト一覧###sla_library", "Saved lights###sla_library" },
        { "適用先 : Light %d / 登録 %zu 件 / お気に入り %zu / %zu", "Apply to: Light %d / Saved: %zu / Favorites: %zu / %zu" },
        { "使う : 種類・色だけ適用。お気に入りはメインに表示します。削除はArchivedへの退避です。", "Use applies type and color only. Favorites appear in the main panel. Delete moves the file to Archived." },
        { "検索", "Search" },
        { " / カスタム色", " / custom color" },
        { "使う", "Use" },
        { "ライトへ適用 : ", "Applied to light: " },
        { "お気に入り解除", "Unfavorite" },
        { "お気に入りに", "Favorite" },
        { "メインの表示だけを変更します。登録や使用中の照明は削除・変更しません。", "Change the main panel shortcuts only. Saved entries and current lighting are not changed." },
        { "%zu件登録済みです。入れ替える項目を選んでください。", "All %zu favorite slots are used. Select one to replace." },
        { "登録はまだありません。メインの「ライト登録」から追加できます。", "No saved lights. Use 'Save light' in the main panel to add one." },
        { "検索に一致するライトはありません。", "No lights match the search." },
        { "ライト登録を削除しました（Archivedへ退避）。", "Saved light removed (moved to Archived)." },
        { "戻る", "Undo" },
        { "進む", "Redo" },
        { "履歴 : ", "History: " },
        { "調整中", "Adjusting" },
        { "栞を登録", "Set bookmark" },
        { "栞へ戻す", "Restore bookmark" },
        { "栞を", "Bookmark" },
        { "登録", "Set" },
        { "戻す", "Restore" },
        { "栞と同じ構成", "Matches bookmark" },
        { "影なし", "No shadows" },
        { "全方向の影", "Omni shadows" },
        { "スポットの影", "Spot shadows" },
        { "影の補正", "Shadow bias" },
        { "選択中のライトの強さ・範囲・距離・高さ・微調整・影の補正を初期値へ戻します。\n配置方向・種類・色・影の方式・点灯状態は変更しません。「戻る」で取り消せます。", "Reset the selected light's intensity, range, distance, height, fine offsets and shadow bias to their defaults.\nKeeps direction, type, color, shadow mode and power. Use Undo to restore the previous values." },
        { "1 (標準)", "1 (Default)" },
        { "影ONで補正を反映", "Applied when shadows are on" },
        { "ライトの色", "Light color" },
        { "ライト登録", "Save light" },
        { "Light %d : ライトの色", "Light %d: Color" },
        { "色だけ変更 / 戻るで取り消し", "Color only / Undo to revert" },
        { "種類の元の色に戻す", "Restore type color" },
        { "閉じる", "Close" },
        { "登録###sla_save", "Save###sla_save" },
        { "プリセット登録", "Save preset" },
        { "ライト登録（種類・色）", "Save light (type and color)" },
        { "登録ボタンを押した時点の設定を保存します。名前は省略しても登録できます。", "Save the settings captured when the save dialog was opened. A name is optional." },
        { "フェイスライトも含める", "Include face light" },
        { "登録する", "Save" },
        { "マイセット ", "My preset " },
        { "マイライト ", "My light " },
        { "登録しました : ", "Saved: " },
        { "ライト", "Lights" },
        { "微調整 : Light {}{}###sla_fine_position", "Fine position: Light {}{}###sla_fine_position" },
        { "未配置 : メイン画面で種類を選ぶと配置できます。", "Not placed. Select a type in the main panel to place this light." },
        { "3. 基本設定 / 編集中 : Light %d", "3. Settings / Editing Light %d" },
        { "未配置 : 位置を選び、ライトを選択してください。", "Not placed. Choose a position and a light type." },
        { "OFF : 設定は保持されています。消灯中も編集できます。", "OFF: Settings are kept and can be edited while off." },
        { "強さ", "Intensity" },
        { "光の範囲", "Range" },
        { "対象からの距離", "Distance from target" },
        { "基準点からの高さ", "Height above anchor" },
        { "広域", "Wide" },
        { "微調整あり : 右 %+.2fm / 上 %+.2fm / 手前 %+.2fm", "Offsets: right %+.2fm / up %+.2fm / toward camera %+.2fm" },
        { "プリセットを選ぶ", "Choose preset" },
        { "登録の保存先 (?) / 0.1.29", "Storage location / 0.1.29" },
        { "対象を選ぶ", "Choose subject" },
        { "撮影対象###sla_target", "Subject###sla_target" },
        { "プレイヤー", "Player" },
        { "現在の対象 : %s", "Current subject: %s" },
        { "周囲30mの読み込み済み人型NPC（最大64人）。対象変更で消灯しますが、照明設定は保持します。", "Loaded humanoid NPCs within 30m of the player (up to 64). Changing the subject switches the lights off but keeps their settings." },
        { "候補を更新", "Refresh nearby list" },
        { "プレイヤーに戻す", "Use player" },
        { "コンソールの選択対象", "Use console selection" },
        { "クロスヘアの対象", "Use crosshair target" },
        { "ゲームが現在認識している対象を取得します。フリーカメラやメニュー中に取得できない場合は一覧を使ってください。", "Use the target currently recognized by the game. If unavailable in free camera or menus, use the nearby list instead." },
        { "候補はありません。NPCの近くで更新してください。", "No candidates. Move near an NPC and refresh." },
        { "ゲーム側の処理を待っています。", "Waiting for the game thread." },
        { "撮影ライトを停止", "Stop lighting" },
        { "撮影ライトを開始", "Start lighting" },
        { "プリセットを選ぶ（下のボタンと同じ一覧）", "Choose preset (same list as the lower button)" },
        { "現在のカメラを正面にする", "Align front to camera" },
        { "保存先が取得できないため登録を利用できません。", "Cannot use saved entries because the storage location is unavailable." },
        { "新しい保存先を利用できないため、従来のMy Games内へ保存します。", "The new storage location is unavailable, so this session will use the previous location under My Games." },
        { "保存先の移行情報を読み込めません。元のファイルは保持しています。", "Cannot read the storage migration marker. Original files are kept." },
        { "従来の保存先から新しい保存先へコピーできないため、この起動では従来の保存先を使います。", "Could not copy the previous storage to the new location, so this session will use the previous location." },
        { "従来の保存先から登録データをコピーしました。旧ファイルはバックアップとして残しています。", "Saved entries were copied from the previous location. The old files remain as a backup." },
        { "クリックで Light %d を編集（位置・点灯状態は変えません）。\n同じ丸の番号はクリックごとに切り替えます。\n右クリックで選択中のライトをこの方向へ移動します。\n図は基本方向です。水色の点は微調整あり。距離・高さ・微調整は各灯で保持します。", "Click to edit Light %d (keeps position and power state).\nRepeated clicks cycle the numbers in this circle.\nRight-click to move the selected light to this direction.\nThe diagram shows base directions. A cyan dot means fine offsets are active. Distance, height and offsets are kept per light." },
        { "クリックで Light %d の%sを変更します。\n距離・高さ・微調整は保持します。\n点灯状態は番号横のマークで切り替えます。", "Click to change Light %d's %s.\nDistance, height and offsets are kept.\nUse the icon next to the slot number to switch it on/off." },
        { "フェイスライト : %s / 強さ %.2f\nクリックでON/OFF。ライト1/2/3や編集対象は変更しません。\n白・影なしの補助光です。隣の設定からカメラ基準／顔の向き基準を選べます。\n停止中は設定だけ変更します。撮影ライトを開始すると反映します。\n頭や配置方向を取得できない場合は待機します。", "Face light: %s / Intensity %.2f\nClick to switch on/off; does not change Lights 1/2/3 or the selected slot.\nWhite, shadowless fill. Choose Camera or Head facing in the adjacent settings.\nWhile stopped, only settings change. Start lighting to apply them.\nWaits if the head or placement direction is unavailable." },
        { "フェイスライトの配置基準・強さ・光の範囲・上下を調整します。開くだけでは点灯・設定を変えません。", "Adjust face light placement basis, intensity, range and height. Opening this panel does not change settings or switch it on." },
        { "強さは0〜3.00、初期値は0.35です。明るい場所では1.00以上にも調整できます。\nOFF中も調整でき、勝手に点灯しません。0で発光しません。\n戻る／進む・栞の登録は、この設定も含みます。フェイスを含めて登録したセット以外では保持します。", "Intensity 0-3.00; default 0.35. Values above 1.00 can help in bright scenes.\nCan be edited while off without switching on. Zero emits no light.\nIncluded in Undo/Redo and bookmarks. Presets keep it unless saved with the face light included." },
        { "フェイスライトの届く範囲です（0.5〜2.0m / 初期1.2m）。\n範囲だけを変え、強さ・位置・ON/OFFとライト1/2/3は保持します。\n狭めると顔への明るさも変わるため、強さと合わせて調整してください。\n顔だけに限定する機能ではなく、近くの髪や服にも光が届きます。", "Face light range: 0.5-2.0m; default 1.2m.\nChanges range only; keeps intensity, position, power and Lights 1/2/3.\nA smaller range also changes illumination at the face, so adjust intensity as needed.\nNot face-only: nearby hair and clothing can also receive light." },
        { "頭を基準にした光源の上下です（-0.50〜+0.50m / 初期+0.15m）。\nカメラ基準ではワールドの上、顔の向き基準では頭の上方向へ動かします。\n光源を上げると顔までの距離も変わります。光の範囲・強さと合わせて調整してください。\nライト1/2/3の高さ・微調整、フェイスライトのON/OFFは変えません。", "Light offset above the head: -0.50 to +0.50m; default +0.15m.\nCamera uses world up; Head facing uses the head's local up direction.\nHeight also changes distance to the face; adjust range and intensity as needed.\nDoes not change Lights 1/2/3 offsets or face light power." },
        { "仕上げ用の微調整パッドを別窓で開きます。距離・高さ・正面更新はメイン画面で操作します。\n窓の番号・点灯状態はメイン画面と連動します。×で閉じても照明は変わりません。\n開いている場合は手前に表示します。移動・サイズ変更できます。", "Open a separate fine-position pad. Adjust distance, height and camera alignment in the main panel.\nSlot selection and power stay synchronized. Closing this window does not change lighting.\nBrings the window forward if already open. It can be moved and resized." },
        { "ライトのある丸 : 編集対象を選択 / 空いた丸 : 移動\n重ねて移動 : 移動元の番号を選び、移動先の丸を右クリック\n上の番号でも編集対象を選べます。番号選択で丸の操作ルールは変わりません。", "Occupied circle: select a light / Empty circle: move the selected light.\nTo overlap lights: select the source slot, then right-click the destination circle.\nThe upper slot buttons also select a light; they do not change these click rules." },
        { "選択中の番号へライトを配置・変更します。\n白・暖色・ろうそく色・青の固定光です。位置・強さ・範囲・影は保持します。\nろうそくの揺らぎはありません。フェイスライトの色は変えません。", "Place or change the light in the selected slot.\nFixed white, warm, blue and candle colors. Keeps position, intensity, range and shadows.\nNo candle flicker. Does not change face light color." },
        { "登録ライト一覧を開きます。お気に入りは最大5件。登録がなくてもここから開けます。", "Open saved lights. Up to five favorites can appear here. Available even with no saved entries." },
        { "選択中のLight %dを配置から削除します。登録ライト・プリセットは削除しません。\n履歴の「戻る」で取り消せます。", "Remove the placed Light %d. Saved lights and presets are not deleted.\nUse Undo in history to restore it." },
        { "選択後も開く", "Keep open" },
        { "ON：適用後も一覧を開いたまま比較できます。OFF：適用すると閉じます。\n×は画面だけを閉じ、最後に適用した照明を残します。\n位置・サイズとこのチェックはゲーム起動中だけ記憶します。", "ON: Keep the gallery open after applying a preset to compare lighting. OFF: Close after applying.\nX closes only the window, keeping the last applied lighting.\nPosition, size and this option are remembered for this game session only." },
        { "管理中はカードで適用せず、名前変更・並び替え・削除・サンプル表示を整理します。\n削除した自作プリセットはArchivedへ退避し、照明や栞を変えません。\n図の色は設定RGB、番号の斜線はOFFです。色はゲーム画面の見え方の再現ではありません。", "In Manage mode, cards do not apply lighting. Rename, reorder, delete or hide samples here.\nDeleted custom presets move to Archived without changing lighting or the bookmark.\nDiagram colors show configured RGB; crossed-out numbers are OFF. They do not reproduce in-game appearance." },
        { "%s\n%s / 種類と色だけを呼び出します。位置・強さ・範囲・影を保持します。\nお気に入りは登録ライト一覧で変更できます。", "%s\n%s / Apply type and color only. Keeps position, intensity, range and shadows.\nManage favorites in the saved lights window." },
        { "選択中のライトだけを撤去します。履歴の「戻る」で取り消せます。", "Remove only the selected light. Use Undo in the history to restore it." },
        { "ドラッグ : 左右・上下 / ホイール : 前後（上回しで奥へ）\n現在の方向・距離・高さへの追加量です。中央は追加量ゼロ、目盛りは0.5m。\n各軸±1m、ホイール1目盛り0.05m。パッドと下のスライダーは同じ値を編集します。\n水平軸は開始時・正面更新時のカメラ基準、上下はワールドの高さです。\n前後は平行移動で、「対象からの距離」とは別です。", "Drag: horizontal/vertical / Wheel: depth (scroll up to move away).\nOffsets are added to the base direction, distance and height. Center is zero; ticks are 0.5m.\nEach axis is limited to +/-1m; one wheel step is 0.05m. The pad and sliders edit the same values.\nHorizontal axes use the camera basis captured at start/alignment; vertical uses world up.\nDepth is a translation, separate from 'Distance from target'." },
        { "左右・上下・前後の追加量だけをゼロにします。\n基本方向・距離・高さ、色・強さ・範囲・影・ON/OFFは変えません。\nこの操作も「戻る」で取り消せます。", "Reset only horizontal, vertical and depth offsets to zero.\nKeeps base direction, distance, height, color, intensity, range, shadows and power.\nCan be reverted with Undo." },
        { "通常はカードで適用して閉じます。管理中は適用せず、並び替え・削除・サンプル表示を整理します。\n削除した自作プリセットはArchivedへ退避し、照明や栞を変えません。\n図の色は設定RGB、番号の斜線はOFFです。色はゲーム画面の見え方の再現ではありません。", "Normally, select a card to apply it and close. Manage mode allows reordering, deletion and sample visibility without applying lighting.\nDeleted custom presets move to Archived. Lighting and bookmarks are unchanged.\nDiagram colors are the saved RGB values; a slash means OFF. They do not simulate in-game shading." },
        { "%s を一覧から外します。\nファイルはArchivedへ退避します。照明は変えません。", "Remove %s from the list.\nMoves its file to Archived. Lighting is unchanged." },
        { "%s を一覧から外します。\n使用中の照明・保存済みプリセットは変わりません。ファイルはArchivedへ退避します。", "Remove %s from the list.\nCurrent lighting and saved presets are unchanged. Moves its file to Archived." },
        { "3灯とフェイスライト共通の履歴を1操作戻します。位置以外の変更も対象です。", "Undo one change in the shared history for all three lights and the face light, including non-position edits." },
        { "3灯とフェイスライト共通の履歴を1操作進めます。編集対象の番号も復元します。", "Redo one change in the shared history for all three lights and the face light. Also restores the selected slot." },
        { "最大50件。スライダーは操作ごとに1件記録します。過去へ戻って編集すると、その先の履歴は置き換わります。ゲーム終了で履歴は消えます。", "Up to 50 entries. Each slider gesture creates one entry. Editing after Undo replaces the later history. History is cleared when the game exits." },
        { "3灯とフェイスライトを一時記憶します（ゲーム終了まで1件）。再登録で上書きします。\n上と下のボタンは同じ栞です。カメラ位置・正面の基準・撮影ライト全体のON/OFFは保存しません。\n再起動後も残すには「プリセット登録」を使ってください。", "Temporarily remember all three lights and the face light (one bookmark until the game exits). Setting it again overwrites it.\nThe upper and lower buttons share this bookmark. Camera position, alignment basis and overall lighting power are not saved.\nUse 'Save preset' to keep a setup across restarts." },
        { "最後に栞へ登録した照明と編集番号へ戻します。履歴の「戻る」で取り消せます。", "Restore the bookmarked lighting and selected slot. Can be reverted with Undo." },
        { "同じライトを切り替えて比較します。位置・強さ・範囲・影の補正は変えません。\nスポットは基準点へ自動で向きます（照射角90°）。\n消灯とは別です。比較時はSAMなど他の撮影ライトを消してください。", "Compare shadow modes for the same light. Keeps position, intensity, range and shadow bias.\nSpot lights automatically aim at the anchor (90-degree cone).\nThis is not an on/off switch. Disable other photography lights, such as SAM, when comparing." },
        { "選択中のライトの影の判定を補正します。標準は1です。\n影をぼかす機能ではありません。大きすぎると影が身体から離れたり、細かい影が消える場合があります。\n表示はスポット選択中のみです。別の影方式・消灯へ切り替えても値を保持します。全方向の影にも同じ補正値が適用されます。\nSkyrim全体の設定は変更しません。", "Adjust shadow depth bias for the selected light. Default is 1.\nThis is not a blur control. Excessive values can detach shadows from the body or remove fine shadows.\nThis control is shown only in Spot mode. Its value is kept in other modes and while off; it also applies to Omni shadows.\nDoes not change Skyrim's global settings." },
        { "選択中のライトの種類・色を登録します。位置・強さ・範囲・影は含みません。", "Save the selected light's type and color. Position, intensity, range and shadows are not included." },
        { "通常3灯の位置・微調整・種類・色・強さ・範囲・影・ON/OFFを保存します。正面の基準と照明全体の開始状態は保存しません。", "Save the three lights' positions, offsets, types, colors, intensity, range, shadows and power states. Camera alignment and overall lighting start/stop state are not saved." },
        { "登録はライト選択の候補を増やします。灯数や現在の照明は変えません。", "Adds a reusable light choice. Does not add a physical light or change current lighting." },
        { "現在のカメラを3灯共通の正面にします。構図の変更後、必要なときだけ更新してください。\n基準点は胸（取得できない場合は頭、さらに取得できなければ足元＋1.2m）に追従します。\n正面の更新は設定の履歴には記録されません。", "Use the current camera as the front direction for all three lights. Update only when needed after changing composition.\nThe anchor follows the chest, falling back to the head, then 1.2m above the feet.\nChanging alignment is not recorded in the settings history." },
        { "選択中のライトの範囲だけを12mにします。色・強さ・位置・影・ON/OFFは保持します。\nスライダーで再調整でき、「戻る」で元の範囲へ戻せます。照射角や影の柔らかさは変えません。", "Set only the selected light's range to 12m. Keeps color, intensity, position, shadows and power.\nReadjust with the slider or restore with Undo. Does not change the cone angle or shadow softness." },
        { "基本方向に沿った距離です。別窓の微調整は追加量として保持します。前後への平行移動とは別です。", "Distance along the base direction. Fine offsets in the separate window are added on top. Different from depth translation." },
        { "基本配置の高さです。別窓の上下は追加量として保持します。", "Base placement height. The separate window's vertical offset is added on top." },
        { "%s\nMO2ではこの仮想Dataパスへの新規ファイルが通常Overwriteへ入ります。\n登録は再起動後も残り、ゲームのセーブとは独立です。\n登録から外したファイルは、この中のArchivedへ退避します。\n履歴・栞の登録は起動中のみの一時退避です。", "%s\nWith MO2, new files written to this virtual Data path normally appear in Overwrite.\nSaved entries survive restarts and are independent of game saves.\nRemoved files move to the Archived folder here.\nHistory and bookmarks are temporary for the current game session." },
        { "%s\n\nLight 1～3と独立したフェイスライトを実機反映します。\nメニューを閉じても点灯を維持します。停止・ロード・場所の移動ではフェイスを含む全灯を撤去し、自動再開しません。\n通常3灯は正面更新までカメラ基準を固定します。フェイスだけは頭・現在のカメラ位置へ自動追従します。", "%s\n\nApplies Lights 1-3 and an independent face light in game.\nLights remain on when the menu closes. Stop, loading or changing location removes all lights, including the face light, without automatic restart.\nThe three lights keep their camera basis until realigned. Only the face light automatically follows the head and current camera position." },
        { "読み込めない登録が ", "Unreadable entries: " },
        { " 件あります。元のファイルは保持しています。", ". Original files are kept." },
        { "登録フォルダーを読み込めません。保存先とアクセス権を確認してください。", "Cannot read the saved entries folder. Check its location and permissions." },
        { "登録を書き込めません。保存先とアクセス権を確認してください。", "Cannot write the entry. Check the storage location and permissions." },
        { "登録を保存できませんでした。既存の登録は変更していません。", "Could not save. Existing entries are unchanged." },
        { "登録を保存できません。保存先と空き容量を確認してください。", "Cannot save. Check the location and free disk space." },
        { "名前を入力してください（UTF-8で120バイト以内、改行・##は使えません）。", "Enter a name (up to 120 UTF-8 bytes; no newlines or ##)." },
        { "ライト登録は128件までです。不要な登録を外してください。", "Up to 128 lights can be saved. Remove unused entries first." },
        { "配置済みの有効なライトを選んでください。", "Select a valid, placed light." },
        { "同じ名前が登録されています。別の名前で登録してください。", "That name is already in use. Choose a different name." },
        { "プリセット登録は128件までです。不要な登録を外してください。", "Up to 128 presets can be saved. Remove unused entries first." },
        { "無効なライト設定が含まれています。", "The setup contains invalid light settings." },
        { "ファイルが変更されています。一覧を読み込み直してから名前を変更してください。", "The file was changed externally. Reload the list before renaming." },
        { "名前を変更できませんでした。保存先とアクセス権を確認してください。", "Could not rename. Check the storage location and permissions." },
        { "登録が見つかりません。", "Entry not found." },
        { "登録を外せませんでした。ファイルと一覧は保持しています。", "Could not remove the entry. The file and list are unchanged." },
        { "一覧設定を読み込めません。既存の設定ファイルを保持し、一覧設定の保存を停止しています。登録データは利用できます。", "Cannot read list preferences. The existing file is kept and preference saving is disabled. Saved lights and presets are still available." },
        { "一覧設定を保存できませんでした。お気に入り・並び順・表示設定は変更していません。", "Could not save list preferences. Favorites, order and visibility are unchanged." },
        { "開始時の構成", "Initial setup" },
        { "位置変更", "Direction" },
        { "ライト選択", "Light type" },
        { "配置", "Placement" },
        { "広域 (12m)", "Wide range (12m)" },
        { "フェイスライト : ON", "Face light: ON" },
        { "カメラ基準", "Camera" },
        { "顔の向き基準", "Head facing" },
        { "従来の配置です。頭からカメラ側へ配置し、高さはワールドの上下を基準にします。\nカメラが頭に近すぎる場合はフェイスライトだけ待機します。", "Original placement: toward the camera from the head, with height measured along world up.\nOnly the face light waits when the camera is too close to the head." },
        { "頭の向きに合わせて顔の前方へ配置します。横向き・上下・傾きに追従し、高さも頭の向きを基準にします。\n通常3灯・カメラ・キャラクターの向きは変えません。頭の向きを取得できない場合はフェイスライトだけ待機します。\n方式は履歴・栞・フェイス込みプリセットに含まれます。", "Placed in front of the face, following head turns, pitch and tilt. Height follows the head's local up direction.\nDoes not change the three main lights, camera or actor orientation. Only the face light waits if head orientation is unavailable.\nIncluded in history, bookmarks and presets saved with face light." },
        { "フェイスライト : カメラ基準", "Face light: Camera" },
        { "フェイスライト : 顔の向き基準", "Face light: Head facing" },
        { "フェイスライト : OFF", "Face light: OFF" },
        { "フェイスライト : 強さ", "Face light: Intensity" },
        { "フェイスライト : 光の範囲", "Face light: Range" },
        { "フェイスライト : 上下", "Face light: Height" },
        { "プリセット : ", "Preset: " },
        { "影 ON", "Shadows ON" },
        { "影 OFF", "Shadows OFF" },
        { "距離", "Distance" },
        { "高さ", "Height" },
        { "位置の微調整", "Fine position" },
        { "位置だけ基準に戻す", "Reset offsets" },
        { "調整を初期値へ", "Reset values" },
        { "栞の構成", "Bookmarked setup" },
        { "栞へ復帰", "Restore bookmark" },
        { "白色ポートレート", "White portrait" },
        { "白色 / 左右の前方から", "White / front left and right" },
        { "暖色ポートレート", "Warm portrait" },
        { "暖色 / 正面と側面から", "Warm / front and side" },
        { "リムライト", "Rim lighting" },
        { "白色 / 後方2灯と正面補助", "White / two rear lights and front fill" },
        { "青色リムライト", "Rim lighting Blue" },
        { "暖色1灯＋青色2灯 / フェイスライト込み", "One warm and two blue / includes face light" },
        { "暖色リムライト", "Rim lighting Warm" },
        { "暖色2灯＋白色1灯", "Two warm and one white" },
        { "キャンドル", "Candlelight" },
        { "ろうそく色 / 低めの光", "Candle color / low placement" },
        { "キャンドル空間照明", "Candle Space" },
        { "白色1灯＋ろうそく色2灯 / フェイスライト込み", "One white and two candle lights / includes face light" },
        { "片側ライティング", "One-sided lighting" },
        { "補助のLight 2はOFFで待機", "Fill Light 2 is stored OFF" },
        { "停止中 : 撮影3灯＋フェイスライトを試せます。", "Stopped: three photography lights and a face light are available." },
        { "状態が変わりました。対象を選び直してください。", "The game state changed. Choose the subject again." },
        { "ロード完了後に対象を選んでください。", "Choose a subject after loading finishes." },
        { "候補を更新しました。距離はプレイヤーからの距離です。", "List refreshed. Distances are measured from the player." },
        { "候補が変わりました。一覧を更新してください。", "The candidate list changed. Refresh it." },
        { "対象を取得できません。近くの人型NPCを選ぶか、一覧を更新してください。", "Subject unavailable. Select a nearby humanoid NPC or refresh the list." },
        { "対象を変更しました。設定を保持して消灯しました。撮影ライトを開始してください。", "Subject changed. Lights are off and settings are kept. Press Start lighting when ready." },
        { "停止処理待ち : 設定・履歴は保持されています。", "Stopping: settings and history are kept." },
        { "停止しました。ライト設定・履歴は保持されています。", "Stopped. Light settings and history are kept." },
        { "画面遷移で停止しました。撮影場所で開始してください。", "Stopped during a menu transition. Start again at the photography location." },
        { "撮影対象を読み込めないため停止しました。対象を選び直してください。", "Stopped because the subject is unavailable. Choose the subject again." },
        { "場所の移動で停止しました。必要な場所で再開してください。", "Stopped after a location change. Start again at the desired location." },
        { "対象・カメラを取得できません。ロード完了後、三人称かフリーカメラで開始してください。", "Subject or camera unavailable. After loading, start in third-person or free camera." },
        { "正面を判定できません。カメラを対象から少し離して開始してください。", "Cannot determine the front direction. Move the camera away from the subject and start again." },
        { "光源設定が無効なため停止しました。", "Stopped: invalid light settings." },
        { "光源の色設定が無効なため全灯を停止しました。", "All lights stopped: invalid color settings." },
        { "配置座標が無効なため停止しました。", "Stopped: invalid placement coordinates." },
        { "スポットの向きを計算できないため停止しました。", "Stopped: cannot calculate the spot direction." },
        { "光源を作成できませんでした。", "Could not create a light." },
        { "描画側への光源登録に失敗しました。", "Could not register a light with the renderer." },
        { "要求と異なる種類の光源が返されたため停止しました。ログを確認してください。", "Stopped: the renderer returned the wrong light type. Check the log." },
        { "強さ0", "zero intensity" },
        { "待機（頭・向き・カメラを確認）", "waiting (check head/orientation/camera)" },
        { "実ライト動作中 : {}/3灯（影付き{}灯） / フェイス {} / {}", "Lighting: {}/3 lights ({} shadowed) / Face {} / {}" },
        { "3灯基準は足元＋1.2m（代替）", "anchor: feet + 1.2m (fallback)" },
        { "3灯基準は頭（代替）", "anchor: head (fallback)" },
        { "3灯基準は胸", "anchor: chest" },
        { "画面遷移で停止しました。自動再点灯はしません。", "Stopped during a menu transition. Lights do not restart automatically." },
        { "光源処理エラーで停止しました。ログを確認してください。", "Stopped after a lighting error. Check the log." },
        { "停止中 : 撮影場所で撮影ライトを開始してください。", "Stopped: start lighting at the photography location." },
        { "ロード待機中 : 実ライトを停止しています。", "Waiting for loading: lights are stopped." },
        { "開始待ち : ゲーム側の処理を待っています。", "Starting: waiting for the game thread." },
        { "言語設定を読み込めません。元のファイルは保持しています。", "Cannot read the language preference. The original file is kept." },
        { "言語設定を保存できません。この起動中だけ切り替えます。", "Cannot save the language preference. The switch applies only to this session." },
        { "日本語／英語を切り替えます。名前・照明・登録データは変えません。", "Switch between Japanese and English. Names, lighting and saved entries are unchanged." },
        };
        bool ReadPreference(const std::filesystem::path& path, Language& result)
        {
            std::error_code error;
            if (std::filesystem::file_size(path, error) > 64 || error) { return false; }
            std::ifstream stream(path, std::ios::binary);
            const std::string contents{std::istreambuf_iterator<char>(stream), {}};
            if (stream.bad()) { return false; }
            if (contents == "SLA LANGUAGE 1\nja\n") { result = Language::Japanese; return true; }
            if (contents == "SLA LANGUAGE 1\nen\n") { result = Language::English; return true; }
            return false;
        }
    }
    Language Current() { return language.load(std::memory_order_relaxed); }
    void Set(Language value) { language.store(value, std::memory_order_relaxed); }
    std::span<const Translation> Table() { return translations; }
    const char* Tr(const char* key, Language requested)
    {
        if (!key || requested == Language::Japanese) { return key; }
        static const auto index = [] {
            std::unordered_map<std::string_view, const char*> result;
            for (const auto& entry : translations) { result.emplace(entry.japanese, entry.english); }
            return result;
        }();
        const auto found = index.find(key);
        return found == index.end() ? key : found->second;
    }
    std::string StoredError(const std::string& error)
    {
        if (Current() == Language::Japanese) { return error; }
        constexpr std::string_view prefix = "読み込めない登録が ";
        constexpr std::string_view suffix = " 件あります。元のファイルは保持しています。";
        if (error.starts_with(prefix) && error.ends_with(suffix)) {
            return "Unreadable entries: " + error.substr(prefix.size(), error.size() - prefix.size() - suffix.size()) + ". Original files are kept.";
        }
        return Tr(error.c_str());
    }
    std::string HistoryText(const std::string& key)
    {
        if (Current() == Language::Japanese) { return key; }
        constexpr std::string_view preset = "プリセット : ";
        if (key.starts_with(preset)) { return "Preset: " + key.substr(preset.size()); } // User name is opaque.
        if (key.starts_with("Light ")) {
            const auto separator = key.find(" : ");
            if (separator != std::string::npos) {
                return key.substr(0, separator) + ": " + Tr(key.substr(separator + 3).c_str());
            }
        }
        return Tr(key.c_str());
    }
    bool LoadPreference(const std::filesystem::path& path, std::string& error)
    {
        error.clear();
        std::error_code ec;
        if (!std::filesystem::exists(path, ec) && !ec) { Set(Language::English); return true; }
        Language saved{};
        if (!ec && ReadPreference(path, saved)) { Set(saved); return true; }
        error = "言語設定を読み込めません。元のファイルは保持しています。";
        return false;
    }
    bool SavePreference(const std::filesystem::path& path, Language requested, std::string& error)
    {
        error = "言語設定を保存できません。この起動中だけ切り替えます。";
        std::error_code ec;
        const bool exists = std::filesystem::exists(path, ec);
        Language previous{};
        if (ec || (exists && !ReadPreference(path, previous))) { return false; }
        std::filesystem::create_directories(path.parent_path(), ec);
        if (ec) { return false; }
        static std::atomic<unsigned long> counter{};
        auto temporary = path;
        temporary += L"." + std::to_wstring(GetCurrentProcessId()) + L"." + std::to_wstring(++counter) + L".tmp";
        const auto handle = CreateFileW(temporary.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (handle == reinterpret_cast<HANDLE>(static_cast<std::intptr_t>(-1))) { return false; }
        const std::string bytes = requested == Language::English ? "SLA LANGUAGE 1\nen\n" : "SLA LANGUAGE 1\nja\n";
        DWORD written{};
        const bool ok = WriteFile(handle, bytes.data(), static_cast<DWORD>(bytes.size()), &written, nullptr) &&
            written == bytes.size() && FlushFileBuffers(handle);
        CloseHandle(handle);
        if (!ok || !MoveFileExW(temporary.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
            DeleteFileW(temporary.c_str()); // Exact temporary file created above, never an existing preference.
            return false;
        }
        error.clear();
        return true;
    }
}
