#include "PadBindingState.hpp"

#include "GamepadGlyphs.hpp"
#include "../../NativeRuntime/System/Exceptions.hpp"
#include "../../NativeRuntime/System/Globalization.hpp"

#include <stdexcept>

namespace MphRead::Mods::Input
{
    namespace Runtime = ::MphRead::NativeRuntime;

    namespace
    {
        [[nodiscard]] std::size_t Index(PadAction action)
        {
            const auto index = static_cast<std::int32_t>(action);
            if (index < 0 || index >= PadActionCount)
            {
                throw std::out_of_range("Index was outside the bounds of the array.");
            }
            return static_cast<std::size_t>(index);
        }

        [[nodiscard]] std::size_t SlotIndex(std::int32_t slot)
        {
            if (slot < 0 || slot > 1)
            {
                throw std::out_of_range("Index was outside the bounds of the array.");
            }
            return static_cast<std::size_t>(slot);
        }

        [[nodiscard]] std::uint64_t Bit(PadAction action) noexcept
        {
            return 1ULL << static_cast<std::int32_t>(action);
        }

        // key[start..^end], which throws where the range is backwards.
        [[nodiscard]] std::string_view Range(std::string_view key, std::size_t start, std::size_t end)
        {
            if (start + end > key.size())
            {
                throw std::out_of_range("Specified argument was out of the range of valid values.");
            }
            return key.substr(start, key.size() - start - end);
        }
    }

    const std::array<GamepadButtons, PadActionCount> PadBindingState::_defaults{
        /* Shoot      */ GamepadButtons::RightTrigger,
        /* Zoom       */ GamepadButtons::LeftTrigger,
        /* Jump       */ GamepadButtons::A,
        /* Morph      */ GamepadButtons::B,
        /* Scan       */ GamepadButtons::X,
        /* ScanVisor  */ GamepadButtons::Y,
        /* Scoreboard */ GamepadButtons::Back,
        /* NextWeapon */ GamepadButtons::RightBumper | GamepadButtons::DpadRight,
        /* PrevWeapon */ GamepadButtons::LeftBumper | GamepadButtons::DpadLeft,
        /* Missile    */ GamepadButtons::DpadUp,
        /* PowerBeam  */ GamepadButtons::DpadDown,
        /* Menu       */ GamepadButtons::Start,
        /* Chat       */ GamepadButtons::LeftThumb,
        /* WeaponWheel */ GamepadButtons::RightThumb,
        // Direct weapons and last weapon are opt-in.
        GamepadButtons::None, GamepadButtons::None, GamepadButtons::None, GamepadButtons::None, GamepadButtons::None,
        GamepadButtons::None, GamepadButtons::None, GamepadButtons::None, GamepadButtons::None};

    const std::array<PadAction, PadActionCount> PadBindingState::ActionOrder{
        PadAction::Shoot, PadAction::Jump, PadAction::Morph, PadAction::Zoom,
        PadAction::ScanVisor, PadAction::Scan, PadAction::NextWeapon,
        PadAction::PrevWeapon, PadAction::Missile, PadAction::PowerBeam,
        PadAction::Scoreboard, PadAction::Menu, PadAction::Chat, PadAction::WeaponWheel,
        PadAction::VoltDriver, PadAction::Battlehammer, PadAction::Imperialist, PadAction::Judicator,
        PadAction::Magmaul, PadAction::ShockCoil, PadAction::OmegaCannon, PadAction::AffinitySlot, PadAction::LastWeapon};

    PadBindingState::PadBindingState()
    {
        Reset();
    }

    GamepadButtons PadBindingState::Get(PadAction action) const
    {
        return _current[Index(action)];
    }

    void PadBindingState::Set(PadAction action, GamepadButtons buttons)
    {
        const std::size_t index = Index(action);
        _current[index] = buttons;
        Primary[index] = Secondary[index] = GamepadButtons::None;
        Modifiers[index][0] = Modifiers[index][1] = GamepadButtons::None;
        for (const GamepadButtons button : GamepadButtonValues)
        {
            if (button != GamepadButtons::None && Any(buttons & button))
            {
                if (Primary[index] == GamepadButtons::None)
                {
                    Primary[index] = button;
                }
                else if (Secondary[index] == GamepadButtons::None)
                {
                    Secondary[index] = button;
                }
            }
        }
        _preset = "Custom";
        _revision++;
    }

    GamepadButtons PadBindingState::Default(PadAction action) const
    {
        return _defaults[Index(action)];
    }

    GamepadButtons PadBindingState::Slot(PadAction action, std::int32_t slot) const
    {
        return slot == 0 ? Primary[Index(action)] : Secondary[Index(action)];
    }

    void PadBindingState::SetSlot(PadAction action, std::int32_t slot, GamepadButtons button, GamepadButtons modifier)
    {
        const std::size_t index = Index(action);
        if (!Single(button) || !Single(modifier) || (button != GamepadButtons::None && button == modifier))
        {
            throw System::ArgumentException("A binding needs distinct single buttons.");
        }
        Modifiers[index][SlotIndex(slot)] = button == GamepadButtons::None ? GamepadButtons::None : modifier;
        const GamepadButtons old = Slot(action, slot);
        if (slot == 0)
        {
            Primary[index] = button;
        }
        else
        {
            Secondary[index] = button;
        }
        _current[index] = (_current[index] & ~old) | Primary[index] | Secondary[index];
        _preset = "Custom";
        _revision++;
    }

    bool PadBindingState::Single(GamepadButtons button) const noexcept
    {
        const auto value = static_cast<std::int32_t>(button);
        return (value & ~0xffff) == 0 && (value & (value - 1)) == 0;
    }

    GamepadButtons PadBindingState::Modifier(PadAction action, std::int32_t slot) const
    {
        return Modifiers[Index(action)][SlotIndex(slot)];
    }

    std::string PadBindingState::DescribeSlot(PadAction action, std::int32_t slot) const
    {
        return (Modifier(action, slot) == GamepadButtons::None ? std::string() : ButtonName(Modifier(action, slot)) + " + ")
            + Describe(Slot(action, slot));
    }

    std::uint64_t PadBindingState::Evaluate(GamepadButtons buttons, GamepadButtons suppressed) const
    {
        GamepadButtons modifiers = GamepadButtons::None;
        GamepadButtons used = GamepadButtons::None;
        std::uint64_t result = 0;
        for (const PadAction action : ActionOrder)
        {
            for (std::int32_t slot = 0; slot < 2; slot++)
            {
                const GamepadButtons modifier = Modifier(action, slot);
                const GamepadButtons button = Slot(action, slot);
                modifiers |= modifier;
                if (modifier != GamepadButtons::None && button != GamepadButtons::None
                    && (buttons & (modifier | button)) == (modifier | button))
                {
                    result |= Bit(action);
                    used |= modifier | button;
                }
            }
        }
        for (const PadAction action : ActionOrder)
        {
            const GamepadButtons available = buttons & ~(modifiers | used | suppressed);
            GamepadButtons plain = Get(action) & ~(Slot(action, 0) | Slot(action, 1));
            for (std::int32_t slot = 0; slot < 2; slot++)
            {
                if (Modifier(action, slot) == GamepadButtons::None)
                {
                    plain |= Slot(action, slot);
                }
            }
            if (Any(available & plain))
            {
                result |= Bit(action);
            }
        }
        return result;
    }

    GamepadButtons PadBindingState::ChordButtons(GamepadButtons buttons) const
    {
        GamepadButtons used = GamepadButtons::None;
        for (const PadAction action : ActionOrder)
        {
            for (std::int32_t slot = 0; slot < 2; slot++)
            {
                const GamepadButtons modifier = Modifier(action, slot);
                const GamepadButtons chord = modifier | Slot(action, slot);
                if (modifier != GamepadButtons::None && (buttons & chord) == chord)
                {
                    used |= chord;
                }
            }
        }
        return used;
    }

    void PadBindingState::Write(std::vector<std::string>& lines) const
    {
        for (const PadAction action : ActionOrder)
        {
            const std::string key = SettingKey(action);
            lines.push_back(key + "=" + ToString(Get(action)));
            for (std::int32_t slot = 0; slot < 2; slot++)
            {
                const std::string suffix = slot == 0 ? "primary" : "secondary";
                lines.push_back(key + "_" + suffix + "=" + ToString(Slot(action, slot)));
                lines.push_back(key + "_" + suffix + "_modifier=" + ToString(Modifier(action, slot)));
            }
        }
    }

    void PadBindingState::LoadSlots(const std::vector<std::string>& lines)
    {
        for (const std::string& line : lines)
        {
            const std::size_t split = line.find('=');
            if (split == std::string::npos)
            {
                continue;
            }
            const std::string key = Runtime::StringTrim(line.substr(0, split));
            const std::string value = Runtime::StringTrim(line.substr(split + 1));
            const std::int32_t slot = key.ends_with("_primary") ? 0 : 1;
            const std::string_view suffix = slot == 0 ? "_primary" : "_secondary";
            if (!key.starts_with("pad_") || !key.ends_with(suffix))
            {
                continue;
            }
            PadAction action{};
            GamepadButtons button = GamepadButtons::None;
            if (TryParse(Range(key, 4, suffix.size()), action) && IsDefined(action)
                && TryParse(value, button) && (static_cast<std::int32_t>(button) & ~0xffff) == 0
                && (static_cast<std::int32_t>(button) & (static_cast<std::int32_t>(button) - 1)) == 0)
            {
                SetSlot(action, slot, button);
            }
        }
        for (const std::string& line : lines)
        {
            const std::size_t split = line.find('=');
            if (split == std::string::npos)
            {
                continue;
            }
            const std::string key = Runtime::StringTrim(line.substr(0, split));
            const std::string value = Runtime::StringTrim(line.substr(split + 1));
            for (const PadAction action : ActionOrder)
            {
                for (std::int32_t slot = 0; slot < 2; slot++)
                {
                    GamepadButtons modifier = GamepadButtons::None;
                    if (key == SettingKey(action) + (slot == 0 ? "_primary_modifier" : "_secondary_modifier")
                        && TryParse(value, modifier) && Single(modifier) && modifier != Slot(action, slot))
                    {
                        SetSlot(action, slot, Slot(action, slot), modifier);
                    }
                }
            }
        }
    }

    std::vector<PadAction> PadBindingState::Conflicts(PadAction action, GamepadButtons button, GamepadButtons modifier) const
    {
        std::vector<PadAction> result;
        if (button != GamepadButtons::None)
        {
            for (const PadAction other : Actions())
            {
                if (other != action && ((Slot(other, 0) == button && Modifier(other, 0) == modifier)
                    || (Slot(other, 1) == button && Modifier(other, 1) == modifier)
                    || (modifier == GamepadButtons::None
                        && Any(Get(other) & ~(Slot(other, 0) | Slot(other, 1)) & button))))
                {
                    result.push_back(other);
                }
            }
        }
        return result;
    }

    void PadBindingState::Assign(PadAction action, std::int32_t slot, GamepadButtons button, std::string_view resolution,
        GamepadButtons modifier)
    {
        if (resolution == "Cancel")
        {
            return;
        }
        const GamepadButtons old = Slot(action, slot);
        const GamepadButtons oldModifier = Modifier(action, slot);
        if (resolution == "Swap" || resolution == "Replace")
        {
            for (const PadAction other : Conflicts(action, button, modifier))
            {
                const std::size_t index = Index(other);
                const GamepadButtons replacement = resolution == "Swap" ? old : GamepadButtons::None;
                for (std::int32_t otherSlot = 0; otherSlot < 2; otherSlot++)
                {
                    if (Slot(other, otherSlot) == button && Modifier(other, otherSlot) == modifier)
                    {
                        SetSlot(other, otherSlot, replacement, resolution == "Swap" ? oldModifier : GamepadButtons::None);
                    }
                }
                _current[index] = (Get(other) & ~button) | replacement | Primary[index] | Secondary[index];
                _revision++;
            }
        }
        SetSlot(action, slot, button, modifier);
    }

    void PadBindingState::ApplyPreset(const std::string& name)
    {
        if (name == "Custom")
        {
            _preset = name;
            return;
        }
        Reset();
        if (name == "Bumper Jumper")
        {
            Set(PadAction::Jump, GamepadButtons::LeftBumper);
            Set(PadAction::PrevWeapon, GamepadButtons::A | GamepadButtons::DpadLeft);
        }
        if (name == "Classic")
        {
            Set(PadAction::WeaponWheel, GamepadButtons::None);
            Set(PadAction::Chat, GamepadButtons::LeftThumb);
        }
        _preset = name;
    }

    std::shared_ptr<PadBindingState> PadBindingState::Clone() const
    {
        auto copy = std::make_shared<PadBindingState>();
        copy->_current = _current;
        copy->Primary = Primary;
        copy->Secondary = Secondary;
        copy->Modifiers = Modifiers;
        copy->_preset = _preset;
        copy->_revision = _revision;
        return copy;
    }

    void PadBindingState::Reset()
    {
        for (const PadAction action : Actions())
        {
            Set(action, _defaults[Index(action)]);
        }
        _preset = "Default";
    }

    std::string PadBindingState::Name(PadAction action) const
    {
        switch (action)
        {
        case PadAction::Shoot: return "Fire / alt attack";
        case PadAction::Zoom: return "Zoom";
        case PadAction::Jump: return "Jump / boost";
        case PadAction::Morph: return "Morph ball";
        case PadAction::Scan: return "Scan";
        case PadAction::ScanVisor: return "Scan visor";
        case PadAction::Scoreboard: return "Map / scoreboard";
        case PadAction::NextWeapon: return "Next weapon";
        case PadAction::PrevWeapon: return "Cycle previous weapon";
        case PadAction::LastWeapon: return "Last equipped weapon";
        case PadAction::VoltDriver: return "Volt Driver";
        case PadAction::ShockCoil: return "Shock Coil";
        case PadAction::OmegaCannon: return "Omega Cannon";
        case PadAction::AffinitySlot: return "Affinity weapon slot";
        case PadAction::Missile: return "Missile";
        case PadAction::PowerBeam: return "Power beam";
        case PadAction::Menu: return "Menu";
        case PadAction::WeaponWheel: return "Weapon wheel";
        default: return ToString(action);
        }
    }

    std::string PadBindingState::Describe(GamepadButtons buttons) const
    {
        if (buttons == GamepadButtons::None)
        {
            return "unbound";
        }
        std::string result;
        for (const GamepadButtons button : GamepadButtonValues)
        {
            if (button != GamepadButtons::None && (buttons & button) == button)
            {
                result += (result.empty() ? "" : " or ") + ButtonName(button);
            }
        }
        return result;
    }

    std::string PadBindingState::ButtonName(GamepadButtons button) const
    {
        return GamepadGlyphs::Resolve(button);
    }

    std::string PadBindingState::SettingKey(PadAction action) const
    {
        return "pad_" + ToString(action);
    }

    bool PadBindingState::TryLoad(std::string_view key, std::string_view value)
    {
        PadAction action{};
        GamepadButtons buttons = GamepadButtons::None;
        if (!key.starts_with("pad_")
            || !TryParse(key.substr(4), action)
            || !IsDefined(action)
            || !TryParse(value, buttons)
            || (static_cast<std::int32_t>(buttons) & ~0xffff) != 0)
        {
            return false;
        }
        Set(action, buttons);
        return true;
    }
}
