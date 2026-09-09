#pragma once

// Native counterpart of RoomEntity's connector handling.
//
// A story room is loaded with the short connecting corridors of every room it
// leads to already built behind its doors, so walking through one is not a
// load.  Only one connector may be live at a time -- they overlap in space --
// which is why activating one deactivates every other and marks every other
// loader door inactive in the same pass.
//
// The size table is the piece worth stating: a connector is placed by
// offsetting from the door, and the offset's sign depends on which way the
// door faces.  The managed test is `facing.X > 0.70703125 || facing.Z >
// 0.70703125` -- 2896/4096, the cosine of 45 degrees -- so a door facing
// broadly +X or +Z pushes the connector the other way.  Testing the axes
// separately, rather than picking the dominant one, is what the cartridge
// does and is kept.

#include "Formats/Types.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace fruityprime::scene {

// RoomEntity._connectorSizes, indexed by the door's connector id.
[[nodiscard]] const std::array<formats::Vector3, 27>&
    connector_sizes() noexcept;

// The cosine of 45 degrees in the cartridge's 1/4096 fixed point.
inline constexpr float ConnectorFacingThreshold = 2896.0F / 4096.0F;

// RoomEntity.AddConnector's size step: the table entry, negated when the door
// faces broadly +X or +Z.  An id outside the table returns a zero offset
// rather than reading past the end.
[[nodiscard]] formats::Vector3 connector_size(
    int connector_id, formats::Vector3 door_facing) noexcept;

// Where the connector's collision goes: half a size past the door.
[[nodiscard]] formats::Vector3 connector_collision_translation(
    formats::Vector3 door_position, formats::Vector3 size) noexcept;

// Where the loader door at the far end of the connector goes: a whole size
// past, facing back the way the original door faces.
[[nodiscard]] formats::Vector3 connector_loader_door_position(
    formats::Vector3 door_position, formats::Vector3 size) noexcept;

// RoomEntity.AddConnector's node search: the first node whose name starts
// with "rm", or "rmMain" when the connector has none.
[[nodiscard]] std::string connector_node_name(
    std::span<const std::string> node_names);

// One connector, as RoomEntity tracks it.
struct Connector {
    int connector_id = -1;
    // The door in the loaded room that this connector hangs off.
    int door_entity_id = -1;
    // The loader door created at the far end.
    int loader_door_entity_id = -1;
    formats::Vector3 size{};
    formats::Vector3 collision_translation{};
    std::string room_name;
    std::string node_name;
    bool model_active = false;
    bool collision_active = false;
    bool loader_door_inactive = true;
    // RoomEntity's "hack": a connector added outside a room transition belongs
    // to the room the player is standing in, and is flagged so the transition
    // can tell the two sets apart.
    bool node_anim_ignore_root = false;
};

// RoomEntity's connector list, plus the transition cancellation flag.
class RoomConnectors final {
public:
    // RoomEntity.AddConnector.  Returns the index of the connector added, or
    // SIZE_MAX when the id is outside the size table.
    std::size_t add_connector(int connector_id, int door_entity_id,
                              formats::Vector3 door_position,
                              formats::Vector3 door_facing,
                              std::string room_name,
                              std::span<const std::string> node_names,
                              bool in_room_transition);

    // RoomEntity.ActivateConnector.  Exactly one connector is live and exactly
    // one loader door is reachable afterwards; everything else is switched
    // off in the same pass, because the connectors overlap in space.
    bool activate_connector(int door_entity_id) noexcept;

    // RoomEntity.CancelTransition: the room load running on another thread is
    // asked to stop.  Nothing else here changes -- the connector that was
    // being walked into stays live until the transition either finishes or is
    // started again.
    void cancel_transition() noexcept { transition_cancelled_ = true; }
    [[nodiscard]] bool transition_cancelled() const noexcept {
        return transition_cancelled_;
    }
    void reset_transition() noexcept { transition_cancelled_ = false; }

    [[nodiscard]] const std::vector<Connector>& connectors() const noexcept {
        return connectors_;
    }
    [[nodiscard]] const Connector* active_connector() const noexcept;
    void set_loader_door_entity_id(std::size_t index, int entity_id) noexcept;

private:
    std::vector<Connector> connectors_;
    bool transition_cancelled_ = false;
};

} // namespace fruityprime::scene
