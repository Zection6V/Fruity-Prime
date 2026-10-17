#include "FreeCameraFlight.hpp"
#include "../../Formats/Types.hpp"

#include <cmath>

namespace MphRead::Mods::Input
{
    using OpenTK::Mathematics::Vector3;
    using OpenTK::Mathematics::MathHelper::DegreesToRadians;
    using OpenTK::Mathematics::MathHelper::RadiansToDegrees;

    Vector3 FreeCameraFlight::Steerable(Vector3 facing) noexcept
    {
        const float Limit = Fixed::ToFloat(40);
        if (facing.X < Limit && facing.X > -Limit && facing.Z < Limit && facing.Z > -Limit)
        {
            // The game tests X twice; Z was surely meant, and is kept as it is.
            if (std::abs(facing.X) >= 1.0F / 4096.0F || std::abs(facing.X) >= 1.0F / 4096.0F)
            {
                facing.X *= 4.0F;
                facing.Z *= 4.0F;
            }
            else
            {
                facing.X = Limit;
            }
        }
        return facing;
    }

    Vector3 FreeCameraFlight::Moved(Vector3 position, Vector3 facing, float sideX, float sideZ, Keys keys) noexcept
    {
        // 0.4 a DS frame, halved for the 60 Hz step, in the game's own order.
        if (keys.Forward)
        {
            position = position + OpenTK::Mathematics::Divide(OpenTK::Mathematics::ScaleVector(facing, 0.4F), 2.0F);
        }
        else if (keys.Back)
        {
            position = position - OpenTK::Mathematics::Divide(OpenTK::Mathematics::ScaleVector(facing, 0.4F), 2.0F);
        }
        if (keys.Left)
        {
            position.X += sideX * 0.4F / 2.0F;
            position.Z += sideZ * 0.4F / 2.0F;
        }
        else if (keys.Right)
        {
            position.X -= sideX * 0.4F / 2.0F;
            position.Z -= sideZ * 0.4F / 2.0F;
        }
        return position;
    }

    Vector3 FreeCameraFlight::Turned(Vector3 facing, float aimX, float aimY) noexcept
    {
        const bool updateY = aimY != 0.0F
            && (facing.Y < 0.985F || aimY < 0.0F)
            && (facing.Y > -0.985F || aimY > 0.0F);
        const bool updateX = aimX != 0.0F;
        if (!updateY && !updateX) return facing;
        float pitch = RadiansToDegrees(std::asin(facing.Y));
        float yaw = RadiansToDegrees(std::atan2(facing.X, facing.Z));
        if (updateY) pitch = std::fmod(pitch + aimY, 360.0F);
        if (updateX) yaw = std::fmod(yaw + aimX, 360.0F);
        pitch = DegreesToRadians(pitch);
        yaw = DegreesToRadians(yaw);
        const float cos = std::cos(pitch);
        return Vector3(std::sin(yaw) * cos, std::sin(pitch), std::cos(yaw) * cos);
    }
}
