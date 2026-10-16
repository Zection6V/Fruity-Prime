#pragma once

namespace MphRead::Mods::Input
{
    // The game's mouse look: pixels of motion to degrees of yaw and pitch,
    // before the player's own invert, zoom and clamp (UpdateAimX/UpdateAimY).
    // One formula for the step that turns the aim and the draw that shows it.
    struct MouseTurn final
    {
        float Yaw = 0;
        float Pitch = 0;
    };

    [[nodiscard]] MouseTurn MouseAimTurn(float deltaX, float deltaY) noexcept;
}
