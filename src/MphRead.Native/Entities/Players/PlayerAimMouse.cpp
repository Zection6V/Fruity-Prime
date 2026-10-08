#include "PlayerEntity.hpp"
#include "../../Mods/InputSettings.hpp"

namespace MphRead::Entities
{
    void PlayerEntity::ApplyMouseAim()
    {
        const float yaw = -_input.MouseDeltaX() / 4 * Mods::InputSettings::MouseSensitivity()
            * (Mods::InputSettings::InvertMouseX() ? -1.0F : 1.0F);
        const float pitch = -_input.MouseDeltaY() / 4 * Mods::InputSettings::MouseSensitivity()
            * (Mods::InputSettings::InvertMouseY() ? -1.0F : 1.0F);
        _aimFrame.Yaw = yaw; _aimFrame.Pitch = pitch;
        _input.HasInput |= yaw != 0 || pitch != 0;
        UpdateHudShiftY(pitch); UpdateHudShiftX(yaw);
        // Direct mouse never waits for Touch or native gameplay phase.
        UpdateAimY(pitch); UpdateAimX(yaw);
    }
}
