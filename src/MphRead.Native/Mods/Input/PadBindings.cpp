#include "PadBindings.hpp"

#include <array>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

namespace MphRead::Mods::Input
{
    namespace
    {
        template <typename TEnum>
        struct EnumName
        {
            std::string_view Name;
            TEnum Value;
        };

        constexpr std::array<EnumName<PadAction>, 13> PadActionNames =
        {{
            {"Shoot", PadAction::Shoot},
            {"Zoom", PadAction::Zoom},
            {"Jump", PadAction::Jump},
            {"Morph", PadAction::Morph},
            {"Scan", PadAction::Scan},
            {"ScanVisor", PadAction::ScanVisor},
            {"Scoreboard", PadAction::Scoreboard},
            {"NextWeapon", PadAction::NextWeapon},
            {"PrevWeapon", PadAction::PrevWeapon},
            {"Missile", PadAction::Missile},
            {"PowerBeam", PadAction::PowerBeam},
            {"Menu", PadAction::Menu},
            {"Chat", PadAction::Chat}
        }};

        constexpr std::array<EnumName<GamepadButtons>, 17> GamepadButtonNames =
        {{
            {"None", GamepadButtons::None},
            {"A", GamepadButtons::A},
            {"B", GamepadButtons::B},
            {"X", GamepadButtons::X},
            {"Y", GamepadButtons::Y},
            {"LeftBumper", GamepadButtons::LeftBumper},
            {"RightBumper", GamepadButtons::RightBumper},
            {"Back", GamepadButtons::Back},
            {"Start", GamepadButtons::Start},
            {"LeftThumb", GamepadButtons::LeftThumb},
            {"RightThumb", GamepadButtons::RightThumb},
            {"DpadUp", GamepadButtons::DpadUp},
            {"DpadRight", GamepadButtons::DpadRight},
            {"DpadDown", GamepadButtons::DpadDown},
            {"DpadLeft", GamepadButtons::DpadLeft},
            {"LeftTrigger", GamepadButtons::LeftTrigger},
            {"RightTrigger", GamepadButtons::RightTrigger}
        }};

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

        bool TryParseInt32(std::string_view value, std::int32_t& parsed)
        {
            if (value.empty())
            {
                return false;
            }

            if (value.front() == '+')
            {
                value.remove_prefix(1);
                if (value.empty() || value.front() < '0' || value.front() > '9')
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

        template <typename TEnum, std::size_t N>
        bool TryParseEnum(std::string_view value,
            const std::array<EnumName<TEnum>, N>& names, TEnum& parsed)
        {
            std::string_view text = TrimDotNetWhitespace(value);
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
                    if (part == name.Name)
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

        std::size_t PadActionIndex(PadAction action)
        {
            const std::int32_t index = static_cast<std::int32_t>(action);
            if (index < 0 || static_cast<std::size_t>(index) >= PadActionNames.size())
            {
                throw std::out_of_range("Index was outside the bounds of the array.");
            }
            return static_cast<std::size_t>(index);
        }

        std::string PadActionToString(PadAction action)
        {
            for (const EnumName<PadAction>& entry : PadActionNames)
            {
                if (entry.Value == action)
                {
                    return std::string(entry.Name);
                }
            }
            return std::to_string(static_cast<std::int32_t>(action));
        }

        std::string GamepadButtonsToString(GamepadButtons buttons)
        {
            for (const EnumName<GamepadButtons>& entry : GamepadButtonNames)
            {
                if (entry.Value == buttons)
                {
                    return std::string(entry.Name);
                }
            }

            std::uint32_t remaining = static_cast<std::uint32_t>(
                static_cast<std::int32_t>(buttons));
            std::array<std::string_view, 16> found{};
            std::size_t foundCount = 0;

            for (std::size_t i = GamepadButtonNames.size(); i-- > 1;)
            {
                const std::uint32_t value = static_cast<std::uint32_t>(
                    static_cast<std::int32_t>(GamepadButtonNames[i].Value));
                if ((remaining & value) == value)
                {
                    remaining &= ~value;
                    found[foundCount++] = GamepadButtonNames[i].Name;
                    if (remaining == 0)
                    {
                        break;
                    }
                }
            }

            if (remaining != 0 || foundCount == 0)
            {
                return std::to_string(static_cast<std::int32_t>(buttons));
            }

            std::string result;
            for (std::size_t i = foundCount; i-- > 0;)
            {
                if (!result.empty())
                {
                    result += ", ";
                }
                result += found[i];
            }
            return result;
        }
    }

    std::array<GamepadButtons, 13> PadBindings::_defaults =
    {
        GamepadButtons::RightTrigger,
        GamepadButtons::LeftTrigger,
        GamepadButtons::A,
        GamepadButtons::B,
        GamepadButtons::X,
        GamepadButtons::Y,
        GamepadButtons::Back,
        static_cast<GamepadButtons>(
            static_cast<std::int32_t>(GamepadButtons::RightBumper)
            | static_cast<std::int32_t>(GamepadButtons::DpadRight)),
        static_cast<GamepadButtons>(
            static_cast<std::int32_t>(GamepadButtons::LeftBumper)
            | static_cast<std::int32_t>(GamepadButtons::DpadLeft)),
        GamepadButtons::DpadUp,
        GamepadButtons::DpadDown,
        GamepadButtons::Start,
        GamepadButtons::LeftThumb
    };

    std::array<GamepadButtons, 13> PadBindings::_current = PadBindings::_defaults;

    std::array<PadAction, 13> PadBindings::_actions =
    {
        PadAction::Shoot,
        PadAction::Jump,
        PadAction::Morph,
        PadAction::Zoom,
        PadAction::ScanVisor,
        PadAction::Scan,
        PadAction::NextWeapon,
        PadAction::PrevWeapon,
        PadAction::Missile,
        PadAction::PowerBeam,
        PadAction::Scoreboard,
        PadAction::Menu,
        PadAction::Chat
    };

    const std::array<PadAction, 13>& PadBindings::Actions() noexcept
    {
        return _actions;
    }

    GamepadButtons PadBindings::Get(PadAction action)
    {
        return _current[PadActionIndex(action)];
    }

    void PadBindings::Set(PadAction action, GamepadButtons buttons)
    {
        _current[PadActionIndex(action)] = buttons;
    }

    GamepadButtons PadBindings::Default(PadAction action)
    {
        return _defaults[PadActionIndex(action)];
    }

    void PadBindings::Reset()
    {
        _current = _defaults;
    }

    std::string PadBindings::Name(PadAction action)
    {
        switch (action)
        {
        case PadAction::Shoot:
            return "Fire / alt attack";
        case PadAction::Zoom:
            return "Zoom";
        case PadAction::Jump:
            return "Jump / boost";
        case PadAction::Morph:
            return "Morph ball";
        case PadAction::Scan:
            return "Scan";
        case PadAction::ScanVisor:
            return "Scan visor";
        case PadAction::Scoreboard:
            return "Map / scoreboard";
        case PadAction::NextWeapon:
            return "Next weapon";
        case PadAction::PrevWeapon:
            return "Previous weapon";
        case PadAction::Missile:
            return "Missile";
        case PadAction::PowerBeam:
            return "Power beam";
        case PadAction::Menu:
            return "Menu";
        default:
            return PadActionToString(action);
        }
    }

    std::string PadBindings::Describe(GamepadButtons buttons)
    {
        if (buttons == GamepadButtons::None)
        {
            return "unbound";
        }

        std::vector<std::string> names;
        names.reserve(GamepadButtonNames.size() - 1);
        const std::int32_t buttonBits = static_cast<std::int32_t>(buttons);
        for (std::size_t i = 1; i < GamepadButtonNames.size(); ++i)
        {
            const GamepadButtons button = GamepadButtonNames[i].Value;
            const std::int32_t value = static_cast<std::int32_t>(button);
            if ((buttonBits & value) == value)
            {
                names.push_back(ButtonName(button));
            }
        }

        std::string result;
        for (const std::string& name : names)
        {
            if (!result.empty())
            {
                result += " or ";
            }
            result += name;
        }
        return result;
    }

    std::string PadBindings::ButtonName(GamepadButtons button)
    {
        switch (button)
        {
        case GamepadButtons::A:
            return "A";
        case GamepadButtons::B:
            return "B";
        case GamepadButtons::X:
            return "X";
        case GamepadButtons::Y:
            return "Y";
        case GamepadButtons::LeftBumper:
            return "LB";
        case GamepadButtons::RightBumper:
            return "RB";
        case GamepadButtons::LeftTrigger:
            return "LT";
        case GamepadButtons::RightTrigger:
            return "RT";
        case GamepadButtons::Back:
            return "Back";
        case GamepadButtons::Start:
            return "Start";
        case GamepadButtons::LeftThumb:
            return "Left stick";
        case GamepadButtons::RightThumb:
            return "Right stick";
        case GamepadButtons::DpadUp:
            return "D-pad up";
        case GamepadButtons::DpadDown:
            return "D-pad down";
        case GamepadButtons::DpadLeft:
            return "D-pad left";
        case GamepadButtons::DpadRight:
            return "D-pad right";
        default:
            return GamepadButtonsToString(button);
        }
    }

    std::string PadBindings::SettingKey(PadAction action)
    {
        return std::string("pad_") + PadActionToString(action);
    }

    bool PadBindings::TryLoad(
        std::optional<std::string_view> key,
        std::optional<std::string_view> value
    )
    {
        if (!key.has_value())
        {
            throw std::runtime_error("Object reference not set to an instance of an object.");
        }

        constexpr std::string_view prefix = "pad_";
        if (!key->starts_with(prefix))
        {
            return false;
        }

        PadAction action = PadAction::Shoot;
        if (!TryParseEnum(key->substr(prefix.size()), PadActionNames, action))
        {
            return false;
        }

        GamepadButtons buttons = GamepadButtons::None;
        if (!value.has_value() || !TryParseEnum(*value, GamepadButtonNames, buttons))
        {
            return false;
        }

        Set(action, buttons);
        return true;
    }
}
