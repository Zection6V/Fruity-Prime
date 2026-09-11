#pragma once

#include <cstdint>
#include <string_view>

namespace MphRead::Mods::Launcher::Gui
{
    enum class TrackedTextCulture : std::uint8_t
    {
        Invariant
    };

    enum class TrackedTextFlowDirection : std::uint8_t
    {
        LeftToRight
    };

    enum class TrackedTextFace : std::uint8_t
    {
        FaceFalse,
        FaceTrue
    };

    struct TrackedTextBrush
    {
        const void* Native;
    };

    struct TrackedTextPoint
    {
        double X;
        double Y;
    };

    struct TrackedTextFormattedText
    {
        const void* Native;
        double Width;
        double Height;
    };

    class TrackedTextAdapter
    {
    public:
        explicit TrackedTextAdapter(TrackedTextBrush textBrush) noexcept
            : TextBrush(textBrush)
        {
        }

        virtual ~TrackedTextAdapter() = default;

        const TrackedTextBrush TextBrush;

        virtual TrackedTextFormattedText CreateFormattedText(std::u16string_view text,
            TrackedTextCulture culture, TrackedTextFlowDirection flowDirection,
            TrackedTextFace face, double fontSize, TrackedTextBrush brush) = 0;
        virtual void DrawText(const TrackedTextFormattedText& text, TrackedTextPoint point) = 0;
    };

    class TrackedText final
    {
    public:
        static TrackedTextFormattedText Make(TrackedTextAdapter& adapter,
            std::u16string_view text, double size, bool bold, TrackedTextBrush brush);

        static void Draw(TrackedTextAdapter& adapter, std::u16string_view text,
            double size, TrackedTextBrush brush, double x, double y, double tracking);

        static double Measure(TrackedTextAdapter& adapter, std::u16string_view text,
            double size, double tracking);

        static double LineHeight(TrackedTextAdapter& adapter, double size);
        static double SpaceWidth(TrackedTextAdapter& adapter, double size);

    private:
        TrackedText() = delete;
    };
}
