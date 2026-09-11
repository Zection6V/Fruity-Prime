#pragma once

#include <cstdint>
#include <string>
#include <string_view>

namespace MphRead::Mods::Launcher::Gui
{
    enum class ProgressRowCulture : std::uint8_t
    {
        Invariant
    };

    enum class ProgressRowFlowDirection : std::uint8_t
    {
        LeftToRight
    };

    enum class ProgressRowFace : std::uint8_t
    {
        FaceFalse,
        FaceTrue
    };

    enum class ProgressRowBrush : std::uint8_t
    {
        TextDimBrush,
        TextBrush,
        Ink,
        AccentBrush
    };

    enum class ProgressRowPen : std::uint8_t
    {
        Null
    };

    struct ProgressRowBounds
    {
        double Width;
        double Height;
    };

    struct ProgressRowPoint
    {
        double X;
        double Y;
    };

    struct ProgressRowRect
    {
        double X;
        double Y;
        double Width;
        double Height;
    };

    struct ProgressRowRoundedRect
    {
        ProgressRowRect Rect;
        double Radius;
    };

    struct ProgressRowFormattedText
    {
        const void* Native;
        double Width;
        double Height;
    };

    class ProgressRowControlAdapter
    {
    public:
        virtual ~ProgressRowControlAdapter() = default;

        virtual void SetHeight(double height) = 0;
        virtual void SetIsVisible(bool isVisible) = 0;
        virtual void InvalidateVisual() = 0;
    };

    class ProgressRowDrawingContext
    {
    public:
        virtual ~ProgressRowDrawingContext() = default;

        virtual ProgressRowFormattedText CreateFormattedText(std::string_view text,
            ProgressRowCulture culture, ProgressRowFlowDirection flowDirection,
            ProgressRowFace face, double fontSize, ProgressRowBrush brush) = 0;
        virtual void DrawText(const ProgressRowFormattedText& text, ProgressRowPoint point) = 0;
        virtual void DrawRectangle(ProgressRowBrush brush, ProgressRowPen pen,
            ProgressRowRoundedRect rect) = 0;
    };

    class ProgressRow final
    {
    public:
        explicit ProgressRow(ProgressRowControlAdapter& control);

        void Set(double fraction, const std::string& stage);
        void Render(ProgressRowDrawingContext& context, ProgressRowBounds bounds);

    private:
        ProgressRowControlAdapter& _control;
        double _fraction = 0;
        std::string _stage;
    };
}
