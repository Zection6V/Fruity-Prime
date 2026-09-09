// Item pickup, authored item spawns, and timed drops.
//
// This is kept as a separate Session slice because the managed tree has
// ItemInstanceEntity/ItemSpawnEntity rather than folding inventory mutation
// into the player or enemy update code.
#include "Entities/gameplay.hpp"
#include "Entities/Players/PlayerEntity.hpp"
#include "gameplay_helpers.hpp"

#include <limits>

namespace fruityprime::gameplay {
using namespace detail;

bool Session::apply_item_pickup(ItemState& item, net::PlayerState& player,
                                RuntimeInput& runtime) {
    const net::Vec3 between = subtract(item.position, player.position);
    const float pickup_radius = config_.body_radius + 0.45F;
    const float pickup_radius_squared = pickup_radius * pickup_radius;
    bool in_range = false;
    if ((player.flags & net::PlayerState::FlagAltForm) != 0) {
        in_range = length_squared(between) < pickup_radius_squared;
    } else {
        const float lateral_squared = between.x * between.x
            + between.z * between.z;
        in_range = lateral_squared < pickup_radius_squared
            && between.y >= -0.5F && between.y <= 1.1F;
    }
    if (!in_range) {
        return false;
    }

    const ItemType type = item.type;
    if (is_health_item(type)) {
        // Prime Hunter is the managed mode's target and cannot consume
        // health pickups. The item remains in the room in that case.
        if (config_.mode == 14
            && objectives_.prime_hunter
                == static_cast<std::int8_t>(player.slot_index)) {
            return false;
        }
        const std::uint32_t amount = type == ItemType::HealthSmall ? 30u
            : type == ItemType::HealthMedium ? 60u : 100u;
        player.health = static_cast<std::uint16_t>(std::min<std::uint32_t>(
            static_cast<std::uint32_t>(player.health) + amount,
            runtime.inventory.health_max));
        return true;
    }

    if (is_ammo_item(type)) {
        const bool missiles = type == ItemType::MissileSmall
            || type == ItemType::MissileBig;
        const std::size_t pool = missiles ? 1u : 0u;
        const std::uint32_t amount = (type == ItemType::UABig
                                      || type == ItemType::MissileBig)
            ? 100u : 50u;
        add_capped(runtime.inventory.ammo[pool], amount,
                   runtime.inventory.ammo_max[pool]);
        return true;
    }

    if (is_weapon_item(type)) {
        const std::uint8_t weapon = weapon_for_item(type, runtime.hunter);
        if (weapon == 0xff) {
            return false;
        }
        // Samus and Guardian use the Missile affinity entry as ammunition;
        // all other hunters turn the generic affinity entry into their own
        // weapon, matching Weapons.AffinityWeapons in the managed code.
        if (type == ItemType::AffinityWeapon
            && (runtime.hunter == 0 || runtime.hunter == 7)) {
            add_capped(runtime.inventory.ammo[1], 50,
                       runtime.inventory.ammo_max[1]);
            return true;
        }
        const bool was_available = runtime.inventory.available_weapons[weapon];
        runtime.inventory.available_weapons[weapon] = true;
        const std::size_t pool = weapon == 1 ? 1u : 0u;
        const std::uint16_t cap = std::min<std::uint16_t>(
            60, runtime.inventory.ammo_max[pool]);
        if (runtime.inventory.ammo[pool] < cap) {
            add_capped(runtime.inventory.ammo[pool], 60, cap);
        }
        if (!was_available) {
            const std::uint8_t previous_weapon = player.current_weapon;
            player.current_weapon = weapon;
            players::PlayerEntity::SessionWeaponChanged(
                *this, player.slot_index, previous_weapon, weapon);
        }
        return true;
    }

    switch (type) {
    case ItemType::DoubleDamage:
        runtime.inventory.double_damage_ticks = 900u * 2u;
        return true;
    case ItemType::Cloak:
        runtime.inventory.cloak_ticks = 900u * 2u;
        return true;
    case ItemType::Deathalt:
        runtime.inventory.deathalt_ticks = 900u * 2u;
        player.flags |= net::PlayerState::FlagAltForm;
        return true;
    case ItemType::EnergyTank:
        runtime.inventory.health_max = static_cast<std::uint16_t>(std::min<
            std::uint32_t>(std::numeric_limits<std::uint16_t>::max(),
                           static_cast<std::uint32_t>(
                               runtime.inventory.health_max) + 100u));
        return true;
    case ItemType::MissileExpansion:
        runtime.inventory.ammo_max[1] = static_cast<std::uint16_t>(std::min<
            std::uint32_t>(std::numeric_limits<std::uint16_t>::max(),
                           static_cast<std::uint32_t>(
                               runtime.inventory.ammo_max[1]) + 100u));
        return true;
    case ItemType::UAExpansion:
        runtime.inventory.ammo_max[0] = static_cast<std::uint16_t>(std::min<
            std::uint32_t>(std::numeric_limits<std::uint16_t>::max(),
                           static_cast<std::uint32_t>(
                               runtime.inventory.ammo_max[0]) + 300u));
        return true;
    case ItemType::ArtifactKey:
        return true;
    default:
        return false;
    }
}

void Session::spawn_item_drop(ItemType type, net::Vec3 position) {
    if (type == ItemType::None) {
        return;
    }
    items_.push_back(ItemState{
        type, add(position, {0.0F, 0.35F, 0.0F}), -1, 300u * 2u, true
    });
}

void Session::update_items() {
    for (auto& runtime : inputs_) {
        if (runtime.inventory.double_damage_ticks > 0) {
            --runtime.inventory.double_damage_ticks;
        }
        if (runtime.inventory.cloak_ticks > 0) {
            --runtime.inventory.cloak_ticks;
        }
        if (runtime.inventory.deathalt_ticks > 0) {
            --runtime.inventory.deathalt_ticks;
        }
    }

    for (std::size_t i = 0; i < items_.size();) {
        auto& item = items_[i];
        if (item.timed && item.remaining_ticks > 0) {
            --item.remaining_ticks;
        }
        if (item.timed && item.remaining_ticks == 0) {
            items_.erase(items_.begin() + static_cast<std::ptrdiff_t>(i));
            continue;
        }
        ++i;
    }

    for (auto& spawner : item_spawns_) {
        if (!spawner.active) {
            continue;
        }
        const bool has_item = std::find_if(
            items_.begin(), items_.end(),
            [&spawner](const ItemState& item) {
                return item.owner_entity_id == spawner.entity_id;
            }) != items_.end();
        if (has_item) {
            continue;
        }
        if (spawner.spawn_cooldown > 0) {
            --spawner.spawn_cooldown;
        }
        if (spawner.spawn_cooldown != 0
            || (spawner.data.max_spawn_count != 0
                && spawner.spawn_count >= spawner.data.max_spawn_count)) {
            continue;
        }
        const ItemType type = item_type_from_record(spawner.data.item_type);
        if (type == ItemType::None) {
            continue;
        }
        items_.push_back(ItemState{
            type, add(spawner.position, {0.0F, 0.65F, 0.0F}),
            spawner.entity_id, 0, false
        });
        SoundEvent spawn_event;
        spawn_event.cue = SoundCue::ItemSpawn;
        spawn_event.item_type = type;
        spawn_event.entity_id = static_cast<std::uint32_t>(
            std::max<std::int16_t>(spawner.entity_id, 0));
        spawn_event.position = add(spawner.position, {0.0F, 0.65F, 0.0F});
        emit_sound(spawn_event);
        ++spawner.spawn_count;
        spawner.spawn_cooldown = static_cast<std::uint32_t>(
            spawner.data.spawn_interval) * 2u;
    }

    for (auto& player : players_) {
        if (!objective_player(player)) {
            continue;
        }
        const auto runtime = std::find_if(
            inputs_.begin(), inputs_.end(), [&player](const RuntimeInput& value) {
                return value.slot == player.slot_index;
            });
        if (runtime == inputs_.end()) {
            continue;
        }
        for (std::size_t i = 0; i < items_.size();) {
            const std::int16_t owner_id = items_[i].owner_entity_id;
            if (!apply_item_pickup(items_[i], player, *runtime)) {
                ++i;
                continue;
            }
            SoundEvent pickup_event;
            pickup_event.cue = SoundCue::ItemPickup;
            pickup_event.slot = player.slot_index;
            pickup_event.item_type = items_[i].type;
            pickup_event.position = items_[i].position;
            emit_sound(pickup_event);
            items_.erase(items_.begin() + static_cast<std::ptrdiff_t>(i));
            if (owner_id >= 0) {
                const auto spawner = std::find_if(
                    item_spawns_.begin(), item_spawns_.end(),
                    [owner_id](const ItemSpawnRuntime& value) {
                        return value.entity_id == owner_id;
                    });
                if (spawner != item_spawns_.end()
                    && story_save_ != nullptr
                    && story_room_id_ >= 27 && story_room_id_ <= 92
                    && !spawner->data.always_active) {
                    story_save_->set_room_state(story_room_id_, owner_id, 1);
                }
                if (spawner != item_spawns_.end()
                    && spawner->data.collected_message != 0
                    && spawner->data.notify_entity_id >= 0) {
                    dispatch_environment_message(
                        spawner->data.notify_entity_id,
                        spawner->data.collected_message,
                        spawner->data.collected_parameter1,
                        spawner->data.collected_parameter2);
                }
            }
        }
    }
}

} // namespace fruityprime::gameplay
