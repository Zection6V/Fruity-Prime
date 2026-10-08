#pragma once

#include "../../Formats/CollisionDetection.hpp"

#include <optional>

namespace MphRead::Entities { class EntityBase; }

namespace MphRead::Mods::Combat
{
    enum class BeamObstacleKind : std::uint8_t { Map, Door, ForceField };

    struct BeamObstacleHit final
    {
        Formats::CollisionResult Collision{};
        Entities::EntityBase* Entity = nullptr;
        BeamObstacleKind Kind = BeamObstacleKind::Map;
    };

    // Geometry/activation rules shared by the scene query and headless tests.
    [[nodiscard]] bool IntersectBeamDoor(OpenTK::Mathematics::Vector3 start,
        OpenTK::Mathematics::Vector3 end, bool open, bool connectorInactive,
        OpenTK::Mathematics::Vector3 facing, OpenTK::Mathematics::Vector3 lockPosition,
        float radiusSquared, float maxDistance, Formats::CollisionResult& result);
    [[nodiscard]] bool IntersectBeamForceField(OpenTK::Mathematics::Vector3 start,
        OpenTK::Mathematics::Vector3 end, bool active, OpenTK::Mathematics::Vector4 plane,
        OpenTK::Mathematics::Vector3 position, OpenTK::Mathematics::Vector3 up,
        OpenTK::Mathematics::Vector3 right, float height, float width,
        float maxDistance, Formats::CollisionResult& result);

    // Simulation-thread query. Collision results own their data, never the query's scratch buffer.
    [[nodiscard]] std::optional<BeamObstacleHit> TraceFirstBeamObstacle(
        OpenTK::Mathematics::Vector3 start, OpenTK::Mathematics::Vector3 end,
        Scene& scene, float maxDistance = 2.0F);
}
