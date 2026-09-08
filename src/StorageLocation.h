#pragma once

#include <filesystem>
#include <string>

namespace ScreenshotLightingAssistant
{
    struct StorageLocationResult
    {
        std::filesystem::path root;
        bool usingLegacy{};
        std::size_t importedFiles{};
        std::string notice;
    };

    // Prefer a virtual Data path so MO2 sends mutable files to Overwrite. On the
    // first successful use, copy the previous SKSE-log storage without removing it.
    // If the preferred path cannot be written, keep using the legacy directory.
    StorageLocationResult PrepareStorageLocation(const std::filesystem::path& preferredRoot,
        const std::filesystem::path& legacyRoot);
}
