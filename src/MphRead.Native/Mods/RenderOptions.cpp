#include "RenderOptions.hpp"
#include "../NativeRuntime/System/Globalization.hpp"
#include "NativeRuntime/System/Globalization.hpp"

#include <bit>
#include <cstddef>
#include <cstdint>
#include <limits>

using ::MphRead::NativeRuntime::CharIsWhiteSpace;
using ::MphRead::NativeRuntime::IsNumberWhiteSpace;
using ::MphRead::NativeRuntime::StringTrimView;

namespace
{
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
    std::int32_t RenderOptions::_fieldOfView = RenderOptions::DefaultFov;
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

        const std::string_view text = StringTrimView(*value);
        if ((::MphRead::NativeRuntime::ToLowerInvariant(text) == "on")
            || (::MphRead::NativeRuntime::ToLowerInvariant(text) == "true")
            || (::MphRead::NativeRuntime::ToLowerInvariant(text) == "yes"))
        {
            return true;
        }
        if ((::MphRead::NativeRuntime::ToLowerInvariant(text) == "off")
            || (::MphRead::NativeRuntime::ToLowerInvariant(text) == "false")
            || (::MphRead::NativeRuntime::ToLowerInvariant(text) == "no"))
        {
            return false;
        }
        return fallback;
    }

    std::string_view RenderOptions::OnOff(bool value) noexcept
    {
        return value ? std::string_view{"on"} : std::string_view{"off"};
    }

    std::int32_t RenderOptions::FieldOfView() noexcept
    {
        return _fieldOfView;
    }

    void RenderOptions::FieldOfView(std::int32_t value) noexcept
    {
        _fieldOfView = ClampInt32(value, MinFov, MaxFov);
    }

    float RenderOptions::FovScale() noexcept
    {
        return static_cast<float>(_fieldOfView) / static_cast<float>(DefaultFov);
    }

    std::int32_t RenderOptions::ParseFov(
        std::optional<std::string_view> value, std::int32_t fallback) noexcept
    {
        std::int32_t parsed = 0;
        if (value.has_value() && TryParseInt32IntegerInvariant(*value, parsed))
        {
            return ClampInt32(parsed, MinFov, MaxFov);
        }
        return fallback;
    }

    std::int32_t RenderOptions::ParseScale(
        std::optional<std::string_view> value, std::int32_t fallback) noexcept
    {
        if (value.has_value())
        {
            std::string_view text = StringTrimView(*value);
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
            std::string_view text = StringTrimView(*value);
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
