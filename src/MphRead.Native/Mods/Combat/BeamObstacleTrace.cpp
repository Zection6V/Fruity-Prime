#include "BeamObstacleTrace.hpp"

#include "../../Entities/DoorEntity.hpp"
#include "../../Entities/ForceFieldEntity.hpp"
#include "../../Scene.hpp"
#include "../../NativeRuntime/System/Managed.hpp"

using MphRead::NativeRuntime::RequireReference;
using OpenTK::Mathematics::Vector3;
using OpenTK::Mathematics::Vector4;
using OpenTK::Mathematics::LengthSquared;

namespace MphRead::Mods::Combat
{
    namespace
    {
        Vector3 ComponentMultiply(Vector3 a, Vector3 b) noexcept
        {
            return Vector3(a.X * b.X, a.Y * b.Y, a.Z * b.Z);
        }
    }

    bool IntersectBeamDoor(Vector3 start, Vector3 end, bool open, bool connectorInactive,
        Vector3 facing, Vector3 lockPosition, float radiusSquared, float maxDistance,
        Formats::CollisionResult& result)
    {
        if (open || connectorInactive) return false;
        Vector4 plane(facing, 0.0F);
        if (Vector3::Dot(start - lockPosition, facing) < 0.0F)
        {
            plane.X *= -1.0F; plane.Y *= -1.0F; plane.Z *= -1.0F; plane.W *= -1.0F;
        }
        const Vector3 wvec = ComponentMultiply(
            plane.Xyz(), lockPosition + OpenTK::Mathematics::ScaleVector(plane.Xyz(), 0.4F));
        plane.W = wvec.X + wvec.Y + wvec.Z;
        if (!Formats::CollisionDetection::CheckCylinderIntersectPlane(start, end, plane, result)
            || result.Distance >= maxDistance || LengthSquared(result.Position - lockPosition) >= radiusSquared)
            return false;
        result.Field0 = 0;
        result.Plane = plane;
        return true;
    }

    bool IntersectBeamForceField(Vector3 start, Vector3 end, bool active, Vector4 plane,
        Vector3 position, Vector3 up, Vector3 right, float height, float width,
        float maxDistance, Formats::CollisionResult& result)
    {
        if (!active || !Formats::CollisionDetection::CheckCylinderIntersectPlane(start, end, plane, result)
            || result.Distance >= maxDistance) return false;
        const Vector3 between = result.Position - position;
        const float upDot = Vector3::Dot(between, up);
        if (upDot > height || upDot < -height) return false;
        const float rightDot = Vector3::Dot(between, right);
        if (rightDot > width || rightDot < -width) return false;
        result.Field0 = 0;
        result.Plane = plane;
        return true;
    }

    std::optional<BeamObstacleHit> TraceFirstBeamObstacle(
        Vector3 start, Vector3 end, Scene& scene, float maxDistance)
    {
        using namespace Entities;
        BeamObstacleHit hit{};
        float minDist = maxDistance;
        Formats::CollisionResult colRes{};
        if (Formats::CollisionDetection::CheckBetweenPoints(
                start, end, Formats::TestFlags::Beams, &scene, colRes)
            && colRes.Distance < minDist)
        {
            const float dot = Vector3::Dot(start, colRes.Plane.Xyz()) - colRes.Plane.W;
            if (dot >= 0.0F)
            {
                minDist = colRes.Distance;
                hit.Collision = colRes;
            }
        }

        auto doors = scene.GetDoorEntities().GetEnumerator();
        while (doors.MoveNext())
        {
            const std::shared_ptr<DoorEntity> doorPtr = doors.Current();
            DoorEntity& door = RequireReference(doorPtr);
            if (TestFlag(door.Flags(), DoorFlags::Open) || door.ConnectorInactive()) continue;
            if (IntersectBeamDoor(start, end, false, false,
                door.FacingVector(), door.LockPosition(), door.RadiusSquared(), minDist, colRes))
            {
                minDist = colRes.Distance;
                hit.Collision = colRes;
                hit.Collision.Flags = Formats::Collision::CollisionFlags::None;
                hit.Entity = doorPtr.get();
                hit.Kind = BeamObstacleKind::Door;
            }
        }

        auto forceFields = scene.GetForceFieldEntities().GetEnumerator();
        while (forceFields.MoveNext())
        {
            const std::shared_ptr<ForceFieldEntity> forceFieldPtr = forceFields.Current();
            ForceFieldEntity& forceField = RequireReference(forceFieldPtr);
            if (!forceField.Active()) continue;
            if (IntersectBeamForceField(start, end, true, forceField.Plane(),
                forceField.Position, forceField.FieldUpVector(), forceField.FieldRightVector(),
                forceField.Height(), forceField.Width(), minDist, colRes))
            {
                minDist = colRes.Distance;
                hit.Collision = colRes;
                hit.Entity = forceFieldPtr.get();
                hit.Kind = BeamObstacleKind::ForceField;
            }
        }
        if (minDist == maxDistance) return std::nullopt;
        return hit;
    }
}
