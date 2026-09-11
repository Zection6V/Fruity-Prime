#pragma once

#include "../../Render/Crosshair.hpp"

#include <cstdint>
#include <optional>

namespace MphRead::Mods::Launcher::Gui
{
    enum class CrosshairPreviewBrush : std::uint8_t
    {
        Panel,
        Edge,
        Text
    };

    struct CrosshairPreviewRect
    {
        double X;
        double Y;
        double Width;
        double Height;
    };

    struct CrosshairPreviewPoint
    {
        double X;
        double Y;
    };

    struct CrosshairPreviewPen
    {
        CrosshairPreviewBrush Brush;
        double Thickness;
    };

    struct CrosshairPreviewRoundedRect
    {
        CrosshairPreviewRect Rect;
        double Radius;
    };

    class CrosshairPreviewDrawingContext
    {
    public:
        virtual ~CrosshairPreviewDrawingContext() = default;

        virtual void DrawRectangle(CrosshairPreviewBrush brush, CrosshairPreviewPen pen,
            CrosshairPreviewRoundedRect rect) = 0;
        virtual void FillRectangle(CrosshairPreviewBrush brush, CrosshairPreviewRect rect) = 0;
        virtual void DrawEllipse(std::optional<CrosshairPreviewBrush> brush,
            CrosshairPreviewPen pen, CrosshairPreviewPoint center,
            double radiusX, double radiusY) = 0;
    };

    class CrosshairPreview final
    {
    public:
        CrosshairPreview() = delete;

        static void Draw(CrosshairPreviewDrawingContext& context, CrosshairPreviewRect area,
            Render::CrosshairStyle style, Render::CrosshairSize size);
    };
}
