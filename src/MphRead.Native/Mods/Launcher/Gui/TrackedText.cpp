#include "TrackedText.hpp"

#include <cmath>

namespace
{
    double MathMax(double val1, double val2) noexcept
    {
        if (val1 != val2)
        {
            if (!std::isnan(val1))
            {
                return val2 < val1 ? val1 : val2;
            }
            return val1;
        }

        return std::signbit(val2) ? val1 : val2;
    }
}

namespace MphRead::Mods::Launcher::Gui
{
    TrackedTextFormattedText TrackedText::Make(TrackedTextAdapter& adapter,
        std::u16string_view text, double size, bool bold, TrackedTextBrush brush)
    {
        return adapter.CreateFormattedText(text, TrackedTextCulture::Invariant,
            TrackedTextFlowDirection::LeftToRight,
            bold ? TrackedTextFace::FaceTrue : TrackedTextFace::FaceFalse,
            size, brush);
    }

    void TrackedText::Draw(TrackedTextAdapter& adapter, std::u16string_view text,
        double size, TrackedTextBrush brush, double x, double y, double tracking)
    {
        const double space = SpaceWidth(adapter, size);
        double pen = x;
        for (const char16_t c : text)
        {
            if (c == u' ')
            {
                pen += space + tracking;
                continue;
            }

            const char16_t glyphText[] = { c };
            const TrackedTextFormattedText glyph = Make(adapter,
                std::u16string_view(glyphText, 1), size, true, brush);
            adapter.DrawText(glyph, TrackedTextPoint{pen, y});
            pen += glyph.Width + tracking;
        }
    }

    double TrackedText::Measure(TrackedTextAdapter& adapter, std::u16string_view text,
        double size, double tracking)
    {
        const double space = SpaceWidth(adapter, size);
        double width = 0;
        for (const char16_t c : text)
        {
            if (c == u' ')
            {
                width += space + tracking;
            }
            else
            {
                const char16_t glyphText[] = { c };
                const TrackedTextFormattedText glyph = Make(adapter,
                    std::u16string_view(glyphText, 1), size, true, adapter.TextBrush);
                width += glyph.Width + tracking;
            }
        }
        return width;
    }

    double TrackedText::LineHeight(TrackedTextAdapter& adapter, double size)
    {
        return Make(adapter, std::u16string_view(u"X"), size, true,
            adapter.TextBrush).Height;
    }

    double TrackedText::SpaceWidth(TrackedTextAdapter& adapter, double size)
    {
        const double pair = Make(adapter, std::u16string_view(u"nn"), size, true,
            adapter.TextBrush).Width;
        const double spaced = Make(adapter, std::u16string_view(u"n n"), size, true,
            adapter.TextBrush).Width;
        return MathMax(spaced - pair, size * 0.22);
    }
}
