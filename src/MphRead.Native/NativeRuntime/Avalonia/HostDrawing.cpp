#include "HostDrawing.hpp"

#include "../Gui/Text.hpp"
#include "../Stb/Image.hpp"

#include <algorithm>
#include <deque>

namespace MphRead::NativeRuntime::Avalonia
{
    namespace
    {
        std::deque<FormattedRun>& Runs()
        {
            static std::deque<FormattedRun> runs;
            return runs;
        }
    }

    Toolkit::Color ToColor(Launcher::GuiColor color) noexcept
    {
        return Toolkit::Color{
            static_cast<float>(color.R) / 255.0F,
            static_cast<float>(color.G) / 255.0F,
            static_cast<float>(color.B) / 255.0F,
            static_cast<float>(color.A) / 255.0F};
    }

    Toolkit::Color ToColor(const Launcher::GuiBrush& brush) noexcept
    {
        Toolkit::Color color = ToColor(brush.Color);
        color.A = static_cast<float>(color.A * brush.Opacity);
        return color;
    }

    std::string Utf8(std::u16string_view text)
    {
        std::string result;
        result.reserve(text.size());
        for (std::size_t i = 0; i < text.size(); ++i)
        {
            char32_t code = text[i];
            if (code >= 0xD800 && code <= 0xDBFF && i + 1 < text.size()
                && text[i + 1] >= 0xDC00 && text[i + 1] <= 0xDFFF)
            {
                code = 0x10000 + ((code - 0xD800) << 10) + (text[i + 1] - 0xDC00);
                ++i;
            }
            if (code < 0x80)
            {
                result.push_back(static_cast<char>(code));
            }
            else if (code < 0x800)
            {
                result.push_back(static_cast<char>(0xC0 | (code >> 6)));
                result.push_back(static_cast<char>(0x80 | (code & 0x3F)));
            }
            else if (code < 0x10000)
            {
                result.push_back(static_cast<char>(0xE0 | (code >> 12)));
                result.push_back(static_cast<char>(0x80 | ((code >> 6) & 0x3F)));
                result.push_back(static_cast<char>(0x80 | (code & 0x3F)));
            }
            else
            {
                result.push_back(static_cast<char>(0xF0 | (code >> 18)));
                result.push_back(static_cast<char>(0x80 | ((code >> 12) & 0x3F)));
                result.push_back(static_cast<char>(0x80 | ((code >> 6) & 0x3F)));
                result.push_back(static_cast<char>(0x80 | (code & 0x3F)));
            }
        }
        return result;
    }

    std::u16string Utf16(std::string_view text)
    {
        std::u16string result;
        for (const char32_t code : Toolkit::Decode(text))
        {
            if (code < 0x10000)
            {
                result.push_back(static_cast<char16_t>(code));
                continue;
            }
            const char32_t value = code - 0x10000;
            result.push_back(static_cast<char16_t>(0xD800 + (value >> 10)));
            result.push_back(static_cast<char16_t>(0xDC00 + (value & 0x3FF)));
        }
        return result;
    }

    Launcher::TrackedTextFormattedText MakeFormattedText(std::u16string_view text,
        bool bold, double fontSize, Launcher::GuiColor color)
    {
        FormattedRun& run = Runs().emplace_back();
        run.Text = Utf8(text);
        run.FontSize = fontSize;
        run.Bold = bold;
        run.Color = color;
        const Toolkit::Size size = Toolkit::MeasureText(run.Text, fontSize,
            bold ? Toolkit::FontWeight::Bold : Toolkit::FontWeight::Normal, 0.0);
        return Launcher::TrackedTextFormattedText{&run, size.Width, size.Height};
    }

    FormattedRun* RunOf(const void* handle) noexcept
    {
        return const_cast<FormattedRun*>(static_cast<const FormattedRun*>(handle));
    }

    HostImage::HostImage(std::int32_t width, std::int32_t height,
        std::vector<std::uint8_t> rgba) noexcept
        : _width(width), _height(height), _rgba(std::move(rgba))
    {
    }

    HostImage::~HostImage()
    {
        if (_texture != 0)
        {
            Toolkit::Backend().DestroyTexture(_texture);
        }
    }

    Toolkit::TextureHandle HostImage::Texture()
    {
        if (_texture == 0 && !_rgba.empty())
        {
            _texture = Toolkit::Backend().CreateColorTexture(
                _width, _height, _rgba.data());
        }
        return _texture;
    }

    std::shared_ptr<HostImage> DecodeImage(std::span<const std::uint8_t> bytes)
    {
        const Image image = LoadPng(
            std::vector<std::uint8_t>(bytes.begin(), bytes.end()), 4);
        if (image.Width <= 0 || image.Height <= 0)
        {
            return nullptr;
        }
        return std::make_shared<HostImage>(image.Width, image.Height, image.Pixels);
    }

    Toolkit::Rect Surface::Map(Launcher::GuiRect rect) const noexcept
    {
        return Toolkit::Rect{
            _bounds.X + rect.X, _bounds.Y + rect.Y, rect.Width, rect.Height};
    }

    void Surface::FillRect(Launcher::GuiRect rect, Launcher::GuiColor color) const
    {
        _renderer.FillRect(Map(rect), ToColor(color));
    }

    void Surface::FillRounded(
        const Launcher::GuiRoundedRect& rect, Launcher::GuiColor color) const
    {
        _renderer.FillRoundedRect(
            Map(rect.Rect), rect.RadiiTopLeft.X, ToColor(color));
    }

    void Surface::StrokeRounded(const Launcher::GuiRoundedRect& rect,
        Launcher::GuiColor color, double thickness) const
    {
        _renderer.StrokeRoundedRect(Map(rect.Rect), rect.RadiiTopLeft.X,
            Toolkit::Thickness{thickness, thickness, thickness, thickness},
            ToColor(color));
    }

    void Surface::DrawEllipse(double centerX, double centerY, double radiusX,
        double radiusY, Launcher::GuiColor color) const
    {
        // A rounded rectangle whose corner radius is its own half-extent is
        // the ellipse, and for the round knobs in play it is the circle.
        const Launcher::GuiRect rect{centerX - radiusX, centerY - radiusY,
            radiusX * 2.0, radiusY * 2.0};
        _renderer.FillRoundedRect(
            Map(rect), std::min(radiusX, radiusY), ToColor(color));
    }

    void Surface::DrawFormatted(
        const Launcher::TrackedTextFormattedText& text, double x, double y) const
    {
        const FormattedRun* const run = RunOf(text.Native);
        if (run == nullptr)
        {
            return;
        }
        _renderer.DrawTextAt(run->Text, _bounds.X + x, _bounds.Y + y, run->FontSize,
            run->Bold ? Toolkit::FontWeight::Bold : Toolkit::FontWeight::Normal,
            ToColor(run->Color));
    }

    void Surface::PushClip(Launcher::GuiRect rect) const
    {
        _renderer.PushClip(Map(rect));
    }

    void Surface::PopClip() const
    {
        _renderer.PopClip();
    }
}
