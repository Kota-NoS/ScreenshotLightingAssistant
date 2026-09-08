#pragma once
#include "RuntimeModel.h"
#include "TargetModel.h"
#include "Localization.h"
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
    };
    enum class TargetAction { Refresh, Nearby, Console, Crosshair, Player };
    void RequestTarget(std::uint64_t epoch, TargetAction action, std::uint64_t revision = 0, std::size_t index = 0);
    void Initialize(); // kDataLoaded, once; no rendering-thread engine mutations.
    void SetGameReady(bool ready);
    View GetView();
    void Start(std::uint64_t epoch, const Scene& lights);
    void Stop(std::uint64_t epoch);
    void Align(std::uint64_t epoch);
    void Submit(std::uint64_t epoch, const Scene& lights);
}
