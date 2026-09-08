#include "PCH.h"

#include "UI.h"
#include "LightingState.h"
#include "ScenePresets.h"
#include "PresetCardLayout.h"
#include "PositionPad.h"
#include "PositionWindowLayout.h"
#include "RuntimeLight.h"
#include "RuntimeModel.h"
#include "PresetLibrary.h"
#include "LightVisual.h"
#include "Localization.h"
#include "StorageLocation.h"
#include <cstdarg>
#include <cstdio>

#include <SKSEMenuFramework.h>
#include <format>

namespace ScreenshotLightingAssistant::UI
{
    namespace
    {
        using namespace Localization;
        using ImGuiMCP::ImDrawList;
        using ImGuiMCP::ImU32;
        using ImGuiMCP::ImVec2;

        struct Direction
        {
            const char* id;
            float angleDegrees;
        };

        constexpr std::array<Direction, 8> kDirections{ {
            { "back", -90.0F },
            { "right_back", -45.0F },
            { "right", 0.0F },
            { "right_front", 45.0F },
            { "front", 90.0F },
            { "left_front", 135.0F },
            { "left", 180.0F },
            { "left_back", 225.0F },
        } };

        LightingEditor g_editor;
        PresetPicker g_presetPicker;
        PresetWindowGeometry g_presetGeometry;
        bool g_focusPresetWindow{};
        bool g_registered{ false };
        bool g_positionWindowOpen{ false };
        bool g_focusPositionWindow{ false };
        std::unique_ptr<PresetLibrary> g_library;
        bool g_openPresetRequested{};
        bool g_openLightLibraryRequested{};
        bool g_managePresets{};
        bool g_openRegistrationRequested{};
        bool g_registerScene{};
        bool g_saveFace{};
        Scene g_registrationSnapshot{};
        int g_registrationSlot{};
        std::array<char, 121> g_registrationName{};
        std::string g_registrationError;
        std::string g_libraryNotice;
        std::string g_libraryLoadError;
        std::filesystem::path g_languagePath;
        std::string g_languageError;

        std::filesystem::path PreferredStorageRoot()
        {
            std::array<wchar_t, 32768> executable{};
            const auto length = GetModuleFileNameW(nullptr, executable.data(), static_cast<DWORD>(executable.size()));
            const auto gameRoot = length && length < executable.size() ?
                std::filesystem::path(std::wstring(executable.data(), length)).parent_path() :
                std::filesystem::current_path();
            return gameRoot / "Data" / "SKSE" / "Plugins" / "ScreenshotLightingAssistant";
        }

        std::array<std::uint8_t, 3> DisplayTint(const LightSettings& light)
        {
            return DiagramTint(light);
        }

        constexpr ImU32 Color(std::uint8_t a_red, std::uint8_t a_green, std::uint8_t a_blue, std::uint8_t a_alpha = 255)
        {
            return static_cast<ImU32>(a_red) |
                   (static_cast<ImU32>(a_green) << 8U) |
                   (static_cast<ImU32>(a_blue) << 16U) |
                   (static_cast<ImU32>(a_alpha) << 24U);
        }

        void Tooltip(const char* format, ...)
        {
            std::array<char, 8192> buffer{};
            va_list args;
            va_start(args, format);
            std::vsnprintf(buffer.data(), buffer.size(), format, args);
            va_end(args);
            if (ImGuiMCP::BeginTooltip()) {
                ImGuiMCP::PushTextWrapPos(ImGuiMCP::GetFontSize() * 34.0F);
                ImGuiMCP::TextUnformatted(buffer.data());
                ImGuiMCP::PopTextWrapPos();
                ImGuiMCP::EndTooltip();
            }
        }

        void DrawCenteredText(ImDrawList* a_drawList, const ImVec2& a_center, const char* a_text, ImU32 a_color, float a_maxWidth = 80.0F, float textScale = 1.0F)
        {
            const auto textSize = ImGuiMCP::CalcTextSize(a_text);
            const float scale = std::min(textScale, a_maxWidth / std::max(1.0F, textSize.x));
            const ImVec2 textPosition{
                a_center.x - textSize.x * scale * 0.5F,
                a_center.y - textSize.y * scale * 0.5F
            };
            ImGuiMCP::ImDrawListManager::AddText(a_drawList, ImGuiMCP::GetFont(), ImGuiMCP::GetFontSize() * scale, textPosition, a_color, a_text);
        }

        void DrawCameraIcon(ImDrawList* a_drawList, const ImVec2& a_center, float scale = 1.0F)
        {
            constexpr auto white = Color(160, 215, 240);
            const ImVec2 bodyMin{ a_center.x - 8.0F * scale, a_center.y - 4.0F * scale };
            const ImVec2 bodyMax{ a_center.x + 8.0F * scale, a_center.y + 6.0F * scale };
            const ImVec2 topMin{ a_center.x - 3.5F * scale, a_center.y - 7.0F * scale };
            const ImVec2 topMax{ a_center.x + 2.5F * scale, a_center.y - 4.0F * scale };
            ImGuiMCP::ImDrawListManager::AddRect(a_drawList, bodyMin, bodyMax, white, 2.0F * scale, 0, 1.5F * scale);
            ImGuiMCP::ImDrawListManager::AddRectFilled(a_drawList, topMin, topMax, white, 1.0F * scale, 0);
            ImGuiMCP::ImDrawListManager::AddCircle(a_drawList, { a_center.x, a_center.y + 1.0F * scale }, 3.0F * scale, white, 16, 1.5F * scale);
        }

        void DrawOccupants(ImDrawList* drawList, const ImVec2& center, float radius, const Scene& scene, int direction, float diagramScale = 1.0F)
        {
            int count = 0;
            for (int slot = 0; slot < 3; ++slot) {
                count += scene[slot].placed && scene[slot].direction == direction ? 1 : 0;
            }
            int ordinal = 0;
            for (int slot = 0; slot < 3; ++slot) {
                const auto& light = scene[slot];
                if (!light.placed || light.direction != direction) { continue; }
                auto badge = OccupantBadge(radius / diagramScale, count, ordinal++);
                badge.x *= diagramScale; badge.y *= diagramScale; badge.radius *= diagramScale;
                const ImVec2 point{center.x + badge.x, center.y + badge.y};
                const auto rgb = DisplayTint(light);
                const auto numberColor = UseBlackNumber(rgb) ? Color(0, 0, 0) : Color(255, 255, 255);
                ImGuiMCP::ImDrawListManager::AddCircleFilled(drawList, point, badge.radius, Color(rgb[0], rgb[1], rgb[2]), 32);
                const auto label = std::to_string(slot + 1);
                // Constrain height as well as width so clustered digits stay inside badges.
                const auto size = ImGuiMCP::CalcTextSize(label.c_str());
                const float scale = std::min({diagramScale, badge.radius * 1.45F / size.y, badge.radius * 1.25F / size.x});
                ImGuiMCP::ImDrawListManager::AddText(drawList, ImGuiMCP::GetFont(), ImGuiMCP::GetFontSize() * scale,
                    {point.x - size.x * scale * .5F, point.y - size.y * scale * .5F}, numberColor, label.c_str());
                if (!light.enabled) {
                    ImGuiMCP::ImDrawListManager::AddLine(drawList,
                        {point.x - badge.radius * .55F, point.y + badge.radius * .6F},
                        {point.x + badge.radius * .55F, point.y - badge.radius * .6F}, numberColor, 1.3F * diagramScale);
                }
            }
        }

        void DrawDirectionDiagram(const Scene& a_scene, const ImVec2& origin, float canvasSize, bool a_interactive)
        {
            const float diagramScale = a_interactive ? 1.0F : PresetDiagramScale(canvasSize);
            const float ringRadius = canvasSize * 0.36F;
            const ImVec2 center{ origin.x + canvasSize * 0.5F, origin.y + canvasSize * 0.5F };
            auto* drawList = ImGuiMCP::GetWindowDrawList();

            ImGuiMCP::ImDrawListManager::AddCircle(drawList, center, ringRadius, Color(55, 68, 82), 96, 3.0F * diagramScale);

            for (std::size_t index = 0; index < kDirections.size(); ++index) {
                const int count = static_cast<int>(std::count_if(a_scene.begin(), a_scene.end(), [&](const auto& light) {
                    return light.placed && light.direction == static_cast<int>(index);
                }));
                const float nodeRadius = DiagramNodeRadius(a_interactive ? canvasSize : kPresetDiagramReferenceSize, count) * diagramScale;
                const auto& direction = kDirections[index];
                const float angle = direction.angleDegrees * std::numbers::pi_v<float> / 180.0F;
                const ImVec2 nodeCenter{
                    center.x + std::cos(angle) * ringRadius,
                    center.y + std::sin(angle) * ringRadius
                };

                bool hovered = false;
                if (a_interactive) {
                    ImGuiMCP::SetCursorScreenPos({ nodeCenter.x - nodeRadius, nodeCenter.y - nodeRadius });
                    ImGuiMCP::PushID(static_cast<int>(index));
                    if (ImGuiMCP::InvisibleButton(direction.id, { nodeRadius * 2.0F, nodeRadius * 2.0F })) {
                        g_editor.ActivateDirection(static_cast<int>(index));
                    }
                    hovered = ImGuiMCP::IsItemHovered();
                    // Keep the per-node popup inside the same ID scope as the hit target.
                    // Default InvisibleButton uses the left button only: right-click does
                    // not select an occupant or move a light before the menu is confirmed.
                    if (ImGuiMCP::BeginPopupContextItem("move_here")) {
                        const bool occupied = g_editor.DirectionSelection(static_cast<int>(index)).has_value();
                        const auto label = Fmt("Light {} : {}", g_editor.SelectedSlot() + 1,
                            !g_editor.CurrentLight().placed ? Tr("ここを配置予定位置にする") :
                            occupied ? Tr("ここへ重ねて移動") : Tr("ここへ移動"));
                        if (ImGuiMCP::MenuItem(label.c_str(), nullptr, false,
                                g_editor.CurrentLight().direction != static_cast<int>(index))) {
                            g_editor.SetDirection(static_cast<int>(index));
                        }
                        ImGuiMCP::EndPopup();
                    }
                    if (hovered) {
                        if (const auto slot = g_editor.DirectionSelection(static_cast<int>(index))) {
                            Tooltip(Tr("クリックで Light %d を編集（位置・点灯状態は変えません）。\n同じ丸の番号はクリックごとに切り替えます。\n右クリックで選択中のライトをこの方向へ移動します。\n図は基本方向です。水色の点は微調整あり。距離・高さ・微調整は各灯で保持します。"), *slot + 1);
                        } else {
                            Tooltip(Tr("クリックで Light %d の%sを変更します。\n距離・高さ・微調整は保持します。\n点灯状態は番号横のマークで切り替えます。"), g_editor.SelectedSlot() + 1,
                                g_editor.CurrentLight().placed ? Tr("配置位置") : Tr("配置予定位置（まだ点灯しません）"));
                        }
                    }
                    ImGuiMCP::PopID();
                }

                const bool selected = a_interactive &&
                    g_editor.CurrentLight().direction == static_cast<int>(index);
                if (selected) {
                    ImGuiMCP::ImDrawListManager::AddCircleFilled(drawList, nodeCenter, nodeRadius + 4.0F, Color(235, 165, 45, 45), 48);
                }
                const auto fill = hovered ? Color(43, 55, 69) : Color(26, 33, 43);
                const auto outline = selected ? Color(255, 202, 102) : (hovered ? Color(107, 139, 166) : Color(78, 94, 111));
                ImGuiMCP::ImDrawListManager::AddCircleFilled(drawList, nodeCenter, nodeRadius, fill, 48);
                ImGuiMCP::ImDrawListManager::AddCircle(drawList, nodeCenter, nodeRadius, outline, 48, selected ? 2.0F : diagramScale);

                DrawOccupants(drawList, nodeCenter, nodeRadius, a_scene, static_cast<int>(index), diagramScale);
                for (const auto& light : a_scene) {
                    if (light.placed && light.direction == static_cast<int>(index) && HasFinePosition(light.fine)) {
                        ImGuiMCP::ImDrawListManager::AddCircleFilled(drawList,
                            { nodeCenter.x + nodeRadius * 0.72F, nodeCenter.y - nodeRadius * 0.72F },
                            4.0F * diagramScale, Color(118, 214, 233), 16);
                        break;
                    }
                }
            }

            const float centerRadius = std::min(44.0F, (a_interactive ? canvasSize : kPresetDiagramReferenceSize) * 0.13F) * diagramScale;
            ImGuiMCP::ImDrawListManager::AddCircleFilled(drawList, center, centerRadius * 1.25F, Color(39, 48, 59), 64);
            ImGuiMCP::ImDrawListManager::AddCircleFilled(drawList, center, centerRadius, Color(4, 8, 13), 64);
            ImGuiMCP::ImDrawListManager::AddCircle(drawList, center, centerRadius, Color(111, 133, 156), 64, 2.0F * diagramScale);
            const float lineOffset = a_interactive ? ImGuiMCP::GetFontSize() * 0.52F : 0.0F;
            DrawCenteredText(drawList, { center.x, center.y - lineOffset }, Tr("対象"), Color(238, 243, 248), centerRadius * 1.6F, diagramScale);
            if (a_interactive) {
                DrawCenteredText(drawList, { center.x, center.y + lineOffset }, "Actor", Color(152, 189, 220), centerRadius * 1.6F);
            }

            if (!a_interactive) {
                DrawCameraIcon(drawList, { center.x, origin.y + canvasSize + 8.0F * diagramScale }, diagramScale);
            }
        }

        float ButtonWidth(const char* a_label);
        void DrawPositionWindowButton();

        void DrawFaceControls(const ImVec2& origin, float canvas, bool& anyActive)
        {
            const auto layout = CalculateFaceControls(canvas);
            auto* drawList = ImGuiMCP::GetWindowDrawList();
            const auto iconButton = [&](const char* id, const WindowRectangle& rect, bool selected) {
                const ImVec2 top{ origin.x + rect.x, origin.y + rect.y };
                ImGuiMCP::SetCursorScreenPos(top);
                const bool clicked = ImGuiMCP::InvisibleButton(id, { rect.width, rect.height });
                const bool hovered = ImGuiMCP::IsItemHovered() || ImGuiMCP::IsItemFocused();
                const ImVec2 end{ top.x + rect.width, top.y + rect.height };
                ImGuiMCP::ImDrawListManager::AddRectFilled(drawList, top, end,
                    selected ? Color(112, 80, 32) : hovered ? Color(43, 55, 69) : Color(26, 33, 43), 4.0F, 0);
                ImGuiMCP::ImDrawListManager::AddRect(drawList, top, end,
                    selected ? Color(255, 202, 102) : hovered ? Color(150, 189, 220) : Color(78, 94, 111), 4.0F, 0, 1.5F);
                return clicked;
            };
            if (iconButton("##face_power", layout.toggle, g_editor.GetScene().face.enabled)) {
                g_editor.ToggleFaceLight();
            }
            const ImVec2 face{ origin.x + layout.toggle.x + 16.0F, origin.y + layout.toggle.y + 16.0F };
            const auto tint = g_editor.GetScene().face.enabled ? Color(255, 224, 155) : Color(137, 151, 167);
            ImGuiMCP::ImDrawListManager::AddCircle(drawList, { face.x, face.y - 4.0F }, 6.0F, tint, 24, 1.5F);
            ImGuiMCP::ImDrawListManager::AddLine(drawList, { face.x - 10.0F, face.y + 9.0F }, { face.x - 5.0F, face.y + 3.0F }, tint, 1.5F);
            ImGuiMCP::ImDrawListManager::AddLine(drawList, { face.x - 5.0F, face.y + 3.0F }, { face.x + 5.0F, face.y + 3.0F }, tint, 1.5F);
            ImGuiMCP::ImDrawListManager::AddLine(drawList, { face.x + 5.0F, face.y + 3.0F }, { face.x + 10.0F, face.y + 9.0F }, tint, 1.5F);
            if (!g_editor.GetScene().face.enabled) {
                ImGuiMCP::ImDrawListManager::AddLine(drawList, { face.x - 11.0F, face.y + 11.0F }, { face.x + 11.0F, face.y - 11.0F }, tint, 1.5F);
            }
            if (ImGuiMCP::IsItemHovered()) {
                Tooltip(Tr("フェイスライト : %s / 強さ %.2f\nクリックでON/OFF。ライト1/2/3や編集対象は変更しません。\n白・影なしの補助光です。隣の設定からカメラ基準／顔の向き基準を選べます。\n停止中は設定だけ変更します。撮影ライトを開始すると反映します。\n頭や配置方向を取得できない場合は待機します。"),
                    g_editor.GetScene().face.enabled ? "ON" : "OFF", g_editor.GetScene().face.intensity);
            }
            if (iconButton("##face_settings_button", layout.settings, false)) {
                g_editor.FinishNumericEdit();
                ImGuiMCP::OpenPopup("face_settings");
            }
            const ImVec2 settings{ origin.x + layout.settings.x + 16.0F, origin.y + layout.settings.y + 16.0F };
            for (int row = 0; row < 3; ++row) {
                const float y = settings.y + (row - 1) * 7.0F;
                ImGuiMCP::ImDrawListManager::AddLine(drawList, { settings.x - 10.0F, y }, { settings.x + 10.0F, y }, Color(180, 201, 218), 1.5F);
                ImGuiMCP::ImDrawListManager::AddCircleFilled(drawList, { settings.x + (row % 2 ? 4.0F : -4.0F), y }, 2.5F, Color(180, 201, 218), 12);
            }
            if (ImGuiMCP::IsItemHovered()) { Tooltip(Tr("フェイスライトの配置基準・強さ・光の範囲・上下を調整します。開くだけでは点灯・設定を変えません。")); }
            if (ImGuiMCP::BeginPopup("face_settings")) {
                ImGuiMCP::TextUnformatted(Tr("フェイスライト（影なし）"));
                if (ImGuiMCP::RadioButton(Tr("カメラ基準"), g_editor.GetScene().face.basis == FaceLightBasis::Camera)) {
                    g_editor.SetFaceBasis(FaceLightBasis::Camera);
                }
                if (ImGuiMCP::IsItemHovered()) {
                    Tooltip(Tr("従来の配置です。頭からカメラ側へ配置し、高さはワールドの上下を基準にします。\nカメラが頭に近すぎる場合はフェイスライトだけ待機します。"));
                }
                ImGuiMCP::SameLine();
                if (ImGuiMCP::RadioButton(Tr("顔の向き基準"), g_editor.GetScene().face.basis == FaceLightBasis::Head)) {
                    g_editor.SetFaceBasis(FaceLightBasis::Head);
                }
                if (ImGuiMCP::IsItemHovered()) {
                    Tooltip(Tr("頭の向きに合わせて顔の前方へ配置します。横向き・上下・傾きに追従し、高さも頭の向きを基準にします。\n通常3灯・カメラ・キャラクターの向きは変えません。頭の向きを取得できない場合はフェイスライトだけ待機します。\n方式は履歴・栞・フェイス込みプリセットに含まれます。"));
                }
                float value = g_editor.GetScene().face.intensity;
                ImGuiMCP::SetNextItemWidth(std::clamp(ImGuiMCP::GetFontSize() * 7.0F, 150.0F, 300.0F));
                const bool changed = ImGuiMCP::SliderFloat(Tr("強さ##face"), &value, 0.0F, kFaceIntensityMax, "%.2f");
                if (ImGuiMCP::IsItemActivated()) { g_editor.FinishNumericEdit(); }
                if (changed) { g_editor.SetFaceIntensity(value); }
                anyActive = ImGuiMCP::IsItemActive() || anyActive;
                if (ImGuiMCP::IsItemDeactivatedAfterEdit()) { g_editor.FinishNumericEdit(); }
                if (ImGuiMCP::IsItemHovered()) {
                    Tooltip(Tr("強さは0〜3.00、初期値は0.35です。明るい場所では1.00以上にも調整できます。\nOFF中も調整でき、勝手に点灯しません。0で発光しません。\n戻る／進む・栞の登録は、この設定も含みます。フェイスを含めて登録したセット以外では保持します。"));
                }
                float range = g_editor.GetScene().face.range;
                ImGuiMCP::SetNextItemWidth(std::clamp(ImGuiMCP::GetFontSize() * 7.0F, 150.0F, 300.0F));
                const bool rangeChanged = ImGuiMCP::SliderFloat(Tr("光の範囲##face"), &range, kFaceRangeMin, kFaceRangeMax, "%.2f m");
                if (ImGuiMCP::IsItemActivated()) { g_editor.FinishNumericEdit(); }
                if (rangeChanged) { g_editor.SetFaceRange(range); }
                anyActive = ImGuiMCP::IsItemActive() || anyActive;
                if (ImGuiMCP::IsItemDeactivatedAfterEdit()) { g_editor.FinishNumericEdit(); }
                if (ImGuiMCP::IsItemHovered()) {
                    Tooltip(Tr("フェイスライトの届く範囲です（0.5〜2.0m / 初期1.2m）。\n範囲だけを変え、強さ・位置・ON/OFFとライト1/2/3は保持します。\n狭めると顔への明るさも変わるため、強さと合わせて調整してください。\n顔だけに限定する機能ではなく、近くの髪や服にも光が届きます。"));
                }
                float height = g_editor.GetScene().face.heightOffset;
                ImGuiMCP::SetNextItemWidth(std::clamp(ImGuiMCP::GetFontSize() * 7.0F, 150.0F, 300.0F));
                const bool heightChanged = ImGuiMCP::SliderFloat(Tr("上下 (+上)##face"), &height, kFaceHeightMin, kFaceHeightMax, "%+.2f m");
                if (ImGuiMCP::IsItemActivated()) { g_editor.FinishNumericEdit(); }
                if (heightChanged) { g_editor.SetFaceHeight(height); }
                anyActive = ImGuiMCP::IsItemActive() || anyActive;
                if (ImGuiMCP::IsItemDeactivatedAfterEdit()) { g_editor.FinishNumericEdit(); }
                if (ImGuiMCP::IsItemHovered()) {
                    Tooltip(Tr("頭を基準にした光源の上下です（-0.50〜+0.50m / 初期+0.15m）。\nカメラ基準ではワールドの上、顔の向き基準では頭の上方向へ動かします。\n光源を上げると顔までの距離も変わります。光の範囲・強さと合わせて調整してください。\nライト1/2/3の高さ・微調整、フェイスライトのON/OFFは変えません。"));
                }
                ImGuiMCP::EndPopup();
            }
        }

        void DrawDirectionSelector(float a_canvasSize, bool& anyActive)
        {
            const float canvasSize = std::clamp(a_canvasSize, 310.0F, 430.0F);
            const auto origin = ImGuiMCP::GetCursorScreenPos();
            DrawDirectionDiagram(g_editor.GetScene(), origin, canvasSize, true);
            DrawFaceControls(origin, canvasSize, anyActive);
            const auto footer = CalculateDiagramFooter(canvasSize, ImGuiMCP::GetFrameHeight());
            DrawCameraIcon(ImGuiMCP::GetWindowDrawList(), { origin.x + footer.cameraX, origin.y + footer.cameraY });
            // Reserve the camera row, followed by the left-column action row.
            ImGuiMCP::SetCursorScreenPos({ origin.x, origin.y + footer.height });
            ImGuiMCP::Dummy({ canvasSize, 0.0F });
        }

        float ButtonWidth(const char* a_label)
        {
            return ImGuiMCP::CalcTextSize(a_label, nullptr, true).x + ImGuiMCP::GetStyle()->FramePadding.x * 2.0F;
        }

        void NextAction(ActionRowLayout& row, const char* label)
        {
            if (row.Place(ButtonWidth(label))) { ImGuiMCP::SameLine(0, row.gap); }
        }

        bool SelectableButton(const char* a_label, bool a_selected = false, float a_minWidth = 0.0F, bool a_danger = false)
        {
            if (a_selected || a_danger) {
                ImGuiMCP::PushStyleColor(ImGuiMCP::ImGuiCol_Button, a_danger ? Color(94, 45, 48) : Color(150, 94, 20));
                ImGuiMCP::PushStyleColor(ImGuiMCP::ImGuiCol_ButtonHovered, a_danger ? Color(137, 58, 61) : Color(184, 121, 31));
                ImGuiMCP::PushStyleColor(ImGuiMCP::ImGuiCol_ButtonActive, a_danger ? Color(166, 66, 67) : Color(205, 142, 42));
            }
            // A fixed 30px height clips 32px fonts even before FramePadding is added.
            // Let ImGui size the height from the actual font and use normal centering.
            ImGuiMCP::PushStyleVar(ImGuiMCP::ImGuiStyleVar_ButtonTextAlign, ImVec2{ 0.5F, 0.5F });
            const bool clicked = ImGuiMCP::Button(a_label, { std::max(a_minWidth, ButtonWidth(a_label)), 0.0F });
            ImGuiMCP::PopStyleVar();
            if (a_selected || a_danger) {
                ImGuiMCP::PopStyleColor(3);
            }
            return clicked;
        }

        void DrawPositionWindowButton()
        {
            if (SelectableButton(Tr("微調整"), g_positionWindowOpen)) {
                g_editor.FinishNumericEdit();
                g_positionWindowOpen = true;
                g_focusPositionWindow = true;
            }
            if (ImGuiMCP::IsItemHovered()) {
                Tooltip(Tr("仕上げ用の微調整パッドを別窓で開きます。距離・高さ・正面更新はメイン画面で操作します。\n窓の番号・点灯状態はメイン画面と連動します。×で閉じても照明は変わりません。\n開いている場合は手前に表示します。移動・サイズ変更できます。"));
            }
        }

        float RemoveLightButtonWidth()
        {
            return ButtonWidth("Light 3") + ImGuiMCP::GetStyle()->ItemInnerSpacing.x + ImGuiMCP::GetFontSize();
        }

        void DrawRemoveLightButton()
        {
            const auto label = "Light " + std::to_string(g_editor.SelectedSlot() + 1);
            const float font = ImGuiMCP::GetFontSize();
            const auto* style = ImGuiMCP::GetStyle();
            ImGuiMCP::BeginDisabled(!g_editor.CurrentLight().placed);
            // Draw our own monochrome icon; no emoji font or external asset required.
            const bool clicked = SelectableButton("##remove_placed_light", false, RemoveLightButtonWidth(), true);
            const auto start = ImGuiMCP::GetItemRectMin();
            const auto end = ImGuiMCP::GetItemRectMax();
            auto* draw = ImGuiMCP::GetWindowDrawList();
            const auto tint = ImGuiMCP::GetColorU32(ImGuiMCP::ImGuiCol_Text);
            const float y = (start.y + end.y - font) * .5F;
            ImGuiMCP::ImDrawListManager::AddText(draw, ImGuiMCP::GetFont(), font,
                {start.x + style->FramePadding.x, y}, tint, label.c_str());
            const float x = end.x - style->FramePadding.x - font;
            const float stroke = std::max(1.0F, font * .065F);
            const auto line = [&](float x1, float y1, float x2, float y2) {
                ImGuiMCP::ImDrawListManager::AddLine(draw,
                    {x + x1 * font, y + y1 * font}, {x + x2 * font, y + y2 * font}, tint, stroke);
            };
            line(.16F, .25F, .84F, .25F); // lid
            line(.38F, .13F, .62F, .13F);
            line(.38F, .13F, .38F, .25F);
            line(.62F, .13F, .62F, .25F);
            line(.24F, .34F, .30F, .87F); // bin
            line(.30F, .87F, .70F, .87F);
            line(.70F, .87F, .76F, .34F);
            line(.42F, .39F, .44F, .74F);
            line(.58F, .39F, .56F, .74F);
            if (ImGuiMCP::IsItemHovered()) {
                Tooltip(Tr("選択中のLight %dを配置から削除します。登録ライト・プリセットは削除しません。\n履歴の「戻る」で取り消せます。"), g_editor.SelectedSlot() + 1);
            }
            if (clicked) { g_editor.DeleteCurrent(); }
            ImGuiMCP::EndDisabled();
        }

        void DrawPowerButton(int a_slot, float a_width)
        {
            const auto origin = ImGuiMCP::GetCursorScreenPos();
            const float height = ImGuiMCP::GetFrameHeight();
            const bool placed = g_editor.GetScene()[a_slot].placed;
            ImGuiMCP::BeginDisabled(!placed);
            if (ImGuiMCP::Button("##power", { a_width, height })) {
                g_editor.ToggleLight(a_slot);
            }
            if (ImGuiMCP::IsItemHovered()) {
                Tooltip(Tr("Light %d : %s / クリックで%s（削除せず設定を保持）"), a_slot + 1,
                    g_editor.GetScene()[a_slot].enabled ? "ON" : "OFF",
                    g_editor.GetScene()[a_slot].enabled ? Tr("消灯") : Tr("点灯"));
            }
            ImGuiMCP::EndDisabled();
            const bool enabled = placed && g_editor.GetScene()[a_slot].enabled;
            auto* drawList = ImGuiMCP::GetWindowDrawList();
            const ImVec2 center{ origin.x + a_width * 0.5F, origin.y + height * 0.43F };
            const float radius = a_width * 0.23F;
            const auto color = enabled ? Color(255, 208, 113) : (placed ? Color(137, 151, 167) : Color(66, 75, 86));
            if (enabled) {
                ImGuiMCP::ImDrawListManager::AddCircleFilled(drawList, center, radius, color, 24);
            } else {
                ImGuiMCP::ImDrawListManager::AddCircle(drawList, center, radius, color, 24, 1.5F);
            }
            ImGuiMCP::ImDrawListManager::AddLine(drawList, { center.x - radius * 0.55F, center.y + radius * 1.45F }, { center.x + radius * 0.55F, center.y + radius * 1.45F }, color, 2.0F);
            if (placed && !enabled) {
                ImGuiMCP::ImDrawListManager::AddLine(drawList, { center.x - radius, center.y + radius }, { center.x + radius, center.y - radius }, color, 1.5F);
            }
        }

        void DrawSlotSelector(float a_width, const char* a_title = Tr("1. 位置"))
        {
            ImGuiMCP::AlignTextToFramePadding();
            ImGuiMCP::TextUnformatted(a_title);
            if (ImGuiMCP::IsItemHovered()) {
                Tooltip(Tr("ライトのある丸 : 編集対象を選択 / 空いた丸 : 移動\n重ねて移動 : 移動元の番号を選び、移動先の丸を右クリック\n上の番号でも編集対象を選べます。番号選択で丸の操作ルールは変わりません。"));
            }
            const float slotWidth = std::max(ImGuiMCP::GetFrameHeight(), ButtonWidth("3"));
            const float powerWidth = std::max(22.0F, ImGuiMCP::GetFrameHeight() * 0.65F);
            constexpr float innerGap = 3.0F;
            const float spacing = ImGuiMCP::GetStyle()->ItemSpacing.x;
            const float groupGap = LightSlotGroupGap(ImGuiMCP::GetFontSize(), spacing);
            const float pairWidth = slotWidth + powerWidth + innerGap;
            const bool besideTitle = LightSlotRowWidth(ImGuiMCP::CalcTextSize(a_title).x, pairWidth, spacing, groupGap) <= a_width;
            const int pairsPerRow = besideTitle ? 3 : std::clamp(static_cast<int>((a_width + groupGap) / (pairWidth + groupGap)), 1, 3);
            if (besideTitle) {
                ImGuiMCP::SameLine();
            }
            for (int slot = 0; slot < 3; ++slot) {
                ImGuiMCP::PushID(slot);
                const auto label = std::to_string(slot + 1);
                if (SelectableButton(label.c_str(), g_editor.SelectedSlot() == slot, slotWidth)) {
                    g_editor.SelectSlot(slot);
                }
                if (ImGuiMCP::IsItemHovered()) {
                    Tooltip(Tr("Light %d を編集 (%s)"), slot + 1, g_editor.GetScene()[slot].placed ? Tr("配置済み") : Tr("未配置"));
                }
                ImGuiMCP::SameLine(0.0F, innerGap);
                DrawPowerButton(slot, powerWidth);
                ImGuiMCP::PopID();
                if (slot < 2 && (slot + 1) % pairsPerRow != 0) {
                    ImGuiMCP::SameLine(0.0F, groupGap);
                }
            }
        }

        void DrawLightSelection(float a_buttonWidth)
        {
            ImGuiMCP::AlignTextToFramePadding();
            ImGuiMCP::Text(Tr("2. ライト選択"));
            if (ImGuiMCP::IsItemHovered()) {
                Tooltip(Tr("選択中の番号へライトを配置・変更します。\n白・暖色・ろうそく色・青の固定光です。位置・強さ・範囲・影は保持します。\nろうそくの揺らぎはありません。フェイスライトの色は変えません。"));
            }
            for (const int type : kLightDisplayOrder) {
                const auto& light = g_editor.CurrentLight();
                if (SelectableButton(Tr(kLightTypes[type].name), light.placed && light.type == type && !light.customTint, a_buttonWidth)) {
                    g_editor.SetType(type);
                }
            }
            ImGuiMCP::AlignTextToFramePadding();
            ImGuiMCP::TextDisabled(Tr("お気に入り"));
            ImGuiMCP::SameLine();
            ImGuiMCP::BeginDisabled(!g_library);
            if (SelectableButton(Tr("管理##light_library"))) {
                g_editor.FinishNumericEdit();
                g_openLightLibraryRequested = true;
            }
            ImGuiMCP::EndDisabled();
            if (ImGuiMCP::IsItemHovered()) { Tooltip(Tr("登録ライト一覧を開きます。お気に入りは最大5件。登録がなくてもここから開けます。")); }
            if (g_library) {
                const auto favorites = g_library->Favorites();
                if (!favorites.empty()) {
                    for (const auto index : favorites) {
                        const auto& entry = g_library->Lights()[index];
                        ImGuiMCP::PushID(static_cast<int>(index));
                        const auto& current = g_editor.CurrentLight();
                        const bool selected = current.placed && current.type == entry.type && current.customTint == entry.tint;
                        if (selected) { ImGuiMCP::PushStyleColor(ImGuiMCP::ImGuiCol_Button, Color(112, 80, 32)); }
                        // Fixed width clips long names; full name remains in tooltip.
                        if (ImGuiMCP::Button(entry.name.c_str(), {a_buttonWidth, 0})) {
                            g_editor.ApplyLightRecipe(entry.type, entry.tint);
                        }
                        if (selected) { ImGuiMCP::PopStyleColor(); }
                        if (ImGuiMCP::IsItemHovered()) {
                            Tooltip(Tr("%s\n%s / 種類と色だけを呼び出します。位置・強さ・範囲・影を保持します。\nお気に入りは登録ライト一覧で変更できます。"), entry.name.c_str(), Tr(kLightTypes[entry.type].name));
                        }
                        ImGuiMCP::PopID();
                    }
                }
            }
        }

        void DrawLightActions(float width)
        {
            const float actionGap = ImGuiMCP::GetStyle()->ItemSpacing.x + ImGuiMCP::GetFontSize() * .8F;
            DrawPositionWindowButton();
            if (FitsInline(width, ButtonWidth(Tr("微調整")), RemoveLightButtonWidth(), actionGap)) {
                ImGuiMCP::SameLine(0, actionGap);
            }
            DrawRemoveLightButton();
        }

        void DrawPlacementAndLightSelection(bool& anyActive)
        {
            const float favoriteHeaderWidth = ImGuiMCP::CalcTextSize(Tr("お気に入り")).x + ImGuiMCP::GetStyle()->ItemSpacing.x + ButtonWidth(Tr("管理"));
            const float selectionWidth = std::max(ButtonWidth(Tr("スタジオ暖色")), favoriteHeaderWidth);
            constexpr float columnGap = 14.0F;
            constexpr float minimumDirectionWidth = 310.0F;
            const float sideBySideMinimumWidth = minimumDirectionWidth + selectionWidth + columnGap;

            const float availableWidth = ImGuiMCP::GetContentRegionAvail().x;
            const bool sideBySide = availableWidth >= sideBySideMinimumWidth;
            const float directionWidth = sideBySide ?
                                             std::clamp(availableWidth - selectionWidth - columnGap, minimumDirectionWidth, 390.0F) :
                                             std::clamp(availableWidth, minimumDirectionWidth, 430.0F);

            ImGuiMCP::BeginGroup(); // Reserve the taller of diagram and favorites.
            ImGuiMCP::BeginGroup();
            DrawSlotSelector(directionWidth);
            DrawDirectionSelector(directionWidth, anyActive);
            // Actions share vertical space with favorite 5 instead of following it.
            if (sideBySide) { DrawLightActions(directionWidth); }
            ImGuiMCP::EndGroup();

            if (sideBySide) {
                ImGuiMCP::SameLine(0.0F, columnGap);
                ImGuiMCP::BeginGroup();
                DrawLightSelection(selectionWidth);
                ImGuiMCP::EndGroup();
            } else {
                ImGuiMCP::Spacing();
                DrawLightSelection(selectionWidth);
            }
            ImGuiMCP::EndGroup();
            if (!sideBySide) { DrawLightActions(availableWidth); }
        }

        void DrawNumeric(const char* a_label, NumericField a_field, float a_min, float a_max, const char* a_format, bool& a_anyActive)
        {
            float value = LightingEditor::Value(g_editor.CurrentLight(), a_field);
            const bool changed = ImGuiMCP::SliderFloat(a_label, &value, a_min, a_max, a_format);
            if (ImGuiMCP::IsItemActivated()) { g_editor.FinishNumericEdit(); }
            if (changed) {
                g_editor.SetNumeric(a_field, value);
            }
            a_anyActive = ImGuiMCP::IsItemActive() || a_anyActive;
            if (ImGuiMCP::IsItemDeactivatedAfterEdit()) {
                g_editor.FinishNumericEdit();
            }
        }

        void DrawPositionPad(bool& anyActive)
        {
            ImGuiMCP::TextUnformatted(Tr("微調整パッド (?)"));
            if (ImGuiMCP::IsItemHovered()) {
                Tooltip(Tr("ドラッグ : 左右・上下 / ホイール : 前後（上回しで奥へ）\n現在の方向・距離・高さへの追加量です。中央は追加量ゼロ、目盛りは0.5m。\n各軸±1m、ホイール1目盛り0.05m。パッドと下のスライダーは同じ値を編集します。\n水平軸は開始時・正面更新時のカメラ基準、上下はワールドの高さです。\n前後は平行移動で、「対象からの距離」とは別です。"));
            }
            const float width = std::max(1.0F, ImGuiMCP::GetContentRegionAvail().x);
            const float height = std::clamp(ImGuiMCP::GetFontSize() * 8.0F, 180.0F, 260.0F);
            const float scale = PadPixelsPerMetre(width, height);
            const auto origin = ImGuiMCP::GetCursorScreenPos();
            const ImVec2 end{ origin.x + width, origin.y + height };
            const ImVec2 center{ origin.x + width * 0.5F, origin.y + height * 0.5F };
            ImGuiMCP::PushID(g_editor.SelectedSlot());
            ImGuiMCP::InvisibleButton("fine_position_pad", { width, height });
            const bool hovered = ImGuiMCP::IsItemHovered();
            const bool active = ImGuiMCP::IsItemActive();
            const bool activated = ImGuiMCP::IsItemActivated();
            // ImGui's wheel owner prevents the surrounding settings pane scrolling
            // while the pad is hovered/active. Do not clear shared IO wheel state.
            ImGuiMCP::SetItemKeyOwner(ImGuiMCP::ImGuiKey_MouseWheelY, ImGuiMCP::ImGuiInputFlags_CondDefault_);
            if (activated) { g_editor.FinishNumericEdit(); }
            static int lastFrame = -1;
            static int lastSlot = -1;
            static double wheelUntil = 0.0;
            const int frame = ImGuiMCP::GetFrameCount();
            const bool continuous = lastFrame == frame - 1 && lastSlot == g_editor.SelectedSlot();
            const double now = ImGuiMCP::GetTime();
            if (!continuous || activated || !hovered) { wheelUntil = 0.0; }
            const auto* io = ImGuiMCP::GetIO();
            const bool dragging = active && !activated && continuous;
            const float wheel = (hovered || active) ? io->MouseWheel : 0.0F;
            if (wheel != 0.0F) { wheelUntil = now + 0.25; }
            if (dragging || wheel != 0.0F) {
                g_editor.SetFinePosition(MoveOnPad(g_editor.CurrentLight().fine,
                    dragging ? io->MouseDelta.x : 0.0F, dragging ? io->MouseDelta.y : 0.0F, wheel, scale));
            }
            anyActive = anyActive || active || (hovered && now < wheelUntil);
            if (ImGuiMCP::IsItemDeactivated()) {
                g_editor.FinishNumericEdit();
            }
            lastFrame = frame;
            lastSlot = g_editor.SelectedSlot();
            ImGuiMCP::PopID();

            auto* draw = ImGuiMCP::GetWindowDrawList();
            ImGuiMCP::ImDrawListManager::AddRectFilled(draw, origin, end, Color(17, 25, 35), 5.0F, 0);
            ImGuiMCP::ImDrawListManager::AddRect(draw, origin, end,
                active ? Color(255, 202, 102) : hovered ? Color(118, 214, 233) : Color(68, 87, 103), 5.0F, 0, 1.5F);
            // Same scale as MoveOnPad: 0.5m occupies equal pixels on either axis.
            for (int tick = -2; tick <= 2; ++tick) {
                const float offset = tick * scale * 0.5F;
                const auto color = tick == 0 ? Color(92, 122, 143) : Color(42, 56, 71);
                ImGuiMCP::ImDrawListManager::AddLine(draw, { center.x + offset, center.y - scale },
                    { center.x + offset, center.y + scale }, color, 1.0F);
                ImGuiMCP::ImDrawListManager::AddLine(draw, { center.x - scale, center.y + offset },
                    { center.x + scale, center.y + offset }, color, 1.0F);
            }
            const auto fine = g_editor.CurrentLight().fine;
            const ImVec2 dot{ center.x + fine.horizontal * scale, center.y - fine.vertical * scale };
            ImGuiMCP::ImDrawListManager::AddCircle(draw, center, 5.0F, Color(144, 168, 185), 24, 1.5F);
            ImGuiMCP::ImDrawListManager::AddLine(draw, center, dot, Color(118, 214, 233), 1.5F);
            ImGuiMCP::ImDrawListManager::AddCircleFilled(draw, dot, 8.0F, Color(255, 202, 102), 24);
            ImGuiMCP::ImDrawListManager::AddCircle(draw, dot, 9.0F, Color(10, 18, 26), 24, 1.5F);
        }

        void DrawFinePosition(bool& anyActive)
        {
            DrawPositionPad(anyActive);
            DrawNumeric(Tr("前後 (+手前)"), NumericField::FineDepth, -kFinePositionLimit, kFinePositionLimit, "%+.2f m", anyActive);
            DrawNumeric(Tr("左右 (+右)"), NumericField::FineHorizontal, -kFinePositionLimit, kFinePositionLimit, "%+.2f m", anyActive);
            DrawNumeric(Tr("上下 (+上)"), NumericField::FineVertical, -kFinePositionLimit, kFinePositionLimit, "%+.2f m", anyActive);
        }

        void DrawFinePositionReset()
        {
            ImGuiMCP::BeginDisabled(!g_editor.CurrentLight().placed || !HasFinePosition(g_editor.CurrentLight().fine));
            if (SelectableButton(Tr("基準に戻す"))) { g_editor.ResetFinePosition(); }
            ImGuiMCP::EndDisabled();
            if (ImGuiMCP::IsItemHovered()) {
                Tooltip(Tr("左右・上下・前後の追加量だけをゼロにします。\n基本方向・距離・高さ、色・強さ・範囲・影・ON/OFFは変えません。\nこの操作も「戻る」で取り消せます。"));
            }
        }

        const char* ShortLightName(const LightSettings& light)
        {
            if (light.customTint) { return Tr("カスタム"); }
            const std::array<const char*, 4> names{Tr("白"), Tr("暖色"), Tr("ろうそく"), Tr("青")};
            return names[light.type];
        }

        void DrawPresetLightLabel(ImDrawList* a_drawList, const ImVec2& a_origin, float a_width, float a_height, int a_slot, const LightSettings& a_light)
        {
            const auto rgb = DisplayTint(a_light);
            const auto tint = a_light.enabled ? Color(rgb[0], rgb[1], rgb[2]) : Color(107, 120, 137);
            // Information only: no nested buttons or independent click targets inside a card.
            ImGuiMCP::ImDrawListManager::AddRectFilled(a_drawList, a_origin, { a_origin.x + a_width, a_origin.y + a_height }, Color(28, 39, 53), 4.0F, 0);
            ImGuiMCP::ImDrawListManager::AddRectFilled(a_drawList, { a_origin.x, a_origin.y + 5.0F }, { a_origin.x + 3.0F, a_origin.y + a_height - 5.0F }, tint, 1.0F, 0);
            const auto label = std::to_string(a_slot + 1) + ": " + ShortLightName(a_light);
            const auto size = ImGuiMCP::CalcTextSize(label.c_str());
            const float scale = std::min(1.0F, std::max(1.0F, a_width - 20.0F) / size.x);
            const ImVec2 textPos{ a_origin.x + 10.0F, a_origin.y + (a_height - size.y * scale) * 0.5F };
            ImGuiMCP::ImDrawListManager::AddText(a_drawList, ImGuiMCP::GetFont(), ImGuiMCP::GetFontSize() * scale, textPos,
                a_light.enabled ? Color(225, 234, 243) : Color(127, 141, 157), label.c_str());
            if (!a_light.enabled) {
                const auto number = std::to_string(a_slot + 1);
                const float numberWidth = ImGuiMCP::CalcTextSize(number.c_str()).x * scale;
                ImGuiMCP::ImDrawListManager::AddLine(a_drawList, { textPos.x, textPos.y + size.y * scale * 0.8F },
                    { textPos.x + numberWidth, textPos.y + size.y * scale * 0.2F }, tint, 1.5F);
            }
        }

        bool DrawPresetCard(const ScenePreset& a_preset, float a_width, float a_labelTextWidth, bool includeFace = false)
        {
            const char* displayName = std::string_view(a_preset.id) == "user" ? a_preset.name : Tr(a_preset.name);
            const float font = ImGuiMCP::GetFontSize();
            int placed = 0;
            for (const auto& light : a_preset.scene) {
                placed += light.placed ? 1 : 0;
            }
            const auto layout = CalculatePresetCardLayout(a_width, font, a_labelTextWidth, placed);
            const float height = layout.height;
            const auto origin = ImGuiMCP::GetCursorScreenPos();
            const bool clicked = ImGuiMCP::Button("##card", { a_width, height });
            const bool hovered = ImGuiMCP::IsItemHovered();
            const bool focused = ImGuiMCP::IsItemFocused();
            if (ImGuiMCP::IsItemVisible()) {
                auto* drawList = ImGuiMCP::GetWindowDrawList();
                const ImVec2 end{ origin.x + a_width, origin.y + height };
                const bool matches = a_preset.scene.lights == g_editor.GetScene().lights &&
                    (!includeFace || a_preset.scene.face == g_editor.GetScene().face);
                ImGuiMCP::ImDrawListManager::AddRectFilled(drawList, origin, end, hovered ? Color(35, 47, 62) : Color(20, 28, 39), 8.0F, 0);
                ImGuiMCP::ImDrawListManager::AddRect(drawList, origin, end,
                    (hovered || focused) ? Color(255, 207, 113) : (matches ? Color(158, 125, 66) : Color(65, 82, 104)), 8.0F, 0, 2.0F);
                const auto title = PresetTitleLines(displayName, layout.titleWidth,
                    [](const std::string& text) { return ImGuiMCP::CalcTextSize(text.c_str()).x; });
                for (std::size_t line = 0; line < title.size(); ++line) {
                    ImGuiMCP::ImDrawListManager::AddText(drawList, ImGuiMCP::GetFont(), font,
                        { origin.x + layout.titleX, origin.y + layout.titleY + font * line },
                        Color(239, 244, 250), title[line].c_str());
                }
                DrawDirectionDiagram(a_preset.scene, { origin.x + layout.diagramX, origin.y + layout.diagramY }, layout.diagramSize, false);
                int row = 0;
                for (int slot = 0; slot < static_cast<int>(a_preset.scene.size()); ++slot) {
                    const auto& light = a_preset.scene[slot];
                    if (!light.placed) {
                        continue;
                    }
                    DrawPresetLightLabel(drawList, { origin.x + layout.labelsX, origin.y + layout.labelsY + row * (layout.rowHeight + layout.rowGap) },
                        layout.labelsWidth, layout.rowHeight, slot, light);
                    ++row;
                }
            }
            if (hovered) {
                ImGuiMCP::BeginTooltip();
                ImGuiMCP::PushTextWrapPos(ImGuiMCP::GetFontSize() * 34.0F);
                ImGuiMCP::TextUnformatted(displayName);
                ImGuiMCP::TextUnformatted(Tr(a_preset.description));
                ImGuiMCP::TextUnformatted(g_managePresets ? Tr("管理中はカードを押しても照明を変えません。") :
                    includeFace ? Tr("クリックで3灯とフェイスライトを適用します。") : Tr("クリックで3灯を適用します。フェイス設定は保持します。"));
                for (int slot = 0; slot < 3; ++slot) {
                    const auto& light = a_preset.scene[slot];
                    if (!light.placed) { continue; }
                    const auto tint = RuntimeTint(light).value();
                    ImGuiMCP::Text("%d: %s%s / RGB %d, %d, %d / %s", slot + 1, Tr(kLightTypes[light.type].name),
                        light.customTint ? Tr("（カスタム色）") : "", static_cast<int>(tint.red * 255 + .5F),
                        static_cast<int>(tint.green * 255 + .5F), static_cast<int>(tint.blue * 255 + .5F), light.enabled ? "ON" : "OFF");
                }
                ImGuiMCP::PopTextWrapPos();
                ImGuiMCP::EndTooltip();
            }
            return clicked && !g_managePresets;
        }


        void DrawRenamePopup(bool scene, std::size_t index, bool requested)
        {
            static std::array<char, 121> name{};
            static std::filesystem::path file;
            static std::string error;
            if (requested) {
                const auto& oldName = scene ? g_library->Scenes()[index].name : g_library->Lights()[index].name;
                file = scene ? g_library->Scenes()[index].file : g_library->Lights()[index].file;
                name.fill('\0');
                std::copy(oldName.begin(), oldName.end(), name.begin());
                error.clear();
                ImGuiMCP::OpenPopup("rename_entry");
            }
            if (ImGuiMCP::BeginPopup("rename_entry")) {
                ImGuiMCP::TextUnformatted(Tr("名前変更"));
                ImGuiMCP::SetNextItemWidth(std::min(280.0F, ImGuiMCP::GetMainViewport()->WorkSize.x * .7F));
                if (ImGuiMCP::InputText(Tr("名前"), name.data(), name.size())) { error.clear(); }
                ImGuiMCP::TextWrapped(Tr("名前だけを変更します。照明・お気に入り・並び順は保持します。"));
                if (SelectableButton(Tr("変更する"))) {
                    const auto currentFile = scene ? g_library->Scenes()[index].file : g_library->Lights()[index].file;
                    if (currentFile != file) { error = Tr("登録が変わりました。開き直してください。"); }
                    else if (scene ? g_library->RenameScene(index, name.data()) : g_library->RenameLight(index, name.data())) {
                        ImGuiMCP::CloseCurrentPopup();
                    } else { error = g_library->Error(); }
                }
                ImGuiMCP::SameLine();
                if (SelectableButton(Tr("キャンセル"))) { ImGuiMCP::CloseCurrentPopup(); }
                if (!error.empty()) { ImGuiMCP::TextWrapped("%s", StoredError(error).c_str()); }
                ImGuiMCP::EndPopup();
            }
        }

        void DrawPresetPicker()
        {
            static std::string operationError;
            static bool wasOpen{};
            if (!g_presetPicker.IsOpen()) {
                wasOpen = false;
                return;
            }
            if (!wasOpen) {
                operationError.clear();
                wasOpen = true;
            }
            const auto* viewport = ImGuiMCP::GetMainViewport();
            const ImVec2 maximum{ viewport->WorkSize.x * 0.95F, viewport->WorkSize.y * 0.95F };
            const WindowRectangle work{viewport->WorkPos.x, viewport->WorkPos.y, viewport->WorkSize.x, viewport->WorkSize.y};
            const auto rectangle = g_presetGeometry.Resolve(work);
            const auto condition = g_presetGeometry.NeedsClamp(work) ? ImGuiMCP::ImGuiCond_Always : ImGuiMCP::ImGuiCond_Appearing;
            ImGuiMCP::SetNextWindowPos({rectangle.x, rectangle.y}, condition);
            ImGuiMCP::SetNextWindowSize({rectangle.width, rectangle.height}, condition);
            ImGuiMCP::SetNextWindowSizeConstraints({ std::min(340.0F, maximum.x), std::min(300.0F, maximum.y) }, maximum);
            if (g_focusPresetWindow) {
                ImGuiMCP::SetNextWindowFocus();
                g_focusPresetWindow = false;
            }
            // Keep the gallery readable independently of the framework's transparent theme.
            ImGuiMCP::PushStyleColor(ImGuiMCP::ImGuiCol_PopupBg, Color(12, 18, 27));
            ImGuiMCP::PushStyleColor(ImGuiMCP::ImGuiCol_WindowBg, Color(12, 18, 27));
            bool open = true;
            // Normal window: no modal dim overlay, main lighting controls stay usable.
            // Only this Lighting page submits it, just like the fine-position window.
            const bool visible = ImGuiMCP::Begin(Tr("プリセット###sla_presets"), &open,
                ImGuiMCP::ImGuiWindowFlags_NoCollapse | ImGuiMCP::ImGuiWindowFlags_NoSavedSettings);
            const auto position = ImGuiMCP::GetWindowPos();
            const auto size = ImGuiMCP::GetWindowSize();
            g_presetGeometry.Remember({position.x, position.y, size.x, size.y});
            if (visible) {
                const auto* style = ImGuiMCP::GetStyle();
                const float headerGap = style->ItemSpacing.x + ImGuiMCP::GetFontSize() * .6F;
                const auto checkboxWidth = [&](const char* label) {
                    return ImGuiMCP::GetFrameHeight() + style->ItemInnerSpacing.x + ImGuiMCP::CalcTextSize(label).x;
                };
                ActionRowLayout header{ImGuiMCP::GetContentRegionAvail().x, headerGap};
                header.Place(checkboxWidth(Tr("管理")));
                ImGuiMCP::Checkbox(Tr("管理"), &g_managePresets);
                if (ImGuiMCP::IsItemHovered()) {
                    Tooltip(Tr("管理中はカードで適用せず、名前変更・並び替え・削除・サンプル表示を整理します。\n削除した自作プリセットはArchivedへ退避し、照明や栞を変えません。\n図の色は設定RGB、番号の斜線はOFFです。色はゲーム画面の見え方の再現ではありません。"));
                }
                if (header.Place(checkboxWidth(Tr("選択後も開く")))) { ImGuiMCP::SameLine(0, headerGap); }
                bool keepOpen = g_presetPicker.KeepOpen();
                if (ImGuiMCP::Checkbox(Tr("選択後も開く"), &keepOpen)) { g_presetPicker.SetKeepOpen(keepOpen); }
                if (ImGuiMCP::IsItemHovered()) {
                    Tooltip(Tr("ON：適用後も一覧を開いたまま比較できます。OFF：適用すると閉じます。\n×は画面だけを閉じ、最後に適用した照明を残します。\n位置・サイズとこのチェックはゲーム起動中だけ記憶します。"));
                }
                const float width = std::max(1.0F, ImGuiMCP::GetContentRegionAvail().x);
                const float gap = ImGuiMCP::GetStyle()->ItemSpacing.x;
                const char* help = g_managePresets ? Tr("整理中 / 照明は変わりません") : Tr("カードを選んで適用");
                const auto sectionHeading = [&](const char* label, bool showHelp) {
                    ImGuiMCP::TextUnformatted(label);
                    if (showHelp && FitsInline(width, ImGuiMCP::CalcTextSize(label).x,
                            ImGuiMCP::CalcTextSize(help).x, headerGap)) {
                        ImGuiMCP::SameLine(0, headerGap);
                        ImGuiMCP::TextUnformatted(help);
                    }
                };
                const float labelTextWidth = std::max(ImGuiMCP::CalcTextSize(Tr("3: カスタム")).x, ImGuiMCP::CalcTextSize(Tr("3: ろうそく")).x);
                const float minimumCard = MinimumPresetCardWidth(labelTextWidth);
                const int columns = std::clamp(static_cast<int>((width + gap) / (minimumCard + gap)), 1, 3);
                const float cardWidth = (width - gap * (columns - 1)) / columns;
                std::optional<std::size_t> selected;
                std::optional<std::size_t> selectedUser;
                std::optional<std::size_t> removeUser;
                std::optional<std::pair<std::size_t, int>> moveUser;
                if (g_library && !g_library->Scenes().empty()) {
                    sectionHeading(Tr("登録したプリセット"), true);
                    for (std::size_t index = 0; index < g_library->Scenes().size(); ++index) {
                        const auto& entry = g_library->Scenes()[index];
                        ImGuiMCP::PushID(static_cast<int>(index));
                        ImGuiMCP::BeginGroup();
                        const ScenePreset card{"user", entry.name.c_str(), entry.includeFace ? Tr("フェイスライトを含む") : Tr("通常3灯のみ"), entry.scene};
                        if (DrawPresetCard(card, cardWidth, labelTextWidth, entry.includeFace)) { selectedUser = index; }
                        if (g_managePresets) {
                            ActionRowLayout actions{cardWidth, gap};
                            NextAction(actions, Tr("前へ"));
                            ImGuiMCP::BeginDisabled(index == 0);
                            if (SelectableButton(Tr("前へ"))) { moveUser = {{index, -1}}; }
                            ImGuiMCP::EndDisabled();
                            NextAction(actions, Tr("後へ"));
                            ImGuiMCP::BeginDisabled(index + 1 == g_library->Scenes().size());
                            if (SelectableButton(Tr("後へ"))) { moveUser = {{index, 1}}; }
                            ImGuiMCP::EndDisabled();
                            NextAction(actions, Tr("名前変更"));
                            const bool rename = SelectableButton(Tr("名前変更"));
                            NextAction(actions, Tr("削除"));
                            if (SelectableButton(Tr("削除"), false, 0, true)) { ImGuiMCP::OpenPopup("archive_scene"); }
                            // Draw popups after the whole row; their items must not
                            // become the preceding item for a parent-row SameLine.
                            DrawRenamePopup(true, index, rename);
                            if (ImGuiMCP::BeginPopup("archive_scene")) {
                                ImGuiMCP::TextWrapped(Tr("%s を一覧から外します。\nファイルはArchivedへ退避します。照明は変えません。"), entry.name.c_str());
                                if (SelectableButton(Tr("削除する"), false, 0, true)) { removeUser = index; ImGuiMCP::CloseCurrentPopup(); }
                                ImGuiMCP::SameLine();
                                if (SelectableButton(Tr("キャンセル"))) { ImGuiMCP::CloseCurrentPopup(); }
                                ImGuiMCP::EndPopup();
                            }
                        }
                        ImGuiMCP::EndGroup();
                        ImGuiMCP::PopID();
                        if ((index + 1) % columns != 0 && index + 1 < g_library->Scenes().size()) { ImGuiMCP::SameLine(); }
                    }
                    ImGuiMCP::Separator();
                }
                sectionHeading(Tr("サンプル"), !g_library || g_library->Scenes().empty());
                std::vector<std::size_t> samples;
                for (std::size_t index = 0; index < kScenePresets.size(); ++index) {
                    if (g_managePresets || !g_library || !g_library->IsSampleHidden(kScenePresets[index].id)) { samples.push_back(index); }
                }
                for (std::size_t ordinal = 0; ordinal < samples.size(); ++ordinal) {
                    const auto index = samples[ordinal];
                    ImGuiMCP::PushID(kScenePresets[index].id);
                    ImGuiMCP::BeginGroup();
                    if (DrawPresetCard(kScenePresets[index], cardWidth, labelTextWidth, kScenePresets[index].includeFace)) {
                        selected = index;
                    }
                    if (g_managePresets && g_library) {
                        const bool hidden = g_library->IsSampleHidden(kScenePresets[index].id);
                        if (SelectableButton(hidden ? Tr("再表示") : Tr("非表示"))) {
                            if (g_library->SetSampleHidden(kScenePresets[index].id, !hidden)) { operationError.clear(); }
                            else { operationError = g_library->Error(); }
                        }
                        if (ImGuiMCP::IsItemHovered()) { Tooltip(Tr("サンプルは削除せず、通常の一覧での表示だけ切り替えます。")); }
                    }
                    ImGuiMCP::EndGroup();
                    ImGuiMCP::PopID();
                    if ((ordinal + 1) % columns != 0 && ordinal + 1 < samples.size()) {
                        ImGuiMCP::SameLine();
                    }
                }
                if (selected) { g_presetPicker.Choose(*selected, g_editor); }
                if (selectedUser && PresetLibrary::Apply(g_library->Scenes()[*selectedUser], g_editor)) {
                    g_presetPicker.Applied();
                }
                if (removeUser) {
                    if (g_library->ArchiveScene(*removeUser)) {
                        g_libraryNotice = Tr("プリセットを削除しました（Archivedへ退避）。");
                        operationError.clear();
                    } else { operationError = g_library->Error(); }
                } else if (moveUser) {
                    if (g_library->MoveScene(moveUser->first, moveUser->second)) { operationError.clear(); }
                    else { operationError = g_library->Error(); }
                }
                if (!operationError.empty()) { ImGuiMCP::TextWrapped("%s", StoredError(operationError).c_str()); }
                if (g_library && !g_library->ViewError().empty()) { ImGuiMCP::TextWrapped("%s", StoredError(g_library->ViewError()).c_str()); }
            }
            ImGuiMCP::End();
            if (!open) {
                g_presetPicker.Close();
            }
            ImGuiMCP::PopStyleColor(2);
        }

        void DrawLightLibrary()
        {
            static std::array<char, 121> search{};
            // These belong to this window, never to preset registration/management.
            static std::string notice, operationError;
            if (g_openLightLibraryRequested) {
                search.fill('\0');
                notice.clear();
                operationError.clear();
                ImGuiMCP::OpenPopup(Tr("登録ライト一覧###sla_library"));
                g_openLightLibraryRequested = false;
            }
            if (!g_library) { return; }
            const auto* viewport = ImGuiMCP::GetMainViewport();
            const ImVec2 maximum{viewport->WorkSize.x * .95F, viewport->WorkSize.y * .95F};
            ImGuiMCP::SetNextWindowSize({std::min(650.0F, maximum.x), std::min(660.0F, maximum.y)}, ImGuiMCP::ImGuiCond_FirstUseEver);
            ImGuiMCP::SetNextWindowSizeConstraints({std::min(320.0F, maximum.x), std::min(300.0F, maximum.y)}, maximum);
            ImGuiMCP::PushStyleColor(ImGuiMCP::ImGuiCol_PopupBg, Color(12, 18, 27));
            bool open = true;
            if (ImGuiMCP::BeginPopupModal(Tr("登録ライト一覧###sla_library"), &open, ImGuiMCP::ImGuiWindowFlags_NoCollapse)) {
                ImGuiMCP::TextWrapped(Tr("適用先 : Light %d / 登録 %zu 件 / お気に入り %zu / %zu"), g_editor.SelectedSlot() + 1,
                    g_library->Lights().size(), g_library->Favorites().size(), PresetLibrary::kFavoriteLimit);
                ImGuiMCP::TextWrapped(Tr("使う : 種類・色だけ適用。お気に入りはメインに表示します。削除はArchivedへの退避です。"));
                ImGuiMCP::SetNextItemWidth(std::max(1.0F, ImGuiMCP::GetContentRegionAvail().x - ButtonWidth(Tr("検索"))));
                ImGuiMCP::InputText(Tr("検索"), search.data(), search.size());
                std::optional<std::size_t> remove;
                bool found = false;
                const float footer = ImGuiMCP::GetFrameHeightWithSpacing() * 3;
                if (ImGuiMCP::BeginChild("entries", {0, std::max(ImGuiMCP::GetFrameHeightWithSpacing() * 3, ImGuiMCP::GetContentRegionAvail().y - footer)}, 0)) {
                    for (std::size_t index = 0; index < g_library->Lights().size(); ++index) {
                        const auto& entry = g_library->Lights()[index];
                        if (entry.name.find(search.data()) == std::string::npos) { continue; }
                        found = true;
                        ImGuiMCP::PushID(static_cast<int>(index));
                        const auto tint = entry.tint.value_or(kRuntimeTints[entry.type]);
                        const float swatch = ImGuiMCP::GetFontSize();
                        ImGuiMCP::ColorButton("##rgb", {tint.red, tint.green, tint.blue, 1}, ImGuiMCP::ImGuiColorEditFlags_NoTooltip, {swatch, swatch});
                        ImGuiMCP::SameLine();
                        ImGuiMCP::TextWrapped("%s", entry.name.c_str());
                        ImGuiMCP::TextDisabled("%s%s", Tr(kLightTypes[entry.type].name), entry.tint ? Tr(" / カスタム色") : "");
                        ActionRowLayout actions{ImGuiMCP::GetContentRegionAvail().x, ImGuiMCP::GetStyle()->ItemSpacing.x};
                        NextAction(actions, Tr("使う"));
                        if (SelectableButton(Tr("使う"))) {
                            g_editor.ApplyLightRecipe(entry.type, entry.tint);
                            notice = Tr("ライトへ適用 : ") + entry.name;
                            operationError.clear();
                        }
                        const bool favorite = g_library->IsFavorite(index);
                        const char* favoriteLabel = favorite ? Tr("お気に入り解除") : Tr("お気に入りに");
                        NextAction(actions, favoriteLabel);
                        if (SelectableButton(favoriteLabel, favorite)) {
                            if (!favorite && g_library->Favorites().size() == PresetLibrary::kFavoriteLimit) { ImGuiMCP::OpenPopup("replace_favorite"); }
                            else { g_library->SetFavorite(index, !favorite); }
                        }
                        if (ImGuiMCP::IsItemHovered()) { Tooltip(Tr("メインの表示だけを変更します。登録や使用中の照明は削除・変更しません。")); }
                        NextAction(actions, Tr("名前変更"));
                        const bool rename = SelectableButton(Tr("名前変更"));
                        NextAction(actions, Tr("削除"));
                        if (SelectableButton(Tr("削除"), false, 0, true)) { ImGuiMCP::OpenPopup("archive_light"); }
                        DrawRenamePopup(false, index, rename);
                        if (ImGuiMCP::BeginPopup("replace_favorite")) {
                            ImGuiMCP::Text(Tr("%zu件登録済みです。入れ替える項目を選んでください。"), PresetLibrary::kFavoriteLimit);
                            for (const auto replace : g_library->Favorites()) {
                                ImGuiMCP::PushID(static_cast<int>(replace));
                                if (SelectableButton(g_library->Lights()[replace].name.c_str())) {
                                    if (g_library->SetFavorite(index, true, replace)) { ImGuiMCP::CloseCurrentPopup(); }
                                }
                                ImGuiMCP::PopID();
                            }
                            if (SelectableButton(Tr("キャンセル"))) { ImGuiMCP::CloseCurrentPopup(); }
                            if (!g_library->ViewError().empty()) { ImGuiMCP::TextWrapped("%s", StoredError(g_library->ViewError()).c_str()); }
                            ImGuiMCP::EndPopup();
                        }
                        if (ImGuiMCP::BeginPopup("archive_light")) {
                            ImGuiMCP::TextWrapped(Tr("%s を一覧から外します。\n使用中の照明・保存済みプリセットは変わりません。ファイルはArchivedへ退避します。"), entry.name.c_str());
                            if (SelectableButton(Tr("削除する"), false, 0, true)) { remove = index; ImGuiMCP::CloseCurrentPopup(); }
                            ImGuiMCP::SameLine();
                            if (SelectableButton(Tr("キャンセル"))) { ImGuiMCP::CloseCurrentPopup(); }
                            ImGuiMCP::EndPopup();
                        }
                        ImGuiMCP::Separator();
                        ImGuiMCP::PopID();
                    }
                    if (!found) { ImGuiMCP::TextWrapped(g_library->Lights().empty() ? Tr("登録はまだありません。メインの「ライト登録」から追加できます。") : Tr("検索に一致するライトはありません。")); }
                }
                ImGuiMCP::EndChild();
                if (remove) {
                    notice.clear();
                    operationError.clear();
                    if (g_library->ArchiveLight(*remove)) { notice = Tr("ライト登録を削除しました（Archivedへ退避）。"); }
                    else { operationError = g_library->Error(); }
                }
                if (!notice.empty()) { ImGuiMCP::TextWrapped("%s", notice.c_str()); }
                if (!operationError.empty()) { ImGuiMCP::TextWrapped("%s", StoredError(operationError).c_str()); }
                if (!g_library->ViewError().empty()) { ImGuiMCP::TextWrapped("%s", StoredError(g_library->ViewError()).c_str()); }
                ImGuiMCP::EndPopup();
            }
            ImGuiMCP::PopStyleColor();
        }

        void DrawHistory(bool a_compact = false)
        {
            const float width = ImGuiMCP::GetContentRegionAvail().x;
            const float spacing = ImGuiMCP::GetStyle()->ItemSpacing.x;
            ImGuiMCP::BeginDisabled(!g_editor.CanUndo());
            if (SelectableButton(Tr("戻る"))) {
                g_editor.Undo();
            }
            ImGuiMCP::EndDisabled();
            if (ImGuiMCP::IsItemHovered()) {
                Tooltip(Tr("3灯とフェイスライト共通の履歴を1操作戻します。位置以外の変更も対象です。"));
            }
            ImGuiMCP::SameLine();
            ImGuiMCP::BeginDisabled(!g_editor.CanRedo());
            if (SelectableButton(Tr("進む"))) {
                g_editor.Redo();
            }
            ImGuiMCP::EndDisabled();
            if (ImGuiMCP::IsItemHovered()) {
                Tooltip(Tr("3灯とフェイスライト共通の履歴を1操作進めます。編集対象の番号も復元します。"));
            }
            if (a_compact) { return; }
            const float historyGap = spacing + ImGuiMCP::GetFontSize() * .7F;
            if (width >= ButtonWidth(Tr("戻る")) + ButtonWidth(Tr("進む")) + spacing + historyGap + ImGuiMCP::GetFontSize() * 7.0F) {
                ImGuiMCP::SameLine(0, historyGap);
            }
            ImGuiMCP::SetNextItemWidth(std::max(1.0F, ImGuiMCP::GetContentRegionAvail().x));
            const auto& history = g_editor.History();
            const auto preview = Tr("履歴 : ") + (g_editor.HasPendingChanges() ? std::string(Tr("調整中")) : HistoryText(history[g_editor.HistoryCursor()].label));
            std::optional<std::uint64_t> jumpId;
            if (ImGuiMCP::BeginCombo("##lighting_history", preview.c_str())) {
                // Do not restore while iterating: finishing a slider can invalidate entries.
                for (std::size_t index = 0; index < history.size(); ++index) {
                    const auto& entry = history[index];
                    const bool selected = index == g_editor.HistoryCursor();
                    const auto label = std::to_string(entry.id) + ". " + HistoryText(entry.label);
                    if (ImGuiMCP::Selectable(label.c_str(), selected)) {
                        jumpId = entry.id;
                    }
                    if (selected) {
                        ImGuiMCP::SetItemDefaultFocus();
                    }
                }
                ImGuiMCP::EndCombo();
            }
            if (jumpId) {
                g_editor.JumpTo(*jumpId);
            }
            if (ImGuiMCP::IsItemHovered()) {
                Tooltip(Tr("最大50件。スライダーは操作ごとに1件記録します。過去へ戻って編集すると、その先の履歴は置き換わります。ゲーム終了で履歴は消えます。"));
            }
        }

        float BookmarkWidth()
        {
            return BookmarkRowWidth(ImGuiMCP::CalcTextSize(Tr("栞を")).x,
                ButtonWidth(Tr("登録")), ButtonWidth(Tr("戻す")), ImGuiMCP::GetStyle()->ItemSpacing.x);
        }

        void DrawBookmark(bool compact = false)
        {
            const float width = ImGuiMCP::GetContentRegionAvail().x;
            ImGuiMCP::PushID("bookmark_controls");
            ActionRowLayout row{width, ImGuiMCP::GetStyle()->ItemSpacing.x};
            row.Place(ImGuiMCP::CalcTextSize(Tr("栞を")).x);
            ImGuiMCP::AlignTextToFramePadding();
            ImGuiMCP::TextUnformatted(Tr("栞を"));
            NextAction(row, Tr("登録"));
            if (SelectableButton(Tr("登録"))) {
                g_editor.Adopt();
            }
            if (ImGuiMCP::IsItemHovered()) {
                Tooltip(Tr("3灯とフェイスライトを一時記憶します（ゲーム終了まで1件）。再登録で上書きします。\n上と下のボタンは同じ栞です。カメラ位置・正面の基準・撮影ライト全体のON/OFFは保存しません。\n再起動後も残すには「プリセット登録」を使ってください。"));
            }
            NextAction(row, Tr("戻す"));
            ImGuiMCP::BeginDisabled(!g_editor.HasBookmark() || g_editor.MatchesBookmark());
            if (SelectableButton(Tr("戻す"))) {
                g_editor.RestoreBookmark();
            }
            ImGuiMCP::EndDisabled();
            if (ImGuiMCP::IsItemHovered()) { Tooltip(Tr("最後に栞へ登録した照明と編集番号へ戻します。履歴の「戻る」で取り消せます。")); }
            if (!compact && g_editor.MatchesBookmark()) {
                ImGuiMCP::TextDisabled(Tr("栞と同じ構成"));
            }
            ImGuiMCP::PopID();
        }

        void DrawShadowModeControls()
        {
            const float width = ImGuiMCP::GetContentRegionAvail().x;
            const float spacing = ImGuiMCP::GetStyle()->ItemSpacing.x;
            float used = 0.0F;
            const std::array<const char*, 3> labels{ Tr("影なし"), Tr("全方向の影"), Tr("スポットの影") };
            for (int mode = 0; mode < 3; ++mode) {
                const auto& light = g_editor.CurrentLight();
                const bool selected = mode == 0 ? !light.castsShadow :
                    light.castsShadow && light.shadowProjection == (mode == 1 ? ShadowProjection::Omni : ShadowProjection::Spot);
                const float buttonWidth = ButtonWidth(labels[mode]);
                if (used > 0.0F && used + spacing + buttonWidth <= width) {
                    ImGuiMCP::SameLine();
                    used += spacing;
                } else { used = 0.0F; }
                if (SelectableButton(labels[mode], selected)) {
                    g_editor.SetShadowMode(mode != 0, mode == 0 ? light.shadowProjection :
                        mode == 1 ? ShadowProjection::Omni : ShadowProjection::Spot);
                }
                if (ImGuiMCP::IsItemHovered()) {
                    Tooltip(Tr("同じライトを切り替えて比較します。位置・強さ・範囲・影の補正は変えません。\nスポットは基準点へ自動で向きます（照射角90°）。\n消灯とは別です。比較時はSAMなど他の撮影ライトを消してください。"));
                }
                used += buttonWidth;
            }
        }

        void DrawShadowBiasControls()
        {
            ImGuiMCP::TextUnformatted(Tr("影の補正"));
            if (ImGuiMCP::IsItemHovered()) {
                Tooltip(Tr("選択中のライトの影の判定を補正します。標準は1です。\n影をぼかす機能ではありません。大きすぎると影が身体から離れたり、細かい影が消える場合があります。\n表示はスポット選択中のみです。別の影方式・消灯へ切り替えても値を保持します。全方向の影にも同じ補正値が適用されます。\nSkyrim全体の設定は変更しません。"));
            }
            const float width = ImGuiMCP::GetContentRegionAvail().x;
            const float spacing = ImGuiMCP::GetStyle()->ItemSpacing.x;
            float used = 0.0F;
            for (const float value : kShadowBiasLevels) {
                const auto label = value == 1.0F ? std::string(Tr("1 (標準)")) : Fmt("{:g}", value);
                const float buttonWidth = ButtonWidth(label.c_str());
                if (used > 0.0F && used + spacing + buttonWidth <= width) {
                    ImGuiMCP::SameLine();
                    used += spacing;
                } else {
                    used = 0.0F;
                }
                if (SelectableButton(label.c_str(), g_editor.CurrentLight().shadowBias == value)) {
                    g_editor.SetShadowBias(value);
                }
                used += buttonWidth;
            }
            if (!g_editor.CurrentLight().castsShadow) {
                ImGuiMCP::TextDisabled(Tr("影ONで補正を反映"));
            }
        }

        void RequestRegistration(bool scene)
        {
            g_editor.FinishNumericEdit();
            g_registerScene = scene;
            g_registrationSnapshot = g_editor.GetScene();
            g_registrationSlot = g_editor.SelectedSlot();
            g_saveFace = false;
            g_registrationName.fill('\0');
            g_registrationError.clear();
            g_libraryNotice.clear();
            g_openRegistrationRequested = true;
        }

        void DrawColorAndRegistration(bool& anyActive)
        {
            const float width = ImGuiMCP::GetContentRegionAvail().x;
            const auto tint = RuntimeTint(g_editor.CurrentLight()).value_or(LightTint{1, 1, 1});
            static int colorSlot = 0;
            // A swatch and text share one click target.
            const float size = ImGuiMCP::GetFontSize() * .72F;
            const float colorWidth = ButtonWidth(Tr("ライトの色")) + size + 10.0F;
            if (ImGuiMCP::Button("##color_button", {colorWidth, 0})) {
                g_editor.FinishNumericEdit();
                colorSlot = g_editor.SelectedSlot();
                ImGuiMCP::OpenPopup("light_color");
            }
            const auto min = ImGuiMCP::GetItemRectMin();
            const auto max = ImGuiMCP::GetItemRectMax();
            const auto padding = ImGuiMCP::GetStyle()->FramePadding;
            const auto textSize = ImGuiMCP::CalcTextSize(Tr("ライトの色"));
            auto* drawList = ImGuiMCP::GetWindowDrawList();
            DrawCenteredText(drawList, {min.x + padding.x + textSize.x * .5F, (min.y + max.y) * .5F}, Tr("ライトの色"),
                Color(238, 243, 248), textSize.x);
            const ImVec2 swatchMin{max.x - padding.x - size, (min.y + max.y - size) * .5F + 1.0F};
            const ImVec2 swatchMax{max.x - padding.x, swatchMin.y + size};
            ImGuiMCP::ImDrawListManager::AddRectFilled(drawList, swatchMin, swatchMax,
                Color(static_cast<std::uint8_t>(tint.red * 255), static_cast<std::uint8_t>(tint.green * 255), static_cast<std::uint8_t>(tint.blue * 255)), 2, 0);
            ImGuiMCP::ImDrawListManager::AddRect(drawList, swatchMin, swatchMax, Color(155, 165, 178), 2, 0, 1);
            // Draw sibling before popup so SameLine does not depend on popup last-item state.
            const float registrationGap = ImGuiMCP::GetStyle()->ItemSpacing.x + ImGuiMCP::GetFontSize() * .6F;
            if (colorWidth + registrationGap + ButtonWidth(Tr("ライト登録")) <= width) { ImGuiMCP::SameLine(0, registrationGap); }
            ImGuiMCP::BeginDisabled(!g_library);
            if (SelectableButton(Tr("ライト登録"))) { RequestRegistration(false); }
            ImGuiMCP::EndDisabled();
            if (ImGuiMCP::IsItemHovered()) { Tooltip(Tr("選択中のライトの種類・色を登録します。位置・強さ・範囲・影は含みません。")); }

            ImGuiMCP::PushStyleColor(ImGuiMCP::ImGuiCol_PopupBg, Color(12, 18, 27));
            if (ImGuiMCP::BeginPopup("light_color")) {
                if (colorSlot != g_editor.SelectedSlot() || !g_editor.CurrentLight().placed) { ImGuiMCP::CloseCurrentPopup(); }
                else {
                    ImGuiMCP::Text(Tr("Light %d : ライトの色"), colorSlot + 1);
                    ImGuiMCP::TextDisabled(Tr("色だけ変更 / 戻るで取り消し"));
                    constexpr std::array<LightTint, 8> palette{{
                        {1,1,1}, {1,0.8F,0.53F}, {1,0.6F,0.29F}, {0.3F,0.55F,1},
                        {1,0.25F,0.2F}, {0.35F,1,0.45F}, {0.3F,1,1}, {0.85F,0.4F,1}
                    }};
                    const float swatch = ImGuiMCP::GetFontSize() * 1.25F;
                    for (int index = 0; index < static_cast<int>(palette.size()); ++index) {
                        ImGuiMCP::PushID(index);
                        const auto& color = palette[index];
                        if (ImGuiMCP::ColorButton("##swatch", {color.red, color.green, color.blue, 1}, ImGuiMCP::ImGuiColorEditFlags_NoTooltip, {swatch, swatch})) {
                            g_editor.FinishNumericEdit();
                            g_editor.SetTint(color);
                            g_editor.FinishNumericEdit();
                        }
                        ImGuiMCP::PopID();
                        if (index != 3 && index != 7) { ImGuiMCP::SameLine(); }
                    }
                    const auto value = RuntimeTint(g_editor.CurrentLight()).value();
                    float rgb[]{value.red, value.green, value.blue};
                    ImGuiMCP::SetNextItemWidth(std::min(ImGuiMCP::GetFontSize() * 13.0F, ImGuiMCP::GetMainViewport()->WorkSize.x * 0.8F));
                    if (ImGuiMCP::ColorPicker3("##picker", rgb, ImGuiMCP::ImGuiColorEditFlags_DisplayRGB |
                        ImGuiMCP::ImGuiColorEditFlags_Uint8 | ImGuiMCP::ImGuiColorEditFlags_PickerHueBar |
                        ImGuiMCP::ImGuiColorEditFlags_NoSidePreview | ImGuiMCP::ImGuiColorEditFlags_NoAlpha)) {
                        g_editor.SetTint({rgb[0], rgb[1], rgb[2]});
                    }
                    anyActive = anyActive || ImGuiMCP::IsAnyItemActive();
                    if (SelectableButton(Tr("種類の元の色に戻す"))) { g_editor.SetType(g_editor.CurrentLight().type); }
                    if (SelectableButton(Tr("閉じる"))) { g_editor.FinishNumericEdit(); ImGuiMCP::CloseCurrentPopup(); }
                }
                ImGuiMCP::EndPopup();
            }
            ImGuiMCP::PopStyleColor();
        }

        void DrawRegistration()
        {
            if (g_openRegistrationRequested) {
                ImGuiMCP::OpenPopup(Tr("登録###sla_save"));
                g_openRegistrationRequested = false;
            }
            bool open = true;
            const auto viewport = ImGuiMCP::GetMainViewport();
            const float maximumWidth = viewport->WorkSize.x * .95F;
            ImGuiMCP::SetNextWindowSize({std::min(ImGuiMCP::GetFontSize() * 22.0F, maximumWidth), 0}, ImGuiMCP::ImGuiCond_Appearing);
            ImGuiMCP::SetNextWindowSizeConstraints({std::min(300.0F, maximumWidth), 0}, {maximumWidth, viewport->WorkSize.y * .95F});
            ImGuiMCP::PushStyleColor(ImGuiMCP::ImGuiCol_PopupBg, Color(12, 18, 27));
            if (ImGuiMCP::BeginPopupModal(Tr("登録###sla_save"), &open, ImGuiMCP::ImGuiWindowFlags_AlwaysAutoResize)) {
                ImGuiMCP::TextUnformatted(g_registerScene ? Tr("プリセット登録") : Tr("ライト登録（種類・色）"));
                ImGuiMCP::TextWrapped(Tr("登録ボタンを押した時点の設定を保存します。名前は省略しても登録できます。"));
                ImGuiMCP::SetNextItemWidth(std::max(1.0F, std::min(ImGuiMCP::GetFontSize() * 15.0F,
                    ImGuiMCP::GetContentRegionAvail().x - ImGuiMCP::CalcTextSize(Tr("名前")).x - ImGuiMCP::GetStyle()->ItemInnerSpacing.x)));
                if (ImGuiMCP::InputText(Tr("名前"), g_registrationName.data(), g_registrationName.size())) {
                    g_registrationError.clear();
                }
                if (g_registerScene) {
                    ImGuiMCP::Checkbox(Tr("フェイスライトも含める"), &g_saveFace);
                    ImGuiMCP::TextWrapped(Tr("通常3灯の位置・微調整・種類・色・強さ・範囲・影・ON/OFFを保存します。正面の基準と照明全体の開始状態は保存しません。"));
                } else {
                    ImGuiMCP::Text("Light %d / %s", g_registrationSlot + 1, Tr(kLightTypes[g_registrationSnapshot[g_registrationSlot].type].name));
                    ImGuiMCP::TextWrapped(Tr("登録はライト選択の候補を増やします。灯数や現在の照明は変えません。"));
                }
                if (SelectableButton(Tr("登録する")) && g_library) {
                    std::string name = g_registrationName.data();
                    if (name.find_first_not_of(' ') == std::string::npos) {
                        int serial = 1;
                        const auto prefix = g_registerScene ? Tr("マイセット ") : Tr("マイライト ");
                        do { name = prefix + std::to_string(serial++); }
                        while (g_registerScene ?
                            std::any_of(g_library->Scenes().begin(), g_library->Scenes().end(), [&](const auto& e) { return e.name == name; }) :
                            std::any_of(g_library->Lights().begin(), g_library->Lights().end(), [&](const auto& e) { return e.name == name; }));
                    }
                    const bool saved = g_registerScene ? g_library->SaveScene(name, g_registrationSnapshot, g_saveFace) :
                        g_library->SaveLight(name, g_registrationSnapshot[g_registrationSlot]);
                    if (saved) {
                        g_libraryNotice = Tr("登録しました : ") + name;
                        logger::info("Saved {} preset: {}", g_registerScene ? "scene" : "light", name);
                        ImGuiMCP::CloseCurrentPopup();
                    } else { g_registrationError = g_library->Error(); }
                }
                ImGuiMCP::SameLine();
                if (SelectableButton(Tr("キャンセル"))) { ImGuiMCP::CloseCurrentPopup(); }
                if (!g_registrationError.empty()) { ImGuiMCP::TextWrapped("%s", StoredError(g_registrationError).c_str()); }
                ImGuiMCP::EndPopup();
            }
            ImGuiMCP::PopStyleColor();
        }

        void DrawCameraAlignment(const RuntimeLight::View& a_runtime, const char* a_label)
        {
            ImGuiMCP::BeginDisabled(!a_runtime.session.running);
            if (SelectableButton(a_label)) {
                g_editor.FinishNumericEdit();
                RuntimeLight::Align(a_runtime.session.epoch);
            }
            ImGuiMCP::EndDisabled();
            if (ImGuiMCP::IsItemHovered()) {
                Tooltip(Tr("現在のカメラを3灯共通の正面にします。構図の変更後、必要なときだけ更新してください。\n基準点は胸（取得できない場合は頭、さらに取得できなければ足元＋1.2m）に追従します。\n正面の更新は設定の履歴には記録されません。"));
            }
        }

        void DrawPositionWindow(bool& a_anyActive)
        {
            if (!g_positionWindowOpen) { return; }
            const auto* viewport = ImGuiMCP::GetMainViewport();
            const auto* style = ImGuiMCP::GetStyle();
            const float font = ImGuiMCP::GetFontSize();
            const float frame = ImGuiMCP::GetFrameHeight();
            const float labelWidth = std::max({ ImGuiMCP::CalcTextSize(Tr("左右 (+右)")).x,
                ImGuiMCP::CalcTextSize(Tr("上下 (+上)")).x, ImGuiMCP::CalcTextSize(Tr("前後 (+手前)")).x });
            const float historyWidth = ButtonWidth(Tr("戻る")) + ButtonWidth(Tr("進む")) + style->ItemSpacing.x;
            const float resetWidth = ButtonWidth(Tr("基準に戻す"));
            const float footerWidth = resetWidth + historyWidth + style->ItemSpacing.x;
            const float pairWidth = std::max(frame, ButtonWidth("3")) + std::max(22.0F, frame * 0.65F) + 3.0F;
            const float selectorWidth = LightSlotRowWidth(ImGuiMCP::CalcTextSize(Tr("ライト")).x, pairWidth,
                style->ItemSpacing.x, LightSlotGroupGap(font, style->ItemSpacing.x));
            const float requiredWidth = std::max({ labelWidth + font * 5.0F + style->ItemSpacing.x,
                footerWidth, selectorWidth }) + style->WindowPadding.x * 2.0F + style->ScrollbarSize;
            const float desiredHeight = std::clamp(font * 8.0F, 180.0F, 260.0F) + frame * 7.0F +
                style->ItemSpacing.y * 8.0F + style->WindowPadding.y * 2.0F + 2.0F;
            const auto hostPosition = ImGuiMCP::GetWindowPos();
            const auto hostSize = ImGuiMCP::GetWindowSize();
            const auto layout = CalculatePositionWindowLayout(
                { viewport->WorkPos.x, viewport->WorkPos.y, viewport->WorkSize.x, viewport->WorkSize.y },
                { hostPosition.x, hostPosition.y, hostSize.x, hostSize.y }, requiredWidth, desiredHeight);
            ImGuiMCP::SetNextWindowPos({ layout.initial.x, layout.initial.y }, ImGuiMCP::ImGuiCond_FirstUseEver);
            ImGuiMCP::SetNextWindowSize({ layout.initial.width, layout.initial.height }, ImGuiMCP::ImGuiCond_FirstUseEver);
            ImGuiMCP::SetNextWindowSizeConstraints({ layout.minimumWidth, layout.minimumHeight }, { layout.maximumWidth, layout.maximumHeight });
            if (g_focusPositionWindow) {
                ImGuiMCP::SetNextWindowFocus();
                g_focusPositionWindow = false;
            }
            // A normal window, not a popup: the main Lighting pane remains interactive.
            // It is submitted only by this page, so it cannot outlive the host menu/page.
            // A new stable ID gives the compact layout its own initial size,
            // without overwriting the user's old window geometry or game settings.
            const auto title = Fmt(Tr("微調整 : Light {}{}###sla_fine_position"), g_editor.SelectedSlot() + 1,
                g_editor.CurrentLight().placed && !g_editor.CurrentLight().enabled ? " [OFF]" : "");
            ImGuiMCP::PushStyleColor(ImGuiMCP::ImGuiCol_WindowBg, Color(12, 18, 27, 242));
            if (ImGuiMCP::Begin(title.c_str(), &g_positionWindowOpen, ImGuiMCP::ImGuiWindowFlags_NoCollapse)) {
                DrawSlotSelector(ImGuiMCP::GetContentRegionAvail().x, Tr("ライト"));
                ImGuiMCP::Separator();
                if (!g_editor.CurrentLight().placed) {
                    ImGuiMCP::TextWrapped(Tr("未配置 : メイン画面で種類を選ぶと配置できます。"));
                }
                ImGuiMCP::PushItemWidth(std::max(1.0F, ImGuiMCP::GetContentRegionAvail().x - labelWidth - style->ItemSpacing.x));
                ImGuiMCP::BeginDisabled(!g_editor.CurrentLight().placed);
                DrawFinePosition(a_anyActive);
                ImGuiMCP::EndDisabled();
                ImGuiMCP::PopItemWidth();
                const auto footerOrigin = ImGuiMCP::GetCursorScreenPos();
                const auto footer = CalculateFinePositionFooter(ImGuiMCP::GetContentRegionAvail().x,
                    resetWidth, historyWidth, style->ItemSpacing.x);
                DrawFinePositionReset();
                ImGuiMCP::SetCursorScreenPos({ footerOrigin.x + footer.historyX,
                    footer.sameRow ? footerOrigin.y : ImGuiMCP::GetCursorScreenPos().y });
                DrawHistory(true);
            }
            ImGuiMCP::End();
            ImGuiMCP::PopStyleColor();
        }

        void DrawPrototypeControls(bool& anySliderActive)
        {
            DrawPlacementAndLightSelection(anySliderActive);

            ImGuiMCP::Separator();
            std::array<char, 128> settingsHeading{};
            std::snprintf(settingsHeading.data(), settingsHeading.size(),
                Tr("3. 基本設定 / 編集中 : Light %d"), g_editor.SelectedSlot() + 1);
            const float settingsWidth = ImGuiMCP::GetContentRegionAvail().x;
            const float settingsGap = ImGuiMCP::GetStyle()->ItemSpacing.x + ImGuiMCP::GetFontSize() * .5F;
            const bool resetOnHeading = FitsInline(settingsWidth, ImGuiMCP::CalcTextSize(settingsHeading.data()).x,
                ButtonWidth(Tr("初期値")), settingsGap);
            if (resetOnHeading) {
                ImGuiMCP::TextUnformatted(settingsHeading.data());
                ImGuiMCP::SameLine(0, settingsGap);
            } else {
                ImGuiMCP::TextWrapped("%s", settingsHeading.data());
            }
            ImGuiMCP::BeginDisabled(!g_editor.CanResetCurrentValues());
            if (SelectableButton(Tr("初期値"))) { g_editor.ResetCurrentValues(); }
            ImGuiMCP::EndDisabled();
            if (ImGuiMCP::IsItemHovered()) {
                Tooltip(Tr("選択中のライトの強さ・範囲・距離・高さ・微調整・影の補正を初期値へ戻します。\n配置方向・種類・色・影の方式・点灯状態は変更しません。「戻る」で取り消せます。"));
            }
            if (!g_editor.CurrentLight().placed) {
                ImGuiMCP::TextWrapped(Tr("未配置 : 位置を選び、ライトを選択してください。"));
            } else if (!g_editor.CurrentLight().enabled) {
                ImGuiMCP::TextWrapped(Tr("OFF : 設定は保持されています。消灯中も編集できます。"));
            }
            ImGuiMCP::BeginDisabled(!g_editor.CurrentLight().placed);
            const float basicLabelWidth = std::max({ ImGuiMCP::CalcTextSize(Tr("強さ")).x,
                ImGuiMCP::CalcTextSize(Tr("光の範囲")).x, ImGuiMCP::CalcTextSize(Tr("対象からの距離")).x,
                ImGuiMCP::CalcTextSize(Tr("基準点からの高さ")).x });
            ImGuiMCP::PushItemWidth(std::max(1.0F, ImGuiMCP::GetContentRegionAvail().x - basicLabelWidth - ImGuiMCP::GetStyle()->ItemSpacing.x));
            DrawNumeric(Tr("強さ"), NumericField::Intensity, 0.0F, 3.0F, "%.2f", anySliderActive);
            const auto* style = ImGuiMCP::GetStyle();
            const float rangeSuffix = ImGuiMCP::CalcTextSize(Tr("光の範囲")).x + style->ItemInnerSpacing.x + style->ItemSpacing.x + ButtonWidth(Tr("広域"));
            const auto rangeLayout = CalculateRangeControl(ImGuiMCP::GetContentRegionAvail().x,
                basicLabelWidth + style->ItemSpacing.x, rangeSuffix, ImGuiMCP::GetFontSize() * 3.0F);
            ImGuiMCP::PushItemWidth(rangeLayout.sliderWidth);
            DrawNumeric(Tr("光の範囲"), NumericField::Range, 0.5F, 12.0F, "%.1f m", anySliderActive);
            ImGuiMCP::PopItemWidth();
            if (rangeLayout.sameRow) { ImGuiMCP::SameLine(); }
            if (SelectableButton(Tr("広域"), g_editor.CurrentLight().range == kWideLightRange)) { g_editor.SetWideRange(); }
            if (ImGuiMCP::IsItemHovered()) {
                Tooltip(Tr("選択中のライトの範囲だけを12mにします。色・強さ・位置・影・ON/OFFは保持します。\nスライダーで再調整でき、「戻る」で元の範囲へ戻せます。照射角や影の柔らかさは変えません。"));
            }
            DrawNumeric(Tr("対象からの距離"), NumericField::Distance, 0.25F, 8.0F, "%.2f m", anySliderActive);
            if (ImGuiMCP::IsItemHovered()) {
                Tooltip(Tr("基本方向に沿った距離です。別窓の微調整は追加量として保持します。前後への平行移動とは別です。"));
            }
            DrawNumeric(Tr("基準点からの高さ"), NumericField::Height, -2.0F, 2.0F, "%+.2f m", anySliderActive);
            if (ImGuiMCP::IsItemHovered()) {
                Tooltip(Tr("基本配置の高さです。別窓の上下は追加量として保持します。"));
            }
            ImGuiMCP::PopItemWidth();
            DrawShadowModeControls();
            if (g_editor.CurrentLight().castsShadow && g_editor.CurrentLight().shadowProjection == ShadowProjection::Spot) {
                DrawShadowBiasControls();
            }
            if (HasFinePosition(g_editor.CurrentLight().fine)) {
                const auto fine = g_editor.CurrentLight().fine;
                ImGuiMCP::TextWrapped(Tr("微調整あり : 右 %+.2fm / 上 %+.2fm / 手前 %+.2fm"), fine.horizontal, fine.vertical, fine.depth);
            }
            DrawColorAndRegistration(anySliderActive);
            ImGuiMCP::EndDisabled();

            ImGuiMCP::Separator();
            const float presetRowWidth = ImGuiMCP::GetContentRegionAvail().x;
            if (SelectableButton(Tr("プリセットを選ぶ"))) {
                g_editor.FinishNumericEdit();
                g_openPresetRequested = true;
            }
            const float presetGap = ImGuiMCP::GetStyle()->ItemSpacing.x + ImGuiMCP::GetFontSize() * .6F;
            if (FitsInline(presetRowWidth, ButtonWidth(Tr("プリセットを選ぶ")), ButtonWidth(Tr("プリセット登録")), presetGap)) { ImGuiMCP::SameLine(0, presetGap); }
            ImGuiMCP::BeginDisabled(!g_library);
            if (SelectableButton(Tr("プリセット登録"))) { RequestRegistration(true); }
            ImGuiMCP::EndDisabled();
            DrawHistory();
            ImGuiMCP::PushID("bookmark_bottom");
            DrawBookmark();
            ImGuiMCP::PopID();
            if (!g_libraryNotice.empty()) { ImGuiMCP::TextWrapped("%s", g_libraryNotice.c_str()); }
            if (!g_libraryLoadError.empty()) { ImGuiMCP::TextWrapped("%s", StoredError(g_libraryLoadError).c_str()); }
            if (g_library && !g_library->ViewError().empty()) { ImGuiMCP::TextWrapped("%s", StoredError(g_library->ViewError()).c_str()); }
            ImGuiMCP::TextDisabled(Tr("登録の保存先 (?) / 0.1.29"));
            if (ImGuiMCP::IsItemHovered() && g_library) {
                const auto utf8 = g_library->Directory().u8string();
                Tooltip(Tr("%s\nMO2ではこの仮想Dataパスへの新規ファイルが通常Overwriteへ入ります。\n登録は再起動後も残り、ゲームのセーブとは独立です。\n登録から外したファイルは、この中のArchivedへ退避します。\n履歴・栞の登録は起動中のみの一時退避です。"), reinterpret_cast<const char*>(utf8.c_str()));
            }
        }


        void DrawTargetPicker(const RuntimeLight::View& runtime)
        {
            const float width = ImGuiMCP::GetContentRegionAvail().x;
            ImGuiMCP::BeginDisabled(!runtime.session.ready || runtime.session.blocked || runtime.targetBusy);
            if (SelectableButton(Tr("対象を選ぶ"))) {
                g_editor.FinishNumericEdit();
                RuntimeLight::RequestTarget(runtime.session.epoch, RuntimeLight::TargetAction::Refresh);
                ImGuiMCP::OpenPopup(Tr("撮影対象###sla_target"));
            }
            ImGuiMCP::EndDisabled();
            const std::string label = runtime.npcTarget ? runtime.targetName : Tr("プレイヤー");
            if (ButtonWidth(Tr("対象を選ぶ")) + ImGuiMCP::GetStyle()->ItemSpacing.x + ImGuiMCP::CalcTextSize(label.c_str()).x <= width) { ImGuiMCP::SameLine(); }
            ImGuiMCP::TextWrapped("%s", label.c_str());
            if (ImGuiMCP::IsItemHovered() && runtime.npcTarget) { Tooltip("RefID: %08X", runtime.targetFormID); }
            const auto* viewport = ImGuiMCP::GetMainViewport();
            const ImVec2 maximum{viewport->WorkSize.x * .95F, viewport->WorkSize.y * .95F};
            ImGuiMCP::SetNextWindowSize({std::min(600.0F, maximum.x), std::min(650.0F, maximum.y)}, ImGuiMCP::ImGuiCond_FirstUseEver);
            ImGuiMCP::SetNextWindowSizeConstraints({std::min(320.0F, maximum.x), std::min(320.0F, maximum.y)}, maximum);
            ImGuiMCP::PushStyleColor(ImGuiMCP::ImGuiCol_PopupBg, Color(12, 18, 27));
            bool open = true;
            if (ImGuiMCP::BeginPopupModal(Tr("撮影対象###sla_target"), &open)) {
                ImGuiMCP::TextWrapped(Tr("現在の対象 : %s"), label.c_str());
                ImGuiMCP::TextWrapped(Tr("周囲30mの読み込み済み人型NPC（最大64人）。対象変更で消灯しますが、照明設定は保持します。"));
                ImGuiMCP::BeginDisabled(runtime.targetBusy || !runtime.session.ready || runtime.session.blocked);
                if (SelectableButton(Tr("候補を更新"))) { RuntimeLight::RequestTarget(runtime.session.epoch, RuntimeLight::TargetAction::Refresh); }
                if (SelectableButton(Tr("プレイヤーに戻す"))) { RuntimeLight::RequestTarget(runtime.session.epoch, RuntimeLight::TargetAction::Player); }
                if (SelectableButton(Tr("コンソールの選択対象"))) { RuntimeLight::RequestTarget(runtime.session.epoch, RuntimeLight::TargetAction::Console); }
                if (SelectableButton(Tr("クロスヘアの対象"))) { RuntimeLight::RequestTarget(runtime.session.epoch, RuntimeLight::TargetAction::Crosshair); }
                if (ImGuiMCP::IsItemHovered()) {
                    Tooltip(Tr("ゲームが現在認識している対象を取得します。フリーカメラやメニュー中に取得できない場合は一覧を使ってください。"));
                }
                ImGuiMCP::Separator();
                const float footer = ImGuiMCP::GetFrameHeightWithSpacing() * 4.0F;
                if (ImGuiMCP::BeginChild("npc_candidates", {0, std::max(80.0F, ImGuiMCP::GetContentRegionAvail().y - footer)}, 0)) {
                    for (std::size_t i = 0; i < runtime.candidates.size(); ++i) {
                        const auto& entry = runtime.candidates[i];
                        ImGuiMCP::PushID(static_cast<int>(i));
                        const auto text = Fmt("{} / {:.1f} m [{:08X}]", entry.name, entry.metres, entry.formID);
                        if (ImGuiMCP::Selectable(text.c_str(), runtime.npcTarget && runtime.targetFormID == entry.formID)) {
                            RuntimeLight::RequestTarget(runtime.session.epoch, RuntimeLight::TargetAction::Nearby, runtime.targetRevision, i);
                        }
                        ImGuiMCP::PopID();
                    }
                    if (runtime.candidates.empty()) { ImGuiMCP::TextWrapped(Tr("候補はありません。NPCの近くで更新してください。")); }
                }
                ImGuiMCP::EndChild();
                ImGuiMCP::EndDisabled();
                if (runtime.targetBusy) { ImGuiMCP::TextWrapped(Tr("ゲーム側の処理を待っています。")); }
                if (!runtime.targetNotice.empty()) { ImGuiMCP::TextWrapped("%s", Tr(runtime.targetNotice.c_str())); }
                if (SelectableButton(Tr("閉じる"))) { ImGuiMCP::CloseCurrentPopup(); }
                ImGuiMCP::EndPopup();
            }
            ImGuiMCP::PopStyleColor();
        }

        void __stdcall Render()
        {
            // The token comes from before this UI frame. A load/stop invalidates it,
            // so a delayed frame cannot resurrect a light from the previous session.
            const auto runtime = RuntimeLight::GetView();
            const float topWidth = ImGuiMCP::GetContentRegionAvail().x;
            const char* powerLabel = runtime.session.running ? Tr("撮影ライトを停止") : Tr("撮影ライトを開始");
            ImGuiMCP::BeginDisabled(!runtime.session.ready || runtime.session.blocked);
            if (SelectableButton(powerLabel, runtime.session.running)) {
                if (runtime.session.running) {
                    RuntimeLight::Stop(runtime.session.epoch);
                } else {
                    RuntimeLight::Start(runtime.session.epoch, g_editor.GetScene());
                }
            }
            ImGuiMCP::EndDisabled();
            if (ImGuiMCP::IsItemHovered()) {
                Tooltip(Tr("%s\n\nLight 1～3と独立したフェイスライトを実機反映します。\nメニューを閉じても点灯を維持します。停止・ロード・場所の移動ではフェイスを含む全灯を撤去し、自動再開しません。\n通常3灯は正面更新までカメラ基準を固定します。フェイスだけは頭・現在のカメラ位置へ自動追従します。"), runtime.status.c_str());
            }
            if (ButtonWidth(powerLabel) + ButtonWidth("P") + ImGuiMCP::GetStyle()->ItemSpacing.x * 2 <= topWidth) { ImGuiMCP::SameLine(0, ImGuiMCP::GetStyle()->ItemSpacing.x * 2); }
            if (SelectableButton("P")) { g_editor.FinishNumericEdit(); g_openPresetRequested = true; }
            if (ImGuiMCP::IsItemHovered()) { Tooltip(Tr("プリセットを選ぶ（下のボタンと同じ一覧）")); }
            const char* languageLabel = Current() == Language::Japanese ? "EN" : "JP";
            const float languageWidth = ButtonWidth(languageLabel);
            if (ImGuiMCP::GetContentRegionAvail().x >= languageWidth &&
                ButtonWidth(powerLabel) + ButtonWidth("P") + languageWidth + ImGuiMCP::GetStyle()->ItemSpacing.x * 4 <= topWidth) { ImGuiMCP::SameLine(); }
            if (SelectableButton(languageLabel)) {
                g_editor.FinishNumericEdit();
                const auto next = Current() == Language::Japanese ? Language::English : Language::Japanese;
                if (g_languagePath.empty()) { g_languageError = "言語設定を保存できません。この起動中だけ切り替えます。"; }
                else { SavePreference(g_languagePath, next, g_languageError); }
                Set(next);
                g_libraryNotice.clear();
            }
            if (ImGuiMCP::IsItemHovered()) { Tooltip("%s", Tr("日本語／英語を切り替えます。名前・照明・登録データは変えません。")); }
            if (!g_languageError.empty()) { ImGuiMCP::TextWrapped("%s", Tr(g_languageError.c_str())); }
            if (runtime.statusImportant) { ImGuiMCP::TextWrapped("%s", runtime.status.c_str()); }
            DrawTargetPicker(runtime);
            DrawCameraAlignment(runtime, Tr("現在のカメラを正面にする"));
            const float bookmarkGap = ImGuiMCP::GetStyle()->ItemSpacing.x + ImGuiMCP::GetFontSize() * .6F;
            const float bookmarkWidth = BookmarkWidth();
            if (ButtonWidth(Tr("現在のカメラを正面にする")) + bookmarkGap + bookmarkWidth <= topWidth) {
                ImGuiMCP::SameLine(0, bookmarkGap);
            }
            ImGuiMCP::PushID("bookmark_top");
            ImGuiMCP::BeginGroup();
            DrawBookmark(true);
            ImGuiMCP::EndGroup();
            ImGuiMCP::PopID();
            ImGuiMCP::Separator();
            bool anyActive = false;
            DrawPrototypeControls(anyActive);
            DrawPositionWindow(anyActive);
            if (g_openPresetRequested) {
                g_managePresets = false;
                g_presetPicker.Open();
                g_focusPresetWindow = true;
                g_openPresetRequested = false;
            }
            DrawPresetPicker();
            DrawLightLibrary();
            DrawRegistration();
            // Both surfaces edit one scene. A gesture in the floating window must
            // not be committed once per frame by an idle main settings pane.
            if (!anyActive) { g_editor.FinishNumericEdit(); }
            RuntimeLight::Submit(runtime.session.epoch, g_editor.GetScene());
        }
    }

    void Register()
    {
        if (g_registered) {
            return;
        }
        if (!SKSEMenuFramework::IsInstalled()) {
            logger::warn("SKSE Menu Framework was not found; the lighting assistant menu was not registered");
            return;
        }

        SKSEMenuFramework::SetSection("Screenshot Lighting Assistant");
        const auto preferredRoot = PreferredStorageRoot();
        const auto logDirectory = SKSE::log::log_directory();
        const auto legacyRoot = logDirectory ? *logDirectory / "ScreenshotLightingAssistant" : std::filesystem::path{};
        const auto storage = PrepareStorageLocation(preferredRoot, legacyRoot);
        if (!storage.root.empty()) {
            g_languagePath = storage.root / "ui-language.sla";
            LoadPreference(g_languagePath, g_languageError);
            g_libraryNotice = storage.notice.empty() ? std::string{} : Tr(storage.notice.c_str());
            g_library = std::make_unique<PresetLibrary>(storage.root / "Presets");
            g_library->Load();
            g_libraryLoadError = g_library->Error();
            logger::info("Preset library: {} lights, {} scenes at {}{}", g_library->Lights().size(),
                g_library->Scenes().size(), storage.root.string(), storage.usingLegacy ? " (legacy fallback)" : "");
        } else {
            g_libraryNotice = Tr("保存先が取得できないため登録を利用できません。");
        }
        SKSEMenuFramework::AddSectionItem("Lighting", Render);
        g_registered = true;
        logger::info("Registered Screenshot Lighting Assistant with SKSE Menu Framework");
    }
}
