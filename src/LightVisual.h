#pragma once
#include "RuntimeModel.h"

namespace ScreenshotLightingAssistant::UI
{
    using RGB8 = std::array<std::uint8_t, 3>;

    inline RGB8 DiagramTint(const LightSettings& light)
    {
        const auto tint = RuntimeTint(light).value_or(LightTint{1, 1, 1});
        const auto channel = [&](float value) {
            // OFF remains tinted but subdued; the slash is the non-color cue.
            return static_cast<std::uint8_t>(std::clamp(light.enabled ? value : value * .28F + .09F, 0.0F, 1.0F) * 255.0F + .5F);
        };
        return {channel(tint.red), channel(tint.green), channel(tint.blue)};
    }
    inline float RelativeLuminance(const RGB8& rgb)
    {
        const auto linear = [](std::uint8_t v) {
            const float s = v / 255.0F;
            return s <= .04045F ? s / 12.92F : std::pow((s + .055F) / 1.055F, 2.4F);
        };
        return .2126F * linear(rgb[0]) + .7152F * linear(rgb[1]) + .0722F * linear(rgb[2]);
    }
    inline bool UseBlackNumber(const RGB8& rgb)
    {
        // Choose whichever of pure black/white has greater contrast (>= 4.58:1).
        return RelativeLuminance(rgb) > .17912878F;
    }
    struct NumberBadge { float x; float y; float radius; };
    inline constexpr float kPresetDiagramReferenceSize = 140.0F;
    inline float PresetDiagramScale(float size) { return size / kPresetDiagramReferenceSize; }
    inline float DiagramNodeRadius(float canvasSize, int count)
    {
        const float base = std::clamp(canvasSize * .073F, 13.0F, 31.0F);
        // A cluster in a thumbnail needs more space than an empty/single node.
        // With only three slots, at most one direction can contain a cluster.
        return count > 1 ? std::max(base, 19.0F) : base;
    }
    inline NumberBadge OccupantBadge(float radius, int count, int ordinal)
    {
        if (count <= 1) { return {0, 0, radius - 2.0F}; }
        if (count == 2) { return {(ordinal ? .48F : -.48F) * radius, 0, radius * .46F}; }
        if (ordinal == 0) { return {0, -.44F * radius, .43F * radius}; }
        return {(ordinal == 1 ? -.43F : .43F) * radius, .32F * radius, .43F * radius};
    }
}
