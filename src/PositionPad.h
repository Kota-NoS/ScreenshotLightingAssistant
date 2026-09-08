#pragma once
#include "LightingState.h"

namespace ScreenshotLightingAssistant
{
    // One isotropic scale for BOTH input and drawing, even on a rectangular pad.
    inline float PadPixelsPerMetre(float width, float height)
    {
        if (!std::isfinite(width) || !std::isfinite(height)) { return 1.0F; }
        return std::max(1.0F, (std::min(width, height) - 32.0F) / (2.0F * kFinePositionLimit));
    }

    inline FinePosition MoveOnPad(FinePosition value, float dx, float dy, float wheel, float pixelsPerMetre)
    {
        if (!std::isfinite(dx) || !std::isfinite(dy) || !std::isfinite(wheel) ||
            !std::isfinite(pixelsPerMetre) || pixelsPerMetre <= 0.0F) { return value; }
        // Double intermediates avoid finite-input overflow; no screen aspect-ratio scaling.
        const auto bounded = [](double offset) {
            return static_cast<float>(std::clamp(offset, -double(kFinePositionLimit), double(kFinePositionLimit)));
        };
        value.horizontal = bounded(double(value.horizontal) + double(dx) / pixelsPerMetre);
        value.vertical = bounded(double(value.vertical) - double(dy) / pixelsPerMetre);
        // Wheel up pushes away from the camera; positive depth brings the light closer.
        value.depth = bounded(double(value.depth) - double(wheel) * 0.05);
        return value;
    }
}
