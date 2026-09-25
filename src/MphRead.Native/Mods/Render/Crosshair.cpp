#include "Crosshair.hpp"
#include "../../NativeRuntime/System/Enum.hpp"
#include "../../NativeRuntime/System/Globalization.hpp"

#include <charconv>
#include <cmath>
#include <limits>
#include <type_traits>

using ::MphRead::NativeRuntime::Int32TryParseInvariant;
using ::MphRead::NativeRuntime::StringEqualsOrdinalIgnoreCase;
using ::MphRead::NativeRuntime::StringTrimView;

namespace MphRead::Mods::Render
{
    namespace
    {
        // Crosshair.cs CrosshairSize : int
        constexpr ::MphRead::NativeRuntime::EnumNameEntry CrosshairSizeNames[] = {
            {0ULL, "Small"},
            {1ULL, "Medium"},
            {2ULL, "Big"},
        };

        // Crosshair.cs CrosshairStyle : int
        constexpr ::MphRead::NativeRuntime::EnumNameEntry CrosshairStyleNames[] = {
            {0ULL, "Cross"},
            {1ULL, "Dot"},
            {2ULL, "CrossDot"},
            {3ULL, "Circle"},
            {4ULL, "Brackets"},
        };

        template <typename TEnum, std::size_t N>
        bool TryParseEnum(std::optional<std::string_view> value,
            const ::MphRead::NativeRuntime::EnumNameEntry (&names)[N], TEnum& parsed)
        {
            // Enum.TryParse(value, ignoreCase: true, out parsed); null is false.
            return value.has_value()
                && ::MphRead::NativeRuntime::ManagedEnumTryParse(*value, true, names, N, parsed);
        }
    }

    std::string ToString(CrosshairSize value)
    {
        return ::MphRead::NativeRuntime::ManagedEnumToString(
            value, CrosshairSizeNames, std::size(CrosshairSizeNames), false);
    }

    std::string ToString(CrosshairStyle value)
    {
        return ::MphRead::NativeRuntime::ManagedEnumToString(
            value, CrosshairStyleNames, std::size(CrosshairStyleNames), false);
    }

    CrosshairSize Crosshair::Size = CrosshairSize::Medium;
    CrosshairStyle Crosshair::Style = CrosshairStyle::Cross;

    std::array<std::string, 3> Crosshair::SizeNames =
    {
        "Small", "Medium", "Big"
    };

    std::array<std::string, 5> Crosshair::StyleNames =
    {
        "Cross", "Dot", "Cross + dot", "Circle", "Brackets"
    };

    float Crosshair::ScaleOf(CrosshairSize size)
    {
        switch (size)
        {
        case CrosshairSize::Small:
            return 0.7F;
        case CrosshairSize::Big:
            return 1.5F;
        default:
            return 1.0F;
        }
    }

    float Crosshair::Scale()
    {
        return ScaleOf(Size);
    }

    std::tuple<float, float> Crosshair::RingOf(CrosshairStyle style, float scale)
    {
        switch (style)
        {
        case CrosshairStyle::Circle:
            return {8.0F * scale, 2.0F * scale};
        default:
            return {0.0F, 0.0F};
        }
    }

    std::vector<CrosshairBar> Crosshair::BarsOf(CrosshairStyle style, float scale)
    {
        std::vector<CrosshairBar> bars;
        bars.reserve(8);
        switch (style)
        {
        case CrosshairStyle::Cross:
            AddCross(bars, 9.0F, 3.0F, 3.0F, scale);
            break;
        case CrosshairStyle::Dot:
            bars.push_back(Dot(4.0F, scale));
            break;
        case CrosshairStyle::CrossDot:
            AddCross(bars, 8.0F, 3.0F, 5.0F, scale);
            bars.push_back(Dot(3.0F, scale));
            break;
        case CrosshairStyle::Circle:
            break;
        case CrosshairStyle::Brackets:
            AddBrackets(bars, 10.0F, 6.0F, 2.0F, scale);
            break;
        }
        return bars;
    }

    CrosshairBar Crosshair::Dot(float side, float scale)
    {
        return CrosshairBar(0.0F, 0.0F, side * scale, side * scale);
    }

    void Crosshair::AddCross(std::vector<CrosshairBar>& bars, float arm, float thickness,
        float gap, float scale)
    {
        const float offset = (gap + arm / 2.0F) * scale;
        const float longSide = arm * scale;
        const float shortSide = thickness * scale;
        bars.emplace_back(0.0F, offset, shortSide, longSide);
        bars.emplace_back(0.0F, -offset, shortSide, longSide);
        bars.emplace_back(-offset, 0.0F, longSide, shortSide);
        bars.emplace_back(offset, 0.0F, longSide, shortSide);
    }

    void Crosshair::AddBrackets(std::vector<CrosshairBar>& bars, float corner, float length,
        float thickness, float scale)
    {
        const float c = corner * scale;
        const float len = length * scale;
        const float thick = thickness * scale;
        for (std::int32_t i = 0; i < 4; i++)
        {
            const float sx = (i & 1) == 0 ? -1.0F : 1.0F;
            const float sy = (i & 2) == 0 ? 1.0F : -1.0F;
            bars.emplace_back(sx * (c - len / 2.0F + thick / 2.0F),
                sy * (c - thick / 2.0F), len, thick);
            bars.emplace_back(sx * (c - thick / 2.0F),
                sy * (c - len / 2.0F - thick / 2.0F), thick, len);
        }
    }

    std::tuple<float, float, float, float> Crosshair::EdgesOf(CrosshairBar bar)
    {
        return {
            Snap(bar.X - bar.Width / 2.0F, true),
            Snap(bar.X + bar.Width / 2.0F, false),
            Snap(bar.Y - bar.Height / 2.0F, true),
            Snap(bar.Y + bar.Height / 2.0F, false)
        };
    }

    float Crosshair::Snap(float value, bool down)
    {
        return down ? std::floor(value) : std::ceil(value);
    }

    CrosshairSize Crosshair::ParseSize(
        std::optional<std::string_view> value, CrosshairSize fallback)
    {
        CrosshairSize parsed = CrosshairSize::Small;
        return TryParseEnum(value, CrosshairSizeNames, parsed) ? parsed : fallback;
    }

    CrosshairStyle Crosshair::ParseStyle(
        std::optional<std::string_view> value, CrosshairStyle fallback)
    {
        CrosshairStyle parsed = CrosshairStyle::Cross;
        return TryParseEnum(value, CrosshairStyleNames, parsed) ? parsed : fallback;
    }
}
