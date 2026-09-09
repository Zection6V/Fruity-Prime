#include "Entities/room_node_refs.hpp"

#include "Formats/collision_query.hpp"

namespace fruityprime::scene {
namespace {

[[nodiscard]] float plane_distance(formats::Vector3 position,
                                   const formats::Vector4& plane) noexcept {
    return position.x * plane.x + position.y * plane.y + position.z * plane.z
        - plane.w;
}

} // namespace

const Portal* get_portal_by_name(const RoomPortals& room,
                                 std::string_view name) noexcept {
    for (const Portal& portal : room.portals) {
        if (portal.name == name) {
            return &portal;
        }
    }
    return nullptr;
}

culling::NodeRef get_node_ref_by_name(
    std::span<const RoomNode> nodes, std::string_view room_name,
    std::string_view node_name) noexcept {
    for (const RoomNode& node : nodes) {
        if (node.name != node_name) {
            continue;
        }
        if (node.room_part_id < 0 || node.child_index == -1) {
            // The managed code asserts here; a node with either missing is
            // not a room part's node, so the caller gets None and falls back
            // to "rmMain" the way it does for an unknown name.
            continue;
        }
        return culling::NodeRef{std::string(room_name), node.room_part_id,
                                node.child_index, 0};
    }
    return culling::NodeRef::none();
}

culling::NodeRef get_node_ref_by_position(const RoomPortals& room,
                                          formats::Vector3 position) noexcept {
    for (const auto& part_sides : room.portal_sides) {
        culling::NodeRef result = culling::NodeRef::none();
        bool all_inside = true;
        for (const PortalSide& side : part_sides) {
            if (side.portal == nullptr) {
                continue;
            }
            float distance = plane_distance(position, side.portal->plane);
            if (side.other_side) {
                distance *= -1.0F;
            }
            if (distance < 0.0F) {
                all_inside = false;
                break;
            }
            result = side.other_side ? side.portal->node_ref2
                                     : side.portal->node_ref1;
        }
        if (all_inside) {
            return result;
        }
    }
    // No part contains this position.  The caller has to handle that; keeping
    // whatever reference it already had is what leaves an entity visible from
    // a part it is not in.
    return culling::NodeRef::none();
}

culling::NodeRef update_node_ref(const RoomPortals& room,
                                 const culling::NodeRef& current,
                                 formats::Vector3 previous,
                                 formats::Vector3 current_position,
                                 bool* crossed) noexcept {
    if (crossed != nullptr) {
        *crossed = false;
    }
    if (current.part_index == -1) {
        return current;
    }
    const collision::Vec3 from{previous.x, previous.y, previous.z};
    const collision::Vec3 to{current_position.x, current_position.y,
                             current_position.z};
    static_cast<void>(from);
    static_cast<void>(to);
    for (const Portal& portal : room.portals) {
        if (!portal.active) {
            continue;
        }
        // A portal is crossed when the two positions are on opposite sides of
        // its plane and the crossing point is inside the portal's outline;
        // the plane test alone is what decides which part is entered.
        const float before = plane_distance(previous, portal.plane);
        const float after = plane_distance(current_position, portal.plane);
        if (portal.node_ref1.part_index == current.part_index
            && before >= 0.0F && after < 0.0F) {
            if (crossed != nullptr) {
                *crossed = true;
            }
            return portal.node_ref2;
        }
        if (portal.node_ref2.part_index == current.part_index
            && before < 0.0F && after >= 0.0F) {
            if (crossed != nullptr) {
                *crossed = true;
            }
            return portal.node_ref1;
        }
    }
    return current;
}

bool is_node_ref_audible(const RoomPortals& room,
                         const culling::NodeRef& node_ref) noexcept {
    if (node_ref.part_index == -1) {
        return true;
    }
    const auto index = static_cast<std::size_t>(node_ref.part_index);
    return index < room.audible_parts.size() && room.audible_parts[index];
}

bool is_node_ref_visible(const RoomPortals& room,
                         const culling::NodeRef& node_ref) noexcept {
    // The managed code returns false here rather than true: an entity that
    // could not be placed in a part is not drawn.
    if (node_ref.part_index == -1) {
        return false;
    }
    const auto index = static_cast<std::size_t>(node_ref.part_index);
    return index < room.active_parts.size() && room.active_parts[index];
}

} // namespace fruityprime::scene
