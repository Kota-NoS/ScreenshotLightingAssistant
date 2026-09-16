#pragma once

#include <filesystem>
#include <string>

namespace ScreenshotLightingAssistant
{
    struct StorageLocationResult
    {
        std::filesystem::path root;
        bool usingLegacy{};
        bool usingResolvedOwner{};
        std::size_t importedFiles{};
        std::string notice;
    };

    // Prefer the physical owner of an existing storage-v2.sla marker. Under
    // MO2 this lets a user move the complete storage folder from Overwrite into
    // a dedicated output mod once; later newly-created preset files then stay
    // beside that marker. Without a marker, keep using virtual Data so MO2 can
    // choose its normal output location. If legacy data exists, copy it once
    // without removing the original.
    StorageLocationResult PrepareStorageLocation(const std::filesystem::path& preferredRoot,
        const std::filesystem::path& legacyRoot);
}
