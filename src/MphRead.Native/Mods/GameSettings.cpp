#include "GameSettings.hpp"

#include "Render/FrameTiming.hpp"
#include "RenderOptions.hpp"
#include "../Formats/Enums.hpp"
#include "../Formats/Formats.hpp"
#include "../Formats/Types.hpp"
#include "../GameState.hpp"
#include "../Menu.hpp"
#include "../Scene.hpp"
#include "../Sound/Music.hpp"
#include "../Sound/Sfx.hpp"

#include <charconv>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <locale>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>

namespace
{
    [[nodiscard]] constexpr bool IsNumberWhiteSpace(unsigned char value) noexcept
    {
        return value == 0x20 || (value >= 0x09 && value <= 0x0D);
    }

    [[nodiscard]] constexpr bool IsDotNetWhiteSpace(char32_t value) noexcept
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

    struct Utf8Character
    {
        char32_t Value;
        std::size_t Length;
        bool Valid;
    };

    [[nodiscard]] Utf8Character DecodeUtf8(
        std::string_view text, std::size_t offset) noexcept
    {
        const auto first = static_cast<unsigned char>(text[offset]);
        if (first < 0x80)
        {
            return {first, 1, true};
        }

        std::size_t length = 0;
        char32_t value = 0;
        char32_t minimum = 0;
        if ((first & 0xE0U) == 0xC0U)
        {
            length = 2;
            value = first & 0x1FU;
            minimum = 0x80;
        }
        else if ((first & 0xF0U) == 0xE0U)
        {
            length = 3;
            value = first & 0x0FU;
            minimum = 0x800;
        }
        else if ((first & 0xF8U) == 0xF0U)
        {
            length = 4;
            value = first & 0x07U;
            minimum = 0x10000;
        }
        else
        {
            return {first, 1, false};
        }

        if (offset + length > text.size())
        {
            return {first, 1, false};
        }
        for (std::size_t index = 1; index < length; ++index)
        {
            const auto next = static_cast<unsigned char>(text[offset + index]);
            if ((next & 0xC0U) != 0x80U)
            {
                return {first, 1, false};
            }
            value = (value << 6) | (next & 0x3FU);
        }
        if (value < minimum || value > 0x10FFFF
            || (value >= 0xD800 && value <= 0xDFFF))
        {
            return {first, 1, false};
        }
        return {value, length, true};
    }

    [[nodiscard]] std::string_view TrimDotNetWhiteSpace(
        std::string_view value) noexcept
    {
        std::size_t first = 0;
        while (first < value.size())
        {
            const Utf8Character character = DecodeUtf8(value, first);
            if (!character.Valid || !IsDotNetWhiteSpace(character.Value))
            {
                break;
            }
            first += character.Length;
        }

        std::size_t cursor = first;
        std::size_t lastNonWhite = first;
        while (cursor < value.size())
        {
            const Utf8Character character = DecodeUtf8(value, cursor);
            if (!character.Valid || !IsDotNetWhiteSpace(character.Value))
            {
                lastNonWhite = cursor + character.Length;
            }
            cursor += character.Length;
        }
        return value.substr(first, lastNonWhite - first);
    }

    [[nodiscard]] std::string_view TrimNumberWhiteSpace(
        std::string_view value) noexcept
    {
        std::size_t first = 0;
        while (first < value.size()
            && IsNumberWhiteSpace(static_cast<unsigned char>(value[first])))
        {
            ++first;
        }

        std::size_t last = value.size();
        while (last > first && value[last - 1] == '\0')
        {
            --last;
        }
        while (last > first
            && IsNumberWhiteSpace(static_cast<unsigned char>(value[last - 1])))
        {
            --last;
        }
        return value.substr(first, last - first);
    }

    [[nodiscard]] bool TryParseInt32(
        std::string_view input, std::int32_t& result) noexcept
    {
        result = 0;
        input = TrimNumberWhiteSpace(input);
        if (input.empty())
        {
            return false;
        }

        std::size_t index = 0;
        bool negative = false;
        if (input[index] == '+' || input[index] == '-')
        {
            negative = input[index] == '-';
            ++index;
        }
        if (index == input.size() || input[index] < '0' || input[index] > '9')
        {
            return false;
        }

        constexpr std::uint64_t PositiveLimit
            = static_cast<std::uint64_t>(std::numeric_limits<std::int32_t>::max());
        constexpr std::uint64_t NegativeLimit = PositiveLimit + 1ULL;
        const std::uint64_t limit = negative ? NegativeLimit : PositiveLimit;
        std::uint64_t value = 0;

        while (index < input.size() && input[index] >= '0' && input[index] <= '9')
        {
            const std::uint64_t digit
                = static_cast<std::uint64_t>(input[index] - '0');
            if (value > (limit - digit) / 10ULL)
            {
                return false;
            }
            value = value * 10ULL + digit;
            ++index;
        }
        if (index != input.size())
        {
            return false;
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

    [[nodiscard]] bool EqualsIgnoreCaseAscii(
        std::string_view left, std::string_view right) noexcept
    {
        if (left.size() != right.size())
        {
            return false;
        }
        for (std::size_t index = 0; index < left.size(); ++index)
        {
            unsigned char a = static_cast<unsigned char>(left[index]);
            unsigned char b = static_cast<unsigned char>(right[index]);
            if (a >= 'A' && a <= 'Z')
            {
                a = static_cast<unsigned char>(a + ('a' - 'A'));
            }
            if (b >= 'A' && b <= 'Z')
            {
                b = static_cast<unsigned char>(b + ('a' - 'A'));
            }
            if (a != b)
            {
                return false;
            }
        }
        return true;
    }

    [[nodiscard]] bool TryParseSpecialSingle(
        std::string_view input, float& result) noexcept
    {
        const std::string_view text = TrimDotNetWhiteSpace(input);
        if (EqualsIgnoreCaseAscii(text, "Infinity")
            || EqualsIgnoreCaseAscii(text, "+Infinity"))
        {
            result = std::numeric_limits<float>::infinity();
            return true;
        }
        if (EqualsIgnoreCaseAscii(text, "-Infinity"))
        {
            result = -std::numeric_limits<float>::infinity();
            return true;
        }
        if (EqualsIgnoreCaseAscii(text, "NaN")
            || EqualsIgnoreCaseAscii(text, "+NaN")
            || EqualsIgnoreCaseAscii(text, "-NaN"))
        {
            result = std::numeric_limits<float>::quiet_NaN();
            return true;
        }
        return false;
    }

    [[nodiscard]] bool TryValidateFiniteSingle(
        std::string_view input, bool& isZero,
        bool& absoluteValueAtLeastOne) noexcept
    {
        isZero = false;
        absoluteValueAtLeastOne = false;
        if (input.empty())
        {
            return false;
        }

        std::size_t index = 0;
        if (input[index] == '-')
        {
            ++index;
            if (index == input.size())
            {
                return false;
            }
        }

        bool sawDigit = false;
        bool sawDecimal = false;
        std::size_t digitsBeforeDecimal = 0;
        std::size_t digitOrdinal = 0;
        std::size_t firstNonZeroDigit = std::string_view::npos;

        while (index < input.size()
            && input[index] != 'e' && input[index] != 'E')
        {
            const char ch = input[index];
            if (ch >= '0' && ch <= '9')
            {
                sawDigit = true;
                if (!sawDecimal)
                {
                    ++digitsBeforeDecimal;
                }
                if (ch != '0' && firstNonZeroDigit == std::string_view::npos)
                {
                    firstNonZeroDigit = digitOrdinal;
                }
                ++digitOrdinal;
                ++index;
                continue;
            }
            if (ch == '.' && !sawDecimal)
            {
                sawDecimal = true;
                ++index;
                continue;
            }
            return false;
        }

        if (!sawDigit)
        {
            return false;
        }

        bool exponentNegative = false;
        std::size_t exponentMagnitude = 0;
        if (index < input.size())
        {
            ++index;
            if (index == input.size())
            {
                return false;
            }
            if (input[index] == '+' || input[index] == '-')
            {
                exponentNegative = input[index] == '-';
                ++index;
            }
            if (index == input.size()
                || input[index] < '0' || input[index] > '9')
            {
                return false;
            }

            constexpr std::size_t MaxSize = std::numeric_limits<std::size_t>::max();
            while (index < input.size())
            {
                const char ch = input[index];
                if (ch < '0' || ch > '9')
                {
                    return false;
                }
                const std::size_t digit = static_cast<std::size_t>(ch - '0');
                if (exponentMagnitude != MaxSize)
                {
                    if (exponentMagnitude > (MaxSize - digit) / 10)
                    {
                        exponentMagnitude = MaxSize;
                    }
                    else
                    {
                        exponentMagnitude = exponentMagnitude * 10 + digit;
                    }
                }
                ++index;
            }
        }

        if (firstNonZeroDigit == std::string_view::npos)
        {
            isZero = true;
            return true;
        }

        const bool baseExponentNonNegative
            = digitsBeforeDecimal > firstNonZeroDigit;
        const std::size_t baseExponentMagnitude = baseExponentNonNegative
            ? digitsBeforeDecimal - firstNonZeroDigit - 1
            : firstNonZeroDigit + 1 - digitsBeforeDecimal;

        if (exponentMagnitude == 0)
        {
            absoluteValueAtLeastOne = baseExponentNonNegative;
        }
        else if (exponentNegative)
        {
            absoluteValueAtLeastOne = baseExponentNonNegative
                && baseExponentMagnitude >= exponentMagnitude;
        }
        else
        {
            absoluteValueAtLeastOne = baseExponentNonNegative
                || exponentMagnitude >= baseExponentMagnitude;
        }
        return true;
    }

    [[nodiscard]] bool TryParseSingleCore(
        std::string_view input, char decimalSeparator, float& result)
    {
        result = 0.0F;
        if (TryParseSpecialSingle(input, result))
        {
            return true;
        }

        input = TrimNumberWhiteSpace(input);
        if (input.empty())
        {
            return false;
        }

        bool negative = false;
        if (input.front() == '+' || input.front() == '-')
        {
            negative = input.front() == '-';
            input.remove_prefix(1);
            if (input.empty())
            {
                return false;
            }
        }

        std::string normalized;
        normalized.reserve(input.size() + 1);
        if (negative)
        {
            normalized.push_back('-');
        }

        bool sawDecimal = false;
        for (char ch : input)
        {
            if (ch == decimalSeparator)
            {
                if (sawDecimal)
                {
                    return false;
                }
                sawDecimal = true;
                normalized.push_back('.');
            }
            else if (decimalSeparator != '.' && ch == '.')
            {
                return false;
            }
            else
            {
                normalized.push_back(ch);
            }
        }

        bool isZero = false;
        bool absoluteValueAtLeastOne = false;
        if (!TryValidateFiniteSingle(
            normalized, isZero, absoluteValueAtLeastOne))
        {
            return false;
        }
        if (isZero)
        {
            result = negative ? -0.0F : 0.0F;
            return true;
        }

        const char* first = normalized.data();
        const char* last = normalized.data() + normalized.size();
        float parsed = 0.0F;
        const auto parsedResult = std::from_chars(
            first, last, parsed, std::chars_format::general);
        if (parsedResult.ec == std::errc{} && parsedResult.ptr == last)
        {
            result = parsed;
            return true;
        }
        if (parsedResult.ec == std::errc::result_out_of_range
            && parsedResult.ptr == last)
        {
            if (absoluteValueAtLeastOne)
            {
                result = negative
                    ? -std::numeric_limits<float>::infinity()
                    : std::numeric_limits<float>::infinity();
            }
            else
            {
                result = negative ? -0.0F : 0.0F;
            }
            return true;
        }
        return false;
    }

    [[nodiscard]] char CurrentCultureDecimalSeparator()
    {
        return std::use_facet<std::numpunct<char>>(std::locale("")).decimal_point();
    }

    [[nodiscard]] std::optional<std::string_view> AsStringView(
        const std::optional<std::string>& value) noexcept
    {
        if (!value)
        {
            return std::nullopt;
        }
        return std::string_view(*value);
    }

    [[nodiscard]] bool Equals(
        const std::optional<std::string>& value, std::string_view expected) noexcept
    {
        return value.has_value() && std::string_view(*value) == expected;
    }

    [[nodiscard]] bool TryParseLanguageName(
        std::string_view value, MphRead::Language& result) noexcept
    {
        if (value == "English")
        {
            result = MphRead::Language::English;
        }
        else if (value == "Japanese")
        {
            result = MphRead::Language::Japanese;
        }
        else if (value == "French")
        {
            result = MphRead::Language::French;
        }
        else if (value == "Spanish")
        {
            result = MphRead::Language::Spanish;
        }
        else if (value == "German")
        {
            result = MphRead::Language::German;
        }
        else if (value == "Italian")
        {
            result = MphRead::Language::Italian;
        }
        else
        {
            return false;
        }
        return true;
    }

    [[nodiscard]] bool TryParseLanguage(
        const std::optional<std::string>& value, MphRead::Language& result) noexcept
    {
        result = static_cast<MphRead::Language>(0);
        if (!value)
        {
            return false;
        }

        const std::string_view text = TrimDotNetWhiteSpace(*value);
        if (text.empty())
        {
            return false;
        }

        std::int32_t numeric = 0;
        if (TryParseInt32(text, numeric))
        {
            result = static_cast<MphRead::Language>(numeric);
            return true;
        }

        std::int32_t combined = 0;
        std::size_t start = 0;
        while (true)
        {
            const std::size_t comma = text.find(',', start);
            const std::string_view part = TrimDotNetWhiteSpace(
                comma == std::string_view::npos
                    ? text.substr(start)
                    : text.substr(start, comma - start));
            if (part.empty())
            {
                return false;
            }

            MphRead::Language parsed{};
            if (!TryParseLanguageName(part, parsed))
            {
                return false;
            }
            combined |= static_cast<std::int32_t>(parsed);

            if (comma == std::string_view::npos)
            {
                break;
            }
            start = comma + 1;
        }

        result = static_cast<MphRead::Language>(combined);
        return true;
    }

    [[nodiscard]] float ClampVolume(float value) noexcept
    {
        if (value < 0.0F)
        {
            return 0.0F;
        }
        if (value > 1.0F)
        {
            return 1.0F;
        }
        return value;
    }
}

namespace MphRead::Mods
{
    std::shared_ptr<MenuSettings> GameSettings::_current{};

    std::shared_ptr<MenuSettings> GameSettings::Current() noexcept
    {
        return _current;
    }

    void GameSettings::Apply(const std::shared_ptr<MenuSettings>& settings)
    {
        _current = settings;
        if (!settings)
        {
            throw System::NullReferenceException();
        }

        float sfx = 0.0F;
        if (TryVolume(settings->SfxVolume, sfx))
        {
            Sound::Sfx::Volume = sfx;
        }

        float music = 0.0F;
        if (TryVolume(settings->MusicVolume, music))
        {
            Music::SetUserVolume(music);
        }

        Language language{};
        if (TryParseLanguage(settings->Language, language))
        {
            Scene::Language(Paths::MphKey == "AMHK0"
                ? Language::Japanese
                : language);
        }

        RenderOptions::ResolutionScale(RenderOptions::ParseScale(
            AsStringView(settings->ResolutionScale), RenderOptions::ResolutionScale()));
        RenderOptions::Lighting(RenderOptions::ParseOnOff(
            AsStringView(settings->Lighting), RenderOptions::Lighting()));
        RenderOptions::Fog(RenderOptions::ParseOnOff(
            AsStringView(settings->Fog), RenderOptions::Fog()));
        RenderOptions::TextureFiltering(RenderOptions::ParseOnOff(
            AsStringView(settings->TextureFiltering), RenderOptions::TextureFiltering()));
        RenderOptions::ShowFps(RenderOptions::ParseOnOff(
            AsStringView(settings->ShowFps), RenderOptions::ShowFps()));
        Render::FrameTiming::SetFrameRateCap(Render::FrameTiming::ParseCap(
            settings->FrameRateCap, Render::FrameTiming::FrameRateCap()));
        RenderOptions::CelShading(RenderOptions::ParseOnOff(
            AsStringView(settings->CelShading), RenderOptions::CelShading()));
        RenderOptions::CelBands(8);
        RenderOptions::CelEdge(0.5F);
    }

    void GameSettings::ApplyMatchRules()
    {
        const std::shared_ptr<MenuSettings> settings = Current();
        if (!settings || !GameState::Multiplayer())
        {
            return;
        }

        float timeLimit = 0.0F;
        if (TryTime(settings->TimeLimit, timeLimit) && timeLimit > 0.0F)
        {
            GameState::MatchTime(timeLimit);
        }

        float timeGoal = 0.0F;
        if (TryTime(settings->TimeGoal, timeGoal) && timeGoal > 0.0F)
        {
            GameState::TimeGoal(timeGoal);
        }

        std::int32_t pointGoal = 0;
        if (settings->PointGoal
            && TryParseInt32(*settings->PointGoal, pointGoal)
            && pointGoal > 0)
        {
            GameState::PointGoal(pointGoal);
        }

        if (Equals(settings->DamageLevel, "low"))
        {
            GameState::DamageLevel(0);
        }
        else if (Equals(settings->DamageLevel, "high"))
        {
            GameState::DamageLevel(2);
        }
        else if (Equals(settings->DamageLevel, "medium"))
        {
            GameState::DamageLevel(1);
        }
        else
        {
            GameState::DamageLevel(GameState::DamageLevel());
        }

        GameState::FriendlyFire(Equals(settings->FriendlyFire, "on"));
        GameState::RadarPlayers(Equals(settings->HunterRadar, "on"));
        GameState::AffinityWeapons(Equals(settings->AffinityWeapons, "on"));
        GameState::ShadowFreeze(!Equals(settings->ShadowFreeze, "off"));
        GameState::OctolithReset(!Equals(settings->PointGoal, "off"));
    }

    bool GameSettings::TryVolume(
        const std::optional<std::string>& value, float& volume)
    {
        volume = 0.0F;
        if (!value)
        {
            return false;
        }

        float parsed = 0.0F;
        if (!TryParseSingleCore(*value, '.', parsed)
            && !TryParseSingleCore(
                *value, CurrentCultureDecimalSeparator(), parsed))
        {
            return false;
        }

        volume = ClampVolume(parsed);
        return true;
    }

    bool GameSettings::TryTime(
        const std::optional<std::string>& value, float& seconds)
    {
        seconds = 0.0F;
        if (!value)
        {
            return false;
        }

        const std::string_view trimmed = TrimDotNetWhiteSpace(*value);
        if (trimmed.empty())
        {
            return false;
        }

        std::size_t partCount = 1;
        for (char ch : trimmed)
        {
            if (ch == ':')
            {
                ++partCount;
            }
        }
        if (partCount > 3)
        {
            return false;
        }

        float total = 0.0F;
        std::size_t start = 0;
        while (true)
        {
            const std::size_t colon = trimmed.find(':', start);
            const std::string_view part = colon == std::string_view::npos
                ? trimmed.substr(start)
                : trimmed.substr(start, colon - start);

            std::int32_t number = 0;
            if (!TryParseInt32(part, number) || number < 0)
            {
                return false;
            }

            total = total * 60.0F + static_cast<float>(number);
            if (colon == std::string_view::npos)
            {
                break;
            }
            start = colon + 1;
        }

        seconds = total;
        return true;
    }
}
