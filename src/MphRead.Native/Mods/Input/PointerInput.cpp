#include "PointerInput.hpp"

#include <bit>
#include <charconv>
#include <cmath>
#include <cstdint>
#include <string>
#include <string_view>

// DebugLog has not been ported yet. Keep its only dependency in this translation
// unit; there is deliberately no fallback logger or substitute behavior.
namespace MphRead::Mods::Input::PointerInputAdapters
{
    void DebugLogLine(std::string_view category, std::string_view message);
}

namespace
{
    std::string FormatCSharpDelta0(float value)
    {
        if (std::isnan(value))
        {
            return "NaN";
        }
        if (std::isinf(value))
        {
            return std::signbit(value) ? "-Infinity" : "Infinity";
        }

        const bool negative = std::signbit(value);
        const float magnitude = std::fabs(value);

        char buffer[64]{};
        const auto conversion = std::to_chars(
            buffer, buffer + sizeof(buffer), magnitude, std::chars_format::general, 7);
        std::string_view formatted(buffer, static_cast<std::size_t>(conversion.ptr - buffer));
        std::size_t exponentPosition = formatted.find_first_of("eE");
        std::string_view mantissa = formatted.substr(0, exponentPosition);
        int exponent = 0;
        if (exponentPosition != std::string_view::npos)
        {
            std::string_view exponentText = formatted.substr(exponentPosition + 1);
            bool negativeExponent = false;
            if (!exponentText.empty() && (exponentText.front() == '+' || exponentText.front() == '-'))
            {
                negativeExponent = exponentText.front() == '-';
                exponentText.remove_prefix(1);
            }
            for (char ch : exponentText)
            {
                exponent = exponent * 10 + (ch - '0');
            }
            if (negativeExponent)
            {
                exponent = -exponent;
            }
        }

        const std::size_t decimalPoint = mantissa.find('.');
        const std::size_t digitsBeforePoint = decimalPoint == std::string_view::npos
            ? mantissa.size()
            : decimalPoint;

        std::string digits;
        digits.reserve(mantissa.size());
        for (char ch : mantissa)
        {
            if (ch != '.')
            {
                digits.push_back(ch);
            }
        }

        const int decimalPosition = static_cast<int>(digitsBeforePoint) + exponent;
        std::string integerPart;
        bool roundUp = false;

        if (decimalPosition <= 0)
        {
            integerPart = "0";
            if (decimalPosition == 0 && !digits.empty())
            {
                roundUp = digits.front() >= '5';
            }
        }
        else
        {
            const std::size_t integerDigits = static_cast<std::size_t>(decimalPosition);
            if (integerDigits >= digits.size())
            {
                integerPart = digits;
                integerPart.append(integerDigits - digits.size(), '0');
            }
            else
            {
                integerPart.assign(digits.data(), integerDigits);
                roundUp = digits[integerDigits] >= '5';
            }
        }

        if (roundUp)
        {
            std::size_t index = integerPart.size();
            while (index > 0 && integerPart[index - 1] == '9')
            {
                integerPart[index - 1] = '0';
                --index;
            }
            if (index == 0)
            {
                integerPart.insert(integerPart.begin(), '1');
            }
            else
            {
                ++integerPart[index - 1];
            }
        }

        if (negative && integerPart != "0")
        {
            integerPart.insert(integerPart.begin(), '-');
        }
        return integerPart;
    }
}

namespace MphRead::Mods::Input
{
    float PointerInput::_jumpPixels = 600.0F;
    bool PointerInput::_guardJumps = true;
    std::int32_t PointerInput::_jumpsIgnored = 0;
    bool PointerInput::_jumpingPointerSeen = false;

    float PointerInput::JumpPixels() noexcept
    {
        return _jumpPixels;
    }

    void PointerInput::JumpPixels(float value) noexcept
    {
        _jumpPixels = value;
    }

    bool PointerInput::GuardJumps() noexcept
    {
        return _guardJumps;
    }

    void PointerInput::GuardJumps(bool value) noexcept
    {
        _guardJumps = value;
    }

    std::int32_t PointerInput::JumpsIgnored() noexcept
    {
        return _jumpsIgnored;
    }

    bool PointerInput::JumpingPointerSeen() noexcept
    {
        return _jumpingPointerSeen;
    }

    float PointerInput::Filter(float delta)
    {
        if (!GuardJumps() || JumpPixels() <= 0.0F)
        {
            return delta;
        }
        if (std::fabs(delta) < JumpPixels())
        {
            return delta;
        }

        const std::uint32_t incremented = static_cast<std::uint32_t>(_jumpsIgnored) + 1U;
        _jumpsIgnored = std::bit_cast<std::int32_t>(incremented);
        if (!JumpingPointerSeen())
        {
            _jumpingPointerSeen = true;
            std::string message = "pointer jumped ";
            message += FormatCSharpDelta0(delta);
            message += " px in a frame and was ignored -- a pen, a touchscreen, or a cursor warp";
            PointerInputAdapters::DebugLogLine("input", message);
        }
        return 0.0F;
    }

    void PointerInput::Reset() noexcept
    {
        _jumpsIgnored = 0;
        _jumpingPointerSeen = false;
    }
}
