#include "enemy_decode_common.hpp"
// Native port of src/MphRead/Entities/Enemies/39_FireSpawn.cs.
// This member definition lives in its enemy module; Session only dispatches it.
#include "39_FireSpawn.hpp"
#include "enemy_common.hpp"

namespace fruityprime::gameplay {

void Session::update_firespawn(EnemyState& agent) {
    const auto& profile = agent.firespawn;
    if (!profile.supported) {
        return;
    }

    const float seconds = config_.tick_seconds;
    const float frames = std::max(1.0F, std::round(
        std::max(0.0F, seconds * 60.0F)));
    const auto frame_step = static_cast<std::uint32_t>(frames);
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
    const auto choose_surface_location = [&]() {
        const auto& volume = profile.location_volume;
        net::Vec3 location{volume.center().x, agent.position.y,
                           volume.center().z};
        if (volume.kind == scene::VolumeKind::Cylinder) {
            const float radius = random_distance(volume.cylinder_radius);
            const float angle = static_cast<float>(rng_.random2(0xB4000u))
                / 4096.0F * 3.14159265358979323846F / 180.0F;
            location.x = volume.cylinder_position.x
                + std::sin(angle) * radius;
            location.z = volume.cylinder_position.z
                + std::cos(angle) * radius;
        } else if (volume.kind == scene::VolumeKind::Sphere) {
            const float radius = random_distance(volume.sphere_radius);
            const float angle = static_cast<float>(rng_.random2(0xB4000u))
                / 4096.0F * 3.14159265358979323846F / 180.0F;
            location.x = volume.sphere_position.x
                + std::sin(angle) * radius;
            location.z = volume.sphere_position.z
                + std::cos(angle) * radius;
        }
        agent.position = location;
    };
    const auto choose_attack_count = [&]() {
        const auto range = static_cast<std::uint32_t>(
            profile.attack_count_max >= profile.attack_count_min
            ? profile.attack_count_max - profile.attack_count_min + 1u : 1u);
        agent.firespawn_attacks_remaining = static_cast<std::uint16_t>(
            profile.attack_count_min + rng_.random2(range));
    };
    const auto set_hit_zone = [this, &agent](bool collidable) {
        for (auto& child : enemies_) {
            if (child.parent_enemy_id != agent.id
                || child.enemy_type != static_cast<std::uint8_t>(
                    formats::EnemyType::HitZone)) {
                continue;
            }
            child.hit_zone_collidable = collidable;
            child.visible = collidable;
            child.invulnerable = true;
        }
    };

    std::size_t target_index = players_.size();
    float target_distance = std::numeric_limits<float>::max();
    for (std::size_t index = 0; index < players_.size(); ++index) {
        const auto& player = players_[index];
        if (!objective_player(player)
            || !profile.active_volume.contains(to_volume_point(
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

    if (agent.firespawn_submerged) {
        set_hit_zone(false);
        agent.visible = false;
        agent.invulnerable = true;
        agent.velocity = {};
        agent.target_slot = 0xff;
        agent.state = 0;
        if (target_index == players_.size()) {
            return;
        }
        if (agent.firespawn_dive_timer > 0) {
            decrement(agent.firespawn_dive_timer);
            return;
        }
        choose_surface_location();
        agent.firespawn_submerged = false;
        agent.firespawn_surface_timer = 0;
        agent.firespawn_tangibility_timer = 0;
        agent.firespawn_attack_timer = 0;
        choose_attack_count();
        agent.visible = true;
        agent.invulnerable = true;
        agent.state = 1;
        return;
    }

    if (target_index == players_.size()) {
        // State6/Behavior6 starts the managed submerge sequence as soon as
        // the activation volume no longer contains the player.
        set_hit_zone(false);
        agent.firespawn_submerged = true;
        agent.visible = false;
        agent.invulnerable = true;
        agent.firespawn_dive_timer = 18u * 2u;
        agent.firespawn_surface_timer = 0;
        agent.firespawn_attacks_remaining = 0;
        agent.state = 0;
        agent.target_slot = 0xff;
        return;
    }

    auto& target = players_[target_index];
    agent.target_slot = target.slot_index;
    agent.state = 4;
    agent.velocity = {};
    agent.facing = normalized_or(
        {target.position.x - agent.position.x, 0.0F,
         target.position.z - agent.position.z}, agent.facing);

    if (agent.firespawn_surface_timer > 0) {
        set_hit_zone(false);
        decrement(agent.firespawn_surface_timer);
        if (agent.firespawn_surface_timer == 0) {
            agent.firespawn_submerged = true;
            agent.visible = false;
            agent.invulnerable = true;
            agent.firespawn_dive_timer = 18u * 2u;
            agent.state = 0;
        }
        return;
    }

    agent.visible = true;
    agent.invulnerable = false;
    agent.firespawn_tangibility_timer = std::min<std::uint32_t>(
        agent.firespawn_tangibility_timer + frame_step, 5u * 2u);
    const bool hit_zone_collidable = agent.firespawn_tangibility_timer
        >= 5u * 2u;
    set_hit_zone(hit_zone_collidable);
    agent.attack_timer = std::max(0.0F, agent.attack_timer - seconds);
    if (hit_zone_collidable && agent.attack_timer <= 0.0F
        && distance_squared(target.position, agent.position)
            <= 1.25F * 1.25F) {
        apply_enemy_contact_damage(agent, target, profile.contact_damage);
        const net::Vec3 away = normalized_or(
            subtract(target.position, agent.position), {});
        target.speed = add(target.speed, multiply(away, 2.0F));
        agent.attack_timer = 0.5F;
    }

    if (agent.firespawn_attacks_remaining == 0) {
        set_hit_zone(false);
        agent.firespawn_surface_timer = 18u * 2u;
        return;
    }
    if (agent.firespawn_attack_timer > 0) {
        decrement(agent.firespawn_attack_timer);
        return;
    }

    const net::Vec3 aim = normalized_or(
        subtract(add(target.position, {0.0F, 0.5F, 0.0F}),
                 agent.position), agent.facing);
    const net::Vec3 right = normalized_or(
        cross(aim, agent.up), {-1.0F, 0.0F, 0.0F});
    spawn_enemy_projectile(agent,
                           add(agent.position, multiply(right, -0.75F)),
                           aim);
    spawn_enemy_projectile(agent,
                           add(agent.position, multiply(right, 0.75F)),
                           aim);
    --agent.firespawn_attacks_remaining;
    agent.firespawn_attack_timer = 15u;
}
} // namespace fruityprime::gameplay

namespace fruityprime::enemy {

FireSpawnProfile decode_firespawn_profile(
    std::uint8_t id, std::span<const std::uint8_t> fields,
    net::Vec3 origin) noexcept {
    FireSpawnProfile result;
    if (id != static_cast<std::uint8_t>(formats::EnemyType::FireSpawn)
        || fields.size() < 8 + 4 * 64) {
        return result;
    }

    result.subtype = std::min<std::uint32_t>(detail::read_u32(fields, 0), 1u);
    result.version = std::min<std::uint32_t>(detail::read_u32(fields, 4), 10u);
    result.location_volume = detail::read_volume(fields, 72, origin);
    result.active_volume = detail::read_volume(fields, 136, origin);

    struct Values {
        std::uint16_t health;
        std::uint16_t beam_damage;
        std::uint16_t splash_damage;
        std::uint16_t contact_damage;
        std::uint32_t effectiveness;
        std::uint16_t attack_delay_frames;
        std::uint16_t attack_count_min;
        std::uint16_t attack_count_max;
        std::uint16_t dive_timer_min;
        std::uint16_t dive_timer_max;
    };
    static constexpr std::array<Values, 2> values{{
        {600, 30, 15, 12, 0x8955, 0, 3, 6, 1, 40},
        {600, 30, 0, 12, 0xB155, 0, 2, 5, 1, 50}
    }};
    const Values& selected = values[result.subtype];
    result.health = selected.health;
    result.beam_damage = selected.beam_damage;
    result.splash_damage = selected.splash_damage;
    result.contact_damage = selected.contact_damage;
    result.effectiveness = selected.effectiveness;
    result.attack_delay_frames = selected.attack_delay_frames;
    result.attack_count_min = selected.attack_count_min;
    result.attack_count_max = selected.attack_count_max;
    result.dive_timer_min = selected.dive_timer_min;
    result.dive_timer_max = selected.dive_timer_max;

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
    result.supported = result.location_volume.kind
            != scene::VolumeKind::Invalid
        && result.active_volume.kind != scene::VolumeKind::Invalid;
    return result;
}

} // namespace fruityprime::enemy

