#pragma once
#include "RuntimeModel.h"
#include "TargetModel.h"
#include "Localization.h"
#include "PersistentFaceSettings.h"
#include <filesystem>
#include <string>

namespace ScreenshotLightingAssistant::RuntimeLight
{
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
    };
    enum class TargetAction { Refresh, Nearby, Console, Crosshair, Player };
    void RequestTarget(std::uint64_t epoch, TargetAction action, std::uint64_t revision = 0, std::size_t index = 0);
    void Initialize(); // kDataLoaded, once; no rendering-thread engine mutations.
    void ConfigurePersistentStorage(const std::filesystem::path& path);
    void SetGameReady(bool ready);
    View GetView();
    void Start(std::uint64_t epoch, const Scene& lights);
    void Stop(std::uint64_t epoch);
    void Align(std::uint64_t epoch);
    void Submit(std::uint64_t epoch, const Scene& lights);
    void SetPersistentFaceEnabled(bool enabled);
    void SetPersistentFaceSettings(const PersistentFaceSettings& settings);
    void BeginPersistentHotkeyCapture();
    void BeginPersistentGamepadCapture();
    void CancelPersistentHotkeyCapture();
    void ClearPersistentHotkey();
    void ClearPersistentGamepadHotkey();
    // Used by both the normal game input sink and SKSE Menu Framework's
    // blocking-menu input callback. Returns true when the key was consumed.
    bool CapturePersistentHotkey(std::uint32_t code);
    bool CapturePersistentGamepadHotkey(std::uint32_t code);
}
