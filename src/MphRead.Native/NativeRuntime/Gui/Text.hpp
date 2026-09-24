#pragma once

// Text measurement and glyph rasterisation for the launcher's controls. The
// font is the platform's own UI face, as Avalonia's Inter default is on the
// C# side.

#include "Backend.hpp"

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace MphRead::NativeRuntime::Gui
{
    // The size a run of text takes at this size and weight. A wrapWidth above
    // zero wraps on spaces, as TextBlock does with TextWrapping.Wrap.
    [[nodiscard]] Size MeasureText(
        std::string_view text, double fontSize, FontWeight weight, double wrapWidth);

    // The lines the same text breaks into.
    [[nodiscard]] std::vector<std::string> WrapLines(
        std::string_view text, double fontSize, FontWeight weight, double wrapWidth);

    struct Glyph final
    {
        // Where the glyph sits in the atlas texture, in pixels.
        std::int32_t AtlasX = 0;
        std::int32_t AtlasY = 0;
        std::int32_t Width = 0;
        std::int32_t Height = 0;
        // Where it sits relative to the pen, in pixels.
        std::int32_t BearingX = 0;
        std::int32_t BearingY = 0;
        double Advance = 0.0;
    };

    // The glyph for one code point, rasterised on first use.
    [[nodiscard]] const Glyph* GetGlyph(
        char32_t code, double fontSize, FontWeight weight);

    // The atlas texture every glyph lives in, and its size. Zero until the
    // first glyph is asked for.
    [[nodiscard]] TextureHandle AtlasTexture();
    [[nodiscard]] std::int32_t AtlasSize();

    // The baseline offset from the top of a line, and the line's height.
    [[nodiscard]] double FontAscent(double fontSize, FontWeight weight);
    [[nodiscard]] double FontLineHeight(double fontSize, FontWeight weight);

    // UTF-8 to code points.
}
