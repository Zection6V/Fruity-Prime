#include "ProgressRow.hpp"

#include <charconv>
#include <cmath>
#include <limits>
#include <string>

namespace
{
    [[nodiscard]] double MathClamp01(double value) noexcept
    {
        if (value < 0.0)
        {
            return 0.0;
        }
        if (value > 1.0)
        {
            return 1.0;
        }
        return value;
    }

    [[nodiscard]] double RoundToEven(double value) noexcept
    {
        if (!std::isfinite(value) || std::abs(value) >= 4503599627370496.0)
        {
            return value;
        }

        const double lower = std::floor(value);
        const double difference = value - lower;
        if (difference < 0.5)
        {
            return lower;
        }
        if (difference > 0.5)
        {
            return lower + 1.0;
        }

        const double half = lower / 2.0;
        return half == std::floor(half) ? lower : lower + 1.0;
    }

    [[nodiscard]] std::int32_t DoubleToInt32(double value) noexcept
    {
        if (std::isnan(value))
        {
            return 0;
        }
        if (value >= static_cast<double>(std::numeric_limits<std::int32_t>::max()))
        {
            return std::numeric_limits<std::int32_t>::max();
        }
        if (value <= static_cast<double>(std::numeric_limits<std::int32_t>::min()))
        {
            return std::numeric_limits<std::int32_t>::min();
        }
        return static_cast<std::int32_t>(value);
    }

    [[nodiscard]] std::string FormatInt32Invariant(std::int32_t value)
    {
        char buffer[16];
        const auto result = std::to_chars(buffer, buffer + sizeof(buffer), value);
        return std::string(buffer, result.ptr);
    }

    [[nodiscard]] double MathMax(double left, double right) noexcept
    {
        if (std::isnan(left))
        {
            return left;
        }
        if (std::isnan(right))
        {
            return right;
        }
        return left > right ? left : right;
    }
}

namespace MphRead::Mods::Launcher::Gui
{
    ProgressRow::ProgressRow(ProgressRowControlAdapter& control)
        : _control(control)
    {
        _control.SetHeight(44.0);
        _control.SetIsVisible(false);
    }

    void ProgressRow::Set(double fraction, const std::string& stage)
    {
        _fraction = MathClamp01(fraction);
        _stage = stage;
        _control.SetIsVisible(true);
        _control.InvalidateVisual();
    }

    void ProgressRow::Render(ProgressRowDrawingContext& context, ProgressRowBounds bounds)
    {
        const double width = bounds.Width;
        constexpr double barHeight = 8.0;
        const double barTop = bounds.Height - barHeight - 2.0;

        const ProgressRowFormattedText stage = context.CreateFormattedText(_stage,
            ProgressRowCulture::Invariant, ProgressRowFlowDirection::LeftToRight,
            ProgressRowFace::FaceFalse, 12.0, ProgressRowBrush::TextDimBrush);
        context.DrawText(stage, ProgressRowPoint{0.0, 2.0});

        const std::int32_t percentValue = DoubleToInt32(RoundToEven(_fraction * 100.0));
        const std::string percent = FormatInt32Invariant(percentValue) + "%";
        const ProgressRowFormattedText number = context.CreateFormattedText(percent,
            ProgressRowCulture::Invariant, ProgressRowFlowDirection::LeftToRight,
            ProgressRowFace::FaceTrue, 12.0, ProgressRowBrush::TextBrush);
        context.DrawText(number, ProgressRowPoint{width - number.Width, 2.0});

        context.DrawRectangle(ProgressRowBrush::Ink, ProgressRowPen::Null,
            ProgressRowRoundedRect{
                ProgressRowRect{0.0, barTop, width, barHeight},
                barHeight / 2.0
            });

        const double filled = width * _fraction;
        if (filled > 1.0)
        {
            context.DrawRectangle(ProgressRowBrush::AccentBrush, ProgressRowPen::Null,
                ProgressRowRoundedRect{
                    ProgressRowRect{0.0, barTop, MathMax(filled, barHeight), barHeight},
                    barHeight / 2.0
                });
        }
    }
}
