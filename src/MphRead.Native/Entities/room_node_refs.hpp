#pragma once

// Native counterpart of RoomEntity's node-reference lookups.
//
// A room is cut into parts by portals, and a node reference names the part an
// entity is in.  The engine uses it to decide what to draw, what to hear, and
// where a remote player's puppet is standing.
//
// The position lookup is the one that matters most: it returns None for a
// position no part contains -- the top of AD2 ALINOS PERCH, among others --
// and a caller that keeps the previous reference on that answer leaves a
// player visible from somewhere they cannot be seen.  The failure is
// deliberately distinguishable from a stale reference, which is why the
// probing form below is separate.

#include "Formats/culling.hpp"
#include "Formats/Types.hpp"

#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

namespace fruityprime::scene {

// Formats.Portal, reduced to what the node-reference lookups read.
struct Portal {
    std::string_view name;
    formats::Vector4 plane{};
    culling::NodeRef node_ref1;
    culling::NodeRef node_ref2;
    bool active = true;
};

// One room part's portal boundary: each portal, and which side of it the part
// lies on.
struct PortalSide {
    const Portal* portal = nullptr;
    bool other_side = false;
};

// The room's portal geometry, which is all the lookups need.
struct RoomPortals {
    std::vector<Portal> portals;
    // One list per room part.
    std::vector<std::vector<PortalSide>> portal_sides;
    // RoomEntity._activeRoomParts / _audibleRoomParts
    std::vector<bool> active_parts;
    std::vector<bool> audible_parts;
};

// RoomEntity.GetPortalByName
[[nodiscard]] const Portal* get_portal_by_name(const RoomPortals& room,
                                               std::string_view name) noexcept;

// One of the room model's nodes, reduced to what the name lookup reads.
struct RoomNode {
    std::string_view name;
    // Which room part this node belongs to; -1 for a node that belongs to
    // none, which the name lookup never returns.
    int room_part_id = -1;
    int child_index = -1;
};

// RoomEntity.GetNodeRefByName: the node reference for a node named in the
// data -- a camera sequence keyframe names one, for instance.  The reference
// always points at the room's own model (index 0), never a connector's,
// because that is the only model these names come from.  A node that has no
// room part or no children is not a room part's node and is skipped rather
// than returned with a -1 in it.
[[nodiscard]] culling::NodeRef get_node_ref_by_name(
    std::span<const RoomNode> nodes, std::string_view room_name,
    std::string_view node_name) noexcept;

// RoomEntity.GetNodeRefByPosition.  A position outside every part returns
// None; the caller decides what to do about that rather than silently
// inheriting the last answer.
[[nodiscard]] culling::NodeRef get_node_ref_by_position(
    const RoomPortals& room, formats::Vector3 position) noexcept;

// RoomEntity.UpdateNodeRef: follow an entity across a portal it crossed
// between two positions.  `crossed` reports whether a portal was crossed,
// which is how the caller distinguishes "still here" from "could not tell".
[[nodiscard]] culling::NodeRef update_node_ref(
    const RoomPortals& room, const culling::NodeRef& current,
    formats::Vector3 previous, formats::Vector3 current_position,
    bool* crossed = nullptr) noexcept;

// RoomEntity.IsNodeRefAudible: a reference with no part is audible, because
// sound with no room part is not positional.
[[nodiscard]] bool is_node_ref_audible(const RoomPortals& room,
                                       const culling::NodeRef& node_ref) noexcept;

// RoomEntity.IsNodeRefVisible: a reference with no part is *not* visible.
// The asymmetry with audibility is in the managed source and is what keeps an
// unplaceable entity from being drawn through a wall.
[[nodiscard]] bool is_node_ref_visible(const RoomPortals& room,
                                       const culling::NodeRef& node_ref) noexcept;

} // namespace fruityprime::scene
