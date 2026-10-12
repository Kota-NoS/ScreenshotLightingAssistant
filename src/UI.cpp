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
#include "ScreenOverlaySettings.h"
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
        bool g_persistentFaceWindowOpen{ false };
        bool g_focusPersistentFaceWindow{ false };
        bool g_photoEffectWindowOpen{ false };
        bool g_focusPhotoEffectWindow{ false };
        bool g_screenOverlayWindowOpen{ false };
        bool g_focusScreenOverlayWindow{ false };
        bool g_toolHotkeyWindowOpen{ false };
        bool g_focusToolHotkeyWindow{ false };
        ScreenOverlaySettings g_screenOverlay{};
        ScreenOverlayLayer g_screenOverlayDetail{ ScreenOverlayLayer::Vignette };
        SKSEMenuFramework::Model::InputEvent* g_frameworkInputEvent{};
        SKSEMenuFramework::Model::HudElement* g_screenOverlayHudElement{};
        SKSEMenuFramework::Model::HudElement* g_screenPlacementHudElement{};
        SKSEMenuFramework::Model::HudElement* g_toolWindowToggleHudElement{};
        SKSEMenuFramework::Model::WindowInterface* g_mainWindow{};
        SKSEMenuFramework::Model::WindowInterface* g_screenPlacementWindow{};
        SKSEMenuFramework::Model::WindowInterface* g_toolWindow{};
        bool g_toolWindowWasOpen{};
        bool g_returnToMainAfterScreenPlacement{};
        bool g_returnToToolWindowAfterScreenPlacement{};
        bool g_editScreenPlacement{};
        bool g_skipScreenPlacementClick{};
        bool g_screenPlacementPanelVisible{};
        bool g_screenPlacementColorPopupVisible{};
        ImVec2 g_screenPlacementPanelMin{};
        ImVec2 g_screenPlacementPanelMax{};
        std::unique_ptr<PresetLibrary> g_library;
        bool g_openPresetRequested{};
        bool g_openLightLibraryRequested{};
        bool g_managePresets{};
        bool g_openRegistrationRequested{};
        bool g_registerScene{};
        bool g_saveFace{};
        Scene g_registrationSnapshot{};
        std::optional<ScreenPlacementSettings> g_registrationScreenPlacement;
        int g_registrationSlot{};
        std::array<char, 121> g_registrationName{};
        std::string g_registrationError;
        std::string g_libraryNotice;
        std::string g_libraryLoadError;
        std::filesystem::path g_languagePath;
        std::string g_languageError;

        bool __stdcall CaptureFrameworkInput(RE::InputEvent* event)
        {
            if (!event || event->GetEventType() != RE::INPUT_EVENT_TYPE::kButton) { return false; }
            const auto* button = event->AsButtonEvent();
            if (!button) { return false; }
            if (event->GetDevice() == RE::INPUT_DEVICE::kKeyboard) {
                if (button->IsDown() && RuntimeLight::CapturePersistentHotkey(button->GetIDCode())) { return true; }
                return RuntimeLight::HandleToolWindowKeyboardEvent(button->GetIDCode(),
                    button->IsPressed(), button->IsDown());
            }
            if (event->GetDevice() == RE::INPUT_DEVICE::kGamepad && button->IsDown()) {
                return RuntimeLight::CapturePersistentGamepadHotkey(button->GetIDCode());
            }
            return false;
        }

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

        void DrawTintSelectionMark(const ImVec2& top, float size, LightTint tint)
        {
            const ImVec2 end{top.x + size, top.y + size};
            auto* drawList = ImGuiMCP::GetWindowDrawList();
            ImGuiMCP::ImDrawListManager::AddRect(drawList, top, end,
                Color(255, 202, 102), 3.0F, 0, 2.0F);
            const float luminance = tint.red * .299F + tint.green * .587F + tint.blue * .114F;
            const auto check = luminance > .68F ? Color(24, 31, 39) : Color(244, 248, 251);
            const ImVec2 first{top.x + size * .25F, top.y + size * .54F};
            const ImVec2 middle{top.x + size * .43F, top.y + size * .72F};
            const ImVec2 last{top.x + size * .76F, top.y + size * .30F};
            ImGuiMCP::ImDrawListManager::AddLine(drawList, first, middle, check, 2.0F);
            ImGuiMCP::ImDrawListManager::AddLine(drawList, middle, last, check, 2.0F);
        }

        bool FaceTintPresetSwatch(const char* id, const char* name, LightTint tint, bool selected)
        {
            const float size = ImGuiMCP::GetFrameHeight();
            const bool clicked = ImGuiMCP::ColorButton(id, {tint.red, tint.green, tint.blue, 1.0F},
                ImGuiMCP::ImGuiColorEditFlags_NoTooltip, {size, size});
            if (selected) { DrawTintSelectionMark(ImGuiMCP::GetItemRectMin(), size, tint); }
            if (ImGuiMCP::IsItemHovered()) {
                Tooltip(Tr("%s。色だけを変更し、強さ・範囲・位置・ON/OFFは保持します。"), name);
            }
            return clicked;
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

        void DrawDirectionDiagram(const Scene& a_scene, const ImVec2& origin, float canvasSize, bool a_interactive,
            std::optional<bool> a_screenPlacementEnabled = std::nullopt)
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
                        g_editScreenPlacement = false;
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
                            g_editScreenPlacement = false;
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
                const ImVec2 cameraCenter{ center.x, origin.y + canvasSize + 8.0F * diagramScale };
                if (a_screenPlacementEnabled) {
                    const auto tint = *a_screenPlacementEnabled ? Color(255, 196, 82) : Color(125, 141, 158);
                    const float radius = 12.0F * diagramScale;
                    ImGuiMCP::ImDrawListManager::AddCircleFilled(drawList, cameraCenter, radius,
                        Color(26, 33, 43), 28);
                    ImGuiMCP::ImDrawListManager::AddCircle(drawList, cameraCenter, radius, tint, 28,
                        1.5F * diagramScale);
                }
                DrawCameraIcon(drawList, cameraCenter, diagramScale);
                if (a_screenPlacementEnabled && !*a_screenPlacementEnabled) {
                    ImGuiMCP::ImDrawListManager::AddLine(drawList,
                        {cameraCenter.x - 8.0F * diagramScale, cameraCenter.y + 8.0F * diagramScale},
                        {cameraCenter.x + 8.0F * diagramScale, cameraCenter.y - 8.0F * diagramScale},
                        Color(125, 141, 158), 1.5F * diagramScale);
                }
            }
        }

        float ButtonWidth(const char* a_label);
        void DrawPositionWindowButton();
        void DrawScreenPlacementLauncher(const RuntimeLight::View& runtime, const ImVec2& center);
        void UpdateScreenPlacementInteraction(bool controlsCaptureMouse = false);
        void __stdcall RenderScreenPlacementWindow();
        void __stdcall Render();
        void __stdcall RenderToolWindow();
        void __stdcall ProcessToolWindowToggle();

        float PersistentFaceLauncherWidth()
        {
            return std::max(ImGuiMCP::GetFrameHeight(), ButtonWidth("F"));
        }

        bool DrawPersistentFaceLauncher(bool selected)
        {
            const float width = PersistentFaceLauncherWidth();
            const float height = ImGuiMCP::GetFrameHeight();
            const ImVec2 top = ImGuiMCP::GetCursorScreenPos();
            const bool clicked = ImGuiMCP::InvisibleButton("##persistent_face_launcher", { width, height });
            const bool hovered = ImGuiMCP::IsItemHovered() || ImGuiMCP::IsItemFocused();
            const ImVec2 end{ top.x + width, top.y + height };
            auto* drawList = ImGuiMCP::GetWindowDrawList();
            ImGuiMCP::ImDrawListManager::AddRectFilled(drawList, top, end,
                selected ? Color(112, 80, 32) : hovered ? Color(43, 55, 69) : Color(26, 33, 43), 4.0F, 0);
            ImGuiMCP::ImDrawListManager::AddRect(drawList, top, end,
                selected ? Color(255, 202, 102) : hovered ? Color(150, 189, 220) : Color(78, 94, 111), 4.0F, 0, 1.5F);

            const ImVec2 center{ top.x + width * .5F, top.y + height * .5F };
            const float scale = std::min(width, height) / 32.0F;
            const auto tint = selected ? Color(255, 224, 155) : hovered ? Color(180, 215, 240) : Color(180, 201, 218);
            ImGuiMCP::ImDrawListManager::AddCircle(drawList, { center.x, center.y - 4.0F * scale }, 6.0F * scale,
                tint, 24, 1.5F * scale);
            ImGuiMCP::ImDrawListManager::AddLine(drawList, { center.x - 10.0F * scale, center.y + 9.0F * scale },
                { center.x - 5.0F * scale, center.y + 3.0F * scale }, tint, 1.5F * scale);
            ImGuiMCP::ImDrawListManager::AddLine(drawList, { center.x - 5.0F * scale, center.y + 3.0F * scale },
                { center.x + 5.0F * scale, center.y + 3.0F * scale }, tint, 1.5F * scale);
            ImGuiMCP::ImDrawListManager::AddLine(drawList, { center.x + 5.0F * scale, center.y + 3.0F * scale },
                { center.x + 10.0F * scale, center.y + 9.0F * scale }, tint, 1.5F * scale);
            return clicked;
        }

        float PhotoEffectLauncherWidth()
        {
            return std::max(ImGuiMCP::GetFrameHeight(), ButtonWidth("*"));
        }

        bool DrawPhotoEffectLauncher(bool selected)
        {
            const float width = PhotoEffectLauncherWidth();
            const float height = ImGuiMCP::GetFrameHeight();
            const ImVec2 top = ImGuiMCP::GetCursorScreenPos();
            const bool clicked = ImGuiMCP::InvisibleButton("##photo_effect_launcher", {width, height});
            const bool hovered = ImGuiMCP::IsItemHovered() || ImGuiMCP::IsItemFocused();
            const ImVec2 end{top.x + width, top.y + height};
            auto* drawList = ImGuiMCP::GetWindowDrawList();
            ImGuiMCP::ImDrawListManager::AddRectFilled(drawList, top, end,
                selected ? Color(112, 80, 32) : hovered ? Color(43, 55, 69) : Color(26, 33, 43), 4.0F, 0);
            ImGuiMCP::ImDrawListManager::AddRect(drawList, top, end,
                selected ? Color(255, 202, 102) : hovered ? Color(150, 189, 220) : Color(78, 94, 111), 4.0F, 0, 1.5F);

            const ImVec2 center{top.x + width * .5F, top.y + height * .5F};
            const float scale = std::min(width, height) / 32.0F;
            const auto tint = selected ? Color(255, 224, 155) : hovered ? Color(210, 229, 245) : Color(190, 208, 224);
            ImGuiMCP::ImDrawListManager::AddLine(drawList, {center.x, center.y - 10.0F * scale},
                {center.x, center.y + 10.0F * scale}, tint, 1.7F * scale);
            ImGuiMCP::ImDrawListManager::AddLine(drawList, {center.x - 8.0F * scale, center.y},
                {center.x + 8.0F * scale, center.y}, tint, 1.7F * scale);
            ImGuiMCP::ImDrawListManager::AddLine(drawList, {center.x - 5.5F * scale, center.y - 5.5F * scale},
                {center.x + 5.5F * scale, center.y + 5.5F * scale}, tint, 1.2F * scale);
            ImGuiMCP::ImDrawListManager::AddLine(drawList, {center.x + 5.5F * scale, center.y - 5.5F * scale},
                {center.x - 5.5F * scale, center.y + 5.5F * scale}, tint, 1.2F * scale);
            ImGuiMCP::ImDrawListManager::AddCircleFilled(drawList,
                {center.x + 9.0F * scale, center.y - 8.0F * scale}, 1.8F * scale, tint, 12);
            return clicked;
        }

        float ScreenOverlayLauncherWidth()
        {
            return std::max(ImGuiMCP::GetFrameHeight(), ButtonWidth("O"));
        }

        bool DrawScreenOverlayLauncher(bool selected)
        {
            const float width = ScreenOverlayLauncherWidth();
            const float height = ImGuiMCP::GetFrameHeight();
            const ImVec2 top = ImGuiMCP::GetCursorScreenPos();
            const bool clicked = ImGuiMCP::InvisibleButton("##screen_overlay_launcher", {width, height});
            const bool hovered = ImGuiMCP::IsItemHovered() || ImGuiMCP::IsItemFocused();
            const ImVec2 end{top.x + width, top.y + height};
            auto* drawList = ImGuiMCP::GetWindowDrawList();
            ImGuiMCP::ImDrawListManager::AddRectFilled(drawList, top, end,
                selected ? Color(112, 80, 32) : hovered ? Color(43, 55, 69) : Color(26, 33, 43), 4.0F, 0);
            ImGuiMCP::ImDrawListManager::AddRect(drawList, top, end,
                selected ? Color(255, 202, 102) : hovered ? Color(150, 189, 220) : Color(78, 94, 111), 4.0F, 0, 1.5F);

            const ImVec2 center{top.x + width * .5F, top.y + height * .5F};
            const float scale = std::min(width, height) / 32.0F;
            const auto tint = selected ? Color(255, 224, 155) : hovered ? Color(210, 229, 245) : Color(190, 208, 224);
            const ImVec2 frameMin{center.x - 10.0F * scale, center.y - 7.0F * scale};
            const ImVec2 frameMax{center.x + 10.0F * scale, center.y + 7.0F * scale};
            ImGuiMCP::ImDrawListManager::AddRect(drawList, frameMin, frameMax, tint, 1.0F * scale, 0, 1.4F * scale);
            ImGuiMCP::ImDrawListManager::AddLine(drawList, frameMin,
                {frameMin.x + 6.0F * scale, frameMin.y}, tint, 2.2F * scale);
            ImGuiMCP::ImDrawListManager::AddLine(drawList, frameMin,
                {frameMin.x, frameMin.y + 5.0F * scale}, tint, 2.2F * scale);
            ImGuiMCP::ImDrawListManager::AddLine(drawList, frameMax,
                {frameMax.x - 6.0F * scale, frameMax.y}, tint, 2.2F * scale);
            ImGuiMCP::ImDrawListManager::AddLine(drawList, frameMax,
                {frameMax.x, frameMax.y - 5.0F * scale}, tint, 2.2F * scale);
            return clicked;
        }

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
                const auto& color = g_editor.GetScene().face.tint;
                Tooltip(Tr("フェイスライト : %s / 強さ %.2f / RGB %d, %d, %d\nクリックでON/OFF。ライト1/2/3や編集対象は変更しません。\n色を調整できる影なしの補助光です。隣の設定から色とカメラ基準／顔の向き基準を選べます。\n停止中は設定だけ変更します。撮影ライトを開始すると反映します。\n頭や配置方向を取得できない場合は待機します。"),
                    g_editor.GetScene().face.enabled ? "ON" : "OFF", g_editor.GetScene().face.intensity,
                    static_cast<int>(color.red * 255 + .5F), static_cast<int>(color.green * 255 + .5F),
                    static_cast<int>(color.blue * 255 + .5F));
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
            if (ImGuiMCP::IsItemHovered()) { Tooltip(Tr("フェイスライトの配置基準・色・強さ・光の範囲・上下を調整します。開くだけでは点灯・設定を変えません。")); }
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
                auto faceTint = g_editor.GetScene().face.tint;
                const auto selectedPreset = FaceTintPresetIndex(faceTint);
                constexpr std::array<const char*, 4> presetNames{"白", "暖色", "青", "ろうそく"};
                constexpr std::array<const char*, 4> presetIds{"##face_white", "##face_warm", "##face_blue", "##face_candle"};
                ImGuiMCP::TextUnformatted(Tr("色"));
                ImGuiMCP::SameLine();
                for (std::size_t index = 0; index < presetNames.size(); ++index) {
                    if (index != 0) { ImGuiMCP::SameLine(); }
                    if (FaceTintPresetSwatch(presetIds[index], Tr(presetNames[index]), FaceTintPreset(index),
                            selectedPreset && *selectedPreset == index)) {
                        g_editor.FinishNumericEdit();
                        g_editor.SetFaceTint(FaceTintPreset(index));
                        g_editor.FinishNumericEdit();
                    }
                }
                ImGuiMCP::SameLine();
                faceTint = g_editor.GetScene().face.tint;
                float color[]{faceTint.red, faceTint.green, faceTint.blue};
                if (ImGuiMCP::ColorEdit3("##face_custom", color, ImGuiMCP::ImGuiColorEditFlags_NoInputs |
                        ImGuiMCP::ImGuiColorEditFlags_NoOptions | ImGuiMCP::ImGuiColorEditFlags_NoAlpha)) {
                    g_editor.SetFaceTint({color[0], color[1], color[2]});
                }
                anyActive = anyActive || ImGuiMCP::IsAnyItemActive();
                const bool customHovered = ImGuiMCP::IsItemHovered();
                const auto currentTint = g_editor.GetScene().face.tint;
                if (!FaceTintPresetIndex(currentTint)) {
                    DrawTintSelectionMark(ImGuiMCP::GetItemRectMin(), ImGuiMCP::GetFrameHeight(), currentTint);
                }
                if (customHovered) {
                    Tooltip(Tr("自由色。色見本を押してフェイスライトの色を選びます。強さ・範囲・位置・ON/OFFは変えません。\n色は履歴・栞・フェイスを含む登録セットに含まれます。"));
                }
                ImGuiMCP::SameLine();
                ImGuiMCP::TextUnformatted(Tr("自由色"));
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

        void DrawDirectionSelector(float a_canvasSize, bool& anyActive, const RuntimeLight::View& runtime)
        {
            const float canvasSize = std::clamp(a_canvasSize, 310.0F, 430.0F);
            const auto origin = ImGuiMCP::GetCursorScreenPos();
            DrawDirectionDiagram(g_editor.GetScene(), origin, canvasSize, true);
            DrawFaceControls(origin, canvasSize, anyActive);
            const auto footer = CalculateDiagramFooter(canvasSize, ImGuiMCP::GetFrameHeight());
            DrawScreenPlacementLauncher(runtime,
                { origin.x + footer.cameraX, origin.y + footer.cameraY });
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

        float RemoveScreenPlacementButtonWidth()
        {
            return ButtonWidth(Tr("フリーライト")) + ImGuiMCP::GetStyle()->ItemInnerSpacing.x +
                ImGuiMCP::GetFontSize();
        }

        void DrawRemoveScreenPlacementButton(bool remembered)
        {
            const float font = ImGuiMCP::GetFontSize();
            const auto* style = ImGuiMCP::GetStyle();
            ImGuiMCP::BeginDisabled(!remembered);
            const bool clicked = SelectableButton("##remove_screen_placement_light", false,
                RemoveScreenPlacementButtonWidth(), true);
            const auto start = ImGuiMCP::GetItemRectMin();
            const auto end = ImGuiMCP::GetItemRectMax();
            auto* draw = ImGuiMCP::GetWindowDrawList();
            const auto tint = ImGuiMCP::GetColorU32(ImGuiMCP::ImGuiCol_Text);
            const float y = (start.y + end.y - font) * .5F;
            ImGuiMCP::ImDrawListManager::AddText(draw, ImGuiMCP::GetFont(), font,
                {start.x + style->FramePadding.x, y}, tint, Tr("フリーライト"));
            const float x = end.x - style->FramePadding.x - font;
            const float stroke = std::max(1.0F, font * .065F);
            const auto line = [&](float x1, float y1, float x2, float y2) {
                ImGuiMCP::ImDrawListManager::AddLine(draw,
                    {x + x1 * font, y + y1 * font}, {x + x2 * font, y + y2 * font}, tint, stroke);
            };
            line(.16F, .25F, .84F, .25F);
            line(.38F, .13F, .62F, .13F);
            line(.38F, .13F, .38F, .25F);
            line(.62F, .13F, .62F, .25F);
            line(.24F, .34F, .30F, .87F);
            line(.30F, .87F, .70F, .87F);
            line(.70F, .87F, .76F, .34F);
            line(.42F, .39F, .44F, .74F);
            line(.58F, .39F, .56F, .74F);
            if (ImGuiMCP::IsItemHovered()) {
                Tooltip(Tr("フリーライトを削除します。位置と設定も消去されます。登録済みプリセットは削除しません。"));
            }
            if (clicked) {
                RuntimeLight::RemoveScreenPlacement();
                g_editScreenPlacement = false;
            }
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
                    g_editScreenPlacement = false;
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

        const LightSettings& SelectedLight(const RuntimeLight::View& runtime)
        {
            return g_editScreenPlacement && runtime.screenPlacement.settingsRemembered ?
                runtime.screenPlacement.light : g_editor.CurrentLight();
        }

        void ApplySelectedLightRecipe(const RuntimeLight::View& runtime, int type,
            std::optional<LightTint> tint = std::nullopt)
        {
            if (g_editScreenPlacement && runtime.screenPlacement.settingsRemembered) {
                auto light = runtime.screenPlacement.light;
                light.type = type;
                light.customTint = tint;
                RuntimeLight::SetScreenPlacementSettings(light);
            } else {
                g_editor.ApplyLightRecipe(type, tint);
            }
        }

        void DrawLightSelection(float a_buttonWidth, const RuntimeLight::View& runtime)
        {
            ImGuiMCP::AlignTextToFramePadding();
            ImGuiMCP::Text(Tr("2. ライト選択"));
            if (ImGuiMCP::IsItemHovered()) {
                Tooltip(Tr("選択中の番号へライトを配置・変更します。\n白・暖色・ろうそく色・青の固定光です。位置・強さ・範囲・影は保持します。\nろうそくの揺らぎはありません。フェイスライトの色は変えません。"));
            }
            for (const int type : kLightDisplayOrder) {
                const auto& light = SelectedLight(runtime);
                if (SelectableButton(Tr(kLightTypes[type].name), light.placed && light.type == type && !light.customTint, a_buttonWidth)) {
                    ApplySelectedLightRecipe(runtime, type);
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
                        const auto& current = SelectedLight(runtime);
                        const bool selected = current.placed && current.type == entry.type && current.customTint == entry.tint;
                        if (selected) { ImGuiMCP::PushStyleColor(ImGuiMCP::ImGuiCol_Button, Color(112, 80, 32)); }
                        // Fixed width clips long names; full name remains in tooltip.
                        if (ImGuiMCP::Button(entry.name.c_str(), {a_buttonWidth, 0})) {
                            ApplySelectedLightRecipe(runtime, entry.type, entry.tint);
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

        void RestoreScreenPlacementMenu()
        {
            if (g_screenPlacementWindow) { g_screenPlacementWindow->IsOpen = false; }
            if (g_returnToMainAfterScreenPlacement && g_mainWindow) { g_mainWindow->IsOpen = true; }
            if (g_returnToToolWindowAfterScreenPlacement && g_toolWindow) { g_toolWindow->IsOpen = true; }
            g_returnToMainAfterScreenPlacement = false;
            g_returnToToolWindowAfterScreenPlacement = false;
            g_skipScreenPlacementClick = false;
        }

        bool BeginScreenPlacementUi(const LightSettings& light)
        {
            if (!RuntimeLight::BeginScreenPlacement(light)) { return false; }
            g_editScreenPlacement = true;
            g_positionWindowOpen = false;
            if (const auto* viewport = ImGuiMCP::GetMainViewport();
                viewport && viewport->Size.x >= 2.0F && viewport->Size.y >= 2.0F) {
                const auto screen = RuntimeLight::GetView().screenPlacement;
                const float x = std::clamp(screen.normalizedX, 0.0F, 1.0F) *
                    (viewport->Size.x - 1.0F);
                const float y = std::clamp(screen.normalizedY, 0.0F, 1.0F) *
                    (viewport->Size.y - 1.0F);
                ImGuiMCP::TeleportMousePos({viewport->Pos.x + x, viewport->Pos.y + y});
                RuntimeLight::UpdateScreenPlacement(x, y, viewport->Size.x, viewport->Size.y);
            }
            g_skipScreenPlacementClick = true;
            if (g_screenPlacementWindow) {
                g_returnToMainAfterScreenPlacement = g_mainWindow && g_mainWindow->IsOpen.load();
                g_returnToToolWindowAfterScreenPlacement = g_toolWindow && g_toolWindow->IsOpen.load();
                g_screenPlacementWindow->IsOpen = true;
                if (g_mainWindow) { g_mainWindow->IsOpen = false; }
                if (g_toolWindow) { g_toolWindow->IsOpen = false; }
            }
            return true;
        }

        void DrawScreenPlacementLauncher(const RuntimeLight::View& runtime, const ImVec2& center)
        {
            const auto activeManual = ActiveManualLightCount(runtime.session);
            const bool remembered = runtime.screenPlacement.settingsRemembered &&
                runtime.screenPlacement.positionRemembered;
            const bool canCreate = runtime.session.ready && !runtime.session.blocked && runtime.session.running &&
                activeManual < kManualLightCount;
            const bool canEnable = remembered && runtime.session.ready && !runtime.session.blocked &&
                (!runtime.session.running || runtime.screenPlacement.enabled || activeManual < kManualLightCount);
            const float size = ImGuiMCP::GetFrameHeight();
            constexpr float gap = 3.0F;
            const float totalWidth = size * 2.0F + gap;
            const ImVec2 top{center.x - totalWidth * .5F, center.y - size * .5F};
            ImGuiMCP::SetCursorScreenPos(top);
            ImGuiMCP::BeginDisabled(!remembered && !canCreate);
            const bool clicked = ImGuiMCP::InvisibleButton("##screen_placement_light", {size, size});
            const bool hovered = ImGuiMCP::IsItemHovered() || ImGuiMCP::IsItemFocused();
            ImGuiMCP::EndDisabled();

            auto* draw = ImGuiMCP::GetWindowDrawList();
            const ImVec2 end{top.x + size, top.y + size};
            const bool selected = remembered && g_editScreenPlacement;
            ImGuiMCP::ImDrawListManager::AddRectFilled(draw, top, end,
                (!remembered && !canCreate) ? Color(18, 23, 30) : selected ? Color(112, 80, 32) :
                hovered ? Color(43, 55, 69) : Color(26, 33, 43), 4.0F, 0);
            ImGuiMCP::ImDrawListManager::AddRect(draw, top, end,
                selected ? Color(255, 202, 102) : hovered && (remembered || canCreate) ?
                Color(150, 189, 220) : Color(78, 94, 111), 4.0F, 0, 1.5F);
            const ImVec2 cameraCenter{top.x + size * .5F, center.y};
            DrawCameraIcon(draw, cameraCenter, std::max(.85F, size / 36.0F));
            const auto rayTint = runtime.screenPlacement.enabled ? Color(255, 202, 102) :
                (remembered || canCreate) ? Color(160, 215, 240) : Color(78, 94, 111);
            const float radius = size * .38F;
            for (int index = 0; index < 3; ++index) {
                const float angle = (-55.0F + index * 55.0F) * std::numbers::pi_v<float> / 180.0F;
                const ImVec2 a{cameraCenter.x + std::cos(angle) * radius,
                    cameraCenter.y + std::sin(angle) * radius};
                const ImVec2 b{cameraCenter.x + std::cos(angle) * (radius + 3.0F),
                    cameraCenter.y + std::sin(angle) * (radius + 3.0F)};
                ImGuiMCP::ImDrawListManager::AddLine(draw, a, b, rayTint, 1.5F);
            }

            if (clicked) {
                if (remembered) {
                    BeginScreenPlacementUi(runtime.screenPlacement.light);
                } else if (canCreate) {
                    BeginScreenPlacementUi(g_editor.CurrentLight());
                }
            }
            if (ImGuiMCP::IsItemHovered()) {
                if (remembered) {
                    Tooltip(Tr("フリーライトの前回位置から画面配置を開きます。確定後は通常メニューの編集対象になります。"));
                } else if (!runtime.session.running) {
                    Tooltip(Tr("先に撮影ライトを開始してください。"));
                } else if (activeManual >= kManualLightCount) {
                    Tooltip(Tr("Light 1～3がすべて点灯中です。どれか1灯をOFFまたは削除すると使用できます。"));
                } else {
                    Tooltip(Tr("選択中ライトを元にする画面配置ライトです。\nクリックすると前回位置で配置を開きます。小窓で強さ・範囲・距離・影・色を調整し、小窓外ではホイールで距離を0.03m、Shift＋ホイールで0.20mずつ調整します。左クリックで確定、右クリックで消灯します。\n撮影ライトの停止・ロード後もゲーム中の調整値を保持し、プリセット登録で次回起動にも復元できます。Light 1～3・履歴は変更しません。"));
                }
            }

            const ImVec2 powerTop{top.x + size + gap, top.y};
            ImGuiMCP::SetCursorScreenPos(powerTop);
            ImGuiMCP::BeginDisabled(!canEnable);
            const bool powerClicked = ImGuiMCP::InvisibleButton("##screen_placement_power", {size, size});
            const bool powerHovered = ImGuiMCP::IsItemHovered() || ImGuiMCP::IsItemFocused();
            ImGuiMCP::EndDisabled();
            const ImVec2 powerEnd{powerTop.x + size, powerTop.y + size};
            ImGuiMCP::ImDrawListManager::AddRectFilled(draw, powerTop, powerEnd,
                !canEnable ? Color(18, 23, 30) : runtime.screenPlacement.enabled ? Color(112, 80, 32) :
                powerHovered ? Color(43, 55, 69) : Color(26, 33, 43), 4.0F, 0);
            ImGuiMCP::ImDrawListManager::AddRect(draw, powerTop, powerEnd,
                runtime.screenPlacement.enabled ? Color(255, 202, 102) : powerHovered && canEnable ?
                Color(150, 189, 220) : Color(78, 94, 111), 4.0F, 0, 1.5F);
            const ImVec2 powerCenter{powerTop.x + size * .5F, powerTop.y + size * .5F};
            const float powerRadius = size * .20F;
            const auto powerTint = runtime.screenPlacement.enabled ? Color(255, 208, 113) :
                canEnable ? Color(137, 151, 167) : Color(66, 75, 86);
            ImGuiMCP::ImDrawListManager::AddCircle(draw, powerCenter, powerRadius, powerTint, 24, 1.8F);
            ImGuiMCP::ImDrawListManager::AddLine(draw,
                {powerCenter.x, powerCenter.y - powerRadius * 1.35F},
                {powerCenter.x, powerCenter.y - powerRadius * .15F}, powerTint, 2.0F);
            if (powerClicked && canEnable) {
                RuntimeLight::SetScreenPlacementEnabled(!runtime.screenPlacement.enabled);
                g_editScreenPlacement = true;
                g_positionWindowOpen = false;
            }
            if (ImGuiMCP::IsItemHovered()) {
                Tooltip(Tr("フリーライトをON/OFFします。OFFでも位置・強さ・範囲・距離・色・影を保持し、編集できます。"));
            }
        }

        void DrawLightActions(float width, const RuntimeLight::View& runtime)
        {
            const float actionGap = ImGuiMCP::GetStyle()->ItemSpacing.x + ImGuiMCP::GetFontSize() * .8F;
            ActionRowLayout row{width, actionGap};
            row.Place(ButtonWidth(Tr("微調整")));
            if (g_editScreenPlacement && runtime.screenPlacement.settingsRemembered) {
                const auto activeManual = ActiveManualLightCount(runtime.session);
                const bool canPlace = runtime.session.ready && !runtime.session.blocked &&
                    runtime.session.running && activeManual < kManualLightCount;
                ImGuiMCP::BeginDisabled(!canPlace);
                if (SelectableButton(Tr("微調整"), runtime.screenPlacement.positioning)) {
                    BeginScreenPlacementUi(runtime.screenPlacement.light);
                }
                ImGuiMCP::EndDisabled();
                if (ImGuiMCP::IsItemHovered()) {
                    Tooltip(Tr("フリーライトの前回位置から画面配置を開きます。"));
                }
                if (row.Place(RemoveScreenPlacementButtonWidth())) { ImGuiMCP::SameLine(0, actionGap); }
                DrawRemoveScreenPlacementButton(runtime.screenPlacement.settingsRemembered);
            } else {
                DrawPositionWindowButton();
                if (row.Place(RemoveLightButtonWidth())) { ImGuiMCP::SameLine(0, actionGap); }
                DrawRemoveLightButton();
            }
        }

        void DrawPlacementAndLightSelection(bool& anyActive, const RuntimeLight::View& runtime)
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
            DrawDirectionSelector(directionWidth, anyActive, runtime);
            // Actions share vertical space with favorite 5 instead of following it.
            if (sideBySide) { DrawLightActions(directionWidth, runtime); }
            ImGuiMCP::EndGroup();

            if (sideBySide) {
                ImGuiMCP::SameLine(0.0F, columnGap);
                ImGuiMCP::BeginGroup();
                DrawLightSelection(selectionWidth, runtime);
                ImGuiMCP::EndGroup();
            } else {
                ImGuiMCP::Spacing();
                DrawLightSelection(selectionWidth, runtime);
            }
            ImGuiMCP::EndGroup();
            if (!sideBySide) { DrawLightActions(availableWidth, runtime); }
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

        void DrawPresetScreenPlacementLabel(ImDrawList* drawList, const ImVec2& origin,
            float width, float height, const ScreenPlacementSettings& screen)
        {
            const auto rgb = DisplayTint(screen.light);
            const auto accent = screen.enabled ? Color(rgb[0], rgb[1], rgb[2]) : Color(107, 120, 137);
            ImGuiMCP::ImDrawListManager::AddRectFilled(drawList, origin,
                {origin.x + width, origin.y + height}, Color(28, 39, 53), 4.0F, 0);
            ImGuiMCP::ImDrawListManager::AddRectFilled(drawList,
                {origin.x, origin.y + 5.0F}, {origin.x + 3.0F, origin.y + height - 5.0F}, accent, 1.0F, 0);
            const float iconScale = std::max(.65F, ImGuiMCP::GetFontSize() / 28.0F);
            const ImVec2 iconCenter{origin.x + 15.0F * iconScale, origin.y + height * .5F};
            DrawCameraIcon(drawList, iconCenter, iconScale);
            if (!screen.enabled) {
                ImGuiMCP::ImDrawListManager::AddLine(drawList,
                    {iconCenter.x - 7.0F * iconScale, iconCenter.y + 7.0F * iconScale},
                    {iconCenter.x + 7.0F * iconScale, iconCenter.y - 7.0F * iconScale}, accent,
                    1.4F * iconScale);
            }
            const auto label = ShortLightName(screen.light);
            const auto size = ImGuiMCP::CalcTextSize(label);
            const float textX = origin.x + 31.0F * iconScale;
            const float scale = std::min(1.0F, std::max(1.0F, origin.x + width - textX - 8.0F) /
                std::max(1.0F, size.x));
            ImGuiMCP::ImDrawListManager::AddText(drawList, ImGuiMCP::GetFont(),
                ImGuiMCP::GetFontSize() * scale,
                {textX, origin.y + (height - size.y * scale) * .5F},
                screen.enabled ? Color(225, 234, 243) : Color(127, 141, 157), label);
        }

        bool DrawPresetCard(const ScenePreset& a_preset, float a_width, float a_labelTextWidth,
            bool includeFace = false,
            std::optional<ScreenPlacementSettings> screenPlacement = std::nullopt)
        {
            const char* displayName = std::string_view(a_preset.id) == "user" ? a_preset.name : Tr(a_preset.name);
            const float font = ImGuiMCP::GetFontSize();
            const bool showScreenPlacementDetail = screenPlacement && screenPlacement->enabled;
            const int detailRows = VisiblePresetDetailRowCount(a_preset.scene,
                showScreenPlacementDetail);
            const auto layout = CalculatePresetCardLayout(a_width, font, a_labelTextWidth, detailRows);
            const float height = layout.height;
            const auto origin = ImGuiMCP::GetCursorScreenPos();
            const bool clicked = ImGuiMCP::Button("##card", { a_width, height });
            const bool hovered = ImGuiMCP::IsItemHovered();
            const bool focused = ImGuiMCP::IsItemFocused();
            if (ImGuiMCP::IsItemVisible()) {
                auto* drawList = ImGuiMCP::GetWindowDrawList();
                const ImVec2 end{ origin.x + a_width, origin.y + height };
                const bool matches = a_preset.scene.lights == g_editor.GetScene().lights &&
                    (!includeFace || a_preset.scene.face == g_editor.GetScene().face) &&
                    screenPlacement == RuntimeLight::GetScreenPlacementPreset();
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
                DrawDirectionDiagram(a_preset.scene,
                    {origin.x + layout.diagramX, origin.y + layout.diagramY}, layout.diagramSize, false,
                    screenPlacement ? std::optional(screenPlacement->enabled) : std::nullopt);
                int row = 0;
                for (int slot = 0; slot < static_cast<int>(a_preset.scene.size()); ++slot) {
                    const auto& light = a_preset.scene[slot];
                    if (!light.placed || !light.enabled) {
                        continue;
                    }
                    DrawPresetLightLabel(drawList, { origin.x + layout.labelsX, origin.y + layout.labelsY + row * (layout.rowHeight + layout.rowGap) },
                        layout.labelsWidth, layout.rowHeight, slot, light);
                    ++row;
                }
                if (showScreenPlacementDetail) {
                    DrawPresetScreenPlacementLabel(drawList,
                        {origin.x + layout.labelsX,
                            origin.y + layout.labelsY + row * (layout.rowHeight + layout.rowGap)},
                        layout.labelsWidth, layout.rowHeight, *screenPlacement);
                }
            }
            if (hovered) {
                ImGuiMCP::BeginTooltip();
                ImGuiMCP::PushTextWrapPos(ImGuiMCP::GetFontSize() * 34.0F);
                ImGuiMCP::TextUnformatted(displayName);
                ImGuiMCP::TextUnformatted(Tr(a_preset.description));
                ImGuiMCP::TextUnformatted(g_managePresets ? Tr("管理中はカードを押しても照明を変えません。") :
                    includeFace ? Tr("クリックで3灯とフェイスライトを適用します。") : Tr("クリックで3灯を適用します。フェイス設定は保持します。"));
                if (screenPlacement) {
                    ImGuiMCP::Text("%s : %s / X %.3f / Y %.3f / %.2f m", Tr("画面配置ライト"),
                        screenPlacement->enabled ? "ON" : "OFF", screenPlacement->normalizedX,
                        screenPlacement->normalizedY, screenPlacement->light.distance);
                }
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
                        if (DrawPresetCard(card, cardWidth, labelTextWidth, entry.includeFace,
                                entry.screenPlacement)) { selectedUser = index; }
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
                if (selected && g_presetPicker.Choose(*selected, g_editor)) {
                    RuntimeLight::ApplyScreenPlacementPreset(std::nullopt);
                    g_editScreenPlacement = false;
                }
                if (selectedUser && PresetLibrary::Apply(g_library->Scenes()[*selectedUser], g_editor)) {
                    RuntimeLight::ApplyScreenPlacementPreset(
                        g_library->Scenes()[*selectedUser].screenPlacement);
                    g_editScreenPlacement = g_library->Scenes()[*selectedUser].screenPlacement.has_value();
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
                if (g_editScreenPlacement && RuntimeLight::GetView().screenPlacement.settingsRemembered) {
                    ImGuiMCP::TextWrapped(Tr("適用先：フリーライト / 登録 %zu 件 / お気に入り %zu / %zu"),
                        g_library->Lights().size(), g_library->Favorites().size(), PresetLibrary::kFavoriteLimit);
                } else {
                    ImGuiMCP::TextWrapped(Tr("適用先 : Light %d / 登録 %zu 件 / お気に入り %zu / %zu"), g_editor.SelectedSlot() + 1,
                        g_library->Lights().size(), g_library->Favorites().size(), PresetLibrary::kFavoriteLimit);
                }
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
                            ApplySelectedLightRecipe(RuntimeLight::GetView(), entry.type, entry.tint);
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
            g_registrationScreenPlacement = scene ? RuntimeLight::GetScreenPlacementPreset() : std::nullopt;
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
                    ImGuiMCP::TextWrapped(g_registrationScreenPlacement ?
                        Tr("通常3灯に加え、画面配置ライトのON/OFF・画面位置・距離・色・強さ・範囲・影を保存します。カードのカメラ印が目印です。") :
                        Tr("通常3灯の位置・微調整・種類・色・強さ・範囲・影・ON/OFFを保存します。正面の基準と照明全体の開始状態は保存しません。"));
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
                    const bool saved = g_registerScene ? g_library->SaveScene(name, g_registrationSnapshot,
                        g_saveFace, g_registrationScreenPlacement) :
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

        void DrawScreenPlacementMainSettings(bool& anyActive, const RuntimeLight::View& runtime)
        {
            auto light = runtime.screenPlacement.light;
            bool changed{};
            const char* heading = Tr("3. 基本設定 / 編集中：フリーライト（仮）");
            const float width = ImGuiMCP::GetContentRegionAvail().x;
            const float gap = ImGuiMCP::GetStyle()->ItemSpacing.x + ImGuiMCP::GetFontSize() * .5F;
            if (FitsInline(width, ImGuiMCP::CalcTextSize(heading).x, ButtonWidth(Tr("初期値")), gap)) {
                ImGuiMCP::TextUnformatted(heading);
                ImGuiMCP::SameLine(0, gap);
            } else {
                ImGuiMCP::TextWrapped("%s", heading);
            }
            if (SelectableButton(Tr("初期値"))) {
                light.intensity = kLightIntensityDefault;
                light.range = kLightRangeDefault;
                light.distance = kScreenPlacementDepthDefault;
                light.shadowBias = kShadowBiasDefault;
                changed = true;
            }
            if (ImGuiMCP::IsItemHovered()) {
                Tooltip(Tr("フリーライトの強さ・範囲・距離・影の補正を初期値へ戻します。位置・種類・色・影の方式・ON/OFFは保持します。"));
            }
            if (!runtime.screenPlacement.enabled) {
                ImGuiMCP::TextWrapped(Tr("OFF：設定と画面位置は保持されています。消灯中も編集できます。"));
            } else if (runtime.session.running && runtime.screenPlacement.waiting) {
                ImGuiMCP::TextWrapped(Tr("待機：点灯中の通常ライトを2灯以下にしてください。"));
            }

            const float labelWidth = std::max({ImGuiMCP::CalcTextSize(Tr("強さ")).x,
                ImGuiMCP::CalcTextSize(Tr("光の範囲")).x, ImGuiMCP::CalcTextSize(Tr("対象からの距離")).x});
            ImGuiMCP::PushItemWidth(std::max(1.0F, ImGuiMCP::GetContentRegionAvail().x - labelWidth -
                ImGuiMCP::GetStyle()->ItemSpacing.x));
            changed = ImGuiMCP::SliderFloat("##free_light_intensity", &light.intensity,
                0.0F, 3.0F, "%.2f") || changed;
            ImGuiMCP::SameLine();
            ImGuiMCP::TextUnformatted(Tr("強さ"));
            anyActive = ImGuiMCP::IsItemActive() || anyActive;
            changed = ImGuiMCP::SliderFloat("##free_light_range", &light.range,
                kScreenPlacementRangeMin, kScreenPlacementRangeMax, "%.2f m") || changed;
            ImGuiMCP::SameLine();
            ImGuiMCP::TextUnformatted(Tr("光の範囲"));
            anyActive = ImGuiMCP::IsItemActive() || anyActive;
            changed = ImGuiMCP::SliderFloat("##free_light_distance", &light.distance,
                kScreenPlacementDepthMin, kScreenPlacementDepthMax, "%.2f m") || changed;
            ImGuiMCP::SameLine();
            ImGuiMCP::TextUnformatted(Tr("対象からの距離"));
            anyActive = ImGuiMCP::IsItemActive() || anyActive;
            if (ImGuiMCP::IsItemHovered()) {
                Tooltip(Tr("対象からカメラ側へ置く距離です。画面上の横・縦位置は変えません。"));
            }
            ImGuiMCP::PopItemWidth();

            const std::array<const char*, 3> shadowLabels{Tr("影なし"), Tr("全方向の影"), Tr("スポットの影")};
            for (int mode = 0; mode < 3; ++mode) {
                if (mode > 0) { ImGuiMCP::SameLine(); }
                const bool selected = mode == 0 ? !light.castsShadow :
                    light.castsShadow && light.shadowProjection ==
                        (mode == 1 ? ShadowProjection::Omni : ShadowProjection::Spot);
                ImGuiMCP::PushID(mode);
                if (SelectableButton(shadowLabels[mode], selected)) {
                    light.castsShadow = mode != 0;
                    if (mode != 0) {
                        light.shadowProjection = mode == 1 ? ShadowProjection::Omni : ShadowProjection::Spot;
                    }
                    changed = true;
                }
                ImGuiMCP::PopID();
            }
            if (light.castsShadow && light.shadowProjection == ShadowProjection::Spot) {
                ImGuiMCP::TextUnformatted(Tr("影の補正"));
                ImGuiMCP::SameLine();
                for (std::size_t index = 0; index < kShadowBiasLevels.size(); ++index) {
                    if (index > 0) { ImGuiMCP::SameLine(); }
                    const float value = kShadowBiasLevels[index];
                    const auto label = value == 1.0F ? std::string(Tr("1 (標準)")) : Fmt("{:g}", value);
                    ImGuiMCP::PushID(static_cast<int>(index));
                    if (SelectableButton(label.c_str(), light.shadowBias == value)) {
                        light.shadowBias = value;
                        changed = true;
                    }
                    ImGuiMCP::PopID();
                }
            }

            const auto tint = RuntimeTint(light).value_or(LightTint{1, 1, 1});
            const float swatchSize = ImGuiMCP::GetFrameHeight();
            if (ImGuiMCP::ColorButton("##free_light_color",
                    {tint.red, tint.green, tint.blue, 1.0F},
                    ImGuiMCP::ImGuiColorEditFlags_NoTooltip, {swatchSize, swatchSize})) {
                ImGuiMCP::OpenPopup("free_light_color_popup");
            }
            ImGuiMCP::SameLine();
            ImGuiMCP::AlignTextToFramePadding();
            ImGuiMCP::TextUnformatted(Tr("ライトの色"));
            ImGuiMCP::PushStyleColor(ImGuiMCP::ImGuiCol_PopupBg, Color(12, 18, 27));
            if (ImGuiMCP::BeginPopup("free_light_color_popup")) {
                constexpr std::array<LightTint, 8> palette{{
                    {1,1,1}, {1,0.8F,0.53F}, {1,0.6F,0.29F}, {0.3F,0.55F,1},
                    {1,0.25F,0.2F}, {0.35F,1,0.45F}, {0.3F,1,1}, {0.85F,0.4F,1}
                }};
                const float paletteSize = ImGuiMCP::GetFontSize() * 1.25F;
                for (int index = 0; index < static_cast<int>(palette.size()); ++index) {
                    ImGuiMCP::PushID(index);
                    const auto& color = palette[index];
                    if (ImGuiMCP::ColorButton("##free_swatch",
                            {color.red, color.green, color.blue, 1.0F},
                            ImGuiMCP::ImGuiColorEditFlags_NoTooltip, {paletteSize, paletteSize})) {
                        light.customTint = color;
                        changed = true;
                    }
                    ImGuiMCP::PopID();
                    if (index != 3 && index != 7) { ImGuiMCP::SameLine(); }
                }
                const auto current = RuntimeTint(light).value_or(LightTint{1, 1, 1});
                float rgb[]{current.red, current.green, current.blue};
                ImGuiMCP::SetNextItemWidth(std::min(ImGuiMCP::GetFontSize() * 13.0F,
                    ImGuiMCP::GetMainViewport()->WorkSize.x * .8F));
                if (ImGuiMCP::ColorPicker3("##free_light_picker", rgb,
                        ImGuiMCP::ImGuiColorEditFlags_DisplayRGB |
                        ImGuiMCP::ImGuiColorEditFlags_Uint8 |
                        ImGuiMCP::ImGuiColorEditFlags_PickerHueBar |
                        ImGuiMCP::ImGuiColorEditFlags_NoSidePreview |
                        ImGuiMCP::ImGuiColorEditFlags_NoAlpha)) {
                    light.customTint = LightTint{rgb[0], rgb[1], rgb[2]};
                    changed = true;
                }
                anyActive = ImGuiMCP::IsAnyItemActive() || anyActive;
                if (SelectableButton(Tr("種類の元の色に戻す"))) {
                    light.customTint.reset();
                    changed = true;
                }
                ImGuiMCP::EndPopup();
            }
            ImGuiMCP::PopStyleColor();
            if (changed) { RuntimeLight::SetScreenPlacementSettings(light); }
        }

        void DrawPrototypeControls(bool& anySliderActive, const RuntimeLight::View& runtime)
        {
            DrawPlacementAndLightSelection(anySliderActive, runtime);

            ImGuiMCP::Separator();
            if (g_editScreenPlacement && runtime.screenPlacement.settingsRemembered) {
                DrawScreenPlacementMainSettings(anySliderActive, runtime);
            } else {
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
            }

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
            ImGuiMCP::TextDisabled(Tr("登録の保存先 (?) / 0.4.0"));
            if (ImGuiMCP::IsItemHovered() && g_library) {
                const auto utf8 = g_library->Directory().u8string();
                Tooltip(Tr("%s\nstorage-v2.slaがある場合は、そのファイルを提供する保存用MODへ新規ファイルも保存します。\nマーカーがない場合はMO2の通常出力先を使います。\n登録は再起動後も残り、ゲームのセーブとは独立です。\n登録から外したファイルは、この中のArchivedへ退避します。\n履歴・栞の登録は起動中のみの一時退避です。"), reinterpret_cast<const char*>(utf8.c_str()));
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

        void DrawPersistentFaceWindow(const RuntimeLight::View& runtime)
        {
            if (!g_persistentFaceWindowOpen) { return; }
            const auto* viewport = ImGuiMCP::GetMainViewport();
            const ImVec2 maximum{viewport->WorkSize.x * .9F, viewport->WorkSize.y * .9F};
            ImGuiMCP::SetNextWindowSize({std::min(430.0F, maximum.x), 0}, ImGuiMCP::ImGuiCond_FirstUseEver);
            ImGuiMCP::SetNextWindowSizeConstraints({std::min(300.0F, maximum.x), 0}, maximum);
            if (g_focusPersistentFaceWindow) {
                ImGuiMCP::SetNextWindowFocus();
                g_focusPersistentFaceWindow = false;
            }
            bool open = true;
            ImGuiMCP::PushStyleColor(ImGuiMCP::ImGuiCol_WindowBg, Color(12, 18, 27));
            const bool visible = ImGuiMCP::Begin(Tr("常用フェイスライト###sla_persistent_face"), &open,
                ImGuiMCP::ImGuiWindowFlags_AlwaysAutoResize | ImGuiMCP::ImGuiWindowFlags_NoCollapse);
            if (visible) {
                auto settings = runtime.persistentFace;
                bool changed{};
                changed = ImGuiMCP::Checkbox(Tr("常用フェイスライト機能を使用"), &settings.featureEnabled) || changed;
                if (!settings.featureEnabled) {
                    ImGuiMCP::TextWrapped(Tr("機能OFF：ホットキーは反応しません。"));
                } else if (runtime.persistentFaceUsesPhotoLight) {
                    ImGuiMCP::TextWrapped(Tr("撮影用フェイスライトを一時的に使用中です。撮影を止めると常用設定へ戻ります。"));
                } else if (!settings.enabled) {
                    ImGuiMCP::TextWrapped(Tr("待機：プレイヤーライトはOFFです。"));
                } else if (settings.intensity == 0.0F) {
                    ImGuiMCP::TextWrapped(Tr("待機：強さが0です。"));
                } else if (runtime.persistentFaceActive) {
                    ImGuiMCP::TextWrapped(Tr("ON：プレイヤーへ追従しています。"));
                } else if (runtime.persistentFaceWaiting) {
                    ImGuiMCP::TextWrapped(Tr("待機：ロード中、または頭を取得できません。"));
                }
                ImGuiMCP::Separator();
                changed = ImGuiMCP::Checkbox(Tr("プレイヤー"), &settings.enabled) || changed;
                ImGuiMCP::SetNextItemWidth(std::clamp(ImGuiMCP::GetFontSize() * 9.0F, 170.0F, 310.0F));
                changed = ImGuiMCP::SliderFloat(Tr("強さ##persistent"), &settings.intensity,
                    0.0F, kFaceIntensityMax, "%.2f") || changed;
                ImGuiMCP::SetNextItemWidth(std::clamp(ImGuiMCP::GetFontSize() * 9.0F, 170.0F, 310.0F));
                changed = ImGuiMCP::SliderFloat(Tr("光の範囲##persistent"), &settings.range,
                    kFaceRangeMin, kFaceRangeMax, "%.2f m") || changed;
                ImGuiMCP::SetNextItemWidth(std::clamp(ImGuiMCP::GetFontSize() * 9.0F, 170.0F, 310.0F));
                changed = ImGuiMCP::SliderFloat(Tr("上下 (+上)##persistent"), &settings.heightOffset,
                    kFaceHeightMin, kFaceHeightMax, "%+.2f m") || changed;
                const auto selectedPreset = FaceTintPresetIndex(settings.tint);
                constexpr std::array<const char*, 4> presetNames{"白", "暖色", "青", "ろうそく"};
                constexpr std::array<const char*, 4> presetIds{"##persistent_white", "##persistent_warm", "##persistent_blue", "##persistent_candle"};
                ImGuiMCP::TextUnformatted(Tr("色"));
                ImGuiMCP::SameLine();
                for (std::size_t index = 0; index < presetNames.size(); ++index) {
                    if (index != 0) { ImGuiMCP::SameLine(); }
                    if (FaceTintPresetSwatch(presetIds[index], Tr(presetNames[index]), FaceTintPreset(index),
                            selectedPreset && *selectedPreset == index)) {
                        settings.tint = FaceTintPreset(index);
                        changed = true;
                    }
                }
                ImGuiMCP::SameLine();
                float color[]{settings.tint.red, settings.tint.green, settings.tint.blue};
                if (ImGuiMCP::ColorEdit3("##persistent_custom", color, ImGuiMCP::ImGuiColorEditFlags_NoInputs |
                        ImGuiMCP::ImGuiColorEditFlags_NoOptions | ImGuiMCP::ImGuiColorEditFlags_NoAlpha)) {
                    settings.tint = {color[0], color[1], color[2]};
                    changed = true;
                }
                const bool customHovered = ImGuiMCP::IsItemHovered();
                if (!FaceTintPresetIndex(settings.tint)) {
                    DrawTintSelectionMark(ImGuiMCP::GetItemRectMin(), ImGuiMCP::GetFrameHeight(), settings.tint);
                }
                ImGuiMCP::SameLine();
                ImGuiMCP::TextUnformatted(Tr("自由色"));
                if (customHovered) {
                    Tooltip(Tr("自由色。色見本を押して常用フェイスライトの色を選びます。撮影用フェイスライトの色とは別に保存します。"));
                }
                if (changed) { RuntimeLight::SetPersistentFaceSettings(settings); }

                ImGuiMCP::Separator();
                ImGuiMCP::Text(Tr("キーボード：%s"), Tr(runtime.persistentHotkeyName.c_str()));
                if (runtime.persistentHotkeyCapture) {
                    ImGuiMCP::TextWrapped(Tr("登録するキーを押してください。Escでキャンセルします。"));
                    if (SelectableButton(Tr("キャンセル##persistent_keyboard"))) { RuntimeLight::CancelPersistentHotkeyCapture(); }
                } else {
                    if (SelectableButton(Tr("変更##persistent_keyboard"))) { RuntimeLight::BeginPersistentHotkeyCapture(); }
                    ImGuiMCP::SameLine();
                    ImGuiMCP::BeginDisabled(settings.hotkey == kNoPersistentFaceHotkey);
                    if (SelectableButton(Tr("解除##persistent_keyboard"))) { RuntimeLight::ClearPersistentHotkey(); }
                    ImGuiMCP::EndDisabled();
                }
                ImGuiMCP::Text(Tr("ゲームパッド（0.6秒長押し）：%s"), Tr(runtime.persistentGamepadHotkeyName.c_str()));
                if (runtime.persistentGamepadCapture) {
                    ImGuiMCP::TextWrapped(Tr("登録するゲームパッドボタンを押してください。Escでキャンセルします。"));
                    if (SelectableButton(Tr("キャンセル##persistent_gamepad"))) { RuntimeLight::CancelPersistentHotkeyCapture(); }
                } else {
                    if (SelectableButton(Tr("変更##persistent_gamepad"))) { RuntimeLight::BeginPersistentGamepadCapture(); }
                    ImGuiMCP::SameLine();
                    ImGuiMCP::BeginDisabled(settings.gamepadHotkey == kNoPersistentFaceHotkey);
                    if (SelectableButton(Tr("解除##persistent_gamepad"))) { RuntimeLight::ClearPersistentGamepadHotkey(); }
                    ImGuiMCP::EndDisabled();
                }
                ImGuiMCP::Separator();
                ImGuiMCP::TextWrapped(Tr("色を調整できる影なしのプレイヤー専用ライトです。撮影ライトとは別に使え、ロードや場所移動の完了後に常用側の色で自動復帰します。ゲームのセーブデータには保存しません。"));
                if (!runtime.persistentFaceError.empty()) {
                    ImGuiMCP::TextWrapped("%s", Tr(runtime.persistentFaceError.c_str()));
                }
            }
            ImGuiMCP::End();
            ImGuiMCP::PopStyleColor();
            if (!open) {
                g_persistentFaceWindowOpen = false;
                RuntimeLight::CancelPersistentHotkeyCapture();
            }
        }

        void DrawToolHotkeyWindow(const RuntimeLight::View& runtime)
        {
            if (!g_toolHotkeyWindowOpen) { return; }
            const auto* viewport = ImGuiMCP::GetMainViewport();
            const ImVec2 maximum{viewport->WorkSize.x * .9F, viewport->WorkSize.y * .9F};
            ImGuiMCP::SetNextWindowSize({std::min(390.0F, maximum.x), 0}, ImGuiMCP::ImGuiCond_FirstUseEver);
            ImGuiMCP::SetNextWindowSizeConstraints({std::min(290.0F, maximum.x), 0}, maximum);
            if (g_focusToolHotkeyWindow) {
                ImGuiMCP::SetNextWindowFocus();
                g_focusToolHotkeyWindow = false;
            }
            bool open = true;
            ImGuiMCP::PushStyleColor(ImGuiMCP::ImGuiCol_WindowBg, Color(12, 18, 27));
            const bool visible = ImGuiMCP::Begin(Tr("ツール表示キー###sla_tool_window_hotkey"), &open,
                ImGuiMCP::ImGuiWindowFlags_AlwaysAutoResize | ImGuiMCP::ImGuiWindowFlags_NoCollapse);
            if (visible) {
                ImGuiMCP::Text(Tr("現在：%s"), Tr(runtime.toolWindowHotkeyName.c_str()));
                ImGuiMCP::TextWrapped(Tr("このキーでLighting単独ウィンドウを開閉します。Shift / Ctrl / Altを押しながら登録できます。"));
                if (runtime.toolWindowHotkeyCapture) {
                    ImGuiMCP::TextWrapped(Tr("登録するキーを押してください。修飾キーは押したままにします。Escでキャンセルします。"));
                    if (SelectableButton(Tr("キャンセル##tool_window_hotkey"))) {
                        RuntimeLight::CancelToolWindowHotkeyCapture();
                    }
                } else {
                    if (SelectableButton(Tr("変更##tool_window_hotkey"))) {
                        RuntimeLight::BeginToolWindowHotkeyCapture();
                    }
                    ImGuiMCP::SameLine();
                    ImGuiMCP::BeginDisabled(runtime.toolWindowHotkey.key == kNoToolWindowHotkey);
                    if (SelectableButton(Tr("解除##tool_window_hotkey"))) {
                        RuntimeLight::ClearToolWindowHotkey();
                    }
                    ImGuiMCP::EndDisabled();
                }
                if (!runtime.toolWindowHotkeyError.empty()) {
                    ImGuiMCP::TextWrapped("%s", Tr(runtime.toolWindowHotkeyError.c_str()));
                }
            }
            ImGuiMCP::End();
            ImGuiMCP::PopStyleColor();
            if (!open) {
                g_toolHotkeyWindowOpen = false;
                RuntimeLight::CancelToolWindowHotkeyCapture();
            }
        }

        void DrawPhotoEffectWindow(const RuntimeLight::View& runtime)
        {
            if (!g_photoEffectWindowOpen) { return; }
            const auto* viewport = ImGuiMCP::GetMainViewport();
            const ImVec2 maximum{viewport->WorkSize.x * .9F, viewport->WorkSize.y * .9F};
            ImGuiMCP::SetNextWindowSize({std::min(420.0F, maximum.x), std::min(650.0F, maximum.y)},
                ImGuiMCP::ImGuiCond_FirstUseEver);
            ImGuiMCP::SetNextWindowSizeConstraints(
                {std::min(300.0F, maximum.x), std::min(240.0F, maximum.y)}, maximum);
            if (g_focusPhotoEffectWindow) {
                ImGuiMCP::SetNextWindowFocus();
                g_focusPhotoEffectWindow = false;
            }
            bool open = true;
            ImGuiMCP::PushStyleColor(ImGuiMCP::ImGuiCol_WindowBg, Color(12, 18, 27));
            const bool visible = ImGuiMCP::Begin(Tr("撮影エフェクト###sla_photo_effect"), &open,
                ImGuiMCP::ImGuiWindowFlags_NoCollapse);
            if (visible) {
                auto settings = runtime.photoEffect;
                bool changed = ImGuiMCP::Checkbox(Tr("撮影エフェクトを使用"), &settings.enabled);
                if (!settings.enabled) {
                    ImGuiMCP::TextWrapped(Tr("OFF：撮影ライトには追加されません。"));
                } else if (!runtime.session.running) {
                    ImGuiMCP::TextWrapped(Tr("待機：撮影ライトを開始すると対象の周囲へ表示します。"));
                } else if (runtime.activePhotoEffects > 0) {
                    const char* activeStatus = settings.type == PhotoEffectType::SakuraPetals ?
                        "ON：対象の周囲へ花びらを表示しています。" :
                        settings.type == PhotoEffectType::MagicStars ?
                        "ON：対象の周囲へ星を表示しています。" :
                        settings.type == PhotoEffectType::AutumnLeaves ?
                        "ON：対象の周囲へ落ち葉を表示しています。" :
                        settings.type == PhotoEffectType::WhiteFeathers ?
                        "ON：対象の周囲へ白い羽を表示しています。" :
                        settings.type == PhotoEffectType::PopStars ?
                        "ON：対象の周囲へ星2を表示しています。" :
                        settings.type == PhotoEffectType::BlackFeathers ?
                        "ON：対象の周囲へ黒い羽を表示しています。" :
                        settings.type == PhotoEffectType::SnowCrystals ?
                        "ON：対象の周囲へ雪の結晶を表示しています。" :
                        "ON：対象の周囲へ光の粒を表示しています。";
                    ImGuiMCP::TextWrapped(Tr(activeStatus));
                } else {
                    ImGuiMCP::TextWrapped(Tr("開始待ち：対象と表示用メッシュを確認しています。"));
                }
                ImGuiMCP::Separator();
                const char* effectItems[]{Tr("キラキラ"), Tr("花びら"), Tr("星"), Tr("星2"),
                    Tr("落ち葉"), Tr("白い羽"), Tr("黒い羽"), Tr("雪の結晶")};
                int effectType = PhotoEffectDisplayIndex(settings.type);
                ImGuiMCP::SetNextItemWidth(std::clamp(ImGuiMCP::GetFontSize() * 9.0F, 170.0F, 310.0F));
                if (ImGuiMCP::Combo(Tr("種類"), &effectType, effectItems, 8)) {
                    settings.type = PhotoEffectTypeFromDisplayIndex(effectType);
                    ApplyPhotoEffectMotionPreset(settings, PhotoEffectMotionPreset::Gentle);
                    changed = true;
                }
                const char* motionItems[]{Tr("ゆっくり舞う"), Tr("上から舞い落ちる"), Tr("風に流れる"), Tr("吹き上がる"),
                    Tr("対象の周囲を旋回"), Tr("カスタム")};
                int motionPreset = static_cast<int>(settings.motionPreset);
                ImGuiMCP::SetNextItemWidth(std::clamp(ImGuiMCP::GetFontSize() * 9.0F, 170.0F, 310.0F));
                if (ImGuiMCP::Combo(Tr("動きプリセット"), &motionPreset, motionItems, 6)) {
                    ApplyPhotoEffectMotionPreset(settings,
                        static_cast<PhotoEffectMotionPreset>(motionPreset));
                    changed = true;
                }
                ImGuiMCP::TextWrapped(Tr("プリセットは下記値の組み合わせです。"));
                ImGuiMCP::SetNextItemWidth(std::clamp(ImGuiMCP::GetFontSize() * 9.0F, 170.0F, 310.0F));
                if (ImGuiMCP::SliderFloat(Tr("密度"), &settings.density,
                    kPhotoEffectDensityMin, kPhotoEffectDensityMax, "%.2f")) {
                    changed = true;
                }
                ImGuiMCP::SetNextItemWidth(std::clamp(ImGuiMCP::GetFontSize() * 9.0F, 170.0F, 310.0F));
                changed = ImGuiMCP::SliderFloat(Tr("粒の大きさ"), &settings.size,
                    kParticleSizeMin, kParticleSizeMax, "%.2f x") || changed;
                ImGuiMCP::SetNextItemWidth(std::clamp(ImGuiMCP::GetFontSize() * 9.0F, 170.0F, 310.0F));
                changed = ImGuiMCP::SliderFloat(Tr("広がる範囲"), &settings.spread,
                    kParticleFieldSpreadMin, kParticleFieldSpreadMax, "%.2f x") || changed;
                ImGuiMCP::SetNextItemWidth(std::clamp(ImGuiMCP::GetFontSize() * 9.0F, 170.0F, 310.0F));
                changed = ImGuiMCP::SliderFloat(Tr("不透明度"), &settings.opacity,
                    kPhotoEffectOpacityMin, kPhotoEffectOpacityMax, "%.2f") || changed;
                ImGuiMCP::SetNextItemWidth(std::clamp(ImGuiMCP::GetFontSize() * 9.0F, 170.0F, 310.0F));
                changed = ImGuiMCP::SliderFloat(Tr("明るさ"), &settings.brightness,
                    kPhotoEffectBrightnessMin, kPhotoEffectBrightnessMax, "%.2f x") || changed;
                ImGuiMCP::SetNextItemWidth(std::clamp(ImGuiMCP::GetFontSize() * 9.0F, 170.0F, 310.0F));
                const bool positionChanged = ImGuiMCP::SliderFloat(Tr("上下位置"), &settings.verticalOffset,
                    kParticleFieldVerticalMin, kParticleFieldVerticalMax, "%+.2f m");
                if (positionChanged) {
                    settings.motionPreset = PhotoEffectMotionPreset::Custom;
                    changed = true;
                }
                if (ImGuiMCP::CollapsingHeader(Tr("動きの詳細調整"))) {
                    bool motionChanged{};
                    ImGuiMCP::SetNextItemWidth(std::clamp(ImGuiMCP::GetFontSize() * 9.0F, 170.0F, 310.0F));
                    motionChanged = ImGuiMCP::SliderFloat(Tr("動く速さ"), &settings.motionSpeed,
                        kPhotoEffectMotionSpeedMin, kPhotoEffectMotionSpeedMax, "%.2f x") || motionChanged;
                    ImGuiMCP::SetNextItemWidth(std::clamp(ImGuiMCP::GetFontSize() * 9.0F, 170.0F, 310.0F));
                    motionChanged = ImGuiMCP::SliderFloat(Tr("上下の移動（+上）"), &settings.verticalMovement,
                        kPhotoEffectFlowMin, kPhotoEffectFlowMax, "%+.2f") || motionChanged;
                    ImGuiMCP::SetNextItemWidth(std::clamp(ImGuiMCP::GetFontSize() * 9.0F, 170.0F, 310.0F));
                    motionChanged = ImGuiMCP::SliderFloat(Tr("横への流れ（+右）"), &settings.horizontalFlow,
                        kPhotoEffectHorizontalFlowMin, kPhotoEffectHorizontalFlowMax, "%+.2f") || motionChanged;
                    ImGuiMCP::SetNextItemWidth(std::clamp(ImGuiMCP::GetFontSize() * 9.0F, 170.0F, 310.0F));
                    motionChanged = ImGuiMCP::SliderFloat(Tr("浮遊感"), &settings.floatiness,
                        kPhotoEffectFloatinessMin, kPhotoEffectFloatinessMax, "%.2f") || motionChanged;
                    ImGuiMCP::SetNextItemWidth(std::clamp(ImGuiMCP::GetFontSize() * 9.0F, 170.0F, 310.0F));
                    motionChanged = ImGuiMCP::SliderFloat(Tr("ゆらぎ"), &settings.sway,
                        kPhotoEffectSwayMin, kPhotoEffectSwayMax, "%.2f") || motionChanged;
                    ImGuiMCP::SetNextItemWidth(std::clamp(ImGuiMCP::GetFontSize() * 9.0F, 170.0F, 310.0F));
                    motionChanged = ImGuiMCP::SliderFloat(Tr("ランダム感"), &settings.randomness,
                        kPhotoEffectRandomnessMin, kPhotoEffectRandomnessMax, "%.2f") || motionChanged;
                    ImGuiMCP::SetNextItemWidth(std::clamp(ImGuiMCP::GetFontSize() * 9.0F, 170.0F, 310.0F));
                    if (ImGuiMCP::SliderFloat(Tr("表示時間"), &settings.lifetime,
                        kPhotoEffectLifetimeMin, kPhotoEffectLifetimeMax, "%.2f x")) {
                        settings.lifetime = PhotoEffectLifetimeFromVariant(
                            PhotoEffectLifetimeVariant(settings.lifetime));
                        motionChanged = true;
                    }
                    motionChanged = ImGuiMCP::Checkbox(Tr("対象の周囲を旋回"), &settings.orbit) || motionChanged;
                    if (settings.orbit) {
                        const char* orbitDirections[]{Tr("時計回り"), Tr("反時計回り")};
                        int directionIndex = settings.orbitDirection < 0 ? 1 : 0;
                        ImGuiMCP::SetNextItemWidth(std::clamp(ImGuiMCP::GetFontSize() * 9.0F, 170.0F, 310.0F));
                        if (ImGuiMCP::Combo(Tr("旋回方向"), &directionIndex, orbitDirections, 2)) {
                            settings.orbitDirection = directionIndex == 0 ? 1 : -1;
                            motionChanged = true;
                        }
                    }
                    if (motionChanged) {
                        settings.motionPreset = PhotoEffectMotionPreset::Custom;
                        changed = true;
                    }
                    ImGuiMCP::TextWrapped(Tr("上下・左右の0は停滞、+上下は上昇、-上下は下降です。浮遊感を上げると加速が穏やかになります。左右は撮影開始時または正面更新時のカメラ基準です。旋回・旋回方向・表示時間の変更時だけ粒を入れ替え、それ以外は自動反映します。"));
                }
                if (changed) { RuntimeLight::SetPhotoEffectSettings(settings); }
                ImGuiMCP::Separator();
                const char* effectDescription = settings.type == PhotoEffectType::SakuraPetals ?
                    "淡い桜色の花びらが向きを変えながら対象の周囲へ舞います。動きはプリセットまたは詳細調整で設定できます。撮影ライトの開始中だけ表示し、停止・ロード・場所移動で撤去します。ゲームのセーブデータには保存しません。" :
                    settings.type == PhotoEffectType::MagicStars ?
                    "白を中心に淡い金色と青色を帯びた五芒星が、回転しながら対象の周囲へ舞います。動きはプリセットまたは詳細調整で設定できます。撮影ライトの開始中だけ表示し、停止・ロード・場所移動で撤去します。ゲームのセーブデータには保存しません。" :
                    settings.type == PhotoEffectType::AutumnLeaves ?
                    "リフテンの秋を思わせる黄・橙・赤褐色の落ち葉が、向きと表裏を変えながら対象の周囲へ舞います。動きはプリセットまたは詳細調整で設定できます。撮影ライトの開始中だけ表示し、停止・ロード・場所移動で撤去します。ゲームのセーブデータには保存しません。" :
                    settings.type == PhotoEffectType::WhiteFeathers ?
                    "白い羽が羽軸と羽枝の淡い青灰色の陰影を保ち、表・斜め・側面・裏面を変えながら対象の周囲へ舞います。動きはプリセットまたは詳細調整で設定できます。撮影ライトの開始中だけ表示し、停止・ロード・場所移動で撤去します。ゲームのセーブデータには保存しません。" :
                    settings.type == PhotoEffectType::PopStars ?
                    "中をくり抜いた明るい黄色の輪郭・コーラルピンク・暖かい淡黄色・スカイブルーを組み合わせた、固定サイズの平面的で先端の尖った星2が対象の周囲へ舞います。動きはプリセットまたは詳細調整で設定できます。撮影ライトの開始中だけ表示し、停止・ロード・場所移動で撤去します。ゲームのセーブデータには保存しません。" :
                    settings.type == PhotoEffectType::BlackFeathers ?
                    "白い羽と同じ形・姿勢・動きを共有する、黒を主体にごく控えめなグラファイトの細部を持つ黒い羽が表・斜め・側面・裏面を変えながら対象の周囲へ舞います。動きはプリセットまたは詳細調整で設定できます。撮影ライトの開始中だけ表示し、停止・ロード・場所移動で撤去します。ゲームのセーブデータには保存しません。" :
                    settings.type == PhotoEffectType::SnowCrystals ?
                    "淡いアイスブルーの半透明な大・中・針状・小型の六花が、形と大きさを保ったまま緩やかに回転し、対象の周囲へ舞い降ります。内部は薄く、枝と輪郭だけが少し明るく見えます。動きはプリセットまたは詳細調整で設定できます。撮影ライトの開始中だけ表示し、停止・ロード・場所移動で撤去します。ゲームのセーブデータには保存しません。" :
                    "白～淡い金色の光の粒が粒ごとに異なる周期でまたたきながら、対象の周囲へ現れます。動きはプリセットまたは詳細調整で設定できます。撮影ライトの開始中だけ表示し、停止・ロード・場所移動で撤去します。ゲームのセーブデータには保存しません。";
                ImGuiMCP::TextWrapped(Tr(effectDescription));
                ImGuiMCP::TextWrapped(Tr("粒の大きさ・広がる範囲・不透明度・明るさは別々に調整できます。"));
                ImGuiMCP::TextWrapped(Tr("0.4.0 撮影エフェクト：初期状態はOFFです。"));
            }
            ImGuiMCP::End();
            ImGuiMCP::PopStyleColor();
            if (!open) { g_photoEffectWindowOpen = false; }
        }

        std::uint8_t OverlayAlpha(float strength)
        {
            return static_cast<std::uint8_t>(std::clamp(strength, 0.0F, 1.0F) * 255.0F + 0.5F);
        }

        std::uint8_t OverlayColorComponent(float component)
        {
            return static_cast<std::uint8_t>(std::clamp(component, 0.0F, 1.0F) * 255.0F + 0.5F);
        }

        void DrawVignette(ImDrawList* drawList, const ImVec2& top, const ImVec2& size,
            float strength, float spread)
        {
            const auto dark = Color(0, 0, 0, OverlayAlpha(strength));
            const auto clear = Color(0, 0, 0, 0);
            const float edge = std::clamp(std::min(size.x, size.y) * spread, 1.0F,
                std::min(size.x, size.y) * .49F);
            const ImVec2 end{top.x + size.x, top.y + size.y};
            ImGuiMCP::ImDrawListManager::AddRectFilledMultiColor(drawList, top, {top.x + edge, end.y},
                dark, clear, clear, dark);
            ImGuiMCP::ImDrawListManager::AddRectFilledMultiColor(drawList, {end.x - edge, top.y}, end,
                clear, dark, dark, clear);
            ImGuiMCP::ImDrawListManager::AddRectFilledMultiColor(drawList, top, {end.x, top.y + edge},
                dark, dark, clear, clear);
            ImGuiMCP::ImDrawListManager::AddRectFilledMultiColor(drawList, {top.x, end.y - edge}, end,
                clear, clear, dark, dark);
        }

        void DrawGradientTriangle(ImDrawList* drawList,
            const ImVec2& first, ImU32 firstColor,
            const ImVec2& second, ImU32 secondColor,
            const ImVec2& third, ImU32 thirdColor)
        {
            const auto baseVertex = static_cast<ImGuiMCP::ImDrawIdx>(drawList->_VtxCurrentIdx);
            const auto uv = ImGuiMCP::GetFontTexUvWhitePixel();
            ImGuiMCP::ImDrawListManager::PrimReserve(drawList, 3, 3);
            ImGuiMCP::ImDrawListManager::PrimWriteIdx(drawList, baseVertex);
            ImGuiMCP::ImDrawListManager::PrimWriteIdx(drawList,
                static_cast<ImGuiMCP::ImDrawIdx>(baseVertex + 1));
            ImGuiMCP::ImDrawListManager::PrimWriteIdx(drawList,
                static_cast<ImGuiMCP::ImDrawIdx>(baseVertex + 2));
            ImGuiMCP::ImDrawListManager::PrimWriteVtx(drawList, first, uv, firstColor);
            ImGuiMCP::ImDrawListManager::PrimWriteVtx(drawList, second, uv, secondColor);
            ImGuiMCP::ImDrawListManager::PrimWriteVtx(drawList, third, uv, thirdColor);
        }

        void DrawDirectionalShade(ImDrawList* drawList, const ImVec2& top, const ImVec2& size,
            const ScreenShadeSettings& settings)
        {
            const auto dark = Color(0, 0, 0, OverlayAlpha(settings.strength));
            const auto clear = Color(0, 0, 0, 0);
            const ImVec2 end{top.x + size.x, top.y + size.y};
            const float horizontal = std::clamp(size.x * settings.spread, 1.0F, size.x);
            const float vertical = std::clamp(size.y * settings.spread, 1.0F, size.y);
            const float diagonalScale = ScreenDiagonalShadeReachScale(settings.spread);
            const float diagonalHorizontal = size.x * diagonalScale;
            const float diagonalVertical = size.y * diagonalScale;
            switch (settings.direction) {
            case ScreenShadeDirection::Left:
                ImGuiMCP::ImDrawListManager::AddRectFilledMultiColor(drawList, top, {top.x + horizontal, end.y},
                    dark, clear, clear, dark);
                break;
            case ScreenShadeDirection::Right:
                ImGuiMCP::ImDrawListManager::AddRectFilledMultiColor(drawList, {end.x - horizontal, top.y}, end,
                    clear, dark, dark, clear);
                break;
            case ScreenShadeDirection::LowerLeft:
                DrawGradientTriangle(drawList,
                    {top.x, end.y - diagonalVertical}, clear,
                    {top.x + diagonalHorizontal, end.y}, clear,
                    {top.x, end.y}, dark);
                break;
            case ScreenShadeDirection::Bottom:
                ImGuiMCP::ImDrawListManager::AddRectFilledMultiColor(drawList,
                    {top.x, end.y - vertical}, end, clear, clear, dark, dark);
                break;
            case ScreenShadeDirection::LowerRight:
                DrawGradientTriangle(drawList,
                    {end.x, end.y - diagonalVertical}, clear,
                    {end.x - diagonalHorizontal, end.y}, clear,
                    {end.x, end.y}, dark);
                break;
            case ScreenShadeDirection::Count:
                break;
            }
        }

        void DrawLightGlow(ImDrawList* drawList, const ImVec2& top, const ImVec2& size,
            const ScreenLightSettings& settings)
        {
            const auto red = OverlayColorComponent(settings.color[0]);
            const auto green = OverlayColorComponent(settings.color[1]);
            const auto blue = OverlayColorComponent(settings.color[2]);
            const auto light = Color(red, green, blue, OverlayAlpha(settings.strength));
            const auto clear = Color(red, green, blue, 0);
            const ImVec2 end{top.x + size.x, top.y + size.y};

            if (settings.direction == ScreenLightDirection::UpperLeft) {
                const float diagonalScale = ScreenDiagonalShadeReachScale(settings.spread);
                DrawGradientTriangle(drawList,
                    {top.x, top.y + size.y * diagonalScale}, clear,
                    {top.x + size.x * diagonalScale, top.y}, clear,
                    top, light);
            } else if (settings.direction == ScreenLightDirection::UpperRight) {
                const float diagonalScale = ScreenDiagonalShadeReachScale(settings.spread);
                DrawGradientTriangle(drawList,
                    {end.x, top.y + size.y * diagonalScale}, clear,
                    {end.x - size.x * diagonalScale, top.y}, clear,
                    {end.x, top.y}, light);
            } else {
                constexpr std::size_t kArcSegments = 64;
                constexpr float kPi = 3.14159265358979323846F;
                const float horizontal = std::clamp(size.x * settings.spread, 1.0F, size.x);
                const float vertical = std::clamp(size.y * settings.spread, 1.0F, size.y);
                const float center = top.x + size.x * 0.5F;
                const float halfReach = std::min(horizontal, size.x * 0.5F);
                std::array<ImVec2, kArcSegments + 1> points{};

                for (std::size_t segment = 0; segment <= kArcSegments; ++segment) {
                    const float angle = kPi - kPi * static_cast<float>(segment) /
                        static_cast<float>(kArcSegments);
                    points[segment] = {
                        center + std::cos(angle) * halfReach,
                        top.y + std::sin(angle) * vertical
                    };
                }

                // A triangle fan gives every pixel a continuously interpolated alpha from the fixed
                // top-center light source to the transparent half-ellipse edge. This avoids seams,
                // rectangular bands, and the visible steps produced by stacked solid shapes.
                const auto baseVertex = static_cast<ImGuiMCP::ImDrawIdx>(drawList->_VtxCurrentIdx);
                const auto uv = ImGuiMCP::GetFontTexUvWhitePixel();
                ImGuiMCP::ImDrawListManager::PrimReserve(drawList,
                    static_cast<int>(kArcSegments * 3), static_cast<int>(points.size() + 1));
                for (std::size_t segment = 0; segment < kArcSegments; ++segment) {
                    ImGuiMCP::ImDrawListManager::PrimWriteIdx(drawList, baseVertex);
                    ImGuiMCP::ImDrawListManager::PrimWriteIdx(drawList,
                        static_cast<ImGuiMCP::ImDrawIdx>(baseVertex + segment + 1));
                    ImGuiMCP::ImDrawListManager::PrimWriteIdx(drawList,
                        static_cast<ImGuiMCP::ImDrawIdx>(baseVertex + segment + 2));
                }
                ImGuiMCP::ImDrawListManager::PrimWriteVtx(drawList,
                    {center, top.y}, uv, light);
                for (const auto& point : points) {
                    ImGuiMCP::ImDrawListManager::PrimWriteVtx(drawList, point, uv, clear);
                }
            }
        }

        bool DrawScreenToneSwatch(const char* id, const char* tooltip,
            const std::array<float, 3>& color, bool selected)
        {
            const float size = ImGuiMCP::GetFrameHeight();
            const bool clicked = ImGuiMCP::ColorButton(id,
                {color[0], color[1], color[2], 1.0F},
                ImGuiMCP::ImGuiColorEditFlags_NoTooltip, {size, size});
            const ImVec2 top = ImGuiMCP::GetItemRectMin();
            const ImVec2 end = ImGuiMCP::GetItemRectMax();
            auto* drawList = ImGuiMCP::GetWindowDrawList();
            if (selected) {
                ImGuiMCP::ImDrawListManager::AddRect(drawList, top, end,
                    Color(255, 202, 102), 3.0F, 0, 2.0F);
                const float luminance = color[0] * .299F + color[1] * .587F + color[2] * .114F;
                const auto check = luminance > .68F ? Color(24, 31, 39) : Color(244, 248, 251);
                const ImVec2 first{top.x + size * .25F, top.y + size * .54F};
                const ImVec2 middle{top.x + size * .43F, top.y + size * .72F};
                const ImVec2 last{top.x + size * .76F, top.y + size * .30F};
                ImGuiMCP::ImDrawListManager::AddLine(drawList, first, middle, check, 2.0F);
                ImGuiMCP::ImDrawListManager::AddLine(drawList, middle, last, check, 2.0F);
            }
            if (ImGuiMCP::IsItemHovered()) { Tooltip(tooltip); }
            return clicked;
        }

        const char* ScreenOverlayLayerLabel(ScreenOverlayLayer layer)
        {
            switch (layer) {
            case ScreenOverlayLayer::Light: return Tr("光・色");
            case ScreenOverlayLayer::Shade: return Tr("方向性の影");
            case ScreenOverlayLayer::Vignette: return Tr("ビネット");
            case ScreenOverlayLayer::Cinema: return Tr("上下枠");
            }
            return "";
        }

        void __stdcall DrawScreenOverlayHud()
        {
            if (!g_screenOverlay.enabled || !AnyScreenOverlayLayerEnabled(g_screenOverlay) ||
                !ValidScreenOverlaySettings(g_screenOverlay)) {
                return;
            }
            auto* viewport = ImGuiMCP::GetMainViewport();
            if (!viewport || viewport->Size.x <= 0.0F || viewport->Size.y <= 0.0F) { return; }
            auto* drawList = ImGuiMCP::GetBackgroundDrawList(viewport);
            if (!drawList) { return; }

            const ImVec2 top = viewport->Pos;
            const ImVec2 size = viewport->Size;
            const ImVec2 end{top.x + size.x, top.y + size.y};

            // Keep the layer order deterministic so users never need to reason about compositing order.
            if (g_screenOverlay.light.enabled) {
                DrawLightGlow(drawList, top, size, g_screenOverlay.light);
            }
            if (g_screenOverlay.shade.enabled) {
                DrawDirectionalShade(drawList, top, size, g_screenOverlay.shade);
            }
            if (g_screenOverlay.vignette.enabled) {
                DrawVignette(drawList, top, size,
                    g_screenOverlay.vignette.strength, g_screenOverlay.vignette.spread);
            }
            if (g_screenOverlay.cinema.enabled) {
                const auto bars = CalculateCinemaBars(size.x, size.y, g_screenOverlay.cinema.aspect);
                if (bars.visible) {
                    const auto black = Color(0, 0, 0, OverlayAlpha(g_screenOverlay.cinema.strength));
                    ImGuiMCP::ImDrawListManager::AddRectFilled(drawList, top,
                        {end.x, top.y + bars.height}, black, 0.0F, 0);
                    ImGuiMCP::ImDrawListManager::AddRectFilled(drawList,
                        {top.x, end.y - bars.height}, end, black, 0.0F, 0);
                }
            }
        }

        void DrawScreenOverlayWindow()
        {
            if (!g_screenOverlayWindowOpen) { return; }
            const auto* viewport = ImGuiMCP::GetMainViewport();
            const ImVec2 maximum{viewport->WorkSize.x * .9F, viewport->WorkSize.y * .9F};
            ImGuiMCP::SetNextWindowSize({std::min(390.0F, maximum.x), std::min(500.0F, maximum.y)},
                ImGuiMCP::ImGuiCond_FirstUseEver);
            ImGuiMCP::SetNextWindowSizeConstraints(
                {std::min(320.0F, maximum.x), std::min(300.0F, maximum.y)}, maximum);
            if (g_focusScreenOverlayWindow) {
                ImGuiMCP::SetNextWindowFocus();
                g_focusScreenOverlayWindow = false;
            }
            bool open = true;
            ImGuiMCP::PushStyleColor(ImGuiMCP::ImGuiCol_WindowBg, Color(12, 18, 27));
            const bool visible = ImGuiMCP::Begin(Tr("画面演出###sla_screen_overlay"), &open,
                ImGuiMCP::ImGuiWindowFlags_NoCollapse);
            if (visible) {
                bool enabled = g_screenOverlay.enabled;
                if (ImGuiMCP::Checkbox(Tr("画面演出を使用"), &enabled)) {
                    g_screenOverlay.enabled = enabled;
                }
                ImGuiMCP::TextWrapped(Tr("同じボタンをもう一度押すと、その演出だけOFFになります。"));
                ImGuiMCP::Separator();

                const float gap = ImGuiMCP::GetStyle()->ItemSpacing.x;
                ImGuiMCP::TextUnformatted(Tr("光・色"));
                ImGuiMCP::SameLine(0, gap);
                struct ToneSwatch
                {
                    ScreenLightTone tone;
                    const char* id;
                    const char* label;
                    std::array<float, 3> color;
                };
                const ToneSwatch tones[]{
                    {ScreenLightTone::Warm, "##tone_warm", "暖色", ScreenLightToneColor(ScreenLightTone::Warm)},
                    {ScreenLightTone::Neutral, "##tone_neutral", "白色", ScreenLightToneColor(ScreenLightTone::Neutral)},
                    {ScreenLightTone::Cool, "##tone_cool", "寒色", ScreenLightToneColor(ScreenLightTone::Cool)},
                    {ScreenLightTone::Custom, "##tone_custom", "自由色", g_screenOverlay.light.customColor}
                };
                for (std::size_t index = 0; index < std::size(tones); ++index) {
                    if (DrawScreenToneSwatch(tones[index].id, Tr(tones[index].label), tones[index].color,
                            g_screenOverlay.light.tone == tones[index].tone)) {
                        SetScreenLightTone(g_screenOverlay, tones[index].tone);
                        g_screenOverlayDetail = ScreenOverlayLayer::Light;
                    }
                    if (index + 1 < std::size(tones)) { ImGuiMCP::SameLine(0, gap); }
                }

                const float threeWidth = std::max(74.0F,
                    (ImGuiMCP::GetContentRegionAvail().x - gap * 2.0F) / 3.0F);
                if (SelectableButton(Tr("左上"),
                        ScreenOverlayLayerActive(g_screenOverlay, g_screenOverlay.light.enabled) &&
                            g_screenOverlay.light.direction == ScreenLightDirection::UpperLeft,
                        threeWidth)) {
                    ToggleScreenLightDirection(g_screenOverlay, ScreenLightDirection::UpperLeft);
                    g_screenOverlayDetail = ScreenOverlayLayer::Light;
                }
                ImGuiMCP::SameLine(0, gap);
                if (SelectableButton(Tr("上中央"),
                        ScreenOverlayLayerActive(g_screenOverlay, g_screenOverlay.light.enabled) &&
                            g_screenOverlay.light.direction == ScreenLightDirection::UpperCenter,
                        threeWidth)) {
                    ToggleScreenLightDirection(g_screenOverlay, ScreenLightDirection::UpperCenter);
                    g_screenOverlayDetail = ScreenOverlayLayer::Light;
                }
                ImGuiMCP::SameLine(0, gap);
                if (SelectableButton(Tr("右上"),
                        ScreenOverlayLayerActive(g_screenOverlay, g_screenOverlay.light.enabled) &&
                            g_screenOverlay.light.direction == ScreenLightDirection::UpperRight,
                        threeWidth)) {
                    ToggleScreenLightDirection(g_screenOverlay, ScreenLightDirection::UpperRight);
                    g_screenOverlayDetail = ScreenOverlayLayer::Light;
                }

                ImGuiMCP::Separator();
                ImGuiMCP::TextUnformatted(Tr("方向性の影"));
                const float twoWidth = std::max(112.0F,
                    (ImGuiMCP::GetContentRegionAvail().x - gap) * .5F);
                if (SelectableButton(Tr("左"),
                        ScreenOverlayLayerActive(g_screenOverlay, g_screenOverlay.shade.enabled) &&
                            g_screenOverlay.shade.direction == ScreenShadeDirection::Left,
                        twoWidth)) {
                    ToggleScreenShadeDirection(g_screenOverlay, ScreenShadeDirection::Left);
                    g_screenOverlayDetail = ScreenOverlayLayer::Shade;
                }
                ImGuiMCP::SameLine(0, gap);
                if (SelectableButton(Tr("右"),
                        ScreenOverlayLayerActive(g_screenOverlay, g_screenOverlay.shade.enabled) &&
                            g_screenOverlay.shade.direction == ScreenShadeDirection::Right,
                        twoWidth)) {
                    ToggleScreenShadeDirection(g_screenOverlay, ScreenShadeDirection::Right);
                    g_screenOverlayDetail = ScreenOverlayLayer::Shade;
                }
                if (SelectableButton(Tr("左下"),
                        ScreenOverlayLayerActive(g_screenOverlay, g_screenOverlay.shade.enabled) &&
                            g_screenOverlay.shade.direction == ScreenShadeDirection::LowerLeft,
                        threeWidth)) {
                    ToggleScreenShadeDirection(g_screenOverlay, ScreenShadeDirection::LowerLeft);
                    g_screenOverlayDetail = ScreenOverlayLayer::Shade;
                }
                ImGuiMCP::SameLine(0, gap);
                if (SelectableButton(Tr("下"),
                        ScreenOverlayLayerActive(g_screenOverlay, g_screenOverlay.shade.enabled) &&
                            g_screenOverlay.shade.direction == ScreenShadeDirection::Bottom,
                        threeWidth)) {
                    ToggleScreenShadeDirection(g_screenOverlay, ScreenShadeDirection::Bottom);
                    g_screenOverlayDetail = ScreenOverlayLayer::Shade;
                }
                ImGuiMCP::SameLine(0, gap);
                if (SelectableButton(Tr("右下"),
                        ScreenOverlayLayerActive(g_screenOverlay, g_screenOverlay.shade.enabled) &&
                            g_screenOverlay.shade.direction == ScreenShadeDirection::LowerRight,
                        threeWidth)) {
                    ToggleScreenShadeDirection(g_screenOverlay, ScreenShadeDirection::LowerRight);
                    g_screenOverlayDetail = ScreenOverlayLayer::Shade;
                }

                ImGuiMCP::Separator();
                ImGuiMCP::TextUnformatted(Tr("ビネット"));
                const float halfWidth = std::max(112.0F,
                    (ImGuiMCP::GetContentRegionAvail().x - gap) * .5F);
                if (SelectableButton(Tr("柔らかい"),
                        ScreenOverlayLayerActive(g_screenOverlay, g_screenOverlay.vignette.enabled) &&
                            g_screenOverlay.vignette.style == ScreenVignetteStyle::Soft,
                        halfWidth)) {
                    ToggleScreenVignetteStyle(g_screenOverlay, ScreenVignetteStyle::Soft);
                    g_screenOverlayDetail = ScreenOverlayLayer::Vignette;
                }
                ImGuiMCP::SameLine(0, gap);
                if (SelectableButton(Tr("標準"),
                        ScreenOverlayLayerActive(g_screenOverlay, g_screenOverlay.vignette.enabled) &&
                            g_screenOverlay.vignette.style == ScreenVignetteStyle::Standard,
                        halfWidth)) {
                    ToggleScreenVignetteStyle(g_screenOverlay, ScreenVignetteStyle::Standard);
                    g_screenOverlayDetail = ScreenOverlayLayer::Vignette;
                }

                ImGuiMCP::Separator();
                ImGuiMCP::TextUnformatted(Tr("上下枠"));
                constexpr const char* aspectLabels[]{"2.20:1", "2.39:1", "2.76:1"};
                ImGuiMCP::PushID("screen_overlay_aspect");
                for (std::size_t index = 0; index < kScreenCinemaPresetAspects.size(); ++index) {
                    if (SelectableButton(aspectLabels[index],
                            ScreenOverlayLayerActive(g_screenOverlay, g_screenOverlay.cinema.enabled) &&
                                std::abs(g_screenOverlay.cinema.aspect - kScreenCinemaPresetAspects[index]) < 0.005F,
                            threeWidth)) {
                        ToggleScreenCinemaAspect(g_screenOverlay, kScreenCinemaPresetAspects[index]);
                        g_screenOverlayDetail = ScreenOverlayLayer::Cinema;
                    }
                    if (index + 1 < kScreenCinemaPresetAspects.size()) { ImGuiMCP::SameLine(0, gap); }
                }
                ImGuiMCP::PopID();

                ImGuiMCP::Separator();
                if (ImGuiMCP::CollapsingHeader(Tr("詳細調整"))) {
                    ImGuiMCP::TextDisabled("%s", ScreenOverlayLayerLabel(g_screenOverlayDetail));
                    const float sliderWidth = std::clamp(ImGuiMCP::GetFontSize() * 10.0F, 180.0F, 330.0F);
                    if (g_screenOverlayDetail == ScreenOverlayLayer::Light) {
                        ImGuiMCP::SetNextItemWidth(sliderWidth);
                        ImGuiMCP::SliderFloat(Tr("強さ##screen_overlay"), &g_screenOverlay.light.strength,
                            kScreenOverlayStrengthMin, kScreenOverlayStrengthMax, "%.2f");
                        ImGuiMCP::SetNextItemWidth(sliderWidth);
                        ImGuiMCP::SliderFloat(Tr("広がり##screen_overlay"), &g_screenOverlay.light.spread,
                            kScreenOverlaySpreadMin, kScreenOverlaySpreadMax, "%.2f");
                        ImGuiMCP::SetNextItemWidth(sliderWidth);
                        if (ImGuiMCP::ColorEdit3(Tr("自由色##screen_overlay"), g_screenOverlay.light.customColor.data(),
                                ImGuiMCP::ImGuiColorEditFlags_NoAlpha)) {
                            SetScreenLightCustomColor(g_screenOverlay, g_screenOverlay.light.customColor);
                        }
                    } else if (g_screenOverlayDetail == ScreenOverlayLayer::Shade) {
                        ImGuiMCP::SetNextItemWidth(sliderWidth);
                        ImGuiMCP::SliderFloat(Tr("強さ##screen_overlay"), &g_screenOverlay.shade.strength,
                            kScreenOverlayStrengthMin, kScreenOverlayStrengthMax, "%.2f");
                        ImGuiMCP::SetNextItemWidth(sliderWidth);
                        ImGuiMCP::SliderFloat(Tr("広がり##screen_overlay"), &g_screenOverlay.shade.spread,
                            kScreenOverlaySpreadMin, kScreenOverlaySpreadMax, "%.2f");
                    } else if (g_screenOverlayDetail == ScreenOverlayLayer::Vignette) {
                        ImGuiMCP::SetNextItemWidth(sliderWidth);
                        ImGuiMCP::SliderFloat(Tr("強さ##screen_overlay"), &g_screenOverlay.vignette.strength,
                            kScreenOverlayStrengthMin, kScreenOverlayStrengthMax, "%.2f");
                        ImGuiMCP::SetNextItemWidth(sliderWidth);
                        ImGuiMCP::SliderFloat(Tr("広がり##screen_overlay"), &g_screenOverlay.vignette.spread,
                            kScreenOverlaySpreadMin, kScreenOverlaySpreadMax, "%.2f");
                    } else {
                        ImGuiMCP::SetNextItemWidth(sliderWidth);
                        ImGuiMCP::SliderFloat(Tr("強さ##screen_overlay"), &g_screenOverlay.cinema.strength,
                            kScreenOverlayStrengthMin, kScreenOverlayStrengthMax, "%.2f");
                        ImGuiMCP::SetNextItemWidth(sliderWidth);
                        ImGuiMCP::SliderFloat(Tr("画面比率##screen_overlay"), &g_screenOverlay.cinema.aspect,
                            kScreenOverlayAspectMin, kScreenOverlayAspectMax, "%.2f : 1");
                    }
                    if (SelectableButton(Tr("選択中の調整値を戻す"))) {
                        ResetScreenOverlayLayerValues(g_screenOverlay, g_screenOverlayDetail);
                    }
                }
                ImGuiMCP::Separator();
                ImGuiMCP::TextWrapped(Tr("光・影・ビネット・上下枠の順で重ねます。設定は保存せず、ゲーム起動時は全体OFFです。"));
            }
            ImGuiMCP::End();
            ImGuiMCP::PopStyleColor();
            if (!open) { g_screenOverlayWindowOpen = false; }
        }

        void __stdcall DrawScreenPlacementHud()
        {
            const auto runtime = RuntimeLight::GetView();
            if (const auto* viewport = ImGuiMCP::GetMainViewport(); viewport) {
                RuntimeLight::SyncScreenPlacementViewport(viewport->Size.x, viewport->Size.y);
            }
            if (!runtime.screenPlacement.positioning) {
                g_screenPlacementPanelVisible = false;
                g_screenPlacementColorPopupVisible = false;
                return;
            }
            auto* viewport = ImGuiMCP::GetMainViewport();
            if (!viewport || viewport->Size.x < 2.0F || viewport->Size.y < 2.0F) { return; }
            auto* draw = ImGuiMCP::GetForegroundDrawList(viewport);
            if (!draw) { return; }
            const auto mouse = ImGuiMCP::GetMousePos();
            const ImVec2 marker{
                std::clamp(mouse.x, viewport->Pos.x, viewport->Pos.x + viewport->Size.x - 1.0F),
                std::clamp(mouse.y, viewport->Pos.y, viewport->Pos.y + viewport->Size.y - 1.0F)};
            const bool overPanel = g_screenPlacementPanelVisible &&
                mouse.x >= g_screenPlacementPanelMin.x && mouse.x <= g_screenPlacementPanelMax.x &&
                mouse.y >= g_screenPlacementPanelMin.y && mouse.y <= g_screenPlacementPanelMax.y;
            const bool colorPickerOpen = g_screenPlacementColorPopupVisible;
            const bool overControls = overPanel || colorPickerOpen;
            ImGuiMCP::SetMouseCursor(overControls ? ImGuiMCP::ImGuiMouseCursor_Arrow :
                ImGuiMCP::ImGuiMouseCursor_None);
            constexpr float radius = 16.0F;
            const auto amber = Color(255, 196, 82, 255);
            const auto outline = Color(15, 18, 24, 220);
            if (!overControls) {
                ImGuiMCP::ImDrawListManager::AddCircleFilled(draw, marker, radius + 3.0F,
                    Color(15, 18, 24, 205), 40);
                ImGuiMCP::ImDrawListManager::AddCircleFilled(draw, marker, radius - 3.0F,
                    Color(255, 196, 82, 70), 40);
                ImGuiMCP::ImDrawListManager::AddCircle(draw, marker, radius, amber, 40, 3.0F);
                for (int index = 0; index < 8; ++index) {
                    const float angle = index * std::numbers::pi_v<float> * .25F;
                    const ImVec2 a{marker.x + std::cos(angle) * (radius + 4.0F),
                        marker.y + std::sin(angle) * (radius + 4.0F)};
                    const ImVec2 b{marker.x + std::cos(angle) * (radius + 10.0F),
                        marker.y + std::sin(angle) * (radius + 10.0F)};
                    ImGuiMCP::ImDrawListManager::AddLine(draw, a, b, amber, 2.0F);
                }
            }

            const auto message = std::format("{} / {}: {:.2f} m",
                Tr("画面配置ライト：小窓外で移動 / ホイールで距離（Shiftで高速） / 左クリックで配置 / 右クリックでキャンセル"),
                Tr("対象からの距離"), runtime.screenPlacement.light.distance);
            const auto textSize = ImGuiMCP::CalcTextSize(message.c_str());
            const ImVec2 textTop{viewport->Pos.x + (viewport->Size.x - textSize.x) * .5F,
                viewport->Pos.y + ImGuiMCP::GetFontSize() * 1.5F};
            ImGuiMCP::ImDrawListManager::AddRectFilled(draw,
                {textTop.x - 10.0F, textTop.y - 6.0F},
                {textTop.x + textSize.x + 10.0F, textTop.y + textSize.y + 6.0F},
                Color(8, 12, 18, 210), 4.0F, 0);
            ImGuiMCP::ImDrawListManager::AddText(draw, ImGuiMCP::GetFont(), ImGuiMCP::GetFontSize(),
                textTop, Color(244, 247, 250), message.c_str());
        }

        bool DrawScreenPlacementControls(const RuntimeLight::View& runtime)
        {
            if (!runtime.screenPlacement.positioning) { return false; }
            auto* viewport = ImGuiMCP::GetMainViewport();
            if (!viewport) { return false; }

            auto light = runtime.screenPlacement.light;
            const float width = std::clamp(ImGuiMCP::GetFontSize() * 14.0F, 230.0F,
                std::min(310.0F, viewport->WorkSize.x * .34F));
            ImGuiMCP::SetNextWindowPos(
                {viewport->WorkPos.x + viewport->WorkSize.x - width - ImGuiMCP::GetFontSize() * 1.5F,
                 viewport->WorkPos.y + ImGuiMCP::GetFontSize() * 4.0F},
                ImGuiMCP::ImGuiCond_FirstUseEver);
            ImGuiMCP::SetNextWindowSizeConstraints(
                {width, 0.0F}, {width, viewport->WorkSize.y * .72F});

            constexpr auto popupId = "screen_placement_color_picker";
            const bool popupWasOpen = ImGuiMCP::IsPopupOpen(popupId);
            bool changed = false;
            bool panelHovered = false;
            bool popupHovered = false;
            ImVec2 panelMin{};
            ImVec2 panelMax{};
            ImGuiMCP::PushStyleColor(ImGuiMCP::ImGuiCol_WindowBg, Color(12, 18, 27, 235));
            const bool visible = ImGuiMCP::Begin(
                Tr("画面配置ライト###sla_screen_placement_controls"), nullptr,
                ImGuiMCP::ImGuiWindowFlags_AlwaysAutoResize | ImGuiMCP::ImGuiWindowFlags_NoCollapse);
            panelMin = ImGuiMCP::GetWindowPos();
            const auto panelSize = ImGuiMCP::GetWindowSize();
            panelMax = {panelMin.x + panelSize.x, panelMin.y + panelSize.y};
            panelHovered = ImGuiMCP::IsWindowHovered(
                ImGuiMCP::ImGuiHoveredFlags_AllowWhenBlockedByActiveItem);
            if (visible) {
                ImGuiMCP::AlignTextToFramePadding();
                ImGuiMCP::TextUnformatted(Tr("強さ"));
                ImGuiMCP::SameLine();
                ImGuiMCP::SetNextItemWidth(-1.0F);
                changed = ImGuiMCP::SliderFloat("##screen_placement_intensity", &light.intensity,
                    0.0F, 3.0F, "%.2f") || changed;
                ImGuiMCP::AlignTextToFramePadding();
                ImGuiMCP::TextUnformatted(Tr("範囲"));
                ImGuiMCP::SameLine();
                ImGuiMCP::SetNextItemWidth(-1.0F);
                changed = ImGuiMCP::SliderFloat("##screen_placement_range", &light.range,
                    kScreenPlacementRangeMin, kScreenPlacementRangeMax, "%.2f m") || changed;
                ImGuiMCP::AlignTextToFramePadding();
                ImGuiMCP::TextUnformatted(Tr("距離"));
                ImGuiMCP::SameLine();
                ImGuiMCP::SetNextItemWidth(-1.0F);
                changed = ImGuiMCP::SliderFloat("##screen_placement_distance", &light.distance,
                    kScreenPlacementDepthMin, kScreenPlacementDepthMax, "%.2f m") || changed;
                if (ImGuiMCP::IsItemHovered()) {
                    Tooltip(Tr("対象からカメラ側へ置く距離です。カメラを越えないよう、実際の配置は現在のカメラ位置の手前で安全に制限されます。"));
                }

                ImGuiMCP::AlignTextToFramePadding();
                ImGuiMCP::TextUnformatted(Tr("影"));
                ImGuiMCP::SameLine();
                constexpr std::array<const char*, 3> shadowLabels{"無", "全方向", "スポット"};
                for (int mode = 0; mode < static_cast<int>(shadowLabels.size()); ++mode) {
                    if (mode > 0) { ImGuiMCP::SameLine(); }
                    const bool selected = mode == 0 ? !light.castsShadow :
                        light.castsShadow && light.shadowProjection ==
                            (mode == 1 ? ShadowProjection::Omni : ShadowProjection::Spot);
                    ImGuiMCP::PushID(mode);
                    if (SelectableButton(Tr(shadowLabels[mode]), selected)) {
                        light.castsShadow = mode != 0;
                        if (mode != 0) {
                            light.shadowProjection = mode == 1 ? ShadowProjection::Omni : ShadowProjection::Spot;
                        }
                        changed = true;
                    }
                    ImGuiMCP::PopID();
                }

                const auto tint = RuntimeTint(light).value_or(LightTint{1.0F, 1.0F, 1.0F});
                const float swatchSize = ImGuiMCP::GetFrameHeight();
                if (ImGuiMCP::ColorButton("##screen_placement_color",
                        {tint.red, tint.green, tint.blue, 1.0F},
                        ImGuiMCP::ImGuiColorEditFlags_NoTooltip, {swatchSize, swatchSize})) {
                    ImGuiMCP::OpenPopup(popupId);
                }
                ImGuiMCP::SameLine();
                ImGuiMCP::AlignTextToFramePadding();
                ImGuiMCP::TextUnformatted(Tr("色"));

                ImGuiMCP::PushStyleColor(ImGuiMCP::ImGuiCol_PopupBg, Color(12, 18, 27));
                if (ImGuiMCP::BeginPopup(popupId)) {
                    popupHovered = ImGuiMCP::IsWindowHovered(
                        ImGuiMCP::ImGuiHoveredFlags_AllowWhenBlockedByActiveItem);
                    constexpr std::array<LightTint, 8> palette{{
                        {1,1,1}, {1,0.8F,0.53F}, {1,0.6F,0.29F}, {0.3F,0.55F,1},
                        {1,0.25F,0.2F}, {0.35F,1,0.45F}, {0.3F,1,1}, {0.85F,0.4F,1}
                    }};
                    const float paletteSize = ImGuiMCP::GetFontSize() * 1.25F;
                    for (int index = 0; index < static_cast<int>(palette.size()); ++index) {
                        ImGuiMCP::PushID(index);
                        const auto& color = palette[index];
                        if (ImGuiMCP::ColorButton("##swatch",
                                {color.red, color.green, color.blue, 1.0F},
                                ImGuiMCP::ImGuiColorEditFlags_NoTooltip, {paletteSize, paletteSize})) {
                            light.customTint = color;
                            changed = true;
                        }
                        ImGuiMCP::PopID();
                        if (index != 3 && index != 7) { ImGuiMCP::SameLine(); }
                    }
                    const auto current = RuntimeTint(light).value_or(LightTint{1.0F, 1.0F, 1.0F});
                    float rgb[]{current.red, current.green, current.blue};
                    ImGuiMCP::SetNextItemWidth(std::min(ImGuiMCP::GetFontSize() * 13.0F,
                        viewport->WorkSize.x * .8F));
                    if (ImGuiMCP::ColorPicker3("##screen_placement_picker", rgb,
                            ImGuiMCP::ImGuiColorEditFlags_DisplayRGB |
                            ImGuiMCP::ImGuiColorEditFlags_Uint8 |
                            ImGuiMCP::ImGuiColorEditFlags_PickerHueBar |
                            ImGuiMCP::ImGuiColorEditFlags_NoSidePreview |
                            ImGuiMCP::ImGuiColorEditFlags_NoAlpha)) {
                        light.customTint = LightTint{rgb[0], rgb[1], rgb[2]};
                        changed = true;
                    }
                    if (SelectableButton(Tr("種類の元の色に戻す"))) {
                        light.customTint.reset();
                        changed = true;
                    }
                    ImGuiMCP::EndPopup();
                }
                ImGuiMCP::PopStyleColor();
            }
            g_screenPlacementColorPopupVisible = ImGuiMCP::IsPopupOpen(popupId);
            ImGuiMCP::End();
            ImGuiMCP::PopStyleColor();

            g_screenPlacementPanelVisible = true;
            g_screenPlacementPanelMin = panelMin;
            g_screenPlacementPanelMax = panelMax;

            if (changed) { RuntimeLight::SetScreenPlacementSettings(light); }

            const auto mouse = ImGuiMCP::GetMousePos();
            const bool insidePanel = panelHovered || (mouse.x >= panelMin.x && mouse.x <= panelMax.x &&
                mouse.y >= panelMin.y && mouse.y <= panelMax.y);
            if (insidePanel || popupHovered || popupWasOpen || ImGuiMCP::IsPopupOpen(popupId)) {
                ImGuiMCP::SetMouseCursor(ImGuiMCP::ImGuiMouseCursor_Arrow);
            }
            static bool panelMouseCapture = false;
            if (insidePanel && ImGuiMCP::IsMouseClicked(ImGuiMCP::ImGuiMouseButton_Left)) {
                panelMouseCapture = true;
            }
            const bool capture = insidePanel || panelMouseCapture || popupHovered || popupWasOpen ||
                ImGuiMCP::IsPopupOpen(popupId) ||
                ImGuiMCP::IsAnyItemActive();
            if (!ImGuiMCP::IsMouseDown(ImGuiMCP::ImGuiMouseButton_Left)) {
                panelMouseCapture = false;
            }
            return capture;
        }

        void UpdateScreenPlacementInteraction(bool controlsCaptureMouse)
        {
            const auto runtime = RuntimeLight::GetView();
            if (!runtime.screenPlacement.positioning) {
                RestoreScreenPlacementMenu();
                return;
            }
            const auto* viewport = ImGuiMCP::GetMainViewport();
            if (!viewport || viewport->Size.x < 2.0F || viewport->Size.y < 2.0F) { return; }
            const auto mouse = ImGuiMCP::GetMousePos();
            if (g_skipScreenPlacementClick) {
                g_skipScreenPlacementClick = false;
                return;
            }
            if (controlsCaptureMouse) { return; }
            RuntimeLight::UpdateScreenPlacement(mouse.x - viewport->Pos.x,
                mouse.y - viewport->Pos.y, viewport->Size.x, viewport->Size.y);
            if (const auto* io = ImGuiMCP::GetIO(); io && io->MouseWheel != 0.0F) {
                RuntimeLight::AdjustScreenPlacementDistance(io->MouseWheel, io->KeyShift);
            }
            if (ImGuiMCP::IsMouseClicked(ImGuiMCP::ImGuiMouseButton_Left)) {
                RuntimeLight::ConfirmScreenPlacement();
                RestoreScreenPlacementMenu();
            } else if (ImGuiMCP::IsMouseClicked(ImGuiMCP::ImGuiMouseButton_Right)) {
                RuntimeLight::DisableScreenPlacement();
                RestoreScreenPlacementMenu();
            }
        }

        void __stdcall RenderScreenPlacementWindow()
        {
            const auto runtime = RuntimeLight::GetView();
            UpdateScreenPlacementInteraction(DrawScreenPlacementControls(runtime));
        }

        void __stdcall RenderToolWindow()
        {
            const auto* viewport = ImGuiMCP::GetMainViewport();
            if (viewport) {
                const float margin = ImGuiMCP::GetFontSize() * 2.0F;
                const float maximumWidth = std::max(1.0F, viewport->WorkSize.x - margin);
                const float maximumHeight = std::max(1.0F, viewport->WorkSize.y - margin);
                ImGuiMCP::SetNextWindowSize({
                    std::min(ImGuiMCP::GetFontSize() * 31.0F, maximumWidth),
                    std::min(ImGuiMCP::GetFontSize() * 48.0F, maximumHeight)
                }, ImGuiMCP::ImGuiCond_FirstUseEver);
            }

            bool open = true;
            const bool visible = ImGuiMCP::Begin(
                "Screenshot Lighting Assistant###sla_tool_window",
                &open, ImGuiMCP::ImGuiWindowFlags_NoCollapse);
            if (visible) { Render(); }
            ImGuiMCP::End();

            if (!open && g_toolWindow) { g_toolWindow->IsOpen = false; }
        }

        void __stdcall ProcessToolWindowToggle()
        {
            if (!g_toolWindow) { return; }
            RuntimeLight::PollPhotoModeToolWindowHotkey();
            const bool currentlyOpen = g_toolWindow->IsOpen.load();
            if (g_toolWindowWasOpen && !currentlyOpen) {
                g_toolHotkeyWindowOpen = false;
                RuntimeLight::CancelToolWindowHotkeyCapture();
            }
            g_toolWindowWasOpen = currentlyOpen;
            if (!RuntimeLight::ConsumeToolWindowToggleRequest()) { return; }

            const bool opening = !g_toolWindow->IsOpen.load();
            if (!opening) {
                g_toolWindow->IsOpen = false;
                g_toolWindowWasOpen = false;
                g_toolHotkeyWindowOpen = false;
                RuntimeLight::CancelToolWindowHotkeyCapture();
                return;
            }

            if (g_screenPlacementWindow && g_screenPlacementWindow->IsOpen.load()) {
                RuntimeLight::DisableScreenPlacement();
                g_screenPlacementWindow->IsOpen = false;
                g_returnToMainAfterScreenPlacement = false;
                g_returnToToolWindowAfterScreenPlacement = false;
            }
            if (g_mainWindow) { g_mainWindow->IsOpen = false; }
            if (g_presetPicker.IsOpen()) { g_presetPicker.Close(); }
            g_openPresetRequested = false;
            g_openLightLibraryRequested = false;
            g_openRegistrationRequested = false;
            g_positionWindowOpen = false;
            g_persistentFaceWindowOpen = false;
            g_photoEffectWindowOpen = false;
            g_screenOverlayWindowOpen = false;
            g_toolHotkeyWindowOpen = false;
            RuntimeLight::CancelPersistentHotkeyCapture();
            RuntimeLight::CancelToolWindowHotkeyCapture();
            g_toolWindow->IsOpen = true;
            g_toolWindowWasOpen = true;
        }

        void __stdcall Render()
        {
            // The token comes from before this UI frame. A load/stop invalidates it,
            // so a delayed frame cannot resurrect a light from the previous session.
            const auto runtime = RuntimeLight::GetView();
            if (g_editScreenPlacement && !runtime.screenPlacement.settingsRemembered) {
                g_editScreenPlacement = false;
            }
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
            const float persistentFaceWidth = PersistentFaceLauncherWidth();
            const float photoEffectWidth = PhotoEffectLauncherWidth();
            const float screenOverlayWidth = ScreenOverlayLauncherWidth();
            if (ButtonWidth(powerLabel) + ButtonWidth("P") + persistentFaceWidth + photoEffectWidth + screenOverlayWidth +
                ImGuiMCP::GetStyle()->ItemSpacing.x * 5 <= topWidth) { ImGuiMCP::SameLine(0, ImGuiMCP::GetStyle()->ItemSpacing.x * 2); }
            if (SelectableButton("P")) {
                g_editor.FinishNumericEdit();
                if (g_presetPicker.IsOpen()) {
                    g_presetPicker.Close();
                    g_openPresetRequested = false;
                } else {
                    g_openPresetRequested = true;
                }
            }
            if (ImGuiMCP::IsItemHovered()) { Tooltip(Tr("プリセット一覧を開閉します（下のボタンと同じ一覧）。")); }
            if (ImGuiMCP::GetContentRegionAvail().x >= persistentFaceWidth) { ImGuiMCP::SameLine(); }
            if (DrawPersistentFaceLauncher(runtime.persistentFace.featureEnabled && runtime.persistentFace.enabled)) {
                g_editor.FinishNumericEdit();
                g_persistentFaceWindowOpen = !g_persistentFaceWindowOpen;
                g_focusPersistentFaceWindow = g_persistentFaceWindowOpen;
                if (!g_persistentFaceWindowOpen) { RuntimeLight::CancelPersistentHotkeyCapture(); }
            }
            if (ImGuiMCP::IsItemHovered()) { Tooltip(Tr("常用フェイスライトの設定を開閉します。機能とプレイヤーライトがONのとき橙色になります。")); }
            if (ImGuiMCP::GetContentRegionAvail().x >= photoEffectWidth) { ImGuiMCP::SameLine(); }
            if (DrawPhotoEffectLauncher(runtime.photoEffect.enabled)) {
                g_editor.FinishNumericEdit();
                g_photoEffectWindowOpen = !g_photoEffectWindowOpen;
                g_focusPhotoEffectWindow = g_photoEffectWindowOpen;
            }
            if (ImGuiMCP::IsItemHovered()) { Tooltip(Tr("撮影エフェクトの設定を開閉します。撮影エフェクトが有効なとき橙色になります。")); }
            if (ImGuiMCP::GetContentRegionAvail().x >= screenOverlayWidth) { ImGuiMCP::SameLine(); }
            if (DrawScreenOverlayLauncher(g_screenOverlay.enabled && AnyScreenOverlayLayerEnabled(g_screenOverlay))) {
                g_editor.FinishNumericEdit();
                g_screenOverlayWindowOpen = !g_screenOverlayWindowOpen;
                g_focusScreenOverlayWindow = g_screenOverlayWindowOpen;
            }
            if (ImGuiMCP::IsItemHovered()) { Tooltip(Tr("画面演出の設定を開閉します。画面演出が有効なとき橙色になります。")); }
            const char* languageLabel = Current() == Language::Japanese ? "EN" : "JP";
            const float languageWidth = ButtonWidth(languageLabel);
            if (ImGuiMCP::GetContentRegionAvail().x >= languageWidth &&
                ButtonWidth(powerLabel) + ButtonWidth("P") + persistentFaceWidth + photoEffectWidth + screenOverlayWidth + languageWidth +
                    ImGuiMCP::GetStyle()->ItemSpacing.x * 7 <= topWidth) { ImGuiMCP::SameLine(); }
            if (SelectableButton(languageLabel)) {
                g_editor.FinishNumericEdit();
                const auto next = Current() == Language::Japanese ? Language::English : Language::Japanese;
                if (g_languagePath.empty()) { g_languageError = "言語設定を保存できません。この起動中だけ切り替えます。"; }
                else { SavePreference(g_languagePath, next, g_languageError); }
                Set(next);
                g_libraryNotice.clear();
            }
            if (ImGuiMCP::IsItemHovered()) { Tooltip("%s", Tr("日本語／英語を切り替えます。名前・照明・登録データは変えません。")); }
            if (ImGuiMCP::GetContentRegionAvail().x >= ButtonWidth("K")) { ImGuiMCP::SameLine(); }
            if (SelectableButton("K", g_toolHotkeyWindowOpen)) {
                g_editor.FinishNumericEdit();
                g_toolHotkeyWindowOpen = !g_toolHotkeyWindowOpen;
                g_focusToolHotkeyWindow = g_toolHotkeyWindowOpen;
                if (!g_toolHotkeyWindowOpen) { RuntimeLight::CancelToolWindowHotkeyCapture(); }
            }
            if (ImGuiMCP::IsItemHovered()) {
                Tooltip("%s", Tr("Lighting単独ウィンドウを開閉するキーを設定します。Shift / Ctrl / Altとの組み合わせも登録できます。"));
            }
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
            DrawPrototypeControls(anyActive, runtime);
            DrawPositionWindow(anyActive);
            DrawPersistentFaceWindow(runtime);
            DrawToolHotkeyWindow(runtime);
            DrawPhotoEffectWindow(runtime);
            DrawScreenOverlayWindow();
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
            if (!g_screenPlacementWindow) {
                UpdateScreenPlacementInteraction(DrawScreenPlacementControls(RuntimeLight::GetView()));
            }
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
        // The framework blocks Skyrim's normal input dispatch while its menu is open.
        // Register its dedicated callback so hotkey capture can finish in-place.
        g_frameworkInputEvent = SKSEMenuFramework::AddInputEvent(CaptureFrameworkInput);
        const auto preferredRoot = PreferredStorageRoot();
        const auto logDirectory = SKSE::log::log_directory();
        const auto legacyRoot = logDirectory ? *logDirectory / "ScreenshotLightingAssistant" : std::filesystem::path{};
        const auto storage = PrepareStorageLocation(preferredRoot, legacyRoot);
        if (!storage.root.empty()) {
            g_languagePath = storage.root / "ui-language.sla";
            RuntimeLight::ConfigurePersistentStorage(storage.root / "persistent-face.sla");
            RuntimeLight::ConfigureToolWindowHotkeyStorage(storage.root / "tool-window-hotkey.sla");
            LoadPreference(g_languagePath, g_languageError);
            g_libraryNotice = storage.notice.empty() ? std::string{} : Tr(storage.notice.c_str());
            g_library = std::make_unique<PresetLibrary>(storage.root / "Presets");
            g_library->Load();
            g_libraryLoadError = g_library->Error();
            logger::info("Preset library: {} lights, {} scenes at {}{}", g_library->Lights().size(),
                g_library->Scenes().size(), storage.root.string(), storage.usingLegacy ? " (legacy fallback)" :
                    storage.usingResolvedOwner ? " (resolved storage owner)" : "");
        } else {
            g_libraryNotice = Tr("保存先が取得できないため登録を利用できません。");
        }
        g_screenOverlayHudElement = SKSEMenuFramework::AddHudElement(DrawScreenOverlayHud);
        g_screenPlacementHudElement = SKSEMenuFramework::AddHudElement(DrawScreenPlacementHud);
        g_mainWindow = SKSEMenuFramework::GetMainWindow();
        g_screenPlacementWindow = SKSEMenuFramework::AddWindow(RenderScreenPlacementWindow, true);
        g_toolWindow = SKSEMenuFramework::AddWindow(RenderToolWindow, true);
        g_toolWindowToggleHudElement = SKSEMenuFramework::AddHudElement(ProcessToolWindowToggle);
        SKSEMenuFramework::AddSectionItem("Lighting", Render);
        g_registered = true;
        logger::info("Registered Screenshot Lighting Assistant with SKSE Menu Framework");
    }
}
