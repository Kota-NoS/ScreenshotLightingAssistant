#include "PersistentFaceSettings.h"

#include <Windows.h>
#include <charconv>
#include <fstream>
#include <iomanip>
#include <locale>
#include <sstream>

namespace ScreenshotLightingAssistant
{
    namespace
    {
        constexpr std::string_view kHeaderV1 = "SLA PERSISTENT FACE 1";
        constexpr std::string_view kHeaderV2 = "SLA PERSISTENT FACE 2";
        constexpr std::string_view kHeaderV3 = "SLA PERSISTENT FACE 3";

        bool ParseUnsigned(std::string_view value, std::uint32_t& result)
        {
            const auto parsed = std::from_chars(value.data(), value.data() + value.size(), result);
            return parsed.ec == std::errc{} && parsed.ptr == value.data() + value.size();
        }

        bool ParseFloat(std::string_view value, float& result)
        {
            const auto parsed = std::from_chars(value.data(), value.data() + value.size(), result,
                std::chars_format::general);
            return parsed.ec == std::errc{} && parsed.ptr == value.data() + value.size() && std::isfinite(result);
        }

        bool ReadExisting(const std::filesystem::path& path, PersistentFaceSettings& result)
        {
            std::error_code error;
            if (std::filesystem::file_size(path, error) > 1024 || error) { return false; }
            std::ifstream stream(path, std::ios::binary);
            const std::string contents{std::istreambuf_iterator<char>(stream), {}};
            return !stream.bad() && DecodePersistentFaceSettings(contents, result);
        }
    }

    bool ValidPersistentFaceSettings(const PersistentFaceSettings& settings)
    {
        return settings.hotkey <= kMaximumKeyboardScanCode &&
            ValidPersistentGamepadHotkey(settings.gamepadHotkey) &&
            std::isfinite(settings.intensity) && settings.intensity >= 0.0F && settings.intensity <= kFaceIntensityMax &&
            std::isfinite(settings.range) && settings.range >= kFaceRangeMin && settings.range <= kFaceRangeMax &&
            std::isfinite(settings.heightOffset) && settings.heightOffset >= kFaceHeightMin &&
            settings.heightOffset <= kFaceHeightMax && settings.basis == FaceLightBasis::Head;
    }

    std::string EncodePersistentFaceSettings(const PersistentFaceSettings& settings)
    {
        if (!ValidPersistentFaceSettings(settings)) { return {}; }
        std::ostringstream stream;
        stream.imbue(std::locale::classic());
        stream << kHeaderV3 << '\n'
               << "feature " << (settings.featureEnabled ? 1 : 0) << '\n'
               << "enabled " << (settings.enabled ? 1 : 0) << '\n'
               << "hotkey " << settings.hotkey << '\n'
               << "gamepad " << settings.gamepadHotkey << '\n'
               << std::fixed << std::setprecision(6)
               << "intensity " << settings.intensity << '\n'
               << "range " << settings.range << '\n'
               << "height " << settings.heightOffset << '\n'
               << "basis head\n";
        return stream.str();
    }

    bool DecodePersistentFaceSettings(const std::string& contents, PersistentFaceSettings& settings)
    {
        std::istringstream stream(contents);
        stream.imbue(std::locale::classic());
        std::string line;
        if (!std::getline(stream, line)) { return false; }
        const bool version1 = line == kHeaderV1;
        const bool version2 = line == kHeaderV2;
        const bool version3 = line == kHeaderV3;
        if (!version1 && !version2 && !version3) { return false; }

        PersistentFaceSettings parsed;
        std::array<bool, 8> seen{};
        while (std::getline(stream, line)) {
            if (line.empty()) { continue; }
            const auto separator = line.find(' ');
            if (separator == std::string::npos || separator == 0 || separator + 1 >= line.size()) { return false; }
            const std::string_view key{line.data(), separator};
            const std::string_view value{line.data() + separator + 1, line.size() - separator - 1};
            std::size_t field{};
            if (key == "feature") {
                field = 7;
                if (!version3) { return false; }
                if (value == "0") { parsed.featureEnabled = false; }
                else if (value == "1") { parsed.featureEnabled = true; }
                else { return false; }
            } else if (key == "enabled") {
                field = 0;
                if (value == "0") { parsed.enabled = false; }
                else if (value == "1") { parsed.enabled = true; }
                else { return false; }
            } else if (key == "hotkey") {
                field = 1;
                if (!ParseUnsigned(value, parsed.hotkey)) { return false; }
            } else if (key == "gamepad") {
                field = 6;
                if (version1 || !ParseUnsigned(value, parsed.gamepadHotkey)) { return false; }
            } else if (key == "intensity") {
                field = 2;
                if (!ParseFloat(value, parsed.intensity)) { return false; }
            } else if (key == "range") {
                field = 3;
                if (!ParseFloat(value, parsed.range)) { return false; }
            } else if (key == "height") {
                field = 4;
                if (!ParseFloat(value, parsed.heightOffset)) { return false; }
            } else if (key == "basis") {
                field = 5;
                if (value == "head") { parsed.basis = FaceLightBasis::Head; }
                else if (value == "camera") { parsed.basis = FaceLightBasis::Camera; }
                else { return false; }
            } else { return false; }
            if (seen[field]) { return false; }
            seen[field] = true;
        }
        const std::size_t requiredFields = version1 ? 6 : version2 ? 7 : 8;
        const auto requiredEnd = seen.begin() + requiredFields;
        // The persistent light is intentionally head-facing from beta4 onward.
        // Older camera-facing profiles remain readable and migrate in memory.
        parsed.basis = FaceLightBasis::Head;
        if (!std::all_of(seen.begin(), requiredEnd, [](bool value) { return value; }) ||
            !ValidPersistentFaceSettings(parsed)) { return false; }
        settings = parsed;
        return true;
    }

    bool LoadPersistentFaceSettings(const std::filesystem::path& path,
        PersistentFaceSettings& settings, std::string& error)
    {
        error.clear();
        std::error_code ec;
        if (!std::filesystem::exists(path, ec) && !ec) {
            settings = {};
            return true;
        }
        PersistentFaceSettings loaded;
        if (!ec && ReadExisting(path, loaded)) {
            settings = loaded;
            return true;
        }
        error = "常用フェイスライト設定を読み込めません。元のファイルは保持しています。";
        return false;
    }

    bool SavePersistentFaceSettings(const std::filesystem::path& path,
        const PersistentFaceSettings& settings, std::string& error)
    {
        error = "常用フェイスライト設定を保存できません。この起動中だけ変更します。";
        if (path.empty() || !ValidPersistentFaceSettings(settings)) { return false; }
        std::error_code ec;
        const bool exists = std::filesystem::exists(path, ec);
        PersistentFaceSettings previous;
        if (ec || (exists && !ReadExisting(path, previous))) { return false; }
        std::filesystem::create_directories(path.parent_path(), ec);
        if (ec) { return false; }

        static std::atomic<unsigned long> counter{};
        auto temporary = path;
        temporary += L"." + std::to_wstring(GetCurrentProcessId()) + L"." +
            std::to_wstring(++counter) + L".tmp";
        std::ofstream stream(temporary, std::ios::binary | std::ios::trunc);
        stream << EncodePersistentFaceSettings(settings);
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
