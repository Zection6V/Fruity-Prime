#include "enemy_decode_common.hpp"
// Native port of src/MphRead/Entities/Enemies/37_Quadtroid.cs.
// This member definition lives in its enemy module; Session only dispatches it.
#include "37_Quadtroid.hpp"
#include "enemy_common.hpp"

namespace fruityprime::gameplay {

void Session::update_quadtroid(EnemyState& agent) {
    const auto& profile = agent.quadtroid;
    if (!profile.supported) {
        return;
    }
    const float seconds = config_.tick_seconds;
    constexpr std::uint8_t Bit0 = 0x01;
    constexpr std::uint8_t Bit1 = 0x02;
    constexpr std::uint8_t Bit2 = 0x04;
    constexpr std::uint8_t Bit6 = 0x40;
    constexpr std::uint8_t Bit7 = 0x80;
    constexpr std::uint8_t StateIdle = 1;
    constexpr std::uint8_t StateTurn = 4;
    constexpr std::uint8_t StateEngage = 7;
    constexpr std::uint8_t StateApproach = 8;
    constexpr std::uint8_t StateGrabWindup = 9;
    constexpr std::uint8_t StateAttach = 10;
    constexpr std::uint8_t StateBipedGrab = 11;
    constexpr std::uint8_t StateMorphTransition = 12;
    constexpr std::uint8_t StateAltGrab = 13;
    constexpr std::uint8_t StateRelease = 14;
    constexpr std::uint8_t StateStunned = 17;
    constexpr std::uint8_t StateBombRelease = 18;

    const auto decrement = [](std::uint32_t& value) noexcept {
        if (value > 0) {
            --value;
        }
    };
    const auto set_state = [&agent](std::uint8_t state,
                                    std::uint32_t timer = 0) noexcept {
        agent.state = state;
        agent.quadtroid_state_timer = timer;
    };
    const auto detach = [&agent]() noexcept {
        agent.target_slot = 0xff;
        agent.quadtroid_attached = false;
        agent.quadtroid_grab_timer = 0;
        agent.quadtroid_flags &= static_cast<std::uint8_t>(
            ~(Bit0 | Bit7));
        agent.invulnerable = false;
    };

    if (!agent.quadtroid_initialized) {
        agent.quadtroid_initialized = true;
        agent.state = StateIdle;
        agent.quadtroid_previous_health = agent.health;
        agent.quadtroid_right = normalized_or(
            cross(agent.facing, agent.up), {1.0F, 0.0F, 0.0F});
        agent.quadtroid_turn_direction = agent.facing;
        agent.quadtroid_idle_timer =
            (45u + rng_.random2(60u)) * 2u;
        agent.quadtroid_attack_timer =
            (90u + rng_.random2(60u)) * 2u;
    }

    // EnemyTakeDamage latches the same one-frame source information that the
    // managed Enemy37Entity consumes after its base health update. A heavy
    // hit interrupts every non-immune state and waits through the damage
    // animation; small hits leave the current traversal intact.
    const bool hit_by_beam = agent.quadtroid_hit_by_beam;
    const bool hit_by_bomb = agent.quadtroid_hit_by_bomb;
    const std::uint16_t damage_taken = agent.quadtroid_damage_taken;
    agent.quadtroid_hit_by_beam = false;
    agent.quadtroid_hit_by_bomb = false;
    agent.quadtroid_damage_taken = 0;
    if (hit_by_bomb || (hit_by_beam && damage_taken >= 25)) {
        detach();
        agent.quadtroid_flags |= Bit7;
        agent.velocity = {};
        set_state(hit_by_bomb ? StateBombRelease : StateStunned,
                  hit_by_bomb ? 24u : 18u);
        if (hit_by_bomb) {
            agent.quadtroid_launch_velocity = add(
                multiply(agent.facing, -0.0889F),
                multiply(agent.up, 0.0532F));
        }
        agent.invulnerable = true;
        return;
    }

    decrement(agent.quadtroid_idle_timer);
    decrement(agent.quadtroid_attack_timer);
    agent.attack_timer = std::max(0.0F, agent.attack_timer - seconds);

    const auto find_target = [&]() -> net::PlayerState* {
        if (agent.target_slot != 0xff) {
            const auto current = std::find_if(
                players_.begin(), players_.end(),
                [&agent, &profile](const net::PlayerState& player) {
                    return player.slot_index == agent.target_slot
                        && objective_player(player)
                        && profile.encounter_volume.contains(to_volume_point(
                            player.position));
                });
            if (current != players_.end()) {
                return &*current;
            }
        }

        net::PlayerState* closest = nullptr;
        float closest_distance = std::numeric_limits<float>::max();
        for (auto& player : players_) {
            if (!objective_player(player)
                || !profile.encounter_volume.contains(to_volume_point(
                    player.position))) {
                continue;
            }
            const float squared = distance_squared(player.position,
                                                   agent.position);
            if (squared <= 10.0F * 10.0F && squared < closest_distance) {
                closest_distance = squared;
                closest = &player;
            }
        }
        return closest;
    };

    const auto move_surface = [&](net::Vec3 destination, float speed) {
        const net::Vec3 delta = subtract(destination, agent.position);
        const net::Vec3 direction = normalized_or(delta, agent.facing);
        const net::Vec3 horizontal = normalized_or(
            {direction.x, 0.0F, direction.z}, agent.facing);
        agent.facing = horizontal;
        agent.quadtroid_turn_direction = horizontal;
        agent.velocity = multiply(direction, speed);
        const net::Vec3 next = add(agent.position,
                                   multiply(agent.velocity, seconds));
        const auto hit = collision::sweep_sphere(
            room_.collision(), to_collision(agent.position),
            to_collision(next), agent.body_radius, 0x2000);
        if (hit.has_value()) {
            agent.position = {hit->center.x + hit->normal.x * 0.001F,
                              hit->center.y + hit->normal.y * 0.001F,
                              hit->center.z + hit->normal.z * 0.001F};
            agent.behavior_surface_normal = normalized_or(
                {hit->normal.x, hit->normal.y, hit->normal.z},
                agent.behavior_surface_normal);
            agent.up = agent.behavior_surface_normal;
            agent.quadtroid_flags |= Bit6;
            agent.velocity = {};
        } else {
            agent.position = next;
            agent.quadtroid_flags &= static_cast<std::uint8_t>(~Bit6);
        }
    };

    net::PlayerState* target = find_target();
    if (target == nullptr && !agent.quadtroid_attached) {
        agent.target_slot = 0xff;
        agent.velocity = {};
        agent.quadtroid_grab_timer = 0;
        if (agent.state >= StateApproach) {
            detach();
        }
        set_state(agent.state == StateTurn ? StateTurn : 0);
        if (agent.state == 0 && agent.quadtroid_idle_timer == 0) {
            set_state(StateIdle);
            agent.quadtroid_idle_timer =
                (45u + rng_.random2(60u)) * 2u;
        }
        if (agent.state == StateTurn && agent.quadtroid_state_timer == 0) {
            set_state(StateIdle);
        }
        return;
    }

    if (target == nullptr) {
        // An attached player died or left the match. This is the same cleanup
        // path as Func214D5B8, and avoids leaving a non-null attachment bit in
        // the native state after the player vector changes.
        detach();
        set_state(StateIdle);
        return;
    }

    agent.target_slot = target->slot_index;
    const bool target_is_alt = (target->flags & net::PlayerState::FlagAltForm)
        != 0;
    const net::Vec3 target_position = add(
        target->position, {0.0F, target_is_alt ? 0.0F : 0.5F, 0.0F});
    const float target_distance = std::sqrt(std::max(
        0.0F, distance_squared(target_position, agent.position)));

    // The ordinary contact volume is active while the crawler is traversing
    // the room. The attached states use their own two-damage cadence below.
    if (!agent.quadtroid_attached && target_distance <= 1.25F
        && agent.attack_timer <= 0.0F) {
        apply_enemy_contact_damage(agent, *target, 3);
        agent.attack_timer = 0.5F;
    }

    switch (agent.state) {
    case 0:
    case StateIdle:
        if (agent.quadtroid_idle_timer == 0) {
            agent.quadtroid_turn_direction = normalized_or(
                rotate_about_axis(agent.facing, agent.up,
                                  rng_.random2(2) == 0 ? -10.0F : 10.0F),
                agent.facing);
            agent.quadtroid_flags |= Bit1 | Bit2;
            agent.quadtroid_state_timer = 20u;
            set_state(StateTurn, 20u);
        }
        if (target_distance <= 10.0F
            && profile.encounter_volume.contains(to_volume_point(
                target->position))) {
            agent.quadtroid_flags &= static_cast<std::uint8_t>(
                ~(Bit0 | Bit7));
            set_state(StateEngage, 8u);
        }
        break;

    case StateTurn:
        agent.facing = normalized_or(agent.quadtroid_turn_direction,
                                     agent.facing);
        agent.quadtroid_right = normalized_or(
            cross(agent.facing, agent.up), agent.quadtroid_right);
        decrement(agent.quadtroid_state_timer);
        agent.quadtroid_flags &= static_cast<std::uint8_t>(~(Bit1 | Bit2));
        if (agent.quadtroid_state_timer == 0) {
            set_state(StateIdle);
            agent.quadtroid_idle_timer =
                (45u + rng_.random2(60u)) * 2u;
        }
        break;

    case 2:
    case 3:
    case 5:
    case 6:
    case StateEngage:
        decrement(agent.quadtroid_state_timer);
        if (agent.quadtroid_state_timer == 0) {
            set_state(StateApproach, 0);
            agent.quadtroid_flags &= static_cast<std::uint8_t>(~Bit7);
        }
        break;

    case StateApproach:
        if (target_distance > 0.20F) {
            move_surface(target_position, 3.0F);
            if (target_distance <= 4.0F && (agent.quadtroid_flags & Bit6) != 0) {
                agent.quadtroid_flags |= Bit7;
                agent.velocity = {};
                set_state(StateGrabWindup, 8u);
            }
        } else {
            agent.position = target_position;
            agent.velocity = {};
            agent.quadtroid_flags |= Bit7;
            set_state(StateGrabWindup, 8u);
        }
        break;

    case StateGrabWindup:
        agent.velocity = {};
        agent.invulnerable = true;
        decrement(agent.quadtroid_state_timer);
        if (agent.quadtroid_state_timer == 0) {
            set_state(StateAttach, 4u);
        }
        break;

    case StateAttach:
        agent.invulnerable = true;
        if (target_distance > 0.375F) {
            move_surface(target_position, 1.0F);
            break;
        }
        agent.quadtroid_attached = true;
        agent.quadtroid_grab_timer = target_is_alt ? 30u : 0u;
        agent.position = target_is_alt
            ? add(target->position, {0.0F, -0.25F, 0.0F})
            : add(target->position, {0.0F, 0.5F, 0.0F});
        if (target_is_alt) {
            agent.quadtroid_flags &= static_cast<std::uint8_t>(~(Bit0 | Bit7));
            agent.invulnerable = false;
            set_state(StateAltGrab);
        } else {
            agent.quadtroid_flags |= Bit0 | Bit7;
            set_state(StateBipedGrab);
        }
        break;

    case StateBipedGrab:
        agent.invulnerable = true;
        if (target_is_alt) {
            set_state(StateMorphTransition, 8u);
            agent.invulnerable = false;
            break;
        }
        agent.position = add(target->position, {0.0F, 0.5F, 0.0F});
        decrement(agent.quadtroid_state_timer);
        if (agent.quadtroid_state_timer == 0) {
            apply_enemy_contact_damage(agent, *target, 2);
            agent.quadtroid_state_timer = 16u;
        }
        break;

    case StateMorphTransition:
        agent.invulnerable = false;
        agent.position = add(target->position,
                             multiply(target->facing, 0.20F));
        decrement(agent.quadtroid_state_timer);
        if (agent.quadtroid_state_timer == 0) {
            set_state(StateAltGrab);
        }
        break;

    case StateAltGrab:
        agent.invulnerable = false;
        agent.position = add(target->position, {0.0F, -0.25F, 0.0F});
        if (!target_is_alt) {
            set_state(StateRelease, 8u);
            break;
        }
        decrement(agent.quadtroid_grab_timer);
        decrement(agent.quadtroid_state_timer);
        if (agent.quadtroid_state_timer == 0) {
            apply_enemy_contact_damage(agent, *target, 2);
            agent.quadtroid_state_timer = 16u;
        }
        break;

    case StateRelease:
        agent.invulnerable = false;
        agent.quadtroid_attached = false;
        agent.quadtroid_flags &= static_cast<std::uint8_t>(
            ~(Bit0 | Bit7));
        decrement(agent.quadtroid_state_timer);
        if (agent.quadtroid_state_timer == 0) {
            set_state(target_is_alt ? StateAltGrab : StateBipedGrab);
        }
        break;

    case StateStunned:
        agent.velocity = {};
        agent.invulnerable = true;
        decrement(agent.quadtroid_state_timer);
        if (agent.quadtroid_state_timer == 0) {
            agent.quadtroid_flags &= static_cast<std::uint8_t>(~Bit7);
            agent.invulnerable = false;
            set_state(StateIdle);
        }
        break;

    case StateBombRelease:
        agent.invulnerable = true;
        agent.velocity = agent.quadtroid_launch_velocity;
        agent.position = add(agent.position,
                             multiply(agent.velocity, seconds));
        decrement(agent.quadtroid_state_timer);
        if (agent.quadtroid_state_timer == 0) {
            agent.quadtroid_flags &= static_cast<std::uint8_t>(~Bit7);
            agent.invulnerable = false;
            set_state(StateIdle);
        }
        break;

    default:
        detach();
        set_state(StateIdle);
        break;
    }

    agent.quadtroid_previous_health = agent.health;
}
} // namespace fruityprime::gameplay

namespace fruityprime::enemy {

QuadtroidProfile decode_quadtroid_profile(
    std::uint8_t id, std::span<const std::uint8_t> fields,
    net::Vec3 origin) noexcept {
    QuadtroidProfile result;
    if (id != static_cast<std::uint8_t>(formats::EnemyType::Quadtroid)
        || fields.size() < 2 * 64) {
        return result;
    }
    result.hurt_volume = detail::read_volume(fields, 0, origin);
    result.encounter_volume = detail::read_volume(fields, 64, origin);
    result.supported = result.hurt_volume.kind != scene::VolumeKind::Invalid
        && result.encounter_volume.kind != scene::VolumeKind::Invalid;
    return result;
}

} // namespace fruityprime::enemy
