#include "Renderer.hpp"
#include "../System/Encoding.hpp"

#include "Backend.hpp"

#include <algorithm>
#include <cmath>

namespace MphRead::NativeRuntime::Gui
{
    void Renderer::Begin(std::int32_t width, std::int32_t height)
    {
        _width = std::max(1, width);
        _height = std::max(1, height);
        _clips.clear();
        _transforms.clear();
        Backend().BeginFrame(_width, _height);
    }

    void Renderer::End()
    {
        Backend().EndFrame();
    }

    double Renderer::Scale() const noexcept
    {
        return _transforms.empty() ? 1.0 : _transforms.back().Scale;
    }

    Rect Renderer::Map(Rect rect) const noexcept
    {
        if (_transforms.empty())
        {
            return rect;
        }
        const Transform& transform = _transforms.back();
        return Rect{transform.X + (rect.X - transform.X) * transform.Scale,
            transform.Y + (rect.Y - transform.Y) * transform.Scale,
            rect.Width * transform.Scale, rect.Height * transform.Scale};
    }

    void Renderer::PushScale(double x, double y, double scale)
    {
        const double outer = Scale();
        const Transform base = _transforms.empty() ? Transform{x, y, 1.0}
                                                   : _transforms.back();
        // The new origin is where the point lands under the transform already
        // in force, and the scales multiply.
        _transforms.push_back(Transform{base.X + (x - base.X) * base.Scale,
            base.Y + (y - base.Y) * base.Scale, outer * scale});
    }

    void Renderer::PopScale()
    {
        if (!_transforms.empty())
        {
            _transforms.pop_back();
        }
    }

    bool Renderer::Clipped(Rect rect) const
    {
        if (_clips.empty())
        {
            return false;
        }
        const Rect& clip = _clips.back();
        return rect.X + rect.Width <= clip.X || rect.X >= clip.X + clip.Width
            || rect.Y + rect.Height <= clip.Y || rect.Y >= clip.Y + clip.Height;
    }

    void Renderer::PushClip(Rect rect)
    {
        Rect value = Map(rect);
        if (!_clips.empty())
        {
            const Rect& parent = _clips.back();
            const double left = std::max(value.X, parent.X);
            const double top = std::max(value.Y, parent.Y);
            const double right = std::min(value.X + value.Width, parent.X + parent.Width);
            const double bottom = std::min(value.Y + value.Height, parent.Y + parent.Height);
            value = Rect{left, top, std::max(0.0, right - left), std::max(0.0, bottom - top)};
        }
        _clips.push_back(value);
        Backend().SetClip(value);
    }

    void Renderer::PopClip()
    {
        if (_clips.empty())
        {
            return;
        }
        _clips.pop_back();
        if (_clips.empty())
        {
            Backend().SetClip(std::nullopt);
            return;
        }
        Backend().SetClip(_clips.back());
    }

    void Renderer::FillRect(Rect rect, Color color)
    {
        FillRoundedRect(rect, 0.0, color);
    }

    void Renderer::FillRoundedRect(Rect rect, double radius, Color color)
    {
        if (color.A <= 0.0F || rect.Width <= 0.0 || rect.Height <= 0.0
            || Clipped(Map(rect)))
        {
            return;
        }
        Backend().FillRoundedRect(Map(rect), radius * Scale(), color);
    }

    void Renderer::StrokeRoundedRect(
        Rect rect, double radius, Thickness thickness, Color color)
    {
        if (color.A <= 0.0F || Clipped(Map(rect)))
        {
            return;
        }
        (void)radius;
        const double scale = Scale();
        Backend().StrokeRect(Map(rect),
            Thickness{thickness.Left * scale, thickness.Top * scale,
                thickness.Right * scale, thickness.Bottom * scale},
            color);
    }

    void Renderer::DrawText(std::string_view text, Rect rect, double fontSize,
        FontWeight weight, Color color, TextAlignment align, bool wrap)
    {
        if (text.empty() || color.A <= 0.0F || Clipped(Map(rect)))
        {
            return;
        }
        // A scaled run is measured and rasterised at the size it is drawn, so
        // the glyphs stay sharp rather than being stretched.
        const double scale = Scale();
        const Rect mapped = Map(rect);
        fontSize *= scale;
        rect = mapped;
        const std::vector<std::string> lines
            = WrapLines(text, fontSize, weight, wrap ? rect.Width : 0.0);
        const double lineHeight = FontLineHeight(fontSize, weight);
        const double ascent = FontAscent(fontSize, weight);

        // Every glyph is in the one atlas, so a whole run is a single batch.
        std::vector<TexturedVertex> vertices;
        double y = rect.Y;
        for (const std::string& line : lines)
        {
            double width = 0.0;
            for (const char32_t code : Utf8ToUtf32(line))
            {
                const Glyph* const glyph = GetGlyph(code, fontSize, weight);
                width += glyph != nullptr ? glyph->Advance : fontSize * 0.5;
            }
            double x = rect.X;
            if (align == TextAlignment::Center)
            {
                x += (rect.Width - width) / 2.0;
            }
            else if (align == TextAlignment::Right)
            {
                x += rect.Width - width;
            }

            const double atlasSize = std::max(1, AtlasSize());
            for (const char32_t code : Utf8ToUtf32(line))
            {
                const Glyph* const glyph = GetGlyph(code, fontSize, weight);
                if (glyph == nullptr)
                {
                    continue;
                }
                if (glyph->Width > 0 && glyph->Height > 0)
                {
                    const float left = static_cast<float>(x + glyph->BearingX);
                    const float top = static_cast<float>(y + ascent - glyph->BearingY);
                    const float right = left + static_cast<float>(glyph->Width);
                    const float bottom = top + static_cast<float>(glyph->Height);
                    const float u0 = static_cast<float>(glyph->AtlasX / atlasSize);
                    const float v0 = static_cast<float>(glyph->AtlasY / atlasSize);
                    const float u1
                        = static_cast<float>((glyph->AtlasX + glyph->Width) / atlasSize);
                    const float v1
                        = static_cast<float>((glyph->AtlasY + glyph->Height) / atlasSize);
                    vertices.push_back(TexturedVertex{left, top, u0, v0});
                    vertices.push_back(TexturedVertex{right, top, u1, v0});
                    vertices.push_back(TexturedVertex{right, bottom, u1, v1});
                    vertices.push_back(TexturedVertex{left, bottom, u0, v1});
                }
                x += glyph->Advance;
            }
            y += lineHeight;
        }
        Backend().DrawTexturedQuads(AtlasTexture(), vertices, color);
    }

    void Renderer::DrawTextAt(std::string_view text, double x, double y,
        double fontSize, FontWeight weight, Color color)
    {
        // The rectangle is only a bound for the run, so it is in the caller's
        // own coordinates and DrawText maps it.
        const Size size = MeasureText(text, fontSize, weight, 0.0);
        DrawText(text, Rect{x, y, size.Width, size.Height}, fontSize, weight, color,
            TextAlignment::Left, false);
    }

    void Renderer::DrawImage(TextureHandle texture, Rect rect, double opacity)
    {
        DrawImage(texture, rect, Rect{0.0, 0.0, 1.0, 1.0}, opacity);
    }

    void Renderer::DrawImage(
        TextureHandle texture, Rect rect, Rect source, double opacity)
    {
        if (texture == 0 || rect.Width <= 0.0 || rect.Height <= 0.0
            || Clipped(Map(rect)))
        {
            return;
        }
        rect = Map(rect);
        const float u0 = static_cast<float>(source.X);
        const float v0 = static_cast<float>(source.Y);
        const float u1 = static_cast<float>(source.X + source.Width);
        const float v1 = static_cast<float>(source.Y + source.Height);
        const TexturedVertex vertices[4] = {
            {static_cast<float>(rect.X), static_cast<float>(rect.Y), u0, v0},
            {static_cast<float>(rect.X + rect.Width), static_cast<float>(rect.Y), u1, v0},
            {static_cast<float>(rect.X + rect.Width),
                static_cast<float>(rect.Y + rect.Height), u1, v1},
            {static_cast<float>(rect.X), static_cast<float>(rect.Y + rect.Height), u0, v1}};
        Backend().DrawTexturedQuads(texture, vertices,
            Color{1.0F, 1.0F, 1.0F, static_cast<float>(opacity)});
    }

    void Renderer::DrawElement(Element& element)
    {
        if (!element.Visible)
        {
            return;
        }
        const Rect bounds = element.Bounds();
        if (bounds.Width <= 0.0 || bounds.Height <= 0.0)
        {
            return;
        }

        if (element.Background.has_value())
        {
            FillRoundedRect(bounds, element.CornerRadius, *element.Background);
        }
        if (element.BorderColor.has_value())
        {
            StrokeRoundedRect(
                bounds, element.CornerRadius, element.BorderThickness, *element.BorderColor);
        }

        // A custom control draws itself, as Avalonia's Render does.
        if (element.Kind() == ElementKind::Custom && element.Behaviour != nullptr)
        {
            element.Behaviour->Render(element, *this);
        }

        if (element.Kind() == ElementKind::TextBlock && element.Text.has_value())
        {
            const Rect content{
                bounds.X + element.Padding.Left,
                bounds.Y + element.Padding.Top,
                std::max(0.0, bounds.Width - element.Padding.Horizontal()),
                std::max(0.0, bounds.Height - element.Padding.Vertical())};
            DrawText(*element.Text, content, element.FontSize, element.Weight,
                element.Foreground, element.Align, element.WrapText);
        }

        // Only a scroll viewer keeps its children inside itself.
        const bool clip = element.Kind() == ElementKind::ScrollViewer;
        if (clip)
        {
            PushClip(bounds);
        }
        const bool scaled = element.LayoutScale > 0.0 && element.LayoutScale != 1.0;
        if (scaled)
        {
            PushScale(bounds.X, bounds.Y, element.LayoutScale);
        }
        for (const ElementPtr& child : element.Children())
        {
            DrawElement(*child);
        }
        if (scaled)
        {
            PopScale();
        }
        if (clip)
        {
            PopClip();
        }
    }
}
