#include "LightingState.h"
#include "ScenePresets.h"
#include "PresetCardLayout.h"
#include "RuntimeModel.h"
#include "PositionPad.h"
#include "PositionWindowLayout.h"
#include "PresetLibrary.h"
#include "LightVisual.h"
#include "TargetModel.h"
#include "Localization.h"
#include "StorageLocation.h"
#include "PersistentFaceSettings.h"
#include <regex>
#include <set>

#include <chrono>
#include <fstream>
#include <iostream>
#include <limits>
#include <stdexcept>

using namespace ScreenshotLightingAssistant;

namespace
{
    int checks = 0;
    const char* lastCheck = "startup";
    void Require(bool a_condition, const char* a_message)
    {
        ++checks;
        lastCheck = a_message;
        if (!a_condition) {
            throw std::runtime_error(a_message);
        }
    }

    Scene OnlyLight(const LightSettings& light)
    {
        Scene scene{};
        scene[0] = light;
        return scene;
    }

    Scene ExpectedPresetScene(const ScenePreset& preset, const Scene& before)
    {
        auto expected = preset.scene;
        if (!preset.includeFace) { expected.face = before.face; }
        return expected;
    }

    void PresetComparisonSession()
    {
        LightingEditor editor;
        PresetPicker picker;
        Require(!picker.KeepOpen(), "comparison defaults to existing close-after-apply behavior");
        picker.SetKeepOpen(true);
        picker.Open();
        editor.Adopt();
        const auto bookmark = editor.GetScene();
        for (std::size_t i = 0; i < kScenePresets.size(); ++i) {
            Require(picker.Choose(i, editor) && picker.IsOpen(), "sample applies without closing in comparison mode");
            Require(editor.GetScene().lights == kScenePresets[i].scene.lights, "comparison applies the real light state");
            const auto history = editor.History().size();
            Require(picker.Choose(i, editor) && picker.IsOpen(), "identical preset stays open too");
            Require(editor.History().size() == history, "identical repeated click does not flood history");
        }
        const auto last = editor.GetScene();
        Require(!picker.Choose(kScenePresets.size(), editor) && picker.IsOpen() && editor.GetScene() == last,
            "invalid comparison choice neither closes nor changes lighting");
        picker.Close();
        Require(editor.GetScene() == last && picker.KeepOpen(), "X keeps applied lighting and session option");
        Require(!picker.Choose(0, editor), "closed comparison picker cannot apply");
        picker.Open();
        Require(picker.KeepOpen(), "reopen remembers comparison option");
        SavedScene user{"saved comparison", kScenePresets[0].scene, false, {}};
        Require(PresetLibrary::Apply(user, editor), "custom preset applies while comparing");
        picker.Applied();
        Require(picker.IsOpen(), "custom preset shares keep-open policy");
        picker.SetKeepOpen(false);
        Require(picker.IsOpen(), "unchecking keep-open does not close until next selection");
        user.scene = kScenePresets[1].scene;
        Require(PresetLibrary::Apply(user, editor), "custom preset still applies with keep-open off");
        picker.Applied();
        Require(!picker.IsOpen(), "custom preset closes when keep-open is off");
        editor.Undo();
        Require(editor.GetScene().lights == kScenePresets[0].scene.lights, "comparison changes remain undoable");
        editor.RestoreBookmark();
        Require(editor.GetScene() == bookmark, "comparison never replaces bookmark");
        PresetPicker nextProcess;
        Require(!nextProcess.KeepOpen(), "new process defaults are independent of prior session");
    }

    void PresetGeometryMemory()
    {
        PresetWindowGeometry geometry;
        const WindowRectangle work{0, 0, 2560, 1440};
        Require(geometry.Resolve(work) == WindowRectangle{720, 320, 1120, 800}, "first gallery opens centered");
        const WindowRectangle chosen{1400, 60, 1000, 900};
        geometry.Remember(chosen);
        Require(geometry.Resolve(work) == chosen && !geometry.NeedsClamp(work), "reopening preserves moved/resized gallery exactly");
        for (const auto viewport : {work, WindowRectangle{100, 50, 1280, 720}, WindowRectangle{-1920, 0, 1920, 1080},
                 WindowRectangle{0, 0, 320, 240}, WindowRectangle{0, 0, 3840, 2160}}) {
            for (const auto rectangle : {chosen, WindowRectangle{-500, -500, 1120, 800},
                     WindowRectangle{9000, 8000, 3000, 2000}, WindowRectangle{30, 40, 340, 300}}) {
                geometry.Remember(rectangle);
                const auto restored = geometry.Resolve(viewport);
                Require(restored.x >= viewport.x && restored.y >= viewport.y, "remembered gallery stays within viewport origin");
                Require(restored.x + restored.width <= viewport.x + viewport.width + .01F &&
                    restored.y + restored.height <= viewport.y + viewport.height + .01F, "resized viewport cannot strand title/close button off-screen");
                Require(restored.width > 0 && restored.height > 0 && restored.width <= viewport.width * .95F + .01F &&
                    restored.height <= viewport.height * .95F + .01F, "gallery obeys viewport-scaled size constraints");
                geometry.Remember(restored);
                Require(!geometry.NeedsClamp(viewport) && geometry.Resolve(viewport) == restored, "clamp settles without resize oscillation");
            }
        }
    }

    void CompactBookmarkRows()
    {
        for (const auto widths : {std::array<float, 3>{48, 56, 56}, std::array<float, 3>{90, 46, 78}}) {
            for (float scale : {.75F, 1.0F, 1.5F, 2.0F}) {
                const float label = widths[0] * scale, set = widths[1] * scale, restore = widths[2] * scale;
                const float gap = 8 * scale;
                const float total = BookmarkRowWidth(label, set, restore, gap);
                Require(total == label + set + restore + 2 * gap, "bookmark width includes plain label and two gaps");
                ActionRowLayout exact{total, gap};
                Require(!exact.Place(label) && exact.Place(set) && exact.Place(restore), "bookmark fits exact row width");
                ActionRowLayout narrow{total - 1, gap};
                Require(!narrow.Place(label) && narrow.Place(set) && !narrow.Place(restore), "bookmark restore wraps only below required width");
                Require(FitsInline(200 * scale + gap + total, 200 * scale, total, gap), "camera and bookmark fit exact combined width");
                Require(!FitsInline(200 * scale + gap + total - 1, 200 * scale, total, gap), "bookmark group moves below camera when needed");
            }
        }
        Localization::Set(Localization::Language::English);
        Require(std::string(Localization::Tr("栞を")) == "Bookmark", "English bookmark group label");
        Require(std::string(Localization::Tr("登録")) == "Set", "English bookmark set label");
        Require(std::string(Localization::Tr("戻す")) == "Restore", "English bookmark restore label");
        Localization::Set(Localization::Language::Japanese);
        Require(std::string(Localization::Tr("栞を")) == "栞を", "Japanese bookmark group label");
        Require(std::string(Localization::Tr("登録")) == "登録", "Japanese bookmark set label");
        Require(std::string(Localization::Tr("戻す")) == "戻す", "Japanese bookmark restore label");
    }

    void ManagementActionRows()
    {
        // Representative JP/EN widths, including both favorite-label states.
        // Production supplies measured text plus frame padding, never estimates.
        const std::array<std::array<float, 4>, 4> examples{{
            {{56, 176, 104, 56}}, {{52, 118, 84, 82}},
            {{56, 56, 104, 56}}, {{92, 70, 84, 82}}
        }};
        for (const auto& example : examples) {
            for (float scale : {.75F, 1.0F, 1.5F, 2.0F}) {
                for (float gap : {0.0F, 4.0F, 8.0F, 16.0F}) {
                    float total = gap * 3;
                    float largest = 0;
                    for (float size : example) {
                        total += size * scale;
                        largest = std::max(largest, size * scale);
                    }
                    ActionRowLayout exact{total, gap};
                    for (std::size_t i = 0; i < example.size(); ++i) {
                        Require(exact.Place(example[i] * scale) == (i != 0), "all four actions share one row when they fit exactly");
                    }
                    for (float width : {largest, total * .7F, total - .25F, total, total + 100}) {
                        ActionRowLayout row{width, gap};
                        float used = 0;
                        for (std::size_t i = 0; i < example.size(); ++i) {
                            const float size = example[i] * scale;
                            const bool expectedInline = i != 0 && used + gap + size <= width;
                            Require(row.Place(size) == expectedInline, "management buttons wrap only when needed, in supplied order");
                            used = expectedInline ? used + gap + size : size;
                            Require(row.used == used && row.used <= width, "new action row resets width and never overlaps the next column");
                        }
                    }
                }
            }
        }
        ActionRowLayout narrow{80, 8};
        Require(!narrow.Place(120), "oversized label starts a row without shrinking");
        Require(!narrow.Place(30), "action after oversized label wraps");
        Require(narrow.Place(30) && narrow.used == 68, "remaining actions can share the new row");
    }

    void IndependentSlotsAndDeletion()
    {
        LightingEditor editor;
        editor.SetNumeric(NumericField::Intensity, 2.0F);
        editor.SetNumeric(NumericField::Range, 7.0F);
        editor.SetNumeric(NumericField::Distance, 3.0F);
        editor.SetNumeric(NumericField::Height, 0.6F);
        editor.SetShadow(false);
        const auto first = editor.CurrentLight();
        const auto beforeSelection = editor.History().size();
        editor.SelectSlot(1);
        Require(editor.History().size() == beforeSelection, "slot selection must not add history");
        Require(!editor.CurrentLight().placed, "slot 2 begins empty");
        editor.SetType(2);
        Require(editor.CurrentLight().intensity == 1.0F, "slot 2 intensity is independent");
        Require(editor.CurrentLight().range == 4.0F, "slot 2 range is independent");
        Require(editor.CurrentLight().distance == 2.2F, "slot 2 distance is independent");
        Require(editor.CurrentLight().heightOffset == 0.0F, "slot 2 height is independent");
        Require(editor.CurrentLight().castsShadow, "slot 2 shadow is independent");
        editor.SetDirection(first.direction);  // Both lights occupy the same direction.
        editor.DeleteCurrent();
        Require(editor.GetScene()[0] == first, "delete must preserve the other co-located light");
        Require(!editor.GetScene()[1].placed, "only selected slot is deleted");
        const auto afterDeletion = editor.History().size();
        editor.DeleteCurrent();
        Require(editor.History().size() == afterDeletion, "deleting an empty slot is a no-op");
        editor.Undo();
        Require(editor.GetScene()[1].placed && editor.GetScene()[1].type == 2, "undo restores deleted light and its type");
        editor.Redo();
        Require(!editor.GetScene()[1].placed, "redo deletes it again");
        editor.SetType(1);
        Require(editor.CurrentLight().placed && editor.CurrentLight().type == 1, "deleted slot can be reused");
        Require(editor.GetScene()[0] == first, "reusing slot must preserve other lights");
        editor.SelectSlot(2);
        editor.SetDirection(6);
        Require(!editor.CurrentLight().placed && editor.CurrentLight().direction == 6, "direction may be staged before placement");
        editor.SetType(0);
        Require(editor.CurrentLight().placed && editor.CurrentLight().direction == 6, "placing uses the staged direction");
        editor.SelectSlot(0);
        Require(editor.CurrentLight() == first, "returning to slot 1 restores all its controls");
    }

    void DiagramSelectionAndExplicitMoves()
    {
        // Every ring node, occupant subset, starting selection and powered/muted case.
        // Other placed lights sit elsewhere: selecting must not move them into the group.
        for (int direction = 0; direction < 8; ++direction) {
            for (int mask = 0; mask < 8; ++mask) {
                for (int selected = 0; selected < 3; ++selected) {
                    for (bool powered : { false, true }) {
                        Scene scene{};
                        std::vector<int> occupants;
                        for (int slot = 0; slot < 3; ++slot) {
                            auto& light = scene[slot];
                            const bool occupies = (mask & (1 << slot)) != 0;
                            light.placed = true;
                            light.enabled = powered;
                            light.direction = occupies ? direction : (direction + 1) % 8;
                            light.type = slot;
                            light.intensity = slot == 2 ? 0.0F : 0.6F + slot * 0.4F;
                            light.distance = 1.0F + slot;
                            light.heightOffset = slot * 0.2F;
                            if (occupies) { occupants.push_back(slot); }
                        }
                        LightingEditor editor;
                        Require(editor.ApplyScene(scene, "diagram test"), "diagram test scene valid");
                        editor.SelectSlot(selected);
                        const auto historySize = editor.History().size();
                        std::optional<int> expectedSlot;
                        if (!occupants.empty()) {
                            const auto current = std::find(occupants.begin(), occupants.end(), selected);
                            expectedSlot = current == occupants.end() || current + 1 == occupants.end() ? occupants.front() : *(current + 1);
                        }
                        Require(editor.DirectionSelection(direction) == expectedSlot, "hover action predicts next occupant including OFF/zero strength");
                        Require(editor.GetScene() == scene && editor.SelectedSlot() == selected && editor.History().size() == historySize,
                            "querying a node never edits, selects or commits");
                        editor.ActivateDirection(direction);
                        if (expectedSlot) {
                            Require(editor.SelectedSlot() == *expectedSlot, "occupied node selects predicted slot");
                            Require(editor.GetScene() == scene && editor.History().size() == historySize, "occupied click changes neither lighting nor history");
                            for (std::size_t click = 0; click < occupants.size(); ++click) {
                                editor.ActivateDirection(direction);
                            }
                            Require(editor.SelectedSlot() == *expectedSlot && editor.GetScene() == scene, "group cycles and wraps without moving lights");
                        } else {
                            auto moved = scene;
                            moved[selected].direction = direction;
                            Require(editor.GetScene() == moved && editor.SelectedSlot() == selected, "empty node only moves current light");
                            Require(editor.History().size() == historySize + 1, "empty node move is one history entry");
                            editor.Undo();
                            Require(editor.GetScene() == scene, "undo empty move restores complete lighting");
                            editor.Redo();
                            Require(editor.GetScene() == moved, "redo empty move restores complete lighting");
                        }

                        // Context confirmation bypasses selection, preserving source identity.
                        LightingEditor explicitMove;
                        Require(explicitMove.ApplyScene(scene, "explicit move"), "explicit move scene valid");
                        explicitMove.SelectSlot(selected);
                        explicitMove.Adopt();
                        const auto beforeMoveHistory = explicitMove.History().size();
                        auto moved = scene;
                        moved[selected].direction = direction;
                        explicitMove.SetDirection(direction);
                        Require(explicitMove.GetScene() == moved && explicitMove.SelectedSlot() == selected, "context move preserves source and all other settings");
                        const bool changed = moved != scene;
                        Require(explicitMove.History().size() == beforeMoveHistory + (changed ? 1 : 0), "context move/no-op history count");
                        if (changed) {
                            explicitMove.Undo();
                            Require(explicitMove.GetScene() == scene, "undo context move restores layout");
                            explicitMove.Redo();
                            Require(explicitMove.GetScene() == moved, "redo context move restores overlap");
                        }
                        explicitMove.RestoreBookmark();
                        Require(explicitMove.GetScene() == scene && explicitMove.SelectedSlot() == selected, "bookmark survives node interaction");
                    }
                }
            }
        }

        LightingEditor editor;
        editor.SetDirection(4);
        editor.SelectSlot(1);
        editor.SetDirection(6);
        editor.SetType(1);
        const auto arranged = editor.GetScene();
        editor.SelectSlot(0);
        const auto beforeDrag = editor.History().size();
        editor.SetNumeric(NumericField::Intensity, 2.4F);
        editor.ActivateDirection(6);
        Require(editor.SelectedSlot() == 1 && editor.History().size() == beforeDrag + 1, "node selection finishes source slider exactly once");
        Require(editor.History().back().label == "Light 1 : 強さ" && editor.GetScene()[1] == arranged[1], "slider commit belongs to old slot");
        editor.Undo();
        Require(editor.GetScene() == arranged && editor.CanRedo(), "undo reverses slider, not selection");
        const auto redoSize = editor.History().size();
        editor.ActivateDirection(6);
        editor.ActivateDirection(6);
        Require(editor.SelectedSlot() == 1 && editor.CanRedo() && editor.History().size() == redoSize, "selection and single-occupant no-op preserve redo");
        editor.Redo();
        Require(editor.GetScene()[0].intensity == 2.4F, "redo still restores source slider after node selection");

        editor.SelectSlot(2);
        editor.SetDirection(6);  // Unplaced staged slot shares direction but is not an occupant.
        Require(editor.DirectionSelection(6) == 1, "unplaced staged slot is not a selectable occupant");
        const auto beforeStagedSelect = editor.GetScene();
        editor.ActivateDirection(6);
        Require(editor.SelectedSlot() == 1 && editor.GetScene() == beforeStagedSelect, "occupied click from unplaced slot only selects");
        editor.SelectSlot(2);
        editor.ActivateDirection(0);
        Require(!editor.CurrentLight().placed && editor.CurrentLight().direction == 0, "empty click stages without creating a light");
        editor.SetDirection(6);  // Right-click confirmation stages an occupied direction too.
        Require(!editor.CurrentLight().placed && editor.SelectedSlot() == 2, "context staging does not create or change selection");
        editor.SetType(2);
        Require(editor.CurrentLight().placed && editor.CurrentLight().direction == 6, "new light uses explicitly staged overlap");
        editor.DeleteCurrent();
        editor.ActivateDirection(6);
        Require(editor.SelectedSlot() == 1 && !editor.GetScene()[2].placed, "deleted light is skipped when cycling");

        const auto beforeBad = editor.GetScene();
        const auto beforeBadHistory = editor.History().size();
        for (int bad : { -1, 8, std::numeric_limits<int>::max() }) {
            Require(!editor.DirectionSelection(bad), "invalid node has no occupant");
            editor.ActivateDirection(bad);
            editor.SetDirection(bad);
        }
        Require(editor.GetScene() == beforeBad && editor.SelectedSlot() == 1 && editor.History().size() == beforeBadHistory,
            "invalid diagram actions leave everything intact");

        // Clicking a selected sole occupant must not finalize an unrelated in-progress drag.
        editor.SetNumeric(NumericField::Range, 7.0F);
        editor.ActivateDirection(6);
        Require(editor.HasPendingChanges() && editor.History().size() == beforeBadHistory, "single occupant selection is a genuine no-op");
        editor.ActivateDirection(7);
        Require(editor.History().size() == beforeBadHistory + 2, "empty move commits slider then movement separately");
        editor.Undo();
        Require(editor.CurrentLight().direction == 6 && editor.CurrentLight().range == 7.0F, "undo movement retains slider edit");
    }

    void CoalescedSliders()
    {
        LightingEditor editor;
        const auto initial = editor.GetScene();
        for (int i = 1; i <= 100; ++i) {
            editor.SetNumeric(NumericField::Intensity, 1.0F + i / 100.0F);
        }
        Require(editor.History().size() == 1, "100 slider frames must not add 100 history entries");
        Require(editor.HasPendingChanges() && editor.CanUndo(), "live gesture is undoable");
        editor.FinishNumericEdit();
        Require(editor.History().size() == 2, "one gesture creates one entry");
        editor.FinishNumericEdit();
        Require(editor.History().size() == 2, "finishing twice is a no-op");
        editor.Undo();
        Require(editor.GetScene() == initial, "undo restores pre-drag value");
        Require(editor.CanRedo(), "undo exposes redo");
        editor.SetNumeric(NumericField::Intensity, 1.8F);
        Require(!editor.CanRedo(), "pending new edit disables stale redo");
        editor.SetNumeric(NumericField::Intensity, 1.0F);
        editor.FinishNumericEdit();
        Require(editor.CanRedo() && editor.History().size() == 2, "returning to original value preserves redo branch");
        editor.Redo();
        Require(editor.CurrentLight().intensity == 2.0F, "redo restores entire gesture");
        editor.SetNumeric(NumericField::Intensity, 2.5F);
        editor.Undo();
        Require(editor.CurrentLight().intensity == 2.0F, "undo flushes and reverses unfinished gesture");
        editor.Redo();
        Require(editor.CurrentLight().intensity == 2.5F, "unfinished gesture remains redoable");
        editor.SetNumeric(NumericField::Range, 5.0F);
        editor.SetNumeric(NumericField::Height, 1.0F);
        Require(!editor.History().empty() && editor.History().back().scene[0].range == 5.0F, "switching fields commits previous gesture");
        editor.SelectSlot(1);
        Require(!editor.HasPendingChanges(), "switching slots commits pending gesture");
        Require(editor.GetScene()[0].heightOffset == 1.0F && editor.GetScene()[1].heightOffset == 0.0F, "pending edit stays on its own slot");
    }

    void HistoryAndBookmarks()
    {
        LightingEditor editor;
        Require(!editor.CanUndo() && !editor.CanRedo() && !editor.HasBookmark(), "initial navigation state");
        editor.Undo();
        editor.Redo();
        editor.RestoreBookmark();
        Require(editor.History().size() == 1, "unavailable navigation is harmless");
        editor.Adopt();
        const auto adopted = editor.GetScene();
        Require(editor.MatchesBookmark() && editor.History().size() == 1, "adoption records separate bookmark");
        editor.SetType(1);
        const auto typeId = editor.History().back().id;
        editor.SetShadow(false);
        editor.SetDirection(7);
        Require(!editor.MatchesBookmark(), "changes diverge from bookmark");
        Require(editor.JumpTo(typeId), "jump finds stable history ID");
        Require(editor.CurrentLight().type == 1 && editor.CurrentLight().castsShadow && editor.CurrentLight().direction == 1, "jump restores full scene");
        Require(editor.CanRedo(), "history jump retains future entries");
        const auto oldLastId = editor.History().back().id;
        editor.SetDirection(3);
        Require(!editor.CanRedo() && editor.History().size() == 3, "new edit truncates only redo branch");
        Require(!editor.JumpTo(oldLastId), "discarded future ID cannot be restored");
        const auto edited = editor.GetScene();
        editor.RestoreBookmark();
        Require(editor.GetScene() == adopted, "bookmark restores all settings");
        editor.Undo();
        Require(editor.GetScene() == edited, "bookmark restore itself can be undone");
        editor.Redo();
        Require(editor.MatchesBookmark(), "bookmark restoration can be redone");
        const auto size = editor.History().size();
        editor.RestoreBookmark();
        Require(editor.History().size() == size, "restoring matching bookmark is a no-op");

        for (int i = 0; i < 150; ++i) {
            editor.SetDirection(i % 8);
        }
        Require(editor.History().size() == LightingEditor::kHistoryLimit, "history remains bounded");
        Require(editor.HasBookmark(), "bookmark survives history eviction");
        for (std::size_t i = 1; i < editor.History().size(); ++i) {
            Require(editor.History()[i - 1].id < editor.History()[i].id, "history IDs stay increasing");
        }
        const auto evictedId = editor.History().front().id;
        const auto retainedId = editor.History()[1].id;
        editor.SetNumeric(NumericField::Intensity, 2.4F);
        Require(editor.JumpTo(retainedId), "jump resolves ID after pending commit prunes history");
        Require(!editor.JumpTo(evictedId), "pruned ID is not accidentally mapped to another entry");
        editor.RestoreBookmark();
        Require(editor.GetScene() == adopted, "evicted checkpoint scene remains recoverable via bookmark");
        editor.Adopt();
        editor.SetType(2);
        editor.Adopt();
        const auto replacedBookmark = editor.GetScene();
        editor.DeleteCurrent();
        editor.RestoreBookmark();
        Require(editor.GetScene() == replacedBookmark, "new adoption replaces the prior bookmark");
    }

    void LightPower()
    {
        LightingEditor editor;
        const auto initial = editor.GetScene();
        editor.ToggleLight(-1);
        editor.ToggleLight(3);
        editor.ToggleLight(1);
        Require(editor.GetScene() == initial && editor.History().size() == 1, "invalid/empty power controls are no-ops");
        editor.SelectSlot(1);
        editor.SetType(2);
        const auto second = editor.CurrentLight();
        editor.ToggleLight(0);
        Require(editor.SelectedSlot() == 1, "power button must not switch the editor selection");
        Require(editor.GetScene()[0].placed && !editor.GetScene()[0].enabled, "OFF retains placement and slot");
        Require(editor.CurrentLight() == second, "power toggle affects only its numbered slot");
        auto offExpected = initial[0];
        offExpected.enabled = false;
        Require(editor.GetScene()[0] == offExpected, "muting preserves all original light properties");
        Require(editor.History().back().label == "Light 1 : OFF", "history names the actual toggled slot");
        editor.Undo();
        Require(editor.GetScene()[0].enabled, "OFF can be undone");
        editor.Redo();
        Require(!editor.GetScene()[0].enabled, "OFF can be redone");
        editor.SelectSlot(0);
        editor.SetNumeric(NumericField::Intensity, 2.2F);
        editor.SetNumeric(NumericField::Height, 0.7F);
        editor.SetShadow(false);
        editor.SetDirection(4);
        editor.SetType(1);
        Require(!editor.CurrentLight().enabled, "editing an OFF light does not relight it");
        Require(editor.CurrentLight().intensity == 2.2F && editor.CurrentLight().heightOffset == 0.7F && !editor.CurrentLight().castsShadow, "OFF light remains editable");
        const auto beforeAdopt = editor.GetScene();
        editor.Adopt();
        editor.ToggleLight(0);
        Require(editor.CurrentLight().enabled && !editor.MatchesBookmark(), "ON differs from an OFF bookmark");
        editor.RestoreBookmark();
        Require(editor.GetScene() == beforeAdopt, "bookmark restores OFF and settings together");
        editor.DeleteCurrent();
        Require(!editor.CurrentLight().placed, "OFF light can be deleted");
        editor.Undo();
        Require(editor.CurrentLight().placed && !editor.CurrentLight().enabled, "undo delete restores muted placement");
        editor.Redo();
        editor.SetType(0);
        Require(editor.CurrentLight().placed && editor.CurrentLight().enabled, "fresh placement in deleted slot starts ON");
        editor.SetNumeric(NumericField::Range, 8.0F);
        const auto historySize = editor.History().size();
        editor.ToggleLight(0);
        Require(editor.History().size() == historySize + 2, "power toggle first commits unfinished slider separately");
        editor.Undo();
        Require(editor.CurrentLight().enabled && editor.CurrentLight().range == 8.0F, "undo power retains preceding slider adjustment");
    }

    void ScenePresetSelection()
    {
        LightingEditor editor;
        PresetPicker picker;
        const auto initial = editor.GetScene();
        Require(!picker.Choose(0, editor), "closed picker cannot apply a preset");
        picker.Open();
        Require(picker.IsOpen() && editor.GetScene() == initial, "opening picker does not alter lighting");
        Require(!picker.Choose(kScenePresets.size(), editor) && picker.IsOpen(), "invalid selection leaves picker open");
        picker.Close();
        Require(!picker.IsOpen() && editor.GetScene() == initial && editor.History().size() == 1, "cancel leaves scene and history untouched");
        for (std::size_t index = 0; index < kScenePresets.size(); ++index) {
            const auto& preset = kScenePresets[index];
            Require(LightingEditor::IsValidScene(preset.scene), "sample preset has valid properties");
            for (std::size_t other = index + 1; other < kScenePresets.size(); ++other) {
                Require(std::string(preset.id) != kScenePresets[other].id, "preset UI IDs are unique");
            }
            const auto previous = editor.GetScene();
            const auto expected = ExpectedPresetScene(preset, previous);
            const auto count = editor.History().size();
            picker.Open();
            Require(picker.Choose(index, editor) && !picker.IsOpen(), "selecting preset applies it and closes picker");
            Require(editor.GetScene() == expected, "sample applies its declared manual/face scope");
            Require(editor.History().size() == count + 1, "whole set application adds exactly one entry");
            Require(editor.History().back().label.find(preset.name) != std::string::npos, "history includes preset name");
            Require(!editor.HasBookmark(), "applying preset does not implicitly adopt it");
            editor.Undo();
            Require(editor.GetScene() == previous, "one undo restores previous complete scene");
            editor.Redo();
            Require(editor.GetScene() == expected, "one redo restores complete preset");
            const auto appliedCount = editor.History().size();
            picker.Open();
            Require(picker.Choose(index, editor) && !picker.IsOpen(), "choosing identical set still closes picker");
            Require(editor.History().size() == appliedCount, "identical set does not add duplicate history");
        }
        Require(!editor.GetScene()[1].enabled && editor.GetScene()[1].placed, "preset preserves an OFF placed slot");

        const auto blue = std::find_if(kScenePresets.begin(), kScenePresets.end(), [](const auto& preset) { return std::string_view(preset.id) == "rim_blue"; });
        const auto warm = std::find_if(kScenePresets.begin(), kScenePresets.end(), [](const auto& preset) { return std::string_view(preset.id) == "rim_warm"; });
        Require(blue != kScenePresets.end() && warm != kScenePresets.end(), "both supplied rim samples are built in");
        Require(blue->includeFace && blue->scene.face == FaceLightSettings{true, .29F, .92F, -.07F, FaceLightBasis::Head},
            "blue rim sample includes the supplied head-facing face light");
        Require(blue->scene[0].type == 1 && blue->scene[1].type == 3 && blue->scene[2].type == 3 &&
            blue->scene[0].fine == FinePosition{-.0892856866F, .178571448F, 0} &&
            blue->scene[1].fine == FinePosition{-.107142851F, .116071299F, 0} &&
            blue->scene[2].fine == FinePosition{.526785493F, .214285582F, 0},
            "blue rim sample retains the supplied light types and fine offsets");
        Require(!warm->includeFace && warm->scene[0].type == 1 && warm->scene[1].type == 1 && warm->scene[2].type == 0 &&
            warm->scene[2].fine == FinePosition{-.616071165F, .303571552F, 0},
            "warm rim sample retains the supplied three-light-only setup");
        const auto candleSpace = std::find_if(kScenePresets.begin(), kScenePresets.end(), [](const auto& preset) {
            return std::string_view(preset.id) == "candle_space";
        });
        Require(candleSpace != kScenePresets.end() && candleSpace->includeFace,
            "supplied Candle Space setup is a built-in face-inclusive sample");
        Require(candleSpace->scene[0] == DetailedPresetLight(3, 0, 1.38F, 5.0F, 2.20F, .25F, true, 1.0F) &&
            candleSpace->scene[1] == DetailedPresetLight(6, 2, 1.45F, 4.40F, 2.60F, 0.0F, false, 1.0F,
                {.97321409F, .562500119F, -.100000001F}) &&
            candleSpace->scene[2] == DetailedPresetLight(1, 2, 1.62F, 2.80F, 2.64F, 1.38F, true, 1.0F,
                {-.589285553F, .00892858859F, -.140000001F}),
            "Candle Space retains the supplied light values and fine offsets");
        Require(candleSpace->scene.face == FaceLightSettings{false, .29F, .92F, -.07F, FaceLightBasis::Head},
            "Candle Space includes the supplied disabled head-facing face settings");
        editor.Adopt();
        const auto adopted = editor.GetScene();
        editor.ToggleLight(1);
        editor.SetNumeric(NumericField::Intensity, 2.8F);
        picker.Open();
        const auto pendingScene = editor.GetScene();
        const auto historySize = editor.History().size();
        picker.Close();
        Require(editor.GetScene() == pendingScene && editor.History().size() == historySize, "cancel also preserves pending edit");
        picker.Open();
        picker.Choose(0, editor);
        Require(editor.History().size() == historySize + 2, "set application flushes pending edit then commits set");
        Require(editor.HasBookmark() && !editor.MatchesBookmark(), "preset application preserves separate bookmark");
        editor.Undo();
        Require(editor.GetScene() == pendingScene, "undo application restores last edited state including power");
        editor.RestoreBookmark();
        Require(editor.GetScene() == adopted, "adopted set and power survive subsequent presets");

        const auto unchanged = editor.GetScene();
        auto bad = unchanged;
        bad[2].direction = 8;
        Require(!editor.ApplyScene(bad, "invalid"), "invalid empty-slot direction rejected");
        bad = unchanged;
        bad[0].intensity = std::numeric_limits<float>::quiet_NaN();
        Require(!editor.ApplyScene(bad, "invalid"), "non-finite set property rejected");
        bad = unchanged;
        bad[1].type = 99;
        Require(!editor.ApplyScene(bad, "invalid"), "invalid set type rejected");
        Require(editor.GetScene() == unchanged, "invalid set is rejected atomically");
        Scene onlyThird{};
        onlyThird[2].placed = true;
        onlyThird[2].enabled = false;
        Require(editor.ApplyScene(onlyThird, "third"), "set with only third slot is valid");
        Require(editor.SelectedSlot() == 2, "application selects first placed slot even if OFF");
        Require(editor.ApplyScene(Scene{}, "empty") && editor.SelectedSlot() == 0, "empty scene can replace all three slots safely");
    }

    void CompactCardLayout()
    {
        using namespace ScreenshotLightingAssistant::UI;
        for (float width : { 240.0F, 320.0F, 440.0F, 620.0F, 900.0F }) {
            for (float font : { 20.0F, 24.0F, 32.0F, 48.0F }) {
                for (int count = 0; count <= 3; ++count) {
                    const auto layout = CalculatePresetCardLayout(width, font, font * 7.0F, count);
                    Require(layout.diagramX >= 8.0F && layout.diagramX + layout.diagramSize <= width - 7.9F, "diagram stays within card width");
                    Require(layout.diagramY >= 8.0F, "diagram stays inside top padding");
                    Require(layout.diagramY + layout.diagramSize + 15.0F * PresetDiagramScale(layout.diagramSize) <= layout.height - 7.9F, "camera fits above card bottom");
                    Require(layout.labelsX >= 8.0F && layout.labelsX + layout.labelsWidth <= width - 7.9F, "labels stay within card width");
                    Require(layout.titleX >= 8.0F && layout.titleX + layout.titleWidth <= width - 7.9F, "title stays within card width");
                    Require(layout.titleHeight == font * 2.0F, "two title lines use full font size");
                    Require(layout.titleY + layout.titleHeight + 5.9F <= layout.labelsY, "title and light information do not overlap");
                    if (count) {
                        Require(layout.labelsY + count * layout.rowHeight + (count - 1) * layout.rowGap <= layout.height - 7.9F, "all placed-light labels fit vertically");
                    }
                    if (layout.horizontal) {
                        Require(layout.diagramX + layout.diagramSize + 5.9F <= layout.labelsX, "diagram and labels do not overlap horizontally");
                        Require(layout.labelsWidth >= font * 7.0F + 16.0F, "horizontal labels retain measured font width");
                        Require(layout.titleX == layout.labelsX && layout.titleWidth == layout.labelsWidth, "name and light information share right column");
                        Require(layout.diagramSize >= 144.0F && layout.diagramSize <= 168.0F, "left diagram remains readable without unbounded growth");
                    } else {
                        Require(layout.labelsY >= layout.diagramY + layout.diagramSize + 15.0F, "narrow layout puts labels below camera");
                        Require(layout.titleY + layout.titleHeight + 5.9F <= layout.diagramY, "narrow layout puts diagram below name");
                    }
                    Require(layout.height == CalculatePresetCardLayout(width, font, font * 7.0F, 3).height, "all light counts keep equal card heights");
                }
            }
        }
        const auto normal = CalculatePresetCardLayout(440.0F, 24.0F, 168.0F, 3);
        Require(normal.horizontal, "normal two-column card keeps diagram beside labels");
        Require(normal.height < (220.0F + 24.0F * 3.4F + 50.0F) * 0.8F, "normal card is at least 20 percent shorter than previous layout");
        const auto narrow = CalculatePresetCardLayout(320.0F, 32.0F, 224.0F, 3);
        Require(!narrow.horizontal, "narrow card uses readable vertical fallback");
        const auto boundary = CalculatePresetCardLayout(MinimumPresetCardWidth(168.0F), 24.0F, 168.0F, 2);
        Require(boundary.horizontal && boundary.diagramSize >= 144.0F, "column threshold leaves readable diagram size");
        Require(normal.height <= 202.0F && normal.diagramSize == 168.0F, "compact diagram reduces normal card height");
        Require(normal.height <= 228.0F * .89F, "normal card is at least eleven percent shorter than 0.1.27");
        Require(!CalculatePresetCardLayout(MinimumPresetCardWidth(168.0F) - 1, 24, 168, 3).horizontal, "one pixel below threshold wraps without shrinking legend");

        // A font-free text measure: ASCII is one unit, a UTF-8 codepoint is two.
        const auto measure = [](const std::string& text) {
            float width = 0;
            for (const unsigned char c : text) {
                if ((c & 0xC0U) != 0x80U) { width += c < 0x80U ? 1.0F : 2.0F; }
            }
            return width;
        };
        const auto one = PresetTitleLines("Portrait", 8, measure);
        Require(one[0] == "Portrait" && one[1].empty(), "short name remains one line at full font size");
        const auto two = PresetTitleLines("暖色ポートレート", 8, measure);
        Require(two[0] + two[1] == "暖色ポートレート", "Japanese name wraps without losing codepoints");
        for (const std::string name : {"", "A", "Very long portrait preset name for testing",
                 "青い窓際と暖色の部屋で撮影するプリセット", "白💡と青💡を重ねるライト構成"}) {
            for (float width : {0.0F, 1.0F, 3.0F, 8.0F, 12.0F, 64.0F}) {
                const auto lines = PresetTitleLines(name, width, measure);
                for (const auto& line : lines) {
                    Require(measure(line) <= width, "wrapped title never extends beyond info column");
                    Require(line.empty() || PresetLibrary::ValidName(line), "title truncation preserves valid UTF-8 including four-byte codepoints");
                }
                if (width >= 3 && lines[0] + lines[1] != name) {
                    Require(lines[1].ends_with("..."), "truncated title is visibly marked; full name remains in tooltip");
                }
            }
        }
    }

    void RuntimeSessionBoundaries()
    {
        LightSession session;
        LightingEditor editor;
        auto light = editor.GetScene()[0];
        Require(!session.Start(0, OnlyLight(light)), "runtime cannot start before game ready");
        session.SetReady(true);
        Require(!session.Get().running, "ready never auto-starts light");
        auto oldEpoch = session.Get().epoch;
        Require(session.Start(oldEpoch, OnlyLight(light)), "explicit start accepted");
        Require(!session.Submit(oldEpoch, OnlyLight(light)), "pre-start frame token is invalidated");
        auto activeEpoch = session.Get().epoch;
        Require(LightSession::ShouldIlluminate(session.Get(), 0), "placed enabled white light illuminates");
        light.enabled = false;
        Require(session.Submit(activeEpoch, OnlyLight(light)), "OFF accepted without discarding settings");
        Require(!LightSession::ShouldIlluminate(session.Get(), 0) && session.Get().running, "OFF removes light but session stays active");
        light.enabled = true;
        light.type = 1;
        session.Submit(activeEpoch, OnlyLight(light));
        Require(LightSession::ShouldIlluminate(session.Get(), 0), "warm type is now supported");
        light.type = 0;
        light.intensity = 0;
        session.Submit(activeEpoch, OnlyLight(light));
        Require(!LightSession::ShouldIlluminate(session.Get(), 0), "zero intensity extinguishes runtime light");
        light.intensity = 1;
        light.placed = false;
        session.Submit(activeEpoch, OnlyLight(light));
        Require(!LightSession::ShouldIlluminate(session.Get(), 0), "deletion extinguishes runtime light");
        light.placed = true;
        session.Submit(activeEpoch, OnlyLight(light));
        const auto beforeAlign = session.Get().alignment;
        Require(session.Align(activeEpoch) && session.Get().alignment == beforeAlign + 1, "camera rebase is explicit");
        Require(session.Get().lights[0] == light, "rebase retains relative settings");
        session.SetReady(false); // kPreLoadGame
        Require(!session.Get().running && !session.Get().ready, "pre-load disarms and closes ready gate");
        Require(!session.Submit(activeEpoch, OnlyLight(light)) && !session.Start(activeEpoch, OnlyLight(light)), "old UI frame cannot resurrect after load");
        session.SetReady(true); // kPostLoadGame success
        Require(!session.Get().running, "successful load never recreates previous light");
        Require(!session.Start(activeEpoch, OnlyLight(light)), "stale start stays rejected after ready reopens");
        Require(session.Start(session.Get().epoch, OnlyLight(light)), "new explicit start after load works");
        activeEpoch = session.Get().epoch;
        session.Block(true); // Main/loading menu
        Require(!session.Get().running && session.Get().blocked, "menu transition cancels session");
        Require(!session.Start(session.Get().epoch, OnlyLight(light)), "blocked menu cannot start a session");
        session.Block(false);
        Require(!session.Get().running, "closing transition menu does not restart light");
        Require(!session.Submit(activeEpoch, OnlyLight(light)), "old request invalid across menu boundary");
        Require(session.Start(session.Get().epoch, OnlyLight(light)), "can start after transition");
        activeEpoch = session.Get().epoch;
        Require(!session.Stop(oldEpoch) && session.Get().running, "old stop cannot kill a newer session");
        auto invalid = light;
        invalid.range = std::numeric_limits<float>::quiet_NaN();
        Require(!session.Submit(activeEpoch, OnlyLight(invalid)), "invalid request rejected");
        Require(session.Get().lights[0] == light, "invalid request leaves last settings intact");
        Require(session.Stop(activeEpoch), "explicit stop works");
        Require(!session.Get().running && !session.Submit(activeEpoch, OnlyLight(light)), "stop invalidates pending UI work");
        session.SetReady(false); // Failed load
        Require(!session.Start(session.Get().epoch, OnlyLight(light)), "failed load stays disarmed");

        // Undo/redo operate on settings only, and feed the same runtime request path.
        session.SetReady(true);
        session.Start(session.Get().epoch, OnlyLight(editor.GetScene()[0]));
        activeEpoch = session.Get().epoch;
        editor.DeleteCurrent();
        session.Submit(activeEpoch, OnlyLight(editor.GetScene()[0]));
        Require(!LightSession::ShouldIlluminate(session.Get(), 0), "delete history state removes runtime light");
        editor.Undo();
        session.Submit(activeEpoch, OnlyLight(editor.GetScene()[0]));
        Require(LightSession::ShouldIlluminate(session.Get(), 0), "undo restores runtime request without any stored handle");
        editor.SetShadow(false);
        session.Submit(activeEpoch, OnlyLight(editor.GetScene()[0]));
        Require(!session.Get().lights[0].castsShadow, "shadow request travels with light settings");
        editor.Redo();
        session.SetReady(true); // New game, same lifetime UI state remains available.
        Require(!session.Get().running, "new game reset also disarms");
    }

    void RelativeLightGeometry()
    {
        const auto near = [](float a, float b) { return std::abs(a - b) < 0.002F; };
        const LightVector anchor{ 100, 200, 300 };
        const auto basis = MakeCameraBasis(anchor, { 100, -100, 450 });
        Require(basis.has_value(), "camera in front defines horizontal basis");
        Require(near(basis->front.x, 0) && near(basis->front.y, -1), "front points from actor toward camera");
        Require(near(basis->right.x, 1) && near(basis->right.y, 0), "right is screen right for level camera");
        auto light = LightingEditor{}.GetScene()[0];
        light.distance = 2;
        light.heightOffset = 0.5F;
        for (int direction = 0; direction < 8; ++direction) {
            light.direction = direction;
            const auto p = LightPosition(anchor, *basis, light);
            Require(p.has_value(), "all ring directions produce position");
            Require(near(std::hypot(p->x - anchor.x, p->y - anchor.y), 2 * kUnitsPerMetre), "direction preserves horizontal distance");
            Require(near(p->z, anchor.z + 0.5F * kUnitsPerMetre), "height stays relative independent of direction");
            const auto moved = LightPosition({ 105, 210, 280 }, *basis, light);
            Require(near(moved->x - p->x, 5) && near(moved->y - p->y, 10) && near(moved->z - p->z, -20), "pose anchor translation carries whole light position");
        }
        light.direction = 4;
        auto p = LightPosition(anchor, *basis, light);
        Require(p->y < anchor.y && near(p->x, anchor.x), "bottom ring node is camera-facing front");
        light.direction = 2;
        p = LightPosition(anchor, *basis, light);
        Require(p->x > anchor.x && near(p->y, anchor.y), "right ring node is screen right");
        const auto turned = MakeCameraBasis(anchor, { 400, 200, 300 });
        p = LightPosition(anchor, *turned, light);
        Require(p->y > anchor.y && near(p->x, anchor.x), "90-degree camera rebase rotates right correctly");
        Require(!MakeCameraBasis(anchor, { 100, 200, 900 }), "vertical-only camera avoids undefined horizontal basis");
        Require(!MakeCameraBasis(anchor, { std::numeric_limits<float>::infinity(), 0, 0 }), "nonfinite camera rejected");
        light.direction = 8;
        Require(!LightPosition(anchor, *basis, light), "invalid direction cannot reach engine");
    }

    void ShadowBiasStateAndRegistration()
    {
        LightingEditor editor;
        const auto baseline = editor.CurrentLight();
        Require(baseline.shadowBias == 1.0F, "baseline retains 0.1.6 bias");
        const auto originalRegistration = LightRegistrationSettings::From(baseline);
        Require(originalRegistration.shadow && originalRegistration.depthBias == 1.0F, "creation uses baseline shadow settings");
        editor.SetShadowBias(1.0F);
        Require(editor.History().size() == 1, "baseline button is a no-op");
        for (const float value : kShadowBiasLevels) {
            const auto count = editor.History().size();
            editor.SetShadowBias(value);
            const auto light = editor.CurrentLight();
            Require(light.shadowBias == value, "all comparison choices are retained exactly");
            Require(editor.History().size() == count + 1, "each comparison click is one history entry");
            auto unchanged = light;
            unchanged.shadowBias = baseline.shadowBias;
            Require(unchanged == baseline, "bias changes no position intensity range type or power");
            const auto registration = LightRegistrationSettings::From(light);
            Require(registration.depthBias == value, "shadow creation receives requested bias");
            Require(registration == LightRegistrationSettings::From(light), "unchanged frames do not request re-registration");
        }
        editor.Undo();
        Require(editor.CurrentLight().shadowBias == 4.0F, "undo restores bias");
        editor.Redo();
        Require(editor.CurrentLight().shadowBias == 8.0F, "redo restores bias");
        editor.Adopt();
        editor.SetShadowBias(1.0F);
        editor.RestoreBookmark();
        Require(editor.CurrentLight().shadowBias == 8.0F, "bookmark restores bias");
        editor.ToggleLight(0);
        editor.SetShadowBias(2.0F);
        Require(!editor.CurrentLight().enabled && editor.CurrentLight().shadowBias == 2.0F, "muted bias edit never enables light");
        editor.SetShadow(false);
        const auto noShadow = LightRegistrationSettings::From(editor.CurrentLight());
        editor.SetShadowBias(4.0F);
        Require(noShadow == LightRegistrationSettings::From(editor.CurrentLight()), "shadow OFF bias edit needs no engine rebuild");
        editor.SetShadow(true);
        Require(LightRegistrationSettings::From(editor.CurrentLight()).depthBias == 4.0F, "re-enabling shadows uses retained bias");
        Require(LightRegistrationSettings::From(editor.CurrentLight()) != noShadow, "shadow toggle requests new registration");
        editor.SetType(1);
        Require(editor.CurrentLight().shadowBias == 4.0F && !editor.CurrentLight().enabled, "type selection retains bias and mute");
        editor.DeleteCurrent();
        editor.SetShadowBias(8.0F);
        Require(editor.CurrentLight().shadowBias == 4.0F, "empty slot cannot edit bias");
        editor.SetType(0);
        Require(editor.CurrentLight().shadowBias == 4.0F && editor.CurrentLight().enabled, "replacement retains bias while enabling light");
        editor.SelectSlot(1);
        editor.SetType(0);
        editor.SetShadowBias(0.5F);
        Require(editor.GetScene()[0].shadowBias == 4.0F, "second slot bias is independent");
        const auto custom = editor.GetScene();
        editor.ApplyScene(kScenePresets[0].scene, "sample");
        Require(editor.CurrentLight().shadowBias == 1.0F, "built-in set carries baseline bias");
        Require(editor.ApplyScene(custom, "custom"), "valid scene accepts per-slot bias");
        Require(editor.GetScene() == custom, "scene application preserves per-slot bias");
        const auto count = editor.History().size();
        for (const float bad : { std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::infinity() }) {
            editor.SetShadowBias(bad);
            Require(editor.GetScene() == custom && editor.History().size() == count, "nonfinite bias leaves state and history untouched");
        }
        for (const float bad : { 0.0F, 8.1F, std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::infinity() }) {
            auto invalid = custom;
            invalid[2].shadowBias = bad;
            Require(!editor.ApplyScene(invalid, "invalid"), "invalid bias in any slot rejects entire scene");
            Require(editor.GetScene() == custom, "invalid scene is atomic");
            LightSession session;
            session.SetReady(true);
            Require(!session.Start(session.Get().epoch, OnlyLight(invalid[2])), "invalid start bias rejected");
            session.Start(session.Get().epoch, OnlyLight(custom[0]));
            Require(!session.Submit(session.Get().epoch, OnlyLight(invalid[2])), "invalid submission bias rejected");
            Require(session.Get().lights[0] == custom[0], "invalid submission preserves current request");
        }
        editor.SetShadowBias(-100.0F);
        Require(editor.CurrentLight().shadowBias == 0.5F, "finite low input clamped");
        editor.SetShadowBias(100.0F);
        Require(editor.CurrentLight().shadowBias == 8.0F, "finite high input clamped");
        const auto tuned = editor.CurrentLight();
        LightSession session;
        session.SetReady(true);
        Require(session.Start(session.Get().epoch, OnlyLight(tuned)), "start accepts tuned bias");
        const auto epoch = session.Get().epoch;
        session.SetReady(false);
        Require(!session.Submit(epoch, OnlyLight(baseline)), "stale baseline cannot overwrite after load");
        session.SetReady(true);
        Require(!session.Get().running, "tuned bias never auto-restarts after load");
        Require(session.Start(session.Get().epoch, OnlyLight(tuned)) && session.Get().lights[0].shadowBias == 8.0F, "explicit restart retains tuned bias");
    }

    void SpotlightStateAndRegistration()
    {
        LightingEditor editor;
        Require(editor.CurrentLight().shadowProjection == ShadowProjection::Spot, "new lights default to spot");
        editor.SetShadowMode(true, ShadowProjection::Omni);
        const auto baseline = editor.CurrentLight();
        const auto omni = LightRegistrationSettings::From(baseline);
        Require(baseline.shadowProjection == ShadowProjection::Omni, "explicit comparison selects omni");
        Require(!omni.spot && omni.fov == 2.0F * std::numbers::pi_v<float> && omni.falloff == 1 && omni.nearDistance == 5, "omni creation parameters unchanged");
        editor.SetShadowMode(true, ShadowProjection::Omni);
        Require(editor.History().size() == 2, "initial omni click is no-op");
        editor.SetShadowMode(true, ShadowProjection::Spot);
        const auto spotlight = editor.CurrentLight();
        Require(editor.History().size() == 3, "projection switch is one undo entry");
        auto unmodified = spotlight;
        unmodified.shadowProjection = baseline.shadowProjection;
        Require(unmodified == baseline, "spot mode changes no intensity range position type bias or power");
        const auto spot = LightRegistrationSettings::From(spotlight);
        Require(spot.shadow && spot.spot && spot.depthBias == omni.depthBias, "spotlight keeps shadow and bias");
        Require(spot.fov == std::numbers::pi_v<float> / 2 && spot.falloff == 5 && spot.nearDistance == 7.508994F, "spot template uses documented radians and near plane");
        Require(spot != omni && spot == LightRegistrationSettings::From(spotlight), "mode transition rebuilds once; steady frames do not");
        editor.Undo();
        Require(editor.CurrentLight() == baseline, "undo restores omni exactly");
        editor.SetShadowMode(true, ShadowProjection::Omni);
        Require(editor.CanRedo() && editor.History().size() == 3, "no-op mode does not prune redo");
        editor.Redo();
        Require(editor.CurrentLight() == spotlight, "redo restores spotlight exactly");
        editor.Adopt();
        editor.SetShadowMode(false, ShadowProjection::Spot);
        auto off = LightRegistrationSettings::From(editor.CurrentLight());
        Require(!off.shadow && !off.spot && off.fov == omni.fov, "shadowless reference is full omni, not an invisible spot cone");
        editor.SetShadowMode(false, ShadowProjection::Omni);
        editor.SetShadowBias(4);
        Require(off == LightRegistrationSettings::From(editor.CurrentLight()), "inactive projection and bias edits do not rebuild");
        editor.RestoreBookmark();
        Require(editor.CurrentLight() == spotlight, "bookmark restores entire spotlight state");
        editor.ToggleLight(0);
        editor.SetShadowMode(true, ShadowProjection::Omni);
        editor.SetShadowMode(true, ShadowProjection::Spot);
        Require(!editor.CurrentLight().enabled, "projection switch never unmutes a light");
        editor.SetType(1);
        Require(editor.CurrentLight().shadowProjection == ShadowProjection::Spot, "changing colour type retains projection");
        editor.DeleteCurrent();
        const auto deleted = editor.CurrentLight();
        editor.SetShadowMode(false, ShadowProjection::Omni);
        Require(editor.CurrentLight() == deleted, "unplaced slot cannot edit projection");
        editor.SetType(0);
        Require(editor.CurrentLight().shadowProjection == ShadowProjection::Spot, "replacement retains projection");
        editor.SelectSlot(1);
        editor.SetType(0);
        editor.SetShadowMode(false, ShadowProjection::Omni);
        Require(editor.GetScene()[0].shadowProjection == ShadowProjection::Spot && editor.GetScene()[0].castsShadow, "other slots remain independent");
        const auto custom = editor.GetScene();
        editor.ApplyScene(kScenePresets[0].scene, "sample");
        Require(editor.CurrentLight().shadowProjection == ShadowProjection::Spot, "built-in presets now use spot");
        Require(editor.ApplyScene(custom, "custom") && editor.GetScene() == custom, "scene applies mixed projections atomically");
        const auto historyCount = editor.History().size();
        for (int value : { -1, 2, 99 }) {
            const auto bad = static_cast<ShadowProjection>(value);
            editor.SetShadowMode(true, bad);
            Require(editor.GetScene() == custom && editor.History().size() == historyCount, "invalid projection rejects edit without mutation");
            auto invalid = custom;
            invalid[2].shadowProjection = bad;
            invalid[2].castsShadow = false;
            Require(!editor.ApplyScene(invalid, "invalid") && editor.GetScene() == custom, "invalid inactive projection rejects whole scene");
            LightSession session;
            session.SetReady(true);
            Require(!session.Start(session.Get().epoch, OnlyLight(invalid[2])), "invalid projection cannot start runtime");
            session.Start(session.Get().epoch, OnlyLight(spotlight));
            Require(!session.Submit(session.Get().epoch, OnlyLight(invalid[2])) && session.Get().lights[0] == spotlight, "invalid submission keeps last light");
        }
        LightingEditor gesture;
        gesture.SetNumeric(NumericField::Intensity, 1.5F);
        gesture.SetShadowMode(true, ShadowProjection::Omni);
        Require(gesture.History().size() == 3, "pending slider finishes before mode commit");
        gesture.Undo();
        Require(gesture.CurrentLight().intensity == 1.5F && gesture.CurrentLight().shadowProjection == ShadowProjection::Spot, "undo mode leaves prior slider adjustment");
        gesture.Undo();
        Require(gesture.CurrentLight() == LightingEditor{}.CurrentLight(), "second undo restores original slider");

        LightSession session;
        session.SetReady(true);
        Require(session.Start(session.Get().epoch, OnlyLight(spotlight)), "spotlight can start");
        auto epoch = session.Get().epoch;
        auto light = spotlight;
        light.castsShadow = false;
        Require(session.Submit(epoch, OnlyLight(light)) && !LightRegistrationSettings::From(session.Get().lights[0]).shadow, "shadowless comparison uses same session");
        Require(session.Submit(epoch, OnlyLight(spotlight)) && LightSession::ShouldIlluminate(session.Get(), 0), "spotlight comparison returns within same session");
        light.enabled = false;
        session.Submit(epoch, OnlyLight(light));
        Require(!LightSession::ShouldIlluminate(session.Get(), 0), "mute removes comparison light");
        light = spotlight; light.placed = false;
        session.Submit(epoch, OnlyLight(light));
        Require(!LightSession::ShouldIlluminate(session.Get(), 0), "delete removes comparison light");
        session.SetReady(false);
        Require(!session.Submit(epoch, OnlyLight(spotlight)), "old spotlight cannot survive load token reset");
        session.SetReady(true);
        Require(!session.Get().running, "load completion never auto-starts spot");
        session.Start(session.Get().epoch, OnlyLight(spotlight));
        epoch = session.Get().epoch;
        session.Block(true); session.Block(false);
        Require(!session.Get().running && !session.Submit(epoch, OnlyLight(spotlight)), "menu transition invalidates spot like omni");
    }

    void ThreeLightScenes()
    {
        LightSession session;
        session.SetReady(true);
        Scene scene{};
        for (std::size_t slot = 0; slot < scene.size(); ++slot) {
            auto& light = scene[slot];
            light.placed = true;
            light.type = static_cast<int>(slot);
            light.direction = static_cast<int>(slot) * 2;
            light.castsShadow = slot != 1;
            light.shadowProjection = slot == 2 ? ShadowProjection::Omni : ShadowProjection::Spot;
        }
        Require(session.Start(session.Get().epoch, scene), "three-colour scene starts atomically");
        auto epoch = session.Get().epoch;
        Require(session.Get().lights == scene, "all three settings reach runtime, not just selection");
        for (int placed = 0; placed < 8; ++placed) {
            for (int enabled = 0; enabled < 8; ++enabled) {
                for (int nonzero = 0; nonzero < 8; ++nonzero) {
                    auto request = scene;
                    for (std::size_t slot = 0; slot < request.size(); ++slot) {
                        request[slot].placed = (placed & (1 << slot)) != 0;
                        request[slot].enabled = (enabled & (1 << slot)) != 0;
                        request[slot].intensity = (nonzero & (1 << slot)) ? 1.0F : 0.0F;
                    }
                    Require(session.Submit(epoch, request), "all 512 placement/mute/strength combinations accepted");
                    for (std::size_t slot = 0; slot < request.size(); ++slot) {
                        const bool expected = (placed & enabled & nonzero & (1 << slot)) != 0;
                        Require(LightSession::ShouldIlluminate(session.Get(), slot) == expected, "only corresponding slot is activated or removed");
                    }
                }
            }
        }
        Require(!LightSession::ShouldIlluminate(session.Get(), 3) &&
            !LightSession::ShouldIlluminate(session.Get(), std::numeric_limits<std::size_t>::max()), "invalid slot never indexes array");
        session.Submit(epoch, scene);
        for (std::size_t slot = 0; slot < scene.size(); ++slot) {
            for (int field = 0; field < 7; ++field) {
                auto invalid = scene;
                switch (field) {
                case 0: invalid[slot].range = std::numeric_limits<float>::quiet_NaN(); break;
                case 1: invalid[slot].intensity = std::numeric_limits<float>::infinity(); break;
                case 2: invalid[slot].type = kLightTypeCount; break;
                case 3: invalid[slot].direction = -1; break;
                case 4: invalid[slot].shadowProjection = static_cast<ShadowProjection>(99); break;
                case 5: invalid[slot].distance = 0; break;
                case 6: invalid[slot].shadowBias = -1; break;
                }
                invalid[slot].enabled = false; invalid[slot].placed = false;
                invalid[(slot + 1) % 3].intensity = 2.5F; // Must not partially apply this valid edit.
                Require(!session.Submit(epoch, invalid), "invalid empty/muted slot rejects whole runtime request");
                Require(session.Get().lights == scene, "invalid request cannot partially change another light");
                LightSession fresh; fresh.SetReady(true);
                Require(!fresh.Start(fresh.Get().epoch, invalid), "invalid scene cannot start at any slot");
            }
        }
        for (int type = 0; type < kLightTypeCount; ++type) {
            const auto tint = RuntimeTint(type);
            Require(tint.has_value(), "every UI type has an actual tint");
            Require(tint->red > 0 && tint->red <= 1 && tint->green > 0 && tint->green <= 1 && tint->blue > 0 && tint->blue <= 1, "tints remain bounded without intensity multiplier");
            for (std::size_t slot = 0; slot < scene.size(); ++slot) {
                auto request = scene;
                const auto registration = LightRegistrationSettings::From(request[slot]);
                request[slot].type = type;
                Require(session.Submit(epoch, request), "all colours work in all slots");
                Require(LightSession::ShouldIlluminate(session.Get(), slot), "colour never disables a valid light");
                Require(registration == LightRegistrationSettings::From(request[slot]), "colour change needs no shadow-registration rebuild");
            }
        }
        Require(RuntimeTint(0) == std::optional<LightTint>{ { 1, 1, 1 } }, "white stays identical to 0.1.8");
        Require(RuntimeTint(1)->green > RuntimeTint(2)->green && RuntimeTint(1)->blue > RuntimeTint(2)->blue, "candle is warmer than studio warm");
        Require(!RuntimeTint(-1) && !RuntimeTint(kLightTypeCount), "unknown tint fails instead of silently becoming white");

        LightingEditor editor;
        editor.ApplyScene(scene, "three colours");
        editor.Adopt();
        session.Submit(epoch, editor.GetScene());
        for (int slot = 0; slot < 3; ++slot) {
            editor.SelectSlot(slot);
            Require(editor.GetScene() == scene, "selection alone changes no scene values");
            editor.ToggleLight(slot);
            session.Submit(epoch, editor.GetScene());
            for (int check = 0; check < 3; ++check) {
                Require(LightSession::ShouldIlluminate(session.Get(), check) == (check != slot), "mute touches only selected slot");
            }
            editor.Undo(); session.Submit(epoch, editor.GetScene());
            Require(session.Get().lights == scene, "undo restores full three-colour runtime snapshot");
            editor.SelectSlot(slot); // Undo also restores the earlier editor selection.
            editor.DeleteCurrent(); session.Submit(epoch, editor.GetScene());
            Require(!LightSession::ShouldIlluminate(session.Get(), slot), "delete extinguishes correct slot");
            editor.RestoreBookmark(); session.Submit(epoch, editor.GetScene());
            Require(session.Get().lights == scene, "bookmark restores all three slots after delete");
        }
        for (const auto& preset : kScenePresets) {
            editor.ApplyScene(scene, "before preset");
            const auto history = editor.History().size();
            editor.ApplyScene(preset.scene, preset.name);
            Require(editor.History().size() == history + 1, "whole preset is a single history entry");
            session.Submit(epoch, editor.GetScene());
            Require(session.Get().lights == preset.scene, "full preset replaces all runtime slots including empty third");
            for (std::size_t slot = 0; slot < scene.size(); ++slot) {
                const auto& light = preset.scene[slot];
                Require(LightSession::ShouldIlluminate(session.Get(), slot) == (light.placed && light.enabled && light.intensity > 0), "two-light/one-light presets remove leftover slots");
                Require(light.shadowProjection == ShadowProjection::Spot, "preset shadow mode defaults to spot even in empty/off slots");
            }
            editor.Undo(); session.Submit(epoch, editor.GetScene());
            Require(session.Get().lights == scene, "undo preset restores all old lights");
            editor.Redo(); session.Submit(epoch, editor.GetScene());
            Require(session.Get().lights == preset.scene, "redo preset removes old extra light again");
        }
        session.Submit(epoch, scene);
        const auto alignment = session.Get().alignment;
        Require(session.Align(epoch) && session.Get().alignment == alignment + 1 && session.Get().lights == scene, "one camera alignment preserves all relative settings");
        for (int boundary = 0; boundary < 4; ++boundary) {
            if (boundary == 0) { session.Stop(epoch); }
            if (boundary == 1) { session.SetReady(false); }
            if (boundary == 2) { session.Block(true); }
            if (boundary == 3) { session.SetReady(true); }
            for (std::size_t slot = 0; slot < scene.size(); ++slot) {
                Require(!LightSession::ShouldIlluminate(session.Get(), slot), "stop/load/menu/new game gates every light");
            }
            Require(!session.Submit(epoch, scene) && !session.Start(epoch, scene) && !session.Align(epoch), "stale operations cannot resurrect any slot");
            session.Block(false); session.SetReady(true);
            Require(!session.Get().running, "closing menus and finishing loads never auto-restarts");
            Require(session.Start(session.Get().epoch, scene), "explicit three-light restart works after boundary");
            epoch = session.Get().epoch;
        }
        Scene empty{};
        Require(session.Submit(epoch, empty) && session.Get().running, "all-empty scene stays available for editing");
        for (std::size_t slot = 0; slot < empty.size(); ++slot) {
            Require(!LightSession::ShouldIlluminate(session.Get(), slot), "empty scene removes every light");
        }
        auto thirdOnly = empty; thirdOnly[2] = scene[2];
        Require(session.Submit(epoch, thirdOnly) && LightSession::ShouldIlluminate(session.Get(), 2) &&
            !LightSession::ShouldIlluminate(session.Get(), 0), "Light 3 can operate alone without a Light 1 dependency");
    }

    void SpotlightGeometry()
    {
        const auto dot = [](LightVector a, LightVector b) { return a.x*b.x + a.y*b.y + a.z*b.z; };
        const auto close = [](float a, float b) { return std::abs(a - b) < 0.0001F; };
        const auto verify = [&](LightVector origin, LightVector target) {
            const auto aim = AimSpotlight(origin, target);
            Require(aim.has_value(), "valid origin and target yield aim basis");
            const auto direction = Unit({ target.x-origin.x, target.y-origin.y, target.z-origin.z });
            Require(close(dot(aim->forward, *direction), 1), "local positive X aims at target");
            Require(close(dot(aim->forward, aim->forward), 1) && close(dot(aim->up, aim->up), 1) && close(dot(aim->right, aim->right), 1), "basis has unit-length columns");
            Require(close(dot(aim->forward, aim->up), 0) && close(dot(aim->forward, aim->right), 0) && close(dot(aim->up, aim->right), 0), "basis columns are perpendicular");
            Require(close(dot(Cross(aim->forward, aim->up), aim->right), 1), "basis has positive determinant without mirroring");
        };
        const LightVector anchor{ 100, 200, 300 };
        for (const LightVector camera : { LightVector{ 100, -100, 450 }, LightVector{ 400, 200, 300 }, LightVector{ -50, 400, 600 } }) {
            const auto basis = MakeCameraBasis(anchor, camera);
            auto light = LightingEditor{}.CurrentLight();
            for (int direction = 0; direction < 8; ++direction) {
                light.direction = direction;
                for (float height : { -2.0F, 0.0F, 2.0F }) {
                    light.heightOffset = height;
                    for (float distance : { 0.25F, 2.2F, 8.0F }) {
                        light.distance = distance;
                        const auto position = LightPosition(anchor, *basis, light);
                        verify(*position, anchor);
                    }
                }
            }
        }
        verify({ 0, 0, 0 }, { 0, 0, 20 });
        verify({ 0, 0, 0 }, { 0, 0, -20 });
        verify({ 0, 0, 0 }, { 0.001F, 0, 20 });
        verify({ -50000, 40000, 200 }, { -49999, 40001, 199 });
        Require(!AimSpotlight(anchor, anchor), "coincident aim rejected");
        for (float bad : { std::numeric_limits<float>::infinity(), std::numeric_limits<float>::quiet_NaN() }) {
            Require(!AimSpotlight({ bad, 0, 0 }, anchor) && !AimSpotlight(anchor, { 0, bad, 0 }), "nonfinite origin or target rejected");
        }
        Require(!AimSpotlight({ -std::numeric_limits<float>::max(), 0, 0 }, { std::numeric_limits<float>::max(), 0, 0 }), "overflowing direction rejected");
    }

    void FinePositionEditing()
    {
        LightingEditor editor;
        const auto original = editor.GetScene();
        Require(!HasFinePosition(editor.CurrentLight().fine), "new light has zero offsets");
        editor.ResetFinePosition();
        Require(editor.History().size() == 1, "zero reset does not create history");
        for (int i = 1; i <= 100; ++i) {
            editor.SetFinePosition({ i * 0.005F, i * -0.004F, i * 0.003F });
            Require(editor.History().size() == 1, "live XYZ preview does not spam history");
        }
        editor.FinishNumericEdit();
        const auto edited = editor.GetScene();
        Require(editor.History().size() == 2, "XYZ drag creates exactly one history entry");
        auto withoutFine = edited;
        withoutFine[0].fine = {};
        Require(withoutFine == original, "pad preserves base position and all light properties");
        editor.Undo(); Require(editor.GetScene() == original, "pad undo restores full scene");
        editor.ResetFinePosition(); Require(editor.CanRedo(), "no-op reset preserves redo");
        editor.Redo(); Require(editor.GetScene() == edited, "pad redo restores all axes");
        editor.Adopt();
        editor.SetDirection(6);
        editor.SetNumeric(NumericField::Distance, 4.0F);
        editor.SetNumeric(NumericField::Height, 0.8F);
        editor.FinishNumericEdit();
        Require(editor.CurrentLight().fine == edited[0].fine, "base changes retain fine offsets");
        const auto newBase = editor.GetScene();
        editor.ResetFinePosition();
        auto expectedReset = newBase; expectedReset[0].fine = {};
        Require(editor.GetScene() == expectedReset, "reset affects fine position only");
        editor.Undo(); Require(editor.GetScene() == newBase, "reset is undoable");
        editor.RestoreBookmark(); Require(editor.GetScene() == edited, "bookmark includes offsets");

        for (int slot = 0; slot < 3; ++slot) {
            editor.SelectSlot(slot); editor.SetType(slot);
            const auto before = editor.GetScene();
            editor.SetNumeric(NumericField::FineHorizontal, (slot + 1) * 0.1F);
            editor.FinishNumericEdit();
            editor.SetNumeric(NumericField::FineVertical, -0.4F);
            editor.FinishNumericEdit();
            editor.SetNumeric(NumericField::FineDepth, 0.8F);
            editor.FinishNumericEdit();
            Require(editor.CurrentLight().fine == FinePosition{ (slot+1)*0.1F, -0.4F, 0.8F }, "sliders edit the same pad values");
            for (int other = 0; other < 3; ++other) {
                if (other != slot) { Require(editor.GetScene()[other] == before[other], "other slots are unchanged"); }
            }
            editor.ToggleLight(slot);
            editor.SetFinePosition({ -0.2F, 0.6F, -0.9F });
            editor.FinishNumericEdit();
            Require(!editor.CurrentLight().enabled && editor.CurrentLight().fine.horizontal == -0.2F, "muted editing does not light the source");
            editor.DeleteCurrent();
            const auto deleted = editor.CurrentLight();
            editor.SetFinePosition({ 1, 1, 1 }); editor.ResetFinePosition();
            Require(editor.CurrentLight() == deleted, "unplaced slot ignores pad and reset");
            editor.SetType(slot);
            Require(editor.CurrentLight().fine == deleted.fine, "replacing deleted source retains fine values");
        }
        editor.SetFinePosition({ -50, 60, 70 }); editor.FinishNumericEdit();
        Require(editor.CurrentLight().fine == FinePosition{ -1, 1, 1 }, "all axes clamp at one metre");
        const auto clamped = editor.GetScene();
        for (float bad : { std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::infinity() }) {
            for (int axis = 0; axis < 3; ++axis) {
                FinePosition fine{};
                if (axis == 0) fine.horizontal = bad;
                if (axis == 1) fine.vertical = bad;
                if (axis == 2) fine.depth = bad;
                editor.SetFinePosition(fine); editor.FinishNumericEdit();
                Require(editor.GetScene() == clamped, "nonfinite vector edit is atomic no-op");
            }
        }
        editor.SetNumeric(NumericField::Intensity, 1.7F);
        const auto beforePad = editor.History().size();
        editor.SetFinePosition({ 0.1F, 0.2F, 0.3F });
        Require(editor.History().size() == beforePad + 1, "pad finishes preceding strength edit");
        editor.SelectSlot(0);
        Require(editor.History().size() == beforePad + 2, "selection finishes pad edit on original slot");
        editor.Undo();
        Require(editor.SelectedSlot() == 2 && editor.CurrentLight().fine == clamped[2].fine, "pending-pad history belongs to original slot");
        for (const auto& preset : kScenePresets) {
            Require(editor.ApplyScene(preset.scene, preset.name), "built-in scene applies");
            for (std::size_t slot = 0; slot < preset.scene.size(); ++slot) {
                Require(editor.GetScene()[slot].fine == preset.scene[slot].fine,
                    "built-in scenes apply their authored fine offsets exactly");
            }
        }
        for (int slot = 0; slot < 3; ++slot) {
            for (int axis = 0; axis < 3; ++axis) {
                for (float bad : { -1.01F, 1.01F, std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::infinity() }) {
                    auto invalid = original;
                    if (axis == 0) invalid[slot].fine.horizontal = bad;
                    if (axis == 1) invalid[slot].fine.vertical = bad;
                    if (axis == 2) invalid[slot].fine.depth = bad;
                    Require(!LightingEditor::IsValidScene(invalid), "invalid fine axis rejects entire scene, including inactive slots");
                    const auto before = editor.GetScene();
                    Require(!editor.ApplyScene(invalid, "bad") && editor.GetScene() == before, "invalid scene preserves editor");
                    LightSession session; session.SetReady(true);
                    Require(!session.Start(session.Get().epoch, invalid), "runtime start validates offsets");
                    Require(session.Start(session.Get().epoch, original), "valid scene starts");
                    Require(!session.Submit(session.Get().epoch, invalid) && session.Get().lights == original, "runtime submit rejects invalid offsets atomically");
                }
            }
        }
    }

    void ResetCurrentLightValues()
    {
        LightingEditor editor;
        editor.SetDirection(6);
        const LightTint tint{ 0.2F, 0.4F, 0.8F };
        editor.ApplyLightRecipe(3, tint);
        editor.SetShadowMode(false, ShadowProjection::Omni);
        editor.SetShadowBias(4.0F);
        editor.ToggleLight(0);
        editor.SetNumeric(NumericField::Intensity, 2.35F);
        editor.SetNumeric(NumericField::Range, 9.5F);
        editor.SetNumeric(NumericField::Distance, 5.75F);
        editor.SetNumeric(NumericField::Height, -1.25F);
        editor.SetFinePosition({ 0.35F, -0.45F, 0.55F });
        editor.SetFaceIntensity(1.4F);
        editor.FinishNumericEdit();

        const auto before = editor.GetScene();
        const auto historyBefore = editor.History().size();
        Require(editor.CanResetCurrentValues(), "changed placed light enables value reset");
        editor.ResetCurrentValues();

        auto expected = before;
        expected[0].intensity = kLightIntensityDefault;
        expected[0].range = kLightRangeDefault;
        expected[0].distance = kLightDistanceDefault;
        expected[0].heightOffset = kLightHeightDefault;
        expected[0].fine = {};
        expected[0].shadowBias = kShadowBiasDefault;
        Require(editor.GetScene() == expected, "value reset changes only documented adjustment fields");
        Require(editor.CurrentLight().direction == 6 && editor.CurrentLight().type == 3 &&
            editor.CurrentLight().customTint == tint && !editor.CurrentLight().castsShadow &&
            editor.CurrentLight().shadowProjection == ShadowProjection::Omni && !editor.CurrentLight().enabled,
            "value reset preserves direction, recipe, tint, shadow mode and power");
        Require(editor.GetScene().face == before.face, "value reset preserves face light settings");
        Require(editor.History().size() == historyBefore + 1 &&
            editor.History().back().label == "Light 1 : 調整を初期値へ",
            "value reset creates one named history entry");
        Require(!editor.CanResetCurrentValues(), "default adjustment values disable reset");

        editor.Undo();
        Require(editor.GetScene() == before, "value reset undo restores all adjusted values");
        editor.Redo();
        Require(editor.GetScene() == expected, "value reset redo reapplies defaults");
        const auto noOpHistory = editor.History().size();
        editor.ResetCurrentValues();
        Require(editor.GetScene() == expected && editor.History().size() == noOpHistory,
            "reset at defaults is a history-preserving no-op");

        editor.DeleteCurrent();
        const auto deleted = editor.GetScene();
        const auto deletedHistory = editor.History().size();
        Require(!editor.CanResetCurrentValues(), "unplaced slot disables value reset");
        editor.ResetCurrentValues();
        Require(editor.GetScene() == deleted && editor.History().size() == deletedHistory,
            "unplaced slot ignores value reset");
    }

    void FinePositionGeometry()
    {
        const auto close = [](float a, float b) { return std::abs(a-b) < 0.0002F; };
        const LightVector anchor{ 100, 200, 300 };
        for (const LightVector camera : { LightVector{ 100, -100, 450 }, LightVector{ 400, 200, 300 }, LightVector{ -50, 400, 600 } }) {
            const auto basis = MakeCameraBasis(anchor, camera);
            for (int direction = 0; direction < 8; ++direction) {
                auto light = LightingEditor{}.CurrentLight(); light.direction = direction;
                const auto base = LightPosition(anchor, *basis, light);
                for (float x : { -1.0F, 0.0F, 1.0F }) {
                    for (float y : { -1.0F, 0.0F, 1.0F }) {
                        for (float z : { -1.0F, 0.0F, 1.0F }) {
                            light.fine = { x, z, y };
                            const auto moved = LightPosition(anchor, *basis, light);
                            Require(moved.has_value(), "offset position is valid");
                            Require(close(moved->x, base->x + kUnitsPerMetre*(x*basis->right.x+y*basis->front.x)) &&
                                close(moved->y, base->y + kUnitsPerMetre*(x*basis->right.y+y*basis->front.y)) &&
                                close(moved->z, base->z + kUnitsPerMetre*z), "offset is camera-relative translation, not orbit");
                            const auto aim = AimPlacedSpotlight(*moved, anchor, *basis, light);
                            const auto toward = Unit({ anchor.x-moved->x, anchor.y-moved->y, anchor.z-moved->z });
                            Require(aim && toward && close(aim->forward.x, toward->x) && close(aim->forward.y, toward->y) && close(aim->forward.z, toward->z), "spot aims from displaced source at anchor");
                            auto original = light; original.fine = {};
                            Require(LightRegistrationSettings::From(light) == LightRegistrationSettings::From(original), "offset requires no shadow registration rebuild");
                        }
                    }
                }
            }
            auto coincident = LightingEditor{}.CurrentLight();
            coincident.direction = 0; coincident.distance = 0.25F; coincident.fine.depth = 0.25F;
            const auto position = LightPosition(anchor, *basis, coincident);
            Require(AimPlacedSpotlight(*position, anchor, *basis, coincident).has_value(), "exact target crossing has a deterministic base aim");
        }
        for (float width : { 160.0F, 320.0F, 600.0F, 900.0F }) {
            for (float height : { 180.0F, 220.0F, 260.0F }) {
                const float scale = PadPixelsPerMetre(width, height);
                const auto moved = MoveOnPad({}, scale*0.5F, -scale*0.5F, 0, scale);
                Require(close(moved.horizontal, 0.5F) && close(moved.vertical, 0.5F), "equal pixels produce equal metres on every aspect ratio");
                Require(close(moved.horizontal*scale, moved.vertical*scale), "drawn dot has equal visual axis scale");
                Require(scale*kFinePositionLimit + 9 <= std::min(width, height)*0.5F, "full-range marker remains inside pad");
                const auto reversed = MoveOnPad(moved, -scale*0.5F, scale*0.5F, 0, scale);
                Require(reversed == FinePosition{}, "opposite drag returns to zero");
                Require(MoveOnPad({}, scale*10, -scale*10, -100, scale) == FinePosition{ 1,1,1 }, "overshoot clamps at bounds");
                const auto limit = MoveOnPad({ 1,1,1 }, -scale*0.1F, scale*0.1F, 2, scale);
                Require(close(limit.horizontal, 0.9F) && close(limit.vertical, 0.9F) && close(limit.depth, 0.9F), "reversing at boundary responds immediately");
            }
        }
        const FinePosition fine{ 0.2F, -0.4F, 0.6F };
        Require(MoveOnPad(fine, 0, 0, 0, 100) == fine, "click alone never jumps the position");
        const auto wheeled = MoveOnPad(fine, 0, 0, 1, 100);
        Require(wheeled.horizontal == fine.horizontal && wheeled.vertical == fine.vertical && close(wheeled.depth, 0.55F), "wheel only changes front/back by 5cm");
        Require(MoveOnPad(fine, 1, 1, 1, 0) == fine, "invalid scale ignored");
        Require(MoveOnPad(fine, std::numeric_limits<float>::quiet_NaN(), 0, 0, 100) == fine, "invalid mouse delta ignored");
        Require(std::isfinite(PadPixelsPerMetre(0, 0)), "collapsed pad scale remains finite");
        const auto basis = *MakeCameraBasis(anchor, { 100, -100, 450 });
        const auto light = LightingEditor{}.CurrentLight();
        Require(!AimPlacedSpotlight({ std::numeric_limits<float>::infinity(), 0, 0 }, anchor, basis, light), "aim fallback never hides invalid input");
    }

    void PositionWindowSizing()
    {
        // Font-aware widths/heights are measured by the UI; exercise common and
        // oversized measurements against wide, small and offset work areas.
        for (const WindowRectangle work : { WindowRectangle{0, 0, 2560, 1440}, {0, 0, 1920, 1080},
                 {0, 0, 1280, 720}, {100, 50, 800, 600}, {-1920, 0, 1920, 1080}, {0, 0, 200, 150} }) {
            for (float required : {300.0F, 420.0F, 530.0F, 730.0F, 2200.0F}) {
                for (float desired : {480.0F, 700.0F, 1050.0F}) {
                    for (float hostX : {work.x, work.x + work.width * 0.65F, work.x + work.width + 200}) {
                        const auto layout = CalculatePositionWindowLayout(work, {hostX, work.y + 30, 600, 1000}, required, desired);
                        const auto rect = layout.initial;
                        Require(rect.x >= work.x && rect.y >= work.y && rect.x + rect.width <= work.x + work.width + 0.001F &&
                            rect.y + rect.height <= work.y + work.height + 0.001F, "initial position window stays inside work area");
                        Require(rect.width >= layout.minimumWidth && rect.width <= layout.maximumWidth &&
                            rect.height >= layout.minimumHeight && rect.height <= layout.maximumHeight, "initial dimensions respect resize constraints");
                        Require(layout.minimumWidth >= std::min(required, layout.maximumWidth), "measured controls fit unless wider than viewport");
                        Require(rect.height >= std::min(desired, layout.maximumHeight), "normal initial window fits content without scrolling");
                    }
                }
            }
        }
        const WindowRectangle work{0,0,2560,1440};
        const auto right = CalculatePositionWindowLayout(work, {500,50,600,1000}, 420, 600);
        Require(right.initial.x >= 1100, "free right side is preferred");
        const auto left = CalculatePositionWindowLayout(work, {1800,50,700,1000}, 420, 600);
        Require(left.initial.x + left.initial.width <= 1800, "right-side host places initial position window on left");
    }

    void CompactPositionControls()
    {
        for (float font : {16.0F, 24.0F, 32.0F, 48.0F, 64.0F}) {
            for (float spacing : {4.0F, 8.0F, 16.0F}) {
                const float groupGap = LightSlotGroupGap(font, spacing);
                Require(groupGap > spacing && groupGap > 3.0F, "slot groups have wider gaps than number-power pairs");
                const float pairWidth = font + 6.0F + std::max(22.0F, (font + 6.0F) * 0.65F) + 3.0F;
                const float titleWidth = font * 3.0F;
                const float row = LightSlotRowWidth(titleWidth, pairWidth, spacing, groupGap);
                const float lastRight = titleWidth + spacing + 2.0F * (pairWidth + groupGap) + pairWidth;
                Require(std::abs(row - lastRight) < 0.001F, "slot row measurement matches drawn group spacing");
                for (float canvas : {310.0F, 390.0F, 430.0F}) {
                    const float frame = font + 6.0F;
                    const auto footer = CalculateDiagramFooter(canvas, frame);
                    Require(footer.cameraY - 7.0F > canvas * 0.933F, "camera stays below bottom direction node");
                    Require(footer.height >= footer.cameraY + 7.0F, "camera fits reserved row; fine button follows below");
                    Require(footer.cameraX == canvas * .5F, "camera stays centered independently of favorites");
                }
                const float helpGap = spacing + font * .6F;
                for (float first : {font * 3.0F, font * 8.0F}) {
                    for (float second : {font * 7.0F, font * 16.0F}) {
                        const float exact = first + helpGap + second;
                        Require(FitsInline(exact, first, second, helpGap), "preset section heading and guidance fit inline at exact width");
                        Require(!FitsInline(exact - 1, first, second, helpGap), "preset guidance is omitted before overflowing its section heading row");
                    }
                }
                const float resetWidth = font * 5.0F + 8.0F;
                const float historyWidth = font * 4.0F + 16.0F + spacing;
                const float exactFit = resetWidth + historyWidth + spacing;
                for (float width : {200.0F, 360.0F, 420.0F, 800.0F, exactFit, exactFit - 1.0F}) {
                    const auto footer = CalculateFinePositionFooter(width, resetWidth, historyWidth, spacing);
                    Require(footer.sameRow == (width >= exactFit), "footer wraps only when one-row controls do not fit");
                    Require(footer.historyX >= 0.0F, "history offset never becomes negative");
                    Require(width < historyWidth || std::abs(footer.historyX + historyWidth - width) < 0.001F,
                        "history buttons align to content right edge");
                    Require(!footer.sameRow || footer.historyX + 0.001F >= resetWidth + spacing,
                        "reset and shared history retain a gap on same row");
                }
            }
        }
        const auto compact = CalculatePositionWindowLayout({0,0,1920,1080}, {1300,50,600,900}, 350, 460);
        Require(compact.initial.width == 360 && compact.initial.height == 460,
            "compact editor no longer has old 420px width floor or old height");

        LightingEditor editor;
        editor.SetFinePosition({0.3F, 0.4F, -0.2F}); editor.FinishNumericEdit();
        const auto beforeBasic = editor.GetScene();
        editor.SetNumeric(NumericField::Distance, 4.0F); editor.FinishNumericEdit();
        editor.SetNumeric(NumericField::Height, 0.75F); editor.FinishNumericEdit();
        Require(editor.CurrentLight().fine == beforeBasic[0].fine, "main distance and height preserve window offsets");
        const auto beforeReset = editor.GetScene();
        editor.ResetFinePosition();
        auto expected = beforeReset; expected[0].fine = {};
        Require(editor.GetScene() == expected, "renamed reset still clears only fine offsets");
        editor.Undo();
        Require(editor.GetScene() == beforeReset, "window undo restores fine reset without changing basic settings");
        editor.Undo();
        Require(editor.CurrentLight().heightOffset == beforeBasic[0].heightOffset && editor.CurrentLight().distance == 4.0F,
            "shared undo from window can revert main height");
        editor.Undo();
        Require(editor.GetScene() == beforeBasic, "next shared undo restores main distance and retains offsets");
    }

    void SharedPositionEditor()
    {
        // Both UI surfaces use this same editor, not independent drafts. These
        // tests exercise state transitions, not ImGui hit testing or rendering.
        LightingEditor editor;
        editor.SelectSlot(1); editor.SetType(1); editor.SelectSlot(0);
        const auto original = editor.GetScene();
        const auto startCount = editor.History().size();
        for (int frame = 1; frame <= 30; ++frame) {
            editor.SetFinePosition({frame * 0.02F, frame * -0.01F, 0.1F});
            Require(editor.History().size() == startCount, "position drag remains pending while other surface is idle");
            Require(editor.GetScene()[1] == original[1], "position drag preserves other light");
        }
        editor.FinishNumericEdit(); // Aggregate end of gesture after both views.
        Require(editor.History().size() == startCount + 1, "one drag creates one shared history entry");
        const auto positioned = editor.GetScene();
        editor.SetNumeric(NumericField::Intensity, 2.0F); // Main pane.
        editor.FinishNumericEdit();
        editor.Undo(); // Position pane's global history, not position-only history.
        Require(editor.GetScene() == positioned, "position window undo also reverts main strength change");
        editor.SelectSlot(1);
        Require(editor.CanRedo() && editor.CurrentLight() == original[1], "selection from second surface preserves redo and values");
        editor.Redo();
        Require(editor.SelectedSlot() == 0 && editor.CurrentLight().intensity == 2.0F, "shared redo restores target and strength");
        editor.ToggleLight(0);
        editor.SetFinePosition({-0.25F, 0.4F, -0.1F});
        editor.SelectSlot(1); // Commits the old slot before changing edit target.
        Require(!editor.HasPendingChanges() && !editor.GetScene()[0].enabled &&
            editor.GetScene()[0].fine == FinePosition{-0.25F, 0.4F, -0.1F}, "switch commits muted light without powering it on");
        editor.SetNumeric(NumericField::Distance, 3.4F);
        editor.FinishNumericEdit();
        editor.ToggleLight(0);
        Require(editor.SelectedSlot() == 1 && editor.CurrentLight().distance == 3.4F, "power buttons do not change editing target");
        editor.SelectSlot(0);
        const auto beforeReset = editor.GetScene();
        auto expectedReset = beforeReset; expectedReset[0].fine = {};
        editor.ResetFinePosition();
        Require(editor.GetScene() == expectedReset, "position reset preserves all non-offset settings across windows");
        editor.Undo();
        Require(editor.GetScene() == beforeReset, "shared history undoes position reset exactly");
    }

    void BlueAndWideControls()
    {
        for (float width : { 220.0F, 310.0F, 390.0F, 600.0F }) {
            for (float font : { 16.0F, 24.0F, 32.0F }) {
                const float basicSuffix = font * 8.0F + 8.0F;
                const float rangeSuffix = font * 6.0F + 20.0F;
                const auto layout = CalculateRangeControl(width, basicSuffix, rangeSuffix, font * 3.0F);
                Require(layout.sameRow == (width >= std::max(basicSuffix, rangeSuffix) + font * 3.0F), "wide action wraps if range row has no room for usable slider");
                Require(layout.sliderWidth >= 1.0F, "range width remains positive in narrow panes");
                if (layout.sameRow) { Require(layout.sliderWidth + rangeSuffix <= width, "wide button and range label fit in shared row"); }
                else { Require(layout.sliderWidth == std::max(1.0F, width - basicSuffix), "wrapped wide action retains normal basic slider width"); }
            }
        }
        static_assert(kLightTypes.size() == kRuntimeTints.size());
        Require(RuntimeTint(3)->blue > RuntimeTint(3)->green && RuntimeTint(3)->green > RuntimeTint(3)->red,
            "studio blue has an actual blue-dominant renderer tint");
        Require(kRuntimeTints[0] == LightTint{ 1, 1, 1 } && kRuntimeTints[1] == LightTint{ 1, .8F, .53F } &&
            kRuntimeTints[2] == LightTint{ 1, .6F, .29F }, "all original tints stay byte-for-byte values");
        for (int slot = 0; slot < 3; ++slot) {
            for (int type = 0; type < kLightTypeCount; ++type) {
                LightingEditor editor;
                editor.ApplyScene(kScenePresets[2].scene, "three");
                editor.SelectSlot(slot);
                editor.SetType(type);
                editor.SetFinePosition({ .2F, -.4F, .3F });
                editor.ToggleLight(slot);
                editor.ToggleFaceLight();
                auto expected = editor.GetScene();
                const auto prior = expected;
                editor.SetWideRange();
                expected[slot].range = kWideLightRange;
                Require(editor.GetScene() == expected && editor.SelectedSlot() == slot, "wide changes ONLY range in any colour/slot, retaining mute and face");
                Require(editor.History().back().label.find("広域") != std::string::npos, "wide gets one named history entry");
                editor.Undo();
                Require(editor.GetScene() == prior && editor.CanRedo(), "wide undo restores the exact prior range and all state");
                editor.Redo();
                const auto count = editor.History().size();
                editor.SetWideRange();
                Require(editor.History().size() == count, "repeating wide has no history entry");
                editor.SetNumeric(NumericField::Range, 6.5F);
                editor.FinishNumericEdit();
                Require(editor.CurrentLight().range == 6.5F, "wide is not a locked mode; slider immediately adjusts it");
                expected = editor.GetScene();
                expected[slot].type = 3;
                editor.SetType(3);
                Require(editor.GetScene() == expected, "blue changes only colour and keeps OFF, fine, range and face");
            }
        }
        LightingEditor empty;
        empty.SelectSlot(2);
        const auto before = empty.GetScene();
        empty.SetWideRange();
        Require(empty.GetScene() == before && empty.History().size() == 1, "wide on an unplaced slot is a no-op");
        empty.SetType(3);
        Require(empty.CurrentLight().placed && empty.CurrentLight().enabled && empty.CurrentLight().type == 3,
            "blue can place a previously empty slot");
    }

    void AdjustableFaceRange()
    {
        LightingEditor editor;
        editor.SelectSlot(2); // Range must be usable even if the manual slot is empty.
        const auto initial = editor.GetScene();
        Require(initial.face.range == 1.2F && !initial.face.enabled, "face range preserves old default and initial OFF");
        editor.SetFaceRange(.7F);
        Require(editor.GetScene().lights == initial.lights && editor.SelectedSlot() == 2 &&
            !editor.GetScene().face.enabled && editor.GetScene().face.intensity == initial.face.intensity,
            "face range edit while OFF preserves manual slots, selection, power and strength");
        Require(editor.HasPendingChanges() && editor.History().size() == 1, "face range preview waits for gesture end");
        editor.FinishNumericEdit();
        Require(editor.History().size() == 2 && editor.History().back().label == "フェイスライト : 光の範囲", "face range has its own history label");
        editor.Undo();
        Require(editor.GetScene() == initial, "range undo restores full prior scene");
        editor.SetFaceRange(kFaceRangeDefault);
        editor.FinishNumericEdit();
        Require(editor.CanRedo(), "no-op face range preserves redo");
        editor.Redo();
        Require(editor.GetScene().face.range == .7F && editor.SelectedSlot() == 2, "range redo restores value and recorded selection");
        const auto count = editor.History().size();
        for (int n = 0; n < 100; ++n) { editor.SetFaceRange(.8F + n * .01F); }
        Require(editor.History().size() == count, "face range drag coalesces live updates");
        editor.FinishNumericEdit();
        Require(editor.History().size() == count + 1, "face range drag commits exactly once");
        editor.Undo();
        Require(editor.GetScene().face.range == .7F, "single undo reverts entire range drag");
        editor.Redo();
        editor.Adopt();
        const auto adopted = editor.GetScene();
        editor.SetFaceRange(.6F);
        editor.SetFaceIntensity(.8F); // Commits preceding range gesture.
        Require(editor.History().back().label == "フェイスライト : 光の範囲", "strength activation ends range gesture");
        editor.SetFaceRange(.9F);
        Require(editor.History().back().label == "フェイスライト : 強さ", "range activation ends strength gesture");
        editor.FinishNumericEdit();
        editor.Undo(); editor.Undo(); editor.Undo();
        Require(editor.GetScene() == adopted, "interleaved face sliders undo as three independent gestures");
        editor.Redo(); editor.Redo(); editor.Redo();
        const auto beforeRestore = editor.GetScene();
        editor.RestoreBookmark();
        Require(editor.GetScene() == adopted, "bookmark includes non-default face range");
        editor.Undo();
        Require(editor.GetScene() == beforeRestore, "bookmark restoration remains undoable");
        editor.SelectSlot(0);
        const auto beforeManual = editor.GetScene();
        editor.SetNumeric(NumericField::Range, 7.0F);
        editor.SetFaceRange(.8F);
        Require(editor.History().back().label == "Light 1 : 光の範囲", "face range does not merge with manual range");
        editor.SetNumeric(NumericField::Height, .5F);
        Require(editor.History().back().label == "フェイスライト : 光の範囲", "manual edit finishes face range under correct label");
        editor.FinishNumericEdit();
        editor.Undo(); editor.Undo(); editor.Undo();
        Require(editor.GetScene() == beforeManual, "mixed manual and face range undo restores exact scene");
        const auto beforeBad = editor.GetScene();
        for (float bad : { std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::infinity(), -std::numeric_limits<float>::infinity() }) {
            editor.SetFaceRange(bad);
            Require(editor.GetScene() == beforeBad && editor.CanRedo(), "nonfinite face range does not alter scene or redo");
        }
        editor.SetFaceRange(-10.0F); editor.FinishNumericEdit();
        Require(editor.GetScene().face.range == .5F, "face range lower clamp");
        editor.SetFaceRange(100.0F); editor.FinishNumericEdit();
        Require(editor.GetScene().face.range == 2.0F, "face range upper clamp");
        editor.SetFaceRange(.75F); editor.FinishNumericEdit();
        editor.ToggleFaceLight();
        const auto face = editor.GetScene().face;
        PresetPicker picker;
        for (std::size_t index = 0; index < kScenePresets.size(); ++index) {
            const auto expectedFace = kScenePresets[index].includeFace ? kScenePresets[index].scene.face : editor.GetScene().face;
            picker.Open(); picker.Choose(index, editor);
            Require(editor.GetScene().face == expectedFace, "sample face scope controls non-default face range");
            const auto appliedFace = editor.GetScene().face;
            for (int slot = 0; slot < 3; ++slot) {
                editor.SelectSlot(slot); editor.SetType(3); editor.SetWideRange();
                editor.SetFinePosition({ .2F, .3F, .4F }); editor.ResetFinePosition();
                editor.ToggleLight(slot); editor.DeleteCurrent();
                Require(editor.GetScene().face == appliedFace, "blue wide fine reset power and deletion never affect face range");
            }
        }
        for (float range : { .5F, .75F, 1.2F, 2.0F }) {
            for (int mask = 0; mask < 8; ++mask) {
                for (bool enabled : { false, true }) {
                    Scene scene = kScenePresets[2].scene;
                    for (int slot = 0; slot < 3; ++slot) { scene[slot].enabled = (mask & (1 << slot)) != 0; }
                    scene.face = { enabled, .84F, range };
                    LightSession session; session.SetReady(true);
                    Require(session.Start(session.Get().epoch, scene), "each allowed face range starts with all manual power masks");
                    Require(LightSession::ShouldIlluminateFace(session.Get()) == enabled, "face range preserves independent illumination gate");
                    const auto settings = *RuntimeSettings(scene, kFaceRuntimeSlot);
                    auto defaultScene = scene; defaultScene.face.range = kFaceRangeDefault;
                    const auto defaults = *RuntimeSettings(defaultScene, kFaceRuntimeSlot);
                    auto expected = defaults; expected.range = range;
                    Require(settings == expected, "only face runtime radius changes, not intensity colour or placement data");
                    Require(LightRegistrationSettings::From(settings) == LightRegistrationSettings::From(defaults), "face range needs no registration replacement");
                    for (int slot = 0; slot < 3; ++slot) {
                        Require(*RuntimeSettings(scene, slot) == scene[slot], "face radius leaves three manual runtime settings untouched");
                    }
                    auto update = scene; update.face.range = range == .5F ? 2.0F : .5F;
                    const auto epoch = session.Get().epoch;
                    Require(session.Submit(epoch, update) && session.Get().lights == update, "running face radius update passes full scene");
                    session.Stop(epoch);
                    Require(!LightSession::ShouldIlluminateFace(session.Get()) && !session.Submit(epoch, scene), "range update cannot bypass stop token");
                    session.Start(session.Get().epoch, update); session.Block(true); session.Block(false);
                    Require(!LightSession::ShouldIlluminateFace(session.Get()) && session.Get().lights.face.range == update.face.range,
                        "loading gate keeps edited range but never relights automatically");
                }
            }
        }
        for (bool enabled : { false, true }) {
            for (float range : { 0.0F, .49F, 2.01F, std::numeric_limits<float>::quiet_NaN(),
                    std::numeric_limits<float>::infinity(), -std::numeric_limits<float>::infinity() }) {
                LightingEditor clean;
                const auto prior = clean.GetScene();
                auto bad = prior; bad.face = { enabled, .35F, range }; bad[1].placed = true;
                Require(!clean.ApplyScene(bad, "invalid face range") && clean.GetScene() == prior, "invalid face range rejects entire editor scene, even OFF");
                LightSession session; session.SetReady(true);
                Require(!session.Start(session.Get().epoch, bad), "invalid face range rejects start");
                session.Start(session.Get().epoch, prior);
                Require(!session.Submit(session.Get().epoch, bad) && session.Get().lights == prior, "invalid face range rejects whole running snapshot atomically");
            }
        }
        Scene faceOnly; faceOnly.face = { true, .35F, .5F };
        LightSession session; session.SetReady(true);
        Require(session.Start(session.Get().epoch, faceOnly) && LightSession::ShouldIlluminateFace(session.Get()), "minimum-radius face works with no manual lights placed");
        for (int slot = 0; slot < 3; ++slot) { Require(!LightSession::ShouldIlluminate(session.Get(), slot), "face-only does not place manual lights"); }
    }

    void IndependentFaceEditing()
    {
        LightingEditor editor;
        Require(!editor.GetScene().face.enabled && editor.GetScene().face.intensity == .35F, "face starts OFF with weak default");
        Require(editor.GetScene().size() == 3, "face never becomes a fourth direction/selection slot");
        editor.SelectSlot(2); // unplaced; face must still be editable
        const auto original = editor.GetScene();
        editor.ToggleFaceLight();
        Require(editor.GetScene().lights == original.lights && editor.SelectedSlot() == 2 && editor.GetScene().face.enabled,
            "face ON neither places nor selects nor disables Light 3");
        editor.Undo();
        Require(editor.GetScene() == original && editor.SelectedSlot() == 0, "face undo restores the preceding snapshot and its recorded selection, like other edits");
        editor.Redo();
        Require(editor.GetScene().face.enabled && editor.SelectedSlot() == 2, "face redo restores its snapshot and recorded selection");
        editor.Adopt();
        const auto adopted = editor.GetScene();
        const auto count = editor.History().size();
        for (int n = 0; n < 100; ++n) { editor.SetFaceIntensity(.4F + n * .004F); }
        Require(editor.History().size() == count && editor.HasPendingChanges(), "face strength preview coalesces without manual placement");
        editor.FinishNumericEdit();
        Require(editor.History().size() == count + 1, "one face drag is one history entry");
        editor.ToggleFaceLight();
        const auto off = editor.GetScene();
        editor.SetFaceIntensity(.2F);
        editor.FinishNumericEdit();
        Require(!editor.GetScene().face.enabled && editor.GetScene().lights == off.lights, "face editing while OFF does not relight or touch normal lights");
        editor.RestoreBookmark();
        Require(editor.GetScene() == adopted, "bookmark restores face and three lights together");
        editor.Undo();
        Require(!editor.GetScene().face.enabled && editor.GetScene().face.intensity == .2F, "bookmark restore itself is undoable for face");
        const auto valid = editor.GetScene();
        const auto validCount = editor.History().size();
        for (float bad : { std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::infinity(), -std::numeric_limits<float>::infinity() }) {
            editor.SetFaceIntensity(bad);
            Require(editor.GetScene() == valid && editor.History().size() == validCount, "nonfinite face edits do not change state/history");
        }
        editor.SetFaceIntensity(-2.0F); editor.FinishNumericEdit();
        Require(editor.GetScene().face.intensity == 0, "face lower clamp");
        editor.SetFaceIntensity(100.0F); editor.FinishNumericEdit();
        Require(editor.GetScene().face.intensity == kFaceIntensityMax, "face upper clamp");
        editor.SelectSlot(0);
        const auto beforeMixed = editor.GetScene();
        editor.SetNumeric(NumericField::Intensity, 2.1F);
        editor.SetFaceIntensity(.7F);  // commits the manual gesture first
        Require(editor.History().back().label == "Light 1 : 強さ", "face edit commits pending manual slider under original label");
        editor.SetNumeric(NumericField::Height, .6F); // commits face first
        Require(editor.History().back().label == "フェイスライト : 強さ", "manual edit commits pending face slider under face label");
        editor.FinishNumericEdit();
        editor.Undo(); editor.Undo(); editor.Undo();
        Require(editor.GetScene() == beforeMixed, "interleaved face/manual edits undo without merging or loss");
        editor.Redo(); editor.Redo(); editor.Redo();
        Require(editor.GetScene().face.intensity == .7F && editor.CurrentLight().heightOffset == .6F, "mixed redo restores both kinds");
        const auto face = editor.GetScene().face;
        for (int slot = 0; slot < 3; ++slot) {
            editor.SelectSlot(slot);
            editor.SetType(3);
            editor.SetWideRange();
            editor.SetFinePosition({ .2F, .2F, .2F });
            editor.ResetFinePosition();
            editor.ToggleLight(slot);
            editor.DeleteCurrent();
            Require(editor.GetScene().face == face, "placement/type/range/fine/reset/mute/delete never affect face");
        }
        PresetPicker picker;
        for (std::size_t index = 0; index < kScenePresets.size(); ++index) {
            const auto beforePreset = editor.GetScene();
            const auto expectedFace = kScenePresets[index].includeFace ? kScenePresets[index].scene.face : beforePreset.face;
            picker.Open();
            Require(picker.Choose(index, editor), "preset can be selected with independent face");
            Require(editor.GetScene().face == expectedFace && editor.GetScene().lights == kScenePresets[index].scene.lights,
                "UI preset applies its declared manual/face scope");
            editor.Undo();
            Require(editor.GetScene() == beforePreset, "preset undo preserves the full prior setup");
            editor.Redo();
        }
    }

    void FaceRuntimeAndGeometry()
    {
        for (int mask = 0; mask < 8; ++mask) {
            Scene scene = kScenePresets[2].scene;
            for (int slot = 0; slot < 3; ++slot) { scene[slot].enabled = (mask & (1 << slot)) != 0; }
            for (bool faceOn : { false, true }) {
                for (float strength : { 0.0F, .35F, 1.0F, 2.42F, 3.0F }) {
                    scene.face = { faceOn, strength };
                    LightSession session; session.SetReady(true);
                    Require(session.Start(session.Get().epoch, scene), "3+face scene starts");
                    const auto epoch = session.Get().epoch;
                    Require(LightSession::ShouldIlluminateFace(session.Get()) == (faceOn && strength > 0), "face illumination independent of all eight normal power masks");
                    for (int slot = 0; slot < 3; ++slot) {
                        Require(LightSession::ShouldIlluminateRuntime(session.Get(), slot) == ((mask & (1 << slot)) != 0), "face never disables a manual light");
                        Require(RuntimeSettings(scene, slot) == std::optional(scene[slot]), "manual renderer settings unchanged");
                    }
                    const auto face = RuntimeSettings(scene, kFaceRuntimeSlot);
                    Require(face && !face->castsShadow && face->type == 0 && face->range == kFaceRangeDefault && face->intensity == strength,
                        "face is white shadowless short-range with only independent strength");
                    Require(!LightRegistrationSettings::From(*face).shadow && !LightRegistrationSettings::From(*face).spot, "face never consumes a shadow registration");
                    Require(!RuntimeSettings(scene, kRuntimeLightCount) && !RuntimeSettings(scene, std::numeric_limits<std::size_t>::max()), "invalid runtime slot is guarded");
                    Require(!LightSession::ShouldIlluminateRuntime(session.Get(), kRuntimeLightCount), "invalid runtime slot cannot illuminate");
                    session.Stop(epoch);
                    for (std::size_t slot = 0; slot < kRuntimeLightCount; ++slot) { Require(!LightSession::ShouldIlluminateRuntime(session.Get(), slot), "stop gates all four lights"); }
                    Require(!session.Submit(epoch, scene) && !session.Start(epoch, scene), "stale request cannot resurrect face or normal lights");
                    Require(session.Get().lights == scene, "stop preserves settings for explicit restart");
                    session.Start(session.Get().epoch, scene);
                    session.Block(true);
                    for (std::size_t slot = 0; slot < kRuntimeLightCount; ++slot) { Require(!LightSession::ShouldIlluminateRuntime(session.Get(), slot), "loading/main menu gates all four"); }
                    session.Block(false);
                    Require(!LightSession::ShouldIlluminateFace(session.Get()), "unblocking never restarts face automatically");
                    session.Start(session.Get().epoch, scene);
                    session.SetReady(false);
                    Require(!LightSession::ShouldIlluminateFace(session.Get()), "preload stops face");
                    session.SetReady(true);
                    Require(!LightSession::ShouldIlluminateFace(session.Get()), "postload/new game leaves face stopped");
                }
            }
        }
        Scene empty;
        empty.face.enabled = true;
        LightSession session; session.SetReady(true);
        Require(session.Start(session.Get().epoch, empty) && LightSession::ShouldIlluminateFace(session.Get()), "face works even with no manual lights placed");
        LightingEditor editor;
        const auto original = editor.GetScene();
        for (float invalid : { -.01F, 3.01F, std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::infinity() }) {
            auto bad = empty;
            bad.face = { false, invalid }; // even OFF must validate
            bad[0].intensity = 2.4F;
            Require(!editor.ApplyScene(bad, "invalid face") && editor.GetScene() == original, "invalid face rejects whole editor scene atomically");
            Require(!session.Submit(session.Get().epoch, bad) && session.Get().lights == empty, "invalid OFF face rejects runtime snapshot atomically");
            LightSession fresh; fresh.SetReady(true);
            Require(!fresh.Start(fresh.Get().epoch, bad), "invalid face cannot start");
        }
        for (LightVector head : { LightVector{}, LightVector{ 120, -350, 180 }, LightVector{ -1000, 2500, -90 } }) {
            for (LightVector direction : { LightVector{ 1, 0, 0 }, LightVector{ -1, 0, 0 }, LightVector{ 0, 1, 0 },
                    LightVector{ 0, -1, 0 }, LightVector{ 0, 0, 1 }, LightVector{ 0, 0, -1 }, LightVector{ 1, 2, 3 } }) {
                const auto unit = *Unit(direction);
                for (float metres : { .12F, .3F, 1.0F, 3.0F, 100.0F }) {
                    const float separation = metres * kUnitsPerMetre;
                    LightVector camera{ head.x + unit.x * separation, head.y + unit.y * separation, head.z + unit.z * separation };
                    for (float height : { -.5F, -.2F, 0.0F, .15F, .5F }) {
                        const auto position = FaceLightPosition(head, camera, height);
                        Require(position && Finite(*position), "face position handles camera orbit, pitch and closeups at all heights");
                        const float expected = std::min(.45F * kUnitsPerMetre, separation * .5F);
                        const LightVector delta{ position->x - head.x, position->y - head.y, position->z - head.z };
                        Require(std::abs(delta.x - unit.x * expected) < .001F && std::abs(delta.y - unit.y * expected) < .001F,
                            "face retains camera-side XY placement, smoothly shrinking under overhead cameras");
                        Require(std::abs(delta.z - height * kUnitsPerMetre) < .001F,
                            "face elevation is head-relative world up, independent of camera pitch");
                    }
                }
            }
        }
        Require(!FaceLightPosition({}, {}, .15F) && !FaceLightPosition({}, { 1, 0, 0 }, .15F), "inside-head camera suppresses fill");
        Require(!FaceLightPosition({ std::numeric_limits<float>::quiet_NaN(), 0, 0 }, { 100, 0, 0 }, .15F) &&
            !FaceLightPosition({}, { std::numeric_limits<float>::infinity(), 0, 0 }, .15F), "nonfinite face/camera suppressed");
        for (float canvas : { 310.0F, 330.0F, 390.0F, 430.0F }) {
            const auto controls = CalculateFaceControls(canvas);
            Require(controls.toggle.x + controls.toggle.width < controls.settings.x, "face toggle/settings hit targets are disjoint");
            Require(controls.toggle.y >= canvas * .5F + std::min(44.0F, canvas * .13F) * 1.25F, "face controls do not cover Actor circle");
            for (const auto rect : { controls.toggle, controls.settings }) {
                for (int node = 0; node < 8; ++node) {
                    const float angle = node * std::numbers::pi_v<float> / 4;
                    const float x = canvas * .5F + std::cos(angle) * canvas * .36F;
                    const float y = canvas * .5F + std::sin(angle) * canvas * .36F;
                    const float radius = std::clamp(canvas * .073F, 13.0F, 31.0F);
                    Require(rect.x + rect.width < x - radius || rect.x > x + radius || rect.y + rect.height < y - radius || rect.y > y + radius,
                        "face targets cannot overlap any direction node's square hit target");
                }
            }
        }
    }

    void AdjustableFaceHeight()
    {
        LightingEditor editor;
        const auto initial = editor.GetScene();
        Require(initial.face.heightOffset == .15F && initial.face.intensity == .35F && !initial.face.enabled,
            "elevated face default retains weak strength and initial OFF");
        editor.SelectSlot(2);
        editor.SetFaceHeight(-.25F);
        auto expected = initial; expected.face.heightOffset = -.25F;
        Require(editor.GetScene() == expected && editor.SelectedSlot() == 2 && editor.History().size() == 1,
            "height preview changes only face height even with an unplaced manual slot selected");
        editor.FinishNumericEdit();
        Require(editor.History().size() == 2 && editor.History().back().label == "フェイスライト : 上下",
            "face height has a dedicated history label");
        editor.Undo();
        Require(editor.GetScene() == initial, "undo returns to elevated default");
        editor.SetFaceHeight(kFaceHeightDefault); editor.FinishNumericEdit();
        Require(editor.CanRedo(), "height no-op retains redo");
        editor.Redo();
        Require(editor.GetScene() == expected && editor.SelectedSlot() == 2, "height redo restores recorded selection");
        const auto count = editor.History().size();
        for (int n = 0; n < 100; ++n) { editor.SetFaceHeight(-.5F + n * .01F); }
        Require(editor.History().size() == count && editor.HasPendingChanges(), "height drag coalesces");
        editor.FinishNumericEdit();
        Require(editor.History().size() == count + 1, "height drag is one history entry");
        editor.Undo();
        Require(editor.GetScene() == expected, "one undo reverts whole height drag");
        editor.Adopt();
        editor.SetFaceHeight(.3F);
        editor.SetFaceRange(.6F);
        Require(editor.History().back().label == "フェイスライト : 上下", "range commits preceding height drag");
        editor.SetFaceIntensity(2.42F);
        Require(editor.History().back().label == "フェイスライト : 光の範囲", "strength commits preceding range drag");
        editor.FinishNumericEdit();
        Require(editor.GetScene().face.intensity == 2.42F, "stronger face fill available without automatic power change");
        editor.Undo(); editor.Undo(); editor.Undo();
        Require(editor.GetScene() == expected, "three face sliders undo independently");
        editor.Redo(); editor.Redo(); editor.Redo();
        editor.RestoreBookmark();
        Require(editor.GetScene() == expected, "bookmark restores height along with range strength and manual lights");
        editor.SelectSlot(0);
        const auto beforeMixed = editor.GetScene();
        editor.SetNumeric(NumericField::Height, .7F);
        editor.SetFaceHeight(.4F);
        Require(editor.History().back().label == "Light 1 : 高さ", "face height commits manual height independently");
        editor.SetNumeric(NumericField::Intensity, 1.8F);
        Require(editor.History().back().label == "フェイスライト : 上下", "manual intensity commits face height");
        editor.FinishNumericEdit();
        editor.Undo(); editor.Undo(); editor.Undo();
        Require(editor.GetScene() == beforeMixed, "mixed gestures restore exact scene");
        for (float bad : { std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::infinity() }) {
            editor.SetFaceHeight(bad);
            Require(editor.GetScene() == beforeMixed && editor.CanRedo(), "invalid height does not destroy redo");
        }
        editor.SetFaceHeight(-100); editor.FinishNumericEdit();
        Require(editor.GetScene().face.heightOffset == -.5F, "height lower clamp");
        editor.SetFaceHeight(100); editor.FinishNumericEdit();
        Require(editor.GetScene().face.heightOffset == .5F, "height upper clamp");
        editor.ToggleFaceLight();
        const auto retained = editor.GetScene().face;
        for (std::size_t i = 0; i < kScenePresets.size(); ++i) {
            const auto expectedFace = kScenePresets[i].includeFace ? kScenePresets[i].scene.face : editor.GetScene().face;
            PresetPicker picker; picker.Open(); Require(picker.Choose(i, editor), "preset selected");
            Require(editor.GetScene().face == expectedFace, "sample face scope controls non-default face height");
            const auto appliedFace = editor.GetScene().face;
            for (int slot = 0; slot < 3; ++slot) {
                editor.SelectSlot(slot); editor.SetType(3); editor.SetWideRange();
                editor.SetFinePosition({ .2F, -.3F, .1F }); editor.ResetFinePosition();
                editor.ToggleLight(slot); editor.DeleteCurrent();
                Require(editor.GetScene().face == appliedFace, "manual controls and reset do not touch face height");
            }
        }
        for (float bad : { -.501F, .501F, std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::infinity() }) {
            Require(!FaceLightPosition({}, { 100, 0, 0 }, bad), "geometry rejects invalid height");
            for (bool enabled : { false, true }) {
                LightingEditor clean; const auto prior = clean.GetScene();
                auto invalid = prior; invalid.face.heightOffset = bad; invalid.face.enabled = enabled; invalid[1].placed = true;
                Require(!clean.ApplyScene(invalid, "invalid height") && clean.GetScene() == prior, "invalid height rejects whole editor scene even OFF");
                LightSession session; session.SetReady(true);
                Require(!session.Start(session.Get().epoch, invalid), "invalid height rejects start");
                session.Start(session.Get().epoch, prior);
                Require(!session.Submit(session.Get().epoch, invalid) && session.Get().lights == prior, "invalid height rejects live snapshot atomically");
            }
        }
        for (int mask = 0; mask < 8; ++mask) {
            for (float height : { -.5F, 0.0F, .15F, .5F }) {
                for (bool enabled : { false, true }) {
                    Scene scene = kScenePresets[2].scene;
                    for (int slot = 0; slot < 3; ++slot) { scene[slot].enabled = (mask & (1 << slot)) != 0; }
                    scene.face = { enabled, 3.0F, .77F, height };
                    LightSession session; session.SetReady(true);
                    Require(session.Start(session.Get().epoch, scene), "all allowed heights start with all power masks");
                    Require(LightSession::ShouldIlluminateFace(session.Get()) == enabled, "height does not change power gate");
                    auto next = scene; next.face.heightOffset = height == .5F ? -.5F : .5F;
                    const auto epoch = session.Get().epoch;
                    Require(session.Submit(epoch, next) && session.Get().lights == next, "live snapshot carries new height");
                    Require(LightRegistrationSettings::From(*RuntimeSettings(scene, kFaceRuntimeSlot)) ==
                        LightRegistrationSettings::From(*RuntimeSettings(next, kFaceRuntimeSlot)), "height is live position only, no re-registration");
                    for (int slot = 0; slot < 3; ++slot) {
                        Require(*RuntimeSettings(next, slot) == scene[slot], "face height never alters manual runtime data");
                    }
                    Require(session.Stop(epoch) && !session.Submit(epoch, scene) && !LightSession::ShouldIlluminateFace(session.Get()),
                        "height cannot bypass stop or resurrect a stale light");
                    session.Start(session.Get().epoch, next); session.Block(true); session.Block(false);
                    Require(!LightSession::ShouldIlluminateFace(session.Get()) && session.Get().lights == next,
                        "load transition preserves height but requires explicit restart");
                }
            }
        }
        for (float cameraZ : { -1000.0F, -100.0F, 0.0F, 100.0F, 1000.0F }) {
            const auto pos = FaceLightPosition({}, { 100, 0, cameraZ }, .15F);
            Require(pos && std::abs(pos->z - .15F * kUnitsPerMetre) < .001F,
                "low cameras no longer lower the face fill");
        }
    }

    void ColorAndRecipeEditing()
    {
        const LightTint pink{.81F, .21F, .47F};
        for (int type = 0; type < kLightTypeCount; ++type) {
            for (int slot = 0; slot < 3; ++slot) {
                for (bool enabled : { false, true }) {
                    LightingEditor editor;
                    auto scene = kScenePresets[2].scene;
                    scene.face = { true, 2.0F, .8F, -.1F };
                    auto& light = scene[slot];
                    light.placed = true;
                    light.enabled = enabled;
                    light.fine = { .2F, -.3F, .4F };
                    light.shadowBias = 4.0F;
                    editor.ApplyScene(scene, "test");
                    editor.SelectSlot(slot);
                    const auto count = editor.History().size();
                    for (int i = 0; i < 20; ++i) { editor.SetTint({.01F * i, .2F, .3F}); }
                    editor.SetTint(pink);
                    Require(editor.History().size() == count, "picker drag does not flood history");
                    auto expected = scene;
                    expected[slot].customTint = pink;
                    Require(editor.GetScene() == expected, "color changes only selected tint, including while OFF");
                    editor.FinishNumericEdit();
                    Require(editor.History().size() == count + 1, "one color gesture is one undo entry");
                    Require(RuntimeTint(editor.CurrentLight()) == pink, "runtime receives exact custom RGB");
                    Require(LightRegistrationSettings::From(editor.CurrentLight()) == LightRegistrationSettings::From(light),
                        "color does not rebuild shadow registration");
                    editor.Undo();
                    Require(editor.GetScene() == scene, "undo restores original color and all other fields");
                    editor.Redo();
                    Require(editor.GetScene() == expected, "redo restores custom color");
                    editor.ApplyLightRecipe(type, pink);
                    expected[slot].type = type;
                    Require(editor.GetScene() == expected, "recipe applies type/color without position, intensity, range or power changes");
                    editor.SetType(type);
                    expected[slot].customTint.reset();
                    Require(editor.GetScene() == expected && RuntimeTint(editor.CurrentLight()) == RuntimeTint(type),
                        "built-in selection restores its original color only");
                    editor.SelectSlot((slot + 1) % 3);
                    editor.DeleteCurrent();
                    const auto before = editor.CurrentLight();
                    editor.SetTint(pink);
                    Require(editor.CurrentLight() == before, "color cannot place an empty slot");
                    editor.ApplyLightRecipe(type, pink);
                    auto placed = before;
                    placed.placed = true; placed.enabled = true; placed.type = type; placed.customTint = pink;
                    Require(editor.CurrentLight() == placed, "recipe places an empty slot with its staged position intact");
                    Require(editor.GetScene().face == scene.face, "recipe never affects face fill");
                }
            }
        }
        LightingEditor editor;
        const auto initial = editor.GetScene();
        for (float bad : { -.01F, 1.01F, std::numeric_limits<float>::infinity(), std::numeric_limits<float>::quiet_NaN() }) {
            for (const auto rgb : { LightTint{bad, 0, 0}, LightTint{0, bad, 0}, LightTint{0, 0, bad} }) {
                editor.SetTint(rgb);
                editor.ApplyLightRecipe(0, rgb);
                auto scene = initial; scene[0].customTint = rgb;
                Require(!LightingEditor::IsValidScene(scene) && !RuntimeTint(scene[0]), "invalid RGB is rejected by both state and runtime");
                Require(!editor.ApplyScene(scene, "invalid"), "invalid scene color is not applied");
                Require(editor.GetScene() == initial, "invalid color cannot alter the editor");
            }
        }
        editor.SetTint({0, 0, 0}); editor.FinishNumericEdit();
        Require(editor.CurrentLight().enabled && RuntimeTint(editor.CurrentLight()) == LightTint{0, 0, 0}, "black is valid without toggling power");
    }

    void PresetSerialization()
    {
        for (int type = 0; type < kLightTypeCount; ++type) {
            for (auto tint : { std::optional<LightTint>{}, std::optional{LightTint{0, 1, .123456789F}} }) {
                SavedLight entry{ "青い光 \\\" #1", type, tint, {} };
                auto decoded = PresetLibrary::DecodeLight(PresetLibrary::Encode(entry));
                Require(decoded && decoded->name == entry.name && decoded->type == type && decoded->tint == tint,
                    "light serialization round trips Japanese, quotes, backslashes and exact RGB");
            }
        }
        for (const auto& preset : kScenePresets) {
            for (int mask = 0; mask < 8; ++mask) {
                for (bool face : { false, true }) {
                    auto scene = preset.scene;
                    for (int i = 0; i < 3; ++i) {
                        scene[i].enabled = (mask & (1 << i)) != 0;
                        scene[i].fine = { -.37F, .52F, -.79F };
                        scene[i].shadowBias = 4.0F;
                        scene[i].customTint = LightTint{.123456789F, .987654321F, .333333333F};
                    }
                    scene.face = { true, 2.49F, .73F, -.31F };
                    SavedScene entry{ "夜の構成", scene, face, {} };
                    const auto text = PresetLibrary::Encode(entry);
                    const auto decoded = PresetLibrary::DecodeScene(text);
                    auto expected = scene;
                    if (!face) { expected.face = {}; expected.face.basis = FaceLightBasis::Camera; }
                    Require(decoded && decoded->name == entry.name && decoded->includeFace == face && decoded->scene == expected,
                        "scene disk round trip preserves every setting, including off/unplaced slots");
                    LightingEditor editor;
                    const auto previous = editor.GetScene();
                    Require(PresetLibrary::Apply(*decoded, editor), "saved scene applies");
                    if (!face) { expected.face = previous.face; }
                    Require(editor.GetScene() == expected, "face checkbox controls apply scope");
                    editor.Undo();
                    Require(editor.GetScene() == previous, "saved scene is one undo step");
                    editor.Redo();
                    Require(editor.GetScene() == expected, "saved scene redo restores exact value-owned settings");
                    Require(!PresetLibrary::DecodeScene(text + "garbage"), "scene rejects trailing content");
                }
            }
        }
        for (const std::string name : std::initializer_list<std::string>{ "", "   ", "bad##id", "line\nbreak", std::string(121, 'a'), std::string("\xC0\xAF") }) {
            Require(!PresetLibrary::ValidName(name), "unsafe or invalid UTF8 names rejected");
        }
        for (const std::string text : std::initializer_list<std::string>{
            "SLA LIGHT 2\n\"name\"\n0\n0 1 1 1\n", "SLA LIGHT 1\n\"name\"\n4\n0 1 1 1\n",
            "SLA LIGHT 1\n\"name\"\n0\n2 1 1 1\n", "SLA LIGHT 1\n\"name\"\n0\n1 -1 1 1\n",
            "SLA LIGHT 1\n\"name\"\n0\n1 nan 1 1\n", "SLA LIGHT 1\n\"name\"\n0\n1 1 1",
            std::string(65537, 'a') }) {
            Require(!PresetLibrary::DecodeLight(text), "invalid schema/version/type/tint/truncation/oversize rejected");
        }
        SavedScene invalid{ "bad", {}, true, {} };
        invalid.scene[0].shadowProjection = static_cast<ShadowProjection>(99);
        Require(!PresetLibrary::DecodeScene(PresetLibrary::Encode(invalid)), "unknown shadow mode rejected");
        invalid.scene[0].shadowProjection = ShadowProjection::Spot;
        invalid.scene.face.range = 20.0F;
        Require(!PresetLibrary::DecodeScene(PresetLibrary::Encode(invalid)), "invalid face settings rejected");
    }

    void PresetFiles()
    {
        // Keep the new, uniquely owned fixtures for inspection; never touch a user's preset folder.
        const auto root = std::filesystem::temp_directory_path() / ("sla-preset-tests-" +
            std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        Require(std::filesystem::create_directory(root), "new test directory is exclusively owned");
        const auto folder = root / u8"登録";
        PresetLibrary library(folder);
        library.Load();
        Require(library.Error().empty() && library.Lights().empty() && library.Scenes().empty(), "missing library is an empty first run");
        auto scene = kScenePresets[2].scene;
        scene[0].placed = true;
        scene[0].customTint = LightTint{.123F, .987F, .345F};
        scene.face = {true, 2.3F, .7F, -.2F};
        Require(library.SaveLight("青/光", scene[0]), "save light to Unicode path with filename-independent display name");
        Require(library.SaveScene("撮影", scene, true), "save full scene");
        Require(library.SaveScene("撮影（顔を保持）", scene, false), "save manual-only scene");
        const auto original = library.Lights()[0];
        auto read = [](const auto& path) { std::ifstream in(path, std::ios::binary); return std::string(std::istreambuf_iterator<char>(in), {}); };
        const auto originalBytes = read(original.file);
        auto changed = scene[0]; changed.customTint = LightTint{1, 0, 0};
        Require(!library.SaveLight("青/光", changed) && read(original.file) == originalBytes, "duplicate names never overwrite saved light");
        Require(!library.SaveScene("撮影", {}, false), "duplicate scene rejected");
        PresetLibrary reloaded(folder); reloaded.Load();
        Require(reloaded.Error().empty() && reloaded.Lights().size() == 1 && reloaded.Scenes().size() == 2, "fresh library instance restores registrations");
        Require(reloaded.Lights()[0].tint == scene[0].customTint && reloaded.Scenes()[0].scene == scene, "disk reload is exact");
        Require(reloaded.ArchiveLight(0) && reloaded.Lights().empty(), "explicit removal archives and updates list");
        const auto archived = std::filesystem::directory_iterator(folder / "Archived")->path();
        Require(read(archived) == originalBytes && !std::filesystem::exists(original.file), "archive retains recoverable exact bytes");
        reloaded.Load();
        Require(reloaded.Lights().empty() && reloaded.Scenes().size() == 2 && reloaded.Scenes()[0].scene == scene,
            "scene colors do not reference archived light entries; archives not auto-loaded");
        std::filesystem::copy_file(archived, original.file);
        reloaded.Load();
        Require(reloaded.Lights().size() == 1, "restoring archived file recovers registration");
        const auto bad = folder / "broken.slalight";
        { std::ofstream out(bad); out << "broken"; }
        { std::ofstream out(folder / "ignored.slaset.tmp"); out << "unfinished"; }
        reloaded.Load();
        Require(reloaded.Lights().size() == 1 && reloaded.Scenes().size() == 2 && !reloaded.Error().empty(),
            "bad file is reported while valid entries and unfinished writes are handled safely");
        Require(read(bad) == "broken", "loading never rewrites invalid file");
        Require(reloaded.ArchiveScene(0) && reloaded.Scenes().size() == 1, "scene can also be archived");
        Require(!reloaded.ArchiveLight(99) && !reloaded.ArchiveScene(99), "invalid archive index is a no-op");
        const auto blocked = root / "not-a-directory";
        { std::ofstream out(blocked); out << "sentinel"; }
        PresetLibrary failed(blocked);
        Require(!failed.SaveLight("new", scene[0]) && failed.Lights().empty() && !failed.Error().empty(), "write failure never creates phantom registration");
        Require(read(blocked) == "sentinel", "write failure preserves existing unrelated file");
        std::cout << "Preset file fixtures: " << root.string() << '\n';
    }

    void DiagramContrastAndOccupants()
    {
        using namespace ScreenshotLightingAssistant::UI;
        LightSettings light;
        light.placed = true;
        for (int r = 0; r <= 255; r += 17) {
            for (int g = 0; g <= 255; g += 17) {
                for (int b = 0; b <= 255; b += 17) {
                    light.customTint = LightTint{r / 255.0F, g / 255.0F, b / 255.0F};
                    for (const bool enabled : {true, false}) {
                        light.enabled = enabled;
                        const auto rgb = DiagramTint(light);
                        const float luminance = RelativeLuminance(rgb);
                        const float contrast = UseBlackNumber(rgb) ? (luminance + .05F) / .05F : 1.05F / (luminance + .05F);
                        Require(contrast >= 4.5F, "black/white number retains contrast on every sampled ON/OFF tint");
                        if (enabled) { Require(rgb == RGB8{static_cast<std::uint8_t>(r), static_cast<std::uint8_t>(g), static_cast<std::uint8_t>(b)}, "ON diagram preserves actual RGB without whitening"); }
                    }
                }
            }
        }
        light.customTint.reset(); light.enabled = true;
        for (int type = 0; type < kLightTypeCount; ++type) {
            light.type = type;
            const auto rgb = DiagramTint(light);
            Require(rgb[0] == static_cast<std::uint8_t>(kRuntimeTints[type].red * 255 + .5F), "default swatch follows runtime color, not schematic type color");
        }
        for (const float radius : {13.0F, 20.0F, 31.0F}) {
            for (int count = 1; count <= 3; ++count) {
                for (int i = 0; i < count; ++i) {
                    const auto badge = OccupantBadge(radius, count, i);
                    Require(std::hypot(badge.x, badge.y) + badge.radius <= radius, "number/color badges stay within node");
                    for (int j = i + 1; j < count; ++j) {
                        const auto other = OccupantBadge(radius, count, j);
                        Require(std::hypot(badge.x - other.x, badge.y - other.y) + .001F >= badge.radius + other.radius,
                            "overlapping-slot colors have non-overlapping independent badges");
                    }
                }
            }
        }
        for (const float canvas : {128.0F, 140.0F, 310.0F, 390.0F}) {
            const float radius = DiagramNodeRadius(canvas, 3);
            Require(radius >= 19, "thumbnail clusters use a larger node for readable numbers");
            const float adjacentDistance = 2 * canvas * .36F * std::sin(std::numbers::pi_v<float> / 8);
            Require(radius + DiagramNodeRadius(canvas, 1) < adjacentDistance, "cluster cannot collide with adjacent single node");
            Require(canvas * .5F + canvas * .36F + radius < canvas + 4, "front cluster stays above camera body");
            Require(OccupantBadge(radius, 3, 0).radius * 1.45F >= 11.8F, "cluster digit height has a readable geometric lower bound");
        }
    }

    void LibraryViewPersistence()
    {
        const auto root = std::filesystem::temp_directory_path() / ("sla-view-tests-" +
            std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        Require(std::filesystem::create_directory(root), "view test fixtures exclusively owned");
        const auto folder = root / u8"保存";
        PresetLibrary library(folder);
        library.Load();
        auto light = kScenePresets[0].scene[0];
        auto read = [](const auto& path) { std::ifstream in(path, std::ios::binary); return std::string(std::istreambuf_iterator<char>(in), {}); };
        for (int i = 0; i < 6; ++i) {
            light.customTint = LightTint{i * .15F, .2F, .8F};
            Require(library.SaveLight("色 " + std::to_string(i), light), "more than three registrations supported");
        }
        Require(PresetLibrary::kFavoriteLimit == 5, "five main favorites available");
        Require(library.Favorites() == std::vector<std::size_t>{0, 1, 2, 3, 4}, "first five registrations become favorites; no silent eviction");
        const auto original = read(library.Lights()[0].file);
        Require(!library.SetFavorite(5, true) && library.Favorites() == std::vector<std::size_t>{0, 1, 2, 3, 4}, "sixth favorite needs explicit victim");
        Require(!library.SetFavorite(5, true, 5), "non-favorite cannot be a replacement victim");
        Require(library.SetFavorite(5, true, 1) && library.Favorites() == std::vector<std::size_t>{0, 5, 2, 3, 4}, "explicit replacement keeps bar order");
        Require(library.SetFavorite(2, false) && library.Lights().size() == 6, "unfavorite is not deletion");
        Require(library.SetFavorite(1, true), "vacant favorite slot can be filled");
        for (int i = 0; i < 4; ++i) { Require(library.SaveScene("構成 " + std::to_string(i), kScenePresets[i].scene, false), "save scene for sorting"); }
        const auto sceneBytes = read(library.Scenes()[1].file);
        Require(library.MoveScene(1, -1) && library.Scenes()[0].name == "構成 1", "reorder scenes");
        Require(!library.MoveScene(0, -1) && !library.MoveScene(3, 1) && !library.MoveScene(0, 2), "reorder boundaries are safe");
        Require(library.SetSampleHidden("rim", true) && library.IsSampleHidden("rim"), "hide built-in without deleting it");
        Require(!library.SetSampleHidden("unknown", true), "unknown built-in id rejected");
        PresetLibrary reloaded(folder); reloaded.Load();
        Require(reloaded.ViewError().empty() && reloaded.Favorites() == library.Favorites(), "favorites survive fresh load");
        Require(reloaded.Scenes()[0].name == "構成 1" && reloaded.IsSampleHidden("rim"), "order and hidden sample survive fresh load");
        Require(read(reloaded.Lights()[0].file) == original && read(reloaded.Scenes()[0].file) == sceneBytes, "view changes never rewrite registration payloads");
        Require(reloaded.SetSampleHidden("rim", false) && !reloaded.IsSampleHidden("rim"), "hidden built-ins can be shown again");
        Require(reloaded.ArchiveLight(3), "archive favorite safely");
        reloaded.Load();
        Require(reloaded.Favorites().size() == 4 && reloaded.Lights().size() == 5, "missing archived favorite is filtered; never replaced implicitly");
        Require(reloaded.ArchiveScene(0), "archive ordered scene safely");
        reloaded.Load();
        Require(reloaded.Scenes().size() == 3 && reloaded.Scenes()[0].name == "構成 0", "ordering ignores archived scene");
        for (const auto index : reloaded.Favorites()) { Require(reloaded.SetFavorite(index, false), "explicitly clear bar"); }
        reloaded.Load();
        Require(reloaded.Favorites().empty(), "empty saved favorite bar remains empty after restart");

        // Old format migration creates no metadata until an explicit edit.
        const auto migration = root / "legacy";
        std::filesystem::create_directory(migration);
        for (int i = 0; i < 4; ++i) { std::filesystem::copy_file(library.Lights()[i == 3 ? 4 : i].file, migration / (std::to_string(i) + ".slalight")); }
        PresetLibrary legacy(migration); legacy.Load();
        Require(legacy.Favorites() == std::vector<std::size_t>{0, 1, 2}, "legacy registrations retain first three shortcuts");
        Require(!std::filesystem::exists(migration / "library-view.sla"), "migration read does not write metadata");
        Require(!std::filesystem::exists(migration / "library-view-v3.sla"), "legacy load does not create v3 metadata");

        // 0.1.18 migration preserves explicit favorites, ordering, hidden samples,
        // and the legacy file bytes for a downgrade. Read alone must not save.
        const auto oldView = migration / "library-view.sla";
        const std::string oldBytes = "SLA VIEW 1\n2\n\"2.slalight\"\n\"0.slalight\"\n2\n\"b.slaset\"\n\"a.slaset\"\n1\n\"rim\"\n";
        { std::ofstream out(oldView, std::ios::binary); out << oldBytes; }
        { std::ofstream out(migration / "a.slaset", std::ios::binary); out << PresetLibrary::Encode(SavedScene{"A", kScenePresets[0].scene, false, {}}); }
        { std::ofstream out(migration / "b.slaset", std::ios::binary); out << PresetLibrary::Encode(SavedScene{"B", kScenePresets[1].scene, false, {}}); }
        legacy.Load();
        Require(legacy.ViewError().empty() && legacy.Favorites() == std::vector<std::size_t>{2, 0}, "v1 favorites migrate in saved order, no automatic refill");
        Require(legacy.Scenes()[0].name == "B" && legacy.IsSampleHidden("rim"), "v1 scene order and hidden samples migrate");
        Require(!std::filesystem::exists(migration / "library-view-v3.sla"), "v1 migration is read-only");
        Require(legacy.SetFavorite(3, true) && legacy.SetFavorite(1, true), "migrated list supports explicit fourth favorite");
        Require(read(oldView) == oldBytes, "v3 editing preserves v1 bytes for rollback");
        legacy.Load();
        Require(legacy.ViewError().empty() && legacy.Favorites() == std::vector<std::size_t>{2, 0, 3, 1}, "v3 takes priority and persists four favorites");
        Require(legacy.Scenes()[0].name == "B" && legacy.IsSampleHidden("rim"), "v3 keeps unrelated organizing preferences");
        for (const auto i : legacy.Favorites()) { Require(legacy.SetFavorite(i, false), "clear migrated favorites explicitly"); }
        legacy.Load();
        Require(legacy.Favorites().empty() && read(oldView) == oldBytes, "empty v3 list never falls back to nonempty v1");
        { std::ofstream out(migration / "library-view-v3.sla", std::ios::binary); out << "SLA VIEW 99\n"; }
        legacy.Load();
        Require(!legacy.ViewError().empty() && legacy.Favorites().empty() && !legacy.SetFavorite(0, true), "invalid present v3 never falls back to legacy settings");
        Require(read(oldView) == oldBytes && read(migration / "library-view-v3.sla") == "SLA VIEW 99\n", "both metadata generations preserved on failure");

        // Failed atomic replacement leaves UI state and all registration files intact.
        const auto viewPath = folder / "library-view-v3.sla";
        std::filesystem::rename(viewPath, folder / "view-backup.sla");
        std::filesystem::create_directory(viewPath);
        const auto beforeName = reloaded.Scenes()[0].name;
        Require(!reloaded.MoveScene(0, 1) && reloaded.Scenes()[0].name == beforeName, "write failure rolls back in-memory order");
        Require(!reloaded.SetFavorite(0, true) && reloaded.Favorites().empty(), "write failure rolls back favorites");
        Require(!reloaded.ViewError().empty() && read(reloaded.Lights()[0].file) == original, "write failure visible, original light preserved");

        const auto invalidFolder = root / "invalid";
        std::filesystem::create_directory(invalidFolder);
        std::filesystem::copy_file(library.Lights()[0].file, invalidFolder / "ok.slalight");
        const auto invalidView = invalidFolder / "library-view.sla";
        const std::vector<std::string> badViews{
            "SLA VIEW 2\n0\n0\n0\n", "SLA VIEW 1\n4\n", "SLA VIEW 1\n1\n\"../escape\"\n0\n0\n",
            "SLA VIEW 1\n2\n\"x\"\n\"x\"\n0\n0\n", "SLA VIEW 1\n0\n129\n",
            "SLA VIEW 1\n0\n0\n0\ntrailing", std::string(65537, 'x')
        };
        for (const auto& invalid : badViews) {
            { std::ofstream out(invalidView, std::ios::binary); out << invalid; }
            PresetLibrary broken(invalidFolder); broken.Load();
            Require(broken.Lights().size() == 1 && !broken.ViewError().empty(), "invalid metadata does not hide valid registrations");
            Require(!broken.SetFavorite(0, true) && read(invalidView) == invalid, "invalid/unsupported metadata is preserved, not overwritten");
        }
        const auto invalidV2 = invalidFolder / "library-view-v2.sla";
        const std::vector<std::string> badV2{
            "SLA VIEW 1\n0\n0\n0\n", "SLA VIEW 2\n5\n", "SLA VIEW 2\n1\n\"../escape\"\n0\n0\n",
            "SLA VIEW 2\n2\n\"x\"\n\"x\"\n0\n0\n", "SLA VIEW 2\n0\n129\n",
            "SLA VIEW 2\n0\n0\n0\ntrailing", std::string(65537, 'x')
        };
        for (const auto& invalid : badV2) {
            { std::ofstream out(invalidV2, std::ios::binary); out << invalid; }
            PresetLibrary broken(invalidFolder); broken.Load();
            Require(broken.Lights().size() == 1 && !broken.ViewError().empty(), "invalid v2 leaves registrations available");
            Require(!broken.SetFavorite(0, true) && read(invalidV2) == invalid, "invalid v2 is preserved without writing");
        }
        // Upgrade 0.1.19-0.1.23's four-slot metadata without overwriting it.
        const auto fromV2 = root / "from-v2";
        std::filesystem::create_directory(fromV2);
        for (int i = 0; i < 6; ++i) {
            std::ofstream out(fromV2 / (std::to_string(i) + ".slalight"), std::ios::binary);
            out << original; // independent fixtures; the earlier archive test moved an original
        }
        const auto priorPath = fromV2 / "library-view-v2.sla";
        const std::string prior = "SLA VIEW 2\n4\n\"3.slalight\"\n\"1.slalight\"\n\"0.slalight\"\n\"2.slalight\"\n0\n1\n\"rim\"\n";
        { std::ofstream out(priorPath, std::ios::binary); out << prior; }
        PresetLibrary upgraded(fromV2); upgraded.Load();
        Require(upgraded.ViewError().empty() && upgraded.Favorites() == std::vector<std::size_t>{3, 1, 0, 2},
            "v2 upgrade keeps all four favorites in original order");
        const auto newestPath = fromV2 / "library-view-v3.sla";
        Require(!std::filesystem::exists(newestPath), "v2 upgrade read creates no new file");
        Require(upgraded.IsSampleHidden("rim") && upgraded.SetFavorite(4, true), "v2 upgrade permits fifth favorite");
        upgraded.Load();
        Require(upgraded.Favorites() == std::vector<std::size_t>{3, 1, 0, 2, 4} && upgraded.IsSampleHidden("rim"),
            "five-slot metadata retains order and other view preferences across reload");
        Require(read(priorPath) == prior, "old v2 remains byte-identical for downgrade");
        Require(!upgraded.SetFavorite(5, true), "sixth upgraded favorite requires explicit replacement");
        Require(upgraded.SetFavorite(5, true, 1), "sixth upgraded favorite can explicitly replace selected favorite");
        const std::vector<std::string> badV3{
            "SLA VIEW 2\n0\n0\n0\n", "SLA VIEW 3\n6\n", "SLA VIEW 3\n1\n\"../escape\"\n0\n0\n",
            "SLA VIEW 3\n2\n\"x\"\n\"x\"\n0\n0\n", "SLA VIEW 3\n0\n129\n",
            "SLA VIEW 3\n0\n0\n0\ntrailing", std::string(65537, 'x')
        };
        for (const auto& invalid : badV3) {
            { std::ofstream out(newestPath, std::ios::binary); out << invalid; }
            upgraded.Load();
            Require(!upgraded.ViewError().empty() && upgraded.Favorites().empty() && upgraded.Lights().size() == 6,
                "invalid v3 never falls back to populated v2 but keeps registrations usable");
            Require(!upgraded.SetFavorite(0, true) && read(newestPath) == invalid && read(priorPath) == prior,
                "invalid v3 and old v2 bytes are both preserved");
        }
        std::cout << "View file fixtures: " << root.string() << '\n';
    }

    void RenamePersistence()
    {
        const auto root = std::filesystem::temp_directory_path() / ("sla-rename-tests-" +
            std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        Require(std::filesystem::create_directory(root), "rename fixtures exclusively owned");
        auto read = [](const auto& p) { std::ifstream in(p, std::ios::binary); return std::string(std::istreambuf_iterator<char>(in), {}); };
        PresetLibrary library(root); library.Load();
        auto scene = kScenePresets[2].scene;
        scene[0].customTint = LightTint{.24F, .56F, .92F};
        scene.face = {true, 2.3F, .76F, -.12F};
        Require(library.SaveLight("Light A", scene[0]) && library.SaveLight("Light B", scene[1]), "prepare lights for rename");
        Require(library.SaveScene("Scene A", scene, true) && library.SaveScene("Scene B", scene, false), "prepare presets for rename");
        Require(library.MoveScene(1, -1) && library.SetFavorite(0, false), "customize order and favorites");
        const auto light = library.Lights()[1]; const auto preset = library.Scenes()[0];
        const auto lightBytes = read(light.file); const auto sceneBytes = read(preset.file);
        const auto viewBytes = read(root / "library-view-v3.sla"); const auto favorites = library.Favorites();
        Require(library.RenameLight(1, "青 💡 light") && library.RenameScene(0, "窓辺 / warm"), "Unicode rename in both categories");
        Require(library.Lights()[1].file == light.file && library.Scenes()[0].file == preset.file, "rename keeps stable file IDs");
        Require(library.Lights()[1].type == light.type && library.Lights()[1].tint == light.tint &&
            library.Scenes()[0].scene == preset.scene && library.Scenes()[0].includeFace == preset.includeFace, "rename changes no lighting values");
        Require(read(root / "library-view-v3.sla") == viewBytes && library.Favorites() == favorites, "rename leaves list preferences byte-identical");
        std::size_t backups = 0; bool backedLight = false, backedScene = false;
        for (const auto& item : std::filesystem::directory_iterator(root / "Archived")) {
            ++backups; backedLight |= read(item.path()) == lightBytes; backedScene |= read(item.path()) == sceneBytes;
        }
        Require(backups == 2 && backedLight && backedScene, "both original names and payloads backed up exactly");
        library.Load();
        Require(library.Lights()[1].name == "青 💡 light" && library.Scenes()[0].name == "窓辺 / warm" &&
            library.Favorites() == favorites, "renames and ordering survive restart");
        const auto renamed = read(light.file);
        Require(library.RenameLight(1, "青 💡 light") && read(light.file) == renamed, "same-name rename is a no-op");
        Require(!library.RenameLight(1, "Light A") && !library.RenameScene(0, "Scene A"), "duplicates rejected within each category");
        for (const auto& name : std::vector<std::string>{"", "bad##id", "bad\nname", std::string(121, 'a')}) {
            Require(!library.RenameLight(1, name) && !library.RenameScene(0, name), "invalid renames rejected");
        }
        Require(!library.RenameLight(99, "x") && !library.RenameScene(99, "x"), "invalid rename indices rejected");
        Require(read(light.file) == renamed, "failed rename does not damage the saved entry");
        { std::ofstream out(light.file, std::ios::binary); out << "externally changed"; }
        Require(!library.RenameLight(1, "new") && read(light.file) == "externally changed" &&
            library.Lights()[1].name == "青 💡 light", "external changes are detected and preserved");
        // Formatting-only edits decode to the same entry and remain renameable after reload.
        { std::ofstream out(light.file, std::ios::binary); out << renamed << "  \n"; }
        library.Load();
        Require(library.RenameLight(1, "Whitespace accepted"), "valid noncanonical whitespace does not prevent rename");
        // A non-directory archive prevents replacement before any original can be touched.
        const auto blocked = root / "blocked";
        std::filesystem::create_directory(blocked);
        PresetLibrary failure(blocked); failure.Load(); failure.SaveLight("Before", scene[0]);
        const auto original = read(failure.Lights()[0].file);
        { std::ofstream out(blocked / "Archived"); out << "sentinel"; }
        Require(!failure.RenameLight(0, "After") && failure.Lights()[0].name == "Before" &&
            read(failure.Lights()[0].file) == original, "backup failure preserves name and original bytes");
        std::cout << "Rename fixtures: " << root.string() << '\n';
    }

    void LanguagePresentation()
    {
        using namespace Localization;
        Set(Language::Japanese);
        LightingEditor editor;
        editor.SetNumeric(NumericField::Intensity, 2.1F); editor.FinishNumericEdit(); editor.Adopt();
        const auto scene = editor.GetScene(); const auto history = editor.History().back().label;
        Message warning{"画面遷移で停止しました。自動再点灯はしません。"};
        Require(std::string(warning.c_str()).starts_with("画面遷移"), "stored warning starts in Japanese");
        Set(Language::English);
        Require(std::string(Tr("影の補正")) == "Shadow bias", "shadow label has no question mark");
        Require(std::string(Tr("初期値")) == "Reset values" &&
            HistoryText("Light 1 : 調整を初期値へ") == "Light 1: Reset values",
            "value reset control and history are bilingual");
        Require(std::string(Tr("青色リムライト")) == "Rim lighting Blue", "blue rim sample has an English title");
        Require(std::string(Tr("暖色リムライト")) == "Rim lighting Warm", "warm rim sample normalizes the submitted English spelling");
        Require(std::string(Tr("暖色1灯＋青色2灯 / フェイスライト込み")) ==
            "One warm and two blue / includes face light", "blue rim sample description is bilingual");
        Require(std::string(Tr("暖色2灯＋白色1灯")) == "Two warm and one white", "warm rim sample description is bilingual");
        Require(std::string(Tr("キャンドル空間照明")) == "Candle Space" &&
            std::string(Tr("白色1灯＋ろうそく色2灯 / フェイスライト込み")) ==
                "One white and two candle lights / includes face light", "Candle Space sample is bilingual");
        Require(std::string(warning.c_str()).starts_with("Stopped"), "existing runtime warning follows language switch");
        Require(HistoryText("Light 1 : 強さ") == "Light 1: Intensity", "history actions translate at display time");
        Require(HistoryText("プリセット : 暖色") == "Preset: 暖色", "user preset names never auto-translate");
        Require(editor.GetScene() == scene && editor.History().back().label == history && editor.MatchesBookmark(),
            "language switch never changes lighting, history or bookmark");
        Require(StoredError("読み込めない登録が 3 件あります。元のファイルは保持しています。") ==
            "Unreadable entries: 3. Original files are kept.", "dynamic load warning translates safely");
        std::set<std::string> keys;
        const std::regex placeholders(R"(%[-+0-9.#]*[zlh]*[sdfugxX]|\{[^}]*\})");
        for (const auto& entry : Table()) {
            Require(keys.emplace(entry.japanese).second, "translation keys are unique");
            Require(entry.english[0] != '\0', "English translation is nonempty");
            const std::string ja(entry.japanese), en(entry.english);
            auto formats = [&](const std::string& text) {
                std::vector<std::string> found;
                for (std::sregex_iterator it(text.begin(), text.end(), placeholders), end; it != end; ++it) { found.push_back(it->str()); }
                return found;
            };
            Require(formats(ja) == formats(en), "translation preserves format argument order and types");
            const auto id = ja.find("###");
            if (id != std::string::npos) { Require(en.ends_with(ja.substr(id)), "translated popup preserves stable ID"); }
            Require(std::string(Tr(entry.japanese, Language::Japanese)) == ja &&
                std::string(Tr(entry.japanese, Language::English)) == en, "both language lookups match the table");
        }
        const auto root = std::filesystem::temp_directory_path() / ("sla-language-tests-" +
            std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        Require(std::filesystem::create_directory(root), "language fixtures exclusively owned");
        const auto path = root / u8"設定" / "ui-language.sla"; std::string error;
        Require(LoadPreference(path, error) && Current() == Language::English && !std::filesystem::exists(path),
            "first load defaults to English without creating files");
        Require(SavePreference(path, Language::English, error), "English preference saves atomically");
        Set(Language::Japanese);
        Require(LoadPreference(path, error) && Current() == Language::English, "English preference persists across restart");
        Require(SavePreference(path, Language::Japanese, error) && LoadPreference(path, error) &&
            Current() == Language::Japanese, "Japanese preference also persists");
        { std::ofstream out(path, std::ios::binary); out << "future version"; }
        Require(!LoadPreference(path, error) && !SavePreference(path, Language::English, error), "unknown preference file is not replaced");
        { std::ifstream in(path); std::string bytes{std::istreambuf_iterator<char>(in), {}}; Require(bytes == "future version", "invalid preference bytes preserved"); }
        const auto blocker = root / "not-a-directory";
        { std::ofstream out(blocker); out << "sentinel"; }
        Require(!SavePreference(blocker / "prefs", Language::English, error) && !error.empty(), "storage failure is reported");
        Set(Language::English);
        std::cout << "Language fixtures: " << root.string() << '\n';
    }

    void StorageMigration()
    {
        const auto root = std::filesystem::temp_directory_path() / ("sla-storage-tests-" +
            std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        Require(std::filesystem::create_directory(root), "storage fixtures exclusively owned");
        const auto legacy = root / u8"旧保存先";
        const auto preferred = root / u8"Data保存先";
        std::filesystem::create_directories(legacy / "Presets" / "Archived");
        { std::ofstream out(legacy / "ui-language.sla", std::ios::binary); out << "SLA LANGUAGE 1\nja\n"; }
        { std::ofstream out(legacy / "Presets" / "one.slaset", std::ios::binary); out << "one"; }
        { std::ofstream out(legacy / "Presets" / "Archived" / "old.slalight", std::ios::binary); out << "old"; }
        std::filesystem::create_directories(preferred / "Presets");
        { std::ofstream out(preferred / "Presets" / "one.slaset", std::ios::binary); out << "destination wins"; }

        const auto migrated = PrepareStorageLocation(preferred, legacy);
        auto read = [](const auto& path) { std::ifstream in(path, std::ios::binary); return std::string(std::istreambuf_iterator<char>(in), {}); };
        Require(migrated.root == preferred && !migrated.usingLegacy && migrated.importedFiles == 2,
            "writable Data storage imports missing legacy files once");
        Require(read(preferred / "ui-language.sla") == "SLA LANGUAGE 1\nja\n" &&
            read(preferred / "Presets" / "Archived" / "old.slalight") == "old",
            "language preference and Archived contents migrate together");
        Require(read(preferred / "Presets" / "one.slaset") == "destination wins",
            "migration never overwrites an existing destination file");
        Require(read(legacy / "Presets" / "one.slaset") == "one" &&
            read(preferred / "storage-v2.sla") == "SLA STORAGE 2\n", "legacy files remain and one-time marker is durable");
        { std::ofstream out(legacy / "Presets" / "late.slaset", std::ios::binary); out << "late"; }
        const auto second = PrepareStorageLocation(preferred, legacy);
        Require(second.importedFiles == 0 && !std::filesystem::exists(preferred / "Presets" / "late.slaset"),
            "migration marker prevents removed or later legacy files from reappearing");

        const auto blocked = root / "blocked";
        { std::ofstream out(blocked); out << "sentinel"; }
        const auto fallback = PrepareStorageLocation(blocked, legacy);
        Require(fallback.root == legacy && fallback.usingLegacy && !fallback.notice.empty() && read(blocked) == "sentinel",
            "unwritable Data path safely falls back without changing the blocker");
        const auto clean = PrepareStorageLocation(root / "clean", {});
        Require(clean.root == root / "clean" && !clean.usingLegacy &&
            read(clean.root / "storage-v2.sla") == "SLA STORAGE 2\n", "fresh install prepares Data storage without legacy input");
        std::cout << "Storage fixtures: " << root.string() << '\n';
    }

    void TargetSelectionAndUniformDiagrams()
    {
        using namespace ScreenshotLightingAssistant::UI;
        Require(kLightDisplayOrder == std::array<int, 4>{0, 1, 3, 2}, "display order is white warm blue candle without renumbering saved types");
        std::vector<TargetCandidate> list{{3, "B", 5}, {2, "A", 5}, {1, "near", 0}, {4, "far", 30},
            {5, "outside", 30.1F}, {0, "invalid", 1}, {6, "negative", -1}, {7, "nan", std::numeric_limits<float>::quiet_NaN()}};
        SortTargetCandidates(list);
        Require(list.size() == 4 && list[0].formID == 1 && list[1].formID == 2 && list[2].formID == 3 &&
            list[3].formID == 4, "nearby candidates bounded and stably sorted by distance then identity");
        for (std::uint32_t i = 10; i < 110; ++i) { list.push_back({i, "NPC", 10}); }
        SortTargetCandidates(list);
        Require(list.size() == 64 && list.back().formID == 70, "only nearest 64 candidates retained");
        Require(ValidTargetChoice(9, 9, 63, 64) && !ValidTargetChoice(8, 9, 0, 64) &&
            !ValidTargetChoice(9, 9, 64, 64) && !ValidTargetChoice(9, 9, 0, 0), "stale and out-of-range candidate clicks rejected");
        LightSession session; session.SetReady(true); auto scene = kScenePresets[2].scene;
        scene.face = {true, 1.5F, .8F, .1F}; session.Start(session.Get().epoch, scene);
        const auto epoch = session.Get().epoch;
        Require(session.Stop(epoch) && !session.Get().running && session.Get().lights == scene, "subject switch stop keeps exact settings");
        Require(!session.Submit(epoch, {}) && !session.Start(epoch, scene), "old subject frame cannot restart or edit the new target session");
        Require(session.Start(session.Get().epoch, scene), "explicit start works with fresh subject token");
        session.Block(true); session.Block(false);
        Require(!session.Get().running && session.Get().lights == scene, "loading keeps lighting settings but never auto-starts");

        for (float size : {140.F, 160.F, 184.F}) {
            const float scale = PresetDiagramScale(size);
            Require(std::abs(scale - size / 140.F) < .00001F, "preset diagram uses one uniform scale");
            for (int count = 1; count <= 3; ++count) {
                const auto radius = DiagramNodeRadius(140.F, count) * scale;
                Require(std::abs(radius / size - DiagramNodeRadius(140.F, count) / 140.F) < .00001F,
                    "small-circle proportion matches original 140px diagram");
                for (int i = 0; i < count; ++i) {
                    const auto badge = OccupantBadge(radius / scale, count, i);
                    Require((std::hypot(badge.x, badge.y) + badge.radius) * scale <= radius + .001F, "uniform scaling preserves clustered badge bounds");
                }
            }
            Require(18.2F * scale / size < .131F && 18.2F * scale / size > .129F, "target center keeps original 13 percent proportion");
            const auto layout = CalculatePresetCardLayout(440, 24, 168, 3);
            Require(layout.diagramY + layout.diagramSize + 15.F * PresetDiagramScale(layout.diagramSize) <= layout.height - 7.9F,
                "enlarged camera is included in layout reserve");
        }
    }

    void FacePlacementModes()
    {
        LightingEditor editor;
        const auto original = editor.GetScene();
        Require(original.face.basis == FaceLightBasis::Head, "new sessions default to head-facing mode");
        editor.SelectSlot(2);
        editor.SetFaceIntensity(.81F);
        editor.SetFaceBasis(FaceLightBasis::Camera);
        Require(!editor.HasPendingChanges() && editor.History().size() == 3, "mode change commits pending slider separately");
        const auto cameraScene = editor.GetScene();
        Require(cameraScene.lights == original.lights && !cameraScene.face.enabled && editor.SelectedSlot() == 2,
            "mode does not change manual lights, power or selection");
        Require(editor.History().back().label == "フェイスライト : カメラ基準", "camera mode history is explicit");
        editor.SetFaceBasis(FaceLightBasis::Camera);
        editor.SetFaceBasis(static_cast<FaceLightBasis>(99));
        Require(editor.GetScene() == cameraScene && editor.History().size() == 3, "same or invalid mode is a no-op");
        editor.Undo();
        Require(editor.GetScene().face.basis == FaceLightBasis::Head && editor.GetScene().face.intensity == .81F,
            "mode undo does not undo preceding intensity edit");
        editor.Redo(); editor.Adopt();
        editor.SetFaceBasis(FaceLightBasis::Head);
        editor.RestoreBookmark();
        Require(editor.GetScene() == cameraScene, "bookmark includes camera placement");
        for (std::size_t i = 0; i < kScenePresets.size(); ++i) {
            const auto before = editor.GetScene();
            PresetPicker picker; picker.Open(); Require(picker.Choose(i, editor), "sample chosen with head mode active");
            Require(editor.GetScene() == ExpectedPresetScene(kScenePresets[i], before), "sample honors its declared face scope");
        }
        auto headScene = cameraScene;
        headScene.face.basis = FaceLightBasis::Head;
        for (bool include : {false, true}) {
            for (auto basis : {FaceLightBasis::Camera, FaceLightBasis::Head}) {
                for (bool enabled : {false, true}) {
                    auto scene = kScenePresets[2].scene;
                    scene.face = {enabled, .92F, .78F, -.23F, basis};
                    SavedScene saved{"mode test", scene, include, {}};
                    const auto encoded = PresetLibrary::Encode(saved);
                    Require(encoded.starts_with(include && basis == FaceLightBasis::Head ? "SLA SCENE 2\n" : "SLA SCENE 1\n"),
                        "only included head mode requires new scene format");
                    const auto decoded = PresetLibrary::DecodeScene(encoded);
                    auto expected = scene;
                    if (!include) { expected.face = {}; expected.face.basis = FaceLightBasis::Camera; }
                    Require(decoded && decoded->scene == expected && decoded->includeFace == include,
                        "both face modes round trip, excluded face remains canonical legacy default");
                    const auto retainedFace = editor.GetScene().face;
                    Require(PresetLibrary::Apply(*decoded, editor), "saved placement mode applies");
                    Require(editor.GetScene().face == (include ? scene.face : retainedFace),
                        "face inclusion controls placement mode scope");
                    Require(!PresetLibrary::DecodeScene(encoded + "extra"), "both scene versions reject trailing content");

                    LightSession session; session.SetReady(true);
                    Require(session.Start(session.Get().epoch, scene), "both modes accepted at session start");
                    Require(LightSession::ShouldIlluminateFace(session.Get()) == enabled, "mode does not bypass face power");
                    auto next = scene;
                    next.face.basis = basis == FaceLightBasis::Camera ? FaceLightBasis::Head : FaceLightBasis::Camera;
                    const auto epoch = session.Get().epoch;
                    Require(session.Submit(epoch, next) && session.Get().lights == next, "live snapshot switches face mode");
                    Require(LightRegistrationSettings::From(*RuntimeSettings(scene, kFaceRuntimeSlot)) ==
                        LightRegistrationSettings::From(*RuntimeSettings(next, kFaceRuntimeSlot)), "mode changes position, not engine registration");
                    session.Stop(epoch);
                    Require(!session.Submit(epoch, scene) && !LightSession::ShouldIlluminateFace(session.Get()), "mode cannot restart stopped session");
                }
            }
        }
        for (int invalid : {-1, 2, 99}) {
            auto scene = headScene; scene.face.basis = static_cast<FaceLightBasis>(invalid);
            const auto prior = editor.GetScene();
            Require(!LightingEditor::IsValidScene(scene) && !editor.ApplyScene(scene, "bad mode") && editor.GetScene() == prior,
                "invalid mode rejects whole scene");
            LightSession session; session.SetReady(true);
            Require(!session.Start(session.Get().epoch, scene), "invalid mode rejects runtime start");
            Require(!PresetLibrary::DecodeScene(PresetLibrary::Encode(SavedScene{"bad", scene, true, {}})),
                "invalid serialized mode is rejected");
        }
        auto camera = headScene; camera.face.basis = FaceLightBasis::Camera;
        const auto legacy = PresetLibrary::Encode(SavedScene{"legacy", camera, true, {}});
        auto version2 = legacy; version2.replace(0, 11, "SLA SCENE 2");
        Require(!PresetLibrary::DecodeScene(version2), "v2 must contain mode field");
        const auto explicitCamera = PresetLibrary::DecodeScene(version2 + "0\n");
        Require(explicitCamera && explicitCamera->scene == camera, "explicit v2 camera is accepted");
        Require(!PresetLibrary::DecodeScene(version2 + "1.5\n"), "fractional mode is rejected");
        auto unknown = legacy; unknown.replace(0, 11, "SLA SCENE 3");
        Require(!PresetLibrary::DecodeScene(unknown), "unknown scene version rejected");

        const auto root = std::filesystem::temp_directory_path() / ("sla-face-mode-tests-" +
            std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        Require(std::filesystem::create_directory(root), "mode fixtures uniquely owned");
        auto read = [](const auto& path) { std::ifstream in(path, std::ios::binary); return std::string(std::istreambuf_iterator<char>(in), {}); };
        PresetLibrary library(root);
        Require(library.SaveScene("camera", camera, true) && library.SaveScene("head", headScene, true), "both formats save to disk");
        const auto legacyPath = library.Scenes()[0].file;
        const auto legacyBytes = read(legacyPath);
        library.Load();
        Require(library.Error().empty() && library.Scenes().size() == 2, "mixed v1/v2 library reloads");
        Require(library.Scenes()[0].scene == camera && library.Scenes()[1].scene == headScene, "disk reload retains both modes");
        Require(read(legacyPath) == legacyBytes, "loading new mode never rewrites old presets");
        Require(library.RenameScene(1, "顔の向き"), "head mode preset can be renamed");
        library.Load();
        Require(library.Scenes()[1].name == "顔の向き" && library.Scenes()[1].scene == headScene,
            "renaming preserves head mode after reload");
        std::cout << "Face mode fixtures: " << root.string() << '\n';
    }

    void HeadFaceGeometry()
    {
        const LightVector head{15, -21, 103};
        const float distance = .45F * kUnitsPerMetre;
        auto near = [](float a, float b) { return std::abs(a - b) < .002F; };
        const auto neutral = HeadFaceLightPosition(head, {0,1,0}, {0,0,1}, .15F);
        Require(neutral && near(neutral->x, head.x) && near(neutral->y, head.y + distance) &&
            near(neutral->z, head.z + .15F * kUnitsPerMetre), "reference head places light forward along local Y and up along Z");
        // Independently rotate the neutral displacement with yaw, pitch and roll.
        for (int yaw = -180; yaw <= 180; yaw += 30) {
            for (int pitch = -90; pitch <= 90; pitch += 30) {
                for (int roll = -90; roll <= 90; roll += 30) {
                    auto rotate = [&](LightVector p) {
                        const float y = yaw * std::numbers::pi_v<float> / 180;
                        const float t = pitch * std::numbers::pi_v<float> / 180;
                        const float r = roll * std::numbers::pi_v<float> / 180;
                        p = {p.x * std::cos(r) + p.z * std::sin(r), p.y, -p.x * std::sin(r) + p.z * std::cos(r)};
                        p = {p.x, p.y * std::cos(t) - p.z * std::sin(t), p.y * std::sin(t) + p.z * std::cos(t)};
                        return LightVector{p.x * std::cos(y) - p.y * std::sin(y), p.x * std::sin(y) + p.y * std::cos(y), p.z};
                    };
                    for (float height : {-.5F, 0.F, .15F, .5F}) {
                        const auto f = rotate({0,1,0}), u = rotate({0,0,1});
                        const auto expected = rotate({0,distance,height * kUnitsPerMetre});
                        const auto actual = HeadFaceLightPosition(head, f, u, height);
                        Require(actual && near(actual->x - head.x, expected.x) && near(actual->y - head.y, expected.y) &&
                            near(actual->z - head.z, expected.z), "head geometry follows combined yaw/pitch/roll");
                        Require(near(std::hypot(actual->x-head.x, actual->y-head.y, actual->z-head.z),
                            std::hypot(distance, height*kUnitsPerMetre)), "head rotation does not change light distance");
                        const auto shifted = HeadFaceLightPosition({head.x+70, head.y-30, head.z+10}, f, u, height);
                        Require(shifted && near(shifted->x-actual->x,70) && near(shifted->y-actual->y,-30) &&
                            near(shifted->z-actual->z,10), "head placement follows translation without camera input");
                    }
                }
            }
        }
        for (const auto bad : {LightVector{}, LightVector{NAN,0,0}, LightVector{0,INFINITY,0}}) {
            Require(!HeadFaceLightPosition(head,bad,{0,0,1},0), "invalid head forward rejected");
            Require(!HeadFaceLightPosition(head,{0,1,0},bad,0), "invalid head up rejected");
        }
        Require(!HeadFaceLightPosition(head,{0,1,0},{0,1,0},0), "parallel head axes rejected");
        Require(!HeadFaceLightPosition({NAN,0,0},{0,1,0},{0,0,1},0), "invalid head translation rejected");
        for (float bad : {-.501F,.501F,NAN,INFINITY}) {
            Require(!HeadFaceLightPosition(head,{0,1,0},{0,0,1},bad), "invalid head-mode height rejected");
        }
        const auto cameraSide = FaceLightPosition(head,{head.x+100,head.y,head.z},.15F);
        Require(cameraSide && near(cameraSide->x,head.x+distance) && near(cameraSide->y,head.y),
            "camera mode remains camera-relative and distinct from head mode");
        Require(!FaceLightPosition(head,head,.15F) && neutral.has_value(), "head mode needs no camera separation to position face fill");
    }

    void InvalidInputsAndNoOps()
    {
        LightingEditor editor;
        const auto initial = editor.GetScene();
        editor.SelectSlot(-1);
        editor.SelectSlot(3);
        editor.SetDirection(-1);
        editor.SetDirection(8);
        editor.SetType(-1);
        editor.SetType(kLightTypeCount);
        editor.SetNumeric(NumericField::Intensity, std::numeric_limits<float>::quiet_NaN());
        editor.SetNumeric(NumericField::Range, std::numeric_limits<float>::infinity());
        editor.SetNumeric(static_cast<NumericField>(99), 1.0F);
        Require(editor.GetScene() == initial && editor.SelectedSlot() == 0, "invalid inputs preserve state");
        editor.SetType(0);
        editor.SetDirection(1);
        editor.SetShadow(true);
        editor.SetNumeric(NumericField::Intensity, 1.0F);
        editor.FinishNumericEdit();
        Require(editor.History().size() == 1, "no-op settings do not create history");
        editor.SetNumeric(NumericField::Intensity, 99.0F);
        editor.SetNumeric(NumericField::Range, -2.0F);
        editor.SetNumeric(NumericField::Distance, 20.0F);
        editor.SetNumeric(NumericField::Height, -20.0F);
        editor.FinishNumericEdit();
        Require(editor.CurrentLight().intensity == 3.0F && editor.CurrentLight().range == 0.5F, "strength and range bounds enforced");
        Require(editor.CurrentLight().distance == 8.0F && editor.CurrentLight().heightOffset == -2.0F, "position bounds enforced");
        editor.SelectSlot(1);
        const auto empty = editor.CurrentLight();
        editor.SetShadow(false);
        editor.SetNumeric(NumericField::Intensity, 2.0F);
        editor.FinishNumericEdit();
        Require(editor.CurrentLight() == empty, "unplaced lights cannot be adjusted by disabled controls");
    }

    void PersistentFacePreferences()
    {
        PersistentFaceSettings settings;
        Require(ValidPersistentFaceSettings(settings), "persistent face defaults are valid");
        Require(settings.featureEnabled && !settings.enabled && settings.hotkey == kNoPersistentFaceHotkey &&
            settings.basis == FaceLightBasis::Head, "persistent face defaults are safe and unassigned");

        settings.featureEnabled = false;
        settings.enabled = true;
        settings.hotkey = 0x42;
        settings.gamepadHotkey = 0x0100;
        settings.intensity = 1.25F;
        settings.range = 1.75F;
        settings.heightOffset = -0.20F;
        settings.basis = FaceLightBasis::Head;
        const auto encoded = EncodePersistentFaceSettings(settings);
        PersistentFaceSettings decoded;
        Require(!encoded.empty() && DecodePersistentFaceSettings(encoded, decoded) && decoded == settings,
            "persistent face settings round-trip exactly");
        Require(!DecodePersistentFaceSettings(encoded + "unknown 1\n", decoded),
            "unknown persistent settings are rejected");
        auto duplicate = encoded + "enabled 1\n";
        Require(!DecodePersistentFaceSettings(duplicate, decoded), "duplicate persistent settings are rejected");
        auto invalidHotkey = settings;
        invalidHotkey.hotkey = kMaximumKeyboardScanCode + 1;
        Require(!ValidPersistentFaceSettings(invalidHotkey) && EncodePersistentFaceSettings(invalidHotkey).empty(),
            "out-of-range keyboard scan code is rejected");
        auto invalidGamepad = settings;
        invalidGamepad.gamepadHotkey = 0x0800;
        Require(!ValidPersistentFaceSettings(invalidGamepad) && EncodePersistentFaceSettings(invalidGamepad).empty(),
            "unknown gamepad button code is rejected");
        auto invalidFloat = settings;
        invalidFloat.intensity = std::numeric_limits<float>::quiet_NaN();
        Require(!ValidPersistentFaceSettings(invalidFloat), "nonfinite persistent values are rejected");

        const std::string version1 =
            "SLA PERSISTENT FACE 1\n"
            "enabled 1\n"
            "hotkey 66\n"
            "intensity 1.250000\n"
            "range 1.750000\n"
            "height -0.200000\n"
            "basis camera\n";
        PersistentFaceSettings migrated;
        Require(DecodePersistentFaceSettings(version1, migrated) && migrated.featureEnabled && migrated.enabled &&
            migrated.hotkey == 66 && migrated.gamepadHotkey == kNoPersistentFaceHotkey &&
            migrated.basis == FaceLightBasis::Head,
            "version 1 persistent settings migrate enabled with gamepad unassigned and head-facing placement");

        const std::string version2 =
            "SLA PERSISTENT FACE 2\n"
            "enabled 1\n"
            "hotkey 66\n"
            "gamepad 256\n"
            "intensity 1.250000\n"
            "range 1.750000\n"
            "height -0.200000\n"
            "basis camera\n";
        Require(DecodePersistentFaceSettings(version2, migrated) && migrated.featureEnabled && migrated.enabled &&
            migrated.hotkey == 66 && migrated.gamepadHotkey == 256 &&
            migrated.basis == FaceLightBasis::Head,
            "version 2 persistent settings migrate enabled and head-facing while preserving both hotkeys");

        PersistentGamepadHoldGate hold;
        Require(!hold.Update(true, 0.0F) && !hold.Update(true, 0.59F),
            "gamepad short press does not toggle");
        Require(hold.Update(true, kPersistentGamepadHoldSeconds) && !hold.Update(true, 0.90F),
            "gamepad hold toggles once at the threshold");
        Require(!hold.Update(false, 0.90F) && !hold.Update(true, 0.0F) &&
            hold.Update(true, kPersistentGamepadHoldSeconds),
            "gamepad release rearms the next long press");
        hold.Reset();
        Require(!hold.Update(true, std::numeric_limits<float>::quiet_NaN()),
            "nonfinite gamepad hold duration fails closed");

        const auto root = std::filesystem::temp_directory_path() / ("sla-persistent-face-tests-" +
            std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        Require(std::filesystem::create_directory(root), "persistent settings fixture is uniquely owned");
        const auto path = root / "persistent-face.sla";
        std::string error;
        PersistentFaceSettings missing;
        missing.enabled = true;
        Require(LoadPersistentFaceSettings(path, missing, error) && missing == PersistentFaceSettings{} && error.empty(),
            "missing persistent settings use safe defaults without creating a file");
        Require(SavePersistentFaceSettings(path, settings, error) && error.empty(),
            "persistent settings save atomically");
        PersistentFaceSettings loaded;
        Require(LoadPersistentFaceSettings(path, loaded, error) && loaded == settings,
            "persistent settings reload across process sessions");

        std::ifstream beforeStream(path, std::ios::binary);
        const std::string before{std::istreambuf_iterator<char>(beforeStream), {}};
        {
            std::ofstream corrupt(path, std::ios::binary | std::ios::trunc);
            corrupt << "not a valid SLA setting\n";
        }
        PersistentFaceSettings preserved;
        Require(!LoadPersistentFaceSettings(path, preserved, error) && !error.empty(),
            "invalid persistent settings report an error");
        Require(!SavePersistentFaceSettings(path, settings, error),
            "invalid existing settings are not silently overwritten");
        std::ifstream corruptStream(path, std::ios::binary);
        const std::string corruptBytes{std::istreambuf_iterator<char>(corruptStream), {}};
        Require(corruptBytes == "not a valid SLA setting\n" && corruptBytes != before,
            "invalid persistent settings remain untouched for diagnosis");
        std::cout << "Persistent face fixtures: " << root.string() << '\n';
    }
}

int main()
{
    try {
        IndependentSlotsAndDeletion();
        DiagramSelectionAndExplicitMoves();
        CoalescedSliders();
        HistoryAndBookmarks();
        InvalidInputsAndNoOps();
        LightPower();
        ScenePresetSelection();
        CompactCardLayout();
        RuntimeSessionBoundaries();
        RelativeLightGeometry();
        ShadowBiasStateAndRegistration();
        SpotlightStateAndRegistration();
        ThreeLightScenes();
        SpotlightGeometry();
        FinePositionEditing();
        ResetCurrentLightValues();
        FinePositionGeometry();
        PositionWindowSizing();
        CompactPositionControls();
        ManagementActionRows();
        CompactBookmarkRows();
        PresetComparisonSession();
        PresetGeometryMemory();
        SharedPositionEditor();
        BlueAndWideControls();
        IndependentFaceEditing();
        FaceRuntimeAndGeometry();
        AdjustableFaceRange();
        AdjustableFaceHeight();
        FacePlacementModes();
        HeadFaceGeometry();
        ColorAndRecipeEditing();
        PresetSerialization();
        PresetFiles();
        DiagramContrastAndOccupants();
        LibraryViewPersistence();
        RenamePersistence();
        LanguagePresentation();
        StorageMigration();
        TargetSelectionAndUniformDiagrams();
        PersistentFacePreferences();
        std::cout << "PASS: " << checks << " lighting-state/layout/runtime-model checks\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "FAIL after " << checks << " checks (" << lastCheck << "): " << error.what() << '\n';
        return 1;
    }
}
