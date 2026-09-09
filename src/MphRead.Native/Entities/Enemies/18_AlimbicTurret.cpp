#include "enemy_decode_common.hpp"
// Native port of src/MphRead/Entities/Enemies/18_AlimbicTurret.cs.
// This member definition lives in its enemy module; Session only dispatches it.
#include "18_AlimbicTurret.hpp"
#include "enemy_common.hpp"

namespace fruityprime::gameplay {

void Session::update_turret(EnemyState& agent) {
    const auto& profile = agent.turret;
    if (!profile.supported) {
        return;
    }
    const bool is_slench_turret = agent.enemy_type == static_cast<std::uint8_t>(
        formats::EnemyType::SlenchTurret);
    if (is_slench_turret && !agent.turret_enabled) {
        agent.state = 3;
        agent.target_slot = 0xff;
        agent.velocity = {};
        return;
    }

    const float seconds = config_.tick_seconds;
    agent.attack_timer = std::max(0.0F, agent.attack_timer - seconds);
    const auto in_range = [&profile, this](const net::PlayerState& player) {
        if (profile.range_volume.kind != scene::VolumeKind::Invalid) {
            return profile.range_volume.contains(to_volume_point(
                player.position));
        }
        return distance_squared(player.position, to_net(
            profile.range_volume.center()))
            <= 24.0F * 24.0F;
    };

    std::size_t target_index = players_.size();
    float nearest_squared = std::numeric_limits<float>::max();
    for (std::size_t player_index = 0; player_index < players_.size();
         ++player_index) {
        const auto& player = players_[player_index];
        if (!objective_player(player) || !in_range(player)) {
            continue;
        }
        const float distance = distance_squared(player.position,
                                                agent.position);
        if (distance < nearest_squared) {
            nearest_squared = distance;
            target_index = player_index;
        }
    }

    const auto patrol_direction = [&agent](float angle_x, float angle_y) {
        constexpr float Pi = 3.14159265358979323846F;
        const net::Vec3 base = normalized_or(
            agent.turret_initial_facing, {0.0F, 0.0F, 1.0F});
        const float base_yaw = std::atan2(base.x, base.z);
        const float base_pitch = std::asin(std::clamp(base.y, -1.0F, 1.0F));
        const float yaw = base_yaw + angle_y * Pi / 180.0F;
        const float pitch = std::clamp(
            base_pitch + angle_x * Pi / 180.0F, -Pi * 0.49F, Pi * 0.49F);
        const float horizontal = std::cos(pitch);
        return normalized_or(net::Vec3{
            std::sin(yaw) * horizontal, std::sin(pitch),
            std::cos(yaw) * horizontal}, base);
    };

    if (target_index == players_.size()) {
        if (agent.target_slot != 0xff) {
            agent.turret_burst_remaining = profile.min_shots;
            agent.turret_shot_timer = static_cast<std::uint32_t>(
                profile.shot_cooldown_frames) * 2u;
            agent.turret_delay_timer = static_cast<std::uint32_t>(
                profile.delay_frames) * 2u;
        }
        agent.target_slot = 0xff;
        agent.state = 0;
        agent.turret_angle_y += profile.angle_increment_y * 0.5F
            * agent.turret_angle_y_sign;
        if (agent.turret_angle_y >= profile.max_angle_y) {
            agent.turret_angle_y = profile.max_angle_y;
            agent.turret_angle_y_sign = -1.0F;
        } else if (agent.turret_angle_y <= profile.min_angle_y) {
            agent.turret_angle_y = profile.min_angle_y;
            agent.turret_angle_y_sign = 1.0F;
        }
        agent.turret_angle_x += profile.angle_increment_x * 0.5F
            * agent.turret_angle_x_sign;
        if (agent.turret_angle_x >= profile.max_angle_x) {
            agent.turret_angle_x = profile.max_angle_x;
            agent.turret_angle_x_sign = -1.0F;
        } else if (agent.turret_angle_x <= profile.min_angle_x) {
            agent.turret_angle_x = profile.min_angle_x;
            agent.turret_angle_x_sign = 1.0F;
        }
        agent.turret_aim = patrol_direction(agent.turret_angle_x,
                                             agent.turret_angle_y);
        return;
    }

    auto& target = players_[target_index];
    agent.target_slot = target.slot_index;
    agent.state = 3;
    agent.turret_aim = normalized_or(
        subtract(add(target.position, {0.0F, 0.5F, 0.0F}), agent.position),
        agent.turret_initial_facing);

    // Enemy45Entity keeps SalvoCooldown separate from ShotCooldown.  The
    // first shot of a newly activated burst is immediate; after the last
    // shot, the next burst waits for the authored salvo interval.  Alimbic
    // turrets retain their existing delay/burst path below.
    if (is_slench_turret) {
        if (agent.turret_salvo_cooldown > 0) {
            --agent.turret_salvo_cooldown;
            return;
        }
        if (agent.turret_burst_remaining == 0) {
            const auto range = static_cast<std::uint32_t>(
                profile.max_shots >= profile.min_shots
                ? profile.max_shots - profile.min_shots + 1u : 1u);
            agent.turret_burst_remaining = static_cast<std::uint16_t>(
                profile.min_shots + rng_.random2(range));
            agent.turret_shot_timer = 0;
        }
        if (agent.turret_shot_timer > 0) {
            --agent.turret_shot_timer;
            return;
        }
        // Enemy45Entity spawns from Position itself.  The authored shot
        // offset belongs to Enemy18Entity and must not leak into Slench's
        // shared controller.
        spawn_enemy_projectile(agent, agent.position, agent.turret_aim,
                               SoundCue::TurretAttack);
        --agent.turret_burst_remaining;
        agent.turret_shot_timer = static_cast<std::uint32_t>(
            profile.shot_cooldown_frames) * 2u;
        if (agent.turret_burst_remaining == 0) {
            agent.turret_salvo_cooldown = static_cast<std::uint32_t>(
                profile.delay_frames) * 2u;
        }
        return;
    }

    const float distance = std::sqrt(std::max(0.0F,
        distance_squared(agent.position, target.position)));
    if (distance <= 1.25F && profile.contact_damage > 0
        && agent.attack_timer <= 0.0F) {
        const std::uint16_t old_health = target.health;
        target.health = old_health > profile.contact_damage
            ? static_cast<std::uint16_t>(old_health - profile.contact_damage)
            : 0;
        target.damage_sequence = static_cast<std::uint8_t>(
            target.damage_sequence + 1);
        target.attacker_slot = 0xff;
        target.damage_beam = agent.enemy_type;
        target.hit_direction = normalized_or(
            subtract(target.position, agent.position), {});
        if (target.health != old_health) {
            SoundEvent damage_event;
            damage_event.cue = SoundCue::PlayerDamage;
            damage_event.slot = target.slot_index;
            damage_event.entity_id = agent.id;
            damage_event.enemy_type = agent.enemy_type;
            damage_event.position = target.position;
            emit_sound(damage_event);
        }
        if (target.health == 0 && old_health != 0) {
            SoundEvent death_event;
            death_event.cue = SoundCue::PlayerDeath;
            death_event.slot = target.slot_index;
            death_event.entity_id = agent.id;
            death_event.enemy_type = agent.enemy_type;
            death_event.position = target.position;
            emit_sound(death_event);
            increment_saturating(target.deaths);
            target.flags &= static_cast<std::uint8_t>(
                ~(net::PlayerState::FlagSpawned
                  | net::PlayerState::FlagAltForm
                  | net::PlayerState::FlagZoomed));
            target.speed = {};
            const auto runtime = std::find_if(
                inputs_.begin(), inputs_.end(), [&target](const RuntimeInput& value) {
                    return value.slot == target.slot_index;
                });
            if (runtime != inputs_.end()) {
                if (config_.survival_mode
                    && target.deaths > config_.survival_lives) {
                    runtime->eliminated = true;
                    runtime->respawn_ticks = 0;
                } else {
                    runtime->eliminated = false;
                    runtime->respawn_ticks = config_.respawn_ticks;
                }
                runtime->fire_cooldown = 0;
            }
        }
        agent.attack_timer = std::max(
            0.1F, static_cast<float>(profile.shot_cooldown_frames * 2)
                * seconds);
    }

    if (agent.turret_delay_timer > 0) {
        --agent.turret_delay_timer;
        return;
    }
    if (agent.turret_burst_remaining == 0) {
        agent.turret_burst_remaining = profile.min_shots;
        agent.turret_shot_timer = static_cast<std::uint32_t>(
            profile.shot_cooldown_frames) * 2u;
    }
    if (agent.turret_shot_timer > 0) {
        --agent.turret_shot_timer;
        return;
    }
    const net::Vec3 spawn_position = add(
        agent.position, multiply(agent.turret_aim, profile.shot_offset));
    spawn_enemy_projectile(agent, spawn_position, agent.turret_aim);
    --agent.turret_burst_remaining;
    agent.turret_shot_timer = static_cast<std::uint32_t>(
        profile.shot_cooldown_frames) * 2u;
    if (agent.turret_burst_remaining == 0) {
        agent.turret_delay_timer = static_cast<std::uint32_t>(
            profile.delay_frames) * 2u;
    }
}

void Session::update_alimbic_turret(EnemyState& agent) {
    update_turret(agent);
}
} // namespace fruityprime::gameplay

namespace fruityprime::enemy {

TurretProfile decode_turret_profile(
    std::uint8_t id, std::span<const std::uint8_t> fields,
    net::Vec3 origin) noexcept {
    TurretProfile result;
    const auto alimbic_turret = static_cast<std::uint8_t>(
        formats::EnemyType::AlimbicTurret);
    const auto slench_turret = static_cast<std::uint8_t>(
        formats::EnemyType::SlenchTurret);
    if ((id != alimbic_turret && id != slench_turret)
        || fields.size() < 8 + 2 * 64) {
        return result;
    }

    const std::uint32_t max_subtype = id == slench_turret ? 3u : 2u;
    const std::uint32_t max_version = id == slench_turret ? 3u : 2u;
    result.subtype = std::min<std::uint32_t>(detail::read_u32(fields, 0),
                                             max_subtype);
    result.version = std::min<std::uint32_t>(detail::read_u32(fields, 4),
                                             max_version);
    // Alimbic Turret stores its target volume in S06 Volume1. Slench's
    // turret uses S10 Volume0 for the same purpose; its S10 Volume1 is the
    // local hurt volume and is intentionally not used for acquisition.
    result.range_volume = detail::read_volume(fields,
        id == slench_turret ? 8 : 72, origin);
    if (result.range_volume.kind == scene::VolumeKind::Invalid) {
        return result;
    }

    // Metadata.Enemy18Values. The original fields are fixed-point integers;
    // keeping the conversion here makes the gameplay state independent of
    // the managed Fixed helper and preserves the three cartridge variants.
    struct Values {
        std::uint16_t health;
        std::uint16_t beam_damage;
        std::uint16_t splash_damage;
        std::uint16_t contact_damage;
        std::uint16_t shot_cooldown;
        std::uint16_t delay;
        std::uint16_t min_shots;
        std::uint16_t max_shots;
    };
    static constexpr std::array<Values, 3> alimbic_values{{
        {24, 2, 0, 5, 5, 40, 1, 2},
        {80, 4, 2, 5, 3, 30, 3, 5},
        {120, 50, 0, 5, 3, 90, 1, 1}
    }};
    static constexpr std::array<Values, 4> slench_values{{
        {30, 2, 0, 10, 20, 90, 1, 1},
        {30, 2, 0, 10, 20, 90, 1, 1},
        {30, 2, 0, 10, 20, 250, 1, 1},
        {30, 2, 0, 5, 20, 250, 1, 1}
    }};
    const Values& selected = id == slench_turret
        ? slench_values[result.subtype]
        : alimbic_values[result.subtype];
    result.health = selected.health;
    result.beam_damage = selected.beam_damage;
    result.splash_damage = selected.splash_damage;
    result.contact_damage = selected.contact_damage;
    result.shot_cooldown_frames = selected.shot_cooldown;
    result.delay_frames = selected.delay;
    result.min_shots = selected.min_shots;
    result.max_shots = selected.max_shots;
    result.index = static_cast<std::int32_t>(detail::read_u32(fields, 136));
    result.projectile_speed = result.version == 0
        ? 2662.0F / 4096.0F * 60.0F
        : result.version == 1
            ? 3276.0F / 4096.0F * 60.0F
            : 819.0F / 4096.0F * 60.0F;
    result.projectile_lifetime = result.version == 1
        ? 90.0F / 60.0F : 255.0F / 60.0F;
    result.projectile_draw_func = 0xff;
    result.projectile_color = result.version == 0 ? 9055
        : result.version == 1 ? 32767 : 32140;
    result.projectile_collision_effect = result.version == 0 ? 242
        : result.version == 1 ? 89 : 8;
    result.projectile_muzzle_effect = result.version == 1 ? 60 : 65;
    result.supported = true;
    return result;
}

} // namespace fruityprime::enemy
