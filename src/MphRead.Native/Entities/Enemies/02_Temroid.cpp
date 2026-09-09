#include "enemy_decode_common.hpp"
// Native port of src/MphRead/Entities/Enemies/02_Temroid.cs.
// This member definition lives in its enemy module; Session only dispatches it.
#include "02_Temroid.hpp"
#include "enemy_common.hpp"

namespace fruityprime::gameplay {
namespace {

constexpr std::uint8_t kNoPlayer = 0xff;
constexpr float kFixedRate = 60.0F;
constexpr float kFloatEqualThreshold = 1.0F / 4096.0F;

[[nodiscard]] bool initial_state(std::uint8_t state) noexcept {
    return state == 0 || state == 7 || state == 10;
}

[[nodiscard]] bool differs(float left, float right) noexcept {
    return std::abs(left - right) >= kFloatEqualThreshold;
}

} // namespace

void Session::update_temroid(EnemyState& agent) {
    if (!agent.temroid.supported) {
        static_cast<void>(update_generic_enemy(agent));
        return;
    }

    const float seconds = std::max(0.0F, config_.tick_seconds);
    const auto frame_step = static_cast<std::uint32_t>(std::max(
        1.0F, std::round(seconds * kFixedRate)));
    const auto decrement = [frame_step](std::uint32_t& value) noexcept {
        value = value > frame_step ? value - frame_step : 0;
    };

    const auto find_player = [this](std::uint8_t slot)
        -> net::PlayerState* {
        if (slot == kNoPlayer) {
            return nullptr;
        }
        const auto found = std::find_if(
            players_.begin(), players_.end(),
            [slot](const net::PlayerState& player) {
                return player.slot_index == slot;
            });
        return found == players_.end() ? nullptr : &*found;
    };

    // Enemy02Entity is a single-player class in the managed tree. The
    // native session can hold several objective players, so use the nearest
    // live slot when there is more than one. The state predicates below still
    // consume the exact authored distance and line-of-sight thresholds.
    const auto current_target = [this, &agent, &find_player]()
        -> net::PlayerState* {
        if (agent.target_slot != kNoPlayer) {
            if (auto* selected = find_player(agent.target_slot);
                selected != nullptr && objective_player(*selected)) {
                return selected;
            }
        }

        net::PlayerState* result = nullptr;
        float nearest = std::numeric_limits<float>::max();
        for (auto& player : players_) {
            if (!objective_player(player)) {
                continue;
            }
            const float distance = distance_squared(
                player.position, agent.position);
            if (distance < nearest) {
                nearest = distance;
                result = &player;
            }
        }
        return result;
    };

    const auto attached_target = [&find_player, &agent]()
        -> net::PlayerState* {
        return find_player(agent.temroid_attached_slot);
    };

    const auto any_other_temroid_attached = [this, &agent](
                                                std::uint8_t slot) noexcept {
        return std::any_of(
            enemies_.begin(), enemies_.end(),
            [&agent, slot](const EnemyState& other) {
                return other.id != agent.id && other.active
                    && other.enemy_type == static_cast<std::uint8_t>(
                        formats::EnemyType::Temroid)
                    && other.temroid_attached_slot == slot;
            });
    };

    const auto blocked_between = [this](net::Vec3 from,
                                        net::Vec3 to) noexcept {
        const auto hit = collision::sweep_sphere(
            room_.collision(), to_collision(from), to_collision(to),
            0.0F, 0);
        return hit.has_value() && hit->fraction < 0.98F;
    };

    const auto overlaps_player = [&agent](const net::PlayerState& player) {
        // Enemy02Entity's S03 hurt volume is the collision volume around the
        // one-unit Temroid body. The native gameplay boundary does not yet
        // retain transformed per-entity volumes, so keep the same contact
        // predicate in one place using EnemyInitialize's one-unit radius.
        constexpr float contact_radius = 1.25F;
        return objective_player(player)
            && distance_squared(agent.position, player.position)
                <= contact_radius * contact_radius;
    };

    const auto set_frame_speed = [&agent](net::Vec3 speed) noexcept {
        // Managed _speed is added once per game frame. EnemyState.velocity is
        // stored in world-units/second, so preserve the same 1/60-step value.
        agent.velocity = multiply(speed, kFixedRate);
    };

    const auto set_animation = [&agent, frame_step](
                                   std::uint8_t animation, bool no_loop,
                                   std::uint32_t duration) {
        agent.temroid_animation = animation;
        agent.temroid_animation_no_loop = no_loop;
        agent.temroid_animation_timer = no_loop
            ? std::max(frame_step, duration) : 0;
        agent.temroid_animation_ended = false;
    };

    const auto animation_ended = [&agent]() noexcept {
        return agent.temroid_animation_no_loop
            && agent.temroid_animation_ended;
    };

    // Temroid_lod0 in the extracted ROM has no companion animation group.
    // Keep a one-frame callback cursor for no-loop states so the native state
    // graph remains live; an extracted animation table can replace this
    // fallback duration without changing the gameplay graph.
    constexpr std::uint32_t kNoLoopFallbackFrames = 1;

    const auto target_for_entry = [&]() -> net::PlayerState* {
        if (auto* selected = find_player(agent.target_slot);
            selected != nullptr && objective_player(*selected)) {
            return selected;
        }
        return current_target();
    };

    const auto configure_state = [&](std::uint8_t state) {
        switch (state) {
        case 0:
            set_animation(0, false, 0);
            agent.velocity = {};
            break;
        case 1:
            set_animation(1, false, 0);
            break;
        case 2:
            // Anim10, NoLoop.
            set_animation(10, true, kNoLoopFallbackFrames);
            agent.velocity = {};
            break;
        case 3: {
            set_animation(11, true, kNoLoopFallbackFrames);
            agent.temroid_phase = 0.0F;
            agent.temroid_height_origin = agent.position;
            auto* target = target_for_entry();
            const net::Vec3 facing = target == nullptr
                ? agent.facing
                : normalized_or(add(subtract(target->position,
                                             agent.position),
                                    {0.0F, 0.5F, 0.0F}),
                                agent.facing);
            agent.facing = facing;
            set_frame_speed(multiply(
                {facing.x, 0.0F, facing.z}, -0.15F));
            break;
        }
        case 4: {
            set_animation(12, true, kNoLoopFallbackFrames);
            auto* target = target_for_entry();
            const net::Vec3 facing = target == nullptr
                ? agent.facing
                : normalized_or(add(subtract(target->position,
                                             agent.position),
                                    {0.0F, 0.5F, 0.0F}),
                                agent.facing);
            agent.facing = facing;
            set_frame_speed(multiply(
                {facing.x, 0.0F, facing.z}, 0.15F));
            break;
        }
        case 5: {
            set_animation(3, false, 0);
            auto* target = target_for_entry();
            const net::Vec3 facing = target == nullptr
                ? agent.facing
                : normalized_or(add(subtract(target->position,
                                             agent.position),
                                    {0.0F, 0.5F, 0.0F}),
                                agent.facing);
            agent.facing = facing;
            set_frame_speed(multiply(facing, 0.25F));
            agent.temroid_field170 = 20u * 2u;
            break;
        }
        case 6:
            set_animation(agent.temroid_animation == 7 ? 9 : 8, true,
                          kNoLoopFallbackFrames);
            agent.velocity = {};
            break;
        case 7:
            set_animation(0, false, 0);
            break;
        case 8: {
            auto* target = target_for_entry();
            const bool alt_form = target != nullptr
                && (target->flags & net::PlayerState::FlagAltForm) != 0;
            net::Vec3 facing = target == nullptr
                ? agent.facing : multiply(target->facing, -1.0F);
            if (alt_form) {
                facing.y = 0.0F;
                if (length_squared(facing) <= 0.000001F) {
                    facing = {1.0F, 0.0F, 0.0F};
                }
            } else if (std::abs(facing.y) <= kFloatEqualThreshold
                       && target != nullptr) {
                facing.y = -target->facing.y;
            }
            agent.facing = normalized_or(facing, agent.facing);
            set_animation(alt_form ? 7 : 4, false, 0);
            agent.velocity = {};
            agent.temroid_hit_by_bomb = false;
            agent.temroid_field170 = 150u * 2u;
            agent.temroid_drain_damage_timer = 0;
            agent.temroid_state_timer = 0;
            break;
        }
        case 9:
            set_animation(15, true, kNoLoopFallbackFrames);
            agent.velocity = {};
            break;
        case 10:
            set_animation(2, false, 0);
            agent.velocity = {};
            break;
        default:
            set_animation(0, false, 0);
            agent.velocity = {};
            break;
        }
    };

    const auto transition = [&](std::uint8_t current,
                                std::uint8_t next) {
        if (initial_state(current) && !initial_state(next)) {
            const std::size_t attached_count = static_cast<std::size_t>(
                std::count_if(
                    enemies_.begin(), enemies_.end(),
                    [](const EnemyState& other) {
                        return other.active
                            && other.enemy_type == static_cast<std::uint8_t>(
                                formats::EnemyType::Temroid)
                            && other.temroid_field1d0;
                    }));
            if (attached_count >= 3) {
                agent.temroid_next_state = current;
                return false;
            }
            agent.temroid_field1d0 = true;
        } else if (!initial_state(current) && initial_state(next)) {
            agent.temroid_field1d0 = false;
        }
        agent.temroid_next_state = next;
        configure_state(next);
        return true;
    };

    if (!agent.temroid_state_initialized) {
        agent.temroid_state_initialized = true;
        agent.temroid_height_origin = agent.position;
        agent.temroid_next_state = agent.state;
        agent.temroid_sub_id = agent.state;
        agent.temroid_time_since_damage = 510;
        configure_state(agent.state);
    }

    // EnemyInstanceEntity.Process increments this before copying state2 to
    // state1. Behavior04 consumes the first frame after a hit.
    if (agent.temroid_time_since_damage < 510) {
        const auto next = static_cast<std::uint32_t>(
            agent.temroid_time_since_damage) + frame_step;
        agent.temroid_time_since_damage = static_cast<std::uint16_t>(
            std::min<std::uint32_t>(510, next));
    }
    if (agent.temroid_animation_no_loop
        && agent.temroid_animation_timer > 0) {
        decrement(agent.temroid_animation_timer);
        if (agent.temroid_animation_timer == 0) {
            agent.temroid_animation_ended = true;
        }
    }

    const std::uint8_t current = agent.temroid_next_state;
    agent.state = current;
    agent.temroid_sub_id = current;

    // DoMovement runs before EnemyProcess in the managed base class. The
    // speed selected by a state is therefore consumed on the next fixed tick.
    if (seconds > 0.0F && current != 8) {
        const net::Vec3 next = add(
            agent.position, multiply(agent.velocity, seconds));
        const auto hit = collision::sweep_sphere(
            room_.collision(), to_collision(agent.position),
            to_collision(next), agent.body_radius, 0x2000);
        if (hit.has_value()) {
            agent.position = {hit->center.x + hit->normal.x * 0.001F,
                              hit->center.y + hit->normal.y * 0.001F,
                              hit->center.z + hit->normal.z * 0.001F};
            const net::Vec3 normal{hit->normal.x, hit->normal.y,
                                   hit->normal.z};
            const float into = dot(agent.velocity, normal);
            if (into < 0.0F) {
                agent.velocity = subtract(agent.velocity,
                                          multiply(normal, into));
            }
            if (current == 5) {
                agent.temroid_field170 = 0;
            }
        } else {
            agent.position = next;
        }
    }

    auto* target = current_target();
    if (target != nullptr) {
        agent.target_slot = target->slot_index;
    }

    const auto state02 = [&]() {
        if (target != nullptr && overlaps_player(*target)) {
            apply_enemy_contact_damage(agent, *target, 15);
        }
    };

    const auto update_height = [&](float sign) {
        // The C# formula uses the model's frame count. The extracted model
        // has no animation group, so use the authored 30-frame fallback.
        constexpr float fallback_frame_count = 30.0F;
        agent.temroid_phase += sign * (
            45.0F / fallback_frame_count / 2.0F)
            * static_cast<float>(frame_step);
        if (agent.temroid_phase >= 360.0F) {
            agent.temroid_phase = 0.0F;
        }
        const float desired_delta = agent.temroid_height_origin.y
            + std::sin(agent.temroid_phase) / 2.0F - agent.position.y;
        agent.velocity.y = desired_delta * kFixedRate;
    };

    const auto steer = [&](net::Vec3 point1, net::Vec3 point2,
                           float sign) {
        const net::Vec3 between = normalized_or(
            subtract(point1, point2), {});
        net::Vec3 facing = agent.facing;
        if (differs(facing.x, between.x) || differs(facing.y, between.y)
            || differs(facing.z, between.z)) {
            const net::Vec3 previous = facing;
            facing.x += (between.x - facing.x) / 8.0F;
            facing.z += (between.z - facing.z) / 8.0F;
            if (facing.x == 0.0F && facing.z == 0.0F) {
                facing.x = previous.x;
                facing.z = previous.z;
            }
            facing.y = between.y;
            facing = normalized_or(facing, previous);
            if (differs(facing.x, previous.x)
                && differs(facing.z, previous.z)) {
                facing.x += 0.125F;
                facing.z -= 0.125F;
                if (facing.x == 0.0F && facing.z == 0.0F) {
                    facing.x += 0.125F;
                    facing.z -= 0.125F;
                }
                facing = normalized_or(facing, previous);
            }
            set_frame_speed(multiply(facing, sign * 0.05F));
            facing.y = 0.0F;
            agent.facing = normalized_or(facing, agent.facing);
            agent.up = {0.0F, 1.0F, 0.0F};
        }
    };

    switch (current) {
    case 0: {
        const std::uint8_t point = std::min<std::uint8_t>(
            agent.temroid_idle_index, 3);
        if (distance_squared(agent.temroid_idle_points[point],
                             agent.position) < 1.0F) {
            agent.temroid_idle_index = static_cast<std::uint8_t>(
                (point + 1) % 4);
        }
        steer(agent.temroid_idle_points[agent.temroid_idle_index],
              agent.position, 1.0F);
        state02();
        // State00's subroutine: Behavior04, then Behavior05.
        if (agent.temroid_time_since_damage == 1) {
            transition(current, 2);
        } else if (target != nullptr
                   && !blocked_between(agent.position, target->position)
                   && distance_squared(target->position, agent.position)
                          < 6.5F * 6.5F) {
            transition(current, 1);
        }
        break;
    }
    case 1:
        if (target != nullptr) {
            steer(target->position, agent.position, 1.0F);
            state02();
            // State01's table is ordered: blocked, <10, attached, <5.
            if (blocked_between(agent.position, target->position)) {
                transition(current, 7);
            } else if (distance_squared(target->position, agent.position)
                           < 100.0F) {
                transition(current, 7);
            } else if (any_other_temroid_attached(target->slot_index)) {
                transition(current, 10);
            } else if (distance_squared(target->position, agent.position)
                           < 25.0F) {
                transition(current, 2);
            }
        }
        break;
    case 2:
        state02();
        if (animation_ended()) {
            transition(current, 3);
        }
        break;
    case 3:
        update_height(1.0F);
        state02();
        if (animation_ended()) {
            transition(current, 4);
        }
        break;
    case 4:
        update_height(-1.0F);
        state02();
        if (animation_ended()) {
            transition(current, 5);
        }
        break;
    case 5:
        // Behavior02 gates the attach check for 40 fixed frames.
        if (agent.temroid_field170 > 0) {
            decrement(agent.temroid_field170);
        } else if (target != nullptr && overlaps_player(*target)
                   && !any_other_temroid_attached(target->slot_index)) {
            agent.temroid_attached_slot = target->slot_index;
            transition(current, 8);
        }
        break;
    case 6:
        steer(agent.position, agent.temroid_idle_points[0], -1.0F);
        if (animation_ended()) {
            transition(current, 9);
        }
        break;
    case 7:
        steer(agent.temroid_idle_points[0], agent.position, 1.0F);
        state02();
        // State07's table is ordered: damaged, visible/near, home.
        if (agent.temroid_time_since_damage == 1) {
            transition(current, 1);
        } else if (target != nullptr
                   && !blocked_between(agent.position, target->position)
                   && distance_squared(target->position, agent.position)
                          < 6.5F * 6.5F) {
            transition(current, 1);
        } else if (distance_squared(agent.temroid_idle_points[0],
                                    agent.position) < 1.0F) {
            transition(current, 0);
        }
        break;
    case 8: {
        auto* attached = attached_target();
        if (attached == nullptr
            || (attached->flags & net::PlayerState::FlagActive) == 0
            || (attached->flags & net::PlayerState::FlagSpectating) != 0) {
            agent.temroid_attached_slot = kNoPlayer;
            transition(current, 6);
            break;
        }

        const bool alt_form = (attached->flags
                               & net::PlayerState::FlagAltForm) != 0;
        net::Vec3 facing = multiply(attached->facing, -1.0F);
        if (alt_form) {
            facing.y = 0.0F;
            if (length_squared(facing) <= 0.000001F) {
                facing = {1.0F, 0.0F, 0.0F};
            }
            agent.position = add(attached->position,
                                 {0.0F, 0.625F, 0.0F});
        } else {
            // PlayerProcess.UpdateAttached uses CameraInfo.Position. The
            // shared PlayerState has no camera, so use the host eye offset.
            agent.position = add(
                add(attached->position, {0.0F, 0.625F, 0.0F}),
                multiply(attached->facing, 0.5F));
            if (std::abs(facing.y) <= kFloatEqualThreshold) {
                facing.y = -attached->facing.y;
            }
        }
        agent.facing = normalized_or(facing, agent.facing);
        agent.velocity = {};
        agent.target_slot = attached->slot_index;

        if (agent.temroid_drain_damage_timer > 0) {
            decrement(agent.temroid_drain_damage_timer);
        } else {
            apply_enemy_contact_damage(agent, *attached, 2);
            agent.temroid_drain_damage_timer = 8u * 2u;
            agent.temroid_state_timer = agent.temroid_drain_damage_timer;
        }

        // State08's table is ordered: bomb latch, timeout, player death.
        bool leave = false;
        if (agent.temroid_hit_by_bomb) {
            agent.temroid_hit_by_bomb = false;
            leave = true;
        } else if (agent.temroid_field170 > 0) {
            decrement(agent.temroid_field170);
        } else {
            leave = true;
        }
        if (leave) {
            agent.temroid_attached_slot = kNoPlayer;
            transition(current, 6);
        }
        break;
    }
    case 9:
        if (animation_ended()) {
            transition(current, 7);
        }
        break;
    case 10:
        state02();
        if (target == nullptr
            || !any_other_temroid_attached(target->slot_index)) {
            transition(current, 7);
        }
        break;
    default:
        transition(current, 0);
        break;
    }

    if (current == 8) {
        agent.temroid_state_timer = agent.temroid_drain_damage_timer;
    }
}
} // namespace fruityprime::gameplay

namespace fruityprime::enemy {

TemroidProfile decode_temroid_profile(
    std::uint8_t id, std::span<const std::uint8_t> fields,
    net::Vec3 origin) noexcept {
    TemroidProfile result;
    if (id != static_cast<std::uint8_t>(formats::EnemyType::Temroid)
        || fields.size() < 128) {
        return result;
    }
    result.hurt_volume = detail::read_volume(fields, 0, origin);
    result.facing = detail::read_vector(fields, 92);
    result.position_offset = detail::read_vector(fields, 104);
    result.idle_range = detail::read_vector(fields, 116);
    result.supported = result.hurt_volume.kind != scene::VolumeKind::Invalid;
    return result;
}

} // namespace fruityprime::enemy
