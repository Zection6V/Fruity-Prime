#pragma once

#include "../../NativeRuntime/OpenTK/Mathematics.hpp"

#include <array>

namespace MphRead
{
    class CollisionVolume;
}

namespace MphRead::Mods::Combat
{
    // Geometry only: no entity ownership, damage, messages or scene mutation.
    class LockjawCollision final
    {
    public:
        LockjawCollision() = delete;
        using Triangle = std::array<OpenTK::Mathematics::Vector3, 3>;

        [[nodiscard]] static bool WireOverlapsVolume(const CollisionVolume& volume,
            OpenTK::Mathematics::Vector3 from, OpenTK::Mathematics::Vector3 to);
        [[nodiscard]] static bool SnareContainsPoint(const Triangle& bombs,
            OpenTK::Mathematics::Vector3 position);
        [[nodiscard]] static bool SnareOverlapsVolume(const Triangle& bombs,
            const CollisionVolume& volume);
    };
}
