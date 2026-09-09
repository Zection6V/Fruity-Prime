#include "enemy_decode_common.hpp"
// Native port of src/MphRead/Entities/Enemies/23_PsychoBit.cs.
// This member definition lives in its enemy module; Session only dispatches it.
#include "23_PsychoBit.hpp"
#include "enemy_common.hpp"

namespace fruityprime::gameplay {

void Session::update_psychobit(EnemyState& agent) {
    const auto& profile = agent.psychobit;
    if (!profile.supported) {
        return;
    }

    const float seconds = config_.tick_seconds;
    const float frames = std::max(1.0F, std::round(
        std::max(0.0F, seconds * 60.0F)));
    const auto frame_step = static_cast<std::uint32_t>(frames);
    agent.attack_timer = std::max(0.0F, agent.attack_timer - seconds);

    const auto decrement = [frame_step](std::uint32_t& value) noexcept {
        value = value > frame_step ? value - frame_step : 0;
    };
    const auto random_distance = [this](float limit) noexcept {
        if (limit <= 0.0F) {
            return 0.0F;
        }
        const auto fixed_limit = static_cast<std::uint32_t>(std::max(
            1.0F, limit * 4096.0F));
        return static_cast<float>(rng_.random2(fixed_limit)) / 4096.0F;
    };
    const auto choose_roam_target = [&]() {
        const scene::VolumePoint center = profile.home_volume.center();
        net::Vec3 target{center.x, center.y, center.z};
        if (profile.home_volume.kind == scene::VolumeKind::Cylinder) {
            const auto& volume = profile.home_volume;
            const float radius = random_distance(volume.cylinder_radius);
            agent.psychobit_roam_angle_sign =
                agent.psychobit_roam_angle_sign == 0
                ? static_cast<std::int8_t>(1)
                : static_cast<std::int8_t>(
                    -agent.psychobit_roam_angle_sign);
            const float angle = static_cast<float>(rng_.random2(0xB4000u))
                / 4096.0F * agent.psychobit_roam_angle_sign
                * 3.14159265358979323846F / 180.0F;
            target.x = volume.cylinder_position.x
                + std::sin(angle) * radius;
            target.z = volume.cylinder_position.z
                + std::cos(angle) * radius;
            // Enemy23Entity's S06 home volumes are vertical cylinders. Keep
            // the generic axis fallback at the authored center for malformed
            // custom rooms rather than allowing a NaN/empty movement vector.
            if (std::abs(volume.cylinder_vector.y) > 0.5F) {
                target.y = volume.cylinder_position.y
                    + random_distance(std::abs(volume.cylinder_dot));
            }
        } else if (profile.home_volume.kind == scene::VolumeKind::Box) {
            const auto& volume = profile.home_volume;
            const float d1 = random_distance(volume.box_dot1);
            const float d2 = random_distance(volume.box_dot2);
            const float d3 = random_distance(volume.box_dot3);
            target = {
                volume.box_position.x + volume.box_vector1.x * d1
                    + volume.box_vector2.x * d2
                    + volume.box_vector3.x * d3,
                volume.box_position.y + volume.box_vector1.y * d1
                    + volume.box_vector2.y * d2
                    + volume.box_vector3.y * d3,
                volume.box_position.z + volume.box_vector1.z * d1
                    + volume.box_vector2.z * d2
                    + volume.box_vector3.z * d3};
        }
        agent.psychobit_move_target = target;
        agent.psychobit_move_target_valid = true;
        agent.psychobit_speed_factor = profile.min_speed_factor1;
    };

    if (!agent.psychobit_move_target_valid
        || distance_squared(agent.position, agent.psychobit_move_target)
            < 0.25F) {
        choose_roam_target();
    }

    // Enemy23Entity calls ContactDamagePlayer before its state machine, so a
    // PsychoBit can still hurt a player even while it is turning or charging.
    if (agent.attack_timer <= 0.0F) {
        for (auto& player : players_) {
            if (!objective_player(player)
                || distance_squared(player.position, agent.position)
                    > 1.25F * 1.25F) {
                continue;
            }
            apply_enemy_contact_damage(agent, player,
                                       profile.contact_damage);
            const net::Vec3 away = normalized_or(
                subtract(player.position, agent.position), {});
            player.speed = add(player.speed, multiply(away, 2.0F));
            agent.attack_timer = 0.5F;
            break;
        }
    }

    std::size_t target_index = players_.size();
    float target_distance = std::numeric_limits<float>::max();
    for (std::size_t index = 0; index < players_.size(); ++index) {
        const auto& player = players_[index];
        if (!objective_player(player)
            || !profile.range_volume.contains(to_volume_point(
                player.position))) {
            continue;
        }
        const float distance = distance_squared(player.position,
                                                agent.position);
        if (distance < target_distance) {
            target_distance = distance;
            target_index = index;
        }
    }

    if (target_index != players_.size()) {
        auto& target = players_[target_index];
        const net::Vec3 desired = normalized_or(
            subtract(add(target.position, {0.0F, 0.5F, 0.0F}),
                     agent.position), agent.facing);
        if (dot(agent.facing, desired) >= profile.range_max_cosine) {
            agent.target_slot = target.slot_index;
            agent.state = 3;
            agent.velocity = {};
            const float blend = std::clamp(frames / std::max(
                1.0F, static_cast<float>(profile.aim_steps * 2)),
                0.0F, 1.0F);
            agent.facing = normalized_or(
                add(multiply(agent.facing, 1.0F - blend),
                    multiply(desired, blend)), agent.facing);

            if (agent.psychobit_delay_timer > 0) {
                decrement(agent.psychobit_delay_timer);
                return;
            }
            if (agent.psychobit_shots_remaining == 0) {
                const auto range = static_cast<std::uint32_t>(
                    profile.max_shots >= profile.min_shots
                    ? profile.max_shots - profile.min_shots + 1u : 1u);
                agent.psychobit_shots_remaining =
                    static_cast<std::uint16_t>(
                        profile.min_shots + rng_.random2(range));
                agent.psychobit_shot_timer = static_cast<std::uint32_t>(
                    profile.shot_frames) * 2u;
            }
            if (agent.psychobit_shot_timer > 0) {
                decrement(agent.psychobit_shot_timer);
                return;
            }

            spawn_enemy_projectile(
                agent, add(agent.position, multiply(agent.facing, 0.5F)),
                agent.facing);
            --agent.psychobit_shots_remaining;
            agent.psychobit_shot_timer = static_cast<std::uint32_t>(
                profile.shot_frames) * 2u;
            if (agent.psychobit_shots_remaining == 0) {
                agent.psychobit_delay_timer = static_cast<std::uint32_t>(
                    profile.delay_frames) * 2u;
            }
            return;
        }
    }

    agent.target_slot = 0xff;
    agent.state = 0;
    const net::Vec3 to_target = subtract(agent.psychobit_move_target,
                                         agent.position);
    const float remaining = std::sqrt(std::max(0.0F,
        length_squared(to_target)));
    const net::Vec3 desired = normalized_or(to_target, agent.facing);
    const float blend = std::clamp(frames / std::max(
        1.0F, static_cast<float>(profile.aim_steps * 2)), 0.0F, 1.0F);
    agent.facing = normalized_or(
        add(multiply(agent.facing, 1.0F - blend),
            multiply(desired, blend)), agent.facing);
    if (remaining > 1.0F) {
        agent.psychobit_speed_factor = std::min(
            profile.max_speed_factor1,
            agent.psychobit_speed_factor
                + profile.speed_increment1 * frames);
    } else {
        agent.psychobit_speed_factor = std::max(
            profile.min_speed_factor1,
            agent.psychobit_speed_factor
                - profile.speed_increment1 * frames);
    }
    agent.velocity = multiply(agent.facing,
                              agent.psychobit_speed_factor * 60.0F);
    const net::Vec3 next = add(agent.position,
                               multiply(agent.velocity, seconds));
    if (!profile.home_volume.contains(to_volume_point(next))) {
        choose_roam_target();
        agent.velocity = {};
        return;
    }
    agent.position = next;
    if (distance_squared(agent.position, agent.psychobit_move_target)
            < 0.25F) {
        agent.psychobit_move_target_valid = false;
    }
}
} // namespace fruityprime::gameplay

namespace fruityprime::enemy {

PsychoBitProfile decode_psychobit_profile(
    std::uint8_t id, std::span<const std::uint8_t> fields,
    net::Vec3 origin) noexcept {
    PsychoBitProfile result;
    if (id != static_cast<std::uint8_t>(formats::EnemyType::PsychoBit1)
        || fields.size() < 8 + 4 * 64) {
        return result;
    }

    result.variant = static_cast<std::uint8_t>(std::min<std::uint32_t>(
        detail::read_u32(fields, 0), 4u));
    result.version = std::min<std::uint32_t>(detail::read_u32(fields, 4), 10u);
    result.hurt_volume = detail::read_volume(fields, 8, origin);
    result.home_volume = detail::read_volume(fields, 72, origin);
    result.range_volume = detail::read_volume(fields, 200, origin);

    struct Values {
        std::uint16_t health;
        std::uint16_t beam_damage;
        std::uint16_t splash_damage;
        std::uint16_t contact_damage;
        std::int32_t min_speed_factor1;
        std::int32_t max_speed_factor1;
        std::int32_t min_speed_factor2;
        std::int32_t max_speed_factor2;
        std::int32_t range_max_cosine;
        std::uint16_t delay_frames;
        std::uint16_t shot_frames;
        std::uint16_t min_shots;
        std::uint16_t max_shots;
        std::uint16_t double_speed_steps;
        std::uint16_t aim_steps;
        std::uint16_t speed_steps;
    };
    // Metadata.Enemy23Values, including the values assigned by the managed
    // constructor for SpeedSteps. The fixed-point conversion is deliberately
    // performed once at the cartridge boundary.
    static constexpr std::array<Values, 5> values{{
        {11, 1, 0, 10, 409, 918, 1638, 2252, -4096,
         40, 15, 1, 2, 50, 4, 26},
        {24, 2, 1, 10, 614, 1228, 2867, 3686, -4096,
         25, 8, 1, 3, 60, 4, 30},
        {120, 10, 2, 10, 614, 1228, 2457, 3276, -4096,
         25, 40, 1, 1, 60, 4, 30},
        {120, 8, 2, 10, 614, 1228, 1638, 2048, -4096,
         25, 30, 1, 2, 20, 30, 10},
        {120, 8, 0, 10, 614, 1228, 1638, 2048, -4096,
         25, 40, 1, 1, 20, 30, 10}
    }};
    const Values& selected = values[result.variant];
    result.health = selected.health;
    result.beam_damage = selected.beam_damage;
    result.splash_damage = selected.splash_damage;
    result.contact_damage = selected.contact_damage;
    result.min_speed_factor1 = static_cast<float>(
        selected.min_speed_factor1) / 4096.0F / 2.0F;
    result.max_speed_factor1 = static_cast<float>(
        selected.max_speed_factor1) / 4096.0F / 2.0F;
    result.min_speed_factor2 = static_cast<float>(
        selected.min_speed_factor2) / 4096.0F / 2.0F;
    result.max_speed_factor2 = static_cast<float>(
        selected.max_speed_factor2) / 4096.0F / 2.0F;
    const float speed_steps = static_cast<float>(
        std::max<std::uint16_t>(1, selected.speed_steps));
    result.speed_increment1 = (result.max_speed_factor1
                               - result.min_speed_factor1)
        / speed_steps / 2.0F;
    result.speed_increment2 = (result.max_speed_factor2
                               - result.min_speed_factor2)
        / speed_steps / 2.0F;
    result.range_max_cosine = static_cast<float>(
        selected.range_max_cosine) / 4096.0F;
    result.delay_frames = selected.delay_frames;
    result.shot_frames = selected.shot_frames;
    result.min_shots = selected.min_shots;
    result.max_shots = selected.max_shots;
    result.aim_steps = selected.aim_steps;
    result.speed_steps = selected.speed_steps;

    result.projectile_weapon = metadata::native_weapon_slot_from_beam(
        static_cast<std::int32_t>(result.version));
    struct ProjectileValues {
        std::uint8_t draw_func;
        std::uint16_t color;
        std::uint8_t collision_effect;
        std::uint8_t muzzle_effect;
        float speed;
        float lifetime;
    };
    static constexpr std::array<ProjectileValues, 3> projectile_values{{
        {21, 9055, 242, 65, 2662.0F / 4096.0F * 60.0F,
         255.0F / 60.0F},
        {2, 32767, 89, 60, 3276.0F / 4096.0F * 60.0F,
         90.0F / 60.0F},
        {7, 32140, 8, 65, 819.0F / 4096.0F * 60.0F,
         255.0F / 60.0F}
    }};
    if (result.version < projectile_values.size()) {
        const auto& projectile = projectile_values[result.version];
        result.projectile_draw_func = projectile.draw_func;
        result.projectile_color = projectile.color;
        result.projectile_collision_effect = projectile.collision_effect;
        result.projectile_muzzle_effect = projectile.muzzle_effect;
        result.projectile_speed = projectile.speed;
        result.projectile_lifetime = projectile.lifetime;
    } else {
        if (result.projectile_weapon == 0xff) {
            result.projectile_weapon = 0;
        }
        const auto& visual = metadata::weapon_visual_info(
            result.projectile_weapon);
        result.projectile_draw_func = visual.draw_func_ids[0];
        result.projectile_color = visual.colors[0];
        result.projectile_collision_effect = visual.collision_effects[0];
        result.projectile_muzzle_effect = visual.muzzle_effects[0];
        result.projectile_speed = metadata::weapon_info(
            result.projectile_weapon).projectile_speed;
        result.projectile_lifetime = metadata::weapon_info(
            result.projectile_weapon).lifetime_seconds;
    }
    result.supported = result.hurt_volume.kind != scene::VolumeKind::Invalid
        && result.home_volume.kind != scene::VolumeKind::Invalid
        && result.range_volume.kind != scene::VolumeKind::Invalid;
    return result;
}

} // namespace fruityprime::enemy

