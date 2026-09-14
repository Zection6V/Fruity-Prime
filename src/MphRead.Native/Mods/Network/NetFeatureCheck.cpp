#include "NetFeatureCheck.hpp"

#include "NetDamage.hpp"
#include "NetPlayerBridge.hpp"
#include "NetProtocol.hpp"
#include "NetSession.hpp"
#include "NetTestScript.hpp"
#include "../SpectatorMode.hpp"
#include "../WorldEvents.hpp"
#include "../../Entities/BeamProjectileEntity.hpp"
#include "../../Entities/BombEntity.hpp"
#include "../../Entities/ItemInstanceEntity.hpp"
#include "../../Entities/Players/HalfturretEntity.hpp"
#include "../../Entities/Players/PlayerEntity.hpp"
#include "../../GameState.hpp"
#include "../../Scene.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <charconv>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

#if defined(_WIN32)
#define NOMINMAX
#include <windows.h>
#else
#include <langinfo.h>
#include <locale.h>
#endif

namespace
{
    using MphRead::BeamType;
    using MphRead::Hunter;
    using MphRead::Entities::LoadFlags;
    using MphRead::Entities::PlayerFlags2;
    using MphRead::Mods::Network::TestPhase;
    using OpenTK::Mathematics::Vector3;

    [[nodiscard]] constexpr std::int32_t AddInt32(
        std::int32_t left, std::int32_t right) noexcept
    {
        return std::bit_cast<std::int32_t>(
            static_cast<std::uint32_t>(left) + static_cast<std::uint32_t>(right));
    }

    [[nodiscard]] constexpr std::int32_t SubInt32(
        std::int32_t left, std::int32_t right) noexcept
    {
        return std::bit_cast<std::int32_t>(
            static_cast<std::uint32_t>(left) - static_cast<std::uint32_t>(right));
    }

    void IncrementInt32(std::int32_t& value) noexcept
    {
        value = AddInt32(value, 1);
    }

    [[nodiscard]] constexpr std::int64_t AddInt64(
        std::int64_t left, std::int64_t right) noexcept
    {
        return std::bit_cast<std::int64_t>(
            static_cast<std::uint64_t>(left) + static_cast<std::uint64_t>(right));
    }

    [[nodiscard]] std::size_t ManagedArrayLength(std::int32_t length)
    {
        if (length < 0)
        {
            throw System::OverflowException();
        }
        return static_cast<std::size_t>(length);
    }

    [[nodiscard]] constexpr std::string_view ManagedNewLine() noexcept
    {
#if defined(_WIN32)
        return "\r\n";
#else
        return "\n";
#endif
    }

    void ConsoleWrite(std::string_view value)
    {
        std::cout.write(value.data(), static_cast<std::streamsize>(value.size()));
    }

    void ConsoleWriteLine()
    {
        ConsoleWrite(ManagedNewLine());
    }

    void ConsoleWriteLine(std::string_view value)
    {
        ConsoleWrite(value);
        ConsoleWrite(ManagedNewLine());
    }

    template <typename TEnum>
    [[nodiscard]] bool TestFlag(TEnum value, TEnum flag) noexcept
    {
        using Underlying = std::underlying_type_t<TEnum>;
        return (static_cast<Underlying>(value) & static_cast<Underlying>(flag)) != 0;
    }

    [[nodiscard]] float Length(Vector3 value) noexcept
    {
        return std::sqrt(value.X * value.X + value.Y * value.Y + value.Z * value.Z);
    }

    [[nodiscard]] float ManagedMin(float left, float right) noexcept
    {
        if (std::isnan(left))
        {
            return left;
        }
        if (std::isnan(right))
        {
            return right;
        }
        if (left < right)
        {
            return left;
        }
        if (right < left)
        {
            return right;
        }
        if (left == 0.0F && right == 0.0F)
        {
            return std::signbit(left) || std::signbit(right) ? -0.0F : 0.0F;
        }
        return left;
    }

    [[nodiscard]] float ManagedMax(float left, float right) noexcept
    {
        if (std::isnan(left))
        {
            return left;
        }
        if (std::isnan(right))
        {
            return right;
        }
        if (left > right)
        {
            return left;
        }
        if (right > left)
        {
            return right;
        }
        if (left == 0.0F && right == 0.0F)
        {
            return !std::signbit(left) || !std::signbit(right) ? 0.0F : -0.0F;
        }
        return left;
    }

    [[nodiscard]] double ManagedMax(double left, double right) noexcept
    {
        if (std::isnan(left))
        {
            return left;
        }
        if (std::isnan(right))
        {
            return right;
        }
        if (left > right)
        {
            return left;
        }
        if (right > left)
        {
            return right;
        }
        if (left == 0.0 && right == 0.0)
        {
            return !std::signbit(left) || !std::signbit(right) ? 0.0 : -0.0;
        }
        return left;
    }

    [[nodiscard]] std::size_t Utf16Length(std::string_view value) noexcept
    {
        std::size_t length = 0;
        for (std::size_t index = 0; index < value.size();)
        {
            const unsigned char lead = static_cast<unsigned char>(value[index]);
            std::uint32_t codePoint = 0;
            std::size_t count = 1;
            if ((lead & 0x80U) == 0)
            {
                codePoint = lead;
            }
            else if ((lead & 0xE0U) == 0xC0U && index + 1 < value.size())
            {
                codePoint = lead & 0x1FU;
                count = 2;
            }
            else if ((lead & 0xF0U) == 0xE0U && index + 2 < value.size())
            {
                codePoint = lead & 0x0FU;
                count = 3;
            }
            else if ((lead & 0xF8U) == 0xF0U && index + 3 < value.size())
            {
                codePoint = lead & 0x07U;
                count = 4;
            }
            else
            {
                ++length;
                ++index;
                continue;
            }

            bool valid = true;
            for (std::size_t offset = 1; offset < count; ++offset)
            {
                const unsigned char next
                    = static_cast<unsigned char>(value[index + offset]);
                if ((next & 0xC0U) != 0x80U)
                {
                    valid = false;
                    break;
                }
                codePoint = (codePoint << 6U) | (next & 0x3FU);
            }
            if (!valid)
            {
                ++length;
                ++index;
                continue;
            }
            length += codePoint > 0xFFFFU ? 2U : 1U;
            index += count;
        }
        return length;
    }

    [[nodiscard]] std::string PadLeftManaged(std::string value, std::size_t width)
    {
        const std::size_t length = Utf16Length(value);
        if (length < width)
        {
            value.insert(0, width - length, ' ');
        }
        return value;
    }

    [[nodiscard]] std::string PadRightManaged(std::string value, std::size_t width)
    {
        const std::size_t length = Utf16Length(value);
        if (length < width)
        {
            value.append(width - length, ' ');
        }
        return value;
    }

    struct NumberSymbols
    {
        std::string Decimal = ".";
        std::string Negative = "-";
        std::string NaN = "NaN";
        std::string PositiveInfinity = "\xE2\x88\x9E";
        std::string NegativeInfinity = "-\xE2\x88\x9E";
    };

    [[nodiscard]] bool GlobalizationInvariantRequested() noexcept
    {
        const char* value = std::getenv("DOTNET_SYSTEM_GLOBALIZATION_INVARIANT");
        if (value == nullptr)
        {
            return false;
        }
        std::string_view text(value);
        if (text == "1")
        {
            return true;
        }
        if (text.size() != 4)
        {
            return false;
        }
        return (text[0] == 't' || text[0] == 'T')
            && (text[1] == 'r' || text[1] == 'R')
            && (text[2] == 'u' || text[2] == 'U')
            && (text[3] == 'e' || text[3] == 'E');
    }

#if defined(_WIN32)
    [[nodiscard]] std::string WideToUtf8(const wchar_t* value)
    {
        if (value == nullptr || *value == L'\0')
        {
            return {};
        }
        const int required = WideCharToMultiByte(
            CP_UTF8, 0, value, -1, nullptr, 0, nullptr, nullptr);
        if (required <= 1)
        {
            return {};
        }
        std::string result(static_cast<std::size_t>(required), '\0');
        const int written = WideCharToMultiByte(
            CP_UTF8, 0, value, -1, result.data(), required, nullptr, nullptr);
        if (written <= 1)
        {
            return {};
        }
        result.resize(static_cast<std::size_t>(written - 1));
        return result;
    }

    [[nodiscard]] std::string LocaleString(LCTYPE type, std::string fallback)
    {
        wchar_t buffer[128]{};
        const int count = GetLocaleInfoEx(
            LOCALE_NAME_USER_DEFAULT, type, buffer, static_cast<int>(std::size(buffer)));
        if (count <= 1)
        {
            return fallback;
        }
        std::string result = WideToUtf8(buffer);
        return result.empty() ? fallback : result;
    }
#endif

    [[nodiscard]] NumberSymbols CurrentNumberSymbols()
    {
        NumberSymbols symbols{};
        if (GlobalizationInvariantRequested())
        {
            symbols.PositiveInfinity = "Infinity";
            symbols.NegativeInfinity = "-Infinity";
            return symbols;
        }
#if defined(_WIN32)
        symbols.Decimal = LocaleString(LOCALE_SDECIMAL, ".");
        symbols.Negative = LocaleString(LOCALE_SNEGATIVESIGN, "-");
#ifdef LOCALE_SNAN
        symbols.NaN = LocaleString(LOCALE_SNAN, "NaN");
#endif
#ifdef LOCALE_SPOSINFINITY
        symbols.PositiveInfinity = LocaleString(LOCALE_SPOSINFINITY, "\xE2\x88\x9E");
#endif
#ifdef LOCALE_SNEGINFINITY
        symbols.NegativeInfinity = LocaleString(LOCALE_SNEGINFINITY, symbols.Negative + "\xE2\x88\x9E");
#endif
#else
        locale_t numericLocale = newlocale(LC_NUMERIC_MASK, "", nullptr);
        if (numericLocale != static_cast<locale_t>(0))
        {
            const char* decimal = nl_langinfo_l(RADIXCHAR, numericLocale);
            if (decimal != nullptr && *decimal != '\0')
            {
                symbols.Decimal = decimal;
            }
            freelocale(numericLocale);
        }
        locale_t monetaryLocale = newlocale(LC_MONETARY_MASK, "", nullptr);
        if (monetaryLocale != static_cast<locale_t>(0))
        {
#if defined(NEGATIVE_SIGN)
            const char* negative = nl_langinfo_l(NEGATIVE_SIGN, monetaryLocale);
            if (negative != nullptr && *negative != '\0')
            {
                symbols.Negative = negative;
            }
#endif
            freelocale(monetaryLocale);
        }
        symbols.NegativeInfinity = symbols.Negative + symbols.PositiveInfinity;
#endif
        return symbols;
    }

    [[nodiscard]] std::string ApplyCurrentSymbols(
        std::string text, const NumberSymbols& symbols)
    {
        if (!text.empty() && text.front() == '-')
        {
            text.erase(text.begin());
            text.insert(0, symbols.Negative);
        }
        const std::size_t dot = text.find('.');
        if (dot != std::string::npos)
        {
            text.replace(dot, 1, symbols.Decimal);
        }
        return text;
    }

    [[nodiscard]] std::string Int32Text(std::int32_t value)
    {
        std::array<char, 32> buffer{};
        auto result = std::to_chars(buffer.data(), buffer.data() + buffer.size(), value);
        return ApplyCurrentSymbols(
            std::string(buffer.data(), static_cast<std::size_t>(result.ptr - buffer.data())),
            CurrentNumberSymbols());
    }

    [[nodiscard]] std::string UInt32Text(std::uint32_t value)
    {
        std::array<char, 32> buffer{};
        auto result = std::to_chars(buffer.data(), buffer.data() + buffer.size(), value);
        return std::string(
            buffer.data(), static_cast<std::size_t>(result.ptr - buffer.data()));
    }

    [[nodiscard]] std::string Int64Text(std::int64_t value)
    {
        std::array<char, 64> buffer{};
        auto result = std::to_chars(buffer.data(), buffer.data() + buffer.size(), value);
        return ApplyCurrentSymbols(
            std::string(buffer.data(), static_cast<std::size_t>(result.ptr - buffer.data())),
            CurrentNumberSymbols());
    }

    void IncrementDecimal(std::string& digits)
    {
        std::size_t index = digits.size();
        while (index > 0 && digits[index - 1] == '9')
        {
            digits[index - 1] = '0';
            --index;
        }
        if (index == 0)
        {
            digits.insert(digits.begin(), '1');
        }
        else
        {
            digits[index - 1]++;
        }
    }

    template <typename TFloat>
    [[nodiscard]] std::string NumberTextCore(
        TFloat value, std::int32_t minDecimals, std::int32_t maxDecimals,
        std::int32_t precision)
    {
        const NumberSymbols symbols = CurrentNumberSymbols();
        if (std::isnan(value))
        {
            return symbols.NaN;
        }
        if (std::isinf(value))
        {
            return std::signbit(value)
                ? symbols.NegativeInfinity
                : symbols.PositiveInfinity;
        }

        const bool negative = std::signbit(value);
        const TFloat magnitude = std::fabs(value);
        std::string scaled = "0";

        if (magnitude != static_cast<TFloat>(0))
        {
            // .NET 9 intentionally generates MaxPrecisionCustomFormat significant
            // digits first (15 for Double, 7 for Single), then NumberToStringFormat
            // rounds that decimal buffer for the custom format. This preserves the
            // runtime's historical double-rounding behavior.
            std::array<char, 96> buffer{};
            const auto converted = std::to_chars(
                buffer.data(), buffer.data() + buffer.size(), magnitude,
                std::chars_format::scientific, precision - 1);
            if (converted.ec != std::errc{})
            {
                throw std::runtime_error("Floating-point formatting failed.");
            }

            const std::string_view scientific(
                buffer.data(), static_cast<std::size_t>(converted.ptr - buffer.data()));
            std::size_t exponentMarker = scientific.find('e');
            if (exponentMarker == std::string_view::npos)
            {
                exponentMarker = scientific.find('E');
            }
            if (exponentMarker == std::string_view::npos)
            {
                throw std::runtime_error("Floating-point formatting failed.");
            }

            std::string significant;
            significant.reserve(static_cast<std::size_t>(precision));
            for (std::size_t index = 0; index < exponentMarker; ++index)
            {
                const char unit = scientific[index];
                if (unit == '.')
                {
                    continue;
                }
                if (unit < '0' || unit > '9')
                {
                    throw std::runtime_error("Floating-point formatting failed.");
                }
                significant.push_back(unit);
            }

            const char* exponentFirst = scientific.data() + exponentMarker + 1;
            const char* const exponentLast = scientific.data() + scientific.size();
            bool exponentNegative = false;
            if (exponentFirst != exponentLast
                && (*exponentFirst == '+' || *exponentFirst == '-'))
            {
                exponentNegative = *exponentFirst == '-';
                ++exponentFirst;
            }
            std::int32_t exponent = 0;
            const auto parsed = std::from_chars(exponentFirst, exponentLast, exponent);
            if (parsed.ec != std::errc{} || parsed.ptr != exponentLast)
            {
                throw std::runtime_error("Floating-point formatting failed.");
            }
            if (exponentNegative)
            {
                exponent = -exponent;
            }

            const std::int32_t shift
                = exponent - (precision - 1) + maxDecimals;
            if (shift >= 0)
            {
                scaled = significant;
                scaled.append(static_cast<std::size_t>(shift), '0');
            }
            else
            {
                const std::int32_t discarded = -shift;
                const std::int32_t kept
                    = static_cast<std::int32_t>(significant.size()) - discarded;
                bool roundUp = false;
                if (kept > 0)
                {
                    scaled.assign(significant, 0, static_cast<std::size_t>(kept));
                    if (static_cast<std::size_t>(kept) < significant.size())
                    {
                        roundUp = significant[static_cast<std::size_t>(kept)] >= '5';
                    }
                }
                else if (kept == 0)
                {
                    scaled = "0";
                    roundUp = !significant.empty() && significant.front() >= '5';
                }
                else
                {
                    scaled = "0";
                }
                if (roundUp)
                {
                    IncrementDecimal(scaled);
                }
            }

            const std::size_t firstNonZero = scaled.find_first_not_of('0');
            if (firstNonZero == std::string::npos)
            {
                scaled = "0";
            }
            else if (firstNonZero != 0)
            {
                scaled.erase(0, firstNonZero);
            }
        }

        std::string result = scaled;
        if (maxDecimals > 0)
        {
            const std::size_t decimals = static_cast<std::size_t>(maxDecimals);
            if (result.size() <= decimals)
            {
                result.insert(0, decimals + 1 - result.size(), '0');
            }
            result.insert(result.end() - static_cast<std::ptrdiff_t>(decimals), '.');
            if (maxDecimals > minDecimals)
            {
                const std::size_t decimalPoint = result.find('.');
                while (result.size() > decimalPoint + 1U
                        + static_cast<std::size_t>(minDecimals)
                    && result.back() == '0')
                {
                    result.pop_back();
                }
                if (minDecimals == 0 && result.back() == '.')
                {
                    result.pop_back();
                }
            }
        }

        // NumberToStringFormat clears the visible sign when custom-format
        // rounding produces zero, including negative zero itself.
        if (negative && scaled != "0")
        {
            result.insert(0, symbols.Negative);
        }
        const std::size_t decimalPoint = result.find('.');
        if (decimalPoint != std::string::npos)
        {
            result.replace(decimalPoint, 1, symbols.Decimal);
        }
        return result;
    }

    [[nodiscard]] std::string NumberText(
        double value, std::int32_t minDecimals, std::int32_t maxDecimals)
    {
        return NumberTextCore(value, minDecimals, maxDecimals, 15);
    }

    [[nodiscard]] std::string NumberText(
        float value, std::int32_t minDecimals, std::int32_t maxDecimals)
    {
        return NumberTextCore(value, minDecimals, maxDecimals, 7);
    }

    [[nodiscard]] std::string HunterName(Hunter hunter)
    {
        switch (hunter)
        {
            case Hunter::Samus: return "Samus";
            case Hunter::Kanden: return "Kanden";
            case Hunter::Trace: return "Trace";
            case Hunter::Sylux: return "Sylux";
            case Hunter::Noxus: return "Noxus";
            case Hunter::Spire: return "Spire";
            case Hunter::Weavel: return "Weavel";
            case Hunter::Guardian: return "Guardian";
            case Hunter::Random: return "Random";
        }
        return Int32Text(static_cast<std::uint8_t>(hunter));
    }

    [[nodiscard]] std::string TestPhaseName(TestPhase phase)
    {
        switch (phase)
        {
            case TestPhase::Idle: return "Idle";
            case TestPhase::Walk: return "Walk";
            case TestPhase::Jump: return "Jump";
            case TestPhase::Turn: return "Turn";
            case TestPhase::Shoot: return "Shoot";
            case TestPhase::SwitchWeapons: return "SwitchWeapons";
            case TestPhase::Charge: return "Charge";
            case TestPhase::MorphA: return "MorphA";
            case TestPhase::AltAttackA: return "AltAttackA";
            case TestPhase::MorphB: return "MorphB";
            case TestPhase::AltAttackB: return "AltAttackB";
            case TestPhase::Unmorph: return "Unmorph";
            case TestPhase::Zoom: return "Zoom";
            case TestPhase::Afflict: return "Afflict";
            case TestPhase::Duel: return "Duel";
        }
        return Int32Text(static_cast<std::int32_t>(phase));
    }

    [[nodiscard]] bool IsMorphPhase(TestPhase phase) noexcept
    {
        return phase == TestPhase::MorphA || phase == TestPhase::AltAttackA
            || phase == TestPhase::MorphB || phase == TestPhase::AltAttackB;
    }

    [[nodiscard]] bool IsUnmorphSamplePhase(TestPhase phase) noexcept
    {
        return phase == TestPhase::Zoom || phase == TestPhase::Duel;
    }

    [[nodiscard]] MphRead::Entities::PlayerEntity& RequirePlayer(
        const std::shared_ptr<MphRead::Entities::PlayerEntity>& player)
    {
        if (!player)
        {
            throw System::NullReferenceException();
        }
        return *player;
    }

    [[nodiscard]] std::string Join(const std::vector<std::string>& values)
    {
        std::string result;
        for (std::size_t i = 0; i < values.size(); ++i)
        {
            if (i != 0)
            {
                result += ", ";
            }
            result += values[i];
        }
        return result;
    }
}

namespace MphRead::Mods::Network
{
    class NetFeatureCheck::Record final
    {
    public:
        std::int32_t SpawnedFrames = 0;
        std::int32_t MovedFrames = 0;
        double Travelled = 0.0;
        float MinY = std::numeric_limits<float>::max();
        float MaxY = std::numeric_limits<float>::lowest();
        double FacingDegrees = 0.0;
        std::int32_t BeamFrames = 0;
        std::int32_t ShotsFired = 0;
        std::int32_t LastFiredTotal = 0;
        std::int32_t BombFrames = 0;
        std::int32_t HalfturretFrames = 0;
        std::int32_t AltFormFrames = 0;
        std::int32_t AltFormInMorphPhase = 0;
        std::int32_t BipedInUnmorphPhase = 0;
        std::int32_t WeaponChanges = 0;
        std::int32_t AltAttackPresses = 0;
        std::int32_t DamageEvents = 0;
        std::int32_t DamageInAltForm = 0;
        std::int32_t Deaths = 0;
        std::int32_t ZoomFrames = 0;
        std::int32_t FrozenFrames = 0;
        std::int32_t DisruptedFrames = 0;
        std::int32_t BurningFrames = 0;
        std::int32_t SpectatingFrames = 0;
        std::int32_t DoubleDamageFrames = 0;
        Vector3 LastPosition{};
        Vector3 LastFramePosition{};
        bool HaveFramePrevious = false;
        Vector3 LastFacing{};
        bool HavePrevious = false;
        std::int32_t LastHealth = -1;
        BeamType LastWeapon = BeamType::None;
        bool WasAlive = false;
        MphRead::Hunter Hunter{};
        std::int32_t FormDisagreeFrames = 0;
        std::int32_t FormDisagreeRun = 0;
        std::int32_t WorstFormDisagreeRun = 0;
        std::string WorstFormContext{};
        double WorstPositionGap = 0.0;
        double WorstStep = 0.0;
        std::int32_t Teleports = 0;
        bool EverCompared = false;
        std::int32_t FramesSinceRespawn = 0;
        std::int32_t FramesSinceLaunch = 0;
        std::int32_t LastWorldEvents = 0;
    };

    std::array<NetFeatureCheck::Feature, 23> NetFeatureCheck::_features{{
        {"spawn", [](const Record& r) -> double { return r.SpawnedFrames; }},
        {"movement", [](const Record& r) -> double { return r.Travelled; }},
        {"jump", &NetFeatureCheck::Height},
        {"facing", [](const Record& r) -> double { return r.FacingDegrees; }},
        {"shooting", [](const Record& r) -> double { return r.BeamFrames; }},
        {"shots", [](const Record& r) -> double { return r.ShotsFired; }},
        {"weapon-switch", [](const Record& r) -> double { return r.WeaponChanges; }},
        {"alt-attack", [](const Record& r) -> double { return r.AltAttackPresses; }},
        {"alt-form", [](const Record& r) -> double { return r.AltFormInMorphPhase; }},
        {"alt-form-total", [](const Record& r) -> double { return r.AltFormFrames; }},
        {"unmorph", [](const Record& r) -> double { return r.BipedInUnmorphPhase; }},
        {"bombs", [](const Record& r) -> double { return r.BombFrames; }},
        {"halfturret", [](const Record& r) -> double { return r.HalfturretFrames; }},
        {"zoom", [](const Record& r) -> double { return r.ZoomFrames; }},
        {"frozen", [](const Record& r) -> double { return r.FrozenFrames; }},
        {"disrupted", [](const Record& r) -> double { return r.DisruptedFrames; }},
        {"burning", [](const Record& r) -> double { return r.BurningFrames; }},
        {"spectating", [](const Record& r) -> double { return r.SpectatingFrames; }},
        {"double-damage", [](const Record& r) -> double { return r.DoubleDamageFrames; }},
        {"damage-taken", [](const Record& r) -> double { return r.DamageEvents; }},
        {"hit-in-alt-form", [](const Record& r) -> double { return r.DamageInAltForm; }},
        {"deaths", [](const Record& r) -> double { return r.Deaths; }},
        {"teleports", [](const Record& r) -> double { return r.Teleports; }}
    }};

    NetFeatureCheck::NetFeatureCheck()
        : _records(ManagedArrayLength(Entities::PlayerEntity::MaxPlayers())),
          Boards(_boards)
    {
        for (std::unique_ptr<Record>& record : _records)
        {
            record = std::make_unique<Record>();
        }
    }

    NetFeatureCheck::~NetFeatureCheck() = default;

    NetFeatureCheck::Record& NetFeatureCheck::RecordAt(std::int32_t slot)
    {
        if (slot < 0 || static_cast<std::size_t>(slot) >= _records.size())
        {
            throw std::out_of_range("Index was outside the bounds of the array.");
        }
        return *_records[static_cast<std::size_t>(slot)];
    }

    const NetFeatureCheck::Record& NetFeatureCheck::RecordAt(std::int32_t slot) const
    {
        if (slot < 0 || static_cast<std::size_t>(slot) >= _records.size())
        {
            throw std::out_of_range("Index was outside the bounds of the array.");
        }
        return *_records[static_cast<std::size_t>(slot)];
    }

    void NetFeatureCheck::IncrementPhase(TestPhase phase)
    {
        for (auto& pair : _phaseFrames)
        {
            if (pair.first == phase)
            {
                pair.second = AddInt32(pair.second, 1);
                return;
            }
        }
        _phaseFrames.emplace_back(phase, 1);
    }

    void NetFeatureCheck::Reset()
    {
        for (std::unique_ptr<Record>& record : _records)
        {
            record = std::make_unique<Record>();
        }
    }

    void NetFeatureCheck::Count(
        std::span<std::int32_t> counts, Entities::EntityBase* owner)
    {
        if (auto* player = dynamic_cast<Entities::PlayerEntity*>(owner);
            player != nullptr && player->SlotIndex() >= 0
            && player->SlotIndex() < static_cast<std::int32_t>(counts.size()))
        {
            const std::int32_t index = player->SlotIndex();
            if (index < 0 || static_cast<std::size_t>(index) >= counts.size())
            {
                throw std::out_of_range("Index was outside the bounds of the array.");
            }
            std::int32_t& value = counts[static_cast<std::size_t>(index)];
            value = AddInt32(value, 1);
        }
        else if (auto* turret = dynamic_cast<Entities::HalfturretEntity*>(owner);
            turret != nullptr)
        {
            const std::shared_ptr<Entities::PlayerEntity> firstOwner = turret->Owner();
            if (!firstOwner)
            {
                throw System::NullReferenceException();
            }
            if (firstOwner->SlotIndex() >= 0)
            {
                const std::shared_ptr<Entities::PlayerEntity> secondOwner = turret->Owner();
                if (!secondOwner)
                {
                    throw System::NullReferenceException();
                }
                if (secondOwner->SlotIndex() < static_cast<std::int32_t>(counts.size()))
                {
                    const std::shared_ptr<Entities::PlayerEntity> thirdOwner = turret->Owner();
                    if (!thirdOwner)
                    {
                        throw System::NullReferenceException();
                    }
                    const std::int32_t index = thirdOwner->SlotIndex();
                    if (index < 0 || static_cast<std::size_t>(index) >= counts.size())
                    {
                        throw std::out_of_range("Index was outside the bounds of the array.");
                    }
                    std::int32_t& value = counts[static_cast<std::size_t>(index)];
                    value = AddInt32(value, 1);
                }
            }
        }
    }

    void NetFeatureCheck::Observe(MphRead::Scene& scene)
    {
        Mods::WorldEvents::SetWatching(true);
        _localSlot = std::max(NetSession::LocalSlot(), 0);

        const bool localSamplePath
            = NetSession::NetFrame() % NetConfig::IntentSendInterval == 0;
        const TestPhase phase = NetTestScript::Phase();
        IncrementPhase(phase);

        const std::size_t beamCount = ManagedArrayLength(
            Entities::PlayerEntity::MaxPlayers());
        std::vector<std::int32_t> beams(beamCount, 0);
        const std::size_t bombCount = ManagedArrayLength(
            Entities::PlayerEntity::MaxPlayers());
        std::vector<std::int32_t> bombs(bombCount, 0);
        const std::size_t turretCount = ManagedArrayLength(
            Entities::PlayerEntity::MaxPlayers());
        std::vector<std::int32_t> turrets(turretCount, 0);

        {
            auto enumerator = scene.Entities().GetEnumerator();
            while (enumerator.MoveNext())
            {
                const std::shared_ptr<Entities::EntityBase> entity = enumerator.Current();
                if (!entity)
                {
                    throw System::NullReferenceException();
                }
                if (entity->Type == EntityType::BeamProjectile)
                {
                    const auto beam
                        = std::dynamic_pointer_cast<Entities::BeamProjectileEntity>(entity);
                    Count(beams, beam ? beam->Owner().get() : nullptr);
                }
                else if (entity->Type == EntityType::Bomb)
                {
                    const auto bomb
                        = std::dynamic_pointer_cast<Entities::BombEntity>(entity);
                    Count(bombs, bomb ? bomb->Owner() : nullptr);
                }
                else if (entity->Type == EntityType::Halfturret)
                {
                    const auto turret
                        = std::dynamic_pointer_cast<Entities::HalfturretEntity>(entity);
                    Count(turrets, turret ? turret->Owner().get() : nullptr);
                }
            }
        }

        _itemsNow = 0;
        {
            auto enumerator = scene.GetItemInstanceEntities().GetEnumerator();
            while (enumerator.MoveNext())
            {
                const std::shared_ptr<Entities::ItemInstanceEntity> item = enumerator.Current();
                static_cast<void>(item);
                IncrementInt32(_itemsNow);
            }
        }
        if (_lastItemCount > _itemsNow)
        {
            _itemsPickedUp = AddInt32(
                _itemsPickedUp, SubInt32(_lastItemCount, _itemsNow));
        }
        _lastItemCount = _itemsNow;
        IncrementInt32(_itemSamples);
        _itemTotal = AddInt64(_itemTotal, _itemsNow);

        for (std::int32_t slot = 0; slot < Entities::PlayerEntity::MaxPlayers(); ++slot)
        {
            if (static_cast<std::size_t>(slot) >= Entities::PlayerEntity::Players().size())
            {
                continue;
            }
            Entities::PlayerEntity& player = RequirePlayer(
                Entities::PlayerEntity::Players().at(static_cast<std::size_t>(slot)));
            if (!TestFlag(player.LoadFlags(), LoadFlags::Active))
            {
                continue;
            }

            Record& record = RecordAt(slot);
            record.Hunter = player.Hunter();

            if (beams.at(static_cast<std::size_t>(slot)) > 0)
            {
                IncrementInt32(record.BeamFrames);
            }

            const std::int32_t firedTotal
                = NetDamage::Fired.at(static_cast<std::size_t>(slot));
            if (firedTotal > record.LastFiredTotal)
            {
                record.ShotsFired = AddInt32(
                    record.ShotsFired, SubInt32(firedTotal, record.LastFiredTotal));
            }
            record.LastFiredTotal = firedTotal;

            if (bombs.at(static_cast<std::size_t>(slot)) > 0)
            {
                IncrementInt32(record.BombFrames);
            }
            if (player.Controls().AltAttack.IsPressed())
            {
                IncrementInt32(record.AltAttackPresses);
            }
            if (turrets.at(static_cast<std::size_t>(slot)) > 0)
            {
                IncrementInt32(record.HalfturretFrames);
            }

            if (!TestFlag(player.LoadFlags(), LoadFlags::Spawned))
            {
                continue;
            }

            IncrementInt32(record.SpawnedFrames);
            if (player.IsAltForm())
            {
                IncrementInt32(record.AltFormFrames);
                if (IsMorphPhase(phase))
                {
                    IncrementInt32(record.AltFormInMorphPhase);
                }
            }
            else if (IsUnmorphSamplePhase(phase))
            {
                IncrementInt32(record.BipedInUnmorphPhase);
            }

            const std::shared_ptr<EquipInfo> equipInfo = player.EquipInfo();
            if (!equipInfo)
            {
                throw System::NullReferenceException();
            }
            if (equipInfo->Zoomed())
            {
                IncrementInt32(record.ZoomFrames);
            }
            if (player.ModFrozen())
            {
                IncrementInt32(record.FrozenFrames);
            }
            if (player.ModDisrupted())
            {
                IncrementInt32(record.DisruptedFrames);
            }
            if (player.ModBurning())
            {
                IncrementInt32(record.BurningFrames);
            }
            if (slot == _localSlot
                    ? SpectatorMode::IsSpectating()
                    : TestFlag(player.Flags2(), PlayerFlags2::Spectating))
            {
                IncrementInt32(record.SpectatingFrames);
            }
            if (player.DoubleDamage())
            {
                IncrementInt32(record.DoubleDamageFrames);
            }
            if (player.CurrentWeapon() != record.LastWeapon)
            {
                if (record.LastWeapon != BeamType::None)
                {
                    IncrementInt32(record.WeaponChanges);
                }
                record.LastWeapon = player.CurrentWeapon();
            }
            if (record.LastHealth > 0 && player.Health() > 0
                && player.Health() < record.LastHealth)
            {
                IncrementInt32(record.DamageEvents);
                if (player.IsAltForm())
                {
                    IncrementInt32(record.DamageInAltForm);
                }
            }
            if (record.WasAlive && player.Health() == 0)
            {
                IncrementInt32(record.Deaths);
            }

            const std::int32_t jumpPads = Mods::WorldEvents::JumpPadsFor(slot);
            const std::int32_t teleports = Mods::WorldEvents::TeleportsFor(slot);
            const std::int32_t worldEvents = AddInt32(jumpPads, teleports);
            record.FramesSinceLaunch = worldEvents != record.LastWorldEvents
                ? 0
                : AddInt32(record.FramesSinceLaunch, 1);
            record.LastWorldEvents = worldEvents;
            record.FramesSinceRespawn
                = player.Health() > record.LastHealth || player.Health() == 0
                ? 0
                : AddInt32(record.FramesSinceRespawn, 1);
            record.LastHealth = player.Health();
            record.WasAlive = player.Health() > 0;

            const float minYCurrent = record.MinY;
            const float minYPosition = player.Position.Y;
            record.MinY = ManagedMin(minYCurrent, minYPosition);
            const float maxYCurrent = record.MaxY;
            const float maxYPosition = player.Position.Y;
            record.MaxY = ManagedMax(maxYCurrent, maxYPosition);

            if (record.HaveFramePrevious)
            {
                const Vector3 framePosition = static_cast<Vector3>(player.Position);
                const float frameStep = Length(framePosition - record.LastFramePosition);
                if (frameStep > 0.01F)
                {
                    IncrementInt32(record.MovedFrames);
                }
                if (frameStep > TeleportStep && record.FramesSinceRespawn > 60
                    && record.FramesSinceLaunch > LaunchGraceFrames)
                {
                    IncrementInt32(record.Teleports);
                    const double worstStepCurrent = record.WorstStep;
                    record.WorstStep
                        = ManagedMax(worstStepCurrent, static_cast<double>(frameStep));
                }
            }
            record.LastFramePosition = static_cast<Vector3>(player.Position);
            record.HaveFramePrevious = true;

            const bool samplePath = slot != _localSlot || localSamplePath;
            if (samplePath && record.HavePrevious)
            {
                const Vector3 pathPosition = static_cast<Vector3>(player.Position);
                const float step = Length(pathPosition - record.LastPosition);
                if (step < 5.0F)
                {
                    record.Travelled += step;
                }
                const Vector3 gunVector = player.ModGunVector();
                const float rawDot = Vector3::Dot(gunVector, record.LastFacing);
                const float dot = std::clamp(rawDot, -1.0F, 1.0F);
                const float radians = std::acos(dot);
                constexpr float RadToDeg = 180.0F / 3.1415927F;
                const float degrees = radians * RadToDeg;
                record.FacingDegrees += degrees;
            }
            if (samplePath)
            {
                record.LastPosition = static_cast<Vector3>(player.Position);
                record.LastFacing = player.ModGunVector();
                record.HavePrevious = true;
            }

            if (slot == _localSlot
                || !NetSession::RemoteStateValid.at(static_cast<std::size_t>(slot)))
            {
                continue;
            }
            if (NetSession::IsAuthority())
            {
                continue;
            }

            record.EverCompared = true;
            const PlayerState state
                = NetSession::RemoteStates.at(static_cast<std::size_t>(slot));
            const bool wantAlt = (state.Flags & PlayerState::FlagAltForm) != 0;
            const bool visible = player.Health() > 0
                && (state.Flags & PlayerState::FlagSpawned) != 0;
            if (visible && wantAlt != player.IsAltForm())
            {
                IncrementInt32(record.FormDisagreeFrames);
                IncrementInt32(record.FormDisagreeRun);
                if (record.FormDisagreeRun > record.WorstFormDisagreeRun)
                {
                    record.WorstFormDisagreeRun = record.FormDisagreeRun;
                    std::string context = "phase ";
                    context += TestPhaseName(phase);
                    context += ", authority wanted ";
                    context += wantAlt ? "alt" : "biped";
                    context += ", puppet ";
                    const std::string formState = player.ModFormState();
                    context += formState;
                    context += ", hp ";
                    const std::int32_t health = player.Health();
                    context += Int32Text(health);
                    record.WorstFormContext = std::move(context);
                }
            }
            else
            {
                record.FormDisagreeRun = 0;
            }
            if (!visible)
            {
                continue;
            }
            if ((state.Flags & PlayerState::FlagSpawned) != 0)
            {
                const Vector3 authorityPosition = state.Position;
                const Vector3 puppetPosition = static_cast<Vector3>(player.Position);
                const double gap = static_cast<double>(
                    Length(authorityPosition - puppetPosition));
                const double worstPositionGapCurrent = record.WorstPositionGap;
                record.WorstPositionGap = ManagedMax(worstPositionGapCurrent, gap);
            }
        }
    }

    bool NetFeatureCheck::Report(std::int32_t& failures)
    {
        failures = 0;
        Record& mine = RecordAt(_localSlot);
        const std::string me
            = GameState::Nicknames().at(static_cast<std::size_t>(_localSlot));

        ConsoleWriteLine();
        ConsoleWriteLine(
            "  feature coverage (mine = what my player did, "
            "theirs = what I saw of them)");

        std::int32_t fails = 0;
        bool anyRemote = false;

        auto emit = [&me](
            std::string_view kind, const std::string& subject,
            std::string_view feature, double value)
        {
            std::string text = "  netcheck ";
            text += me;
            text += ' ';
            text += kind;
            text += ' ';
            text += subject;
            text += ' ';
            text += feature;
            text += ' ';
            text += NumberText(value, 0, 2);
            ConsoleWriteLine(text);
        };

        for (const Feature& feature : _features)
        {
            emit("mine", me, feature.Name, feature.Get(mine));
        }

        for (std::int32_t slot = 0;
            static_cast<std::size_t>(slot) < _records.size(); ++slot)
        {
            if (slot == _localSlot || RecordAt(slot).SpawnedFrames == 0)
            {
                continue;
            }
            anyRemote = true;
            Record& other = RecordAt(slot);
            const std::string them
                = GameState::Nicknames().at(static_cast<std::size_t>(slot));
            std::string heading = "    --- as I saw ";
            heading += them;
            heading += " (slot ";
            heading += Int32Text(slot);
            heading += ", ";
            heading += HunterName(other.Hunter);
            heading += ") ---";
            ConsoleWriteLine(heading);
            for (const Feature& feature : _features)
            {
                emit("saw", them, feature.Name, feature.Get(other));
            }
            fails = AddInt32(fails, ReportOne(mine, other, them));
        }

        if (!anyRemote)
        {
            ConsoleWriteLine(
                "    no other player was ever spawned in this scene -- nothing to compare");
            failures = 1;
            return false;
        }

        std::vector<std::string> invulnerable;
        for (std::int32_t slot = 0;
            slot < Entities::PlayerEntity::SlotCapacity
                && static_cast<std::size_t>(slot) < Entities::PlayerEntity::Players().size();
            ++slot)
        {
            const std::shared_ptr<Entities::PlayerEntity> player
                = Entities::PlayerEntity::Players().at(static_cast<std::size_t>(slot));
            if (RecordAt(slot).SpawnedFrames > 0)
            {
                if (!player)
                {
                    throw System::NullReferenceException();
                }
                if (TestFlag(player->LoadFlags(), LoadFlags::Spawned)
                    && !player->ModCanBeHurt())
                {
                    const std::string nickname
                        = GameState::Nicknames().at(static_cast<std::size_t>(slot));
                    invulnerable.push_back(
                        nickname + " (slot " + Int32Text(slot) + ")");
                }
            }
        }
        if (!invulnerable.empty())
        {
            const std::string text
                = "    FAIL: no beam can hurt these players at all: " + Join(invulnerable);
            ConsoleWriteLine(text);
            IncrementInt32(fails);
        }

        std::vector<std::string> untouched;
        for (std::int32_t slot = 0;
            static_cast<std::size_t>(slot) < _records.size(); ++slot)
        {
            if (RecordAt(slot).SpawnedFrames > 600
                && RecordAt(slot).DamageEvents == 0)
            {
                const std::string nickname
                    = GameState::Nicknames().at(static_cast<std::size_t>(slot));
                untouched.push_back(
                    nickname + " (slot " + Int32Text(slot) + ")");
            }
        }
        if (!untouched.empty())
        {
            const std::string text
                = "    FAIL: never took a single hit: " + Join(untouched);
            ConsoleWriteLine(text);
            IncrementInt32(fails);
        }

        if (_localSlot < static_cast<std::int32_t>(Entities::PlayerEntity::Players().size()))
        {
            Entities::PlayerEntity& player = RequirePlayer(
                Entities::PlayerEntity::Players().at(static_cast<std::size_t>(_localSlot)));
            const auto [rows, height] = player.ModScoreboardSize();
            const bool fits = height <= 192.0F;
            std::string text = "    scoreboard: ";
            text += Int32Text(rows);
            text += " row(s), ";
            text += NumberText(height, 0, 0);
            text += " px tall ";
            text += fits ? "(fits)" : "(OVERFLOWS the screen)";
            ConsoleWriteLine(text);
            if (!fits)
            {
                IncrementInt32(fails);
            }
        }

        const std::int32_t itemsNow = _itemsNow;
        double itemAverage = 0.0;
        const std::int32_t itemSamplesCondition = _itemSamples;
        if (itemSamplesCondition > 0)
        {
            const std::int64_t itemTotalForAverage = _itemTotal;
            const std::int32_t itemSamplesDivisor = _itemSamples;
            itemAverage = static_cast<double>(itemTotalForAverage)
                / static_cast<double>(itemSamplesDivisor);
        }
        const std::int32_t itemsPickedUp = _itemsPickedUp;
        std::string items = "    items: ";
        items += Int32Text(itemsNow);
        items += " on the map now, ";
        items += NumberText(itemAverage, 1, 1);
        items += " on average, ";
        items += Int32Text(itemsPickedUp);
        items += " taken or expired";
        ConsoleWriteLine(items);

        const std::string currentBoard = Scoreboard("    scoreboard as I see it:");
        ConsoleWriteLine(currentBoard);
        for (const auto& pair : Boards)
        {
            ConsoleWriteLine(pair.second);
        }
        if (!Boards.empty())
        {
        }

        std::string phases = "    phases seen:";
        for (const auto& pair : _phaseFrames)
        {
            std::string entry = " ";
            entry += TestPhaseName(pair.first);
            entry += '=';
            entry += Int32Text(pair.second);
            phases += entry;
        }
        ConsoleWriteLine(phases);

        std::string pipeline = "    damage pipeline (resolved here / replayed here):";
        for (std::int32_t slot = 0; slot < Entities::PlayerEntity::SlotCapacity; ++slot)
        {
            if (RecordAt(slot).SpawnedFrames == 0)
            {
                continue;
            }
            const std::size_t index = static_cast<std::size_t>(slot);
            std::string entry = " [";
            entry += Int32Text(slot);
            entry += "] ";
            const std::int32_t resolved = NetDamage::Resolved.at(index);
            entry += Int32Text(resolved);
            entry += '/';
            const std::int32_t replayed = NetDamage::Replayed.at(index);
            entry += Int32Text(replayed);
            pipeline += entry;
        }
        ConsoleWriteLine(pipeline);

        std::string fired = "    shots spawned here (per slot):";
        for (std::int32_t slot = 0; slot < Entities::PlayerEntity::SlotCapacity; ++slot)
        {
            if (RecordAt(slot).SpawnedFrames == 0)
            {
                continue;
            }
            const std::size_t index = static_cast<std::size_t>(slot);
            const std::int32_t conditionFired = NetDamage::Fired.at(index);
            double avg = 0.0;
            if (conditionFired > 0)
            {
                const double aimDrift = NetDamage::AimDrift.at(index);
                const std::int32_t divisorFired = NetDamage::Fired.at(index);
                avg = aimDrift / divisorFired;
            }
            std::string entry = " [";
            entry += Int32Text(slot);
            entry += "] ";
            const std::int32_t displayedFired = NetDamage::Fired.at(index);
            entry += Int32Text(displayedFired);
            entry += "(drift ";
            entry += NumberText(avg, 1, 1);
            entry += '/';
            const double worstDrift = NetDamage::WorstDrift.at(index);
            entry += NumberText(worstDrift, 1, 1);
            entry += " deg)";
            fired += entry;
        }
        ConsoleWriteLine(fired);

        std::string collision = "    player collision checks:";
        for (std::int32_t slot = 0; slot < Entities::PlayerEntity::SlotCapacity; ++slot)
        {
            if (RecordAt(slot).SpawnedFrames == 0)
            {
                continue;
            }
            const std::size_t index = static_cast<std::size_t>(slot);
            std::string entry = " [";
            entry += Int32Text(slot);
            entry += "] ";
            const std::int32_t checks = NetDamage::PlayerChecks.at(index);
            entry += Int32Text(checks);
            entry += '/';
            const std::int32_t overlaps = NetDamage::PlayerOverlaps.at(index);
            entry += Int32Text(overlaps);
            entry += '/';
            const std::int32_t accepted = NetDamage::PlayerAccepted.at(index);
            entry += Int32Text(accepted);
            collision += entry;
        }
        ConsoleWriteLine(collision);

        std::string pairs = "    player overlaps by shooter:";
        for (std::int32_t shooter = 0; shooter < Entities::PlayerEntity::SlotCapacity; ++shooter)
        {
            for (std::int32_t target = 0; target < Entities::PlayerEntity::SlotCapacity; ++target)
            {
                const std::int32_t count = NetDamage::PlayerOverlapsByShooter
                    .at(static_cast<std::size_t>(shooter))
                    .at(static_cast<std::size_t>(target));
                if (count > 0)
                {
                    std::string entry = " [";
                    entry += Int32Text(shooter);
                    entry += "->";
                    entry += Int32Text(target);
                    entry += "] ";
                    entry += Int32Text(count);
                    pairs += entry;
                }
            }
        }
        ConsoleWriteLine(pairs);

        std::string authority = "    authority for ";
        const std::int64_t authorityFrames = NetSession::AuthorityFrames();
        authority += Int64Text(authorityFrames);
        authority += " frame(s) of ";
        const std::uint32_t netFrame = NetSession::NetFrame();
        authority += UInt32Text(netFrame);
        ConsoleWriteLine(authority);

        std::string snaps = "    remote position snaps: ";
        const std::int64_t snapCount = NetPlayerBridge::Snaps();
        snaps += Int64Text(snapCount);
        snaps += " (worst ";
        const float worstSnap = NetPlayerBridge::WorstSnap();
        snaps += NumberText(worstSnap, 1, 1);
        snaps += " units) -- these are the visible teleports";
        ConsoleWriteLine(snaps);

        std::string unresolved = "    node lookups unresolved: ";
        const std::int64_t unresolvedCount = NetPlayerBridge::NodeLookupsUnresolved;
        unresolved += Int64Text(unresolvedCount);
        unresolved += " -- these players are drawn uncalled";
        ConsoleWriteLine(unresolved);

        if (NetPlayerBridge::RejectedUpdates() > 0)
        {
            std::string rejected = "    FAIL: ";
            const std::int64_t rejectedCount = NetPlayerBridge::RejectedUpdates();
            rejected += Int64Text(rejectedCount);
            rejected += " update(s) rejected for holding impossible values";
            ConsoleWriteLine(rejected);
            IncrementInt32(fails);
        }

        failures = fails;
        return fails == 0;
    }

    void NetFeatureCheck::SampleScoreboard(std::int32_t serverSecond)
    {
        std::string prefix = "    scoreboard at t=";
        prefix += Int32Text(serverSecond);
        prefix += "s:";
        const std::string board = Scoreboard(prefix);
        Boards[serverSecond] = board;
    }

    std::string NetFeatureCheck::Scoreboard(const std::string& prefix) const
    {
        std::string board = prefix;
        for (std::int32_t slot = 0; slot < Entities::PlayerEntity::MaxPlayers(); ++slot)
        {
            if (RecordAt(slot).SpawnedFrames == 0)
            {
                continue;
            }
            const std::size_t index = static_cast<std::size_t>(slot);
            std::string entry = " [";
            entry += Int32Text(slot);
            entry += "] ";
            const std::string nickname = GameState::Nicknames().at(index);
            entry += nickname;
            entry += ' ';
            const std::int32_t kills = GameState::Kills().at(index);
            entry += Int32Text(kills);
            entry += "k/";
            const std::int32_t deaths = GameState::Deaths().at(index);
            entry += Int32Text(deaths);
            entry += "d/";
            const std::int32_t points = GameState::Points().at(index);
            entry += Int32Text(points);
            entry += 'p';
            board += entry;
        }
        return board;
    }

    std::int32_t NetFeatureCheck::ReportOne(
        const Record& mine, const Record& other, const std::string& them) const
    {
        std::string report;
        std::int32_t fails = 0;

        auto line = [&report, &fails](
            const std::string& feature, double self, double seen, double needed,
            const std::string& unit, bool applicable = true, bool pairwise = false)
        {
            const bool tested
                = applicable && (pairwise ? self + seen >= needed : self >= needed);
            const bool ok = seen >= needed;
            const char* verdict = !applicable ? "n/a"
                : !tested ? "untested"
                : pairwise ? "ok"
                : ok ? "ok"
                : "FAIL";
            if (tested && !pairwise && !ok)
            {
                IncrementInt32(fails);
            }

            std::string text = "    ";
            text += PadRightManaged(feature, 16);
            text += " mine ";
            text += PadLeftManaged(NumberText(self, 0, 0), 7);
            text += ' ';
            text += PadRightManaged(unit, 6);
            text += " theirs ";
            text += PadLeftManaged(NumberText(seen, 0, 0), 7);
            text += ' ';
            text += PadRightManaged(unit, 6);
            text += ' ';
            text += verdict;
            report += text;
            report += ManagedNewLine();
        };

        line("spawn", mine.SpawnedFrames, other.SpawnedFrames, 30, "frames");
        line("movement", mine.Travelled, other.Travelled, 5, "units");
        line("jump", Height(mine), Height(other), 1.5, "units");
        line("facing", mine.FacingDegrees, other.FacingDegrees, 180, "deg");
        line("shots", mine.ShotsFired, other.ShotsFired, 10, "shots");
        line("shooting", mine.BeamFrames, other.BeamFrames, 10, "beam-frames");
        line(
            "weapon switch", mine.WeaponChanges, other.WeaponChanges,
            2, "changes", true, true);
        line(
            "alt attack", mine.AltAttackPresses, other.AltAttackPresses,
            3, "presses", true, true);
        line(
            "alt form", mine.AltFormInMorphPhase, other.AltFormInMorphPhase,
            30, "frames");
        line(
            "unmorph", mine.BipedInUnmorphPhase, other.BipedInUnmorphPhase,
            30, "frames");
        line(
            "bombs", mine.BombFrames, other.BombFrames, 5, "frames",
            LaysBombs(other.Hunter));
        line(
            "halfturret", mine.HalfturretFrames, other.HalfturretFrames,
            5, "frames", other.Hunter == Hunter::Weavel);
        line("zoom", mine.ZoomFrames, other.ZoomFrames, 10, "frames");
        line(
            "double damage", mine.DoubleDamageFrames, other.DoubleDamageFrames,
            10, "frames");
        line(
            "taking damage", mine.DamageEvents, other.DamageEvents,
            1, "hits");
        line(
            "hit in alt form", mine.DamageInAltForm, other.DamageInAltForm,
            2, "hits", true, true);
        line(
            "deaths", mine.Deaths, other.Deaths,
            1, "deaths", true, true);

        ConsoleWrite(report);

        std::string jump = "    ";
        jump += them;
        jump += ": ";
        jump += Int32Text(other.Teleports);
        jump += " teleport(s), worst jump ";
        jump += NumberText(other.WorstStep, 1, 1);
        jump += " units";
        ConsoleWriteLine(jump);

        if (!other.EverCompared)
        {
            const std::string agreement = "    " + them
                + ": form and position agreement not measured here -- "
                + "this client is the authority and receives no snapshot to compare with";
            ConsoleWriteLine(agreement);
        }
        else
        {
            std::string agreement = "    ";
            agreement += them;
            agreement += ": form disagreed on ";
            agreement += Int32Text(other.FormDisagreeFrames);
            agreement += " frame(s) (longest run ";
            agreement += Int32Text(other.WorstFormDisagreeRun);
            agreement += "), worst position gap ";
            agreement += NumberText(other.WorstPositionGap, 2, 2);
            agreement += " units";
            ConsoleWriteLine(agreement);
        }

        if (other.WorstFormDisagreeRun > 60)
        {
            std::string failure = "    FAIL: their form stayed wrong for ";
            failure += Int32Text(other.WorstFormDisagreeRun);
            failure += " frames in a row -- ";
            failure += other.WorstFormContext;
            ConsoleWriteLine(failure);
            IncrementInt32(fails);
        }
        if (other.WorstPositionGap > 8.0
            || !std::isfinite(other.WorstPositionGap))
        {
            ConsoleWriteLine(
                "    FAIL: their position drifted far from the authority's");
            IncrementInt32(fails);
        }
        return fails;
    }

    bool NetFeatureCheck::LaysBombs(Hunter hunter) noexcept
    {
        return hunter == Hunter::Samus
            || hunter == Hunter::Kanden
            || hunter == Hunter::Sylux;
    }

    double NetFeatureCheck::Height(const Record& record) noexcept
    {
        return record.MaxY > record.MinY
            ? static_cast<double>(record.MaxY - record.MinY)
            : 0.0;
    }
}
