#include "enemy_decode_common.hpp"
// Native port of src/MphRead/Entities/Enemies/46_LesserIthrak.cs.
// This member definition lives in its enemy module; Session only dispatches it.
#include "46_LesserIthrak.hpp"
#include "enemy_common.hpp"

namespace fruityprime::gameplay {

void Session::update_ithrak(EnemyState& agent) {
    const auto& profile = agent.ithrak;
    if (!profile.supported) {
        static_cast<void>(update_generic_enemy(agent));
        return;
    }

    // Enemy46Entity is driven by a twenty-entry subroutine table.  Preserve
    // those state IDs and the timers that the table observes instead of
    // replacing it with the generic chase controller used by simple enemies.
    constexpr std::uint8_t StateIdle = 0;
    constexpr std::uint8_t StateWarn = 1;
    constexpr std::uint8_t StateDrop = 2;
    constexpr std::uint8_t StateCombat = 3;
    constexpr std::uint8_t StateTurnAfterDrop = 4;
    constexpr std::uint8_t StateAttackWindup = 5;
    constexpr std::uint8_t StateLunge = 6;
    constexpr std::uint8_t StateGrab = 7;
    constexpr std::uint8_t StateReposition = 8;
    constexpr std::uint8_t StateGrabRecover = 9;
    constexpr std::uint8_t StateCombatTurn = 10;
    constexpr std::uint8_t StateCombatAdvance = 11;
    constexpr std::uint8_t StateReturnStart = 12;
    constexpr std::uint8_t StateReturnTurn = 13;
    constexpr std::uint8_t StateReturn = 14;
    constexpr std::uint8_t StateReleaseTurn = 15;
    constexpr std::uint8_t StateRelease = 16;
    constexpr std::uint8_t StateReleaseRecover = 17;
    constexpr std::uint8_t StateReturnRecover = 18;
    constexpr std::uint8_t StateSurfaceRecover = 19;

    const float seconds = config_.tick_seconds;
    const auto frame_step = static_cast<std::uint32_t>(std::max(
        1.0F, std::round(std::max(0.0F, seconds * 60.0F))));
    const auto decrement = [frame_step](std::uint32_t& value) noexcept {
        value = value > frame_step ? value - frame_step : 0;
    };
    agent.attack_timer = std::max(0.0F, agent.attack_timer - seconds);
    if (agent.ithrak_animation_timer > 0) {
        decrement(agent.ithrak_animation_timer);
    }

    const auto set_animation = [&agent](std::uint8_t animation,
                                        std::uint32_t duration) {
        agent.ithrak_animation = animation;
        agent.ithrak_animation_timer = duration;
    };
    const auto set_state = [&agent](std::uint8_t state) {
        agent.state = state;
        agent.ithrak_wall_collision = false;
    };
    const auto horizontal = [](net::Vec3 value) noexcept {
        value.y = 0.0F;
        return value;
    };
    const auto face = [&agent, &horizontal](net::Vec3 value) noexcept {
        agent.facing = normalized_or(horizontal(value), agent.facing);
    };
    const auto begin_turn = [&agent, &horizontal](net::Vec3 value) {
        agent.ithrak_target_vec = normalized_or(
            horizontal(value), agent.facing);
        const float cosine = std::clamp(
            dot(horizontal(agent.facing), agent.ithrak_target_vec),
            -1.0F, 1.0F);
        agent.ithrak_aim_angle_step = std::acos(cosine)
            * 180.0F / 3.14159265358979323846F / 20.0F;
        agent.ithrak_step_count = 20;
    };
    const auto seek_turn = [&agent, &face, frame_step]() {
        if (agent.ithrak_step_count > 0) {
            const float total = static_cast<float>(
                agent.ithrak_step_count);
            const float blend = std::clamp(
                static_cast<float>(frame_step) / std::max(1.0F, total),
                0.0F, 1.0F);
            agent.facing = normalized_or(
                add(multiply(agent.facing, 1.0F - blend),
                    multiply(agent.ithrak_target_vec, blend)),
                agent.facing);
            agent.ithrak_step_count = agent.ithrak_step_count > frame_step
                ? agent.ithrak_step_count - frame_step : 0;
            return false;
        }
        face(agent.ithrak_target_vec);
        return true;
    };

    const auto pick_move_target = [&]() {
        agent.ithrak_move_start = agent.position;
        const auto& volume = profile.home_volume;
        net::Vec3 target = to_net(volume.center());
        if (volume.kind == scene::VolumeKind::Cylinder
            || volume.kind == scene::VolumeKind::Sphere) {
            const float radius_limit = volume.kind
                == scene::VolumeKind::Cylinder
                ? volume.cylinder_radius : volume.sphere_radius;
            const auto fixed_limit = static_cast<std::uint32_t>(std::max(
                1.0F, radius_limit * 4096.0F));
            const float radius = static_cast<float>(rng_.random2(
                fixed_limit)) / 4096.0F;
            agent.ithrak_drop_angle_sign =
                agent.ithrak_drop_angle_sign == 0
                ? static_cast<std::int8_t>(1)
                : static_cast<std::int8_t>(
                    -agent.ithrak_drop_angle_sign);
            const float angle = static_cast<float>(rng_.random2(0xB4000u))
                / 4096.0F * static_cast<float>(
                    agent.ithrak_drop_angle_sign)
                * 3.14159265358979323846F / 180.0F;
            const net::Vec3 center = volume.kind
                == scene::VolumeKind::Cylinder
                ? to_net(volume.cylinder_position)
                : to_net(volume.sphere_position);
            target = {center.x + std::sin(angle) * radius,
                      agent.position.y - 20.0F,
                      center.z + std::cos(angle) * radius};
        } else if (volume.kind == scene::VolumeKind::Box) {
            const auto fixed = [](float value) {
                return static_cast<std::uint32_t>(std::max(
                    1.0F, value * 4096.0F));
            };
            const float distance1 = static_cast<float>(rng_.random2(
                fixed(volume.box_dot1))) / 4096.0F;
            const float distance3 = static_cast<float>(rng_.random2(
                fixed(volume.box_dot3))) / 4096.0F;
            target = to_net(volume.box_position);
            const auto vector1 = to_net(volume.box_vector1);
            const auto vector3 = to_net(volume.box_vector3);
            target = add(target, add(multiply(vector1, distance1),
                                     multiply(vector3, distance3)));
            target.y = agent.position.y - 20.0F;
        } else {
            target.y = agent.position.y - 20.0F;
        }

        const auto hit = collision::sweep_sphere(
            room_.collision(), to_collision(agent.position),
            to_collision(target), agent.body_radius, 0x2000);
        if (hit.has_value()) {
            target.y = hit->center.y;
        } else {
            target.y = agent.position.y;
        }
        agent.ithrak_move_target = target;
        const net::Vec3 travel = horizontal(subtract(
            target, agent.position));
        agent.ithrak_move_distance_squared = length_squared(travel);
    };
    const auto start_step_move = [&agent, seconds](net::Vec3 target,
                                                    float step) {
        const net::Vec3 travel = subtract(target, agent.position);
        const float distance = std::sqrt(std::max(0.0F,
            length_squared(travel)));
        agent.ithrak_move_target = target;
        if (distance <= 0.0001F || seconds <= 0.0F) {
            agent.velocity = {};
            agent.ithrak_step_count = 0;
            return;
        }
        agent.ithrak_step_count = static_cast<std::uint32_t>(
            distance / step) + 1u;
        agent.velocity = multiply(travel, step / distance / seconds);
    };
    const auto can_recoil = [this, &agent]() {
        const net::Vec3 destination = subtract(
            agent.position, multiply(agent.facing, 2.0F));
        return !collision::sweep_sphere(
            room_.collision(), to_collision(agent.position),
            to_collision(destination), agent.body_radius, 0x2000)
            .has_value();
    };
    const auto start_recoil = [&agent, this]() {
        agent.ithrak_recoil_angle_sign =
            agent.ithrak_recoil_angle_sign == 0
            ? static_cast<std::int8_t>(1)
            : static_cast<std::int8_t>(
                -agent.ithrak_recoil_angle_sign);
        const float angle = (30.0F + static_cast<float>(rng_.random2(
            0x1E000u)) / 4096.0F) * static_cast<float>(
                agent.ithrak_recoil_angle_sign);
        const net::Vec3 direction = rotate_about_axis(
            multiply(agent.facing, -500.0F / 4096.0F / 2.0F),
            agent.up, angle);
        agent.velocity = {direction.x * 60.0F,
                          1000.0F / 4096.0F / 2.0F * 60.0F,
                          direction.z * 60.0F};
        agent.ithrak_step_count = 14;
        agent.ithrak_ground_collision = false;
    };
    const auto blocking_motion = [&]() {
        agent.ithrak_ground_collision = false;
        agent.ithrak_wall_collision = false;
        if (seconds <= 0.0F) {
            return;
        }
        const net::Vec3 next = add(agent.position,
                                   multiply(agent.velocity, seconds));
        if (length_squared(subtract(next, agent.position)) <= 0.000001F) {
            return;
        }
        const auto hit = collision::sweep_sphere(
            room_.collision(), to_collision(agent.position),
            to_collision(next), agent.body_radius, 0x2000);
        if (!hit.has_value()) {
            agent.position = next;
            return;
        }
        agent.position = {hit->center.x + hit->normal.x * 0.001F,
                          hit->center.y + hit->normal.y * 0.001F,
                          hit->center.z + hit->normal.z * 0.001F};
        const net::Vec3 normal{hit->normal.x, hit->normal.y,
                               hit->normal.z};
        agent.ithrak_ground_collision = std::abs(normal.y) >= 0.1F;
        agent.ithrak_wall_collision = !agent.ithrak_ground_collision;
        const float into = dot(agent.velocity, normal);
        if (into < 0.0F) {
            agent.velocity = subtract(agent.velocity,
                                      multiply(normal, into));
        }
    };
    const auto probe_surface = [&]() {
        const net::Vec3 start = add(agent.position, {0.0F, 0.5F, 0.0F});
        const net::Vec3 end = add(agent.position, {0.0F, -0.5F, 0.0F});
        const auto hit = collision::sweep_sphere(
            room_.collision(), to_collision(start), to_collision(end),
            agent.body_radius, 0x2000);
        if (!hit.has_value()) {
            return false;
        }
        agent.position = {hit->center.x + hit->normal.x * 0.001F,
                          hit->center.y + hit->normal.y * 0.001F,
                          hit->center.z + hit->normal.z * 0.001F};
        agent.ithrak_ground_collision = std::abs(hit->normal.y) >= 0.1F;
        return true;
    };
    const auto apply_contact = [&]() {
        if (agent.attack_timer > 0.0F) {
            return false;
        }
        for (auto& player : players_) {
            if (!objective_player(player)
                || distance_squared(player.position, agent.position)
                    > 1.25F * 1.25F) {
                continue;
            }
            apply_enemy_contact_damage(agent, player,
                                       profile.contact_damage);
            const net::Vec3 away = normalized_or(
                horizontal(subtract(player.position, agent.position)),
                {});
            player.speed = add(player.speed, multiply(away, 0.2F));
            agent.attack_timer = 0.5F;
            return true;
        }
        return false;
    };

    std::size_t any_target = players_.size();
    std::size_t range_target = players_.size();
    float nearest_any = std::numeric_limits<float>::max();
    float nearest_range = std::numeric_limits<float>::max();
    for (std::size_t index = 0; index < players_.size(); ++index) {
        const auto& player = players_[index];
        if (!objective_player(player)) {
            continue;
        }
        const float distance = distance_squared(player.position,
                                                agent.position);
        if (distance < nearest_any) {
            nearest_any = distance;
            any_target = index;
        }
        if (profile.range_volume.contains(to_volume_point(player.position))
            && distance < nearest_range) {
            nearest_range = distance;
            range_target = index;
        }
    }
    const auto target_position = [&]() {
        return any_target == players_.size()
            ? add(agent.position, agent.facing)
            : players_[any_target].position;
    };
    const bool in_warn_volume = any_target != players_.size()
        && profile.warn_volume.contains(to_volume_point(
            players_[any_target].position));
    const bool in_range = range_target != players_.size();

    // DoMovement runs before EnemyProcess in the managed class.  Apply the
    // previous state's velocity first, then execute the current subroutine.
    if (agent.state != StateGrab && agent.state != StateSurfaceRecover) {
        blocking_motion();
    } else if (seconds > 0.0F) {
        agent.position = add(agent.position,
                             multiply(agent.velocity, seconds));
    }

    // EnemyProcess applies the gravity term after CallStateProcess for every
    // state other than 0/1/2. The native record stores velocity per second.
    const auto apply_gravity = [&agent]() {
        agent.velocity.y -= 100.0F / 4096.0F / 4.0F * 60.0F;
    };

    switch (agent.state) {
    case StateIdle:
        agent.target_slot = 0xff;
        agent.velocity = {};
        if (in_warn_volume) {
            agent.target_slot = players_[any_target].slot_index;
            set_animation(6, 15u * 2u);
            set_state(StateWarn);
        }
        break;
    case StateWarn:
        agent.target_slot = in_warn_volume
            ? players_[any_target].slot_index : 0xff;
        if (!in_warn_volume) {
            agent.velocity = {};
            set_state(StateIdle);
        } else if (in_range) {
            pick_move_target();
            const net::Vec3 travel = subtract(
                agent.ithrak_move_target, agent.position);
            start_step_move(agent.ithrak_move_target, 0.25F);
            set_animation(12, 30u * 2u);
            agent.ithrak_step_count = static_cast<std::uint32_t>(
                std::sqrt(std::max(0.0F, length_squared(travel))) / 0.25F)
                + 1u;
            set_state(StateDrop);
        }
        break;
    case StateDrop:
        if (agent.ithrak_ground_collision || probe_surface()) {
            agent.ithrak_move_target.y = agent.position.y;
            agent.velocity.x = 0.0F;
            agent.velocity.z = 0.0F;
            agent.ithrak_step_count = 60u * 2u;
            set_animation(14, 60u * 2u);
            set_state(StateTurnAfterDrop);
        }
        break;
    case StateCombat:
        if (any_target != players_.size()) {
            face(subtract(target_position(), agent.position));
        }
        if (any_target != players_.size()
            && distance_squared(agent.position,
                                target_position()) <= 1.5F * 1.5F
            && can_recoil()) {
            start_recoil();
            set_animation(7, 30u * 2u);
            set_state(StateSurfaceRecover);
        } else if (any_target != players_.size()) {
            const float distance = std::sqrt(std::max(0.0F,
                distance_squared(agent.position, target_position())));
            if (distance > 1.5F && distance < 2.0F) {
                agent.velocity = {};
                agent.ithrak_delay_timer = 15u * 2u;
                set_state(StateReposition);
            } else if (distance > 3.5F && distance < 5.0F) {
                agent.velocity = {};
                agent.ithrak_step_count = 38u * 2u;
                set_animation(1, 38u * 2u);
                set_state(StateAttackWindup);
            } else if (!(profile.range_volume.contains(to_volume_point(
                           agent.position)) && in_range)) {
                agent.velocity = {};
                set_animation(14, 30u * 2u);
                set_state(StateReturnStart);
            }
        }
        break;
    case StateTurnAfterDrop:
        if (agent.ithrak_step_count > 0) {
            decrement(agent.ithrak_step_count);
        } else {
            begin_turn(subtract(target_position(), agent.position));
            set_animation(13, 20u);
            agent.velocity = multiply(agent.facing,
                                      0.15F / 2.0F * 60.0F);
            agent.ithrak_step_count = 3u * 2u;
            set_state(StateCombat);
        }
        break;
    case StateAttackWindup:
        if (any_target != players_.size()) {
            face(subtract(target_position(), agent.position));
        }
        if (agent.ithrak_step_count > 0) {
            decrement(agent.ithrak_step_count);
        } else {
            set_animation(8, 12u * 2u);
            agent.ithrak_step_count = 12u * 2u;
            set_state(StateLunge);
        }
        break;
    case StateLunge:
        if (any_target != players_.size()) {
            face(subtract(target_position(), agent.position));
        }
        if (any_target != players_.size()
            && distance_squared(agent.position,
                                target_position()) <= 1.5F * 1.5F
            && can_recoil()) {
            start_recoil();
            set_state(StateSurfaceRecover);
        } else if (any_target != players_.size()
                   && distance_squared(agent.position,
                                      target_position()) > 1.5F * 1.5F
                   && distance_squared(agent.position,
                                      target_position()) < 2.0F * 2.0F) {
            agent.ithrak_delay_timer = 15u * 2u;
            set_state(StateReposition);
        } else if (agent.ithrak_step_count > 0) {
            decrement(agent.ithrak_step_count);
        } else {
            agent.velocity = add(multiply(agent.facing,
                                          1800.0F / 4096.0F / 2.0F * 60.0F),
                                 {0.0F, 600.0F / 4096.0F / 2.0F * 60.0F,
                                  0.0F});
            agent.ithrak_step_count = 7u * 2u;
            set_state(StateGrab);
        }
        break;
    case StateGrab:
        if (agent.ithrak_step_count > 0) {
            decrement(agent.ithrak_step_count);
            break;
        }
        if (probe_surface()) {
            set_animation(10, 30u * 2u);
            agent.velocity = multiply(agent.facing,
                                      0.18F / 2.0F * 60.0F);
            agent.ithrak_acceleration = multiply(agent.velocity,
                                                 1.0F / (10.0F * 2.0F));
            set_state(StateGrabRecover);
        } else if (any_target != players_.size()
                   && distance_squared(agent.position,
                                      target_position()) <= 1.5F * 1.5F) {
            agent.velocity = {};
            apply_enemy_contact_damage(agent, players_[any_target], 15);
            set_animation(9, 30u * 2u);
            set_state(StateReposition);
        }
        break;
    case StateReposition:
        if (any_target != players_.size()) {
            face(subtract(target_position(), agent.position));
        }
        if (any_target != players_.size()
            && distance_squared(agent.position,
                                target_position()) <= 1.5F * 1.5F
            && can_recoil()) {
            start_recoil();
            set_state(StateSurfaceRecover);
        } else if (agent.ithrak_delay_timer == 0
                   && any_target != players_.size()
                   && distance_squared(agent.position,
                                      target_position()) > 2.0F * 2.0F) {
            agent.velocity = multiply(agent.facing,
                                      0.15F / 2.0F * 60.0F);
            agent.ithrak_delay_timer = 30u * 2u;
            set_state(StateCombat);
        } else if (agent.ithrak_delay_timer >= 30u * 2u
                   && rng_.random2(0x64000u) < 1024u) {
            start_recoil();
            set_state(StateSurfaceRecover);
        }
        break;
    case StateGrabRecover:
        if (agent.ithrak_animation_timer == 0) {
            begin_turn(subtract(target_position(), agent.position));
            set_state(StateCombatTurn);
        }
        break;
    case StateCombatTurn:
        if (seek_turn()) {
            agent.velocity = multiply(agent.facing,
                                      0.15F / 2.0F * 60.0F);
            set_state(StateCombatAdvance);
        }
        break;
    case StateCombatAdvance:
        if (seek_turn()) {
            set_state(StateCombat);
        }
        break;
    case StateReturnStart:
        if (agent.ithrak_animation_timer == 0) {
            set_animation(16, 1);
            pick_move_target();
            begin_turn(subtract(agent.ithrak_move_target,
                                agent.position));
            agent.ithrak_reaching_target = false;
            agent.ithrak_step_count = 20;
            set_state(StateReturnTurn);
        }
        break;
    case StateReturnTurn:
        if (seek_turn()) {
            start_step_move(agent.ithrak_move_target, 0.25F);
            agent.ithrak_step_count = static_cast<std::uint32_t>(
                std::sqrt(std::max(0.0F,
                    agent.ithrak_move_distance_squared)) / 0.25F) + 1u;
            set_state(StateReturn);
        }
        break;
    case StateReturn:
        if (!agent.ithrak_reaching_target
            && distance_squared(agent.position,
                                agent.ithrak_move_target) <= 0.25F * 0.25F) {
            agent.ithrak_reaching_target = true;
            agent.velocity = {};
        }
        if (agent.ithrak_reaching_target) {
            pick_move_target();
            begin_turn(subtract(agent.ithrak_move_target,
                                agent.position));
            agent.ithrak_reaching_target = false;
            agent.ithrak_step_count = 20;
            set_state(StateReleaseTurn);
        } else if (profile.range_volume.contains(to_volume_point(
                       agent.position)) && in_range) {
            begin_turn(subtract(target_position(), agent.position));
            set_state(StateCombatAdvance);
        } else if (agent.ithrak_move_timer > 0) {
            decrement(agent.ithrak_move_timer);
        } else {
            begin_turn(subtract(target_position(), agent.position));
            agent.ithrak_move_timer = 600u * 2u;
            set_state(StateRelease);
        }
        break;
    case StateReleaseTurn:
        if (seek_turn()) {
            set_animation(14, 50u * 2u);
            agent.ithrak_step_count = 50u * 2u;
            set_state(StateReleaseRecover);
        }
        break;
    case StateRelease:
        if (seek_turn()) {
            set_animation(14, 50u * 2u);
            agent.ithrak_step_count = 50u * 2u;
            set_state(StateReleaseRecover);
        }
        break;
    case StateReleaseRecover:
        if (agent.ithrak_animation_timer == 0) {
            pick_move_target();
            start_step_move(agent.ithrak_move_target, 0.25F);
            set_state(StateReturn);
        }
        break;
    case StateReturnRecover:
        if (agent.ithrak_animation_timer == 0) {
            start_step_move(agent.ithrak_move_target, 0.25F);
            set_state(StateReturn);
        } else if (profile.range_volume.contains(to_volume_point(
                       agent.position)) && in_range) {
            begin_turn(subtract(target_position(), agent.position));
            set_state(StateCombatAdvance);
        }
        break;
    case StateSurfaceRecover:
        if (agent.ithrak_step_count > 0) {
            decrement(agent.ithrak_step_count);
        } else if (probe_surface()) {
            set_animation(13, 20u);
            agent.velocity = multiply(agent.facing,
                                      0.15F / 2.0F * 60.0F);
            set_state(StateCombat);
        }
        break;
    default:
        set_state(StateIdle);
        agent.velocity = {};
        break;
    }

    if (agent.state != StateIdle && agent.state != StateWarn
        && agent.state != StateDrop && !agent.ithrak_ground_collision) {
        apply_gravity();
    }
    if (agent.state != StateGrab && agent.state != StateReposition) {
        static_cast<void>(apply_contact());
    }
}

void Session::update_lesser_ithrak(EnemyState& agent) {
    update_ithrak(agent);
}
} // namespace fruityprime::gameplay

namespace fruityprime::enemy {

IthrakProfile decode_ithrak_profile(
    std::uint8_t id, std::span<const std::uint8_t> fields,
    net::Vec3 origin) noexcept {
    IthrakProfile result;
    const auto lesser = static_cast<std::uint8_t>(
        formats::EnemyType::LesserIthrak);
    const auto greater = static_cast<std::uint8_t>(
        formats::EnemyType::GreaterIthrak);
    if (id != lesser && id != greater) {
        return result;
    }
    // Enemy46 passes S00.Volume0, Volume2, Volume1, Volume3 to Setup
    // (hurt, home, range, warning). Enemy47 passes S05.Volume0..3 in order.
    // The two managed constructors intentionally differ; do not normalize
    // them into one sequential layout here.
    const std::size_t base = id == greater ? 4 : 0;
    if (fields.size() < base + 4 * 64) {
        return result;
    }
    result.variant = id;
    result.hurt_volume = detail::read_volume(fields, base, origin);
    const std::size_t home_index = id == lesser ? 2 : 1;
    const std::size_t range_index = id == lesser ? 1 : 2;
    result.home_volume = detail::read_volume(
        fields, base + home_index * 64, origin);
    result.range_volume = detail::read_volume(
        fields, base + range_index * 64, origin);
    result.warn_volume = detail::read_volume(fields, base + 192, origin);
    result.supported = result.hurt_volume.kind != scene::VolumeKind::Invalid
        && result.home_volume.kind != scene::VolumeKind::Invalid
        && result.range_volume.kind != scene::VolumeKind::Invalid
        && result.warn_volume.kind != scene::VolumeKind::Invalid;
    return result;
}

} // namespace fruityprime::enemy
