#include "Features.hpp"

#include "Mods/Render/Crosshair.hpp"

#include <bit>
#include <charconv>
#include <cmath>
#include <cstdint>
#include <limits>
#include <optional>
#include <string_view>
#include <system_error>

namespace MphRead
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

        std::string_view TrimDotNetWhitespaceAndNull(std::string_view value)
        {
            while (!value.empty())
            {
                if (value.front() == '\0')
                {
                    value.remove_prefix(1);
                    continue;
                }
                const std::size_t count = DotNetWhitespacePrefixLength(value);
                if (count == 0)
                {
                    break;
                }
                value.remove_prefix(count);
            }
            while (!value.empty())
            {
                if (value.back() == '\0')
                {
                    value.remove_suffix(1);
                    continue;
                }
                const std::size_t count = DotNetWhitespaceSuffixLength(value);
                if (count == 0)
                {
                    break;
                }
                value.remove_suffix(count);
            }
            return value;
        }

        constexpr bool IsNumberWhitespace(char value)
        {
            const unsigned char ch = static_cast<unsigned char>(value);
            return ch == 0x20 || (ch >= 0x09 && ch <= 0x0D);
        }

        std::string_view TrimNumberInput(std::string_view value)
        {
            while (!value.empty() && IsNumberWhitespace(value.front()))
            {
                value.remove_prefix(1);
            }
            while (!value.empty() && value.back() == '\0')
            {
                value.remove_suffix(1);
            }
            while (!value.empty() && IsNumberWhitespace(value.back()))
            {
                value.remove_suffix(1);
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

        bool TryParseBoolean(std::string_view value, bool& parsed)
        {
            value = TrimDotNetWhitespaceAndNull(value);
            if (EqualsIgnoreCase(value, "true"))
            {
                parsed = true;
                return true;
            }
            if (EqualsIgnoreCase(value, "false"))
            {
                parsed = false;
                return true;
            }
            return false;
        }

        float DotNetSingleNaN()
        {
            return std::bit_cast<float>(std::uint32_t{0xFFC00000U});
        }

        std::int64_t DecimalOrder(std::string_view value)
        {
            if (!value.empty() && (value.front() == '+' || value.front() == '-'))
            {
                value.remove_prefix(1);
            }

            const std::size_t exponentPosition = value.find_first_of("eE");
            std::string_view mantissa = value.substr(0, exponentPosition);
            std::int64_t explicitExponent = 0;
            if (exponentPosition != std::string_view::npos)
            {
                std::string_view exponent = value.substr(exponentPosition + 1);
                bool negative = false;
                if (!exponent.empty() && (exponent.front() == '+' || exponent.front() == '-'))
                {
                    negative = exponent.front() == '-';
                    exponent.remove_prefix(1);
                }
                std::uint64_t magnitude = 0;
                const char* const end = exponent.data() + exponent.size();
                const auto [ptr, error] = std::from_chars(exponent.data(), end, magnitude, 10);
                if (error == std::errc::result_out_of_range)
                {
                    return negative ? std::numeric_limits<std::int64_t>::min()
                        : std::numeric_limits<std::int64_t>::max();
                }
                if (error == std::errc{} && ptr == end)
                {
                    if (magnitude > static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max()))
                    {
                        return negative ? std::numeric_limits<std::int64_t>::min()
                            : std::numeric_limits<std::int64_t>::max();
                    }
                    explicitExponent = negative
                        ? -static_cast<std::int64_t>(magnitude)
                        : static_cast<std::int64_t>(magnitude);
                }
            }

            const std::size_t dot = mantissa.find('.');
            const std::size_t digitsBeforeDot = dot == std::string_view::npos ? mantissa.size() : dot;
            std::size_t digitIndex = 0;
            std::size_t firstNonZero = std::string_view::npos;
            for (char ch : mantissa)
            {
                if (ch == '.')
                {
                    continue;
                }
                if (ch != '0' && firstNonZero == std::string_view::npos)
                {
                    firstNonZero = digitIndex;
                }
                digitIndex++;
            }
            if (firstNonZero == std::string_view::npos)
            {
                return 0;
            }

            const std::int64_t base = static_cast<std::int64_t>(digitsBeforeDot)
                - static_cast<std::int64_t>(firstNonZero) - 1;
            if (explicitExponent > 0 && base > std::numeric_limits<std::int64_t>::max() - explicitExponent)
            {
                return std::numeric_limits<std::int64_t>::max();
            }
            if (explicitExponent < 0 && base < std::numeric_limits<std::int64_t>::min() - explicitExponent)
            {
                return std::numeric_limits<std::int64_t>::min();
            }
            return base + explicitExponent;
        }

        bool TryParseSingleInvariant(std::string_view value, float& parsed)
        {
            const std::string_view special = TrimDotNetWhitespace(value);
            if (EqualsIgnoreCase(special, "nan")
                || EqualsIgnoreCase(special, "+nan")
                || EqualsIgnoreCase(special, "-nan"))
            {
                parsed = DotNetSingleNaN();
                return true;
            }
            if (EqualsIgnoreCase(special, "infinity") || EqualsIgnoreCase(special, "+infinity"))
            {
                parsed = std::numeric_limits<float>::infinity();
                return true;
            }
            if (EqualsIgnoreCase(special, "-infinity"))
            {
                parsed = -std::numeric_limits<float>::infinity();
                return true;
            }

            value = TrimNumberInput(value);
            if (value.empty())
            {
                return false;
            }

            std::string normalized;
            normalized.reserve(value.size());
            bool sawDecimal = false;
            bool sawExponent = false;
            bool sawMantissaDigit = false;
            bool sawNonZeroMantissa = false;
            for (char ch : value)
            {
                if (ch >= '0' && ch <= '9')
                {
                    if (!sawExponent)
                    {
                        sawMantissaDigit = true;
                        sawNonZeroMantissa |= ch != '0';
                    }
                    normalized.push_back(ch);
                    continue;
                }
                if (ch == ',')
                {
                    if (sawDecimal || sawExponent || !sawMantissaDigit)
                    {
                        return false;
                    }
                    continue;
                }
                if (ch == '.')
                {
                    sawDecimal = true;
                    normalized.push_back(ch);
                    continue;
                }
                if (ch == 'e' || ch == 'E')
                {
                    sawExponent = true;
                    normalized.push_back(ch);
                    continue;
                }
                if (ch == '+' || ch == '-')
                {
                    normalized.push_back(ch);
                    continue;
                }
                return false;
            }
            if (normalized.empty())
            {
                return false;
            }

            std::string_view numeric(normalized);
            const bool negative = numeric.front() == '-';
            if (numeric.front() == '+')
            {
                numeric.remove_prefix(1);
                if (numeric.empty())
                {
                    return false;
                }
            }

            float direct = 0.0F;
            const char* const end = numeric.data() + numeric.size();
            const auto [ptr, error] = std::from_chars(
                numeric.data(), end, direct, std::chars_format::general);
            if (error == std::errc::result_out_of_range)
            {
                if (!sawMantissaDigit)
                {
                    return false;
                }
                if (!sawNonZeroMantissa || DecimalOrder(normalized) < 0)
                {
                    parsed = negative ? -0.0F : 0.0F;
                }
                else
                {
                    parsed = negative ? -std::numeric_limits<float>::infinity()
                        : std::numeric_limits<float>::infinity();
                }
                return true;
            }
            if (error != std::errc{} || ptr != end)
            {
                return false;
            }

            parsed = direct;
            return true;
        }

        std::string BoolLower(bool value)
        {
            return value ? "true" : "false";
        }

        std::string BoolDefault(bool value)
        {
            return value ? "True" : "False";
        }

        std::string FormatSingleInvariant(float value)
        {
            if (std::isnan(value))
            {
                return "NaN";
            }
            if (std::isinf(value))
            {
                return std::signbit(value) ? "-Infinity" : "Infinity";
            }

            char buffer[64]{};
            const auto [ptr, error] = std::to_chars(
                buffer, buffer + sizeof(buffer), value, std::chars_format::general);
            if (error != std::errc{})
            {
                return {};
            }
            std::string raw(buffer, ptr);

            bool negative = false;
            std::string_view text(raw);
            if (!text.empty() && text.front() == '-')
            {
                negative = true;
                text.remove_prefix(1);
            }

            const std::size_t exponentPosition = text.find_first_of("eE");
            const std::string_view mantissa = text.substr(0, exponentPosition);
            std::int32_t explicitExponent = 0;
            if (exponentPosition != std::string_view::npos)
            {
                std::string_view exponent = text.substr(exponentPosition + 1);
                bool exponentNegative = false;
                if (!exponent.empty() && (exponent.front() == '+' || exponent.front() == '-'))
                {
                    exponentNegative = exponent.front() == '-';
                    exponent.remove_prefix(1);
                }
                std::int32_t magnitude = 0;
                const char* const exponentEnd = exponent.data() + exponent.size();
                const auto [exponentPtr, exponentError] = std::from_chars(
                    exponent.data(), exponentEnd, magnitude, 10);
                if (exponentError == std::errc{} && exponentPtr == exponentEnd)
                {
                    explicitExponent = exponentNegative ? -magnitude : magnitude;
                }
            }

            const std::size_t dot = mantissa.find('.');
            const std::size_t digitsBeforeDot = dot == std::string_view::npos ? mantissa.size() : dot;
            std::string digits;
            digits.reserve(mantissa.size());
            for (char ch : mantissa)
            {
                if (ch != '.')
                {
                    digits.push_back(ch);
                }
            }

            const std::size_t firstNonZero = digits.find_first_not_of('0');
            if (firstNonZero == std::string::npos)
            {
                return negative ? "-0" : "0";
            }
            const std::int32_t decimalExponent = static_cast<std::int32_t>(digitsBeforeDot)
                - static_cast<std::int32_t>(firstNonZero) - 1 + explicitExponent;
            digits.erase(0, firstNonZero);
            while (digits.size() > 1 && digits.back() == '0')
            {
                digits.pop_back();
            }

            std::string result;
            if (negative)
            {
                result.push_back('-');
            }

            if (decimalExponent >= -4 && decimalExponent < 9)
            {
                if (decimalExponent >= 0)
                {
                    const std::size_t integerDigits = static_cast<std::size_t>(decimalExponent) + 1;
                    if (digits.size() <= integerDigits)
                    {
                        result.append(digits);
                        result.append(integerDigits - digits.size(), '0');
                    }
                    else
                    {
                        result.append(digits.data(), integerDigits);
                        result.push_back('.');
                        result.append(digits.data() + integerDigits, digits.size() - integerDigits);
                    }
                }
                else
                {
                    result.append("0.");
                    result.append(static_cast<std::size_t>(-decimalExponent - 1), '0');
                    result.append(digits);
                }
                return result;
            }

            result.push_back(digits.front());
            if (digits.size() > 1)
            {
                result.push_back('.');
                result.append(digits.data() + 1, digits.size() - 1);
            }
            result.push_back('E');
            result.push_back(decimalExponent < 0 ? '-' : '+');
            std::uint32_t magnitude = static_cast<std::uint32_t>(
                decimalExponent < 0 ? -decimalExponent : decimalExponent);
            if (magnitude < 10)
            {
                result.push_back('0');
            }
            char exponentBuffer[16]{};
            const auto [exponentPtr, exponentError] = std::to_chars(
                exponentBuffer, exponentBuffer + sizeof(exponentBuffer), magnitude);
            if (exponentError == std::errc{})
            {
                result.append(exponentBuffer, exponentPtr);
            }
            return result;
        }

        std::string CrosshairStyleToString(Mods::Render::CrosshairStyle value)
        {
            switch (value)
            {
            case Mods::Render::CrosshairStyle::Cross:
                return "Cross";
            case Mods::Render::CrosshairStyle::Dot:
                return "Dot";
            case Mods::Render::CrosshairStyle::CrossDot:
                return "CrossDot";
            case Mods::Render::CrosshairStyle::Circle:
                return "Circle";
            case Mods::Render::CrosshairStyle::Brackets:
                return "Brackets";
            default:
                return std::to_string(static_cast<std::int32_t>(value));
            }
        }

        std::string CrosshairSizeToString(Mods::Render::CrosshairSize value)
        {
            switch (value)
            {
            case Mods::Render::CrosshairSize::Small:
                return "Small";
            case Mods::Render::CrosshairSize::Medium:
                return "Medium";
            case Mods::Render::CrosshairSize::Big:
                return "Big";
            default:
                return std::to_string(static_cast<std::int32_t>(value));
            }
        }
    }

    bool Bugfixes::SmoothCamSeqHandoff = false;
    bool Bugfixes::BetterCamSeqNodeRef = true;
    bool Bugfixes::NoStrayRespawnText = false;
    bool Bugfixes::CorrectBountySfx = true;
    bool Bugfixes::NoDoubleEnemyDeath = true;
    bool Bugfixes::NoSlenchRollTimerUnderflow = true;

    void Bugfixes::Load(const std::unordered_map<std::string, std::string>& values)
    {
        bool parsed = false;
        if (const auto it = values.find("SmoothCamSeqHandoff");
            it != values.end() && TryParseBoolean(it->second, parsed))
        {
            SmoothCamSeqHandoff = parsed;
        }
        if (const auto it = values.find("BetterCamSeqNodeRef");
            it != values.end() && TryParseBoolean(it->second, parsed))
        {
            BetterCamSeqNodeRef = parsed;
        }
        if (const auto it = values.find("NoStrayRespawnText");
            it != values.end() && TryParseBoolean(it->second, parsed))
        {
            NoStrayRespawnText = parsed;
        }
        if (const auto it = values.find("CorrectBountySfx");
            it != values.end() && TryParseBoolean(it->second, parsed))
        {
            CorrectBountySfx = parsed;
        }
        if (const auto it = values.find("NoDoubleEnemyDeath");
            it != values.end() && TryParseBoolean(it->second, parsed))
        {
            NoDoubleEnemyDeath = parsed;
        }
        if (const auto it = values.find("NoSlenchRollTimerUnderflow");
            it != values.end() && TryParseBoolean(it->second, parsed))
        {
            NoSlenchRollTimerUnderflow = parsed;
        }
    }

    std::unordered_map<std::string, std::string> Bugfixes::Commit()
    {
        return {
            {"SmoothCamSeqHandoff", BoolLower(SmoothCamSeqHandoff)},
            {"BetterCamSeqNodeRef", BoolLower(BetterCamSeqNodeRef)},
            {"NoStrayRespawnText", BoolLower(NoStrayRespawnText)},
            {"CorrectBountySfx", BoolLower(CorrectBountySfx)},
            {"NoDoubleEnemyDeath", BoolLower(NoDoubleEnemyDeath)},
            {"NoSlenchRollTimerUnderflow", BoolLower(NoSlenchRollTimerUnderflow)}
        };
    }

    bool Features::NoRepeatEncounters = false;
    bool Features::AllowInvalidTeams = true;
    bool Features::TopScreenTargetInfo = true;
    bool Features::HudSway = true;
    bool Features::TargetInfoSway = false;
    bool Features::DelayedIdleSway = true;
    bool Features::NoIdleSway = false;
    bool Features::NoMapCentering = false;
    bool Features::MaxRoomDetail = false;
    bool Features::MaxPlayerDetail = true;
    bool Features::LogSpatialAudio = false;
    bool Features::HalfSecondAlarm = false;
    bool Features::FullBoostCharge = false;
    bool Features::BoostOpensDoors = false;
    bool Features::AlternateHunters1P = true;
    bool Features::HalfDamageUnscoped = false;
    bool Features::ProHud = false;
    float Features::helmetOpacity_ = 1.0F;
    float Features::visorOpacity_ = 0.5F;
    float Features::HudOpacity = 1.0F;
    float Features::ReticleOpacity = 1.0F;
    bool Features::fixedCrosshair_ = false;
    bool Features::customCrosshair_ = false;
    bool Features::modernHud_ = false;
    float Features::weaponListScale_ = 1.0F;
    bool Features::fixedWeapon_ = false;
    bool Features::ProHudFixedWeapon = true;

    float Features::HelmetOpacity()
    {
        return ProHud ? 0.0F : helmetOpacity_;
    }

    void Features::SetHelmetOpacity(float value)
    {
        helmetOpacity_ = value;
    }

    float Features::VisorOpacity()
    {
        return ProHud ? 0.0F : visorOpacity_;
    }

    void Features::SetVisorOpacity(float value)
    {
        visorOpacity_ = value;
    }

    bool Features::FixedCrosshair()
    {
        return ProHud || fixedCrosshair_;
    }

    void Features::SetFixedCrosshair(bool value)
    {
        fixedCrosshair_ = value;
    }

    bool Features::CustomCrosshair()
    {
        return ProHud || customCrosshair_;
    }

    void Features::SetCustomCrosshair(bool value)
    {
        customCrosshair_ = value;
    }

    bool Features::ModernHud()
    {
        return ProHud || modernHud_;
    }

    void Features::SetModernHud(bool value)
    {
        modernHud_ = value;
    }

    float Features::WeaponListScale()
    {
        return ProHud ? ProHudWeaponListScale : weaponListScale_;
    }

    void Features::SetWeaponListScale(float value)
    {
        weaponListScale_ = value;
    }

    bool Features::FixedWeapon()
    {
        return ProHud ? ProHudFixedWeapon : fixedWeapon_;
    }

    void Features::SetFixedWeapon(bool value)
    {
        fixedWeapon_ = value;
    }

    void Features::Load(const std::unordered_map<std::string, std::string>& values)
    {
        if (const auto it = values.find("ReticleOpacity"); it != values.end())
        {
            float parsed = 0.0F;
            if (TryParseSingleInvariant(it->second, parsed))
            {
                ReticleOpacity = parsed;
            }
        }

        bool parsed = false;
        if (const auto it = values.find("ProHud");
            it != values.end() && TryParseBoolean(it->second, parsed))
        {
            ProHud = parsed;
        }
        if (const auto it = values.find("ProHudFixedWeapon");
            it != values.end() && TryParseBoolean(it->second, parsed))
        {
            ProHudFixedWeapon = parsed;
        }
        if (const auto it = values.find("CrosshairStyle"); it != values.end())
        {
            Mods::Render::Crosshair::Style = Mods::Render::Crosshair::ParseStyle(
                std::optional<std::string_view>{std::string_view(it->second)},
                Mods::Render::Crosshair::Style);
        }
        if (const auto it = values.find("CrosshairSize"); it != values.end())
        {
            Mods::Render::Crosshair::Size = Mods::Render::Crosshair::ParseSize(
                std::optional<std::string_view>{std::string_view(it->second)},
                Mods::Render::Crosshair::Size);
        }
    }

    std::unordered_map<std::string, std::string> Features::Commit()
    {
        return {
            {"ReticleOpacity", FormatSingleInvariant(ReticleOpacity)},
            {"ProHud", BoolLower(ProHud)},
            {"ProHudFixedWeapon", BoolLower(ProHudFixedWeapon)},
            {"CrosshairStyle", CrosshairStyleToString(Mods::Render::Crosshair::Style)},
            {"CrosshairSize", CrosshairSizeToString(Mods::Render::Crosshair::Size)}
        };
    }

    bool Cheats::FreeWeaponSelect = false;
    bool Cheats::UnlimitedJumps = false;
    bool Cheats::NoRandomEncounters = false;
    bool Cheats::UnlockAllDoors = false;
    bool Cheats::ContinueFromCurrentRoom = false;
    bool Cheats::SkipPlanetIntros = false;
    bool Cheats::StartWithAllUpgrades = false;
    bool Cheats::StartWithAllOctoliths = false;
    bool Cheats::WalkThroughWalls = false;
    bool Cheats::AlwaysFightGorea2 = false;
    bool Cheats::QuadrupleDamage = false;

    void Cheats::Load(const std::unordered_map<std::string, std::string>& values)
    {
        bool parsed = false;
        if (const auto it = values.find("FreeWeaponSelect");
            it != values.end() && TryParseBoolean(it->second, parsed))
        {
            FreeWeaponSelect = parsed;
        }
        if (const auto it = values.find("UnlimitedJumps");
            it != values.end() && TryParseBoolean(it->second, parsed))
        {
            UnlimitedJumps = parsed;
        }
        if (const auto it = values.find("NoRandomEncounters");
            it != values.end() && TryParseBoolean(it->second, parsed))
        {
            NoRandomEncounters = parsed;
        }
        if (const auto it = values.find("UnlockAllDoors");
            it != values.end() && TryParseBoolean(it->second, parsed))
        {
            UnlockAllDoors = parsed;
        }
        if (const auto it = values.find("ContinueFromCurrentRoom");
            it != values.end() && TryParseBoolean(it->second, parsed))
        {
            ContinueFromCurrentRoom = parsed;
        }
        if (const auto it = values.find("SkipPlanetIntros");
            it != values.end() && TryParseBoolean(it->second, parsed))
        {
            SkipPlanetIntros = parsed;
        }
        if (const auto it = values.find("StartWithAllUpgrades");
            it != values.end() && TryParseBoolean(it->second, parsed))
        {
            StartWithAllUpgrades = parsed;
        }
        if (const auto it = values.find("StartWithAllOctoliths");
            it != values.end() && TryParseBoolean(it->second, parsed))
        {
            StartWithAllOctoliths = parsed;
        }
        if (const auto it = values.find("WalkThroughWalls");
            it != values.end() && TryParseBoolean(it->second, parsed))
        {
            WalkThroughWalls = parsed;
        }
        if (const auto it = values.find("AlwaysFightGorea2");
            it != values.end() && TryParseBoolean(it->second, parsed))
        {
            AlwaysFightGorea2 = parsed;
        }
        if (const auto it = values.find("QuadrupleDamage");
            it != values.end() && TryParseBoolean(it->second, parsed))
        {
            QuadrupleDamage = parsed;
        }
    }

    std::unordered_map<std::string, std::string> Cheats::Commit()
    {
        return {
            {"FreeWeaponSelect", BoolLower(FreeWeaponSelect)},
            {"UnlimitedJumps", BoolLower(UnlimitedJumps)},
            {"NoRandomEncounters", BoolDefault(NoRandomEncounters)},
            {"UnlockAllDoors", BoolDefault(UnlockAllDoors)},
            {"ContinueFromCurrentRoom", BoolDefault(ContinueFromCurrentRoom)},
            {"SkipPlanetIntros", BoolDefault(SkipPlanetIntros)},
            {"StartWithAllUpgrades", BoolDefault(StartWithAllUpgrades)},
            {"StartWithAllOctoliths", BoolLower(StartWithAllOctoliths)},
            {"WalkThroughWalls", BoolLower(WalkThroughWalls)},
            {"AlwaysFightGorea2", BoolLower(AlwaysFightGorea2)},
            {"QuadrupleDamage", BoolLower(QuadrupleDamage)}
        };
    }
}
