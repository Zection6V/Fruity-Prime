#include "DialancheHitTest.hpp"

#include "../../Formats/CollisionDetection.hpp"
#include "../../Formats/Formats.hpp"

namespace MphRead::Mods::Combat
{
    bool DialancheHitTest::Overlaps(
        const Entities::DialancheNativeCollision::Pose& pose, const CollisionVolume& volume)
    {
        // EU1.1 0200B808/0211DA34: two radius-0.5 rocks, one OR result.
        Formats::CollisionResult unused{};
        return Formats::CollisionDetection::CheckSphereOverlapVolume(&volume, pose.Left, RockRadius, unused)
            || Formats::CollisionDetection::CheckSphereOverlapVolume(&volume, pose.Right, RockRadius, unused);
    }

    CollisionVolume DialancheHitTest::DoorContactVolume(
        OpenTK::Mathematics::Vector3 lockPosition,
        OpenTK::Mathematics::Vector3 facing, float radius)
    {
        // Preserve the existing door aperture and contact depth (+/-1.25).
        constexpr float halfDepth = 1.25F;
        return CollisionVolume(facing,
            lockPosition - OpenTK::Mathematics::ScaleVector(facing, halfDepth), radius, halfDepth * 2);
    }
}
