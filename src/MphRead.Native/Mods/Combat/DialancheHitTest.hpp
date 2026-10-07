#pragma once

#include "../../Entities/Players/DialancheNativeCollision.hpp"

namespace MphRead
{
    class CollisionVolume;
}

namespace MphRead::Mods::Combat
{
    // Geometry only. The caller selects the native pose and applies reactions.
    class DialancheHitTest final
    {
    public:
        DialancheHitTest() = delete;
        static constexpr float RockRadius = 0.5F;

        [[nodiscard]] static bool Overlaps(
            const Entities::DialancheNativeCollision::Pose& pose, const CollisionVolume& volume);
        [[nodiscard]] static CollisionVolume DoorContactVolume(
            OpenTK::Mathematics::Vector3 lockPosition,
            OpenTK::Mathematics::Vector3 facing, float radius);
    };
}
