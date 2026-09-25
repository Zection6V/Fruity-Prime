#include "GameSettings.hpp"
#include "NativeRuntime/System/Charconv.hpp"

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
#include "../NativeRuntime/System/Globalization.hpp"
#include "NativeRuntime/System/Globalization.hpp"

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

using ::MphRead::NativeRuntime::CharIsWhiteSpace;
using ::MphRead::NativeRuntime::Int32TryParseInvariant;
using ::MphRead::NativeRuntime::IsNumberWhiteSpace;
using ::MphRead::NativeRuntime::StringEqualsOrdinalIgnoreCase;
using ::MphRead::NativeRuntime::StringTrimView;

namespace
{

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

        const std::string_view text = StringTrimView(*value);
        if (text.empty())
        {
            return false;
        }

        std::int32_t numeric = 0;
        if (Int32TryParseInvariant(text, numeric))
        {
            result = static_cast<MphRead::Language>(numeric);
            return true;
        }

        std::int32_t combined = 0;
        std::size_t start = 0;
        while (true)
        {
            const std::size_t comma = text.find(',', start);
            const std::string_view part = StringTrimView(
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
        // Read once a frame by the camera, so this reaches the match that is
        // running behind the settings page as soon as it is saved.
        RenderOptions::FieldOfView(RenderOptions::ParseFov(
            AsStringView(settings->FieldOfView), RenderOptions::FieldOfView()));
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
        if (Int32TryParseInvariant(settings->PointGoal, pointGoal)
            && pointGoal > 0)
        {
            GameState::PointGoal(pointGoal);
        }

        // Not the damage level. It is pinned to medium -- see
        // GameState::DamageLevel -- because it scales every weapon's damage
        // and was the one match rule each machine read out of its own file.
        // The key stays in settings.json and is ignored.
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
        using ::MphRead::NativeRuntime::NumberFormatInfo;
        using ::MphRead::NativeRuntime::NumberStyles;
        if (!::MphRead::NativeRuntime::TryParseSingle(
                *value, NumberStyles::Float, NumberFormatInfo::InvariantInfo(), parsed)
            && !::MphRead::NativeRuntime::TryParseSingle(
                *value, NumberStyles::Float, NumberFormatInfo::CurrentInfo(), parsed))
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

        const std::string_view trimmed = StringTrimView(*value);
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
            if (!Int32TryParseInvariant(part, number) || number < 0)
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
