#include "enemy_decode_common.hpp"
// Native port of src/MphRead/Entities/Enemies/35_Voldrum.cs.
// This member definition lives in its enemy module; Session only dispatches it.
#include "35_Voldrum.hpp"
#include "enemy_common.hpp"

namespace fruityprime::gameplay {

void Session::update_voldrum(EnemyState& agent) {
    const auto& profile = agent.voldrum;
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
    const auto choose_roam_target = [&]() {
        const scene::VolumePoint center = profile.home_volume.center();
        net::Vec3 target{center.x, agent.position.y, center.z};
        if (profile.home_volume.kind == scene::VolumeKind::Cylinder
            && profile.home_volume.cylinder_radius > 0.0F) {
            const auto radius_limit = static_cast<std::uint32_t>(std::max(
                1.0F, profile.home_volume.cylinder_radius * 4096.0F));
            const float radius = static_cast<float>(rng_.random2(
                radius_limit)) / 4096.0F;
            agent.voldrum_roam_angle_sign =
                agent.voldrum_roam_angle_sign == 0
                ? static_cast<std::int8_t>(1)
                : static_cast<std::int8_t>(-agent.voldrum_roam_angle_sign);
            const float angle = static_cast<float>(rng_.random2(0xB4000u))
                / 4096.0F * agent.voldrum_roam_angle_sign
                * 3.14159265358979323846F / 180.0F;
            target.x = profile.home_volume.cylinder_position.x
                + std::sin(angle) * radius;
            target.z = profile.home_volume.cylinder_position.z
                + std::cos(angle) * radius;
        }
        agent.voldrum_move_target = target;
        agent.voldrum_move_target_valid = true;
        agent.voldrum_speed_factor = profile.min_speed_factor;
    };

    if (!agent.voldrum_move_target_valid
        || distance_squared(agent.position, agent.voldrum_move_target)
            < 0.25F) {
        choose_roam_target();
    }

    std::size_t target_index = players_.size();
    float target_distance = std::numeric_limits<float>::max();
    const float detection_radius = profile.ranged ? 35.0F : 18.0F;
    for (std::size_t index = 0; index < players_.size(); ++index) {
        const auto& player = players_[index];
        if (!objective_player(player)) {
            continue;
        }
        const float distance = distance_squared(player.position,
                                                agent.position);
        if (distance <= detection_radius * detection_radius
            && distance < target_distance) {
            target_distance = distance;
            target_index = index;
        }
    }

    const auto apply_contact = [this, &agent](std::uint16_t damage,
                                                bool knockback) {
        if (agent.attack_timer > 0.0F) {
            return false;
        }
        for (auto& player : players_) {
            if (!objective_player(player)
                || distance_squared(player.position, agent.position)
                    > 1.25F * 1.25F) {
                continue;
            }
            apply_enemy_contact_damage(agent, player, damage);
            if (knockback) {
                const net::Vec3 away = normalized_or(
                    subtract(player.position, agent.position), {});
                player.speed = add(player.speed, multiply(away, 2.0F));
            }
            agent.attack_timer = 0.5F;
            return true;
        }
        return false;
    };

    if (profile.ranged && target_index != players_.size()) {
        auto& target = players_[target_index];
        const net::Vec3 to_target = subtract(
            add(target.position, {0.0F, 0.45F, 0.0F}), agent.position);
        const net::Vec3 desired = normalized_or(to_target, agent.facing);
        const float facing_dot = dot(agent.facing, desired);
        // Enemy36Entity uses the S06 range cosine for acquisition; the home
        // cylinder constrains roaming but is not an aim/attack gate.
        if (facing_dot >= profile.range_max_cosine) {
            agent.target_slot = target.slot_index;
            agent.state = 3;
            agent.velocity = {};
            // Enemy36Entity clamps vertical aim to a half-unit slope before
            // rebuilding its transform; the linear blend retains that
            // bounded movement without importing the managed matrix type.
            net::Vec3 aim = desired;
            aim.y = std::clamp(aim.y, -0.5F, 0.5F);
            aim = normalized_or(aim, agent.facing);
            const float blend = std::clamp(frames / 20.0F, 0.0F, 1.0F);
            agent.facing = normalized_or(
                add(multiply(agent.facing, 1.0F - blend),
                    multiply(aim, blend)), agent.facing);
            static_cast<void>(apply_contact(profile.contact_damage, true));

            if (agent.voldrum_delay_timer > 0) {
                decrement(agent.voldrum_delay_timer);
                return;
            }
            if (agent.voldrum_shots_remaining == 0) {
                const auto range = static_cast<std::uint32_t>(
                    profile.max_shots >= profile.min_shots
                    ? profile.max_shots - profile.min_shots + 1u : 1u);
                agent.voldrum_shots_remaining = static_cast<std::uint16_t>(
                    profile.min_shots + rng_.random2(range));
                agent.voldrum_shot_timer = static_cast<std::uint32_t>(
                    profile.shot_frames) * 2u;
            }
            if (agent.voldrum_shot_timer > 0) {
                decrement(agent.voldrum_shot_timer);
                return;
            }

            const net::Vec3 right = normalized_or(
                cross(agent.facing, agent.up), {-1.0F, 0.0F, 0.0F});
            spawn_enemy_projectile(agent,
                                   add(agent.position, multiply(right, -0.43F)),
                                   agent.facing);
            spawn_enemy_projectile(agent,
                                   add(agent.position, multiply(right, 0.43F)),
                                   agent.facing);
            --agent.voldrum_shots_remaining;
            agent.voldrum_shot_timer = static_cast<std::uint32_t>(
                profile.shot_frames) * 2u;
            if (agent.voldrum_shots_remaining == 0) {
                agent.voldrum_delay_timer = static_cast<std::uint32_t>(
                    profile.delay_frames) * 2u;
            }
            return;
        }
    }

    if (target_index != players_.size()) {
        auto& target = players_[target_index];
        const bool in_home = profile.home_volume.contains(
            to_volume_point(target.position));
        const float distance = std::sqrt(std::max(0.0F,
            distance_squared(target.position, agent.position)));
        if (!profile.ranged && in_home && distance <= 8.0F) {
            agent.target_slot = target.slot_index;
            agent.state = 2;
            agent.voldrum_move_target = target.position;
            agent.voldrum_move_target.y = agent.position.y;
            agent.voldrum_move_target_valid = true;
            if (distance <= 1.25F
                && apply_contact(15, true)) {
                agent.voldrum_move_target_valid = false;
                return;
            }
        } else {
            agent.target_slot = 0xff;
            agent.state = 0;
        }
    } else {
        agent.target_slot = 0xff;
        agent.state = 0;
    }

    net::Vec3 to_target = subtract(agent.voldrum_move_target,
                                   agent.position);
    to_target.y = 0.0F;
    if (length_squared(to_target) <= 0.0001F) {
        choose_roam_target();
        to_target = subtract(agent.voldrum_move_target, agent.position);
        to_target.y = 0.0F;
    }
    const net::Vec3 desired = normalized_or(to_target, agent.facing);
    const float blend = std::clamp(frames / std::max(
        1.0F, static_cast<float>(profile.aim_steps * 2)), 0.0F, 1.0F);
    agent.facing = normalized_or(
        add(multiply(agent.facing, 1.0F - blend),
            multiply(desired, blend)), agent.facing);
    const float remaining = std::sqrt(std::max(0.0F,
        length_squared(to_target)));
    if (remaining > 1.0F) {
        agent.voldrum_speed_factor = std::min(
            profile.max_speed_factor,
            agent.voldrum_speed_factor + profile.speed_increment * frames);
    } else {
        agent.voldrum_speed_factor = std::max(
            profile.min_speed_factor,
            agent.voldrum_speed_factor - profile.speed_increment * frames);
    }
    agent.velocity = multiply(agent.facing,
                              agent.voldrum_speed_factor * 60.0F);
    const net::Vec3 next = add(agent.position,
                               multiply(agent.velocity, seconds));
    if (!profile.home_volume.contains(to_volume_point(next))) {
        choose_roam_target();
        agent.velocity = {};
        return;
    }
    const auto hit = collision::sweep_sphere(
        room_.collision(), to_collision(agent.position), to_collision(next),
        agent.body_radius, 0x2000);
    if (hit.has_value()) {
        agent.position = {hit->center.x + hit->normal.x * 0.001F,
                          hit->center.y + hit->normal.y * 0.001F,
                          hit->center.z + hit->normal.z * 0.001F};
        agent.voldrum_move_target_valid = false;
        agent.velocity = {};
    } else {
        agent.position = next;
    }
    if (distance_squared(agent.position, agent.voldrum_move_target)
            < 0.25F) {
        agent.voldrum_move_target_valid = false;
    }
}

void Session::update_voldrum2(EnemyState& agent) {
    update_voldrum(agent);
}
} // namespace fruityprime::gameplay

namespace fruityprime::enemy {

VoldrumProfile decode_voldrum_profile(
    std::uint8_t id, std::span<const std::uint8_t> fields,
    net::Vec3 origin) noexcept {
    VoldrumProfile result;
    const auto voldrum2 = static_cast<std::uint8_t>(
        formats::EnemyType::Voldrum2);
    const auto voldrum1 = static_cast<std::uint8_t>(
        formats::EnemyType::Voldrum1);
    if (id == voldrum2) {
        if (fields.size() < 2 * 64) {
            return result;
        }
        result.variant = id;
        result.hurt_volume = detail::read_volume(fields, 0, origin);
        result.home_volume = detail::read_volume(fields, 64, origin);
        result.health = 42;
        result.contact_damage = 2;
        result.min_speed_factor = 0.1F / 2.0F;
        result.max_speed_factor = 0.2F / 2.0F;
        result.speed_increment = 410.0F / 4096.0F
            / 3.0F / 2.0F;
        result.jump_speed = 1000.0F / 4096.0F / 2.0F;
        result.aim_steps = 10;
        result.supported = result.hurt_volume.kind != scene::VolumeKind::Invalid
            && result.home_volume.kind != scene::VolumeKind::Invalid;
        return result;
    }
    if (id != voldrum1 || fields.size() < 2 * 64 + 8) {
        return result;
    }

    result.ranged = true;
    result.variant = static_cast<std::uint8_t>(std::min<std::uint32_t>(
        detail::read_u32(fields, 0), 4u));
    result.version = std::min<std::uint32_t>(detail::read_u32(fields, 4), 10u);
    // Enemy51Entity does not read a CollisionVolume from S07. It constructs a
    // fixed local sphere at (0, 409/4096, 0), then transforms it by the
    // spawner transform. Keep that authored detail instead of interpreting the
    // following bytes as a generic volume union.
    result.hurt_volume.kind = scene::VolumeKind::Sphere;
    result.hurt_volume.sphere_position = {
        origin.x, origin.y + 409.0F / 4096.0F, origin.z};
    result.hurt_volume.sphere_radius = 1843.0F / 4096.0F;
    result.home_volume = detail::read_volume(fields, 72, origin);

    struct Values {
        std::uint16_t health;
        std::uint16_t beam_damage;
        std::uint16_t splash_damage;
        std::uint16_t contact_damage;
        std::int32_t min_speed_factor;
        std::int32_t max_speed_factor;
        std::uint16_t delay_frames;
        std::uint16_t shot_frames;
        std::uint16_t min_shots;
        std::uint16_t max_shots;
        std::int32_t jump_speed;
        std::int32_t range_max_cosine;
        std::uint16_t aim_steps;
    };
    static constexpr std::array<Values, 5> values{{
        {55, 2, 0, 7, 819, 1433, 40, 15, 1, 2,
         819, -4096, 10},
        {100, 5, 1, 7, 819, 1433, 25, 8, 2, 3,
         819, -4096, 10},
        {150, 10, 2, 7, 819, 1433, 30, 25, 1, 1,
         819, -4096, 10},
        {150, 8, 2, 7, 1228, 2048, 25, 20, 1, 2,
         819, -4096, 10},
        {152, 8, 0, 7, 409, 1024, 25, 40, 1, 2,
         819, -4096, 10}
    }};
    const Values& selected = values[result.variant];
    result.health = selected.health;
    result.beam_damage = selected.beam_damage;
    result.splash_damage = selected.splash_damage;
    result.contact_damage = selected.contact_damage;
    result.min_speed_factor = static_cast<float>(selected.min_speed_factor)
        / 4096.0F / 2.0F;
    result.max_speed_factor = static_cast<float>(selected.max_speed_factor)
        / 4096.0F / 2.0F;
    result.speed_increment = (result.max_speed_factor
                              - result.min_speed_factor)
        / 7.0F;
    result.jump_speed = static_cast<float>(selected.jump_speed)
        / 4096.0F / 2.0F;
    result.range_max_cosine = static_cast<float>(selected.range_max_cosine)
        / 4096.0F;
    result.delay_frames = selected.delay_frames;
    result.shot_frames = selected.shot_frames;
    result.min_shots = selected.min_shots;
    result.max_shots = selected.max_shots;
    result.aim_steps = selected.aim_steps;

    // EnemyWeapons uses BeamType ordinals, while the native projectile record
    // uses the compact gameplay weapon slots. The first three versions are
    // the Voldrum variants present in the retail rooms; retain their exact
    // enemy draw/effect values rather than borrowing the player table.
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
        && result.home_volume.kind != scene::VolumeKind::Invalid;
    return result;
}

} // namespace fruityprime::enemy

