#include "StorageLocation.h"

#include <Windows.h>
#include <array>
#include <fstream>
#include <optional>

namespace ScreenshotLightingAssistant
{
    namespace
    {
        constexpr std::string_view kMarker = "SLA STORAGE 2\n";

        bool WritableRootWithoutCreatingIt(const std::filesystem::path& root)
        {
            if (root.empty()) { return false; }
            std::error_code error;
            auto existing = root;
            while (!std::filesystem::exists(existing, error)) {
                if (error) { return false; }
                const auto parent = existing.parent_path();
                if (parent.empty() || parent == existing) { return false; }
                existing = parent;
            }
            if (error || !std::filesystem::is_directory(existing, error) || error) { return false; }
            const HANDLE directory = CreateFileW(existing.c_str(), FILE_ADD_FILE | FILE_ADD_SUBDIRECTORY,
                FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING,
                FILE_FLAG_BACKUP_SEMANTICS, nullptr);
            if (directory == reinterpret_cast<HANDLE>(static_cast<std::intptr_t>(-1))) { return false; }
            CloseHandle(directory);
            return true;
        }

        bool HasLegacyContent(const std::filesystem::path& root)
        {
            if (root.empty()) { return false; }
            std::error_code error;
            for (const auto* name : {L"ui-language.sla", L"persistent-face.sla"}) {
                if (std::filesystem::is_regular_file(root / name, error) && !error) { return true; }
                if (error) { return true; }
            }
            const auto presets = root / "Presets";
            if (!std::filesystem::is_directory(presets, error)) { return static_cast<bool>(error); }
            for (std::filesystem::recursive_directory_iterator it(presets,
                     std::filesystem::directory_options::skip_permission_denied, error), end;
                 it != end; it.increment(error)) {
                if (error) { return true; }
                if (it->is_symlink(error)) {
                    if (error) { return true; }
                    if (it->is_directory(error)) { it.disable_recursion_pending(); }
                    continue;
                }
                if (error || it->is_regular_file(error)) { return true; }
                if (error) { return true; }
            }
            return static_cast<bool>(error);
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

        std::optional<std::filesystem::path> ResolveMarkerOwner(const std::filesystem::path& marker)
        {
            // USVFS redirects CreateFileW to the file supplied by the winning
            // mod. The resulting kernel handle still identifies that physical
            // file, so GetFinalPathNameByHandleW reveals the folder which owns
            // the explicit storage marker. New files can then be kept beside it
            // instead of being split back into Overwrite.
            const HANDLE file = CreateFileW(marker.c_str(), GENERIC_READ,
                FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr,
                OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
            if (file == reinterpret_cast<HANDLE>(static_cast<std::intptr_t>(-1))) { return std::nullopt; }

            std::array<wchar_t, 32768> buffer{};
            const DWORD length = GetFinalPathNameByHandleW(file, buffer.data(),
                static_cast<DWORD>(buffer.size()), FILE_NAME_NORMALIZED | VOLUME_NAME_DOS);
            CloseHandle(file);
            if (!length || length >= buffer.size()) { return std::nullopt; }

            std::wstring resolved(buffer.data(), length);
            constexpr std::wstring_view uncPrefix = L"\\\\?\\UNC\\";
            constexpr std::wstring_view longPrefix = L"\\\\?\\";
            if (resolved.starts_with(uncPrefix)) {
                resolved = L"\\\\" + resolved.substr(uncPrefix.size());
            } else if (resolved.starts_with(longPrefix)) {
                resolved.erase(0, longPrefix.size());
            }

            const std::filesystem::path physicalMarker{resolved};
            if (physicalMarker.filename() != marker.filename()) { return std::nullopt; }
            const auto owner = physicalMarker.parent_path();
            return owner.empty() ? std::nullopt : std::optional<std::filesystem::path>{owner};
        }

        void UseResolvedMarkerOwner(const std::filesystem::path& marker, StorageLocationResult& result)
        {
            const auto owner = ResolveMarkerOwner(marker);
            if (!owner || !WritableRootWithoutCreatingIt(*owner)) { return; }
            result.root = *owner;
            result.usingResolvedOwner = result.root.lexically_normal() != marker.parent_path().lexically_normal();
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
        if (!WritableRootWithoutCreatingIt(preferredRoot)) {
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
        if (markerExists) {
            UseResolvedMarkerOwner(marker, result);
            return result;
        }

        // A read-only startup must not create MO2 Overwrite directories. The
        // preset, language and persistent-setting writers create parents only
        // when the user actually changes something. Migration is the sole
        // startup case that writes files and its one-time marker.
        if (!HasLegacyContent(legacyRoot)) { return result; }

        if (!ImportLegacy(legacyRoot, preferredRoot, result.importedFiles) || !WriteMarker(marker)) {
            result.root = legacyRoot;
            result.usingLegacy = true;
            result.notice = "従来の保存先から新しい保存先へコピーできないため、この起動では従来の保存先を使います。";
            return result;
        }
        if (result.importedFiles) {
            result.notice = "従来の保存先から登録データをコピーしました。旧ファイルはバックアップとして残しています。";
        }
        UseResolvedMarkerOwner(marker, result);
        return result;
    }
}
