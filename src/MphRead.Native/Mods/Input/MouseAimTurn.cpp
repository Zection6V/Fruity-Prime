#include "MouseAimTurn.hpp"

#include "../InputSettings.hpp"

namespace MphRead::Mods::Input
{
    MouseTurn MouseAimTurn(float deltaX, float deltaY) noexcept
    {
        const float sensitivity = InputSettings::MouseSensitivity();
        return {-deltaX / 4 * sensitivity * (InputSettings::InvertMouseX() ? -1.0F : 1.0F),
            -deltaY / 4 * sensitivity * (InputSettings::InvertMouseY() ? -1.0F : 1.0F)};
    }
}
