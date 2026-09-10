#include "StorageLocation.h"

#include <Windows.h>
#include <atomic>
#include <fstream>

namespace ScreenshotLightingAssistant
{
    namespace
    {
        constexpr std::string_view kMarker = "SLA STORAGE 2\n";

        bool ProbeWritable(const std::filesystem::path& root)
        {
            std::error_code error;
            std::filesystem::create_directories(root / "Presets", error);
            if (error) { return false; }
            static std::atomic<unsigned long> counter{};
            const auto probe = root / (L".write-probe-" + std::to_wstring(GetCurrentProcessId()) + L"-" +
                std::to_wstring(++counter) + L".tmp");
            {
                std::ofstream stream(probe, std::ios::binary | std::ios::trunc);
                stream << "probe";
                if (!stream) { return false; }
            }
            const bool removed = std::filesystem::remove(probe, error);
            return removed && !error;
        }

        bool ReadMarker(const std::filesystem::path& marker, bool& exists)
        {
            std::error_code error;
            exists = std::filesystem::exists(marker, error);
            if (error || !exists) { return !error; }
            std::ifstream stream(marker, std::ios::binary);
            const std::string bytes{std::istreambuf_iterator<char>(stream), {}};
            return !stream.bad() && bytes == kMarker;
        }

        bool WriteMarker(const std::filesystem::path& marker)
        {
            std::ofstream stream(marker, std::ios::binary | std::ios::trunc);
            stream << kMarker;
            stream.flush();
            return static_cast<bool>(stream);
        }

        bool CopyMissingFile(const std::filesystem::path& source, const std::filesystem::path& destination,
            std::size_t& copied)
        {
            std::error_code error;
            if (!std::filesystem::is_regular_file(source, error) || error || std::filesystem::is_symlink(source, error) || error) {
                return !error;
            }
            std::filesystem::create_directories(destination.parent_path(), error);
            if (error) { return false; }
            const bool didCopy = std::filesystem::copy_file(source, destination,
                std::filesystem::copy_options::skip_existing, error);
            if (error) { return false; }
            copied += didCopy ? 1U : 0U;
            return true;
        }

        bool ImportLegacy(const std::filesystem::path& legacyRoot, const std::filesystem::path& preferredRoot,
            std::size_t& copied)
        {
            if (legacyRoot.empty() || legacyRoot.lexically_normal() == preferredRoot.lexically_normal()) { return true; }
            std::error_code error;
            if (!std::filesystem::exists(legacyRoot, error)) { return !error; }
            if (!CopyMissingFile(legacyRoot / "ui-language.sla", preferredRoot / "ui-language.sla", copied)) { return false; }
            const auto persistent = legacyRoot / "persistent-face.sla";
            const bool persistentExists = std::filesystem::exists(persistent, error);
            if (error || (persistentExists && !CopyMissingFile(persistent,
                    preferredRoot / "persistent-face.sla", copied))) { return false; }
            const auto source = legacyRoot / "Presets";
            if (!std::filesystem::exists(source, error)) { return !error; }
            for (std::filesystem::recursive_directory_iterator it(source,
                     std::filesystem::directory_options::skip_permission_denied, error), end;
                 it != end; it.increment(error)) {
                if (error) { return false; }
                if (it->is_symlink(error)) {
                    if (error) { return false; }
                    if (it->is_directory(error)) { it.disable_recursion_pending(); }
                    continue;
                }
                if (error) { return false; }
                if (it->is_regular_file(error)) {
                    if (error || !CopyMissingFile(it->path(), preferredRoot / "Presets" /
                            std::filesystem::relative(it->path(), source, error), copied) || error) { return false; }
                } else if (error) { return false; }
            }
            return !error;
        }
    }

    StorageLocationResult PrepareStorageLocation(const std::filesystem::path& preferredRoot,
        const std::filesystem::path& legacyRoot)
    {
        StorageLocationResult result{preferredRoot};
        if (preferredRoot.empty() || !ProbeWritable(preferredRoot)) {
            result.root = legacyRoot;
            result.usingLegacy = true;
            result.notice = "新しい保存先を利用できないため、従来のMy Games内へ保存します。";
            return result;
        }

        const auto marker = preferredRoot / "storage-v2.sla";
        bool markerExists{};
        if (!ReadMarker(marker, markerExists)) {
            result.notice = "保存先の移行情報を読み込めません。元のファイルは保持しています。";
            return result;
        }
        if (markerExists) { return result; }

        if (!ImportLegacy(legacyRoot, preferredRoot, result.importedFiles) || !WriteMarker(marker)) {
            result.root = legacyRoot;
            result.usingLegacy = true;
            result.notice = "従来の保存先から新しい保存先へコピーできないため、この起動では従来の保存先を使います。";
            return result;
        }
        if (result.importedFiles) {
            result.notice = "従来の保存先から登録データをコピーしました。旧ファイルはバックアップとして残しています。";
        }
        return result;
    }
}
