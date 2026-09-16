#pragma once

#include "PhotoEffectSettings.h"
#include "RuntimeModel.h"

#include <algorithm>
#include <cmath>

namespace ScreenshotLightingAssistant
{
    struct PhotoEffectMotionSample
    {
        LightVector gravityAxis{0.0F, 0.0F, 1.0F};
        float gravityStrength{};
        float turbulence{};
        float turbulenceScale{};
    };

    inline PhotoEffectMotionSample SamplePhotoEffectMotion(const PhotoEffectSettings& settings,
        const CameraBasis& basis, float /*seconds*/)
    {
        // Vertical and horizontal controls are signed movement forces. Skyrim's
        // World-space NiPSysGravityModifier accelerates opposite its stored axis,
        // so negate the requested movement to keep the UI semantics (+up/+right).
        // Target-local orbit meshes use the direct local axis instead; treating
        // both variants alike reverses the vertical control only while orbiting.
        // A zero vector really means hover instead of retaining hidden pull.
        // Floatiness softens acceleration without changing the chosen direction.
        const float vertical = settings.verticalMovement;
        const float horizontal = settings.horizontalFlow;
        LightVector vector{
            basis.right.x * horizontal,
            basis.right.y * horizontal,
            vertical
        };
        const float magnitude = std::hypot(vector.x, vector.y, vector.z);
        PhotoEffectMotionSample result;
        if (std::isfinite(magnitude) && magnitude >= 0.001F) {
            const float axisSign = settings.orbit ? 1.0F : -1.0F;
            result.gravityAxis = {axisSign * vector.x / magnitude,
                axisSign * vector.y / magnitude,
                axisSign * vector.z / magnitude};
            const float acceleration = std::lerp(10.0F, 3.0F, settings.floatiness);
            result.gravityStrength = std::clamp(
                settings.motionSpeed * magnitude * acceleration, 0.0F, 36.0F);
        }
        result.turbulence = 0.04F + settings.sway * 0.62F;
        result.turbulenceScale = 0.35F + settings.randomness * 0.60F + settings.floatiness * 0.25F;
        return result;
    }

    inline float PhotoEffectOrbitAngle(const PhotoEffectSettings& settings, float seconds)
    {
        if (!settings.orbit || !std::isfinite(seconds)) { return 0.0F; }
        constexpr float fullTurn = 6.28318530718F;
        const float direction = settings.orbitDirection < 0 ? -1.0F : 1.0F;
        return std::remainder(seconds * settings.motionSpeed * 0.78F * direction, fullTurn);
    }
}
