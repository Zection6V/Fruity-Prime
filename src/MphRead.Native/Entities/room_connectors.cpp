// Native counterpart of RoomEntity's connector handling.
#include "room_connectors.hpp"

#include <limits>
#include <utility>

namespace fruityprime::scene {

namespace {

[[nodiscard]] constexpr float fixed_to_float(int value) noexcept {
    return static_cast<float>(value) / 4096.0F;
}

[[nodiscard]] bool starts_with_rm(const std::string& name) noexcept {
    return name.size() >= 2 && name[0] == 'r' && name[1] == 'm';
}

} // namespace

const std::array<formats::Vector3, 27>& connector_sizes() noexcept {
    // Two entries per unit's four connector shapes, then the oddly sized ones:
    // 0xA60F is 10.3787, and the pair at 20/21 is the sloped connector whose
    // two halves are not mirror images of each other.
    static const std::array<formats::Vector3, 27> sizes{{
        {10.0F, 0.0F, 0.0F},
        {10.0F, 0.0F, 0.0F},
        {0.0F, 0.0F, 10.0F},
        {0.0F, 0.0F, 10.0F},
        {10.0F, 0.0F, 0.0F},
        {10.0F, 0.0F, 0.0F},
        {0.0F, 0.0F, 10.0F},
        {0.0F, 0.0F, 10.0F},
        {fixed_to_float(0xA60F), 0.0F, 0.0F},
        {fixed_to_float(0xA60F), 0.0F, 0.0F},
        {0.0F, 0.0F, fixed_to_float(0xA60F)},
        {0.0F, 0.0F, fixed_to_float(0xA60F)},
        {10.0F, 0.0F, 0.0F},
        {10.0F, 0.0F, 0.0F},
        {0.0F, 0.0F, 10.0F},
        {0.0F, 0.0F, 10.0F},
        {10.0F, 0.0F, 0.0F},
        {10.0F, 0.0F, 0.0F},
        {0.0F, 0.0F, 10.0F},
        {0.0F, 0.0F, 10.0F},
        {0.0F, fixed_to_float(0x24B9), fixed_to_float(0x16A77)},
        {0.0F, fixed_to_float(-7659), fixed_to_float(0x16A76)},
        {10.0F, 0.0F, 0.0F},
        {10.0F, 0.0F, 0.0F},
        {0.0F, 0.0F, 20.0F},
        {0.0F, 0.0F, 10.0F},
        {0.0F, 0.0F, 10.0F}
    }};
    return sizes;
}

formats::Vector3 connector_size(int connector_id,
                                formats::Vector3 door_facing) noexcept {
    if (connector_id < 0
        || connector_id >= static_cast<int>(connector_sizes().size())) {
        return {};
    }
    formats::Vector3 size =
        connector_sizes()[static_cast<std::size_t>(connector_id)];
    // The two axes are tested separately rather than by picking the dominant
    // one, which is what the cartridge does: a door facing diagonally into
    // +X +Z flips once, not twice.
    if (door_facing.x > ConnectorFacingThreshold
        || door_facing.z > ConnectorFacingThreshold) {
        size = {-size.x, -size.y, -size.z};
    }
    return size;
}

formats::Vector3 connector_collision_translation(
    formats::Vector3 door_position, formats::Vector3 size) noexcept {
    return {door_position.x + size.x / 2.0F, door_position.y + size.y / 2.0F,
            door_position.z + size.z / 2.0F};
}

formats::Vector3 connector_loader_door_position(
    formats::Vector3 door_position, formats::Vector3 size) noexcept {
    return {door_position.x + size.x, door_position.y + size.y,
            door_position.z + size.z};
}

std::string connector_node_name(std::span<const std::string> node_names) {
    for (const std::string& name : node_names) {
        if (starts_with_rm(name)) {
            return name;
        }
    }
    return "rmMain";
}

std::size_t RoomConnectors::add_connector(
    int connector_id, int door_entity_id, formats::Vector3 door_position,
    formats::Vector3 door_facing, std::string room_name,
    std::span<const std::string> node_names, bool in_room_transition) {
    if (connector_id < 0
        || connector_id >= static_cast<int>(connector_sizes().size())) {
        return (std::numeric_limits<std::size_t>::max)();
    }
    Connector connector;
    connector.connector_id = connector_id;
    connector.door_entity_id = door_entity_id;
    connector.size = connector_size(connector_id, door_facing);
    connector.collision_translation =
        connector_collision_translation(door_position, connector.size);
    connector.room_name = std::move(room_name);
    connector.node_name = connector_node_name(node_names);
    // A connector starts switched off; walking into its door is what turns it
    // on, and only one may be on at a time.
    connector.model_active = false;
    connector.collision_active = false;
    connector.loader_door_inactive = true;
    connector.node_anim_ignore_root = !in_room_transition;
    connectors_.push_back(std::move(connector));
    return connectors_.size() - 1;
}

bool RoomConnectors::activate_connector(int door_entity_id) noexcept {
    bool found = false;
    for (Connector& connector : connectors_) {
        // Everything is switched off first, including the one being turned
        // on: the connectors occupy the same space, so leaving a second one
        // live puts two sets of walls in the same corridor.
        connector.model_active = false;
        connector.collision_active = false;
        connector.loader_door_inactive = true;
        if (connector.door_entity_id == door_entity_id) {
            found = true;
        }
    }
    if (!found) {
        return false;
    }
    for (Connector& connector : connectors_) {
        if (connector.door_entity_id == door_entity_id) {
            connector.model_active = true;
            connector.collision_active = true;
            connector.loader_door_inactive = false;
        }
    }
    return true;
}

const Connector* RoomConnectors::active_connector() const noexcept {
    for (const Connector& connector : connectors_) {
        if (connector.model_active) {
            return &connector;
        }
    }
    return nullptr;
}

void RoomConnectors::set_loader_door_entity_id(std::size_t index,
                                               int entity_id) noexcept {
    if (index >= connectors_.size()) {
        return;
    }
    connectors_[index].loader_door_entity_id = entity_id;
}

} // namespace fruityprime::scene
