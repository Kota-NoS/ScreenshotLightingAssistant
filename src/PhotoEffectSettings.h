#pragma once

#include <algorithm>
#include <array>
#include <cmath>

namespace ScreenshotLightingAssistant
{
    enum class PhotoEffectType : int
    {
        LuminousMotes = 0,
        SakuraPetals = 1,
        MagicStars = 2,
        AutumnLeaves = 3,
        WhiteFeathers = 4,
        PopStars = 5,
        BlackFeathers = 6,
        SnowCrystals = 7
    };

    enum class PhotoEffectMotionPreset : int
    {
        Gentle = 0,
        Falling = 1,
        Wind = 2,
        Updraft = 3,
        Orbit = 4,
        Custom = 5
    };

    inline constexpr int kPhotoEffectTypeMin = static_cast<int>(PhotoEffectType::LuminousMotes);
    inline constexpr int kPhotoEffectTypeMax = static_cast<int>(PhotoEffectType::SnowCrystals);
    inline constexpr int kPhotoEffectMotionPresetMin = static_cast<int>(PhotoEffectMotionPreset::Gentle);
    inline constexpr int kPhotoEffectMotionPresetMax = static_cast<int>(PhotoEffectMotionPreset::Custom);
    // UI order is deliberately independent from the serialized enum values.
    // Keep related pairs together without invalidating earlier saved settings.
    inline constexpr std::array<PhotoEffectType, 8> kPhotoEffectDisplayOrder{
        PhotoEffectType::LuminousMotes,
        PhotoEffectType::SakuraPetals,
        PhotoEffectType::MagicStars,
        PhotoEffectType::PopStars,
        PhotoEffectType::AutumnLeaves,
        PhotoEffectType::WhiteFeathers,
        PhotoEffectType::BlackFeathers,
        PhotoEffectType::SnowCrystals
    };

    inline constexpr int PhotoEffectDisplayIndex(PhotoEffectType type)
    {
        for (std::size_t index = 0; index < kPhotoEffectDisplayOrder.size(); ++index) {
            if (kPhotoEffectDisplayOrder[index] == type) { return static_cast<int>(index); }
        }
        return 0;
    }

    inline constexpr PhotoEffectType PhotoEffectTypeFromDisplayIndex(int index)
    {
        return kPhotoEffectDisplayOrder[static_cast<std::size_t>(
            std::clamp(index, 0, static_cast<int>(kPhotoEffectDisplayOrder.size()) - 1))];
    }
    inline constexpr float kPhotoEffectDensityMin = 1.00F;
    inline constexpr float kPhotoEffectDensityMax = 3.00F;
    inline constexpr float kParticleSizeMin = 0.50F;
    inline constexpr float kParticleSizeMax = 2.00F;
    inline constexpr float kParticleFieldSpreadMin = 0.50F;
    inline constexpr float kParticleFieldSpreadMax = 4.00F;
    inline constexpr float kPhotoEffectOpacityMin = 0.25F;
    inline constexpr float kPhotoEffectOpacityMax = 1.00F;
    inline constexpr float kPhotoEffectBrightnessMin = 0.25F;
    inline constexpr float kPhotoEffectBrightnessMax = 4.00F;
    inline constexpr float kParticleFieldVerticalMin = -1.00F;
    inline constexpr float kParticleFieldVerticalMax = 1.00F;
    inline constexpr float kPhotoEffectMotionSpeedMin = 0.25F;
    inline constexpr float kPhotoEffectMotionSpeedMax = 4.00F;
    inline constexpr float kPhotoEffectFlowMin = -1.00F;
    inline constexpr float kPhotoEffectFlowMax = 1.00F;
    inline constexpr float kPhotoEffectHorizontalFlowMin = -2.00F;
    inline constexpr float kPhotoEffectHorizontalFlowMax = 2.00F;
    inline constexpr float kPhotoEffectFloatinessMin = 0.00F;
    inline constexpr float kPhotoEffectFloatinessMax = 1.00F;
    inline constexpr float kPhotoEffectSwayMin = 0.00F;
    inline constexpr float kPhotoEffectSwayMax = 1.00F;
    inline constexpr float kPhotoEffectRandomnessMin = 0.00F;
    inline constexpr float kPhotoEffectRandomnessMax = 1.00F;
    inline constexpr float kPhotoEffectLifetimeMin = 0.50F;
    inline constexpr float kPhotoEffectLifetimeMax = 2.00F;
    inline constexpr float kPhotoEffectLifetimeStep = 0.25F;
    inline constexpr int kPhotoEffectLifetimeVariantCount = 7;
    inline constexpr char kBlackFeatherTexturePath[] =
        "textures\\ScreenshotLightingAssistant\\effects\\black_feather_atlas.dds";

    struct PhotoEffectSettings
    {
        bool enabled{};
        PhotoEffectType type{PhotoEffectType::LuminousMotes};
        float density{2.00F};
        float size{1.00F};
        float spread{1.00F};
        float opacity{1.00F};
        float brightness{1.00F};
        float verticalOffset{0.00F};
        PhotoEffectMotionPreset motionPreset{PhotoEffectMotionPreset::Gentle};
        float motionSpeed{0.70F};
        float verticalMovement{0.22F};
        float horizontalFlow{0.04F};
        float floatiness{0.68F};
        float sway{0.24F};
        float randomness{0.35F};
        float lifetime{1.00F};
        bool orbit{};
        int orbitDirection{1};

        bool operator==(const PhotoEffectSettings&) const = default;
    };

    inline int PhotoEffectLifetimeVariant(float lifetime)
    {
        return std::clamp(static_cast<int>(std::lround(
            (lifetime - kPhotoEffectLifetimeMin) / kPhotoEffectLifetimeStep)),
            0, kPhotoEffectLifetimeVariantCount - 1);
    }

    inline float PhotoEffectLifetimeFromVariant(int variant)
    {
        return kPhotoEffectLifetimeMin +
            static_cast<float>(std::clamp(variant, 0, kPhotoEffectLifetimeVariantCount - 1)) *
            kPhotoEffectLifetimeStep;
    }

    inline float PhotoEffectEmissionRate(PhotoEffectType type, float density)
    {
        const float value = std::clamp(density, kPhotoEffectDensityMin, kPhotoEffectDensityMax);
        const bool sparseVisual = type == PhotoEffectType::SakuraPetals ||
            type == PhotoEffectType::MagicStars ||
            type == PhotoEffectType::AutumnLeaves ||
            type == PhotoEffectType::WhiteFeathers ||
            type == PhotoEffectType::PopStars ||
            type == PhotoEffectType::BlackFeathers ||
            type == PhotoEffectType::SnowCrystals;
        const float low = sparseVisual ? 3.0F : 4.0F;
        const float standard = sparseVisual ? 6.0F : 8.0F;
        const float high = sparseVisual ? 10.0F : 14.0F;
        if (value <= 2.0F) {
            return std::lerp(low, standard, value - 1.0F);
        }
        return std::lerp(standard, high, value - 2.0F);
    }

    inline float PhotoEffectSpreadCompensation(float spread)
    {
        // The emitter node carries the requested field spread. Keep the child
        // particle system at unit world scale so world-space particle positions
        // are not multiplied by the size control in large exterior-cell coordinates.
        return 1.0F / std::clamp(spread,
            kParticleFieldSpreadMin, kParticleFieldSpreadMax);
    }

    inline float PhotoEffectCompositeStrength(float opacity, float brightness)
    {
        // The intended post-blend visual-strength model. Runtime removes the
        // extra alpha factor applied by Skyrim's effect-shader path so that the
        // user-facing opacity x brightness relationship remains meaningful.
        return std::clamp(opacity, kPhotoEffectOpacityMin, kPhotoEffectOpacityMax) *
            std::clamp(brightness, kPhotoEffectBrightnessMin, kPhotoEffectBrightnessMax);
    }

    inline float PhotoEffectMaterialBrightnessScale(float opacity, float brightness)
    {
        // BSEffectShader applies baseColor alpha while its source-alpha blend
        // applies coverage again. Divide the material intensity by one alpha
        // factor so 0.25 opacity x 4.00 brightness can recover approximately
        // the authored 1.00 x 1.00 contribution without changing edge coverage.
        return std::clamp(brightness, kPhotoEffectBrightnessMin, kPhotoEffectBrightnessMax) /
            std::clamp(opacity, kPhotoEffectOpacityMin, kPhotoEffectOpacityMax);
    }

    struct PhotoEffectMaterialTint
    {
        float red{1.0F};
        float green{1.0F};
        float blue{1.0F};
    };

    inline constexpr bool IsFeatherPhotoEffect(PhotoEffectType type)
    {
        return type == PhotoEffectType::WhiteFeathers ||
            type == PhotoEffectType::BlackFeathers;
    }

    inline constexpr bool IsFourIdentityPhotoEffect(PhotoEffectType type)
    {
        return type == PhotoEffectType::AutumnLeaves ||
            IsFeatherPhotoEffect(type) ||
            type == PhotoEffectType::PopStars ||
            type == PhotoEffectType::SnowCrystals;
    }

    inline constexpr PhotoEffectMaterialTint PhotoEffectTint(PhotoEffectType type)
    {
        // White and black feathers share a NIF. Alpha44 neutralizes that NIF's
        // former 0.92 scale and cool emitter/fade multipliers for White. Keep
        // Black's peak contribution at the alpha43 level while retaining its
        // dedicated grayscale atlas detail.
        if (type == PhotoEffectType::BlackFeathers) {
            return PhotoEffectMaterialTint{
                0.92F * 0.96F,
                0.92F * 0.98F * 0.99F,
                0.92F * 0.95F};
        }
        return PhotoEffectMaterialTint{};
    }

    inline constexpr PhotoEffectMaterialTint BlackFeatherFallbackTint()
    {
        // If the dedicated dark atlas is missing, darken the shared White atlas
        // as alpha43 did and include the same shared-NIF compensation.
        return PhotoEffectMaterialTint{
            0.12F * 0.92F * 0.96F,
            0.125F * 0.92F * 0.98F * 0.99F,
            0.14F * 0.92F * 0.95F};
    }

    inline void ApplyPhotoEffectMotionPreset(PhotoEffectSettings& settings,
        PhotoEffectMotionPreset preset)
    {
        settings.motionPreset = preset;
        if (preset == PhotoEffectMotionPreset::Custom) { return; }
        settings.orbit = preset == PhotoEffectMotionPreset::Orbit;
        switch (preset) {
        case PhotoEffectMotionPreset::Gentle:
            if (settings.type == PhotoEffectType::SnowCrystals) {
                // Snow crystals drift slowly downward by default. Their texture
                // frames are fixed, so this movement never changes crystal identity.
                settings.verticalOffset = 0.35F;
                settings.motionSpeed = 0.54F;
                settings.verticalMovement = -0.22F;
                settings.horizontalFlow = 0.03F;
                settings.floatiness = 0.72F;
                settings.sway = 0.20F;
                settings.randomness = 0.32F;
                settings.lifetime = 1.25F;
            } else {
                settings.verticalOffset = 0.00F;
                settings.motionSpeed = 0.70F;
                settings.verticalMovement = 0.22F;
                settings.horizontalFlow = 0.04F;
                settings.floatiness = 0.68F;
                settings.sway = 0.24F;
                settings.randomness = 0.35F;
                settings.lifetime = 1.00F;
            }
            break;
        case PhotoEffectMotionPreset::Falling:
            settings.verticalOffset = 0.85F;
            settings.motionSpeed = 0.74F;
            settings.verticalMovement = -0.55F;
            settings.horizontalFlow = 0.03F;
            settings.floatiness = 0.30F;
            settings.sway = 0.34F;
            settings.randomness = 0.46F;
            settings.lifetime = 1.50F;
            break;
        case PhotoEffectMotionPreset::Wind:
            settings.verticalOffset = 0.00F;
            settings.motionSpeed = 1.00F;
            settings.verticalMovement = 0.08F;
            settings.horizontalFlow = 0.78F;
            settings.floatiness = 0.58F;
            settings.sway = 0.36F;
            settings.randomness = 0.58F;
            settings.lifetime = 1.00F;
            break;
        case PhotoEffectMotionPreset::Updraft:
            settings.verticalOffset = -0.10F;
            settings.motionSpeed = 0.72F;
            settings.verticalMovement = 0.82F;
            settings.horizontalFlow = 0.06F;
            settings.floatiness = 0.52F;
            settings.sway = 0.22F;
            settings.randomness = 0.38F;
            settings.lifetime = 1.25F;
            break;
        case PhotoEffectMotionPreset::Orbit:
            settings.verticalOffset = 0.00F;
            settings.motionSpeed = 0.78F;
            settings.verticalMovement = 0.00F;
            settings.horizontalFlow = 0.00F;
            settings.floatiness = 0.85F;
            settings.sway = 0.20F;
            settings.randomness = 0.38F;
            settings.lifetime = 1.25F;
            settings.orbit = true;
            settings.orbitDirection = settings.orbitDirection < 0 ? -1 : 1;
            break;
        case PhotoEffectMotionPreset::Custom:
            break;
        }
    }

    inline bool ValidPhotoEffectSettings(const PhotoEffectSettings& settings)
    {
        const auto type = static_cast<int>(settings.type);
        const auto motionPreset = static_cast<int>(settings.motionPreset);
        return type >= kPhotoEffectTypeMin && type <= kPhotoEffectTypeMax &&
            motionPreset >= kPhotoEffectMotionPresetMin && motionPreset <= kPhotoEffectMotionPresetMax &&
            std::isfinite(settings.density) && settings.density >= kPhotoEffectDensityMin &&
            settings.density <= kPhotoEffectDensityMax &&
            std::isfinite(settings.size) && settings.size >= kParticleSizeMin &&
            settings.size <= kParticleSizeMax &&
            std::isfinite(settings.spread) && settings.spread >= kParticleFieldSpreadMin &&
            settings.spread <= kParticleFieldSpreadMax && std::isfinite(settings.verticalOffset) &&
            std::isfinite(settings.opacity) && settings.opacity >= kPhotoEffectOpacityMin &&
            settings.opacity <= kPhotoEffectOpacityMax &&
            std::isfinite(settings.brightness) && settings.brightness >= kPhotoEffectBrightnessMin &&
            settings.brightness <= kPhotoEffectBrightnessMax &&
            settings.verticalOffset >= kParticleFieldVerticalMin &&
            settings.verticalOffset <= kParticleFieldVerticalMax &&
            std::isfinite(settings.motionSpeed) && settings.motionSpeed >= kPhotoEffectMotionSpeedMin &&
            settings.motionSpeed <= kPhotoEffectMotionSpeedMax &&
            std::isfinite(settings.verticalMovement) && settings.verticalMovement >= kPhotoEffectFlowMin &&
            settings.verticalMovement <= kPhotoEffectFlowMax &&
            std::isfinite(settings.horizontalFlow) &&
            settings.horizontalFlow >= kPhotoEffectHorizontalFlowMin &&
            settings.horizontalFlow <= kPhotoEffectHorizontalFlowMax &&
            std::isfinite(settings.floatiness) && settings.floatiness >= kPhotoEffectFloatinessMin &&
            settings.floatiness <= kPhotoEffectFloatinessMax &&
            std::isfinite(settings.sway) && settings.sway >= kPhotoEffectSwayMin &&
            settings.sway <= kPhotoEffectSwayMax &&
            std::isfinite(settings.randomness) && settings.randomness >= kPhotoEffectRandomnessMin &&
            settings.randomness <= kPhotoEffectRandomnessMax &&
            std::isfinite(settings.lifetime) && settings.lifetime >= kPhotoEffectLifetimeMin &&
            settings.lifetime <= kPhotoEffectLifetimeMax &&
            (settings.orbitDirection == -1 || settings.orbitDirection == 1);
    }
}
