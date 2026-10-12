#pragma once
#include "RuntimeModel.h"
#include "TargetModel.h"
#include "Localization.h"
#include "PersistentFaceSettings.h"
#include "PhotoEffectSettings.h"
#include "ToolWindowHotkeySettings.h"
#include <filesystem>
#include <string>

namespace ScreenshotLightingAssistant::RuntimeLight
{
    struct ScreenPlacementView
    {
        bool enabled{};
        bool positioning{};
        bool active{};
        bool waiting{};
        bool settingsRemembered{};
        bool positionRemembered{};
        float normalizedX{0.5F};
        float normalizedY{0.5F};
        float x{};
        float y{};
        float width{1.0F};
        float height{1.0F};
        LightSettings light{};
    };

    struct View
    {
        LightSessionSnapshot session;
        Localization::Message status;
        bool statusImportant{}; // Normal status is tooltip-only; warnings stay visible.
        bool npcTarget{};
        std::string targetName;
        std::uint32_t targetFormID{};
        std::uint64_t targetRevision{};
        std::vector<TargetCandidate> candidates;
        bool targetBusy{};
        std::string targetNotice;
        PersistentFaceSettings persistentFace;
        bool persistentFaceActive{};
        bool persistentFaceWaiting{};
        bool persistentFaceUsesPhotoLight{};
        bool persistentHotkeyCapture{};
        std::string persistentHotkeyName;
        bool persistentGamepadCapture{};
        std::string persistentGamepadHotkeyName;
        std::string persistentFaceError;
        ToolWindowHotkeySettings toolWindowHotkey;
        bool toolWindowHotkeyCapture{};
        std::string toolWindowHotkeyName;
        std::string toolWindowHotkeyError;
        PhotoEffectSettings photoEffect;
        std::size_t activePhotoEffects{};
        ScreenPlacementView screenPlacement;
    };
    enum class TargetAction { Refresh, Nearby, Console, Crosshair, Player };
    void RequestTarget(std::uint64_t epoch, TargetAction action, std::uint64_t revision = 0, std::size_t index = 0);
    void Initialize(); // kDataLoaded, once; no rendering-thread engine mutations.
    void ConfigurePersistentStorage(const std::filesystem::path& path);
    void ConfigureToolWindowHotkeyStorage(const std::filesystem::path& path);
    void SetGameReady(bool ready);
    View GetView();
    void Start(std::uint64_t epoch, const Scene& lights);
    void Stop(std::uint64_t epoch);
    void Align(std::uint64_t epoch);
    void Submit(std::uint64_t epoch, const Scene& lights);
    void SetPersistentFaceEnabled(bool enabled);
    void SetPersistentFaceSettings(const PersistentFaceSettings& settings);
    void SetPhotoEffectSettings(const PhotoEffectSettings& settings);
    bool BeginScreenPlacement(const LightSettings& settings);
    void UpdateScreenPlacement(float x, float y, float width, float height);
    void SyncScreenPlacementViewport(float width, float height);
    void AdjustScreenPlacementDistance(float wheel, bool fast);
    void SetScreenPlacementSettings(const LightSettings& settings);
    bool SetScreenPlacementEnabled(bool enabled);
    void RemoveScreenPlacement();
    void ConfirmScreenPlacement();
    void DisableScreenPlacement();
    std::optional<ScreenPlacementSettings> GetScreenPlacementPreset();
    void ApplyScreenPlacementPreset(const std::optional<ScreenPlacementSettings>& settings);
    void BeginPersistentHotkeyCapture();
    void BeginPersistentGamepadCapture();
    void CancelPersistentHotkeyCapture();
    void ClearPersistentHotkey();
    void ClearPersistentGamepadHotkey();
    // Used by both the normal game input sink and SKSE Menu Framework's
    // blocking-menu input callback. Returns true when the key was consumed.
    bool CapturePersistentHotkey(std::uint32_t code);
    bool CapturePersistentGamepadHotkey(std::uint32_t code);
    void BeginToolWindowHotkeyCapture();
    void CancelToolWindowHotkeyCapture();
    void ClearToolWindowHotkey();
    // Updates modifier state for both press and release events. Returns true when
    // capture or a configured open/close hotkey consumed the keyboard event.
    bool HandleToolWindowKeyboardEvent(std::uint32_t code, bool pressed, bool initialDown);
    // SKSE Menu Framework skips addon callbacks while another ImGui control is
    // active. Photo Mode is such a full-screen ImGui UI, so poll only while its
    // official active-state export is true and preserve the normal path elsewhere.
    void PollPhotoModeToolWindowHotkey();
    bool ConsumeToolWindowToggleRequest();
}
