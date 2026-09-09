#include "enemy_decode_common.hpp"
// Native port of src/MphRead/Entities/Enemies/00_WarWasp.cs.
//
// Enemy00 and Enemy10 deliberately share the movement helpers in the managed
// implementation, but not the subroutine table or ranged attack. Keeping the
// controller here and exposing a member entry point lets the two source
// modules share only that documented part of the implementation.
#include "00_WarWasp.hpp"
#include "enemy_common.hpp"

namespace fruityprime::gameplay {
namespace {

constexpr float Pi = 3.14159265358979323846F;

[[nodiscard]] std::uint32_t frame_count(float seconds) noexcept {
    return static_cast<std::uint32_t>(std::max(
        1.0F, std::round(std::max(0.0F, seconds * 60.0F))));
}

} // namespace

void Session::update_warwasp(EnemyState& agent) {
    update_wasp_controller(agent, false);
}

void Session::update_wasp_controller(EnemyState& agent, bool barbed) {
    if (!agent.authored_motion.supported) {
        static_cast<void>(update_generic_enemy(agent));
        return;
    }

    const auto& motion = agent.authored_motion;
    const float seconds = config_.tick_seconds;
    const std::uint32_t frames = frame_count(seconds);
    const std::uint32_t movement_type = motion.movement_type;
    const auto decrement = [frames](std::uint32_t& value) noexcept {
        value = value > frames ? value - frames : 0;
    };

    std::size_t target_index = players_.size();
    float target_distance = std::numeric_limits<float>::max();
    for (std::size_t index = 0; index < players_.size(); ++index) {
        const auto& player = players_[index];
        if (!objective_player(player)) {
            continue;
        }
        const float distance = distance_squared(player.position,
                                                agent.position);
        if (distance < target_distance) {
            target_distance = distance;
            target_index = index;
        }
    }

    const auto target_position = [&]() noexcept {
        return target_index == players_.size()
            ? agent.wasp_attack_target
            : players_[target_index].position;
    };
    const auto face = [&agent](net::Vec3 direction) noexcept {
        agent.facing = normalized_or(direction, agent.facing);
    };
    const auto home_contains = [&motion](net::Vec3 position) noexcept {
        return motion.home_volume.contains(to_volume_point(position));
    };

    const auto start_moving = [&agent, seconds](net::Vec3 target,
                                                float step) noexcept {
        agent.wasp_move_target = target;
        agent.wasp_step_distance = step;
        const net::Vec3 travel = subtract(target, agent.position);
        const float distance = std::sqrt(std::max(0.0F,
                                                  length_squared(travel)));
        if (distance <= 0.0001F || step <= 0.0F || seconds <= 0.0F) {
            agent.velocity = {};
            agent.wasp_step_count = 0;
            return;
        }
        agent.wasp_step_count = static_cast<std::uint32_t>(
            distance / step) + 1u;
        agent.wasp_step_count *= 2u;
        // EnemyInstanceEntity adds _speed once per authored call. The native
        // session stores velocity per second, so retain the same half-step
        // and convert it to this session's tick duration.
        agent.velocity = multiply(travel,
                                  (step * 0.5F) / distance / seconds);
    };

    const auto start_moving_to_position = [&]() noexcept {
        if (motion.path_count == 0) {
            agent.velocity = {};
            agent.wasp_step_count = 0;
            return;
        }
        agent.wasp_move_index = std::min<std::uint8_t>(
            agent.wasp_move_index,
            static_cast<std::uint8_t>(motion.path_count - 1));
        const auto index = static_cast<std::size_t>(agent.wasp_move_index);
        const float step = barbed
            ? static_cast<float>(agent.barbed_warwasp.step_distance1)
                / 4096.0F
            : (movement_type == 3 ? 0.25F : 0.2F);
        start_moving(agent.wasp_move_positions[index], step);
        face(agent.velocity);
    };

    const auto reverse_pattern = [&]() noexcept {
        agent.wasp_pattern = agent.wasp_next_pattern;
        if (agent.wasp_pattern == 0) {
            agent.wasp_final_move_index = agent.wasp_max_move_index;
            agent.wasp_move_index = agent.wasp_move_index
                >= agent.wasp_max_move_index
                ? 0 : static_cast<std::uint8_t>(agent.wasp_move_index + 1);
        } else {
            agent.wasp_final_move_index = 0;
            agent.wasp_move_index = agent.wasp_move_index == 0
                ? agent.wasp_max_move_index
                : static_cast<std::uint8_t>(agent.wasp_move_index - 1);
        }
        start_moving_to_position();
    };
    const auto reach_or_reverse = [&]() noexcept {
        if (length_squared(subtract(agent.wasp_move_target,
                                     agent.position)) <= 0.000001F) {
            agent.wasp_step_count = 0;
        } else {
            reverse_pattern();
        }
    };
    const auto advance_pattern = [&](float step) noexcept {
        if (motion.path_count == 0) {
            agent.velocity = {};
            agent.wasp_step_count = 0;
            return;
        }
        if (agent.wasp_pattern == 1) {
            agent.wasp_move_index = agent.wasp_move_index == 0
                ? agent.wasp_max_move_index
                : static_cast<std::uint8_t>(agent.wasp_move_index - 1);
        } else if (agent.wasp_pattern == 2 || agent.wasp_pattern == 0) {
            agent.wasp_move_index = agent.wasp_move_index
                >= agent.wasp_max_move_index
                ? 0 : static_cast<std::uint8_t>(agent.wasp_move_index + 1);
            if (agent.wasp_pattern == 0 && movement_type == 3
                && agent.wasp_move_index == 0) {
                agent.health = 0;
                agent.active = false;
                return;
            }
        } else if (agent.wasp_pattern == 3
                   && agent.wasp_move_index > 0) {
            --agent.wasp_move_index;
        }
        agent.wasp_move_target = agent.wasp_move_positions[
            std::min<std::size_t>(agent.wasp_move_index,
                                  motion.path_count - 1)];
        start_moving(agent.wasp_move_target, step);
        face(agent.velocity);
    };

    // DoMovement happens before EnemyProcess in EnemyInstanceEntity. Apply
    // the previous authored speed first, then schedule the next one.
    if (seconds > 0.0F) {
        if (movement_type == 0 && motion.circle_radius > 0.0F) {
            const net::Vec3 old_position = agent.position;
            agent.wasp_circle_angle += 0.75F
                * static_cast<float>(frames);
            while (agent.wasp_circle_angle >= 360.0F) {
                agent.wasp_circle_angle -= 360.0F;
            }
            const float angle = agent.wasp_circle_angle * Pi / 180.0F;
            agent.position = {
                agent.wasp_initial_position.x
                    + std::sin(angle) * motion.circle_radius,
                agent.wasp_initial_position.y,
                agent.wasp_initial_position.z
                    + std::cos(angle) * motion.circle_radius};
            agent.velocity = multiply(
                subtract(agent.position, old_position), 1.0F / seconds);
            face(agent.velocity);
        } else {
            agent.position = add(agent.position,
                                 multiply(agent.velocity, seconds));
        }
    }

    const std::uint16_t contact_damage = barbed
        ? agent.barbed_warwasp.contact_damage : 3;
    if ((!barbed && agent.state != 4 && agent.state != 5)
        || barbed) {
        for (auto& player : players_) {
            if (!objective_player(player)
                || distance_squared(player.position, agent.position)
                    > 1.4F * 1.4F) {
                continue;
            }
            apply_enemy_contact_damage(agent, player, contact_damage);
            break;
        }
    }

    if (barbed) {
        switch (agent.state) {
        case 0: // State0 / Behavior01 then Behavior03.
            if (movement_type != 0) {
                face(subtract(agent.wasp_move_target, agent.position));
            }
            if (agent.wasp_step_count > 0) {
                --agent.wasp_step_count;
                break;
            }
            if (movement_type != 0) {
                advance_pattern(static_cast<float>(
                    agent.barbed_warwasp.step_distance1) / 4096.0F);
            }
            break;
        case 1: // State1
            face(subtract(target_position(), agent.position));
            if (movement_type == 0) {
                break;
            }
            if (agent.wasp_step_count > 0) {
                --agent.wasp_step_count;
                break;
            }
            if (agent.wasp_move_index == agent.wasp_final_move_index) {
                agent.velocity = {};
                agent.wasp_windup_timer = 20;
                agent.state = 2;
            } else if (!home_contains(agent.position)) {
                reach_or_reverse();
                agent.state = 4;
            } else if (target_index != players_.size()
                       && !home_contains(target_position())) {
                reverse_pattern();
                agent.state = 4;
            } else {
                advance_pattern(static_cast<float>(
                    agent.barbed_warwasp.step_distance1) / 4096.0F);
                agent.state = 1;
            }
            break;
        case 2: // State2 / Behavior00
            face(subtract(target_position(), agent.position));
            decrement(agent.wasp_windup_timer);
            if (agent.wasp_windup_timer == 0) {
                agent.wasp_aim_vector = normalized_or(
                    subtract(add(target_position(), {0.0F, 0.5F, 0.0F}),
                             agent.position), agent.facing);
                agent.state = 3;
            }
            break;
        case 3: // State3 / Behavior04 then Behavior05.
            if (agent.wasp_shot_count > 0
                && agent.wasp_shot_timer == 10u * 2u) {
                spawn_enemy_projectile(
                    agent, add(agent.position, {0.0F, -0.5F, 0.0F}),
                    agent.wasp_aim_vector);
                --agent.wasp_shot_count;
            }
            decrement(agent.wasp_shot_timer);
            if (agent.wasp_shot_timer == 0) {
                if (agent.wasp_shot_count > 0) {
                    agent.wasp_shot_timer = 30u * 2u;
                    agent.wasp_windup_timer = 20;
                    agent.state = 2;
                } else {
                    agent.wasp_shot_count = static_cast<std::uint16_t>(
                        agent.barbed_warwasp.min_shots + rng_.random2(
                            static_cast<std::uint32_t>(
                                agent.barbed_warwasp.max_shots
                                    >= agent.barbed_warwasp.min_shots
                                ? agent.barbed_warwasp.max_shots
                                    - agent.barbed_warwasp.min_shots + 1u : 1u)));
                    agent.wasp_shot_timer = 30u * 2u;
                    agent.state = 4;
                    if (movement_type != 0) {
                        start_moving_to_position();
                    } else {
                        agent.velocity = {};
                    }
                }
            }
            break;
        case 4: // State4 / Behavior01.
            face(subtract(agent.wasp_move_target, agent.position));
            if (agent.wasp_step_count > 0) {
                --agent.wasp_step_count;
            } else {
                agent.state = 0;
                if (movement_type != 0) {
                    start_moving_to_position();
                }
            }
            break;
        case 5: // collision recovery state
            agent.state = 1;
            break;
        default:
            agent.state = 0;
            break;
        }
    } else {
        switch (agent.state) {
        case 0: // State0: orbit/path movement, then Behavior02/03.
            if (movement_type != 0) {
                face(subtract(agent.wasp_move_target, agent.position));
            }
            if (movement_type == 0) {
                break;
            }
            if (agent.wasp_step_count > 0) {
                --agent.wasp_step_count;
                break;
            }
            if (target_index != players_.size()
                && home_contains(target_position())) {
                agent.wasp_final_move_index = agent.wasp_move_index;
                agent.wasp_next_pattern = agent.wasp_pattern = 2;
                agent.wasp_step_distance = 0.15F;
                agent.state = 1;
            } else {
                advance_pattern(agent.state == 6 ? 0.2F
                                                  : agent.wasp_step_distance);
                agent.state = 1;
            }
            break;
        case 1: // State1
            face(subtract(target_position(), agent.position));
            if (agent.wasp_step_count > 0) {
                --agent.wasp_step_count;
                break;
            }
            if (agent.wasp_move_index == agent.wasp_final_move_index) {
                agent.velocity = {};
                agent.state = 6;
            } else if (!home_contains(agent.position)) {
                reach_or_reverse();
                agent.state = 6;
            } else if (target_index != players_.size()
                       && !home_contains(target_position())) {
                reverse_pattern();
                agent.state = 6;
            } else {
                advance_pattern(agent.wasp_step_distance);
                agent.state = 2;
            }
            break;
        case 2: // State2 / State1 body, then Behavior09/07/10/08.
            face(subtract(target_position(), agent.position));
            if (agent.wasp_attack_delay > 0) {
                --agent.wasp_attack_delay;
                break;
            }
            agent.wasp_attack_target = target_position();
            agent.wasp_step_count = 40u * 2u;
            agent.state = 3;
            break;
        case 3: // State3 / State1 body, Behavior00.
            face(subtract(target_position(), agent.position));
            if (agent.wasp_step_count > 0) {
                --agent.wasp_step_count;
            } else {
                start_moving(agent.wasp_attack_target, 1.2F);
                agent.state = 4;
            }
            break;
        case 4: // State4
            face(subtract(agent.wasp_move_target, agent.position));
            if (target_index != players_.size()
                && distance_squared(target_position(), agent.position)
                    <= 1.4F * 1.4F) {
                apply_enemy_contact_damage(agent, players_[target_index], 25);
            }
            if (agent.wasp_step_count > 0) {
                --agent.wasp_step_count;
                break;
            }
            start_moving(agent.wasp_attack_target, 1.2F);
            agent.state = 5;
            break;
        case 5: // State5 / Behavior01
            if (target_index != players_.size()
                && distance_squared(target_position(), agent.position)
                    <= 1.4F * 1.4F) {
                apply_enemy_contact_damage(agent, players_[target_index], 25);
            }
            agent.wasp_attack_delay = 30u * 2u;
            if (agent.wasp_step_count > 0) {
                --agent.wasp_step_count;
                break;
            }
            agent.velocity = {};
            if (movement_type != 0) {
                start_moving_to_position();
            }
            agent.state = 1;
            break;
        case 6: // State6 / Behavior02/03
            if (movement_type != 0) {
                face(subtract(agent.wasp_move_target, agent.position));
            }
            if (agent.wasp_step_count > 0) {
                --agent.wasp_step_count;
                break;
            }
            if (target_index != players_.size()
                && home_contains(target_position())) {
                agent.wasp_final_move_index = agent.wasp_move_index;
                agent.wasp_next_pattern = agent.wasp_pattern = 2;
                agent.wasp_step_distance = 0.15F;
                agent.state = 1;
            } else {
                advance_pattern(0.2F);
                agent.state = 0;
            }
            break;
        default:
            agent.state = 0;
            break;
        }
    }

    if (agent.health == 0) {
        agent.active = false;
    }
}
} // namespace fruityprime::gameplay

static_assert(fruityprime::enemy::module_00_war_wasp::kModule.managed_class.size() != 0);

namespace fruityprime::enemy {

AuthoredMotion decode_authored_motion(
    std::uint8_t id, std::span<const std::uint8_t> fields,
    net::Vec3 origin) noexcept {
    AuthoredMotion result;
    if ((id != static_cast<std::uint8_t>(formats::EnemyType::WarWasp)
         && id != static_cast<std::uint8_t>(
             formats::EnemyType::BarbedWarWasp))
        || fields.size() < 400) {
        return result;
    }

    // S01 starts at the beginning of the union; S08 has two common scalar
    // fields before the same WarWasp payload.
    const std::size_t base = id == static_cast<std::uint8_t>(
        formats::EnemyType::BarbedWarWasp) ? 8 : 0;
    if (base + 392 > fields.size()) {
        return result;
    }

    result.supported = true;
    result.initial_position = origin;
    result.movement_type = detail::read_u32(fields, base + 388);
    result.step_distance = result.movement_type == 3 ? 0.25F : 0.2F;
    result.movement_volume = detail::read_volume(fields, base + 64, origin);
    result.home_volume = detail::read_volume(fields, base + 128, origin);

    const std::size_t path_count_offset = base + 384;
    const std::size_t path_offset = base + 192;
    if (result.movement_type == 2 || result.movement_type == 3) {
        result.path_count = static_cast<std::uint8_t>(std::min<std::uint32_t>(
            detail::read_u32(fields, path_count_offset) & 0xffu, 16u));
        for (std::size_t i = 0; i < result.path_count; ++i) {
            result.path_positions[i] = detail::add(
                origin, detail::read_vector(fields, path_offset + i * 12));
        }
    } else if (result.movement_type == 1) {
        // The managed class expands the authored box into four corners.
        // This is the S01/S08 Volume1 layout: vector3 and dot1/dot3 define
        // the two horizontal axes and position is the box center.
        const std::size_t volume = base + 64;
        if (detail::read_u32(fields, volume) == static_cast<std::uint32_t>(
                formats::VolumeType::Box)) {
            const net::Vec3 vector3 = detail::read_vector(fields, volume + 28);
            const net::Vec3 center = detail::add(
                origin, detail::read_vector(fields, volume + 40));
            const float dot1 = detail::read_fixed(fields, volume + 52);
            const float dot3 = detail::read_fixed(fields, volume + 60);
            const float xx = vector3.x * dot1;
            const float xz = vector3.x * dot3;
            const float zx = vector3.z * dot1;
            const float zz = vector3.z * dot3;
            result.path_count = 4;
            result.path_positions[0] = center;
            result.path_positions[1] = detail::add(center, {xz, 0.0F, zz});
            result.path_positions[2] = detail::add(
                center, {xz - zx, 0.0F, zz + xx});
            result.path_positions[3] = detail::add(center, {-zx, 0.0F, xx});
        }
    } else if (result.movement_type == 0) {
        const std::size_t volume = base + 128;
        if (detail::read_u32(fields, volume) == static_cast<std::uint32_t>(
                formats::VolumeType::Cylinder)) {
            result.circle_radius = std::abs(detail::read_fixed(fields, volume + 28));
        }
    }
    return result;
}

} // namespace fruityprime::enemy

