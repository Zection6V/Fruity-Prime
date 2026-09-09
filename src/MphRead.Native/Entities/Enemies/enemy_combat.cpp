// Shared combat services used by the per-enemy native modules.
//
// Projectile construction and contact damage are part of EnemyInstanceEntity
// in the managed tree, but keeping them in gameplay.cpp made the C++ port's
// central file grow into a second enemy implementation. This module is the
// explicit shared boundary: class files request a projectile or contact hit,
// while Session still owns the authoritative queues and player state.
#include "enemy_common.hpp"

namespace fruityprime::gameplay {

void Session::spawn_enemy_projectile(const EnemyState& enemy,
                                     net::Vec3 position,
                                     net::Vec3 direction,
                                     SoundCue sound_cue) {
    const bool firespawn = enemy.firespawn.supported;
    const bool psychobit = enemy.psychobit.supported;
    const bool barbed_warwasp = enemy.barbed_warwasp.supported;
    const bool gorea_arm = enemy.enemy_type == static_cast<std::uint8_t>(
        formats::EnemyType::GoreaArm) && enemy.gorea_activated;
    const bool voldrum = enemy.voldrum.supported && enemy.voldrum.ranged;
    const bool cretaphid = enemy.cretaphid.supported
        && enemy.cretaphid_part_kind != 0;
    const bool slench = enemy.slench.supported;
    const bool turret = enemy.turret.supported;
    const bool force_field = enemy.force_field_entity_id >= 0;
    if (!firespawn && !psychobit && !barbed_warwasp && !gorea_arm && !voldrum
        && !cretaphid && !slench && !turret && !force_field) {
        return;
    }
    direction = normalized_or(direction, enemy.facing);
    std::uint8_t weapon = 0;
    std::uint16_t damage = 0;
    float speed = 0.0F;
    float lifetime = 0.0F;
    std::uint8_t draw_func = 0xff;
    std::uint16_t color = 0x7fff;
    std::uint8_t collision_effect = 0xff;
    std::uint8_t muzzle_effect = 0xff;
    if (force_field) {
        weapon = metadata::native_weapon_slot_from_beam(
            static_cast<std::int32_t>(enemy.force_field_type));
        if (weapon == 0xff) {
            return;
        }
        const auto& profile = metadata::weapon_info(weapon);
        const auto& visual = metadata::weapon_visual_info(weapon);
        damage = profile.uncharged_damage;
        speed = profile.projectile_speed;
        lifetime = profile.lifetime_seconds;
        draw_func = visual.draw_func_ids[0];
        color = visual.colors[0];
        collision_effect = visual.collision_effects[0];
        muzzle_effect = visual.muzzle_effects[0];
    } else if (firespawn) {
        const auto& profile = enemy.firespawn;
        weapon = profile.projectile_weapon;
        damage = profile.beam_damage;
        speed = profile.projectile_speed;
        lifetime = profile.projectile_lifetime;
        draw_func = profile.projectile_draw_func;
        color = profile.projectile_color;
        collision_effect = profile.projectile_collision_effect;
        muzzle_effect = profile.projectile_muzzle_effect;
    } else if (psychobit) {
        const auto& profile = enemy.psychobit;
        weapon = profile.projectile_weapon;
        damage = profile.beam_damage;
        speed = profile.projectile_speed;
        lifetime = profile.projectile_lifetime;
        draw_func = profile.projectile_draw_func;
        color = profile.projectile_color;
        collision_effect = profile.projectile_collision_effect;
        muzzle_effect = profile.projectile_muzzle_effect;
    } else if (barbed_warwasp) {
        const auto& profile = enemy.barbed_warwasp;
        // Weapons.EnemyWeapons is a BeamType-indexed table, while the native
        // gameplay table stores the player's compact weapon slots. Preserve
        // the eleven authored version choices at this boundary.
        static constexpr std::array<std::uint8_t, 11> weapon_slots{
            0, 2, 1, 3, 4, 5, 6, 7, 0, 6, 0};
        const std::size_t version = std::min<std::size_t>(
            profile.version, weapon_slots.size() - 1);
        weapon = weapon_slots[version];
        damage = profile.beam_damage;
        const auto& native_profile = metadata::weapon_info(weapon);
        const auto& visual = metadata::weapon_visual_info(weapon);
        speed = native_profile.projectile_speed;
        lifetime = native_profile.lifetime_seconds;
        draw_func = visual.draw_func_ids[0];
        color = visual.colors[0];
        collision_effect = visual.collision_effects[0];
        muzzle_effect = visual.muzzle_effects[0];
    } else if (gorea_arm) {
        // Enemy26's EquipInfo uses the dedicated GoreaWeapons table. Convert
        // its six entries from cartridge BeamType values to native slots at
        // the shared projectile boundary.
        static constexpr std::array<std::int32_t, 6> beam_types{
            3, 1, 6, 5, 4, 0};
        const std::size_t index = std::min<std::size_t>(
            enemy.gorea_weapon_index, beam_types.size() - 1);
        weapon = metadata::native_weapon_slot_from_beam(beam_types[index]);
        const auto& profile = metadata::weapon_info(weapon);
        const auto& visual = metadata::weapon_visual_info(weapon);
        damage = profile.uncharged_damage;
        speed = profile.projectile_speed;
        lifetime = profile.lifetime_seconds;
        draw_func = visual.draw_func_ids[0];
        color = visual.colors[0];
        collision_effect = visual.collision_effects[0];
        muzzle_effect = visual.muzzle_effects[0];
    } else if (voldrum) {
        const auto& profile = enemy.voldrum;
        weapon = profile.projectile_weapon;
        damage = profile.beam_damage;
        speed = profile.projectile_speed;
        lifetime = profile.projectile_lifetime;
        draw_func = profile.projectile_draw_func;
        color = profile.projectile_color;
        collision_effect = profile.projectile_collision_effect;
        muzzle_effect = profile.projectile_muzzle_effect;
    } else if (cretaphid) {
        const auto& profile = enemy.cretaphid;
        const auto& phase = profile.phases[std::min<std::size_t>(
            enemy.cretaphid_phase, profile.phases.size() - 1)];
        if (enemy.cretaphid_part_kind == 2) {
            weapon = 0;
            damage = phase.crystal_beam_damage;
        } else {
            weapon = static_cast<std::uint8_t>(std::min<std::uint8_t>(
                enemy.cretaphid_beam_type,
                static_cast<std::uint8_t>(metadata::WeaponCount - 1)));
            damage = phase.eye_beam_damage;
        }
        const auto& visual = metadata::weapon_visual_info(weapon);
        speed = metadata::weapon_info(weapon).projectile_speed;
        lifetime = metadata::weapon_info(weapon).lifetime_seconds;
        draw_func = visual.draw_func_ids[0];
        color = visual.colors[0];
        collision_effect = visual.collision_effects[0];
        muzzle_effect = visual.muzzle_effects[0];
    } else if (slench) {
        const auto& profile = enemy.slench;
        if (enemy.slench_firing_tear) {
            weapon = profile.tear_weapon;
            damage = profile.tear_damage;
            speed = profile.tear_speed;
            lifetime = profile.tear_lifetime;
            draw_func = profile.tear_draw_func;
            color = profile.tear_color;
            collision_effect = profile.tear_collision_effect;
            muzzle_effect = profile.tear_muzzle_effect;
        } else {
            weapon = profile.projectile_weapon;
            damage = profile.projectile_damage;
            speed = profile.projectile_speed;
            lifetime = profile.projectile_lifetime;
            draw_func = profile.projectile_draw_func;
            color = profile.projectile_color;
            collision_effect = profile.projectile_collision_effect;
            muzzle_effect = profile.projectile_muzzle_effect;
        }
    } else {
        const auto& profile = enemy.turret;
        weapon = static_cast<std::uint8_t>(std::min<std::uint32_t>(
            profile.version, metadata::WeaponCount - 1));
        damage = profile.beam_damage;
        speed = profile.projectile_speed;
        lifetime = profile.projectile_lifetime;
        draw_func = profile.projectile_draw_func;
        color = profile.projectile_color;
        collision_effect = profile.projectile_collision_effect;
        muzzle_effect = profile.projectile_muzzle_effect;
    }
    Projectile projectile{
        position, direction, 0xff, weapon, damage, speed, 0.0F
    };
    projectile.owner_enemy_id = enemy.id;
    projectile.max_lifetime = lifetime;
    projectile.back_position = position;
    projectile.spawn_position = position;
    projectile.past_positions.fill(position);
    projectile.draw_func_id = draw_func;
    projectile.color = color_from_rgb555(color);
    projectile.collision_effect = collision_effect;
    projectile.muzzle_effect = muzzle_effect;
    projectile.damage_dir_type = 0;
    projectile.damage_interpolation = 0;
    projectiles_.push_back(std::move(projectile));

    SoundEvent shot_event;
    shot_event.cue = sound_cue;
    shot_event.slot = 0xff;
    shot_event.weapon = weapon;
    shot_event.enemy_type = enemy.enemy_type;
    shot_event.entity_id = enemy.id;
    shot_event.position = position;
    emit_sound(shot_event);
    if (muzzle_effect != 0xff) {
        spawn_effect(muzzle_effect, position, direction, enemy.id);
    }
}

void Session::apply_enemy_contact_damage(EnemyState& agent,
                                          net::PlayerState& target,
                                          std::uint16_t damage) {
    if (damage == 0 || !objective_player(target) || target.health == 0) {
        return;
    }

    const std::uint16_t old_health = target.health;
    target.health = old_health > damage
        ? static_cast<std::uint16_t>(old_health - damage) : 0;
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
    if (target.health != 0 || old_health == 0) {
        return;
    }

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
    if (runtime == inputs_.end()) {
        return;
    }
    if (config_.survival_mode && target.deaths > config_.survival_lives) {
        runtime->eliminated = true;
        runtime->respawn_ticks = 0;
    } else {
        runtime->eliminated = false;
        runtime->respawn_ticks = config_.respawn_ticks;
    }
    runtime->fire_cooldown = 0;
}

} // namespace fruityprime::gameplay
