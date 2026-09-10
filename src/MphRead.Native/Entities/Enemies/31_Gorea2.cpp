#include "33_GoreaMeteor.hpp"
#include "31_Gorea2.hpp"
#include "enemy_common.hpp"
#include "gorea_common.hpp"

#include <algorithm>
#include <cmath>
#include <initializer_list>
#include <limits>
#include <utility>

namespace fruityprime::gameplay {

namespace {

[[nodiscard]] constexpr std::uint8_t type_id(formats::EnemyType type) noexcept {
    return static_cast<std::uint8_t>(type);
}

[[nodiscard]] net::Vec3 remove_projection(net::Vec3 value,
                                           net::Vec3 axis) noexcept {
    const float axis_length_squared = length_squared(axis);
    if (axis_length_squared <= std::numeric_limits<float>::epsilon()) {
        return value;
    }
    return subtract(value, multiply(axis,
                                    dot(value, axis)
                                        / axis_length_squared));
}

[[nodiscard]] net::Vec3 gorea2_player_volume_point(
    const EnemyState& agent, net::Vec3 player_position) noexcept {
    // This is Func21405FC -> Func204E2A8 from Enemy31Entity.  The latter is
    // retained instead of replacing it with a normalized direction*radius
    // shortcut because the cartridge deliberately returns the centre when
    // the authored radius is <= 1.
    const net::Vec3 direction_source = subtract(player_position,
                                                agent.gorea_anchor);
    const float source_length_squared = length_squared(direction_source);
    if (source_length_squared <= 1.0F / 128.0F) {
        return {};
    }
    const net::Vec3 direction = multiply(
        direction_source, 1.0F / std::sqrt(source_length_squared));
    const float direction_length_squared = length_squared(direction);
    const float radius = agent.gorea_volume_radius;
    const float discriminant = -(direction_length_squared
                                 * (-(radius * radius) * 4.0F));
    if (discriminant < 0.0F) {
        return {};
    }
    float distance = 0.0F;
    if (discriminant > 4.0F) {
        const float root = std::sqrt(discriminant);
        const float positive = root / (direction_length_squared * 2.0F);
        const float negative = -root / (direction_length_squared * 2.0F);
        distance = positive;
        if (negative > 0.0F && negative < positive) {
            distance = negative;
        }
    }
    return add(agent.gorea_anchor, multiply(direction, distance));
}

[[nodiscard]] net::Vec3 gorea2_fallback_tangent(
    net::Vec3 target_position, net::Vec3 anchor) noexcept {
    const net::Vec3 between = subtract(target_position, anchor);
    const net::Vec3 up{0.0F, 1.0F, 0.0F};
    net::Vec3 tangent = remove_projection(between, up);
    if (length_squared(tangent) < 1.0F / 128.0F) {
        const net::Vec3 right{1.0F, 0.0F, 0.0F};
        tangent = remove_projection(between, right);
    }
    // Matches Func213FA58: the cross is intentionally not normalized here;
    // the caller applies Normalized() after selecting the fallback.
    return cross(tangent, up);
}

[[nodiscard]] float point_segment_distance_squared(
    net::Vec3 point, net::Vec3 start, net::Vec3 end) noexcept {
    const net::Vec3 segment = subtract(end, start);
    const float segment_length_squared = length_squared(segment);
    if (segment_length_squared <= std::numeric_limits<float>::epsilon()) {
        return distance_squared(point, start);
    }
    const float fraction = std::clamp(
        dot(subtract(point, start), segment) / segment_length_squared,
        0.0F, 1.0F);
    return distance_squared(point, add(start, multiply(segment, fraction)));
}

[[nodiscard]] float vector_length(net::Vec3 value) noexcept {
    return std::sqrt(length_squared(value));
}

} // namespace

void Session::update_gorea_2(EnemyState& agent) {
    // Enemy31Entity owns the second boss encounter. Its 19 managed states
    // are represented here as explicit native states: intro/hover, laser
    // charge and fire, meteor launch, teleport fade, damage-light changes,
    // and the terminal death state. Rendering consumes the same animation
    // cursor but does not decide any of these gameplay events.
    if (!agent.active) {
        return;
    }

    const std::uint32_t step = gorea::frame_step(config_.tick_seconds);
    const float frame_scale = static_cast<float>(step);
    agent.health = 65535;
    agent.health_max = 65535;
    agent.invulnerable = true;
    agent.target_slot = 0xff;

    auto sphere = std::find_if(
        enemies_.begin(), enemies_.end(),
        [id = agent.id](const EnemyState& value) {
            return value.parent_enemy_id == id
                && value.enemy_type == type_id(
                    formats::EnemyType::GoreaSealSphere2);
        });

    auto enter = [&agent](std::uint8_t state) {
        gorea::enter_state(agent, state);
        agent.gorea_state_initialized = true;
    };

    auto objective_target = [&]() {
        return gorea::nearest_player(
            players_, agent.position, std::numeric_limits<float>::max());
    };

    auto update_sphere_visibility = [&]() {
        // Enemy31 calls Enemy32.UpdateVisibility only when its facing-angle
        // gate succeeds.  Enemy32 then keeps that result until the next such
        // call; recomputing LOS from the sphere every frame changes the
        // managed hidden/targetable timing around strafes and teleports.
        if (sphere == enemies_.end()) {
            return;
        }
        const auto target_index = objective_target();
        if (target_index == players_.size()) {
            return;
        }
        const auto& target = players_[target_index];
        const net::Vec3 facing{agent.facing.x, 0.0F, agent.facing.z};
        const net::Vec3 between{
            target.position.x - agent.position.x, 0.0F,
            target.position.z - agent.position.z};
        if (length_squared(facing) <= 1.0F / 128.0F
            || length_squared(between) <= 1.0F / 128.0F) {
            return;
        }
        if (dot(normalized_or(between, {}), normalized_or(facing, {}))
            <= -1.0F) {
            return;
        }
        const float eye_height = (target.flags & net::PlayerState::FlagAltForm)
            != 0 ? 0.5F : 1.0F;
        const auto hit = collision::sweep_sphere(
            room_.collision(), to_collision(sphere->position),
            to_collision(add(target.position, {0.0F, eye_height, 0.0F})),
            0.0F);
        sphere->gorea_visibility = !hit.has_value()
            || hit->fraction >= 0.999F;
    };

    auto choose_trigger = [&](std::uint8_t mode,
                              bool check_collision) -> TriggerRuntime* {
        // This is Enemy31Entity.FindTrigger(mode, checkCollision).  The
        // random candidate is intentionally chosen independently from the
        // nearest/farthest candidates: the cartridge uses mode 0 for the
        // random trigger, mode 1 for the farthest trigger, and mode 2 for
        // the nearest trigger.
        TriggerRuntime* min_trigger = nullptr;
        TriggerRuntime* max_trigger = nullptr;
        TriggerRuntime* random_trigger = nullptr;
        float min_distance = std::numeric_limits<float>::max();
        float max_distance = 0.0F;
        const auto phase = static_cast<std::uint32_t>(
            (agent.gorea_flags & (gorea::Gorea2Phase0
                                  | gorea::Gorea2Phase1)) >> 14);
        for (auto& trigger : trigger_volumes_) {
            if (!trigger.active
                || (trigger.data.parent_message.message != 36
                    && trigger.data.child_message.message != 36)) {
                continue;
            }
            // FindTrigger records the first random candidate before it
            // rejects the current trigger. Preserve that ordering quirk.
            if (phase != 1 && random_trigger == nullptr) {
                random_trigger = &trigger;
            }
            if (trigger.entity_id == agent.gorea_current_trigger_id) {
                continue;
            }
            const auto trigger_center = trigger.data.volume.center();
            if (phase == 1 && trigger_center.y < agent.gorea_volume_height) {
                continue;
            }
            const net::Vec3 center = to_net(trigger.data.volume.center());
            const auto target_index = objective_target();
            const float distance = target_index == players_.size()
                ? distance_squared(center, agent.position)
                : distance_squared(center,
                                  players_[target_index].position);
            const bool random_chance = (rng_.random2(255u) & 1u) != 0;
            if (phase != 1 || trigger_center.y >= agent.gorea_volume_height) {
                if (distance < min_distance) {
                    min_trigger = &trigger;
                    min_distance = distance;
                }
                if (distance > max_distance) {
                    max_trigger = &trigger;
                    max_distance = distance;
                }
                if (random_chance && (phase != 1 || random_trigger == nullptr)) {
                    random_trigger = &trigger;
                }
            }
        }
        TriggerRuntime* chosen = nullptr;
        if (mode == 0) {
            chosen = random_trigger;
        } else if (mode == 1) {
            chosen = max_trigger;
        } else if (mode == 2) {
            chosen = min_trigger;
        }
        if (chosen != nullptr && check_collision) {
            const auto target_index = objective_target();
            if (target_index != players_.size()) {
                const auto hit = collision::sweep_sphere(
                    room_.collision(),
                    to_collision(to_net(chosen->data.volume.center())),
                    to_collision(players_[target_index].position),
                    0.0F);
                if (hit.has_value() && hit->fraction < 0.999F) {
                    return nullptr;
                }
            }
        }
        return chosen;
    };

    auto begin_teleport = [&](TriggerRuntime* trigger) {
        if (trigger != nullptr) {
            agent.gorea_teleport_destination = to_net(
                trigger->data.volume.center());
            agent.gorea_current_trigger_id = trigger->entity_id;
        } else {
            // SearchAndTeleport's no-trigger fallback is a horizontal
            // reflection of Position, not a point reconstructed from the
            // authored sphere radius.
            agent.gorea_teleport_destination = {
                -agent.position.x, agent.position.y, -agent.position.z};
            agent.gorea_current_trigger_id = -1;
        }
        agent.gorea_flags &= ~gorea::Gorea2LaserActive;
        agent.gorea_flags &= ~(gorea::Gorea2Teleporting
                               | gorea::Gorea2Teleporting2);
        agent.gorea_flags |= gorea::Gorea2Teleporting;
        agent.gorea_flags |= gorea::Gorea2IntroDone;
        agent.gorea_field22c = 15u * 2u;
        agent.gorea_targetable = false;
        const net::Vec3 effect_position = sphere != enemies_.end()
            ? sphere->position : agent.position;
        spawn_effect(80, effect_position, agent.facing, agent.id);
    };

    auto start_teleport = [&](std::uint8_t mode, bool check_collision) {
        begin_teleport(choose_trigger(mode, check_collision));
    };

    auto queue_meteor = [&](net::Vec3 position, net::Vec3 direction) {
        EnemyState meteor;
        meteor.id = next_enemy_id_++;
        meteor.parent_enemy_id = agent.id;
        meteor.enemy_type = type_id(formats::EnemyType::GoreaMeteor);
        meteor.position = position;
        meteor.behavior_origin = position;
        meteor.facing = normalized_or(direction, agent.facing);
        meteor.up = agent.up;
        meteor.gorea_activated = true;
        meteor.active = true;
        meteor.spawner_entity_id = -1;
        // Everything else about a meteor is the meteor's own business.
        enemy::module_33_gorea_meteor::EnemyInitialize(meteor);
        enemy::module_33_gorea_meteor::InitializePosition(meteor, position);
        meteor.gorea_meteor_effect_facing = meteor.facing;
        pending_enemy_spawns_.push_back(std::move(meteor));
        if (agent.gorea_meteor_count < 255) {
            ++agent.gorea_meteor_count;
        }
        spawn_effect(174, position, agent.facing, agent.id);
    };

    auto update_teleport = [&]() {
        // EnemyProcess calls Func213D194 after the state callback every frame,
        // including during a fade.  The old native controller returned early
        // and treated the two fade bits as a single timer, which skipped the
        // cartridge's 30-frame settle and 180-frame trigger phase.
        const auto value = static_cast<std::uint32_t>(
            (agent.gorea_flags & (gorea::Gorea2Teleporting
                                  | gorea::Gorea2Teleporting2)) >> 4);
        if (value == 0) {
            gorea::decrement(agent.gorea_field22c, step);
            return false;
        }

        agent.visible = true; // the managed entity fades alpha, not Visible
        agent.gorea_targetable = false;
        if (value == 1) {
            gorea::decrement(agent.gorea_field22c, step);
            if (agent.gorea_field22c == 0) {
                if ((agent.gorea_flags & gorea::Gorea2DamageFlash) != 0) {
                    agent.gorea_flags &= ~gorea::Gorea2DamageFlash;
                    if (sphere != enemies_.end()) {
                        sphere->invulnerable = false;
                    }
                }
                agent.gorea_flags &= ~(gorea::Gorea2Teleporting
                                       | gorea::Gorea2Teleporting2);
                agent.gorea_flags |= gorea::Gorea2Teleporting2;
                agent.gorea_field22c = 15u * 2u;
                agent.gorea_target_position = agent.gorea_teleport_destination;
                const auto target_index = objective_target();
                const net::Vec3 target_position =
                    target_index == players_.size()
                    ? agent.position
                    : players_[target_index].position;
                agent.facing = normalized_or(
                    {target_position.x - agent.position.x, 0.0F,
                     target_position.z - agent.position.z},
                    {0.0F, 0.0F, 1.0F});
                // Func213D3C8 calls CreateTeleportEffect(true): the effect is
                // attached to the seal sphere node and then offset by the
                // selected teleport destination.  The sphere position is
                // already sampled from ChestBall1 above.
                const net::Vec3 effect_position = sphere != enemies_.end()
                    ? add(sphere->position, agent.gorea_teleport_destination)
                    : add(agent.position, agent.gorea_teleport_destination);
                spawn_effect(80, effect_position, agent.facing, agent.id);
            }
        } else if (value == 2) {
            gorea::decrement(agent.gorea_field22c, step);
            if (agent.gorea_field22c == 0) {
                if ((agent.gorea_flags & gorea::Gorea2DamageFlash) != 0) {
                    agent.gorea_flags &= ~gorea::Gorea2DamageFlash;
                    if (sphere != enemies_.end()) {
                        sphere->invulnerable = false;
                    }
                }
                if ((agent.gorea_flags & gorea::Gorea2IntroDone) != 0) {
                    agent.gorea_flags |= gorea::Gorea2Teleporting
                        | gorea::Gorea2Teleporting2;
                } else {
                    agent.gorea_flags &= ~(gorea::Gorea2Teleporting
                                           | gorea::Gorea2Teleporting2);
                }
                agent.gorea_field22c = 90u * 2u;
                // Func213D30C only completes the fade and refreshes the
                // 90-frame hold.  It does not create another teleport effect;
                // the managed entry made at the beginning of the transition
                // remains responsible for the visible tail.
            }
        } else { // value == 3, Func213D204
            gorea::decrement_signed(agent.gorea_field23c, step);
            gorea::decrement(agent.gorea_field22c, step);
            if ((agent.gorea_flags & gorea::Gorea2DamageFlash) != 0) {
                agent.gorea_flags &= ~gorea::Gorea2DamageFlash;
                if (sphere != enemies_.end()) {
                    sphere->invulnerable = false;
                }
            }
            if (agent.gorea_field22c == 0) {
                agent.gorea_flags &= ~(gorea::Gorea2Teleporting
                                       | gorea::Gorea2Teleporting2);
                if ((agent.gorea_flags & gorea::Gorea2DamageSequence) == 0) {
                    agent.gorea_flags |= gorea::Gorea2Teleporting;
                    agent.gorea_field22c = 15u * 2u;
                    const auto target_index = objective_target();
                    agent.gorea_teleport_destination =
                        target_index == players_.size()
                        ? net::Vec3{-agent.position.x, agent.position.y,
                                    -agent.position.z}
                        : gorea2_player_volume_point(
                              agent, players_[target_index].position);
                    const net::Vec3 effect_position = sphere != enemies_.end()
                        ? sphere->position : agent.position;
                    spawn_effect(80, effect_position, agent.facing, agent.id);
                }
                agent.gorea_flags &= ~gorea::Gorea2IntroDone;
            }
        }
        return true;
    };

    const auto update_charge_and_flash_effects = [&]() {
        // State03 owns two EffectEntry objects in the managed controller. The
        // charge entry has ElementExtension enabled, so it must remain alive
        // while the subroutine waits and follow the moving seal sphere every
        // frame. The eye flash follows the animated Head node.
        const net::Vec3 charge_position = sphere != enemies_.end()
            ? sphere->position : agent.position;
        if (agent.gorea_charge_effect_id == 0) {
            agent.gorea_charge_effect_id = spawn_effect(
                210, charge_position, agent.facing, agent.id,
                0.25F, false, true, true);
        } else {
            update_effect_transform(agent.gorea_charge_effect_id,
                                    charge_position, agent.facing);
        }

        formats::Matrix4 head_transform{};
        net::Vec3 flash_position = gorea::local_position(
            agent, {0.0F, 3.5F, 0.8F});
        if (sample_gorea_2_node(agent, "Head", head_transform)) {
            flash_position = {head_transform.m41, head_transform.m42,
                              head_transform.m43};
        }
        if (agent.gorea_flash_effect_id == 0) {
            agent.gorea_flash_effect_id = spawn_effect(
                104, flash_position, agent.facing, agent.id,
                0.25F, false, true, false);
        } else {
            update_effect_transform(agent.gorea_flash_effect_id,
                                    flash_position, agent.facing);
        }
    };

    const auto detach_charge_and_flash_effects = [&]() {
        detach_effect(agent.gorea_charge_effect_id);
        detach_effect(agent.gorea_flash_effect_id);
        agent.gorea_charge_effect_id = 0;
        agent.gorea_flash_effect_id = 0;
    };

    auto update_laser_targeting = [&]() {
        if ((agent.gorea_flags & gorea::Gorea2LaserActive) == 0
            || sphere == enemies_.end()) {
            return;
        }
        const auto target_index = objective_target();
        if (target_index == players_.size()) {
            agent.gorea_flags &= ~gorea::Gorea2LaserActive;
            return;
        }

        // Enemy31.UpdateLaserTargeting tracks the player's authored position,
        // not the aim point used by the meteor launcher.  Keep the two paths
        // separate: the half-unit head offset here changes both the target
        // latch and the cylinder hit window on the cartridge.
        const net::Vec3 target_position = players_[target_index].position;
        const net::Vec3 position_to_player = subtract(
            target_position, agent.gorea_laser_target);
        if (vector_length(position_to_player) <= 0.125F) {
            agent.gorea_laser_target = target_position;
            agent.gorea_flags |= gorea::Gorea2LaserOnTarget;
        } else {
            agent.gorea_laser_target = add(
                agent.gorea_laser_target,
                multiply(position_to_player, frame_scale / 24.0F));
            agent.gorea_flags &= ~gorea::Gorea2LaserOnTarget;
        }

        const net::Vec3 sphere_position = sphere->position;
        // TestFlags.None is the managed query used by UpdateLaserTargeting;
        // passing an ignore-player mask here incorrectly lets the beam pass
        // through any collision polygon carrying the player bit.
        const auto hit = collision::sweep_sphere(
            room_.collision(), to_collision(sphere_position),
            to_collision(agent.gorea_laser_target), 0.0F);
        if (hit.has_value() && hit->fraction < 0.999F) {
            agent.gorea_flags |= gorea::Gorea2LaserBlocked;
            agent.gorea_laser_target = {
                hit->contact.x, hit->contact.y, hit->contact.z};
            agent.gorea_laser_normal = {
                hit->normal.x, hit->normal.y, hit->normal.z};
        } else {
            agent.gorea_flags &= ~gorea::Gorea2LaserBlocked;
            const net::Vec3 laser_vector = subtract(
                agent.gorea_laser_target, sphere_position);
            if (length_squared(laser_vector)
                > std::numeric_limits<float>::epsilon()) {
                agent.gorea_laser_target = add(
                    agent.gorea_laser_target,
                    multiply(normalized_or(laser_vector, {}),
                             frame_scale / 24.0F));
            }
        }
    };

    auto check_laser_hit = [&]() {
        if ((agent.gorea_flags & gorea::Gorea2LaserActive) == 0
            || sphere == enemies_.end()) {
            return;
        }
        const auto target_index = objective_target();
        if (target_index == players_.size()) {
            return;
        }
        auto& target = players_[target_index];
        const net::Vec3 sphere_position = sphere->position;
        bool laser_hit = (agent.gorea_flags
                          & gorea::Gorea2LaserOnTarget) != 0;
        if (!laser_hit) {
            laser_hit = vector_length(subtract(agent.gorea_laser_target,
                                               sphere_position)) < 0.5F;
            if (!laser_hit) {
                // CheckCylinderOverlapVolume is the managed player's volume
                // query. PlayerState intentionally has no renderer-side
                // collision volume, so use its center against the same
                // authored 0.5-unit beam radius as the closest native
                // representation of that query.
                laser_hit = point_segment_distance_squared(
                    target.position, sphere_position,
                    agent.gorea_laser_target) <= 0.5F * 0.5F;
            }
        }
        if (!laser_hit || agent.gorea_laser_damage_timer != 0) {
            return;
        }

        net::Vec3 direction = remove_projection(
            subtract(agent.gorea_laser_target, sphere_position),
            {0.0F, 1.0F, 0.0F});
        if (length_squared(direction) > 1.0F / 128.0F) {
            direction = multiply(normalized_or(direction, {}), 1.0F / 30.0F);
            if ((target.flags & net::PlayerState::FlagAltForm) != 0) {
                target.speed = add(target.speed, multiply(direction, 0.4F));
            } else {
                target.speed = add(target.speed,
                                   {direction.x, 0.0F, direction.z});
                if (direction.y <= 0.0F) {
                    target.speed.y += direction.y;
                } else {
                    target.speed.y = std::min(target.speed.y + direction.y,
                                              0.25F);
                }
            }
        }
        apply_enemy_contact_damage(agent, target, 55);
        agent.gorea_laser_damage_timer = 30u;
    };

    auto run_behavior = [&](std::uint8_t behavior,
                            std::uint8_t state) -> bool {
        const auto target_index = objective_target();
        const auto phase = static_cast<std::uint32_t>(
            (agent.gorea_flags & (gorea::Gorea2Phase0
                                  | gorea::Gorea2Phase1)) >> 14);
        const auto teleport_value = static_cast<std::uint32_t>(
            (agent.gorea_flags & (gorea::Gorea2Teleporting
                                  | gorea::Gorea2Teleporting2)) >> 4);
        const auto omega_active = [&]() {
            return target_index != players_.size()
                && metadata::beam_type_from_native_weapon_slot(
                       players_[target_index].current_weapon) == 8;
        };
        const auto omega_sequence = [&]() {
            return phase != 0
                && (agent.gorea_flags & gorea::Gorea2OmegaUsed) != 0
                && omega_active();
        };

        switch (behavior) {
        case 0: // Behavior00
        case 1: // Behavior01
            return true;
        case 2: // Behavior02
            if ((agent.gorea_flags & gorea::Gorea2Death) != 0) {
                const net::Vec3 effect_position = sphere != enemies_.end()
                    ? sphere->position : agent.position;
                spawn_effect(72, effect_position, agent.facing, agent.id);
                return true;
            }
            return false;
        case 3: // Behavior03
            return teleport_value == 0 || teleport_value == 3;
        case 4: // Behavior04
            if ((agent.gorea_flags & gorea::Gorea2HitFlash) != 0) {
                agent.gorea_return_state = state;
                return true;
            }
            return false;
        case 5: { // Behavior05
            const std::uint32_t damage = sphere == enemies_.end()
                ? 0 : sphere->gorea_damage;
            const std::uint32_t level = damage > 720u ? 6u
                : damage > 600u ? 5u
                : damage > 480u ? 4u
                : damage > 360u ? 3u
                : damage > 240u ? 2u
                : damage > 120u ? 1u : 0u;
            if (level == 0
                || (agent.gorea_light_mask & (1u << (level - 1))) == 0) {
                return false;
            }
            agent.gorea_light_mask = static_cast<std::uint8_t>(
                agent.gorea_light_mask
                & ~(2u * (1u << (level - 1)) - 1u));
            agent.gorea_field232 = 0;
            agent.gorea_return_state = state;
            return true;
        }
        case 6: { // Behavior06
            if (agent.gorea_field232 == 0) {
                return false;
            }
            gorea::decrement(agent.gorea_field232, step);
            if (agent.gorea_field232 != 0) {
                return false;
            }
            std::uint32_t index = 0;
            while (index < 32u
                   && (agent.gorea_light_mask & (1u << index)) == 0) {
                ++index;
            }
            index = std::min<std::uint32_t>(index, 6u);
            if (index == 0 || sphere == enemies_.end()) {
                return false;
            }
            agent.gorea_light_mask = static_cast<std::uint8_t>(
                agent.gorea_light_mask | (1u << (index - 1)));
            agent.gorea_field232 = 0;
            sphere->gorea_damage = 120u * (index - 1u);
            agent.gorea_phase = sphere->gorea_damage <= 210u ? 0
                : sphere->gorea_damage <= 503u ? 1 : 2;
            agent.gorea_flags &= ~(gorea::Gorea2Phase0
                                   | gorea::Gorea2Phase1);
            if (agent.gorea_phase == 1) {
                agent.gorea_flags |= gorea::Gorea2Phase0;
            } else if (agent.gorea_phase == 2) {
                agent.gorea_flags |= gorea::Gorea2Phase1;
            }
            gorea::set_animation(agent, 7, 60, true);
            return true;
        }
        case 7: // Behavior07
            if (agent.gorea_model_animation == 0) {
                return agent.gorea_animation_frame >= 26;
            }
            if (agent.gorea_model_animation != 4) {
                gorea::set_animation(agent, 4, 60);
            } else if (gorea::animation_ended(agent)) {
                gorea::set_animation(agent, 0, 60);
            }
            return false;
        case 8: // Behavior08
            return sphere != enemies_.end() && !sphere->gorea_visibility;
        case 9: { // Behavior09
            if (omega_sequence()) {
                return false;
            }
            if (sphere != enemies_.end() && sphere->gorea_visibility) {
                agent.gorea_field234 = 22u * 2u;
                return false;
            }
            gorea::decrement(agent.gorea_field234, step);
            if (agent.gorea_field234 != 0
                || (teleport_value != 0 && teleport_value != 3)) {
                return false;
            }
            auto* trigger = choose_trigger(2, true);
            if (trigger == nullptr) {
                return false;
            }
            begin_teleport(trigger);
            agent.gorea_field234 = state;
            return true;
        }
        case 10: // Behavior10
            gorea::decrement(agent.gorea_field230, step);
            if (agent.gorea_field230 != 0) {
                return false;
            }
            agent.gorea_flags &= ~gorea::Gorea2LaserActive;
            return true;
        case 11: // Behavior11
            return sphere != enemies_.end() && sphere->gorea_visibility;
        case 12: // Behavior12
            gorea::decrement(agent.gorea_field230, step);
            if (agent.gorea_field230 != 0 || target_index == players_.size()) {
                return false;
            }
            {
                const float distance = std::sqrt(distance_squared(
                    players_[target_index].position, agent.position));
                const bool alt_form = (players_[target_index].flags
                                       & net::PlayerState::FlagAltForm) != 0;
                return distance < (alt_form ? 130.0F : 50.0F);
            }
        case 13: // Behavior13
            if (!omega_sequence() || teleport_value == 1 || teleport_value == 2) {
                return false;
            }
            if (sphere != enemies_.end()) {
                sphere->invulnerable = true;
            }
            gorea::set_animation(agent, 10, 60);
            spawn_effect(225, sphere != enemies_.end()
                                   ? sphere->position : agent.position,
                         agent.facing, agent.id);
            agent.gorea_flags |= gorea::Gorea2DamageSequence;
            return true;
        case 14: // Behavior14
            if (sphere == enemies_.end() || !sphere->gorea_visibility
                || phase == 0) {
                return false;
            }
            gorea::decrement(agent.gorea_field22e, step);
            if (agent.gorea_field22e != 0 || agent.gorea_meteor_count >= 2) {
                return false;
            }
            agent.gorea_field22e = 120u * 2u;
            agent.gorea_flags &= ~gorea::Gorea2MeteorLeft;
            if (rng_.random2(2u) == 1u) {
                agent.gorea_flags |= gorea::Gorea2MeteorLeft;
            }
            agent.gorea_return_state = state;
            gorea::set_animation(agent,
                                 (agent.gorea_flags & gorea::Gorea2MeteorLeft)
                                     != 0 ? 2 : 3,
                                 60);
            return true;
        case 15: // Behavior15
            if (sphere == enemies_.end() || !sphere->gorea_visibility) {
                return false;
            }
            return teleport_value != 1 && teleport_value != 2;
        case 16: // Behavior16
            if (!omega_sequence()) {
                agent.gorea_flags &= ~gorea::Gorea2DamageSequence;
                return true;
            }
            return false;
        case 17: { // Behavior17
            if (phase != 0) {
                return false;
            }
            if (sphere != enemies_.end() && sphere->gorea_visibility) {
                agent.gorea_field234 = 22u * 2u;
                return false;
            }
            gorea::decrement(agent.gorea_field234, step);
            if (agent.gorea_field234 != 0
                || (teleport_value != 0 && teleport_value != 3)) {
                return false;
            }
            auto* trigger = choose_trigger(2, false);
            if (trigger == nullptr) {
                return false;
            }
            begin_teleport(trigger);
            agent.gorea_field234 = state;
            return true;
        }
        case 255: // BehaviorXX
            return gorea::animation_ended(agent);
        default:
            return false;
        }
    };

    auto call_subroutine = [&](std::uint8_t state) {
        auto dispatch = [&](std::initializer_list<std::pair<std::uint8_t,
                                                              std::uint8_t>> entries) {
            for (const auto& [behavior, next_state] : entries) {
                if (run_behavior(behavior, state)) {
                    return std::pair{true, next_state};
                }
            }
            return std::pair{false, static_cast<std::uint8_t>(0)};
        };
        switch (state) {
        case 0: return dispatch({{255, 1}}); // BehaviorXX
        case 1: return dispatch({{2, 17}, {4, 14}, {5, 15}, {6, 16},
                                  {12, 2}, {13, 9}, {14, 6}});
        case 2: return dispatch({{2, 17}, {4, 14}, {5, 15}, {6, 16},
                                  {11, 3}, {13, 9}, {14, 6}, {17, 7}});
        case 3: return dispatch({{2, 17}, {4, 14}, {5, 15}, {6, 16},
                                  {13, 9}, {14, 6}, {7, 4}});
        case 4: return dispatch({{2, 17}, {4, 14}, {5, 15}, {6, 16},
                                  {13, 9}, {9, 7}, {10, 5}});
        case 5: return dispatch({{0, 1}});
        case 6: return dispatch({{2, 17}, {255, 18}});
        case 7: return dispatch({{2, 17}, {3, 18}});
        case 8: return dispatch({{2, 17}, {4, 14}, {5, 15}, {6, 16},
                                  {15, 11}, {14, 6}, {16, 1}});
        case 9: return dispatch({{255, 8}});
        case 10: return dispatch({{2, 17}, {4, 14}, {5, 15}, {6, 16},
                                  {11, 11}, {8, 8}});
        case 11: return dispatch({{2, 17}, {4, 14}, {5, 15}, {6, 16},
                                  {7, 12}, {8, 8}});
        case 12: return dispatch({{2, 17}, {4, 14}, {5, 15}, {6, 16},
                                  {9, 7}, {10, 13}});
        case 13: return dispatch({{0, 8}});
        case 14: return dispatch({{2, 17}, {255, 18}});
        case 15: return dispatch({{2, 17}, {0, 18}});
        case 16: return dispatch({{255, 18}});
        case 17: return dispatch({{0, 17}});
        case 18: return dispatch({{1, 18}});
        default: return dispatch({});
        }
    };

    if (!agent.gorea_state_initialized) {
        agent.gorea_state_initialized = true;
        // Enemy31.InitializeTrigger first checks the authored S12 sphere. If
        // the body was placed outside it, an enclosing Gorea2Trigger starts
        // in the two-bit fade state and keeps the current trigger identity.
        const bool in_spawn_volume = agent.gorea_volume_radius > 0.0F
            && distance_squared(agent.position, agent.gorea_anchor)
                <= agent.gorea_volume_radius * agent.gorea_volume_radius;
        if (!in_spawn_volume) {
            for (auto& trigger : trigger_volumes_) {
                if (!trigger.active
                    || (trigger.data.parent_message.message != 36
                        && trigger.data.child_message.message != 36)
                    || !trigger.data.volume.contains(
                        to_volume_point(agent.position))) {
                    continue;
                }
                agent.gorea_current_trigger_id = trigger.entity_id;
                agent.gorea_flags |= gorea::Gorea2Teleporting
                    | gorea::Gorea2Teleporting2;
                agent.gorea_field22c = 90u * 2u;
                break;
            }
        }
        if (length_squared(agent.gorea_target_position) <=
                std::numeric_limits<float>::epsilon()
            && length_squared(agent.position) >
                std::numeric_limits<float>::epsilon()) {
            agent.gorea_target_position = agent.position;
        }
        if (in_spawn_volume
            && (agent.gorea_flags & (gorea::Gorea2Teleporting
                                     | gorea::Gorea2Teleporting2
                                     | gorea::Gorea2DamageSequence)) == 0) {
            // InitTrigger calls Func213D80C immediately when the spawner's
            // S12 volume contains the body. The per-frame centre update below
            // runs again during EnemyProcess, just as the managed constructor
            // call is followed by its first process call.
            const auto target_index = objective_target();
            if (target_index != players_.size()) {
                const net::Vec3 goal = gorea2_player_volume_point(
                    agent, players_[target_index].position);
                const net::Vec3 between = subtract(
                    agent.gorea_target_position, goal);
                if (length_squared(between) > 1.0F / 6.0F) {
                    net::Vec3 tangent = remove_projection(
                        subtract(goal, agent.gorea_target_position),
                        subtract(agent.gorea_target_position,
                                 agent.gorea_anchor));
                    if (length_squared(tangent) < 1.0F / 128.0F) {
                        tangent = gorea2_fallback_tangent(
                            agent.gorea_target_position, agent.gorea_anchor);
                    }
                    agent.gorea_target_position = add(
                        agent.gorea_target_position,
                        multiply(normalized_or(tangent, {}), 1.0F / 6.0F));
                } else {
                    agent.gorea_target_position = goal;
                }
                const net::Vec3 radial = normalized_or(
                    subtract(agent.gorea_target_position, agent.gorea_anchor),
                    {});
                agent.gorea_target_position = add(
                    agent.gorea_anchor,
                    multiply(radial, agent.gorea_volume_radius));
            }
        }
    }

    // Enemy31's ModelInstance is advanced by the inherited base process after
    // the state callback, on every other scene frame. Apply the previous
    // even-tick update before this callback observes the cursor.
    if (gorea::should_advance_animation(tick_count_)) {
        static_cast<void>(gorea::advance_animation(agent, step));
    }
    if (sphere != enemies_.end()) {
        sphere->position = gorea::local_position(agent,
                                                 {0.0F, 0.0F, 0.35F});
        sphere->gorea_activated = agent.gorea_state != 0
            && (agent.gorea_flags & gorea::Gorea2Death) == 0;
        sphere->gorea_targetable = sphere->gorea_activated
            && (agent.gorea_flags & (gorea::Gorea2Teleporting
                                     | gorea::Gorea2Teleporting2)) == 0;
        sphere->invulnerable = !sphere->gorea_targetable;
    }

    if (sphere != enemies_.end()) {
        const std::uint32_t damage = sphere->gorea_damage;
        agent.gorea_phase = damage <= 210u ? 0
            : damage <= 503u ? 1 : 2;
        agent.gorea_flags &= ~(gorea::Gorea2Phase0 | gorea::Gorea2Phase1);
        if (agent.gorea_phase == 1) {
            agent.gorea_flags |= gorea::Gorea2Phase0;
        } else if (agent.gorea_phase == 2) {
            agent.gorea_flags |= gorea::Gorea2Phase1;
        }
        if (damage >= 840u) {
            // Enemy32 raises Bit11; leave the transition to Behavior02 so the
            // state machine emits the same death effect before State17.
            agent.gorea_flags |= gorea::Gorea2Death;
        }
    }

    const auto facing_target = objective_target();
    if (facing_target != players_.size()) {
        const net::Vec3 facing{agent.facing.x, 0.0F, agent.facing.z};
        const net::Vec3 between{
            players_[facing_target].position.x - agent.position.x, 0.0F,
            players_[facing_target].position.z - agent.position.z};
        if (length_squared(facing) > 1.0F / 128.0F
            && length_squared(between) > 1.0F / 128.0F
            && dot(normalized_or(between, {}), normalized_or(facing, {}))
                > -1.0F) {
            update_sphere_visibility();
        }
    }

    // Enemy31.Process runs the laser, laser-hit, and player-contact checks in
    // that order, then consumes _field236, but only while Bit11 is clear.
    // Keep the guard and order visible instead of folding these into the
    // state switch: State17 must not continue the attack on its death frame.
    const bool death_flag_set = (agent.gorea_flags
                                 & gorea::Gorea2Death) != 0;
    if (!death_flag_set) {
        update_laser_targeting();
        check_laser_hit();
    }
    {
        const std::size_t target_index = objective_target();
        if (!death_flag_set && target_index != players_.size()) {
            auto& target = players_[target_index];
            agent.target_slot = target.slot_index;
            // Enemy31's _hurtVolumeInit is a radius-1 sphere.  HitPlayers is
            // populated from that volume before EnemyProcess runs, so the
            // native contact gate must use the same combined player-sphere
            // radius instead of the old distance-2 approximation.  The
            // managed CheckPlayerCollision does not turn Gorea here; its
            // facing changes only in the explicit teleport/initialisation
            // paths above.
            if ((agent.gorea_flags & gorea::Gorea2LaserActive) == 0
                && distance_squared(target.position, agent.position)
                    <= (agent.body_radius + config_.body_radius)
                        * (agent.body_radius + config_.body_radius)
                && agent.gorea_attack_timer == 0) {
                // Enemy31Entity.CheckPlayerCollision applies the raw vector
                // between the player and Gorea (not a normalized knockback)
                // before TakeDamage.  Keep this separate from the generic
                // contact helper: the latter records damage/kill metadata,
                // while this line is the gameplay-visible recoil.
                target.speed = add(
                    target.speed,
                    multiply(subtract(target.position, agent.position), 1.0F / 8.0F));
                apply_enemy_contact_damage(agent, target, 10);
                agent.gorea_attack_timer = 30u * 2u;
            }
        }
        gorea::decrement(agent.gorea_attack_timer, step);

        gorea::decrement(agent.gorea_field236, step);
        gorea::decrement(agent.gorea_laser_damage_timer, step);

        switch (agent.gorea_state) {
        case 0: {
            if (agent.gorea_model_animation != 9) {
                gorea::set_animation(agent, 9, 60);
            }
            const auto result = call_subroutine(0);
            if (result.first) {
                gorea::set_animation(agent, 7, 60, true);
                agent.gorea_flags |= gorea::Gorea2IntroDone
                    | gorea::Gorea2Subroutine;
                if (sphere != enemies_.end()) {
                    sphere->gorea_activated = true;
                    sphere->invulnerable = false;
                    sphere->gorea_targetable = true;
                }
                enter(result.second);
            }
            break;
        }
        case 1:
        case 2:
        case 5: {
            const auto state = agent.gorea_state;
            if (state == 1
                && (agent.gorea_flags & gorea::Gorea2Subroutine) != 0) {
                agent.gorea_flags &= ~gorea::Gorea2Subroutine;
                const auto phase = static_cast<std::uint32_t>(
                    (agent.gorea_flags & (gorea::Gorea2Phase0
                                          | gorea::Gorea2Phase1)) >> 14);
                agent.gorea_field230 = phase == 0 ? 90u * 2u : 0u;
            }
            const auto result = call_subroutine(state);
            if (result.first) {
                if (state == 1 || state == 2) {
                    agent.gorea_flags |= gorea::Gorea2Subroutine;
                }
                enter(result.second);
            }
            break;
        }
        case 3: {
            update_charge_and_flash_effects();
            const auto result = call_subroutine(3);
            if (result.first) {
                detach_charge_and_flash_effects();
                agent.gorea_flags |= gorea::Gorea2Subroutine;
                enter(result.second);
            }
            break;
        }
        case 11: {
            const auto state = agent.gorea_state;
            const auto result = call_subroutine(state);
            if (result.first) {
                agent.gorea_flags |= gorea::Gorea2Subroutine;
                enter(result.second);
            }
            break;
        }
        case 4:
        case 12: {
            const auto state = agent.gorea_state;
            if ((agent.gorea_flags & gorea::Gorea2Subroutine) != 0) {
                agent.gorea_flags &= ~gorea::Gorea2Subroutine;
                agent.gorea_flags |= gorea::Gorea2LaserActive;
                agent.gorea_field230 = 45u * 2u;
                agent.gorea_laser_damage_timer = 0;
                agent.gorea_laser_target = sphere != enemies_.end()
                    ? add(sphere->position, {0.0F, -25.0F, 0.0F})
                    : agent.position;
                update_laser_targeting();
            }
            if ((agent.gorea_flags & gorea::Gorea2LaserBlocked) != 0) {
                const net::Vec3 effect_position = agent.gorea_laser_target;
                const net::Vec3 effect_direction = normalized_or(
                    agent.gorea_laser_normal, agent.facing);
                if (agent.gorea_collision_effect_id == 0) {
                    agent.gorea_collision_effect_id = spawn_effect(
                        224, effect_position, effect_direction, agent.id,
                        0.25F, false, true, true);
                } else {
                    update_effect_transform(agent.gorea_collision_effect_id,
                                            effect_position, effect_direction);
                }
            } else if (agent.gorea_collision_effect_id != 0) {
                detach_effect(agent.gorea_collision_effect_id);
                agent.gorea_collision_effect_id = 0;
            }
            if (agent.gorea_model_animation == 0
                && gorea::animation_ended(agent)) {
                gorea::set_animation(agent, 7, 60, true);
            }
            const auto result = call_subroutine(state);
            if (result.first) {
                agent.gorea_flags &= ~gorea::Gorea2LaserActive;
                detach_effect(agent.gorea_collision_effect_id);
                agent.gorea_collision_effect_id = 0;
                enter(result.second);
            }
            break;
        }
        case 6: {
            if (agent.gorea_animation_frame >= 27
                && agent.gorea_meteor_timer == 0
                && agent.gorea_meteor_count < 2) {
                const bool left_spike =
                    (agent.gorea_flags & gorea::Gorea2MeteorLeft) != 0;
                const char* spike_name = left_spike
                    ? "L_BodySpike" : "R_BodySpike";
                formats::Matrix4 spike_transform{};
                net::Vec3 origin = gorea::local_position(
                    agent, {left_spike ? -0.5F : 0.5F, 0.0F, 0.25F});
                if (sample_gorea_2_node(agent, spike_name, spike_transform)) {
                    // Enemy31.ShootMeteor creates the projectile at the
                    // animated body-spike node, not at a fixed side offset.
                    // The offset remains only for headless sessions without
                    // the renderer-owned Gorea2 model.
                    origin = {spike_transform.m41, spike_transform.m42,
                              spike_transform.m43};
                }
                const auto target_index = objective_target();
                const net::Vec3 direction = target_index == players_.size()
                    ? agent.facing
                    : gorea::aim_at(origin, add(
                        players_[target_index].position, {0.0F, 0.5F, 0.0F}),
                        agent.facing);
                queue_meteor(origin, direction);
                agent.gorea_meteor_timer = 1;
            }
            const auto result = call_subroutine(6);
            if (result.first) {
                agent.gorea_meteor_timer = 0;
                gorea::set_animation(agent, 7, 60, true);
                agent.gorea_flags |= gorea::Gorea2Subroutine;
                enter(result.second);
            }
            break;
        }
        case 7: {
            const auto result = call_subroutine(7);
            if (result.first) {
                agent.gorea_field234 = 22u * 2u;
                agent.gorea_flags |= gorea::Gorea2Subroutine;
                enter(result.second);
            }
            break;
        }
        case 8: {
            if ((agent.gorea_flags & gorea::Gorea2Subroutine) != 0) {
                agent.gorea_flags &= ~gorea::Gorea2Subroutine;
                const auto phase = static_cast<std::uint32_t>(
                    (agent.gorea_flags & (gorea::Gorea2Phase0
                                          | gorea::Gorea2Phase1)) >> 14);
                agent.gorea_field230 = phase == 0 ? 90u * 2u : 0u;
            }
            if (agent.gorea_field236 == 0) {
                agent.gorea_field236 = (rng_.random2(300u) + 300u) * 2u;
                start_teleport(1, false);
            }
            const auto result = call_subroutine(8);
            if (result.first) {
                agent.gorea_flags |= gorea::Gorea2Subroutine
                    | gorea::Gorea2Phase0 | gorea::Gorea2Phase1;
                agent.gorea_field22c = agent.gorea_field236;
                if (agent.gorea_field23c == 0) {
                    agent.gorea_field23c = std::max(agent.gorea_field22c,
                                                     15u * 2u);
                }
                enter(result.second);
            }
            break;
        }
        case 9: {
            const auto result = call_subroutine(9);
            if (result.first) {
                start_teleport(1, false);
                agent.gorea_field236 = (rng_.random2(300u) + 300u) * 2u;
                agent.gorea_flags |= gorea::Gorea2DamageFlash
                    | gorea::Gorea2Subroutine;
                gorea::set_animation(agent, 7, 60, true);
                enter(result.second);
            }
            break;
        }
        case 10: {
            const auto result = call_subroutine(10);
            if (result.first) {
                enter(result.second);
            }
            break;
        }
        case 13: {
            const auto result = call_subroutine(13);
            if (result.first) {
                agent.gorea_flags |= gorea::Gorea2Subroutine;
                agent.gorea_field236 = 0;
                enter(result.second);
            }
            break;
        }
        case 14: {
            if ((agent.gorea_flags & gorea::Gorea2Subroutine) != 0) {
                agent.gorea_flags &= ~(gorea::Gorea2Subroutine
                                       | gorea::Gorea2HitFlash);
                gorea::set_animation(agent, 8, 60);
            }
            const auto result = call_subroutine(14);
            if (result.first) {
                agent.gorea_flags |= gorea::Gorea2Subroutine;
                start_teleport(0, false);
                gorea::set_animation(agent, 7, 60, true);
                enter(result.second);
            }
            break;
        }
        case 15: {
            const auto result = call_subroutine(15);
            if (result.first) {
                agent.gorea_flags |= gorea::Gorea2Subroutine;
                gorea::set_animation(agent, 7, 60, true);
                enter(result.second);
            }
            break;
        }
        case 16: {
            const auto result = call_subroutine(16);
            if (result.first) {
                enter(result.second);
            }
            break;
        }
        case 17: {
            if ((agent.gorea_flags & gorea::Gorea2Finish) == 0) {
                agent.gorea_flags |= gorea::Gorea2Finish;
                agent.gorea_state_timer = 60u * 2u;
                agent.health = 1;
                agent.invulnerable = true;
                agent.gorea_targetable = false;
                if (sphere != enemies_.end()) {
                    sphere->gorea_activated = false;
                    sphere->gorea_targetable = false;
                    sphere->gorea_beam_collidable = false;
                    sphere->invulnerable = true;
                }
            }
            if (run_behavior(3, 17)) {
                gorea::set_animation(agent, 7, 60, true);
            }
            break;
        }
        case 18: {
            const auto result = call_subroutine(18);
            if (result.first) {
                // State18 calls State02 through the current _subId (18),
                // then overwrites the metadata-selected state with _field243.
                // Preserve the Bit17 side effect of that State02 wrapper.
                agent.gorea_flags |= gorea::Gorea2Subroutine;
                enter(agent.gorea_return_state == 0
                          ? static_cast<std::uint8_t>(1)
                          : agent.gorea_return_state);
            }
            break;
        }
        default:
            enter(1);
            break;
        }
    }

    // Enemy31Process updates the movement centre and teleport fade after the
    // state callback, using the Bit11 snapshot from the start of the frame.
    // The final hover transform is unconditional: the managed controller
    // continues to bob the body during the death/fade states.
    if (!death_flag_set) {
        if ((agent.gorea_flags & (gorea::Gorea2Teleporting
                                  | gorea::Gorea2Teleporting2
                                  | gorea::Gorea2DamageSequence)) == 0) {
            const auto center_target_index = objective_target();
            if (center_target_index != players_.size()) {
                const net::Vec3 goal = gorea2_player_volume_point(
                    agent, players_[center_target_index].position);
                const net::Vec3 between = subtract(agent.gorea_target_position,
                                                   goal);
                if (length_squared(between) > 1.0F / 6.0F) {
                    net::Vec3 tangent = remove_projection(
                        subtract(goal, agent.gorea_target_position),
                        subtract(agent.gorea_target_position,
                                 agent.gorea_anchor));
                    if (length_squared(tangent) < 1.0F / 128.0F) {
                        tangent = gorea2_fallback_tangent(
                            agent.gorea_target_position, agent.gorea_anchor);
                    }
                    agent.gorea_target_position = add(
                        agent.gorea_target_position,
                        multiply(normalized_or(tangent, {}),
                                 frame_scale / 6.0F));
                } else {
                    agent.gorea_target_position = goal;
                }
                const net::Vec3 radial = normalized_or(
                    subtract(agent.gorea_target_position, agent.gorea_anchor),
                    {});
                agent.gorea_target_position = add(
                    agent.gorea_anchor,
                    multiply(radial, agent.gorea_volume_radius));

                net::Vec3 to_player = subtract(
                    agent.position, players_[center_target_index].position);
                if (length_squared(to_player) > 1.0F / 128.0F) {
                    to_player = normalized_or(to_player, {});
                }
                const net::Vec3 horizontal{to_player.x, 0.0F, to_player.z};
                if (length_squared(horizontal) > 0.0F) {
                    agent.gorea_target_facing = normalized_or(
                        horizontal, agent.gorea_target_facing);
                }
            }
        }

        static_cast<void>(update_teleport());
    }

    {
        agent.gorea_field23e += static_cast<std::int32_t>(4u * step);
        while (agent.gorea_field23e >= 360) {
            agent.gorea_field23e -= 360;
            agent.gorea_flags ^= gorea::Gorea2HoverFlip;
        }
        agent.gorea_field240 += static_cast<std::int32_t>(2u * step);
        while (agent.gorea_field240 >= 360) {
            agent.gorea_field240 -= 360;
        }
        constexpr float Pi = 3.14159265358979323846F;
        const float angle = static_cast<float>(agent.gorea_field23e) * Pi
            / 180.0F;
        const net::Vec3 facing = normalized_or(agent.facing,
                                               {0.0F, 0.0F, 1.0F});
        const net::Vec3 up = normalized_or(agent.up,
                                           {0.0F, 1.0F, 0.0F});
        const float vertical = std::sin(angle) * 1.5F;
        float horizontal = static_cast<float>(agent.gorea_field23e)
            / 360.0F * 3.0F;
        if ((agent.gorea_flags & gorea::Gorea2HoverFlip) != 0) {
            horizontal = 3.0F - horizontal;
        }
        const float random_lateral = (static_cast<float>(rng_.random2(227u))
                                      - 113.0F) / 4096.0F;
        const net::Vec3 rotated_lateral = rotate_about_axis(
            cross(up, facing), facing, static_cast<float>(agent.gorea_field240));
        agent.position = add(
            agent.gorea_target_position,
            multiply(rotated_lateral,
                     horizontal - 1.5F - random_lateral));
        const float random_vertical = (static_cast<float>(rng_.random2(227u))
                                       - 113.0F) / 4096.0F;
        const net::Vec3 rotated_up = rotate_about_axis(
            up, facing, static_cast<float>(agent.gorea_field240));
        agent.position = add(agent.position,
                             multiply(rotated_up, vertical - random_vertical));
    }
}

} // namespace fruityprime::gameplay

static_assert(fruityprime::enemy::module_31_gorea_2::kModule.managed_class.size() != 0);

namespace fruityprime::enemy::module_31_gorea_2 {

net::Vec3 Func21418EC(const net::Vec3 vec1, const net::Vec3 vec2) noexcept {
    const auto cross = [](const net::Vec3 a, const net::Vec3 b) {
        return net::Vec3{a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z,
                         a.x * b.y - a.y * b.x};
    };
    const auto too_short = [](const net::Vec3 v) {
        return v.x * v.x + v.y * v.y + v.z * v.z <= 1.0F / 128.0F;
    };
    net::Vec3 result = cross(vec1, vec2);
    if (too_short(result)) {
        result = cross(vec1, {1.0F, 0.0F, 0.0F});
        if (too_short(result)) {
            result = cross(vec1, {0.0F, 1.0F, 0.0F});
            if (too_short(result)) {
                result = cross(vec1, {0.0F, 0.0F, 1.0F});
                if (too_short(result)) {
                    // The cartridge returns whatever was last in the
                    // register here.  Zero is the one answer that cannot
                    // be mistaken for a direction.
                    return {};
                }
            }
        }
    }
    const float length = std::sqrt(result.x * result.x + result.y * result.y
                                   + result.z * result.z);
    if (length > 0.0F) {
        result = {result.x / length, result.y / length, result.z / length};
    }
    return cross(result, vec1);
}

} // namespace fruityprime::enemy::module_31_gorea_2
