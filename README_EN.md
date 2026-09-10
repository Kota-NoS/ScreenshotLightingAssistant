# Screenshot Lighting Assistant 0.2.0

Three adjustable photography lights, an independent photography face fill, and an optional persistent player face light for Skyrim SE/AE. Version 0.2.0 is the current stable release.

## Changed in 0.2.0

- Added a compact person icon that opens a separate Persistent Face Light window without changing the circular photography-light layout.
- Added a feature master switch at the top of the Persistent Face Light window. Turning it off extinguishes the persistent light and ignores both assigned hotkeys while preserving the player-light state, values and bindings. Turning it back on resumes the preserved state.
- The persistent light is player-only, white and shadowless. Its placement is now always head-facing; intensity, range and height remain independently adjustable.
- An optional keyboard hotkey switches it on or off. Select Change and press one keyboard key without closing the framework menu; press Esc to cancel without changing the current assignment. Clear removes the assignment. SKSE Menu Framework's dedicated input callback handles this in-menu capture.
- A separate gamepad button can be assigned. Hold it for about 0.6 seconds during normal gameplay to toggle once. A short press does nothing, and holding the button cannot retrigger until it is released.
- Its feature state, light state, both hotkeys and values are stored in `persistent-face.sla`, independently of Skyrim saves, and restored after restarting the game. Older beta profiles remain readable and migrate to head-facing placement.
- The persistent player light is removed during loading and recreated only after the player, cell and renderer are safe again. Ordinary photography lights still stop on loading/location changes and do not auto-restart.
- When photography targets the player and its photography face light is enabled, that light temporarily replaces the persistent one to prevent duplicate illumination. Stopping photography restores the persistent profile. An NPC can still use the photography face light while the persistent light remains on the player.
- Lighting, persistence, transitions, in-menu assignment, gamepad long-press behavior, the master switch, head-facing migration and the revised status UI all passed the supplied in-game checklist. Version 0.2.0 promotes the same feature code tested in beta4.

## Changed in 0.1.29

- Added Reset values beside the Settings heading. It resets only the selected light's intensity (1.00), range (4.0m), distance (2.20m), height and fine offsets (0.00m), and shadow bias (1 Default).
- Direction, type, color, shadow mode and power are preserved. The reset is one Undo/Redo history action and is disabled for unplaced lights or values already at defaults.
- RC1 clean-install, restart-persistence and one-time migration checks passed in game. The RC2 Reset values control also passed live verification before this release.
- Updated DLL/UI version presentation and replaced the trial startup log with a neutral release log.
- Corrected the CommonLibSSE-NG attribution to `alandtse/CommonLibSSE-NG v6.1.0`; its exception text and original MIT notice are included.
- Removed the PDB from the MO2 archive. Debug symbols are supplied separately and are not required to play.
- Added concise Japanese/English release documentation and a final clean-install/upgrade checklist.

## Changed in 0.1.28 (historical)

- Added the user's exact `Candle Space` scene as a built-in sample, including fine offsets and its disabled Head-facing face-light settings. Japanese UI labels it キャンドル空間照明. It remains separate from user-created Saved presets.
- English is now the default only when no language preference exists. Existing JP/EN preferences remain authoritative after updating.
- Mutable files now target virtual `Data/SKSE/Plugins/ScreenshotLightingAssistant/`. Under MO2 they should normally appear in `Overwrite/SKSE/Plugins/ScreenshotLightingAssistant/`. A non-MO2 install uses the physical Data path and falls back to the previous My Games location if Data is not writable.
- On first successful use, language, saved entries, list metadata and Archived files are copied from the old SKSE-log location. Old files remain untouched as a rollback backup, and a marker prevents repeated imports.
- Normal-width preset cards are about 11% shorter than 0.1.27 while retaining the current font size. Diagram, padding and information rows were compacted together.
- 44,816 automated checks and the SE/AE DLL build passed. Live MO2 redirection, lighting output, localization and UI scaling still require in-game verification.

## Changed in 0.1.27 (historical)

- Select a card to apply now appears to the right of Saved presets instead of taking a separate row. With no user presets, it appears to the right of Samples.
- At narrow widths the guidance is omitted rather than pushing headings and cards down. Managing / lighting stays unchanged follows the same rule.
- Saved lights and presets remain external files under the SKSE log directory. They are independent of save files and carry across new games.

## Changed in 0.1.26 (historical)

- Head facing is now the default face-light basis for newly initialized scenes. Camera remains available. Existing SCENE 1 saved presets still decode as Camera and are not rewritten.
- Added Rim lighting Blue and Rim lighting Warm to Samples. Blue applies the authored three-light setup plus its Head-facing face fill; Warm applies its three manual lights and keeps the current face-light settings.
- Built-in samples do not create entries under Saved presets, so they remain separate from user registrations.
- Select a card to apply now appears on its own row below the Manage and Keep open checkboxes.
- Reopening a light/preset registration dialog or editing its name clears the previous registration error. Persistent storage-load warnings remain separate.

## Changed in 0.1.25 (historical)

- Open the face settings button below the center of the diagram. Choose Camera (the 0.1.25 default) or Head facing. Switching does not turn the face light on.
- Camera retains the original camera-side placement and world-up height. Head facing places the fill 0.45m ahead of the animated head, following turns, pitch and tilt; height follows the head's local up direction.
- Only the face fill is affected. The three main lights, camera and actor orientation are not modified. Face color remains white and shadowless; intensity/range controls are unchanged.
- Undo/Redo, bookmarks and presets saved with face light include the mode. Samples and presets without face light retain the current face settings.
- Older presets load unchanged as Camera. Only new presets including Head facing use SCENE 2; builds before 0.1.25 cannot read those entries and leave their files untouched. Other presets remain SCENE 1. Back up the Presets folder before updating.
- In side-by-side layouts, Fine position and removal now belong to the left diagram column, sharing vertical space with favorite 5 instead of following both columns. Narrow layouts still stack/wrap safely.

## Changed in 0.1.24 (historical)

- The placed-light removal button now sits beside Fine position: Light N with a theme-matched line-drawn trash icon. It removes only that placed light, not saved entries, and can be undone. Narrow windows wrap the action row.
- Favorites now hold up to five saved lights. A sixth requires choosing which favorite to replace.
- Keep open beside Manage defaults to OFF (apply and close). Enable it to apply presets immediately without closing the gallery. Closing with X keeps the last applied lighting; it does not cancel it.
- The gallery no longer dims the scene or blocks the main lighting controls. Start lighting first to compare illumination. Manage mode still prevents card application.
- Gallery position, size and Keep open are remembered for this game session only. The first opening is centered; remembered geometry is clamped to the current viewport.
- List metadata now saves to library-view-v3.sla, importing v2 or v1 if v3 is absent without changing the older file. Downgrading to 0.1.23 uses the older list settings. Saved light/preset file formats are unchanged.

## Changed in 0.1.23

- Both upper and lower controls now read Bookmark [Set] [Restore]; Japanese reads 栞を [登録] [戻す].
- The group uses measured label/button widths to fit beside camera alignment when possible; narrow windows wrap only when needed.
- Bookmark behavior, hover help and disabled states are unchanged. Set replaces the one temporary bookmark; Restore restores its lighting settings.
- Check both languages and window resizing in game. The previous release's English UI and NPC application have been user-tested; this new bookmark layout has not yet been checked in game.

## Install

Close Skyrim. Back up the previous `ScreenshotLightingAssistant` storage folder and keep your previous installer for rollback. Install **ScreenshotLightingAssistant-0.2.0-MO2.zip** with MO2; disable the previous DLL. The archive root contains `SKSE/Plugins` and `docs`, with no extra `Data` wrapper. Do not install the separate source or symbols ZIP in MO2.

Requires SKSE64, Address Library, SKSE Menu Framework 3, and the Microsoft Visual C++ 2015–2022 Redistributable (x64). This build targets SE/AE, not VR. No ESP, game INI edits or changes to other mods are included.

## Controls

Open Screenshot Lighting Assistant > Lighting in the mod control panel. Use EN at the top for English and JP to return to Japanese. The choice is saved across restarts. Custom names and NPC names are not translated.

- Select a slot (1-3), direction and type. Built-in order: Studio White, Studio Warm, Studio Blue, Candle.
- Start lighting to display the setup. Lights stay on when the menu closes.
- Each light has independent color, intensity, range, distance, height, fine offsets, shadows and power.
- Reset values restores only intensity, range, distance, height, fine offsets and shadow bias. It keeps direction, type, color, shadow mode and power, and can be undone.
- Shadow bias appears only in Spot mode. Hover for its explanation. It is depth bias, not a shadow blur control.
- Save light stores type and color only. Favorites > Manage > Rename changes a saved light's name.
- P / Choose preset opens the same gallery. Save preset stores the full three-light setup, optionally including the face light. Enable Manage in the gallery to rename, reorder or remove your presets.
- The person icon beside P opens Persistent Face Light settings. Enabling Player keeps an independent face light on the player outside photography sessions; assign a keyboard key or a long-press gamepad button for quick on/off control.
- Undo/Redo and the bookmark cover lighting values. The bookmark lasts only until game exit; use Save preset for durable storage.
- Preset diagrams use the older 140px proportions, uniformly enlarged, with name and light information on the right.

## NPC subjects

Choose subject opens a distance-sorted list of loaded humanoid NPCs within 30m of the player (up to 64). Refresh to rescan; there is no continuous background NPC scan. Select a candidate by name, distance and reference ID. Use player restores the player as subject.

Console selection and crosshair target are optional alternatives. In free camera or while a menu is open, the game may not supply a crosshair target; use the nearby list instead.

Changing the subject switches the lights off but keeps their settings. Close the subject window, check the displayed name, then press Start lighting. The three lights follow the subject's chest/head anchor; the face light uses that subject's head and the selected placement mode. Camera alignment still controls the three-light arrangement.

Only one photography subject is supported at a time. This mod does not freeze actors or change AI, schedules, poses or NPC records. It never force-loads NPCs. Subject loss or cell changes stop the photography lights; loading/main-menu transitions clear subject selection back to the player. Photography lights never restart automatically. The optional persistent player face light is separate and safely recreates after transitions. Subjects, handles and camera transforms are not written to presets or game saves.

## Storage and recovery

The actual storage path is shown by hovering over Storage location at the bottom. Version 0.1.28 targets virtual `Data/SKSE/Plugins/ScreenshotLightingAssistant/Presets`; under MO2 it should normally appear in `Overwrite/SKSE/Plugins/ScreenshotLightingAssistant/Presets`. It remains independent of Skyrim saves. Outside MO2, an unwritable Data path falls back to the old SKSE-log/My Games directory.

The first successful 0.1.28 launch copies the previous language preference, entries, list metadata and Archived directory without deleting the originals. `storage-v2.sla` prevents later re-import, so removing an entry from the new location does not make the old copy reappear. After verification, convert the Overwrite output into a dedicated writable-output mod or back it up as desired.

Existing .slalight/.slaset formats and saved light type IDs are unchanged. Favorites and preset order retain their file-based identity when an entry is renamed. Duplicate or invalid names are refused (120 UTF-8 bytes maximum, no control characters or ##). Historical entries keep the names recorded at that time.

Before renaming, an exact backup is placed in Archived with a filename ending in -before-rename-<original filename>. The original entry is atomically replaced under its existing filename. To restore a backup manually, close Skyrim, preserve the current file separately, then restore the backup under the corresponding original filename. Simply copying it into Presets with a different name creates a separate registration.

The language preference is stored one directory above Presets in `ui-language.sla`; the persistent player face-light profile is stored beside it in `persistent-face.sla`. Unknown/corrupt preferences are preserved rather than overwritten. Invalid saved entries are reported without deleting their files.

## Verification boundary

The MSVC DLL build and automated checks pass. The 0.1.29 photography features and beta2 persistent-light behavior were exercised in game by the author. Beta3's gamepad input still requires in-game verification. This does not claim compatibility with every game build or mod combination. Keep only one version of this DLL enabled.

This release does not claim compatibility with every game build or mod combination. See `RELEASE_CHECKLIST_JA.md` for the Japanese verification record and `ENGINE_NOTES.md` for implementation details. Source licensing is in `LICENSE` and `THIRD_PARTY_NOTICES.md`.

## Author and license

Copyright © 2026 kota (@kotaSkyrim)  
Licensed under GPL-3.0-or-later. See `LICENSE` and `THIRD_PARTY_NOTICES.md`.
