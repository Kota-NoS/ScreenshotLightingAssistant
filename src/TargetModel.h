#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

namespace ScreenshotLightingAssistant
{
    inline constexpr float kTargetSearchMetres = 30.0F;
    inline constexpr std::size_t kTargetCandidateLimit = 64;
    struct TargetCandidate
    {
        std::uint32_t formID{};
        std::string name;
        float metres{};
    };
    // Pure display filtering. Engine handles stay in Runtime, not in UI/presets.
    inline void SortTargetCandidates(std::vector<TargetCandidate>& entries)
    {
        std::erase_if(entries, [](const auto& e) {
            return !e.formID || !std::isfinite(e.metres) || e.metres < 0 || e.metres > kTargetSearchMetres;
        });
        std::sort(entries.begin(), entries.end(), [](const auto& a, const auto& b) {
            return a.metres != b.metres ? a.metres < b.metres : a.formID < b.formID;
        });
        if (entries.size() > kTargetCandidateLimit) { entries.resize(kTargetCandidateLimit); }
    }
    inline bool ValidTargetChoice(std::uint64_t requestedRevision, std::uint64_t currentRevision,
        std::size_t index, std::size_t size)
    {
        return requestedRevision == currentRevision && index < size;
    }
}
