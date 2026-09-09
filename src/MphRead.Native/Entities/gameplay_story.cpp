// Adventure-save inventory and room-state integration.
//
// These methods mirror the managed story boundary: PlayerEntity inventory is
// restored from StorySave, while room-scoped dynamic entities use the same
// packed state and message transitions as the cartridge.
#include "Entities/gameplay.hpp"
#include "Entities/Players/PlayerEntity.hpp"
#include "Metadata/metadata.hpp"
#include "gameplay_helpers.hpp"

#include <algorithm>
#include <limits>
#include <stdexcept>

namespace fruityprime::gameplay {
using namespace detail;

void Session::apply_story_save(std::uint8_t slot,
                               const game::StorySave& save) {
    const std::size_t player_slot = player_index(slot);
    const auto runtime = std::find_if(
        inputs_.begin(), inputs_.end(),
        [slot](const RuntimeInput& value) { return value.slot == slot; });
    if (runtime == inputs_.end()) {
        throw std::out_of_range("story save player slot is not active");
    }

    const auto health_max = static_cast<std::uint16_t>(std::clamp(
        save.health_max, 1, static_cast<int>(
            std::numeric_limits<std::uint16_t>::max())));
    runtime->inventory.health_max = health_max;
    players_[player_slot].health = static_cast<std::uint16_t>(std::clamp(
        save.health, 0, static_cast<int>(health_max)));
    for (std::size_t index = 0; index < runtime->inventory.ammo.size();
         ++index) {
        runtime->inventory.ammo_max[index] = static_cast<std::uint16_t>(
            std::clamp(save.ammo_max[index], 0, static_cast<int>(
                std::numeric_limits<std::uint16_t>::max())));
        runtime->inventory.ammo[index] = static_cast<std::uint16_t>(
            std::clamp(save.ammo[index], 0, static_cast<int>(
                runtime->inventory.ammo_max[index])));
    }
    for (std::size_t weapon = 0;
         weapon < runtime->inventory.available_weapons.size(); ++weapon) {
        const auto beam_type = metadata::beam_type_from_native_weapon_slot(
            static_cast<std::uint8_t>(weapon));
        runtime->inventory.available_weapons[weapon] = beam_type >= 0
            && (save.weapons & (std::uint16_t{1} << beam_type)) != 0;
    }
    // The power beam is always a valid story fallback, even if a damaged or
    // hand-edited save has no weapon bits set.
    runtime->inventory.available_weapons[0] = true;
    std::uint8_t selected = 0;
    const auto native_slot = metadata::native_weapon_slot_from_beam(
        save.weapon_slots[0]);
    if (native_slot != 0xff
        && native_slot < runtime->inventory.available_weapons.size()
        && runtime->inventory.available_weapons[native_slot]) {
        selected = native_slot;
    }
    const std::uint8_t previous_weapon = players_[player_slot].current_weapon;
    players_[player_slot].current_weapon = selected;
    players::PlayerEntity::SessionWeaponChanged(
        *this, slot, previous_weapon, selected);
}

void Session::apply_story_room_state(std::int32_t room_id,
                                     game::StorySave& save) {
    if (config_.mode != static_cast<std::uint8_t>(game::Mode::SinglePlayer)
        || room_id < 27 || room_id > 92) {
        story_save_ = nullptr;
        story_room_id_ = -1;
        story_area_id_ = -1;
        return;
    }
    story_save_ = &save;
    story_room_id_ = room_id;
    story_area_id_ = metadata::area_info(room_id);

    // Room-scoped state is initialized from the authored active bit on the
    // first visit, then the packed save value becomes authoritative.  This is
    // the same boundary used by the managed ItemSpawn/EnemySpawn/volume
    // constructors, but applied after Session has decoded its environment.
    for (auto& spawn : player_spawns_) {
        const auto state = save.init_room_state(
            room_id, spawn.entity_id, spawn.default_active);
        spawn.active = state != 0;
    }
    for (auto& spawn : item_spawns_) {
        const auto state = save.init_room_state(
            room_id, spawn.entity_id, spawn.data.enabled);
        spawn.active = spawn.data.always_active
            ? spawn.data.enabled : state != 0;
        if (!spawn.active) {
            items_.erase(std::remove_if(
                items_.begin(), items_.end(),
                [id = spawn.entity_id](const ItemState& item) {
                    return item.owner_entity_id == id;
                }), items_.end());
        }
    }

    for (auto& spawn : enemy_spawns_) {
        if (spawn.spawner == nullptr) {
            continue;
        }
        const auto state = save.init_room_state(
            room_id, spawn.entity_id, spawn.data.active);
        const bool active = spawn.data.always_active
            ? spawn.data.active : state != 0;
        spawn.spawner->activate(active);
    }

    for (auto& field : force_fields_) {
        const auto state = save.init_room_state(
            room_id, field.entity_id, field.default_active);
        field.active = state != 0;
        if (field.active) {
            spawn_force_field_lock(field);
        } else {
            remove_force_field_lock(field);
        }
    }

    for (auto& jump_pad : jump_pads_) {
        const auto state = save.init_room_state(
            room_id, jump_pad.entity_id, jump_pad.active);
        jump_pad.active = state != 0;
    }
    for (auto& teleporter : teleporters_) {
        const auto state = save.init_room_state(
            room_id, teleporter.entity_id, teleporter.active);
        teleporter.active = state != 0;
    }
    for (auto& area : area_volumes_) {
        const auto state = save.init_room_state(
            room_id, area.entity_id, area.data.active);
        area.active = area.data.always_active
            ? area.data.active : state != 0;
    }
    for (auto& trigger : trigger_volumes_) {
        const auto state = save.init_room_state(
            room_id, trigger.entity_id, trigger.data.active);
        trigger.active = trigger.data.always_active
            ? trigger.data.active : state != 0;
    }
}

void Session::update_story_room_state(std::int32_t target_id,
                                      std::uint32_t message,
                                      std::int32_t parameter1) noexcept {
    if (story_save_ == nullptr || story_room_id_ < 27
        || story_room_id_ > 92 || target_id < 0) {
        return;
    }
    bool active = false;
    bool recognized = true;
    if (message == MessageSetActive) {
        active = parameter1 != 0;
    } else if (message == MessageUnlock) {
        active = false;
    } else if (message == MessageLock) {
        active = true;
    } else if (message == MessageActivate || message == 44) {
        active = true;
    } else if (message == 6 || message == 45) {
        active = false;
    } else {
        recognized = false;
    }
    if (!recognized) {
        return;
    }

    const auto target = static_cast<std::int16_t>(target_id);
    bool found = false;
    for (const auto& spawn : player_spawns_) {
        if (spawn.entity_id == target) {
            found = true;
            break;
        }
    }
    for (const auto& spawn : item_spawns_) {
        if (!found && spawn.entity_id == target) {
            found = true;
            break;
        }
    }
    if (!found) {
        for (const auto& spawn : enemy_spawns_) {
            if (spawn.entity_id == target) {
                found = true;
                break;
            }
        }
    }
    if (!found) {
        for (const auto& field : force_fields_) {
            if (field.entity_id == target) {
                found = true;
                break;
            }
        }
    }
    if (!found) {
        for (const auto& jump_pad : jump_pads_) {
            if (jump_pad.entity_id == target) {
                found = true;
                break;
            }
        }
    }
    if (!found) {
        for (const auto& teleporter : teleporters_) {
            if (teleporter.entity_id == target) {
                found = true;
                break;
            }
        }
    }
    if (!found) {
        for (const auto& area : area_volumes_) {
            if (area.entity_id == target) {
                found = true;
                break;
            }
        }
    }
    if (!found) {
        for (const auto& trigger : trigger_volumes_) {
            if (trigger.entity_id == target) {
                found = true;
                break;
            }
        }
    }
    if (found) {
        story_save_->set_room_state(story_room_id_, target_id,
                                    active ? 3 : 1);
    }
}

const InventoryState& Session::inventory(std::uint8_t slot) const {
    const auto found = std::find_if(
        inputs_.begin(), inputs_.end(),
        [slot](const RuntimeInput& value) { return value.slot == slot; });
    if (found == inputs_.end()) {
        throw std::out_of_range("gameplay inventory slot is not active");
    }
    return found->inventory;
}

} // namespace fruityprime::gameplay
