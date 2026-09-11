#include "CrosshairPreview.hpp"

#include <cstddef>
#include <tuple>
#include <vector>

namespace MphRead::Mods::Launcher::Gui
{
    void CrosshairPreview::Draw(CrosshairPreviewDrawingContext& context,
        CrosshairPreviewRect area, Render::CrosshairStyle style, Render::CrosshairSize size)
    {
        context.DrawRectangle(CrosshairPreviewBrush::Panel,
            CrosshairPreviewPen{CrosshairPreviewBrush::Edge, 1.0},
            CrosshairPreviewRoundedRect{area, 4.0});

        const double cx = area.X + area.Width / 2.0;
        const double cy = area.Y + area.Height / 2.0;
        const float scale = Render::Crosshair::ScaleOf(size);
        const std::vector<Render::CrosshairBar> bars = Render::Crosshair::BarsOf(style, scale);
        for (std::size_t i = 0; i < bars.size(); i++)
        {
            const auto [left, right, bottom, top] = Render::Crosshair::EdgesOf(bars[i]);
            context.FillRectangle(CrosshairPreviewBrush::Text, CrosshairPreviewRect{
                cx + left,
                cy - top,
                right - left,
                top - bottom
            });
        }

        const auto [radius, thickness] = Render::Crosshair::RingOf(style, scale);
        if (thickness > 0.0F)
        {
            context.DrawEllipse(std::nullopt,
                CrosshairPreviewPen{CrosshairPreviewBrush::Text, thickness},
                CrosshairPreviewPoint{cx, cy}, radius, radius);
        }
    }
}
