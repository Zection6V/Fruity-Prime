#include "Features.hpp"
#include "NativeRuntime/System/Charconv.hpp"

#include <iterator>
#include <span>

#include "Mods/Render/Crosshair.hpp"
#include "NativeRuntime/System/Globalization.hpp"

#include <bit>
#include <charconv>
#include <cmath>
#include <cstdint>
#include <limits>
#include <optional>
#include <string_view>
#include <system_error>

using ::MphRead::NativeRuntime::BooleanTryParse;
using ::MphRead::NativeRuntime::IsNumberWhiteSpace;
using ::MphRead::NativeRuntime::StringEqualsOrdinalIgnoreCase;
using ::MphRead::NativeRuntime::StringTrimView;

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

        std::string_view TrimNumberInput(std::string_view value)
        {
            while (!value.empty() && IsNumberWhiteSpace(value.front()))
            {
                value.remove_prefix(1);
            }
            while (!value.empty() && value.back() == '\0')
            {
                value.remove_suffix(1);
            }
            while (!value.empty() && IsNumberWhiteSpace(value.back()))
            {
                value.remove_suffix(1);
            }
            return value;
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
            const std::string_view special = StringTrimView(value);
            if (StringEqualsOrdinalIgnoreCase(special, "nan")
                || StringEqualsOrdinalIgnoreCase(special, "+nan")
                || StringEqualsOrdinalIgnoreCase(special, "-nan"))
            {
                parsed = DotNetSingleNaN();
                return true;
            }
            if (StringEqualsOrdinalIgnoreCase(special, "infinity") || StringEqualsOrdinalIgnoreCase(special, "+infinity"))
            {
                parsed = std::numeric_limits<float>::infinity();
                return true;
            }
            if (StringEqualsOrdinalIgnoreCase(special, "-infinity"))
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
            const auto [ptr, error] = ::MphRead::NativeRuntime::FromChars(
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

    bool Bugfixes::_smoothCamSeqHandoff = false;
    bool Bugfixes::_betterCamSeqNodeRef = true;
    bool Bugfixes::_noStrayRespawnText = false;
    bool Bugfixes::_correctBountySfx = true;
    bool Bugfixes::_noDoubleEnemyDeath = true;
    bool Bugfixes::_noSlenchRollTimerUnderflow = true;

    bool Bugfixes::SmoothCamSeqHandoff() noexcept
    {
        return _smoothCamSeqHandoff;
    }

    void Bugfixes::SmoothCamSeqHandoff(bool value) noexcept
    {
        _smoothCamSeqHandoff = value;
    }

    bool Bugfixes::BetterCamSeqNodeRef() noexcept
    {
        return _betterCamSeqNodeRef;
    }

    void Bugfixes::BetterCamSeqNodeRef(bool value) noexcept
    {
        _betterCamSeqNodeRef = value;
    }

    bool Bugfixes::NoStrayRespawnText() noexcept
    {
        return _noStrayRespawnText;
    }

    void Bugfixes::NoStrayRespawnText(bool value) noexcept
    {
        _noStrayRespawnText = value;
    }

    bool Bugfixes::CorrectBountySfx() noexcept
    {
        return _correctBountySfx;
    }

    void Bugfixes::CorrectBountySfx(bool value) noexcept
    {
        _correctBountySfx = value;
    }

    bool Bugfixes::NoDoubleEnemyDeath() noexcept
    {
        return _noDoubleEnemyDeath;
    }

    void Bugfixes::NoDoubleEnemyDeath(bool value) noexcept
    {
        _noDoubleEnemyDeath = value;
    }

    bool Bugfixes::NoSlenchRollTimerUnderflow() noexcept
    {
        return _noSlenchRollTimerUnderflow;
    }

    void Bugfixes::NoSlenchRollTimerUnderflow(bool value) noexcept
    {
        _noSlenchRollTimerUnderflow = value;
    }

    void Bugfixes::Load(const std::unordered_map<std::string, std::string>& values)
    {
        bool parsed = false;
        if (const auto it = values.find("SmoothCamSeqHandoff");
            it != values.end() && BooleanTryParse(it->second, parsed))
        {
            SmoothCamSeqHandoff(parsed);
        }
        if (const auto it = values.find("BetterCamSeqNodeRef");
            it != values.end() && BooleanTryParse(it->second, parsed))
        {
            BetterCamSeqNodeRef(parsed);
        }
        if (const auto it = values.find("NoStrayRespawnText");
            it != values.end() && BooleanTryParse(it->second, parsed))
        {
            NoStrayRespawnText(parsed);
        }
        if (const auto it = values.find("CorrectBountySfx");
            it != values.end() && BooleanTryParse(it->second, parsed))
        {
            CorrectBountySfx(parsed);
        }
        if (const auto it = values.find("NoDoubleEnemyDeath");
            it != values.end() && BooleanTryParse(it->second, parsed))
        {
            NoDoubleEnemyDeath(parsed);
        }
        if (const auto it = values.find("NoSlenchRollTimerUnderflow");
            it != values.end() && BooleanTryParse(it->second, parsed))
        {
            NoSlenchRollTimerUnderflow(parsed);
        }
    }

    std::unordered_map<std::string, std::string> Bugfixes::Commit()
    {
        return {
            {"SmoothCamSeqHandoff", BoolLower(SmoothCamSeqHandoff())},
            {"BetterCamSeqNodeRef", BoolLower(BetterCamSeqNodeRef())},
            {"NoStrayRespawnText", BoolLower(NoStrayRespawnText())},
            {"CorrectBountySfx", BoolLower(CorrectBountySfx())},
            {"NoDoubleEnemyDeath", BoolLower(NoDoubleEnemyDeath())},
            {"NoSlenchRollTimerUnderflow", BoolLower(NoSlenchRollTimerUnderflow())}
        };
    }

    bool Features::_noRepeatEncounters = false;
    bool Features::_allowInvalidTeams = true;
    bool Features::_topScreenTargetInfo = true;
    bool Features::_hudSway = true;
    bool Features::_targetInfoSway = false;
    bool Features::_delayedIdleSway = true;
    bool Features::_noIdleSway = false;
    bool Features::_noMapCentering = false;
    bool Features::_maxRoomDetail = false;
    bool Features::_maxPlayerDetail = true;
    bool Features::_logSpatialAudio = false;
    bool Features::_halfSecondAlarm = false;
    bool Features::_fullBoostCharge = false;
    bool Features::_boostOpensDoors = false;
    bool Features::_alternateHunters1P = true;
    bool Features::_halfDamageUnscoped = false;
    bool Features::_proHud = false;
    float Features::_helmetOpacity = 1.0F;
    float Features::_visorOpacity = 0.5F;
    float Features::_hudOpacity = 1.0F;
    float Features::_reticleOpacity = 1.0F;
    bool Features::_fixedCrosshair = false;
    bool Features::_customCrosshair = false;
    bool Features::_modernHud = false;
    float Features::_weaponListScale = 1.0F;
    bool Features::_fixedWeapon = false;
    bool Features::_proHudFixedWeapon = true;

    bool Features::NoRepeatEncounters() noexcept
    {
        return _noRepeatEncounters;
    }

    void Features::NoRepeatEncounters(bool value) noexcept
    {
        _noRepeatEncounters = value;
    }

    bool Features::AllowInvalidTeams() noexcept
    {
        return _allowInvalidTeams;
    }

    void Features::AllowInvalidTeams(bool value) noexcept
    {
        _allowInvalidTeams = value;
    }

    bool Features::TopScreenTargetInfo() noexcept
    {
        return _topScreenTargetInfo;
    }

    void Features::TopScreenTargetInfo(bool value) noexcept
    {
        _topScreenTargetInfo = value;
    }

    bool Features::HudSway() noexcept
    {
        return _hudSway;
    }

    void Features::HudSway(bool value) noexcept
    {
        _hudSway = value;
    }

    bool Features::TargetInfoSway() noexcept
    {
        return _targetInfoSway;
    }

    void Features::TargetInfoSway(bool value) noexcept
    {
        _targetInfoSway = value;
    }

    bool Features::DelayedIdleSway() noexcept
    {
        return _delayedIdleSway;
    }

    void Features::DelayedIdleSway(bool value) noexcept
    {
        _delayedIdleSway = value;
    }

    bool Features::NoIdleSway() noexcept
    {
        return _noIdleSway;
    }

    void Features::NoIdleSway(bool value) noexcept
    {
        _noIdleSway = value;
    }

    bool Features::NoMapCentering() noexcept
    {
        return _noMapCentering;
    }

    void Features::NoMapCentering(bool value) noexcept
    {
        _noMapCentering = value;
    }

    bool Features::MaxRoomDetail() noexcept
    {
        return _maxRoomDetail;
    }

    void Features::MaxRoomDetail(bool value) noexcept
    {
        _maxRoomDetail = value;
    }

    bool Features::MaxPlayerDetail() noexcept
    {
        return _maxPlayerDetail;
    }

    void Features::MaxPlayerDetail(bool value) noexcept
    {
        _maxPlayerDetail = value;
    }

    bool Features::LogSpatialAudio() noexcept
    {
        return _logSpatialAudio;
    }

    void Features::LogSpatialAudio(bool value) noexcept
    {
        _logSpatialAudio = value;
    }

    bool Features::HalfSecondAlarm() noexcept
    {
        return _halfSecondAlarm;
    }

    void Features::HalfSecondAlarm(bool value) noexcept
    {
        _halfSecondAlarm = value;
    }

    bool Features::FullBoostCharge() noexcept
    {
        return _fullBoostCharge;
    }

    void Features::FullBoostCharge(bool value) noexcept
    {
        _fullBoostCharge = value;
    }

    bool Features::BoostOpensDoors() noexcept
    {
        return _boostOpensDoors;
    }

    void Features::BoostOpensDoors(bool value) noexcept
    {
        _boostOpensDoors = value;
    }

    bool Features::AlternateHunters1P() noexcept
    {
        return _alternateHunters1P;
    }

    void Features::AlternateHunters1P(bool value) noexcept
    {
        _alternateHunters1P = value;
    }

    bool Features::HalfDamageUnscoped() noexcept
    {
        return _halfDamageUnscoped;
    }

    void Features::HalfDamageUnscoped(bool value) noexcept
    {
        _halfDamageUnscoped = value;
    }

    bool Features::ProHud() noexcept
    {
        return _proHud;
    }

    void Features::ProHud(bool value) noexcept
    {
        _proHud = value;
    }

    float Features::HelmetOpacity() noexcept
    {
        return ProHud() ? 0.0F : _helmetOpacity;
    }

    void Features::HelmetOpacity(float value) noexcept
    {
        _helmetOpacity = value;
    }

    float Features::VisorOpacity() noexcept
    {
        return ProHud() ? 0.0F : _visorOpacity;
    }

    void Features::VisorOpacity(float value) noexcept
    {
        _visorOpacity = value;
    }

    float Features::HudOpacity() noexcept
    {
        return _hudOpacity;
    }

    void Features::HudOpacity(float value) noexcept
    {
        _hudOpacity = value;
    }

    float Features::ReticleOpacity() noexcept
    {
        return _reticleOpacity;
    }

    void Features::ReticleOpacity(float value) noexcept
    {
        _reticleOpacity = value;
    }

    bool Features::FixedCrosshair() noexcept
    {
        return ProHud() || _fixedCrosshair;
    }

    void Features::FixedCrosshair(bool value) noexcept
    {
        _fixedCrosshair = value;
    }

    bool Features::CustomCrosshair() noexcept
    {
        return ProHud() || _customCrosshair;
    }

    void Features::CustomCrosshair(bool value) noexcept
    {
        _customCrosshair = value;
    }

    bool Features::ModernHud() noexcept
    {
        return ProHud() || _modernHud;
    }

    void Features::ModernHud(bool value) noexcept
    {
        _modernHud = value;
    }

    float Features::WeaponListScale() noexcept
    {
        return ProHud() ? ProHudWeaponListScale : _weaponListScale;
    }

    void Features::WeaponListScale(float value) noexcept
    {
        _weaponListScale = value;
    }

    bool Features::FixedWeapon() noexcept
    {
        return ProHud() ? ProHudFixedWeapon() : _fixedWeapon;
    }

    void Features::FixedWeapon(bool value) noexcept
    {
        _fixedWeapon = value;
    }

    bool Features::ProHudFixedWeapon() noexcept
    {
        return _proHudFixedWeapon;
    }

    void Features::ProHudFixedWeapon(bool value) noexcept
    {
        _proHudFixedWeapon = value;
    }

    void Features::Load(const std::unordered_map<std::string, std::string>& values)
    {
        if (const auto it = values.find("ReticleOpacity"); it != values.end())
        {
            float parsed = 0.0F;
            if (TryParseSingleInvariant(it->second, parsed))
            {
                ReticleOpacity(parsed);
            }
        }

        bool parsed = false;
        if (const auto it = values.find("ProHud");
            it != values.end() && BooleanTryParse(it->second, parsed))
        {
            ProHud(parsed);
        }
        if (const auto it = values.find("ProHudFixedWeapon");
            it != values.end() && BooleanTryParse(it->second, parsed))
        {
            ProHudFixedWeapon(parsed);
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
            {"ReticleOpacity", FormatSingleInvariant(ReticleOpacity())},
            {"ProHud", BoolLower(ProHud())},
            {"ProHudFixedWeapon", BoolLower(ProHudFixedWeapon())},
            {"CrosshairStyle", CrosshairStyleToString(Mods::Render::Crosshair::Style)},
            {"CrosshairSize", CrosshairSizeToString(Mods::Render::Crosshair::Size)}
        };
    }

    bool Cheats::_freeWeaponSelect = false;
    bool Cheats::_unlimitedJumps = false;
    bool Cheats::_noRandomEncounters = false;
    bool Cheats::_unlockAllDoors = false;
    bool Cheats::_continueFromCurrentRoom = false;
    bool Cheats::_skipPlanetIntros = false;
    bool Cheats::_startWithAllUpgrades = false;
    bool Cheats::_startWithAllOctoliths = false;
    bool Cheats::_walkThroughWalls = false;
    bool Cheats::_alwaysFightGorea2 = false;
    bool Cheats::_quadrupleDamage = false;

    bool Cheats::FreeWeaponSelect() noexcept
    {
        return _freeWeaponSelect;
    }

    void Cheats::FreeWeaponSelect(bool value) noexcept
    {
        _freeWeaponSelect = value;
    }

    bool Cheats::UnlimitedJumps() noexcept
    {
        return _unlimitedJumps;
    }

    void Cheats::UnlimitedJumps(bool value) noexcept
    {
        _unlimitedJumps = value;
    }

    bool Cheats::NoRandomEncounters() noexcept
    {
        return _noRandomEncounters;
    }

    void Cheats::NoRandomEncounters(bool value) noexcept
    {
        _noRandomEncounters = value;
    }

    bool Cheats::UnlockAllDoors() noexcept
    {
        return _unlockAllDoors;
    }

    void Cheats::UnlockAllDoors(bool value) noexcept
    {
        _unlockAllDoors = value;
    }

    bool Cheats::ContinueFromCurrentRoom() noexcept
    {
        return _continueFromCurrentRoom;
    }

    void Cheats::ContinueFromCurrentRoom(bool value) noexcept
    {
        _continueFromCurrentRoom = value;
    }

    bool Cheats::SkipPlanetIntros() noexcept
    {
        return _skipPlanetIntros;
    }

    void Cheats::SkipPlanetIntros(bool value) noexcept
    {
        _skipPlanetIntros = value;
    }

    bool Cheats::StartWithAllUpgrades() noexcept
    {
        return _startWithAllUpgrades;
    }

    void Cheats::StartWithAllUpgrades(bool value) noexcept
    {
        _startWithAllUpgrades = value;
    }

    bool Cheats::StartWithAllOctoliths() noexcept
    {
        return _startWithAllOctoliths;
    }

    void Cheats::StartWithAllOctoliths(bool value) noexcept
    {
        _startWithAllOctoliths = value;
    }

    bool Cheats::WalkThroughWalls() noexcept
    {
        return _walkThroughWalls;
    }

    void Cheats::WalkThroughWalls(bool value) noexcept
    {
        _walkThroughWalls = value;
    }

    bool Cheats::AlwaysFightGorea2() noexcept
    {
        return _alwaysFightGorea2;
    }

    void Cheats::AlwaysFightGorea2(bool value) noexcept
    {
        _alwaysFightGorea2 = value;
    }

    bool Cheats::QuadrupleDamage() noexcept
    {
        return _quadrupleDamage;
    }

    void Cheats::QuadrupleDamage(bool value) noexcept
    {
        _quadrupleDamage = value;
    }

    void Cheats::Load(const std::unordered_map<std::string, std::string>& values)
    {
        bool parsed = false;
        if (const auto it = values.find("FreeWeaponSelect");
            it != values.end() && BooleanTryParse(it->second, parsed))
        {
            FreeWeaponSelect(parsed);
        }
        if (const auto it = values.find("UnlimitedJumps");
            it != values.end() && BooleanTryParse(it->second, parsed))
        {
            UnlimitedJumps(parsed);
        }
        if (const auto it = values.find("NoRandomEncounters");
            it != values.end() && BooleanTryParse(it->second, parsed))
        {
            NoRandomEncounters(parsed);
        }
        if (const auto it = values.find("UnlockAllDoors");
            it != values.end() && BooleanTryParse(it->second, parsed))
        {
            UnlockAllDoors(parsed);
        }
        if (const auto it = values.find("ContinueFromCurrentRoom");
            it != values.end() && BooleanTryParse(it->second, parsed))
        {
            ContinueFromCurrentRoom(parsed);
        }
        if (const auto it = values.find("SkipPlanetIntros");
            it != values.end() && BooleanTryParse(it->second, parsed))
        {
            SkipPlanetIntros(parsed);
        }
        if (const auto it = values.find("StartWithAllUpgrades");
            it != values.end() && BooleanTryParse(it->second, parsed))
        {
            StartWithAllUpgrades(parsed);
        }
        if (const auto it = values.find("StartWithAllOctoliths");
            it != values.end() && BooleanTryParse(it->second, parsed))
        {
            StartWithAllOctoliths(parsed);
        }
        if (const auto it = values.find("WalkThroughWalls");
            it != values.end() && BooleanTryParse(it->second, parsed))
        {
            WalkThroughWalls(parsed);
        }
        if (const auto it = values.find("AlwaysFightGorea2");
            it != values.end() && BooleanTryParse(it->second, parsed))
        {
            AlwaysFightGorea2(parsed);
        }
        if (const auto it = values.find("QuadrupleDamage");
            it != values.end() && BooleanTryParse(it->second, parsed))
        {
            QuadrupleDamage(parsed);
        }
    }

    std::unordered_map<std::string, std::string> Cheats::Commit()
    {
        return {
            {"FreeWeaponSelect", BoolLower(FreeWeaponSelect())},
            {"UnlimitedJumps", BoolLower(UnlimitedJumps())},
            {"NoRandomEncounters", BoolDefault(NoRandomEncounters())},
            {"UnlockAllDoors", BoolDefault(UnlockAllDoors())},
            {"ContinueFromCurrentRoom", BoolDefault(ContinueFromCurrentRoom())},
            {"SkipPlanetIntros", BoolDefault(SkipPlanetIntros())},
            {"StartWithAllUpgrades", BoolDefault(StartWithAllUpgrades())},
            {"StartWithAllOctoliths", BoolLower(StartWithAllOctoliths())},
            {"WalkThroughWalls", BoolLower(WalkThroughWalls())},
            {"AlwaysFightGorea2", BoolLower(AlwaysFightGorea2())},
            {"QuadrupleDamage", BoolLower(QuadrupleDamage())}
        };
    }
}

namespace MphRead
{
    std::span<const Cheats::BooleanProperty> Cheats::BooleanProperties() noexcept
    {
        // Declaration order, which is the order reflection reports.
        static constexpr BooleanProperty properties[] = {
            {"FreeWeaponSelect", &Cheats::FreeWeaponSelect,
                [](bool value) noexcept { Cheats::FreeWeaponSelect(value); }},
            {"UnlimitedJumps", &Cheats::UnlimitedJumps,
                [](bool value) noexcept { Cheats::UnlimitedJumps(value); }},
            {"NoRandomEncounters", &Cheats::NoRandomEncounters,
                [](bool value) noexcept { Cheats::NoRandomEncounters(value); }},
            {"UnlockAllDoors", &Cheats::UnlockAllDoors,
                [](bool value) noexcept { Cheats::UnlockAllDoors(value); }},
            {"ContinueFromCurrentRoom", &Cheats::ContinueFromCurrentRoom,
                [](bool value) noexcept { Cheats::ContinueFromCurrentRoom(value); }},
            {"SkipPlanetIntros", &Cheats::SkipPlanetIntros,
                [](bool value) noexcept { Cheats::SkipPlanetIntros(value); }},
            {"StartWithAllUpgrades", &Cheats::StartWithAllUpgrades,
                [](bool value) noexcept { Cheats::StartWithAllUpgrades(value); }},
            {"StartWithAllOctoliths", &Cheats::StartWithAllOctoliths,
                [](bool value) noexcept { Cheats::StartWithAllOctoliths(value); }},
            {"WalkThroughWalls", &Cheats::WalkThroughWalls,
                [](bool value) noexcept { Cheats::WalkThroughWalls(value); }},
            {"AlwaysFightGorea2", &Cheats::AlwaysFightGorea2,
                [](bool value) noexcept { Cheats::AlwaysFightGorea2(value); }},
            {"QuadrupleDamage", &Cheats::QuadrupleDamage,
                [](bool value) noexcept { Cheats::QuadrupleDamage(value); }},
        };
        return std::span<const BooleanProperty>(properties, std::size(properties));
    }
}
