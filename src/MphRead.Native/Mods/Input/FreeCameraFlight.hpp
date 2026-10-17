#pragma once

#include "../../NativeRuntime/OpenTK/Mathematics.hpp"

namespace MphRead::Mods::Input
{
    // The death camera's own flight (PlayerEntity::UpdateCameraFree), in one
    // place for the step that flies it and for the pictures between steps,
    // which show where the next step puts it.
    class FreeCameraFlight final
    {
    public:
        FreeCameraFlight() = delete;

        struct Keys final
        {
            bool Forward = false, Back = false, Left = false, Right = false;
        };

        // A facing looking almost straight up or down given a heading to
        // fly along, as the step gives it one before it moves.
        [[nodiscard]] static OpenTK::Mathematics::Vector3 Steerable(OpenTK::Mathematics::Vector3 facing) noexcept;
        // `position` moved by a step's keys: along `facing`, and sideways
        // along the camera's side (CameraInfo::Field50 and Field54).
        [[nodiscard]] static OpenTK::Mathematics::Vector3 Moved(OpenTK::Mathematics::Vector3 position,
            OpenTK::Mathematics::Vector3 facing, float sideX, float sideZ, Keys keys) noexcept;
        // `facing` turned by an aim in degrees, short of looking straight up or down.
        [[nodiscard]] static OpenTK::Mathematics::Vector3 Turned(OpenTK::Mathematics::Vector3 facing,
            float aimX, float aimY) noexcept;
    };
}
