#pragma once

#include <algorithm>
#include <array>
#include <string>
#include <string_view>

namespace ScreenshotLightingAssistant::UI
{
    // Geometry only: test responsive cards without a game or an ImGui context.
    struct PresetCardLayout
    {
        float height;
        float diagramX;
        float diagramY;
        float diagramSize;
        float labelsX;
        float labelsY;
        float labelsWidth;
        float rowHeight;
        float rowGap;
        bool horizontal;
        float titleX;
        float titleY;
        float titleWidth;
        float titleHeight;
    };

    inline float MinimumPresetCardWidth(float a_labelTextWidth)
    {
        return 16.0F + 144.0F + 6.0F + a_labelTextWidth + 16.0F;
    }

    inline PresetCardLayout CalculatePresetCardLayout(float a_width, float a_fontHeight, float a_labelTextWidth, int a_count)
    {
        constexpr float padding = 8.0F;
        constexpr float gap = 6.0F;
        constexpr float cameraHeight = 15.0F;
        const float inner = std::max(1.0F, a_width - padding * 2.0F);
        const float titleHeight = a_fontHeight * 2.0F;
        const float labelWidth = a_labelTextWidth + 16.0F;
        const float rowHeight = a_fontHeight + 4.0F;
        constexpr float rowGap = 3.0F;
        // Reserve all three information rows to keep every card in a grid equal
        // height, including empty, one-light and two-light presets.
        (void)a_count;
        const float rowsHeight = rowHeight * 3.0F + rowGap * 2.0F;
        const bool horizontal = a_width >= MinimumPresetCardWidth(a_labelTextWidth);
        const float diagram = horizontal ? std::clamp(inner - gap - labelWidth, 144.0F, 168.0F) : std::min(inner, 168.0F);
        const float diagramHeight = diagram + cameraHeight * (diagram / 140.0F);
        if (horizontal) {
            const float bodyHeight = std::max(diagramHeight, titleHeight + gap + rowsHeight);
            const float infoX = padding + diagram + gap;
            const float infoWidth = inner - diagram - gap;
            return { padding * 2.0F + bodyHeight, padding, padding + (bodyHeight - diagramHeight) * 0.5F, diagram,
                infoX, padding + titleHeight + gap, infoWidth, rowHeight, rowGap, true,
                infoX, padding, infoWidth, titleHeight };
        }
        // Only very narrow cards stack; names and legends retain their font size.
        return { padding * 2.0F + titleHeight + gap + diagramHeight + gap + rowsHeight,
            padding + (inner - diagram) * 0.5F, padding + titleHeight + gap, diagram,
            padding, padding + titleHeight + gap + diagramHeight + gap, inner, rowHeight, rowGap, false,
            padding, padding, inner, titleHeight };
    }

    // Registration names are validated UTF-8. Never split a codepoint or shrink
    // the font to fit a long name. The card tooltip still contains the full name.
    template <class Measure>
    std::array<std::string, 2> PresetTitleLines(std::string_view name, float width, Measure measure)
    {
        std::array<std::string, 2> lines;
        std::size_t offset = 0;
        for (auto& line : lines) {
            while (offset < name.size()) {
                auto next = offset + 1;
                while (next < name.size() && (static_cast<unsigned char>(name[next]) & 0xC0U) == 0x80U) { ++next; }
                const auto candidate = line + std::string(name.substr(offset, next - offset));
                if (measure(candidate) > width) { break; }
                line = candidate;
                offset = next;
            }
        }
        if (offset < name.size()) {
            auto& last = lines.back();
            while (!last.empty() && measure(last + "...") > width) {
                auto start = last.size() - 1;
                while (start > 0 && (static_cast<unsigned char>(last[start]) & 0xC0U) == 0x80U) { --start; }
                last.resize(start);
            }
            if (measure(last + "...") <= width) { last += "..."; }
        }
        return lines;
    }
}
