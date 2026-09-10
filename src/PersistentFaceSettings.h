#pragma once

#include "LightingState.h"
#include <filesystem>
#include <string>

namespace ScreenshotLightingAssistant
{
    inline constexpr std::uint32_t kNoPersistentFaceHotkey = 0;
    inline constexpr std::uint32_t kMaximumKeyboardScanCode = 0xED;
    inline constexpr float kPersistentGamepadHoldSeconds = 0.60F;

    constexpr bool ValidPersistentGamepadHotkey(std::uint32_t code)
    {
        switch (code) {
        case 0:      // unassigned
        case 0x0001: // D-pad up
        case 0x0002: // D-pad down
        case 0x0004: // D-pad left
        case 0x0008: // D-pad right
        case 0x0010: // Start
        case 0x0020: // Back
        case 0x0040: // L3
        case 0x0080: // R3
        case 0x0100: // LB
        case 0x0200: // RB
        case 0x0009: // LT
        case 0x000A: // RT
        case 0x1000: // A / Cross
        case 0x2000: // B / Circle
        case 0x4000: // X / Square
        case 0x8000: // Y / Triangle
            return true;
        default:
            return false;
        }
    }

    struct PersistentGamepadHoldGate
    {
        bool triggered{};

        bool Update(bool pressed, float heldSeconds) noexcept
        {
            if (!pressed || !std::isfinite(heldSeconds) || heldSeconds <= 0.0F) {
                triggered = false;
                return false;
            }
            if (heldSeconds < kPersistentGamepadHoldSeconds || triggered) { return false; }
            triggered = true;
            return true;
        }

        void Reset() noexcept { triggered = false; }
    };

    struct PersistentFaceSettings
    {
        bool featureEnabled{true};
        bool enabled{};
        std::uint32_t hotkey{kNoPersistentFaceHotkey};
        std::uint32_t gamepadHotkey{kNoPersistentFaceHotkey};
        float intensity{0.35F};
        float range{kFaceRangeDefault};
        float heightOffset{kFaceHeightDefault};
        FaceLightBasis basis{FaceLightBasis::Head};
        bool operator==(const PersistentFaceSettings&) const = default;
    };

    bool ValidPersistentFaceSettings(const PersistentFaceSettings& settings);
    std::string EncodePersistentFaceSettings(const PersistentFaceSettings& settings);
    bool DecodePersistentFaceSettings(const std::string& contents, PersistentFaceSettings& settings);

    // Missing files use defaults. Invalid files are preserved and never overwritten.
    bool LoadPersistentFaceSettings(const std::filesystem::path& path,
        PersistentFaceSettings& settings, std::string& error);
    bool SavePersistentFaceSettings(const std::filesystem::path& path,
        const PersistentFaceSettings& settings, std::string& error);
}
