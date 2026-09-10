#include "Crosshair.hpp"

#include <charconv>
#include <cmath>
#include <limits>
#include <type_traits>

namespace MphRead::Mods::Render
{
    namespace
    {
        std::size_t DotNetWhitespacePrefixLength(std::string_view value)
        {
            if (value.empty())
            {
                return 0;
            }

            const auto byte = [](char ch) { return static_cast<unsigned char>(ch); };
            const unsigned char c0 = byte(value[0]);
            if ((c0 >= 0x09 && c0 <= 0x0D) || c0 == 0x20)
            {
                return 1;
            }
            if (value.size() >= 2 && c0 == 0xC2)
            {
                const unsigned char c1 = byte(value[1]);
                if (c1 == 0x85 || c1 == 0xA0)
                {
                    return 2;
                }
            }
            if (value.size() >= 3)
            {
                const unsigned char c1 = byte(value[1]);
                const unsigned char c2 = byte(value[2]);
                if (c0 == 0xE1 && c1 == 0x9A && c2 == 0x80)
                {
                    return 3;
                }
                if (c0 == 0xE2 && c1 == 0x80
                    && ((c2 >= 0x80 && c2 <= 0x8A) || c2 == 0xA8 || c2 == 0xA9 || c2 == 0xAF))
                {
                    return 3;
                }
                if (c0 == 0xE2 && c1 == 0x81 && c2 == 0x9F)
                {
                    return 3;
                }
                if (c0 == 0xE3 && c1 == 0x80 && c2 == 0x80)
                {
                    return 3;
                }
            }
            return 0;
        }

        std::size_t DotNetWhitespaceSuffixLength(std::string_view value)
        {
            if (value.empty())
            {
                return 0;
            }

            const auto byte = [](char ch) { return static_cast<unsigned char>(ch); };
            const unsigned char last = byte(value.back());
            if ((last >= 0x09 && last <= 0x0D) || last == 0x20)
            {
                return 1;
            }
            if (value.size() >= 2 && byte(value[value.size() - 2]) == 0xC2
                && (last == 0x85 || last == 0xA0))
            {
                return 2;
            }
            if (value.size() >= 3)
            {
                const unsigned char c0 = byte(value[value.size() - 3]);
                const unsigned char c1 = byte(value[value.size() - 2]);
                if (c0 == 0xE1 && c1 == 0x9A && last == 0x80)
                {
                    return 3;
                }
                if (c0 == 0xE2 && c1 == 0x80
                    && ((last >= 0x80 && last <= 0x8A) || last == 0xA8 || last == 0xA9 || last == 0xAF))
                {
                    return 3;
                }
                if (c0 == 0xE2 && c1 == 0x81 && last == 0x9F)
                {
                    return 3;
                }
                if (c0 == 0xE3 && c1 == 0x80 && last == 0x80)
                {
                    return 3;
                }
            }
            return 0;
        }

        std::string_view TrimDotNetWhitespace(std::string_view value)
        {
            while (const std::size_t count = DotNetWhitespacePrefixLength(value))
            {
                value.remove_prefix(count);
            }
            while (const std::size_t count = DotNetWhitespaceSuffixLength(value))
            {
                value.remove_suffix(count);
            }
            return value;
        }

        constexpr char FoldAsciiCase(char value)
        {
            if (value >= 'A' && value <= 'Z')
            {
                return static_cast<char>(value + ('a' - 'A'));
            }
            return value;
        }

        bool EqualsIgnoreCase(std::string_view left, std::string_view right)
        {
            if (left.size() != right.size())
            {
                return false;
            }
            for (std::size_t i = 0; i < left.size(); i++)
            {
                if (FoldAsciiCase(left[i]) != FoldAsciiCase(right[i]))
                {
                    return false;
                }
            }
            return true;
        }

        bool TryParseInt32(std::string_view value, std::int32_t& parsed)
        {
            if (value.empty())
            {
                return false;
            }

            if (value.front() == '+')
            {
                value.remove_prefix(1);
                if (value.empty())
                {
                    return false;
                }
            }

            std::int64_t wide = 0;
            const char* const end = value.data() + value.size();
            const auto [ptr, error] = std::from_chars(value.data(), end, wide, 10);
            if (error != std::errc{} || ptr != end
                || wide < std::numeric_limits<std::int32_t>::min()
                || wide > std::numeric_limits<std::int32_t>::max())
            {
                return false;
            }

            parsed = static_cast<std::int32_t>(wide);
            return true;
        }

        template <typename TEnum>
        struct EnumName
        {
            std::string_view Name;
            TEnum Value;
        };

        template <typename TEnum, std::size_t N>
        bool TryParseEnum(std::optional<std::string_view> value,
            const std::array<EnumName<TEnum>, N>& names, TEnum& parsed)
        {
            if (!value.has_value())
            {
                return false;
            }

            std::string_view text = TrimDotNetWhitespace(*value);
            if (text.empty())
            {
                return false;
            }

            const char first = text.front();
            if ((first >= '0' && first <= '9') || first == '+' || first == '-')
            {
                std::int32_t numeric = 0;
                if (!TryParseInt32(text, numeric))
                {
                    return false;
                }
                parsed = static_cast<TEnum>(numeric);
                return true;
            }

            using Underlying = std::underlying_type_t<TEnum>;
            std::uint32_t combined = 0;
            std::size_t start = 0;
            while (true)
            {
                const std::size_t comma = text.find(',', start);
                const std::size_t count = comma == std::string_view::npos
                    ? std::string_view::npos
                    : comma - start;
                const std::string_view part = TrimDotNetWhitespace(text.substr(start, count));
                if (part.empty())
                {
                    return false;
                }

                bool found = false;
                for (const EnumName<TEnum>& name : names)
                {
                    if (EqualsIgnoreCase(part, name.Name))
                    {
                        combined |= static_cast<std::uint32_t>(
                            static_cast<Underlying>(name.Value));
                        found = true;
                        break;
                    }
                }
                if (!found)
                {
                    return false;
                }

                if (comma == std::string_view::npos)
                {
                    break;
                }
                start = comma + 1;
            }

            parsed = static_cast<TEnum>(static_cast<std::int32_t>(combined));
            return true;
        }

        constexpr std::array<EnumName<CrosshairSize>, 3> CrosshairSizeNames =
        {{
            {"Small", CrosshairSize::Small},
            {"Medium", CrosshairSize::Medium},
            {"Big", CrosshairSize::Big}
        }};

        constexpr std::array<EnumName<CrosshairStyle>, 5> CrosshairStyleNames =
        {{
            {"Cross", CrosshairStyle::Cross},
            {"Dot", CrosshairStyle::Dot},
            {"CrossDot", CrosshairStyle::CrossDot},
            {"Circle", CrosshairStyle::Circle},
            {"Brackets", CrosshairStyle::Brackets}
        }};
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
