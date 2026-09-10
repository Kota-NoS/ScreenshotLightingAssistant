#include "PCH.h"

#include "UI.h"
#include "RuntimeLight.h"

namespace
{
    void OnSKSEMessage(SKSE::MessagingInterface::Message* a_message)
    {
        if (!a_message) { return; }
        switch (a_message->type) {
        case SKSE::MessagingInterface::kPostLoad:
            ScreenshotLightingAssistant::UI::Register();
            break;
        case SKSE::MessagingInterface::kDataLoaded:
            ScreenshotLightingAssistant::RuntimeLight::Initialize();
            break;
        case SKSE::MessagingInterface::kPreLoadGame:
            ScreenshotLightingAssistant::RuntimeLight::SetGameReady(false);
            break;
        case SKSE::MessagingInterface::kPostLoadGame:
            // SKSE encodes the success bool in the pointer value, not its pointee.
            ScreenshotLightingAssistant::RuntimeLight::SetGameReady(a_message->data != nullptr);
            break;
        case SKSE::MessagingInterface::kNewGame:
            ScreenshotLightingAssistant::RuntimeLight::SetGameReady(true);
            break;
        default:
            break;
        }
    }
}

SKSEPluginLoad(const SKSE::LoadInterface* a_skse)
{
    SKSE::Init(a_skse);
    logger::info("Screenshot Lighting Assistant 0.2.0 loading");

    if (!SKSE::GetTaskInterface()) {
        logger::critical("Game task interface unavailable; refusing to enable runtime lighting");
        return false;
    }

    const auto messaging = SKSE::GetMessagingInterface();
    if (!messaging || !messaging->RegisterListener(OnSKSEMessage)) {
        logger::critical("Failed to register the SKSE messaging listener");
        return false;
    }

    return true;
}
