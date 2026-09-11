#include "GamepadState.hpp"

namespace MphRead::Mods::Input
{
    bool GamepadState::Down(GamepadButtons button) const noexcept
    {
        return (static_cast<std::int32_t>(Buttons) & static_cast<std::int32_t>(button)) != 0;
    }
}
