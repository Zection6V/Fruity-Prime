// Room environment, message dispatch, and area-volume simulation.
//
// These methods are the native counterpart of the dynamic room entities
// (ForceFieldEntity, AreaVolumeEntity, TriggerVolumeEntity, EnemySpawnEntity,
// and their message graph). They are kept separate from the fixed-step
// orchestrator so the source layout follows the managed Entities boundary.
#include "Entities/gameplay.hpp"
#include "Mods/world_events.hpp"
#include "Metadata/metadata.hpp"
#include "Entities/room_catalog.hpp"
#include "gameplay_helpers.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <string_view>
#include <utility>

namespace fruityprime::gameplay {
using namespace detail;

namespace gorea_message {
constexpr std::uint32_t LaserActive = 1u << 1;
constexpr std::uint32_t Teleporting = 1u << 4;
constexpr std::uint32_t Teleporting2 = 1u << 5;
constexpr std::uint32_t IntroDone = 1u << 6;
constexpr std::uint32_t DamageSequence = 1u << 10;
constexpr std::uint32_t Phase0 = 1u << 14;
constexpr std::uint32_t Phase1 = 1u << 15;
} // namespace gorea_message

[[nodiscard]] bool overlaps_sphere(const scene::EntityVolume& volume,
                                   scene::VolumePoint center,
                                   float radius) noexcept {
    radius = std::max(radius, 0.0F);
    if (volume.kind == scene::VolumeKind::Sphere) {
        const auto delta = subtract(to_net(center),
                                    to_net(volume.sphere_position));
        const float combined = radius + volume.sphere_radius;
        return length_squared(delta) <= combined * combined;
    }
    if (volume.kind == scene::VolumeKind::Cylinder) {
        const net::Vec3 bottom = to_net(volume.cylinder_position);
        const net::Vec3 axis = multiply(to_net(volume.cylinder_vector),
                                        volume.cylinder_dot);
        const float axis_squared = length_squared(axis);
        if (axis_squared <= 0.0000001F) {
            return false;
        }
        const net::Vec3 offset = subtract(to_net(center), bottom);
        const float amount = std::clamp(dot(offset, axis) / axis_squared,
                                        0.0F, 1.0F);
        const net::Vec3 closest = add(bottom, multiply(axis, amount));
        const float combined = radius + volume.cylinder_radius;
        return length_squared(subtract(to_net(center), closest))
            <= combined * combined;
    }
    if (volume.kind == scene::VolumeKind::Box) {
        const net::Vec3 offset = subtract(to_net(center),
                                          to_net(volume.box_position));
        const std::array<net::Vec3, 3> axes{
            to_net(volume.box_vector1), to_net(volume.box_vector2),
            to_net(volume.box_vector3)};
        const std::array<float, 3> extents{
            volume.box_dot1, volume.box_dot2, volume.box_dot3};
        float outside_squared = 0.0F;
        for (std::size_t index = 0; index < axes.size(); ++index) {
            const float axis_squared = length_squared(axes[index]);
            if (axis_squared <= 0.0000001F) {
                return false;
            }
            const float projection = dot(offset, axes[index]);
            const float clamped = std::clamp(projection, 0.0F,
                                             extents[index]);
            const float distance = (projection - clamped)
                / std::sqrt(axis_squared);
            outside_squared += distance * distance;
        }
        return outside_squared <= radius * radius;
    }
    return false;
}

void Session::initialize_environment() {
    items_.clear();
    enemies_.clear();
    item_spawns_.clear();
    enemy_spawns_.clear();
    force_fields_.clear();
    morph_cameras_.clear();
    player_spawns_.clear();
    next_enemy_id_ = 0x80000000u;
    jump_pads_.clear();
    teleporters_.clear();
    area_volumes_.clear();
    trigger_volumes_.clear();
    room_transition_.reset();
    for (const auto& entity : room_.entities()) {
        if (entity.kind == scene::EntityKind::ForceField) {
            const auto* data = std::get_if<scene::ForceFieldData>(
                &entity.typed_data);
            if (data == nullptr) {
                continue;
            }
            ForceFieldRuntime field;
            field.entity_id = entity.entity_id;
            field.type = data->type;
            field.position = to_float(entity.position);
            field.up = normalized_or(to_float(entity.up_vector),
                                    {0.0F, 1.0F, 0.0F});
            field.facing = normalized_or(to_float(entity.facing_vector),
                                        {0.0F, 0.0F, 1.0F});
            field.right = normalized_or(cross(field.up, field.facing),
                                        {1.0F, 0.0F, 0.0F});
            field.width = std::max(0.0F, data->width);
            field.height = std::max(0.0F, data->height);
            field.default_active = data->active;
            field.active = data->active;
            force_fields_.push_back(field);
        } else if (entity.kind == scene::EntityKind::MorphCamera) {
            const auto* data = std::get_if<scene::MorphCameraData>(
                &entity.typed_data);
            if (data != nullptr
                && data->volume.kind != scene::VolumeKind::Invalid) {
                morph_cameras_.push_back(MorphCameraRuntime{
                    entity.entity_id, data->volume, to_float(entity.position)});
            }
        } else if (entity.kind == scene::EntityKind::PlayerSpawn) {
            const auto* data = std::get_if<scene::PlayerSpawnData>(
                &entity.typed_data);
            PlayerSpawnRuntime spawn;
            spawn.entity_id = entity.entity_id;
            spawn.position = to_float(entity.position);
            spawn.facing = normalized_or(to_float(entity.facing_vector),
                                         {0.0F, 0.0F, 1.0F});
            spawn.up = normalized_or(to_float(entity.up_vector),
                                    {0.0F, 1.0F, 0.0F});
            spawn.default_active = data != nullptr && data->active;
            spawn.availability = data != nullptr && data->availability != 0;
            spawn.active = spawn.default_active;
            player_spawns_.push_back(spawn);
        } else if (entity.kind == scene::EntityKind::EnemySpawn) {
            const auto* data = std::get_if<scene::EnemySpawnData>(
                &entity.typed_data);
            if (data == nullptr) {
                continue;
            }
            EnemySpawnRuntime spawn_runtime;
            spawn_runtime.entity_id = entity.entity_id;
            spawn_runtime.data = *data;
            spawn_runtime.facing = normalized_or(to_float(entity.facing_vector),
                                                {0.0F, 0.0F, 1.0F});
            spawn_runtime.up = normalized_or(to_float(entity.up_vector),
                                             {0.0F, 1.0F, 0.0F});
            spawn_runtime.spawner = std::make_unique<fruityprime::runtime::EnemySpawnerEntity>(
                static_cast<std::uint32_t>(
                    static_cast<std::uint16_t>(entity.entity_id)),
                data->enemy_type,
                to_float(entity.position),
                data->spawn_total,
                data->spawn_limit,
                data->spawn_count,
                data->cooldown_time,
                data->initial_cooldown,
                data->active,
                data->always_active,
                data->active_distance);
            enemy_spawns_.push_back(std::move(spawn_runtime));
        } else if (entity.kind == scene::EntityKind::ItemSpawn) {
            const auto* data = std::get_if<scene::ItemSpawnData>(
                &entity.typed_data);
            if (data == nullptr) {
                continue;
            }
            ItemSpawnRuntime item_spawn;
            item_spawn.entity_id = entity.entity_id;
            item_spawn.data = *data;
            item_spawn.position = to_float(entity.position);
            item_spawn.spawn_cooldown = static_cast<std::uint32_t>(
                data->spawn_delay) * 2u;
            item_spawn.active = data->enabled;
            item_spawns_.push_back(std::move(item_spawn));
        } else if (entity.kind == scene::EntityKind::JumpPad) {
            const auto* data = std::get_if<scene::JumpPadData>(
                &entity.typed_data);
            if (data == nullptr
                || data->volume.kind == scene::VolumeKind::Invalid) {
                continue;
            }
            const net::Vec3 forward = normalized_or(
                to_float(entity.facing_vector), {0.0F, 0.0F, 1.0F});
            const net::Vec3 up = normalized_or(
                to_float(entity.up_vector), {0.0F, 1.0F, 0.0F});
            const net::Vec3 right = normalized_or(
                cross(up, forward), {1.0F, 0.0F, 0.0F});
            const net::Vec3 local_beam = normalized_or(
                to_net(data->beam_vector), {0.0F, 1.0F, 0.0F});
            const net::Vec3 world_beam = add(
                add(multiply(right, local_beam.x), multiply(up, local_beam.y)),
                multiply(forward, local_beam.z));
            jump_pads_.push_back(JumpPadRuntime{
                entity.entity_id,
                data->volume,
                multiply(normalized_or(world_beam, {0.0F, 1.0F, 0.0F}),
                         data->speed),
                forward,
                static_cast<std::uint32_t>(data->control_lock_time) * 2u,
                static_cast<std::uint32_t>(data->cooldown_time) * 2u,
                0,
                data->trigger_flags,
                data->active
            });
        } else if (entity.kind == scene::EntityKind::Teleporter) {
            const auto* data = std::get_if<scene::TeleporterData>(
                &entity.typed_data);
            if (data == nullptr) {
                continue;
            }
            TeleporterRuntime teleporter;
            teleporter.entity_id = entity.entity_id;
            teleporter.position = to_float(entity.position);
            teleporter.target_position = to_net(data->target_position);
            teleporter.facing = normalized_or(
                to_float(entity.facing_vector), {0.0F, 0.0F, 1.0F});
            teleporter.target_room_name = room_name_for_entity_filename(
                data->entity_filename);
            teleporter.target_entity_id = data->target_index;
            teleporter.triggered.fill(true);
            teleporter.teleport_radius = data->artifact_id < 8
                ? 1.5F : 1.0F;
            teleporter.active = data->active;
            teleporters_.push_back(teleporter);
        } else if (entity.kind == scene::EntityKind::AreaVolume) {
            const auto* data = std::get_if<scene::AreaVolumeData>(
                &entity.typed_data);
            if (data == nullptr
                || data->volume.kind == scene::VolumeKind::Invalid) {
                continue;
            }
            AreaRuntime area;
            area.entity_id = entity.entity_id;
            area.data = *data;
            area.cooldown_ticks = data->cooldown > 0
                ? static_cast<std::uint32_t>(data->cooldown - 1) * 2u : 0u;
            area.active = data->active;
            area_volumes_.push_back(std::move(area));
        } else if (entity.kind == scene::EntityKind::TriggerVolume) {
            const auto* data = std::get_if<scene::TriggerVolumeData>(
                &entity.typed_data);
            if (data == nullptr
                || data->volume.kind == scene::VolumeKind::Invalid) {
                continue;
            }
            TriggerRuntime trigger;
            trigger.entity_id = entity.entity_id;
            trigger.data = *data;
            trigger.repeat_ticks = static_cast<std::uint32_t>(
                data->repeat_delay) * 2u;
            trigger.active = data->active;
            trigger_volumes_.push_back(std::move(trigger));
        }
    }

    // ForceFieldEntity creates its Enemy49 lock during Initialize rather than
    // from an EnemySpawn record. Materialize the same linked runtime child so
    // direct Session users and the Scene/Win32 host see the lock immediately.
    for (auto& field : force_fields_) {
        if (field.active && field.type <= 8) {
            spawn_force_field_lock(field);
        }
    }
}

std::optional<net::Vec3> Session::morph_camera_position(
    std::uint8_t slot) const noexcept {
    const auto player = std::find_if(
        players_.begin(), players_.end(), [slot](const net::PlayerState& value) {
            return value.slot_index == slot;
        });
    if (player == players_.end()
        || (player->flags & net::PlayerState::FlagAltForm) == 0) {
        return std::nullopt;
    }
    const auto runtime = std::find_if(
        inputs_.begin(), inputs_.end(), [slot](const RuntimeInput& value) {
            return value.slot == slot;
        });
    if (runtime == inputs_.end()) {
        return std::nullopt;
    }
    const auto& profile = players::profiles()[std::min<std::size_t>(
        runtime->hunter, players::ProfileCount - 1)];
    const float radius = players::Profile::fixed_to_float(
        profile.alt_collision_radius);
    scene::VolumePoint center = to_volume_point(player->position);
    center.y += players::Profile::fixed_to_float(profile.alt_collision_y);
    for (const auto& camera : morph_cameras_) {
        if (overlaps_sphere(camera.volume, center, radius)) {
            return camera.position;
        }
    }
    return std::nullopt;
}

void Session::spawn_force_field_lock(ForceFieldRuntime& field) {
    if (!field.active || field.type > 8) {
        return;
    }
    if (field.lock_id != 0) {
        const auto existing = std::find_if(
            enemies_.begin(), enemies_.end(),
            [&field](const EnemyState& value) {
                return value.id == field.lock_id && value.active;
            });
        if (existing != enemies_.end()) {
            return;
        }
        field.lock_id = 0;
    }

    EnemyState lock;
    lock.id = next_enemy_id_++;
    lock.enemy_type = static_cast<std::uint8_t>(
        formats::EnemyType::ForceFieldLock);
    lock.position = add(field.position, multiply(field.facing,
                                                   409.0F / 4096.0F));
    lock.behavior_origin = field.position;
    lock.facing = field.facing;
    lock.up = field.up;
    lock.health = lock.health_max = 1;
    lock.body_radius = 0.5F;
    lock.force_field_entity_id = field.entity_id;
    lock.force_field_type = static_cast<std::uint8_t>(field.type);
    lock.force_field_origin = field.position;
    lock.force_field_up = field.up;
    lock.force_field_facing = field.facing;
    lock.force_field_right = field.right;
    lock.force_field_width = field.width;
    lock.force_field_height = field.height;
    lock.force_field_velocity = {};
    lock.force_field_shot_timer = 0;
    lock.force_field_bounce = false;
    lock.active = true;
    lock.visible = true;
    lock.invulnerable = false;
    lock.spawner_entity_id = -1;
    enemies_.push_back(std::move(lock));
    field.lock_id = enemies_.back().id;
}

void Session::remove_force_field_lock(ForceFieldRuntime& field) noexcept {
    if (field.lock_id == 0) {
        return;
    }
    const auto lock = std::find_if(
        enemies_.begin(), enemies_.end(),
        [&field](const EnemyState& value) { return value.id == field.lock_id; });
    if (lock != enemies_.end()) {
        lock->active = false;
        lock->visible = false;
        lock->health = 0;
    }
    field.lock_id = 0;
}

void Session::apply_area_effect(const scene::AreaVolumeData& area,
                                net::PlayerState& player,
                                RuntimeInput& runtime) {
    const auto apply_damage = [this, &player, &runtime](std::uint32_t amount) {
        if (amount == 0 || player.health == 0) {
            return;
        }
        const std::uint16_t old_health = player.health;
        player.health = amount >= old_health
            ? 0
            : static_cast<std::uint16_t>(old_health - amount);
        player.damage_sequence = static_cast<std::uint8_t>(
            player.damage_sequence + 1);
        player.attacker_slot = 0xff;
        player.damage_beam = 0xff;
        player.hit_direction = {};
        SoundEvent damage_event;
        damage_event.cue = SoundCue::PlayerDamage;
        damage_event.slot = player.slot_index;
        damage_event.position = player.position;
        emit_sound(damage_event);
        if (player.health != 0) {
            return;
        }
        SoundEvent death_event;
        death_event.cue = SoundCue::PlayerDeath;
        death_event.slot = player.slot_index;
        death_event.position = player.position;
        emit_sound(death_event);
        increment_saturating(player.deaths);
        player.flags &= static_cast<std::uint8_t>(
            ~(net::PlayerState::FlagSpawned
              | net::PlayerState::FlagAltForm
              | net::PlayerState::FlagZoomed
              | net::PlayerState::FlagFrozen));
        player.speed = {};
        if (config_.survival_mode
            && player.deaths > config_.survival_lives) {
            runtime.eliminated = true;
            runtime.respawn_ticks = 0;
        } else {
            runtime.eliminated = false;
            runtime.respawn_ticks = config_.respawn_ticks;
        }
        runtime.fire_cooldown = 0;
    };

    switch (area.inside_message) {
    case MessageDamage:
        apply_damage(area.inside_parameter1 > 0
            ? static_cast<std::uint32_t>(area.inside_parameter1) : 0u);
        return;
    case MessageDeath:
        apply_damage(area.inside_parameter1 > 0
            ? static_cast<std::uint32_t>(area.inside_parameter1)
            : static_cast<std::uint32_t>(runtime.inventory.health_max));
        return;
    case MessageGravity:
        // Managed Gravity parameters are fixed-point integers. The highest
        // priority volume wins for the current player and frame.
        if (area.inside_parameter1 != 0
            && area.priority >= runtime.gravity_priority) {
            runtime.gravity_override = static_cast<float>(
                area.inside_parameter1) / 4096.0F;
            runtime.gravity_priority = area.priority;
            runtime.gravity_override_active = true;
        }
        return;
    case MessagePreventFormSwitch:
        runtime.prevent_form_switch = true;
        return;
    case MessageDripMoatPlatform:
        runtime.biped_lock = area.inside_parameter1 != 0;
        return;
    default:
        break;
    }

    // Non-player area messages are sent to the managed parent entity. The
    // native environment currently handles activation of volume-backed
    // entities; other message recipients remain inert until their runtime
    // entity is migrated.
    if (area.inside_message != 0 && area.inside_message != 22
        && area.parent_id >= 0) {
        dispatch_environment_message(area.parent_id, area.inside_message,
                                     area.inside_parameter1,
                                     area.inside_parameter2);
    }
}

void Session::dispatch_message(const messaging::MessageInfo& info) {
    const std::uint32_t message = info.cartridge_message != 0
        ? info.cartridge_message
        : static_cast<std::uint32_t>(info.message);
    dispatch_environment_message(info.target, message, info.parameter1,
                                 info.parameter2, false, info.sender);
}

void Session::dispatch_environment_message(std::int32_t target_id,
                                           std::uint32_t message,
                                           std::int32_t parameter1,
                                           std::int32_t parameter2,
                                           bool forward_to_scene,
                                           std::int32_t sender_id) {
    static_cast<void>(parameter2);
    if (target_id < 0) {
        return;
    }
    const auto target = static_cast<std::int16_t>(target_id);

    // Enemy31Entity.HandleMessage receives the actual TriggerVolumeEntity as
    // MessageInfo.Sender. The native environment used to discard that
    // identity and therefore Gorea2 never reacted to authored Gorea2Trigger
    // links, even though the same link still reached the other recipients.
    // Reconstruct the small managed dispatch boundary from the room IDs:
    // EnemySpawnEntity forwards this message to the first Gorea2 instance in
    // the scene, and the Gorea2 controller uses the triggering volume as its
    // current teleport destination.
    if (message == 36 && sender_id >= 0) {
        TriggerRuntime* sender_trigger = nullptr;
        for (auto& trigger : trigger_volumes_) {
            if (trigger.entity_id == sender_id) {
                sender_trigger = &trigger;
                break;
            }
        }
        if (sender_trigger != nullptr) {
            auto gorea2 = std::find_if(
                enemies_.begin(), enemies_.end(),
                [target_id](const EnemyState& value) {
                    if (value.enemy_type != static_cast<std::uint8_t>(
                            formats::EnemyType::Gorea2)) {
                        return false;
                    }
                    // A normal trigger targets the Gorea2 spawner. Keep the
                    // fallback below for rooms whose link targets a generated
                    // child or a relay endpoint.
                    return value.spawner_entity_id == target_id;
                });
            if (gorea2 == enemies_.end()) {
                gorea2 = std::find_if(
                    enemies_.begin(), enemies_.end(),
                    [](const EnemyState& value) {
                        return value.enemy_type == static_cast<std::uint8_t>(
                            formats::EnemyType::Gorea2);
                    });
            }
            if (gorea2 != enemies_.end()) {
                const std::uint32_t phase =
                    (gorea2->gorea_flags
                     & (gorea_message::Phase0 | gorea_message::Phase1)) >> 14;
                const std::uint32_t teleport_value =
                    (gorea2->gorea_flags
                     & (gorea_message::Teleporting
                        | gorea_message::Teleporting2)) >> 4;
                if ((gorea2->gorea_flags & gorea_message::DamageSequence) == 0
                    && phase != 1) {
                    const auto teleport_to_trigger = [&]() {
                        gorea2->gorea_flags &= ~gorea_message::LaserActive;
                        gorea2->gorea_flags &= ~(gorea_message::Teleporting
                                                | gorea_message::Teleporting2);
                        gorea2->gorea_flags |= gorea_message::Teleporting
                            | gorea_message::IntroDone;
                        gorea2->gorea_current_trigger_id =
                            sender_trigger->entity_id;
                        gorea2->gorea_teleport_destination = to_net(
                            sender_trigger->data.volume.center());
                        gorea2->gorea_field22c = 15u * 2u;
                        gorea2->gorea_targetable = false;
                        const auto sphere = std::find_if(
                            enemies_.begin(), enemies_.end(),
                            [id = gorea2->id](const EnemyState& value) {
                                return value.parent_enemy_id == id
                                    && value.enemy_type
                                        == static_cast<std::uint8_t>(
                                            formats::EnemyType::GoreaSealSphere2);
                            });
                        spawn_effect(80,
                                     sphere != enemies_.end()
                                         ? sphere->position
                                         : gorea2->position,
                                     gorea2->facing, gorea2->id);
                    };
                    if (teleport_value == 0 && gorea2->gorea_field22c == 0) {
                        teleport_to_trigger();
                    } else if (teleport_value == 3) {
                        if (gorea2->gorea_current_trigger_id
                            == sender_trigger->entity_id) {
                            gorea2->gorea_field22c = 90u * 2u;
                        } else {
                            teleport_to_trigger();
                        }
                    }
                }
            }
        }
    }
    for (auto& player_spawn : player_spawns_) {
        if (player_spawn.entity_id != target) {
            continue;
        }
        if (message == MessageActivate) {
            player_spawn.active = true;
        } else if (message == MessageSetActive) {
            player_spawn.active = parameter1 != 0;
        }
    }
    for (auto& item_spawn : item_spawns_) {
        if (item_spawn.entity_id != target) {
            continue;
        }
        if (message == MessageActivate) {
            item_spawn.active = true;
        } else if (message == MessageSetActive) {
            item_spawn.active = parameter1 != 0;
            if (!item_spawn.active) {
                const auto item = std::find_if(
                    items_.begin(), items_.end(),
                    [target](const ItemState& value) {
                        return value.owner_entity_id == target;
                    });
                if (item != items_.end()) {
                    items_.erase(item);
                    if (item_spawn.spawn_count > 0) {
                        --item_spawn.spawn_count;
                    }
                }
            }
        }
    }
    for (auto& jump_pad : jump_pads_) {
        if (jump_pad.entity_id != target) {
            continue;
        }
        if (message == MessageActivate) {
            jump_pad.active = true;
        } else if (message == MessageSetActive) {
            jump_pad.active = parameter1 != 0;
        }
    }
    for (auto& teleporter : teleporters_) {
        if (teleporter.entity_id != target) {
            continue;
        }
        if (message == MessageActivate) {
            teleporter.active = true;
        } else if (message == MessageSetActive) {
            teleporter.active = parameter1 != 0;
        }
    }
    for (auto& area : area_volumes_) {
        if (area.entity_id != target) {
            continue;
        }
        if (message == MessageActivate) {
            area.active = true;
        } else if (message == MessageSetActive) {
            area.active = parameter1 != 0;
        }
    }
    for (auto& trigger : trigger_volumes_) {
        if (trigger.entity_id != target) {
            continue;
        }
        if (message == MessageActivate) {
            trigger.active = true;
        } else if (message == MessageSetActive) {
            trigger.active = parameter1 != 0;
            if (!trigger.active && trigger.data.subtype
                == TriggerTypeAutomatic) {
                trigger.repeat_ticks = static_cast<std::uint32_t>(
                    trigger.data.repeat_delay) * 2u;
            }
        } else if (message == MessageTrigger
                   && trigger.data.subtype == TriggerTypeThreshold) {
            if (trigger.threshold_count < trigger.data.trigger_threshold) {
                ++trigger.threshold_count;
            }
            if (trigger.threshold_count >= trigger.data.trigger_threshold) {
                trigger.pending = true;
                trigger.repeat_ticks = static_cast<std::uint32_t>(
                    trigger.data.check_delay) * 2u;
            }
        } else if (trigger.data.subtype == TriggerTypeRelay) {
            // Relay volumes forward the incoming message unchanged.
            if (trigger.data.parent_id >= 0) {
                dispatch_environment_message(trigger.data.parent_id, message,
                                             parameter1, parameter2, true,
                                             sender_id);
            }
            if (trigger.data.child_id >= 0) {
                dispatch_environment_message(trigger.data.child_id, message,
                                             parameter1, parameter2, true,
                                             sender_id);
            }
        }
    }
    for (auto& spawner : enemy_spawns_) {
        if (spawner.entity_id != target || spawner.spawner == nullptr) {
            continue;
        }
        if (message == MessageActivate) {
            spawner.spawner->activate(true);
        } else if (message == MessageSetActive) {
            spawner.spawner->activate(parameter1 != 0);
        }
    }

    // ForceFieldEntity owns a linked Enemy49 lock rather than an ordinary
    // EnemySpawn record.  Keep both the barrier and its lock in step with the
    // cartridge message graph, including the common SetActive/Activate
    // aliases used by room scripts.
    for (auto& field : force_fields_) {
        if (field.entity_id != target) {
            continue;
        }
        if (message == MessageUnlock) {
            field.active = false;
            remove_force_field_lock(field);
        } else if (message == MessageLock) {
            field.active = true;
            spawn_force_field_lock(field);
        } else if (message == MessageSetActive) {
            field.active = parameter1 != 0;
            if (field.active) {
                spawn_force_field_lock(field);
            } else {
                remove_force_field_lock(field);
            }
        } else if (message == MessageActivate || message == 44) {
            field.active = true;
            spawn_force_field_lock(field);
        } else if (message == 6 || message == 45) {
            field.active = false;
            remove_force_field_lock(field);
        }
    }

    // Enemy instances are the dynamic recipients of the same cartridge
    // message graph. The managed scene routes these messages through the
    // entity list; keep the native session from stopping at the spawner and
    // losing activation/deactivation state after an enemy has been created.
    for (auto& enemy : enemies_) {
        if (enemy.spawner_entity_id != target
            && static_cast<std::int32_t>(enemy.id) != target) {
            continue;
        }
        if (message == MessageSetActive) {
            enemy.active = parameter1 != 0;
        } else if (message == MessageActivate || message == 44) {
            enemy.active = true;
        } else if (message == 6 || message == 45) {
            enemy.active = false;
        } else if (message == 48 && enemy.enemy_type == static_cast<std::uint8_t>(
                       formats::EnemyType::SlenchTurret)) {
            const bool was_enabled = enemy.turret_enabled;
            enemy.turret_enabled = true;
            if (!was_enabled) {
                // Enemy45Entity.ActivateTurret starts State0 with a fresh
                // burst; it does not inherit the spawn-time Alimbic delay.
                enemy.state = 0;
                enemy.target_slot = 0xff;
                enemy.turret_salvo_cooldown = 0;
                enemy.turret_shot_timer = 0;
            }
        } else if (message == 51 && enemy.enemy_type == static_cast<std::uint8_t>(
                       formats::EnemyType::SlenchTurret)) {
            enemy.turret_enabled = false;
            enemy.state = 3;
            enemy.target_slot = 0xff;
            enemy.turret_shot_timer = 0;
        }
    }
    update_story_room_state(target_id, message, parameter1);
    if (forward_to_scene && message_sink_) {
        messaging::MessageInfo forwarded;
        forwarded.cartridge_message = message;
        forwarded.sender = sender_id;
        forwarded.target = target_id;
        forwarded.parameter1 = parameter1;
        forwarded.parameter2 = parameter2;
        forwarded.queued_frame = tick_count_;
        forwarded.execute_frame = tick_count_ + 1;
        message_sink_(forwarded);
    }
}

void Session::complete_enemy_spawner(
    std::int16_t entity_id, std::uint8_t enemy_type,
    const scene::EnemySpawnData& data) {
    if (story_save_ != nullptr && story_room_id_ >= 27
        && story_room_id_ <= 92) {
        story_save_->set_room_state(story_room_id_, entity_id, 1);

        const auto hunter_type = static_cast<std::uint8_t>(
            formats::EnemyType::Hunter);
        const std::uint32_t encounter_type = enemy_type == hunter_type
            ? read_enemy_field_u32(data.fields, 4) : 0;
        if (story_area_id_ >= 0 && story_area_id_ < 8
            && (enemy_type != hunter_type || encounter_type == 1)
            && (enemy_type >> 3) < story_save_->enemy_encounters[0].size()) {
            story_save_->enemy_encounters[
                static_cast<std::size_t>(story_area_id_)][enemy_type >> 3]
                = static_cast<std::uint8_t>(
                    story_save_->enemy_encounters[
                        static_cast<std::size_t>(story_area_id_)][enemy_type >> 3]
                    | (1u << (enemy_type & 7)));
        }

        const auto set_boss_escape = [this](std::int32_t area_id) {
            if (story_save_ == nullptr || area_id < 0 || area_id > 8) {
                return;
            }
            const auto shift = static_cast<unsigned>(area_id * 2);
            const auto mask = static_cast<std::uint32_t>(3u << shift);
            story_save_->boss_flags = (story_save_->boss_flags & ~mask)
                | static_cast<std::uint32_t>(1u << shift);
        };
        if (enemy_type == static_cast<std::uint8_t>(
                              formats::EnemyType::Cretaphid)) {
            story_save_->areas = static_cast<std::uint16_t>(
                story_save_->areas | 0x0003u);
            set_boss_escape(story_area_id_);
        } else if (enemy_type == static_cast<std::uint8_t>(
                                     formats::EnemyType::Slench)) {
            story_save_->areas = static_cast<std::uint16_t>(
                story_save_->areas | 0x00f0u);
            set_boss_escape(story_area_id_);
        } else if (enemy_type == static_cast<std::uint8_t>(
                                     formats::EnemyType::Gorea1A)) {
            set_boss_escape(story_area_id_);
        }
    }

    const std::array<scene::MessageLinkData, 3> messages{
        data.message1, data.message2, data.message3};
    for (const auto& link : messages) {
        if (link.target >= 0 && link.message != 0) {
            dispatch_environment_message(link.target, link.message,
                                         link.parameter1, link.parameter2);
        }
    }
}

void Session::update_environment() {
    // Area effects are recomputed each frame. This also naturally clears
    // gravity/form locks on the first frame after a player leaves a volume.
    for (auto& input : inputs_) {
        input.gravity_override = 0.0F;
        input.gravity_priority = 0;
        input.gravity_override_active = false;
        input.prevent_form_switch = false;
        input.biped_lock = false;
    }

    update_enemy_spawns();

    for (auto& jump_pad : jump_pads_) {
        if (jump_pad.remaining_ticks > 0) {
            --jump_pad.remaining_ticks;
        }
        if (!jump_pad.active || jump_pad.remaining_ticks > 0) {
            continue;
        }
        bool launched = false;
        for (auto& player : players_) {
            if (!objective_player(player)) {
                continue;
            }
            const bool alt_form = (player.flags & net::PlayerState::FlagAltForm)
                != 0;
            const std::uint32_t required_flag = alt_form
                ? TriggerPlayerAlt : TriggerPlayerBiped;
            if ((jump_pad.trigger_flags & required_flag) == 0
                || !jump_pad.volume.contains(to_volume_point(player.position))) {
                continue;
            }
            player.speed = jump_pad.impulse;
            const auto runtime = std::find_if(
                inputs_.begin(), inputs_.end(), [&player](const RuntimeInput& value) {
                    return value.slot == player.slot_index;
                });
            if (runtime != inputs_.end()) {
                runtime->control_lock_ticks = jump_pad.control_lock_ticks;
            }
            world::WorldEvents::NoteJumpPad(
                player.slot_index, jump_pad.entity_id);
            launched = true;
        }
        if (launched) {
            jump_pad.remaining_ticks = jump_pad.cooldown_ticks;
        }
    }

    for (auto& teleporter : teleporters_) {
        if (!teleporter.active) {
            continue;
        }
        for (auto& player : players_) {
            if (!objective_player(player)
                || player.slot_index >= teleporter.triggered.size()) {
                continue;
            }
            const net::Vec3 delta = subtract(player.position,
                                             teleporter.position);
            const float horizontal_squared = delta.x * delta.x
                + delta.z * delta.z;
            const bool in_activation_volume = delta.y >= -1.5F
                && delta.y <= 1.5F
                && horizontal_squared <= teleporter.activation_radius
                    * teleporter.activation_radius;
            if (!in_activation_volume) {
                teleporter.triggered[player.slot_index] = false;
                continue;
            }
            if (horizontal_squared > teleporter.teleport_radius
                    * teleporter.teleport_radius) {
                teleporter.triggered[player.slot_index] = false;
                continue;
            }
            if (teleporter.triggered[player.slot_index]) {
                continue;
            }
            if (!teleporter.target_room_name.empty()) {
                if (!room_transition_.has_value()) {
                    room_transition_ = RoomTransitionRequest{
                        teleporter.target_room_name,
                        teleporter.target_entity_id,
                        (player.flags & net::PlayerState::FlagAltForm) != 0,
                        static_cast<std::uint32_t>(static_cast<std::uint16_t>(
                            teleporter.entity_id))};
                }
                player.speed = {0.0F, player.speed.y, 0.0F};
                teleporter.triggered[player.slot_index] = true;
                world::WorldEvents::NoteTeleport(
                    player.slot_index, teleporter.entity_id);
                continue;
            }
            player.position = teleporter.target_position;
            player.position.y += 0.5F;
            player.speed = {0.0F, player.speed.y, 0.0F};
            player.facing = teleporter.facing;
            teleporter.triggered[player.slot_index] = true;
            world::WorldEvents::NoteTeleport(
                player.slot_index, teleporter.entity_id);
        }
    }

    for (auto& area : area_volumes_) {
        if (!area.active) {
            continue;
        }
        for (auto& player : players_) {
            if (player.slot_index >= area.triggered.size()) {
                continue;
            }
            auto runtime = std::find_if(
                inputs_.begin(), inputs_.end(), [&player](const RuntimeInput& value) {
                    return value.slot == player.slot_index;
                });
            if (runtime == inputs_.end()) {
                continue;
            }
            if (!objective_player(player)) {
                area.triggered[player.slot_index] = false;
                area.cooldown[player.slot_index] = 0;
                continue;
            }
            const bool alt_form = (player.flags & net::PlayerState::FlagAltForm)
                != 0;
            const std::uint32_t required_flag = alt_form
                ? TriggerPlayerAlt : TriggerPlayerBiped;
            const bool inside = (area.data.trigger_flags & required_flag) != 0
                && area.data.volume.contains(to_volume_point(player.position));
            if (!inside) {
                if (area.triggered[player.slot_index]) {
                    area.triggered[player.slot_index] = false;
                    area.cooldown[player.slot_index] = area.cooldown_ticks;
                    if (area.data.exit_message == MessageDamage
                        || area.data.exit_message == MessageDeath) {
                        scene::AreaVolumeData exit = area.data;
                        exit.inside_message = area.data.exit_message;
                        exit.inside_parameter1 = area.data.exit_parameter1;
                        exit.inside_parameter2 = area.data.exit_parameter2;
                        apply_area_effect(exit, player, *runtime);
                    } else if (area.data.exit_message != 0
                               && area.data.exit_message != 22
                               && area.data.child_id >= 0) {
                        dispatch_environment_message(
                            area.data.child_id, area.data.exit_message,
                            area.data.exit_parameter1,
                            area.data.exit_parameter2);
                    }
                }
                continue;
            }

            const bool entering = !area.triggered[player.slot_index];
            if (entering) {
                area.triggered[player.slot_index] = true;
                area.cooldown[player.slot_index] = 0;
            }
            bool send_event = entering;
            if (!entering && area.data.allow_multiple) {
                if (area.cooldown[player.slot_index] > 0) {
                    --area.cooldown[player.slot_index];
                } else {
                    send_event = true;
                    area.cooldown[player.slot_index] = area.cooldown_ticks;
                }
            }
            const bool persistent = area.data.inside_message == MessageGravity
                || area.data.inside_message == MessagePreventFormSwitch
                || area.data.inside_message == MessageDripMoatPlatform;
            if (persistent || send_event) {
                apply_area_effect(area.data, player, *runtime);
            }
        }
    }

    for (auto& trigger : trigger_volumes_) {
        if (!trigger.active) {
            continue;
        }
        if (trigger.repeat_ticks > 0) {
            --trigger.repeat_ticks;
        }
        bool colliding = false;
        if (trigger.data.subtype == TriggerTypeVolume) {
            for (auto& player : players_) {
                if (player.slot_index >= trigger.inside.size()) {
                    continue;
                }
                const bool alt_form = (player.flags
                    & net::PlayerState::FlagAltForm) != 0;
                const std::uint32_t required_flag = alt_form
                    ? TriggerPlayerAlt : TriggerPlayerBiped;
                const bool inside = objective_player(player)
                    && (trigger.data.trigger_flags & required_flag) != 0
                    && trigger.data.volume.contains(
                        to_volume_point(player.position));
                trigger.inside[player.slot_index] = inside;
                colliding = colliding || inside;
            }
            if (!colliding && trigger.data.check_delay != 0) {
                trigger.repeat_ticks = static_cast<std::uint32_t>(
                    trigger.data.check_delay) * 2u;
            }
        }

        bool fire = trigger.pending;
        if (trigger.data.subtype == TriggerTypeVolume
            || trigger.data.subtype == TriggerTypeAutomatic) {
            fire = fire || colliding;
        }
        if (!fire || trigger.repeat_ticks > 0) {
            continue;
        }
        if (trigger.data.parent_message.message != 0
            && trigger.data.parent_id >= 0) {
            dispatch_environment_message(
                trigger.data.parent_id, trigger.data.parent_message.message,
                trigger.data.parent_message.parameter1,
                trigger.data.parent_message.parameter2, true,
                trigger.entity_id);
        }
        if (trigger.data.child_message.message != 0
            && trigger.data.child_id >= 0) {
            dispatch_environment_message(
                trigger.data.child_id, trigger.data.child_message.message,
                trigger.data.child_message.parameter1,
                trigger.data.child_message.parameter2, true,
                trigger.entity_id);
        }
        trigger.pending = false;
        trigger.repeat_ticks = static_cast<std::uint32_t>(
            trigger.data.repeat_delay) * 2u;
        if (trigger.data.deactivate_after_use) {
            trigger.active = false;
        }
        if (trigger.data.subtype == TriggerTypeStateBits) {
            trigger.active = false;
        }
    }
}

} // namespace fruityprime::gameplay
