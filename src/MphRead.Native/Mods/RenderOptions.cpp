#include "RenderOptions.hpp"

#include <bit>
#include <cstddef>
#include <cstdint>
#include <limits>

namespace
{
    constexpr bool IsDotNetWhiteSpace(char32_t value) noexcept
    {
        return (value >= U'\u0009' && value <= U'\u000D')
            || value == U'\u0020'
            || value == U'\u0085'
            || value == U'\u00A0'
            || value == U'\u1680'
            || (value >= U'\u2000' && value <= U'\u200A')
            || value == U'\u2028'
            || value == U'\u2029'
            || value == U'\u202F'
            || value == U'\u205F'
            || value == U'\u3000';
    }

    bool DecodeFirst(std::string_view text, char32_t& value,
        std::size_t& length) noexcept
    {
        if (text.empty())
        {
            return false;
        }

        const auto first = static_cast<unsigned char>(text.front());
        if (first < 0x80)
        {
            value = first;
            length = 1;
            return true;
        }

        std::size_t expected = 0;
        char32_t codePoint = 0;
        if ((first & 0xE0) == 0xC0)
        {
            expected = 2;
            codePoint = first & 0x1F;
        }
        else if ((first & 0xF0) == 0xE0)
        {
            expected = 3;
            codePoint = first & 0x0F;
        }
        else if ((first & 0xF8) == 0xF0)
        {
            expected = 4;
            codePoint = first & 0x07;
        }
        else
        {
            return false;
        }

        if (text.size() < expected)
        {
            return false;
        }
        for (std::size_t index = 1; index < expected; ++index)
        {
            const auto next = static_cast<unsigned char>(text[index]);
            if ((next & 0xC0) != 0x80)
            {
                return false;
            }
            codePoint = (codePoint << 6) | (next & 0x3F);
        }

        if ((expected == 2 && codePoint < 0x80)
            || (expected == 3 && codePoint < 0x800)
            || (expected == 4 && codePoint < 0x10000)
            || codePoint > 0x10FFFF
            || (codePoint >= 0xD800 && codePoint <= 0xDFFF))
        {
            return false;
        }

        value = codePoint;
        length = expected;
        return true;
    }

    bool DecodeLast(std::string_view text, char32_t& value,
        std::size_t& length) noexcept
    {
        if (text.empty())
        {
            return false;
        }

        std::size_t start = text.size() - 1;
        std::size_t continuation = 0;
        while (start > 0
            && (static_cast<unsigned char>(text[start]) & 0xC0) == 0x80
            && continuation < 3)
        {
            --start;
            ++continuation;
        }

        std::size_t decodedLength = 0;
        if (!DecodeFirst(text.substr(start), value, decodedLength)
            || start + decodedLength != text.size())
        {
            return false;
        }
        length = decodedLength;
        return true;
    }

    std::string_view Trim(std::string_view text) noexcept
    {
        while (!text.empty())
        {
            char32_t value = 0;
            std::size_t length = 0;
            if (!DecodeFirst(text, value, length) || !IsDotNetWhiteSpace(value))
            {
                break;
            }
            text.remove_prefix(length);
        }
        while (!text.empty())
        {
            char32_t value = 0;
            std::size_t length = 0;
            if (!DecodeLast(text, value, length) || !IsDotNetWhiteSpace(value))
            {
                break;
            }
            text.remove_suffix(length);
        }
        return text;
    }

    bool EqualsLowerInvariantAscii(std::string_view text,
        std::string_view expected) noexcept
    {
        if (text.size() != expected.size())
        {
            return false;
        }
        for (std::size_t index = 0; index < text.size(); ++index)
        {
            unsigned char value = static_cast<unsigned char>(text[index]);
            if (value >= 'A' && value <= 'Z')
            {
                value = static_cast<unsigned char>(value + ('a' - 'A'));
            }
            if (value != static_cast<unsigned char>(expected[index]))
            {
                return false;
            }
        }
        return true;
    }

    constexpr bool IsNumberWhiteSpace(unsigned char value) noexcept
    {
        return value == 0x20 || (value >= 0x09 && value <= 0x0D);
    }

    bool TryParseInt32IntegerInvariant(std::string_view text,
        std::int32_t& result) noexcept
    {
        while (!text.empty()
            && IsNumberWhiteSpace(static_cast<unsigned char>(text.front())))
        {
            text.remove_prefix(1);
        }
        if (text.empty())
        {
            return false;
        }

        bool negative = false;
        if (text.front() == '+' || text.front() == '-')
        {
            negative = text.front() == '-';
            text.remove_prefix(1);
        }
        if (text.empty())
        {
            return false;
        }

        std::uint32_t magnitude = 0;
        const std::uint32_t limit = negative
            ? std::uint32_t{2147483648u}
            : std::uint32_t{2147483647u};
        bool sawDigit = false;

        std::size_t index = 0;
        for (; index < text.size(); ++index)
        {
            const unsigned char value = static_cast<unsigned char>(text[index]);
            if (value < '0' || value > '9')
            {
                break;
            }
            sawDigit = true;
            const std::uint32_t digit = value - '0';
            if (magnitude > (limit - digit) / 10)
            {
                return false;
            }
            magnitude = magnitude * 10 + digit;
        }

        if (!sawDigit)
        {
            return false;
        }

        while (index < text.size()
            && IsNumberWhiteSpace(static_cast<unsigned char>(text[index])))
        {
            ++index;
        }
        while (index < text.size() && text[index] == '\0')
        {
            ++index;
        }
        if (index != text.size())
        {
            return false;
        }

        if (negative)
        {
            if (magnitude == 2147483648u)
            {
                result = std::numeric_limits<std::int32_t>::min();
            }
            else
            {
                result = -static_cast<std::int32_t>(magnitude);
            }
        }
        else
        {
            result = static_cast<std::int32_t>(magnitude);
        }
        return true;
    }

    std::int32_t ClampInt32(std::int32_t value, std::int32_t minimum,
        std::int32_t maximum) noexcept
    {
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
}

namespace MphRead::Mods
{
    std::int32_t RenderOptions::_resolutionScale = 100;
    bool RenderOptions::_lighting = true;
    bool RenderOptions::_celShading = false;
    bool RenderOptions::_showFps = false;
    std::int32_t RenderOptions::_celBands = 8;
    float RenderOptions::_celEdge = 0.5f;
    bool RenderOptions::_fog = true;
    bool RenderOptions::_textureFiltering = false;

    std::int32_t RenderOptions::ResolutionScale() noexcept
    {
        return _resolutionScale;
    }

    void RenderOptions::ResolutionScale(std::int32_t value) noexcept
    {
        _resolutionScale = ClampInt32(value, MinScale, 100);
    }

    bool RenderOptions::Lighting() noexcept
    {
        return _lighting;
    }

    void RenderOptions::Lighting(bool value) noexcept
    {
        _lighting = value;
    }

    bool RenderOptions::CelShading() noexcept
    {
        return _celShading;
    }

    void RenderOptions::CelShading(bool value) noexcept
    {
        _celShading = value;
    }

    bool RenderOptions::ShowFps() noexcept
    {
        return _showFps;
    }

    void RenderOptions::ShowFps(bool value) noexcept
    {
        _showFps = value;
    }

    std::int32_t RenderOptions::CelBands() noexcept
    {
        return _celBands;
    }

    void RenderOptions::CelBands(std::int32_t value) noexcept
    {
        _celBands = ClampInt32(value, 2, 8);
    }

    float RenderOptions::CelEdge() noexcept
    {
        return _celEdge;
    }

    void RenderOptions::CelEdge(float value) noexcept
    {
        if (value < 0.0f)
        {
            _celEdge = 0.0f;
        }
        else if (value > 1.0f)
        {
            _celEdge = 1.0f;
        }
        else
        {
            _celEdge = value;
        }
    }

    bool RenderOptions::Fog() noexcept
    {
        return _fog;
    }

    void RenderOptions::Fog(bool value) noexcept
    {
        _fog = value;
    }

    bool RenderOptions::TextureFiltering() noexcept
    {
        return _textureFiltering;
    }

    void RenderOptions::TextureFiltering(bool value) noexcept
    {
        _textureFiltering = value;
    }

    std::int32_t RenderOptions::Scaled(std::int32_t pixels) noexcept
    {
        if (_resolutionScale >= 100)
        {
            return pixels;
        }

        const std::uint32_t productBits = static_cast<std::uint32_t>(pixels)
            * static_cast<std::uint32_t>(_resolutionScale);
        const std::int32_t product = std::bit_cast<std::int32_t>(productBits);
        const std::int32_t scaled = product / 100;
        return scaled > 1 ? scaled : 1;
    }

    bool RenderOptions::ParseOnOff(std::optional<std::string_view> value,
        bool fallback) noexcept
    {
        if (!value.has_value())
        {
            return fallback;
        }

        const std::string_view text = Trim(*value);
        if (EqualsLowerInvariantAscii(text, "on")
            || EqualsLowerInvariantAscii(text, "true")
            || EqualsLowerInvariantAscii(text, "yes"))
        {
            return true;
        }
        if (EqualsLowerInvariantAscii(text, "off")
            || EqualsLowerInvariantAscii(text, "false")
            || EqualsLowerInvariantAscii(text, "no"))
        {
            return false;
        }
        return fallback;
    }

    std::string_view RenderOptions::OnOff(bool value) noexcept
    {
        return value ? std::string_view{"on"} : std::string_view{"off"};
    }

    std::int32_t RenderOptions::ParseScale(
        std::optional<std::string_view> value, std::int32_t fallback) noexcept
    {
        if (value.has_value())
        {
            std::string_view text = Trim(*value);
            while (!text.empty() && text.back() == '%')
            {
                text.remove_suffix(1);
            }

            std::int32_t percent = 0;
            if (TryParseInt32IntegerInvariant(text, percent))
            {
                return ClampInt32(percent, MinScale, 100);
            }
        }
        return fallback;
    }

    std::int32_t RenderOptions::ParseInt(
        std::optional<std::string_view> value, std::int32_t fallback) noexcept
    {
        if (value.has_value())
        {
            std::string_view text = Trim(*value);
            while (!text.empty() && text.back() == '%')
            {
                text.remove_suffix(1);
            }

            std::int32_t number = 0;
            if (TryParseInt32IntegerInvariant(text, number))
            {
                return number;
            }
        }
        return fallback;
    }
}
