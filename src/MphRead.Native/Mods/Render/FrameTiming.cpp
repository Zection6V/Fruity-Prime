#include "FrameTiming.hpp"
#include "../../NativeRuntime/System/Globalization.hpp"
#include "../../NativeRuntime/System/Managed.hpp"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <cstddef>
#include <limits>
#include <locale>
#include <string_view>

using ::MphRead::NativeRuntime::Int32TryParseCurrentCulture;
using ::MphRead::NativeRuntime::UncheckedAdd;

namespace MphRead::Mods
{
    class DebugLog
    {
    public:
        static void Line(std::string_view category, std::string_view message);
    };
}

namespace
{
    constexpr std::array<std::string_view, 25> DotNetWhiteSpaceUtf8{
        "\x09", "\x0A", "\x0B", "\x0C", "\x0D", "\x20",
        "\xC2\x85", "\xC2\xA0", "\xE1\x9A\x80",
        "\xE2\x80\x80", "\xE2\x80\x81", "\xE2\x80\x82", "\xE2\x80\x83",
        "\xE2\x80\x84", "\xE2\x80\x85", "\xE2\x80\x86", "\xE2\x80\x87",
        "\xE2\x80\x88", "\xE2\x80\x89", "\xE2\x80\x8A",
        "\xE2\x80\xA8", "\xE2\x80\xA9", "\xE2\x80\xAF",
        "\xE2\x81\x9F", "\xE3\x80\x80"
    };

    [[nodiscard]] std::string_view TrimLikeDotNet(std::string_view value) noexcept
    {
        bool removed = true;
        while (removed && !value.empty())
        {
            removed = false;
            for (const std::string_view whiteSpace : DotNetWhiteSpaceUtf8)
            {
                if (value.starts_with(whiteSpace))
                {
                    value.remove_prefix(whiteSpace.size());
                    removed = true;
                    break;
                }
            }
        }
        removed = true;
        while (removed && !value.empty())
        {
            removed = false;
            for (const std::string_view whiteSpace : DotNetWhiteSpaceUtf8)
            {
                if (value.ends_with(whiteSpace))
                {
                    value.remove_suffix(whiteSpace.size());
                    removed = true;
                    break;
                }
            }
        }
        return value;
    }

    [[nodiscard]] constexpr char FoldAscii(char value) noexcept
    {
        return value >= 'A' && value <= 'Z' ? static_cast<char>(value + ('a' - 'A')) : value;
    }

    [[nodiscard]] bool EqualsOrdinalIgnoreCaseAscii(
        std::string_view left, std::string_view right) noexcept
    {
        if (left.size() != right.size())
        {
            return false;
        }
        for (std::size_t index = 0; index < left.size(); index++)
        {
            const unsigned char unit = static_cast<unsigned char>(left[index]);
            if (unit >= 0x80U || FoldAscii(left[index]) != FoldAscii(right[index]))
            {
                return false;
            }
        }
        return true;
    }

    [[nodiscard]] std::string CurrentNumberDecimalSeparator()
    {
        try
        {
            const std::locale culture("");
            return std::string(1, std::use_facet<std::numpunct<char>>(culture).decimal_point());
        }
        catch (...)
        {
            // NumberFormatInfo defaults to "." when no culture-specific data is available.
            return ".";
        }
    }

    [[nodiscard]] std::string FormatDoubleLikeDotNetCustom(
        double value, std::int32_t fractionalDigits, const std::string& decimalSeparator)
    {
        // .NET custom Double formatting first creates at most 15 significant decimal digits,
        // correctly rounded to nearest/even, then the custom-format pass rounds that decimal
        // buffer again with digit >= '5'. Preserve that two-stage behavior for "0.00"/"0.0".
        if (std::isnan(value))
        {
            return "NaN";
        }
        if (std::isinf(value))
        {
            return std::signbit(value) ? "-Infinity" : "Infinity";
        }

        constexpr std::int32_t DotNetDoubleCustomPrecision = 15;
        constexpr std::int32_t ScientificFractionalDigits = DotNetDoubleCustomPrecision - 1;
        const bool negative = std::signbit(value);
        const double magnitude = std::abs(value);
        std::array<char, DotNetDoubleCustomPrecision> digits{};
        std::int32_t digitCount = 0;
        std::int32_t scale = 0;

        if (magnitude != 0.0)
        {
            // std::to_chars is locale-independent and its precision overload performs the
            // correctly-rounded decimal conversion needed for the 15-digit NumberBuffer stage.
            std::array<char, 64> buffer{};
            const auto converted = std::to_chars(
                buffer.data(), buffer.data() + buffer.size(), magnitude,
                std::chars_format::scientific, ScientificFractionalDigits);
            const char* const exponentMarker = std::find(buffer.data(), converted.ptr, 'e');

            for (const char* cursor = buffer.data(); cursor < exponentMarker; cursor++)
            {
                if (*cursor >= '0' && *cursor <= '9')
                {
                    digits[static_cast<std::size_t>(digitCount++)] = *cursor;
                }
            }

            const char* exponentCursor = exponentMarker + 1;
            bool negativeExponent = false;
            if (exponentCursor < converted.ptr && (*exponentCursor == '+' || *exponentCursor == '-'))
            {
                negativeExponent = *exponentCursor == '-';
                exponentCursor++;
            }
            std::int32_t exponent = 0;
            while (exponentCursor < converted.ptr)
            {
                exponent = exponent * 10 + (*exponentCursor - '0');
                exponentCursor++;
            }
            if (negativeExponent)
            {
                exponent = -exponent;
            }
            scale = exponent + 1;

            const std::int32_t roundPosition = scale + fractionalDigits;
            std::int32_t kept = 0;
            while (kept < roundPosition && kept < digitCount)
            {
                kept++;
            }

            if (kept == roundPosition && kept < digitCount
                && digits[static_cast<std::size_t>(kept)] >= '5')
            {
                while (kept > 0 && digits[static_cast<std::size_t>(kept - 1)] == '9')
                {
                    kept--;
                }
                if (kept > 0)
                {
                    digits[static_cast<std::size_t>(kept - 1)]++;
                }
                else
                {
                    scale++;
                    digits[0] = '1';
                    kept = 1;
                }
            }
            else
            {
                while (kept > 0 && digits[static_cast<std::size_t>(kept - 1)] == '0')
                {
                    kept--;
                }
            }

            digitCount = kept;
            if (digitCount == 0)
            {
                scale = 0;
            }
        }

        std::string result;
        if (negative)
        {
            result += '-';
        }

        if (scale > 0)
        {
            for (std::int32_t position = 0; position < scale; position++)
            {
                result += position < digitCount
                    ? digits[static_cast<std::size_t>(position)]
                    : '0';
            }
        }
        else
        {
            result += '0';
        }

        result += decimalSeparator;
        for (std::int32_t position = 0; position < fractionalDigits; position++)
        {
            const std::int32_t digitIndex = scale + position;
            result += digitIndex >= 0 && digitIndex < digitCount
                ? digits[static_cast<std::size_t>(digitIndex)]
                : '0';
        }
        return result;
    }
}

namespace MphRead::Mods::Render
{
    static_assert(sizeof(double) == 8 && std::numeric_limits<double>::is_iec559,
        "FrameTiming requires .NET Double-compatible IEEE-754 binary64.");
    static_assert(sizeof(std::int64_t) == 8,
        "FrameTiming requires .NET Int64-compatible 64-bit integers.");

    std::int32_t FrameTiming::_frameRateCap = FrameTiming::DisplayRate;
    bool FrameTiming::_active = false;
    double FrameTiming::_accumulator = 0.0;
    std::int32_t FrameTiming::_stepsThisFrame = 0;

    std::int64_t FrameTiming::_totalSteps = 0;
    std::int64_t FrameTiming::_totalFrames = 0;
    std::int64_t FrameTiming::_droppedSteps = 0;
    std::int64_t FrameTiming::_stalls = 0;
    std::array<std::int64_t, FrameTiming::MaxCatchUpSteps + 1> FrameTiming::_stepHistogram{};
    double FrameTiming::_measuredSimulationHz = 0.0;
    double FrameTiming::_measuredFrameHz = 0.0;

    double FrameTiming::_windowSeconds = 0.0;
    std::int64_t FrameTiming::_windowSteps = 0;
    std::int64_t FrameTiming::_windowFrames = 0;
    std::int32_t FrameTiming::_windowsSinceReport = 0;
    std::int64_t FrameTiming::_reportedDrops = 0;
    std::int64_t FrameTiming::_reportedStalls = 0;

    std::int32_t FrameTiming::FrameRateCap() noexcept
    {
        return _frameRateCap;
    }

    void FrameTiming::SetFrameRateCap(std::int32_t value) noexcept
    {
        _frameRateCap = value <= 0 ? DisplayRate : std::clamp(value, MinCap, MaxCap);
    }

    bool FrameTiming::Active() noexcept
    {
        return _active;
    }

    std::int32_t FrameTiming::StepsThisFrame() noexcept
    {
        return _stepsThisFrame;
    }

    std::int64_t FrameTiming::TotalSteps() noexcept
    {
        return _totalSteps;
    }

    std::int64_t FrameTiming::TotalFrames() noexcept
    {
        return _totalFrames;
    }

    std::int64_t FrameTiming::DroppedSteps() noexcept
    {
        return _droppedSteps;
    }

    std::int64_t FrameTiming::Stalls() noexcept
    {
        return _stalls;
    }

    std::array<std::int64_t, FrameTiming::MaxCatchUpSteps + 1>& FrameTiming::StepHistogram() noexcept
    {
        return _stepHistogram;
    }

    double FrameTiming::MeasuredSimulationHz() noexcept
    {
        return _measuredSimulationHz;
    }

    double FrameTiming::MeasuredFrameHz() noexcept
    {
        return _measuredFrameHz;
    }

    void FrameTiming::ResetDiagnostics() noexcept
    {
        _totalSteps = 0;
        _totalFrames = 0;
        _droppedSteps = 0;
        _stalls = 0;
        _stepHistogram.fill(0);
        _measuredSimulationHz = 0.0;
        _measuredFrameHz = 0.0;
        _windowSeconds = 0.0;
        _windowSteps = 0;
        _windowFrames = 0;
    }

    std::string FrameTiming::Describe()
    {
        const std::string decimalSeparator = CurrentNumberDecimalSeparator();
        std::string result = "sim ";
        result += FormatDoubleLikeDotNetCustom(_measuredSimulationHz, 2, decimalSeparator);
        result += " Hz / draw ";
        result += FormatDoubleLikeDotNetCustom(_measuredFrameHz, 1, decimalSeparator);
        result += " Hz, ";
        result += std::to_string(_totalSteps);
        result += " steps over ";
        result += std::to_string(_totalFrames);
        result += " frames, ";
        result += std::to_string(_droppedSteps);
        result += " dropped, ";
        result += std::to_string(_stalls);
        result += " stalls, steps per frame [";
        for (std::size_t index = 0; index < _stepHistogram.size(); index++)
        {
            if (index != 0)
            {
                result += ", ";
            }
            result += std::to_string(_stepHistogram[index]);
        }
        result += "], cap ";
        result += _frameRateCap == DisplayRate ? "display" : std::to_string(_frameRateCap);
        return result;
    }

    void FrameTiming::Reset() noexcept
    {
        _accumulator = 0.0;
        _stepsThisFrame = 0;
        _active = false;
    }

    std::int32_t FrameTiming::Advance(double elapsedSeconds)
    {
        _active = true;
        _totalFrames = UncheckedAdd(_totalFrames, 1);
        if (elapsedSeconds > StallSeconds || elapsedSeconds < 0.0 || std::isnan(elapsedSeconds))
        {
            _stalls = UncheckedAdd(_stalls, 1);
            _accumulator = 0.0;
            _stepsThisFrame = 1;
            _totalSteps = UncheckedAdd(_totalSteps, 1);
            _stepHistogram[1] = UncheckedAdd(_stepHistogram[1], 1);
            Tally(StepSeconds, 1);
            return 1;
        }

        _accumulator += elapsedSeconds;
        std::int32_t steps = 0;
        while (_accumulator >= StepSeconds && steps < MaxCatchUpSteps)
        {
            _accumulator -= StepSeconds;
            steps++;
        }
        if (_accumulator >= StepSeconds)
        {
            const std::int32_t owed = static_cast<std::int32_t>(_accumulator / StepSeconds);
            _droppedSteps = UncheckedAdd(_droppedSteps, owed);
            _accumulator -= owed * StepSeconds;
        }
        _stepsThisFrame = steps;
        _totalSteps = UncheckedAdd(_totalSteps, steps);
        const std::size_t histogramIndex = static_cast<std::size_t>(steps);
        _stepHistogram[histogramIndex] = UncheckedAdd(_stepHistogram[histogramIndex], 1);
        Tally(elapsedSeconds, steps);
        return steps;
    }

    void FrameTiming::Tally(double elapsedSeconds, std::int32_t steps)
    {
        _windowSeconds += elapsedSeconds;
        _windowSteps = UncheckedAdd(_windowSteps, steps);
        _windowFrames = UncheckedAdd(_windowFrames, 1);
        if (_windowSeconds >= 2.0)
        {
            _measuredSimulationHz = _windowSteps / _windowSeconds;
            _measuredFrameHz = _windowFrames / _windowSeconds;
            _windowSeconds = 0.0;
            _windowSteps = 0;
            _windowFrames = 0;
            ReportWindow();
        }
    }

    void FrameTiming::ReportWindow()
    {
        const bool trouble = _droppedSteps != _reportedDrops || _stalls != _reportedStalls
            || std::abs(_measuredSimulationHz - SimulationHz) > SimulationHz * 0.02;
        _windowsSinceReport++;
        if (!trouble && _windowsSinceReport < 5)
        {
            return;
        }
        _windowsSinceReport = 0;
        _reportedDrops = _droppedSteps;
        _reportedStalls = _stalls;
        MphRead::Mods::DebugLog::Line(trouble ? "frametiming!" : "frametiming", Describe());
    }

    std::int32_t FrameTiming::ParseCap(
        const std::optional<std::string>& value, std::int32_t fallback) noexcept
    {
        if (!value.has_value())
        {
            return fallback;
        }
        const std::string_view trimmed = TrimLikeDotNet(*value);
        if (trimmed.empty())
        {
            return fallback;
        }
        if (EqualsOrdinalIgnoreCaseAscii(trimmed, "display")
            || EqualsOrdinalIgnoreCaseAscii(trimmed, "vsync")
            || EqualsOrdinalIgnoreCaseAscii(trimmed, "auto")
            || trimmed == "0")
        {
            return DisplayRate;
        }
        if (EqualsOrdinalIgnoreCaseAscii(trimmed, "uncapped")
            || EqualsOrdinalIgnoreCaseAscii(trimmed, "unlimited"))
        {
            return MaxCap;
        }
        std::int32_t parsed = 0;
        if (Int32TryParseCurrentCulture(trimmed, parsed))
        {
            return parsed <= 0 ? DisplayRate : std::clamp(parsed, MinCap, MaxCap);
        }
        return fallback;
    }

    std::string FrameTiming::CapString(std::int32_t cap)
    {
        return cap == DisplayRate ? "display" : std::to_string(cap);
    }
}
