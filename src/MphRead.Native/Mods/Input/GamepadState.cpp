#include "GamepadState.hpp"

namespace MphRead::Mods::Input
{
    bool GamepadState::Down(GamepadButtons button) const noexcept
    {
        return (static_cast<int>(Buttons) & static_cast<int>(button)) != 0;
    }
}
