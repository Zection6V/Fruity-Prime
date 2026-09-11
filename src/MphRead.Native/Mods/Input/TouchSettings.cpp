#include "TouchSettings.hpp"

#include <cstddef>

namespace
{
    std::size_t LeadingTrimWidth(std::string_view value)
    {
        if (value.empty())
        {
            return 0;
        }

        const auto byte = [&value](std::size_t index)
        {
            return static_cast<unsigned char>(value[index]);
        };

        const unsigned char c0 = byte(0);
        if (c0 == 0 || (c0 >= 0x09 && c0 <= 0x0D) || c0 == 0x20)
        {
            return 1;
        }
        if (value.size() >= 2 && c0 == 0xC2)
        {
            const unsigned char c1 = byte(1);
            if (c1 == 0x85 || c1 == 0xA0)
            {
                return 2;
            }
        }
        if (value.size() >= 3)
        {
            const unsigned char c1 = byte(1);
            const unsigned char c2 = byte(2);
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

    std::size_t TrailingTrimWidth(std::string_view value)
    {
        if (value.empty())
        {
            return 0;
        }

        const auto byte = [&value](std::size_t index)
        {
            return static_cast<unsigned char>(value[index]);
        };

        const std::size_t size = value.size();
        const unsigned char last = byte(size - 1);
        if (last == 0 || (last >= 0x09 && last <= 0x0D) || last == 0x20)
        {
            return 1;
        }
        if (size >= 2 && byte(size - 2) == 0xC2 && (last == 0x85 || last == 0xA0))
        {
            return 2;
        }
        if (size >= 3)
        {
            const unsigned char c0 = byte(size - 3);
            const unsigned char c1 = byte(size - 2);
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

    std::string_view TrimBooleanInput(std::string_view value)
    {
        while (const std::size_t width = LeadingTrimWidth(value))
        {
            value.remove_prefix(width);
        }
        while (const std::size_t width = TrailingTrimWidth(value))
        {
            value.remove_suffix(width);
        }
        return value;
    }

    bool EqualsIgnoreCaseAscii(std::string_view value, std::string_view expectedLower)
    {
        if (value.size() != expectedLower.size())
        {
            return false;
        }
        for (std::size_t index = 0; index < value.size(); ++index)
        {
            unsigned char current = static_cast<unsigned char>(value[index]);
            if (current >= 'A' && current <= 'Z')
            {
                current = static_cast<unsigned char>(current + ('a' - 'A'));
            }
            if (current != static_cast<unsigned char>(expectedLower[index]))
            {
                return false;
            }
        }
        return true;
    }

    bool TryParseBoolean(std::optional<std::string_view> value, bool& result)
    {
        result = false;
        if (!value.has_value())
        {
            return false;
        }

        const std::string_view trimmed = TrimBooleanInput(*value);
        if (EqualsIgnoreCaseAscii(trimmed, "true"))
        {
            result = true;
            return true;
        }
        if (EqualsIgnoreCaseAscii(trimmed, "false"))
        {
            return true;
        }
        return false;
    }
}

namespace MphRead::Mods::Input
{
    bool TouchSettings::ButtonsVisible = true;
    std::unordered_set<TouchControl> TouchSettings::_hidden{};

    TouchControlOrderEntry TouchSettings::_order[11] =
    {
        { TouchControl::Shoot, "FIRE" },
        { TouchControl::Jump, "JUMP" },
        { TouchControl::Morph, "MORPH" },
        { TouchControl::ScanVisor, "VISOR" },
        { TouchControl::Scan, "SCAN" },
        { TouchControl::Missile, "MISSILE" },
        { TouchControl::WeaponMenu, "WEAPON wheel" },
        { TouchControl::Zoom, "ZOOM" },
        { TouchControl::Pause, "MENU" },
        { TouchControl::Scoreboard, "SCORE" },
        { TouchControl::Chat, "CHAT" }
    };

    TouchControlOrderEntry (&TouchSettings::Order)[11] = TouchSettings::_order;

    bool TouchSettings::IsEnabled(TouchControl control)
    {
        return _hidden.find(control) == _hidden.end();
    }

    void TouchSettings::SetEnabled(TouchControl control, bool enabled)
    {
        if (enabled)
        {
            _hidden.erase(control);
        }
        else
        {
            _hidden.insert(control);
        }
    }

    bool TouchSettings::Shown(TouchControl control)
    {
        return ButtonsVisible && IsEnabled(control);
    }

    void TouchSettings::Reset()
    {
        ButtonsVisible = true;
        _hidden.clear();
    }

    std::string TouchSettings::SettingKey(TouchControl control)
    {
        std::string name;
        switch (control)
        {
        case TouchControl::Shoot:
            name = "shoot";
            break;
        case TouchControl::Jump:
            name = "jump";
            break;
        case TouchControl::Morph:
            name = "morph";
            break;
        case TouchControl::ScanVisor:
            name = "scanvisor";
            break;
        case TouchControl::Scan:
            name = "scan";
            break;
        case TouchControl::Missile:
            name = "missile";
            break;
        case TouchControl::WeaponMenu:
            name = "weaponmenu";
            break;
        case TouchControl::Zoom:
            name = "zoom";
            break;
        case TouchControl::Pause:
            name = "pause";
            break;
        case TouchControl::Scoreboard:
            name = "scoreboard";
            break;
        case TouchControl::Chat:
            name = "chat";
            break;
        default:
            name = std::to_string(static_cast<std::int32_t>(control));
            break;
        }
        return std::string("touch_") + name;
    }

    bool TouchSettings::ReadSetting(
        std::optional<std::string_view> key,
        std::optional<std::string_view> value
    )
    {
        if (key.has_value() && *key == ButtonsSettingKey)
        {
            bool visible = false;
            if (TryParseBoolean(value, visible))
            {
                ButtonsVisible = visible;
            }
            return true;
        }

        for (const TouchControlOrderEntry& entry : Order)
        {
            if (key.has_value() && *key == SettingKey(entry.Control))
            {
                bool enabled = false;
                if (TryParseBoolean(value, enabled))
                {
                    SetEnabled(entry.Control, enabled);
                }
                return true;
            }
        }
        return false;
    }

    void TouchSettings::WriteSettings(std::vector<std::string>& lines)
    {
        lines.emplace_back(
            std::string(ButtonsSettingKey) + "=" + (ButtonsVisible ? "true" : "false")
        );
        for (const TouchControlOrderEntry& entry : Order)
        {
            lines.emplace_back(
                SettingKey(entry.Control) + "=" + (IsEnabled(entry.Control) ? "true" : "false")
            );
        }
    }
}
