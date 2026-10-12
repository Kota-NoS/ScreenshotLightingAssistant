#pragma once

#include <cstdint>
#include <filesystem>
#include <string>

namespace ScreenshotLightingAssistant
{
    inline constexpr std::uint32_t kNoToolWindowHotkey = 0;
    inline constexpr std::uint8_t kToolWindowModifierShift = 1U << 0U;
    inline constexpr std::uint8_t kToolWindowModifierControl = 1U << 1U;
    inline constexpr std::uint8_t kToolWindowModifierAlt = 1U << 2U;
    inline constexpr std::uint8_t kToolWindowModifierMask =
        kToolWindowModifierShift | kToolWindowModifierControl | kToolWindowModifierAlt;

    struct ToolWindowHotkeySettings
    {
        std::uint32_t key{kNoToolWindowHotkey};
        std::uint8_t modifiers{};
        bool operator==(const ToolWindowHotkeySettings&) const = default;
    };

    struct ToolWindowModifierState
    {
        bool leftShift{};
        bool rightShift{};
        bool leftControl{};
        bool rightControl{};
        bool leftAlt{};
        bool rightAlt{};

        bool Update(std::uint32_t scanCode, bool pressed) noexcept;
        [[nodiscard]] std::uint8_t Mask() const noexcept;
        void Reset() noexcept { *this = {}; }
    };

    [[nodiscard]] bool IsToolWindowModifierKey(std::uint32_t scanCode) noexcept;
    [[nodiscard]] bool ValidToolWindowHotkeySettings(const ToolWindowHotkeySettings& settings) noexcept;
    [[nodiscard]] bool ToolWindowHotkeyMatches(const ToolWindowHotkeySettings& settings,
        std::uint32_t scanCode, std::uint8_t modifiers, bool initialDown) noexcept;
    // Converts Skyrim/DirectInput scan codes to the Win32 virtual-key values
    // used by the Photo Mode fallback poll. Returns zero when unmappable.
    [[nodiscard]] std::uint32_t ToolWindowVirtualKeyFromScanCode(std::uint32_t scanCode) noexcept;
    std::string EncodeToolWindowHotkeySettings(const ToolWindowHotkeySettings& settings);
    bool DecodeToolWindowHotkeySettings(const std::string& contents, ToolWindowHotkeySettings& settings);

    // Missing files use defaults. Invalid files are preserved and never overwritten.
    bool LoadToolWindowHotkeySettings(const std::filesystem::path& path,
        ToolWindowHotkeySettings& settings, std::string& error);
    bool SaveToolWindowHotkeySettings(const std::filesystem::path& path,
        const ToolWindowHotkeySettings& settings, std::string& error);
}
