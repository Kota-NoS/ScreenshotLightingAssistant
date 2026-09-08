#pragma once

#include <algorithm>
#include <optional>

namespace ScreenshotLightingAssistant
{
    struct WindowRectangle
    {
        float x, y, width, height;
        bool operator==(const WindowRectangle&) const = default;
    };

    // Process-local geometry: no game saves or framework INI writes.
    // Restore on appearance, and clamp again when the viewport changes.
    class PresetWindowGeometry
    {
    public:
        WindowRectangle Resolve(WindowRectangle work) const
        {
            const float maxWidth = std::max(1.0F, work.width * .95F);
            const float maxHeight = std::max(1.0F, work.height * .95F);
            const float width = std::clamp(saved_ ? saved_->width : 1120.0F, std::min(340.0F, maxWidth), maxWidth);
            const float height = std::clamp(saved_ ? saved_->height : 800.0F, std::min(300.0F, maxHeight), maxHeight);
            return {
                std::clamp(saved_ ? saved_->x : work.x + (work.width - width) * .5F,
                    work.x, work.x + std::max(0.0F, work.width - width)),
                std::clamp(saved_ ? saved_->y : work.y + (work.height - height) * .5F,
                    work.y, work.y + std::max(0.0F, work.height - height)),
                width, height };
        }
        bool NeedsClamp(WindowRectangle work) const { return saved_ && *saved_ != Resolve(work); }
        void Remember(WindowRectangle rectangle) { saved_ = rectangle; }
    private:
        std::optional<WindowRectangle> saved_;
    };

    struct PositionWindowLayout
    {
        WindowRectangle initial;
        float minimumWidth, minimumHeight, maximumWidth, maximumHeight;
    };

    struct FaceControlsLayout { WindowRectangle toggle, settings; };
    inline FaceControlsLayout CalculateFaceControls(float canvas)
    {
        // Two explicit hit targets just below the Actor circle, inside the ring.
        const float y = canvas * 0.5F + std::min(44.0F, canvas * 0.13F) * 1.25F + 4.0F;
        return { { canvas * 0.5F - 35.0F, y, 32.0F, 32.0F },
            { canvas * 0.5F + 3.0F, y, 32.0F, 32.0F } };
    }

    inline float LightSlotGroupGap(float font, float spacing)
    {
        return std::max(12.0F, spacing + font * 0.35F);
    }

    struct RangeControlLayout { float sliderWidth; bool sameRow; };
    inline RangeControlLayout CalculateRangeControl(float width, float basicSuffix, float rangeSuffix, float minimumSlider)
    {
        const bool sameRow = width >= std::max(basicSuffix, rangeSuffix) + minimumSlider;
        return { std::max(1.0F, width - (sameRow ? std::max(basicSuffix, rangeSuffix) : basicSuffix)), sameRow };
    }

    inline float LightSlotRowWidth(float titleWidth, float pairWidth, float titleGap, float groupGap)
    {
        return titleWidth + titleGap + pairWidth * 3.0F + groupGap * 2.0F;
    }

    struct FinePositionFooterLayout
    {
        bool sameRow;
        float historyX;
    };

    inline FinePositionFooterLayout CalculateFinePositionFooter(float width, float resetWidth, float historyWidth, float gap)
    {
        return { width >= resetWidth + gap + historyWidth, std::max(0.0F, width - historyWidth) };
    }

    struct DiagramFooterLayout
    {
        float cameraX, cameraY, height;
    };

    inline DiagramFooterLayout CalculateDiagramFooter(float canvas, float frame)
    {
        const float y = std::max(canvas * 0.94F + 4.0F, canvas + 5.0F - frame * 0.5F);
        // Fine adjustment follows this camera reserve inside the diagram column.
        return { canvas * 0.5F, y + frame * 0.5F, y + frame };
    }

    inline bool FitsInline(float width, float firstWidth, float secondWidth, float gap)
    {
        return firstWidth + gap + secondWidth <= width;
    }

    inline float BookmarkRowWidth(float labelWidth, float setWidth, float restoreWidth, float gap)
    {
        return labelWidth + setWidth + restoreWidth + gap * 2.0F;
    }

    // Feed actual item widths in display order. Reset the used width on wrap,
    // so later buttons can share the new row without shrinking their labels.
    struct ActionRowLayout
    {
        float width, gap;
        float used{};
        bool started{};

        bool Place(float buttonWidth)
        {
            const bool sameLine = started && FitsInline(width, used, buttonWidth, gap);
            used = sameLine ? used + gap + buttonWidth : buttonWidth;
            started = true;
            return sameLine;
        }
    };

    // Inputs are finite ImGui measurements in pixels. Prefer beside the main
    // pane on first use; clamp inside the work area when no free side exists.
    inline PositionWindowLayout CalculatePositionWindowLayout(WindowRectangle work, WindowRectangle host,
        float requiredWidth, float desiredHeight)
    {
        const float margin = std::min({ 12.0F, work.width * 0.025F, work.height * 0.025F });
        const float maxWidth = std::max(1.0F, work.width - margin * 2.0F);
        const float maxHeight = std::max(1.0F, work.height - margin * 2.0F);
        const float minWidth = std::min(std::max(300.0F, requiredWidth), maxWidth);
        const float minHeight = std::min(300.0F, maxHeight);
        const float width = std::clamp(std::max(360.0F, requiredWidth), minWidth, maxWidth);
        const float height = std::clamp(desiredHeight, minHeight, maxHeight);
        const float left = work.x + margin;
        const float right = left + maxWidth;
        float x = host.x + host.width + margin;
        if (x + width > right) { x = host.x - width - margin; }
        x = std::clamp(x, left, right - width);
        const float y = std::clamp(host.y, work.y + margin, work.y + margin + maxHeight - height);
        return { { x, y, width, height }, minWidth, minHeight, maxWidth, maxHeight };
    }
}
