#pragma once

#include <algorithm>
#include <array>
#include <cmath>

namespace ScreenshotLightingAssistant
{
    enum class ScreenLightDirection : int
    {
        UpperLeft = 0,
        UpperCenter,
        UpperRight,
        Count
    };

    enum class ScreenLightTone : int
    {
        Warm = 0,
        Neutral,
        Cool,
        Custom,
        Count
    };

    enum class ScreenShadeDirection : int
    {
        Left = 0,
        Right,
        LowerLeft,
        Bottom,
        LowerRight,
        Count
    };

    enum class ScreenVignetteStyle : int
    {
        Soft = 0,
        Standard,
        Count
    };

    enum class ScreenOverlayLayer : int
    {
        Light = 0,
        Shade,
        Vignette,
        Cinema
    };

    constexpr std::array kScreenOverlayDrawOrder{
        ScreenOverlayLayer::Light,
        ScreenOverlayLayer::Shade,
        ScreenOverlayLayer::Vignette,
        ScreenOverlayLayer::Cinema
    };

    constexpr float kScreenOverlayStrengthMin = 0.0F;
    constexpr float kScreenOverlayStrengthMax = 1.0F;
    constexpr float kScreenOverlaySpreadMin = 0.08F;
    constexpr float kScreenOverlaySpreadMax = 0.80F;
    constexpr float kScreenOverlayAspectMin = 1.78F;
    constexpr float kScreenOverlayAspectMax = 2.76F;
    constexpr std::array kScreenCinemaPresetAspects{2.20F, 2.39F, 2.76F};

    struct ScreenLightSettings
    {
        bool enabled{false};
        ScreenLightDirection direction{ScreenLightDirection::UpperLeft};
        ScreenLightTone tone{ScreenLightTone::Warm};
        std::array<float, 3> color{1.0F, 0.698F, 0.282F};
        std::array<float, 3> customColor{1.0F, 0.698F, 0.282F};
        float strength{0.22F};
        float spread{0.48F};

        bool operator==(const ScreenLightSettings&) const = default;
    };

    struct ScreenShadeSettings
    {
        bool enabled{false};
        ScreenShadeDirection direction{ScreenShadeDirection::Left};
        float strength{0.48F};
        float spread{0.62F};

        bool operator==(const ScreenShadeSettings&) const = default;
    };

    struct ScreenVignetteSettings
    {
        bool enabled{false};
        ScreenVignetteStyle style{ScreenVignetteStyle::Soft};
        float strength{0.24F};
        float spread{0.22F};

        bool operator==(const ScreenVignetteSettings&) const = default;
    };

    struct ScreenCinemaSettings
    {
        bool enabled{false};
        float strength{1.0F};
        float aspect{2.39F};

        bool operator==(const ScreenCinemaSettings&) const = default;
    };

    struct ScreenOverlaySettings
    {
        bool enabled{false};
        ScreenLightSettings light{};
        ScreenShadeSettings shade{};
        ScreenVignetteSettings vignette{};
        ScreenCinemaSettings cinema{};

        bool operator==(const ScreenOverlaySettings&) const = default;
    };

    struct CinemaBarLayout
    {
        float height{};
        bool visible{};
    };

    inline CinemaBarLayout CalculateCinemaBars(float width, float height, float targetAspect)
    {
        if (!std::isfinite(width) || !std::isfinite(height) || !std::isfinite(targetAspect) ||
            width <= 0.0F || height <= 0.0F || targetAspect <= 0.0F) {
            return {};
        }
        const float contentHeight = width / targetAspect;
        const float barHeight = std::max(0.0F, (height - contentHeight) * 0.5F);
        return {barHeight, barHeight > 0.0F};
    }

    inline bool AnyScreenOverlayLayerEnabled(const ScreenOverlaySettings& settings)
    {
        return settings.light.enabled || settings.shade.enabled ||
            settings.vignette.enabled || settings.cinema.enabled;
    }

    inline bool ScreenOverlayLayerActive(const ScreenOverlaySettings& settings, bool layerEnabled)
    {
        return settings.enabled && layerEnabled;
    }

    inline std::array<float, 3> ScreenLightToneColor(ScreenLightTone tone)
    {
        switch (tone) {
        case ScreenLightTone::Warm:
            return {1.0F, 0.698F, 0.282F};
        case ScreenLightTone::Neutral:
            return {1.0F, 1.0F, 1.0F};
        case ScreenLightTone::Cool:
            return {0.49F, 0.72F, 1.0F};
        case ScreenLightTone::Custom:
        case ScreenLightTone::Count:
            return {1.0F, 0.698F, 0.282F};
        }
        return {1.0F, 0.698F, 0.282F};
    }

    inline void SelectScreenLightDirection(ScreenOverlaySettings& settings, ScreenLightDirection direction)
    {
        if (direction < ScreenLightDirection::UpperLeft || direction >= ScreenLightDirection::Count) {
            direction = ScreenLightDirection::UpperLeft;
        }
        settings.enabled = true;
        settings.light.enabled = true;
        settings.light.direction = direction;
    }

    inline void SetScreenLightTone(ScreenOverlaySettings& settings, ScreenLightTone tone)
    {
        if (tone < ScreenLightTone::Warm || tone >= ScreenLightTone::Count) {
            tone = ScreenLightTone::Warm;
        }
        settings.light.tone = tone;
        settings.light.color = tone == ScreenLightTone::Custom ?
            settings.light.customColor : ScreenLightToneColor(tone);
    }

    inline void SetScreenLightCustomColor(ScreenOverlaySettings& settings,
        const std::array<float, 3>& color)
    {
        settings.light.customColor = color;
        settings.light.tone = ScreenLightTone::Custom;
        settings.light.color = color;
    }

    inline void SelectScreenShadeDirection(ScreenOverlaySettings& settings, ScreenShadeDirection direction)
    {
        if (direction < ScreenShadeDirection::Left || direction >= ScreenShadeDirection::Count) {
            direction = ScreenShadeDirection::Left;
        }
        settings.enabled = true;
        settings.shade.enabled = true;
        settings.shade.direction = direction;
    }

    inline void SelectScreenVignetteStyle(ScreenOverlaySettings& settings, ScreenVignetteStyle style)
    {
        if (style < ScreenVignetteStyle::Soft || style >= ScreenVignetteStyle::Count) {
            style = ScreenVignetteStyle::Soft;
        }
        settings.enabled = true;
        settings.vignette.enabled = true;
        settings.vignette.style = style;
        if (style == ScreenVignetteStyle::Soft) {
            settings.vignette.strength = 0.24F;
            settings.vignette.spread = 0.22F;
        } else {
            settings.vignette.strength = 0.42F;
            settings.vignette.spread = 0.30F;
        }
    }

    inline void SelectScreenCinemaAspect(ScreenOverlaySettings& settings, float aspect)
    {
        settings.enabled = true;
        settings.cinema.enabled = true;
        settings.cinema.aspect = std::clamp(aspect, kScreenOverlayAspectMin, kScreenOverlayAspectMax);
    }

    inline void ToggleScreenLightDirection(ScreenOverlaySettings& settings, ScreenLightDirection direction)
    {
        if (ScreenOverlayLayerActive(settings, settings.light.enabled) &&
            settings.light.direction == direction) {
            settings.light.enabled = false;
            return;
        }
        SelectScreenLightDirection(settings, direction);
    }

    inline void ToggleScreenShadeDirection(ScreenOverlaySettings& settings, ScreenShadeDirection direction)
    {
        if (ScreenOverlayLayerActive(settings, settings.shade.enabled) &&
            settings.shade.direction == direction) {
            settings.shade.enabled = false;
            return;
        }
        SelectScreenShadeDirection(settings, direction);
    }

    inline void ToggleScreenVignetteStyle(ScreenOverlaySettings& settings, ScreenVignetteStyle style)
    {
        if (ScreenOverlayLayerActive(settings, settings.vignette.enabled) &&
            settings.vignette.style == style) {
            settings.vignette.enabled = false;
            return;
        }
        if (settings.vignette.style == style) {
            settings.enabled = true;
            settings.vignette.enabled = true;
        } else {
            SelectScreenVignetteStyle(settings, style);
        }
    }

    inline void ToggleScreenCinemaAspect(ScreenOverlaySettings& settings, float aspect)
    {
        const float bounded = std::clamp(aspect, kScreenOverlayAspectMin, kScreenOverlayAspectMax);
        if (ScreenOverlayLayerActive(settings, settings.cinema.enabled) &&
            std::abs(settings.cinema.aspect - bounded) < 0.005F) {
            settings.cinema.enabled = false;
            return;
        }
        SelectScreenCinemaAspect(settings, bounded);
    }

    inline float ScreenDiagonalShadeReachScale(float spread)
    {
        const float normalized = std::clamp(
            (spread - kScreenOverlaySpreadMin) /
                (kScreenOverlaySpreadMax - kScreenOverlaySpreadMin),
            0.0F, 1.0F);
        return 0.5F + normalized;
    }

    inline void ResetScreenOverlayLayerValues(ScreenOverlaySettings& settings, ScreenOverlayLayer layer)
    {
        switch (layer) {
        case ScreenOverlayLayer::Light:
            settings.light.strength = 0.22F;
            settings.light.spread = 0.48F;
            if (settings.light.tone != ScreenLightTone::Custom) {
                const bool wasEnabled = settings.light.enabled;
                const bool masterEnabled = settings.enabled;
                SetScreenLightTone(settings, settings.light.tone);
                settings.light.enabled = wasEnabled;
                settings.enabled = masterEnabled;
            }
            break;
        case ScreenOverlayLayer::Shade:
            settings.shade.strength = 0.48F;
            settings.shade.spread = 0.62F;
            break;
        case ScreenOverlayLayer::Vignette:
            if (settings.vignette.style == ScreenVignetteStyle::Soft) {
                settings.vignette.strength = 0.24F;
                settings.vignette.spread = 0.22F;
            } else {
                settings.vignette.strength = 0.42F;
                settings.vignette.spread = 0.30F;
            }
            break;
        case ScreenOverlayLayer::Cinema:
            settings.cinema.strength = 1.0F;
            settings.cinema.aspect = 2.39F;
            break;
        }
    }

    template <class Enum>
    inline bool ValidOverlayEnum(Enum value, Enum first, Enum count)
    {
        return static_cast<int>(value) >= static_cast<int>(first) &&
            static_cast<int>(value) < static_cast<int>(count);
    }

    inline bool ValidScreenOverlaySettings(const ScreenOverlaySettings& settings)
    {
        const auto validStrength = [](float value) {
            return std::isfinite(value) && value >= kScreenOverlayStrengthMin && value <= kScreenOverlayStrengthMax;
        };
        const auto validSpread = [](float value) {
            return std::isfinite(value) && value >= kScreenOverlaySpreadMin && value <= kScreenOverlaySpreadMax;
        };
        const auto validColor = [](float value) {
            return std::isfinite(value) && value >= 0.0F && value <= 1.0F;
        };

        return ValidOverlayEnum(settings.light.direction, ScreenLightDirection::UpperLeft, ScreenLightDirection::Count) &&
            ValidOverlayEnum(settings.light.tone, ScreenLightTone::Warm, ScreenLightTone::Count) &&
            std::all_of(settings.light.color.begin(), settings.light.color.end(), validColor) &&
            std::all_of(settings.light.customColor.begin(), settings.light.customColor.end(), validColor) &&
            validStrength(settings.light.strength) && validSpread(settings.light.spread) &&
            ValidOverlayEnum(settings.shade.direction, ScreenShadeDirection::Left, ScreenShadeDirection::Count) &&
            validStrength(settings.shade.strength) && validSpread(settings.shade.spread) &&
            ValidOverlayEnum(settings.vignette.style, ScreenVignetteStyle::Soft, ScreenVignetteStyle::Count) &&
            validStrength(settings.vignette.strength) && validSpread(settings.vignette.spread) &&
            validStrength(settings.cinema.strength) &&
            std::isfinite(settings.cinema.aspect) &&
            settings.cinema.aspect >= kScreenOverlayAspectMin && settings.cinema.aspect <= kScreenOverlayAspectMax;
    }
}
