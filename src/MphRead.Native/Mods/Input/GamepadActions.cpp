#include "GamepadActions.hpp"

#include "GamepadOptions.hpp"
#include "PadBindings.hpp"
#include "../../Entities/Players/PlayerInput.hpp"
#include "../../Formats/Enums.hpp"

namespace MphRead::Mods::Input
{
    namespace
    {
        [[nodiscard]] std::uint64_t Bit(PadAction action) noexcept
        {
            return 1ULL << static_cast<std::int32_t>(action);
        }
    }

    bool GamepadActions::Down(PadAction action) const noexcept
    {
        return (_down & Bit(action)) != 0;
    }

    bool GamepadActions::WasPressed(PadAction action) const noexcept
    {
        return (_pressed & Bit(action)) != 0;
    }

    bool GamepadActions::Take(PadAction action) noexcept
    {
        const bool pressed = WasPressed(action);
        _pressed &= ~Bit(action);
        return pressed;
    }

    void GamepadActions::Reset() noexcept
    {
        _down = _pressed = 0;
        _suppressed = GamepadButtons::None;
        _wheelOpen = false;
    }

    void GamepadActions::Update(GamepadButtons buttons)
    {
        _suppressed &= buttons;
        _suppressed |= PadBindings::ChordButtons(buttons);
        const std::uint64_t down = PadBindings::Evaluate(buttons, _suppressed);
        _pressed = down & ~_down;
        _down = down;
        _wheelOpen = GamepadOptions::WheelToggle()
            ? WasPressed(PadAction::WeaponWheel) ? !_wheelOpen : _wheelOpen
            : Down(PadAction::WeaponWheel);
    }

    Entities::Keybind* GamepadActions::WeaponBind(Entities::PlayerControls& controls, BeamType weapon)
    {
        switch (weapon)
        {
        case BeamType::PowerBeam: return &controls.PowerBeam();
        case BeamType::Missile: return &controls.Missile();
        case BeamType::VoltDriver: return &controls.VoltDriver();
        case BeamType::Battlehammer: return &controls.Battlehammer();
        case BeamType::Imperialist: return &controls.Imperialist();
        case BeamType::Judicator: return &controls.Judicator();
        case BeamType::Magmaul: return &controls.Magmaul();
        case BeamType::ShockCoil: return &controls.ShockCoil();
        case BeamType::OmegaCannon: return &controls.OmegaCannon();
        default: return nullptr;
        }
    }
}
