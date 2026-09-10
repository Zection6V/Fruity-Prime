#include "SetupProgress.hpp"

#include <bit>
#include <charconv>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <system_error>
#include <utility>

namespace
{
    constexpr std::string_view PreviewPrefix = "[thumbnails] ";

    [[nodiscard]] bool StartsWithOrdinal(std::string_view value, std::string_view prefix) noexcept
    {
        return value.size() >= prefix.size() && value.compare(0, prefix.size(), prefix) == 0;
    }

    [[nodiscard]] bool IsNumberStylesIntegerWhite(char value) noexcept
    {
        return value == ' ' || (value >= '\t' && value <= '\r');
    }

    [[nodiscard]] bool IsAsciiDigit(char value) noexcept
    {
        return value >= '0' && value <= '9';
    }

    [[nodiscard]] std::int32_t WrapAdd(std::int32_t left, std::int32_t right) noexcept
    {
        const std::uint32_t a = std::bit_cast<std::uint32_t>(left);
        const std::uint32_t b = std::bit_cast<std::uint32_t>(right);
        return std::bit_cast<std::int32_t>(a + b);
    }

    [[nodiscard]] std::int32_t WrapNegate(std::int32_t value) noexcept
    {
        const std::uint32_t bits = std::bit_cast<std::uint32_t>(value);
        return std::bit_cast<std::int32_t>(std::uint32_t{0} - bits);
    }

    [[nodiscard]] bool TryParseInt32(std::string_view text, std::int32_t& result) noexcept
    {
        result = 0;

        std::size_t begin = 0;
        while (begin < text.size() && IsNumberStylesIntegerWhite(text[begin]))
        {
            ++begin;
        }

        std::size_t end = text.size();
        while (end > begin && IsNumberStylesIntegerWhite(text[end - 1]))
        {
            --end;
        }
        if (begin == end)
        {
            return false;
        }

        bool negative = false;
        if (text[begin] == '+' || text[begin] == '-')
        {
            negative = text[begin] == '-';
            ++begin;
        }
        if (begin == end)
        {
            return false;
        }

        constexpr std::uint32_t PositiveLimit = static_cast<std::uint32_t>(std::numeric_limits<std::int32_t>::max());
        constexpr std::uint32_t NegativeLimit = PositiveLimit + 1U;
        const std::uint32_t limit = negative ? NegativeLimit : PositiveLimit;
        std::uint32_t value = 0;

        for (std::size_t index = begin; index < end; ++index)
        {
            const char character = text[index];
            if (!IsAsciiDigit(character))
            {
                return false;
            }
            const std::uint32_t digit = static_cast<std::uint32_t>(character - '0');
            if (value > (limit - digit) / 10U)
            {
                return false;
            }
            value = value * 10U + digit;
        }

        if (!negative)
        {
            result = static_cast<std::int32_t>(value);
        }
        else if (value == NegativeLimit)
        {
            result = std::numeric_limits<std::int32_t>::min();
        }
        else
        {
            result = -static_cast<std::int32_t>(value);
        }
        return true;
    }

    [[nodiscard]] double MathClamp(double value, double minimum, double maximum)
    {
        if (minimum > maximum)
        {
            throw std::invalid_argument("minimum is greater than maximum");
        }
        if (value < minimum)
        {
            return minimum;
        }
        if (value > maximum)
        {
            return maximum;
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

    [[nodiscard]] std::string FormatInt32(std::int32_t value)
    {
        char buffer[16];
        const auto [end, error] = std::to_chars(buffer, buffer + sizeof(buffer), value);
        if (error != std::errc{})
        {
            throw std::runtime_error("failed to format Int32");
        }
        return std::string(buffer, end);
    }
}

namespace MphRead::Mods::Launcher
{
    const SetupProgress::Band SetupProgress::_files{0.03, 0.40, 45, "Writing game files"};
    const SetupProgress::Band SetupProgress::_archives{0.40, 0.57, 25, "Unpacking archives"};
    const SetupProgress::Band SetupProgress::_sound{0.57, 0.63, 3, "Converting music"};
    const SetupProgress::Band SetupProgress::_binaries{0.63, 0.72, 6, "Decompressing code"};

    SetupProgress::Band::Band(double start, double end, double scale, std::string stage)
        : Start(start), End(end), Scale(scale), Stage(std::move(stage))
    {
    }

    double SetupProgress::Fraction() const noexcept
    {
        return _fraction;
    }

    const std::string& SetupProgress::Stage() const noexcept
    {
        return _stage;
    }

    bool SetupProgress::Done() const noexcept
    {
        return _done;
    }

    bool SetupProgress::Observe(const std::string& line)
    {
        if (_done)
        {
            return false;
        }

        std::int32_t done = 0;
        std::int32_t total = 0;
        if (TryPreviewCount(line, done, total) && total > 0)
        {
            _band = Band(
                _previewStart,
                1,
                1,
                "Rendering map previews (" + FormatInt32(done) + "/" + FormatInt32(total) + ")");
            return Set(
                _previewStart + (1 - _previewStart) * static_cast<double>(done) / static_cast<double>(total),
                _band.Stage);
        }

        Band next = Classify(line);
        if (next.Stage != _band.Stage)
        {
            _band = std::move(next);
            _seen = 0;
        }
        _seen = WrapAdd(_seen, 1);

        const double span = _band.End - _band.Start;
        const std::int32_t negatedSeen = WrapNegate(_seen);
        const double eased = 1 - std::exp(static_cast<double>(negatedSeen) / _band.Scale);
        return Set(_band.Start + span * eased, _band.Stage);
    }

    void SetupProgress::Finish(bool ok)
    {
        _done = true;
        _fraction = 1;
        _stage = ok ? "Ready to play" : "Setup did not finish";
    }

    bool SetupProgress::TryPreviewCount(std::string_view line, std::int32_t& done, std::int32_t& total)
    {
        done = 0;
        total = 0;
        if (!StartsWithOrdinal(line, PreviewPrefix))
        {
            return false;
        }

        const std::string_view rest = line.substr(PreviewPrefix.size());
        const std::size_t slash = rest.find('/');
        if (slash == std::string_view::npos || slash == 0)
        {
            return false;
        }

        const std::string_view after = rest.substr(slash + 1);
        std::size_t end = 0;
        while (end < after.size() && IsAsciiDigit(after[end]))
        {
            ++end;
        }

        return TryParseInt32(rest.substr(0, slash), done)
            && end > 0
            && TryParseInt32(after.substr(0, end), total);
    }

    SetupProgress::Band SetupProgress::Classify(std::string_view line) const
    {
        if (StartsWithOrdinal(line, "Writing "))
        {
            return _files;
        }
        if (StartsWithOrdinal(line, "Reading ") || StartsWithOrdinal(line, "Extracted "))
        {
            return _archives;
        }
        if (StartsWithOrdinal(line, "Converting "))
        {
            return _sound;
        }
        if (StartsWithOrdinal(line, "Decompressing "))
        {
            return _binaries;
        }
        return _band;
    }

    bool SetupProgress::Set(double fraction, const std::string& stage)
    {
        const double clamped = MathClamp(fraction, 0, 0.99);
        const bool changed = clamped > _fraction + 0.0005 || stage != _stage;
        if (clamped > _fraction)
        {
            _fraction = clamped;
        }
        _stage = stage;
        return changed;
    }

    std::string SetupProgress::Bar(std::int32_t width) const
    {
        const std::int32_t filled = static_cast<std::int32_t>(RoundToEven(_fraction * static_cast<double>(width)));
        const std::int32_t capacity = WrapAdd(width, 8);
        if (capacity < 0)
        {
            throw std::out_of_range("capacity");
        }

        std::string text;
        text.reserve(static_cast<std::size_t>(capacity));
        text.push_back('[');
        if (filled < 0)
        {
            throw std::out_of_range("repeatCount");
        }
        text.append(static_cast<std::size_t>(filled), '#');

        const std::int32_t remaining = width > filled ? width - filled : 0;
        text.append(static_cast<std::size_t>(remaining), '-');
        text.append("] ");

        const std::int32_t percent = static_cast<std::int32_t>(RoundToEven(_fraction * 100));
        const std::string percentText = FormatInt32(percent);
        if (percentText.size() < 3)
        {
            text.append(3 - percentText.size(), ' ');
        }
        text.append(percentText);
        text.push_back('%');
        return text;
    }
}
