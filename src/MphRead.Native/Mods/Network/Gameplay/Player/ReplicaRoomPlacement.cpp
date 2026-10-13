#include "Mods/Network/Gameplay/Player/ReplicaRoomPlacement.hpp"

#include "Entities/RoomEntity.hpp"
#include "Formats/Collision.hpp"
#include "Formats/Model.hpp"

#include <cmath>

namespace MphRead::Mods::Network
{
    using OpenTK::Mathematics::Vector3;
    using Formats::Culling::NodeRef;

    NodeRef ReplicaRoomPlacement::Find(const Entities::RoomEntity* room,
        Vector3 probe, Vector3 bodyCenter, bool& ambiguous)
    {
        ambiguous = false;
        NodeRef best = NodeRef::None;
        if (!room) return best;
        for (const auto& sides : room->_portalSides)
        {
            NodeRef candidate = NodeRef::None;
            bool allInside = true;
            for (const auto& side : sides)
            {
                const auto& portal = *side.first;
                float distance = Vector3::Dot(probe, portal.Plane.Xyz()) - portal.Plane.W;
                if (side.second) distance *= -1.0F;
                if (distance < 0.0F)
                {
                    allInside = false;
                    break;
                }
                candidate = side.second ? portal.NodeRef2 : portal.NodeRef1;
            }
            if (!allInside || candidate == NodeRef::None
                || !GeometryBoundsContain(*room, candidate, bodyCenter)) continue;
            // Overlapping physical candidates do not prove membership. None
            // preserves the replica's existing unresolved-body fallback.
            if (best != NodeRef::None)
            {
                ambiguous = true;
                return NodeRef::None;
            }
            best = candidate;
        }
        return best;
    }

    bool ReplicaRoomPlacement::GeometryBoundsContain(const Entities::RoomEntity& room,
        NodeRef nodeRef, Vector3 position)
    {
        if (!std::isfinite(position.X) || !std::isfinite(position.Y) || !std::isfinite(position.Z)
            || nodeRef.PartIndex < 0 || nodeRef.PartIndex >= room._roomPartMax
            || nodeRef.NodeIndex < 0 || nodeRef.ModelIndex < 0)
            return false;
        std::shared_ptr<ModelInstance> instance;
        Vector3 offset = Vector3::Zero;
        if (nodeRef.ModelIndex == 0)
        {
            const auto& models = room.GetModels();
            if (models.empty()) return false;
            instance = models[0];
        }
        else
        {
            const auto modelIndex = static_cast<std::size_t>(nodeRef.ModelIndex);
            if (modelIndex > room._connectorModels.size() || modelIndex >= room._roomCollision.size()
                || !room._roomCollision[modelIndex]) return false;
            instance = room._connectorModels[modelIndex - 1];
            offset = room._roomCollision[modelIndex]->Translation;
        }
        const auto model = instance ? instance->Model() : nullptr;
        if (!model || !model->Nodes || (nodeRef.RoomName.HasValue() && !room.ModKnowsRoom(*nodeRef.RoomName)))
            return false;
        const auto& nodes = *model->Nodes;
        constexpr float tolerance = 1.0F / 4096.0F;
        bool containsPosition = false;
        auto index = nodeRef.NodeIndex;
        std::size_t visited = 0;
        for (; index != -1 && visited < nodes.size(); ++visited)
        {
            if (index < 0 || static_cast<std::size_t>(index) >= nodes.size() || !nodes[static_cast<std::size_t>(index)])
                return false;
            const Node& node = *nodes[static_cast<std::size_t>(index)];
            // Original room assets name background meshes sky*. They describe
            // the view background, not the physical extent of a room part.
            if (node.Enabled && node.MeshCount > 0 && !node.Name.starts_with("sky"))
            {
                if (!node.Bounds || node.Bounds->Length() < 6) return false;
                const auto& bounds = *node.Bounds;
                const Vector3 nodeMin = Vector3(bounds[0], bounds[1], bounds[2]) + offset;
                const Vector3 nodeMax = Vector3(bounds[3], bounds[4], bounds[5]) + offset;
                if (!std::isfinite(nodeMin.X) || !std::isfinite(nodeMin.Y) || !std::isfinite(nodeMin.Z)
                    || !std::isfinite(nodeMax.X) || !std::isfinite(nodeMax.Y) || !std::isfinite(nodeMax.Z)
                    || nodeMin.X > nodeMax.X || nodeMin.Y > nodeMax.Y || nodeMin.Z > nodeMax.Z)
                    return false;
                // Do not bridge disconnected pieces with one aggregate AABB.
                // Portal membership is checked separately by Find.
                containsPosition |= position.X >= nodeMin.X - tolerance && position.X <= nodeMax.X + tolerance
                    && position.Y >= nodeMin.Y - tolerance && position.Y <= nodeMax.Y + tolerance
                    && position.Z >= nodeMin.Z - tolerance && position.Z <= nodeMax.Z + tolerance;
            }
            index = node.NextIndex;
        }
        return index == -1 && containsPosition;
    }
}
