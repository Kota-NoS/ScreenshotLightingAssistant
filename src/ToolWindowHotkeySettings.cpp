#include "ToolWindowHotkeySettings.h"
#include "PersistentFaceSettings.h"

#include <Windows.h>
#include <array>
#include <atomic>
#include <charconv>
#include <fstream>
#include <sstream>

namespace ScreenshotLightingAssistant
{
    namespace
    {
        constexpr std::string_view kHeader = "SLA TOOL WINDOW HOTKEY 1";
        constexpr std::uint32_t kLeftShift = 0x2A;
        constexpr std::uint32_t kRightShift = 0x36;
        constexpr std::uint32_t kLeftControl = 0x1D;
        constexpr std::uint32_t kRightControl = 0x9D;
        constexpr std::uint32_t kLeftAlt = 0x38;
        constexpr std::uint32_t kRightAlt = 0xB8;

        bool ParseUnsigned(std::string_view value, std::uint32_t& result)
        {
            const auto parsed = std::from_chars(value.data(), value.data() + value.size(), result);
            return parsed.ec == std::errc{} && parsed.ptr == value.data() + value.size();
        }

        bool ReadExisting(const std::filesystem::path& path, ToolWindowHotkeySettings& result)
        {
            std::error_code error;
            if (std::filesystem::file_size(path, error) > 256 || error) { return false; }
            std::ifstream stream(path, std::ios::binary);
            const std::string contents{std::istreambuf_iterator<char>(stream), {}};
            return !stream.bad() && DecodeToolWindowHotkeySettings(contents, result);
        }
    }

    bool ToolWindowModifierState::Update(std::uint32_t scanCode, bool pressed) noexcept
    {
        bool* target{};
        switch (scanCode) {
        case kLeftShift: target = &leftShift; break;
        case kRightShift: target = &rightShift; break;
        case kLeftControl: target = &leftControl; break;
        case kRightControl: target = &rightControl; break;
        case kLeftAlt: target = &leftAlt; break;
        case kRightAlt: target = &rightAlt; break;
        default: return false;
        }
        *target = pressed;
        return true;
    }

    std::uint8_t ToolWindowModifierState::Mask() const noexcept
    {
        std::uint8_t result{};
        if (leftShift || rightShift) { result |= kToolWindowModifierShift; }
        if (leftControl || rightControl) { result |= kToolWindowModifierControl; }
        if (leftAlt || rightAlt) { result |= kToolWindowModifierAlt; }
        return result;
    }

    bool IsToolWindowModifierKey(std::uint32_t scanCode) noexcept
    {
        ToolWindowModifierState state;
        return state.Update(scanCode, true);
    }

    bool ValidToolWindowHotkeySettings(const ToolWindowHotkeySettings& settings) noexcept
    {
        return settings.key <= kMaximumKeyboardScanCode &&
            !IsToolWindowModifierKey(settings.key) &&
            (settings.key != kNoToolWindowHotkey || settings.modifiers == 0) &&
            (settings.modifiers & ~kToolWindowModifierMask) == 0;
    }

    bool ToolWindowHotkeyMatches(const ToolWindowHotkeySettings& settings,
        std::uint32_t scanCode, std::uint8_t modifiers, bool initialDown) noexcept
    {
        return initialDown && settings.key != kNoToolWindowHotkey &&
            scanCode == settings.key && modifiers == settings.modifiers &&
            ValidToolWindowHotkeySettings(settings);
    }

    std::uint32_t ToolWindowVirtualKeyFromScanCode(std::uint32_t scanCode) noexcept
    {
        if (scanCode > kMaximumKeyboardScanCode) { return 0; }
        // DirectInput stores extended set-1 scan codes by setting bit 7. Win32
        // expects E0 in the high byte for MAPVK_VSC_TO_VK_EX instead.
        if (scanCode == 0xC5U) { return VK_PAUSE; }
        const auto win32ScanCode = (scanCode & 0x7FU) |
            ((scanCode & 0x80U) != 0 ? 0xE000U : 0U);
        return MapVirtualKeyW(win32ScanCode, MAPVK_VSC_TO_VK_EX);
    }

    std::string EncodeToolWindowHotkeySettings(const ToolWindowHotkeySettings& settings)
    {
        if (!ValidToolWindowHotkeySettings(settings)) { return {}; }
        std::ostringstream stream;
        stream << kHeader << '\n'
               << "key " << settings.key << '\n'
               << "modifiers " << static_cast<unsigned>(settings.modifiers) << '\n';
        return stream.str();
    }

    bool DecodeToolWindowHotkeySettings(const std::string& contents, ToolWindowHotkeySettings& settings)
    {
        std::istringstream stream(contents);
        std::string line;
        if (!std::getline(stream, line) || line != kHeader) { return false; }
        ToolWindowHotkeySettings parsed;
        std::array<bool, 2> seen{};
        while (std::getline(stream, line)) {
            if (line.empty()) { continue; }
            const auto separator = line.find(' ');
            if (separator == std::string::npos || separator == 0 || separator + 1 >= line.size()) { return false; }
            const std::string_view key{line.data(), separator};
            const std::string_view value{line.data() + separator + 1, line.size() - separator - 1};
            std::uint32_t number{};
            if (!ParseUnsigned(value, number)) { return false; }
            std::size_t field{};
            if (key == "key") {
                field = 0;
                parsed.key = number;
            } else if (key == "modifiers") {
                field = 1;
                if (number > 0xFFU) { return false; }
                parsed.modifiers = static_cast<std::uint8_t>(number);
            } else {
                return false;
            }
            if (seen[field]) { return false; }
            seen[field] = true;
        }
        if (!seen[0] || !seen[1] || !ValidToolWindowHotkeySettings(parsed)) { return false; }
        settings = parsed;
        return true;
    }

    bool LoadToolWindowHotkeySettings(const std::filesystem::path& path,
        ToolWindowHotkeySettings& settings, std::string& error)
    {
        error.clear();
        std::error_code ec;
        if (!std::filesystem::exists(path, ec) && !ec) {
            settings = {};
            return true;
        }
        ToolWindowHotkeySettings loaded;
        if (!ec && ReadExisting(path, loaded)) {
            settings = loaded;
            return true;
        }
        error = "ツール表示キー設定を読み込めません。元のファイルは保持しています。";
        return false;
    }

    bool SaveToolWindowHotkeySettings(const std::filesystem::path& path,
        const ToolWindowHotkeySettings& settings, std::string& error)
    {
        error = "ツール表示キー設定を保存できません。この起動中だけ変更します。";
        if (path.empty() || !ValidToolWindowHotkeySettings(settings)) { return false; }
        std::error_code ec;
        const bool exists = std::filesystem::exists(path, ec);
        ToolWindowHotkeySettings previous;
        if (ec || (exists && !ReadExisting(path, previous))) { return false; }
        std::filesystem::create_directories(path.parent_path(), ec);
        if (ec) { return false; }

        static std::atomic<unsigned long> counter{};
        auto temporary = path;
        temporary += L"." + std::to_wstring(GetCurrentProcessId()) + L"." +
            std::to_wstring(++counter) + L".tmp";
        std::ofstream stream(temporary, std::ios::binary | std::ios::trunc);
        stream << EncodeToolWindowHotkeySettings(settings);
        stream.flush();
        const bool complete = static_cast<bool>(stream);
        stream.close();
        if (!complete || !MoveFileExW(temporary.c_str(), path.c_str(),
                MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
            std::filesystem::remove(temporary, ec);
            return false;
        }
        error.clear();
        return true;
    }
}
