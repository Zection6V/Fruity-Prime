#pragma once

// What Avalonia gives the launcher's controls when they draw: a DrawingContext
// and a FormattedText. The controls themselves are already ported; this is the
// surface underneath them, drawn by NativeRuntime/Gui's renderer.
//
// A control's Render is written in its own coordinates, with the origin at its
// top left, so a surface carries the offset of the control it belongs to.

#include "../System/Encoding.hpp"
#include "../Gui/Renderer.hpp"
#include "../../Mods/Launcher/Gui/GuiTheme.hpp"
#include "../../Mods/Launcher/Gui/TrackedText.hpp"

#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace MphRead::NativeRuntime::Avalonia
{
    namespace Toolkit = ::MphRead::NativeRuntime::Gui;
    namespace Launcher = ::MphRead::Mods::Launcher::Gui;

    [[nodiscard]] Toolkit::Color ToColor(Launcher::GuiColor color) noexcept;
    [[nodiscard]] Toolkit::Color ToColor(const Launcher::GuiBrush& brush) noexcept;


    // One measured run, kept for as long as the process runs: a FormattedText
    // is an opaque handle on the managed side and the controls hold them only
    // for the length of a Render.
    struct FormattedRun final
    {
        std::string Text;
        double FontSize = 0.0;
        bool Bold = false;
        Launcher::GuiColor Color{};
        double MaxTextWidth = 0.0;
        double MaxTextHeight = 0.0;
        bool Trim = false;
    };

    // Measures the run and returns the handle a TrackedTextFormattedText
    // carries, with the width and height Avalonia would have filled in.
    [[nodiscard]] Launcher::TrackedTextFormattedText MakeFormattedText(
        std::u16string_view text, bool bold, double fontSize,
        Launcher::GuiColor color);
    [[nodiscard]] FormattedRun* RunOf(const void* handle) noexcept;

    // The application icon's own pixels, which is all a window needs of it.
    struct IconPixels final
    {
        std::int32_t Width = 0;
        std::int32_t Height = 0;
        std::vector<std::uint8_t> Rgba;
    };

    // A decoded picture. The texture is made on the first draw rather than on
    // decoding, because a control may load its image while the tree is being
    // measured, which happens with no drawing surface current.
    class HostImage final
    {
    public:
        HostImage(std::int32_t width, std::int32_t height,
            std::vector<std::uint8_t> rgba) noexcept;
        ~HostImage();

        HostImage(const HostImage&) = delete;
        HostImage& operator=(const HostImage&) = delete;
        HostImage(HostImage&&) = delete;
        HostImage& operator=(HostImage&&) = delete;

        [[nodiscard]] Toolkit::TextureHandle Texture();
        [[nodiscard]] std::int32_t Width() const noexcept { return _width; }
        [[nodiscard]] std::int32_t Height() const noexcept { return _height; }

    private:
        std::int32_t _width = 0;
        std::int32_t _height = 0;
        std::vector<std::uint8_t> _rgba;
        Toolkit::TextureHandle _texture = 0;
    };

    // Decodes a PNG into one, or nothing when the bytes are not a picture.
    [[nodiscard]] std::shared_ptr<HostImage> DecodeImage(
        std::span<const std::uint8_t> bytes);

    // The drawing surface one control's Render writes to.
    class Surface final
    {
    public:
        Surface(Toolkit::Renderer& renderer, Toolkit::Rect bounds) noexcept
            : _renderer(renderer), _bounds(bounds)
        {
        }

        [[nodiscard]] Toolkit::Renderer& Renderer() const noexcept { return _renderer; }
        [[nodiscard]] Toolkit::Rect Bounds() const noexcept { return _bounds; }

        // A rectangle in the control's own coordinates, in the window's.
        [[nodiscard]] Toolkit::Rect Map(Launcher::GuiRect rect) const noexcept;

        void FillRect(Launcher::GuiRect rect, Launcher::GuiColor color) const;
        void FillRounded(const Launcher::GuiRoundedRect& rect,
            Launcher::GuiColor color) const;
        void StrokeRounded(const Launcher::GuiRoundedRect& rect,
            Launcher::GuiColor color, double thickness) const;
        void DrawEllipse(double centerX, double centerY, double radiusX,
            double radiusY, Launcher::GuiColor color) const;
        void DrawFormatted(const Launcher::TrackedTextFormattedText& text,
            double x, double y) const;
        void PushClip(Launcher::GuiRect rect) const;
        void PopClip() const;

    private:
        Toolkit::Renderer& _renderer;
        Toolkit::Rect _bounds;
    };
}
