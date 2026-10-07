#include "LockjawCollision.hpp"

#include "../../Formats/CollisionDetection.hpp"
#include "../../Formats/Formats.hpp"

#include <algorithm>
#include <cmath>

namespace MphRead::Mods::Combat
{
    using OpenTK::Mathematics::LengthSquared;
    using OpenTK::Mathematics::ScaleVector;
    using OpenTK::Mathematics::Vector3;

    namespace
    {
        constexpr float ZeroThreshold = 0.000001F;
        constexpr float SnareHalfThickness = 0.75F;
    }

    bool LockjawCollision::WireOverlapsVolume(
        const CollisionVolume& volume, Vector3 from, Vector3 to)
    {
        if (LengthSquared(to - from) < ZeroThreshold)
        {
            return volume.TestPoint(from);
        }
        if (volume.Type != VolumeType::Box)
        {
            Formats::CollisionResult result{};
            return Formats::CollisionDetection::CheckCylinderOverlapVolume(
                &volume, from, to, 0.0F, result);
        }
        // The beam helper handles spheres and cylinders. Clip the wire
        // against each pair of planes for a box hurt volume.
        float enter = 0.0F;
        float exit = 1.0F;
        const Vector3 axes[]{volume.BoxVector1, volume.BoxVector2, volume.BoxVector3};
        const float lengths[]{volume.BoxDot1, volume.BoxDot2, volume.BoxDot3};
        for (int i = 0; i < 3; ++i)
        {
            const float start = Vector3::Dot(from - volume.BoxPosition, axes[i]);
            const float delta = Vector3::Dot(to - from, axes[i]);
            if (std::abs(delta) < ZeroThreshold)
            {
                if (start < 0.0F || start > lengths[i])
                {
                    return false;
                }
                continue;
            }
            const float a = -start / delta;
            const float b = (lengths[i] - start) / delta;
            enter = std::max(enter, std::min(a, b));
            exit = std::min(exit, std::max(a, b));
            if (enter > exit)
            {
                return false;
            }
        }
        return true;
    }

    bool LockjawCollision::SnareContainsPoint(const Triangle& bombs, Vector3 position)
    {
        const Vector3 zeroToOne = bombs[1] - bombs[0];
        const Vector3 oneToTwo = bombs[2] - bombs[1];
        const Vector3 normal = Vector3::Cross(oneToTwo, zeroToOne).Normalized();
        const Vector3 zeroToPosition = position - bombs[0];
        const float dot = Vector3::Dot(normal, zeroToPosition);
        if (!(dot > -SnareHalfThickness && dot < SnareHalfThickness))
        {
            return false;
        }
        // Preserve the existing strict edge tests and winding for hunters.
        return Vector3::Dot(Vector3::Cross(zeroToPosition, zeroToOne), normal) > 0.0F
            && Vector3::Dot(Vector3::Cross(position - bombs[1], oneToTwo), normal) > 0.0F
            && Vector3::Dot(Vector3::Cross(position - bombs[2], bombs[0] - bombs[2]), normal) > 0.0F;
    }

    bool LockjawCollision::SnareOverlapsVolume(
        const Triangle& bombs, const CollisionVolume& volume)
    {
        const Vector3 normal = Vector3::Cross(bombs[1] - bombs[0], bombs[2] - bombs[0]);
        const float lengthSquared = LengthSquared(normal);
        if (lengthSquared < ZeroThreshold)
        {
            return false;
        }
        const Vector3 center = volume.GetCenter();
        const Vector3 projected = center - ScaleVector(normal,
            Vector3::Dot(center - bombs[0], normal) / lengthSquared);
        Formats::CollisionResult result{};
        // Check the actual body against the plane so tall enemies are caught
        // even when their center stands above it.
        return SnareContainsPoint(bombs, projected)
            && Formats::CollisionDetection::CheckSphereOverlapVolume(
                &volume, projected, SnareHalfThickness, result);
    }
}
