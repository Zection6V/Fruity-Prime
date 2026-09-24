#pragma once

// Drawing for the launcher's control tree: solid and rounded rectangles,
// borders, text and images, in pixels with the origin at the top left.
//
// Every draw goes through RenderBackend, so this file knows nothing about the
// graphics API in use.

#include "Backend.hpp"
#include "Element.hpp"
#include "Text.hpp"

#include <cstdint>
#include <string_view>

namespace MphRead::NativeRuntime::Gui
{
    class Renderer final
    {
    public:
        // Everything after this is in pixels within a surface this size.
        void Begin(std::int32_t width, std::int32_t height);
        void End();

        void FillRect(Rect rect, Color color);
        void FillRoundedRect(Rect rect, double radius, Color color);
        // A border drawn inside the rect, as Avalonia's BorderThickness is.
        void StrokeRoundedRect(Rect rect, double radius, Thickness thickness, Color color);
        void DrawText(std::string_view text, Rect rect, double fontSize,
            FontWeight weight, Color color, TextAlignment align, bool wrap);
        // One line with its top left corner at the point, which is where
        // Avalonia's DrawText puts a FormattedText.
        void DrawTextAt(std::string_view text, double x, double y, double fontSize,
            FontWeight weight, Color color);
        // An RGBA texture, stretched to the rect.
        void DrawImage(TextureHandle texture, Rect rect, double opacity = 1.0);
        // The same, taking only the part of the texture the source rect names,
        // in texture coordinates from zero to one.
        void DrawImage(TextureHandle texture, Rect rect, Rect source,
            double opacity);

        // Nothing outside this rectangle is drawn until it is popped.
        void PushClip(Rect rect);
        void PopClip();

        // Everything drawn until the matching pop is scaled about the point,
        // which is how a layout transform reaches the picture.
        void PushScale(double x, double y, double scale);
        void PopScale();

        // The whole arranged tree, in order.
        void DrawElement(Element& element);

    private:
        struct Transform final
        {
            double X = 0.0;
            double Y = 0.0;
            double Scale = 1.0;
        };

        [[nodiscard]] bool Clipped(Rect rect) const;
        // A rectangle in the element's own coordinates, in the window's.
        [[nodiscard]] Rect Map(Rect rect) const noexcept;
        [[nodiscard]] double Scale() const noexcept;

        std::int32_t _width = 0;
        std::int32_t _height = 0;
        std::vector<Rect> _clips;
        std::vector<Transform> _transforms;
    };
}
