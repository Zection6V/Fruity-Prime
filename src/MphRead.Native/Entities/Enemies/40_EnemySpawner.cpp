// Native counterpart of src/MphRead/Entities/Enemies/40_EnemySpawner.cs.
// Enemy40 owns the room-spawner lifecycle boundary. Its update body is kept
// here so gameplay.cpp remains an orchestration module rather than a second
// implementation of every enemy class.
#include "05_Petrasyl3.hpp"
#include "04_Petrasyl2.hpp"
#include "03_Petrasyl1.hpp"
#include "10_BarbedWarWasp.hpp"
#include "00_WarWasp.hpp"
#include "38_CrashPillar.hpp"
#include "16_Blastcap.hpp"
#include "11_Shriekbat.hpp"
#include "40_EnemySpawner.hpp"
#include "enemy_common.hpp"
#include "gorea_common.hpp"

namespace fruityprime::gameplay {

void Session::update_enemy_spawns() {
    std::vector<net::Vec3> player_positions;
    player_positions.reserve(players_.size());
    for (const auto& player : players_) {
        if (objective_player(player)) {
            player_positions.push_back(player.position);
        }
    }

    for (auto& runtime : enemy_spawns_) {
        if (runtime.spawner == nullptr) {
            continue;
        }
        const auto result = runtime.spawner->tick(
            config_.tick_seconds, player_positions, true, true);
        for (std::uint8_t i = 0; i < result.spawned; ++i) {
            const auto tuning = enemy::combat_tuning(
                runtime.spawner->enemy_type());
            EnemyState spawned;
            spawned.id = next_enemy_id_++;
            spawned.spawner_entity_id = runtime.entity_id;
            spawned.enemy_type = runtime.spawner->enemy_type();
            spawned.position = runtime.spawner->position();
            spawned.behavior_origin = spawned.position;
            spawned.facing = runtime.facing;
            spawned.up = runtime.up;
            spawned.zoomer = enemy::decode_zoomer_profile(
                spawned.enemy_type, runtime.data.fields, spawned.position);
            spawned.psychobit = enemy::decode_psychobit_profile(
                spawned.enemy_type, runtime.data.fields, spawned.position);
            spawned.firespawn = enemy::decode_firespawn_profile(
                spawned.enemy_type, runtime.data.fields, spawned.position);
            spawned.voldrum = enemy::decode_voldrum_profile(
                spawned.enemy_type, runtime.data.fields, spawned.position);
            spawned.temroid = enemy::decode_temroid_profile(
                spawned.enemy_type, runtime.data.fields, spawned.position);
            spawned.petrasyl = enemy::decode_petrasyl_profile(
                spawned.enemy_type, runtime.data.fields, spawned.position);
            spawned.carnivorous_plant =
                enemy::decode_carnivorous_plant_profile(
                    spawned.enemy_type, runtime.data.fields, spawned.position);
            spawned.ithrak = enemy::decode_ithrak_profile(
                spawned.enemy_type, runtime.data.fields, spawned.position);
            spawned.quadtroid = enemy::decode_quadtroid_profile(
                spawned.enemy_type, runtime.data.fields, spawned.position);
            spawned.crash_pillar = enemy::decode_crash_pillar_profile(
                spawned.enemy_type, runtime.data.fields, spawned.position);
            spawned.cretaphid = enemy::decode_cretaphid_profile(
                spawned.enemy_type, runtime.data.fields, spawned.position);
            spawned.slench = enemy::decode_slench_profile(
                spawned.enemy_type, runtime.data.fields, spawned.position,
                room_.definition().name);
            if (spawned.enemy_type == static_cast<std::uint8_t>(
                    formats::EnemyType::SlenchShield)) {
                spawned.slench_shield = enemy::slench_shield_profile();
            } else if (spawned.enemy_type == static_cast<std::uint8_t>(
                           formats::EnemyType::SlenchNest)) {
                spawned.slench_nest = enemy::slench_nest_profile();
            } else if (spawned.enemy_type == static_cast<std::uint8_t>(
                           formats::EnemyType::SlenchSynapse)) {
                spawned.slench_synapse = enemy::slench_synapse_profile(
                    0, 0);
            }
            spawned.shriekbat = enemy::decode_shriekbat_profile(
                spawned.enemy_type, runtime.data.fields, spawned.position);
            if (spawned.shriekbat.supported) {
                spawned.shriekbat_target = add(
                    spawned.position, spawned.shriekbat.path_vector);
            }
            if (spawned.temroid.supported) {
                spawned.position = add(
                    add(spawned.position, spawned.temroid.position_offset),
                    {0.0F, 0.5F, 0.0F});
                const net::Vec3 temroid_facing = normalized_or(
                    {spawned.temroid.facing.x, 0.0F,
                     spawned.temroid.facing.z}, spawned.facing);
                spawned.facing = temroid_facing;
                const net::Vec3 side{-temroid_facing.z, 0.0F,
                                     temroid_facing.x};
                const float idle_x = spawned.temroid.idle_range.x;
                const float idle_z = spawned.temroid.idle_range.z;
                spawned.temroid_idle_points[0] = spawned.position;
                spawned.temroid_idle_points[1] = add(
                    spawned.position, multiply(temroid_facing, idle_z));
                spawned.temroid_idle_points[2] = add(
                    spawned.temroid_idle_points[1], multiply(side, idle_x));
                spawned.temroid_idle_points[3] = add(
                    spawned.position, multiply(side, idle_x));
                spawned.temroid_height_origin = spawned.position;
                spawned.temroid_idle_index = 0;
                spawned.temroid_attached_slot = 0xff;
                spawned.temroid_state_timer = 0;
                spawned.temroid_phase = 0.0F;
                spawned.temroid_next_state = 0;
                spawned.temroid_sub_id = 0;
                spawned.temroid_field170 = 0;
                spawned.temroid_time_since_damage = 510;
                spawned.temroid_drain_damage_timer = 0;
                spawned.temroid_animation = 0;
                spawned.temroid_animation_timer = 0;
                spawned.temroid_animation_no_loop = false;
                spawned.temroid_animation_ended = false;
                spawned.temroid_field1d0 = false;
                spawned.temroid_hit_by_bomb = false;
                spawned.temroid_state_initialized = false;
            }
            if (spawned.petrasyl.supported) {
                constexpr float spawn_height = 5461.0F / 4096.0F;
                const auto& profile = spawned.petrasyl;
                spawned.petrasyl_initial_position = add(
                    spawned.position, profile.position_offset);
                spawned.petrasyl_initial_position.y += spawn_height;
                spawned.position = spawned.petrasyl_initial_position;
                spawned.petrasyl_target_y = spawned.position.y;

                const net::Vec3 authored_facing = normalized_or(
                    {profile.facing.x, 0.0F, profile.facing.z},
                    spawned.facing);
                if (profile.variant == static_cast<std::uint8_t>(
                        formats::EnemyType::Petrasyl1)) {
                    const net::Vec3 side{-authored_facing.z, 0.0F,
                                         authored_facing.x};
                    spawned.petrasyl_idle_limit = add(
                        spawned.position,
                        add(multiply(authored_facing, profile.idle_range.z),
                            multiply(side, profile.idle_range.x)));
                    spawned.facing = multiply(authored_facing, -1.0F);
                    spawned.petrasyl_direction = authored_facing;
                    spawned.petrasyl_bob_offset = static_cast<float>(
                        rng_.random2(0x1800u) + 2048u) / 4096.0F / 2.0F;
                    spawned.petrasyl_bob_speed = static_cast<float>(
                        rng_.random2(0x6000u)) / 4096.0F + 1.0F;
                    spawned.petrasyl_timer = 20u * 2u;
                    spawned.petrasyl_secondary_timer = 20u * 2u;
                    spawned.invulnerable = true;
                } else {
                    spawned.facing = normalized_or(spawned.facing,
                                                   {0.0F, 0.0F, 1.0F});
                    spawned.petrasyl_direction = spawned.facing;
                    spawned.petrasyl_bob_offset = static_cast<float>(
                        rng_.random2(0x1aabu) + 1365u) / 4096.0F / 2.0F;
                    spawned.petrasyl_bob_speed = static_cast<float>(
                        rng_.random2(profile.variant == static_cast<std::uint8_t>(
                            formats::EnemyType::Petrasyl2) ? 0x6000u
                                                          : 0x3000u))
                        / 4096.0F + 1.0F;
                    spawned.invulnerable = profile.variant != static_cast<
                        std::uint8_t>(formats::EnemyType::Petrasyl2);
                    if (profile.variant == static_cast<std::uint8_t>(
                            formats::EnemyType::Petrasyl2)) {
                        spawned.state = 1;
                    } else {
                        spawned.petrasyl_timer = profile.variant == static_cast<
                            std::uint8_t>(formats::EnemyType::Petrasyl3)
                            ? 20u * 2u : 10u * 2u;
                        spawned.petrasyl_secondary_timer = profile.variant ==
                            static_cast<std::uint8_t>(formats::EnemyType::Petrasyl4)
                            ? 10u * 2u : 0u;
                    }
                }
            }
            if (spawned.zoomer.supported) {
                spawned.behavior_surface_normal = normalized_or(
                    spawned.up, {0.0F, 1.0F, 0.0F});
                spawned.behavior_direction = normalized_or(
                    cross(spawned.facing, spawned.behavior_surface_normal),
                    {1.0F, 0.0F, 0.0F});
                spawned.behavior_intended_direction =
                    spawned.behavior_direction;
            }
            spawned.authored_motion = enemy::decode_authored_motion(
                spawned.enemy_type, runtime.data.fields, spawned.position);
            spawned.barbed_warwasp = enemy::decode_barbed_warwasp_profile(
                spawned.enemy_type, runtime.data.fields);
            spawned.turret = enemy::decode_turret_profile(
                spawned.enemy_type, runtime.data.fields, spawned.position);
            if (spawned.authored_motion.supported
                && spawned.authored_motion.movement_type != 0
                && spawned.authored_motion.path_count > 1
                && spawned.authored_motion.movement_type == 1) {
                // WarWasp's box patrol starts at corner 1; vector patrols
                // start at corner 0 in the managed constructor.
                spawned.motion_index = 1;
            }
            spawned.health = tuning.health;
            spawned.health_max = tuning.health;
            spawned.body_radius = tuning.body_radius;
            if (spawned.enemy_type == static_cast<std::uint8_t>(
                    formats::EnemyType::Blastcap)) {
                enemy::module_16_blastcap::EnemyInitialize(spawned);
            } else if (spawned.enemy_type == static_cast<std::uint8_t>(
                    formats::EnemyType::CrashPillar)) {
                enemy::module_38_crash_pillar::EnemyInitialize(spawned);
            } else if (spawned.enemy_type == static_cast<std::uint8_t>(
                    formats::EnemyType::WarWasp)) {
                enemy::module_00_war_wasp::EnemyInitialize(spawned);
            } else if (spawned.enemy_type == static_cast<std::uint8_t>(
                    formats::EnemyType::BarbedWarWasp)) {
                enemy::module_10_barbed_war_wasp::EnemyInitialize(spawned);
            } else if (spawned.enemy_type == static_cast<std::uint8_t>(
                    formats::EnemyType::Petrasyl1)) {
                enemy::module_03_petrasyl1::EnemyInitialize(spawned);
            } else if (spawned.enemy_type == static_cast<std::uint8_t>(
                    formats::EnemyType::Petrasyl2)) {
                enemy::module_04_petrasyl2::EnemyInitialize(spawned);
            } else if (spawned.enemy_type == static_cast<std::uint8_t>(
                    formats::EnemyType::Petrasyl3)) {
                enemy::module_05_petrasyl3::EnemyInitialize(spawned);
            }
            if (spawned.voldrum.supported) {
                const auto& profile = spawned.voldrum;
                spawned.health = spawned.health_max = profile.health;
                spawned.body_radius = 0.5F;
                spawned.voldrum_speed_factor = profile.min_speed_factor;
                spawned.voldrum_move_target = to_net(
                    profile.home_volume.center());
                spawned.voldrum_move_target_valid = true;
                spawned.voldrum_delay_timer = static_cast<std::uint32_t>(
                    profile.delay_frames) * 2u;
                spawned.voldrum_shot_timer = static_cast<std::uint32_t>(
                    profile.shot_frames) * 2u;
                if (profile.max_shots >= profile.min_shots) {
                    const auto range = static_cast<std::uint32_t>(
                        profile.max_shots - profile.min_shots + 1u);
                    spawned.voldrum_shots_remaining = static_cast<std::uint16_t>(
                        profile.min_shots + rng_.random2(range));
                }
            }
            if (spawned.psychobit.supported) {
                const auto& profile = spawned.psychobit;
                spawned.health = spawned.health_max = profile.health;
                spawned.body_radius = 1.0F;
                spawned.psychobit_speed_factor = profile.min_speed_factor1;
                spawned.psychobit_move_target = to_net(
                    profile.home_volume.center());
                spawned.psychobit_move_target_valid = true;
                spawned.psychobit_delay_timer = static_cast<std::uint32_t>(
                    profile.delay_frames) * 2u;
                spawned.psychobit_shot_timer = static_cast<std::uint32_t>(
                    profile.shot_frames) * 2u;
                if (profile.max_shots >= profile.min_shots) {
                    const auto range = static_cast<std::uint32_t>(
                        profile.max_shots - profile.min_shots + 1u);
                    spawned.psychobit_shots_remaining =
                        static_cast<std::uint16_t>(
                            profile.min_shots + rng_.random2(range));
                }
            }
            if (spawned.firespawn.supported) {
                const auto& profile = spawned.firespawn;
                spawned.health = spawned.health_max = profile.health;
                spawned.body_radius = 1.0F;
                spawned.visible = false;
                spawned.invulnerable = true;
                spawned.firespawn_submerged = true;
                spawned.firespawn_tangibility_timer = 0;
                const auto dive_range = static_cast<std::uint32_t>(
                    profile.dive_timer_max >= profile.dive_timer_min
                    ? profile.dive_timer_max - profile.dive_timer_min + 1u : 1u);
                spawned.firespawn_dive_timer = static_cast<std::uint32_t>(
                    profile.dive_timer_min + rng_.random2(dive_range)) * 2u;
                const auto attack_range = static_cast<std::uint32_t>(
                    profile.attack_count_max >= profile.attack_count_min
                    ? profile.attack_count_max - profile.attack_count_min + 1u : 1u);
                spawned.firespawn_attacks_remaining =
                    static_cast<std::uint16_t>(profile.attack_count_min
                        + rng_.random2(attack_range));
            }
            if (spawned.carnivorous_plant.supported) {
                spawned.health = spawned.health_max =
                    spawned.carnivorous_plant.health != 0
                    ? spawned.carnivorous_plant.health : tuning.health;
                spawned.body_radius = 1843.0F / 4096.0F;
            }
            if (spawned.ithrak.supported) {
                spawned.health = spawned.health_max = spawned.ithrak.health;
                spawned.body_radius = 0.5F;
                spawned.ithrak_move_target = to_net(
                    spawned.ithrak.home_volume.center());
                spawned.ithrak_delay_timer = 30u * 2u;
                spawned.ithrak_move_timer = 600u * 2u;
                spawned.ithrak_move_start = spawned.position;
                spawned.ithrak_target_vec = spawned.facing;
                spawned.ithrak_animation = 0;
                spawned.ithrak_animation_timer = 1;
                spawned.ithrak_drop_angle_sign = 1;
                spawned.ithrak_recoil_angle_sign = 1;
                spawned.ithrak_reaching_target = false;
            }
            if (spawned.quadtroid.supported) {
                spawned.health = spawned.health_max = 120;
                spawned.body_radius = 0.25F;
                spawned.behavior_surface_normal = normalized_or(
                    spawned.up, {0.0F, 1.0F, 0.0F});
                spawned.behavior_direction = normalized_or(
                    cross(spawned.facing, spawned.behavior_surface_normal),
                    {1.0F, 0.0F, 0.0F});
                spawned.behavior_intended_direction =
                    spawned.behavior_direction;
                spawned.quadtroid_right = normalized_or(
                    cross(spawned.facing, spawned.up),
                    {1.0F, 0.0F, 0.0F});
                spawned.quadtroid_turn_direction = spawned.facing;
                spawned.quadtroid_previous_health = spawned.health;
                spawned.quadtroid_idle_timer =
                    (45u + rng_.random2(60u)) * 2u;
                spawned.quadtroid_attack_timer =
                    (90u + rng_.random2(60u)) * 2u;
                spawned.quadtroid_state_timer = 0;
                spawned.quadtroid_initialized = true;
                spawned.quadtroid_grab_timer = 0;
            }
            if (spawned.crash_pillar.supported) {
                spawned.health = spawned.health_max = 150;
                spawned.body_radius = 0.5F;
                spawned.invulnerable = true;
                spawned.crash_pillar_vulnerable_timer = 10u;
                spawned.crash_pillar_cooldown = 30u;
                spawned.crash_pillar_airborne = false;
            }
            if (spawned.cretaphid.supported) {
                const auto& profile = spawned.cretaphid;
                spawned.health = spawned.health_max = profile.health;
                spawned.body_radius = profile.collision_radius;
                spawned.cretaphid_phase = 0;
                spawned.cretaphid_state = 0; // intro/eye cycle
                spawned.cretaphid_state_timer = static_cast<std::uint32_t>(
                    profile.phase_flash_time) * 2u;
                spawned.cretaphid_part_timer = 0;
                spawned.cretaphid_part_shot_timer = 0;
                spawned.cretaphid_crystal_open = false;
                spawned.invulnerable = true;
            }
            if (spawned.slench.supported) {
                const auto& profile = spawned.slench;
                spawned.health = spawned.health_max = profile.health;
                // Enemy41 replaces the raw S00 radius with a fixed 2.9-unit
                // eye volume during initialization and is invulnerable until
                // its shield/synapse cycle opens a damage window.
                spawned.body_radius = 2.9F;
                spawned.slench_phase = 0;
                spawned.slench_state = 0; // Initial
                spawned.slench_static_shots_remaining = 0;
                spawned.slench_synapses_alive = 3;
                spawned.slench_state_timer = 0;
                spawned.slench_shot_timer = 0;
                spawned.slench_roam_timer = 0;
                spawned.slench_cycle_count = 0;
                spawned.slench_pattern_angle = 0.0F;
                spawned.slench_target_angle = spawned.facing.y < -0.5F
                    ? 90.0F : 0.0F;
                spawned.slench_float_base_y = 0.0F;
                spawned.slench_drop_speed = 0.0F;
                spawned.slench_wobble_angle = 0.0F;
                spawned.slench_flags = spawned.slench.subtype == 3
                    ? 0x0002u : 0x0004u; // Rolling : Floating
                spawned.slench_state_after_slam = 0;
                spawned.slench_recoil_timer = 1000;
                spawned.slench_slam_timer = 0;
                spawned.slench_wobble_timer = 0;
                spawned.slench_static_shot_counter = 0;
                spawned.slench_static_shot_cooldown = 0;
                spawned.slench_static_shot_timer = 0;
                spawned.slench_death_timer = 0;
                spawned.slench_roll_timer = 0;
                spawned.slench_target_horizontal = normalized_or(
                    {spawned.facing.x, 0.0F, spawned.facing.z},
                    {0.0F, 0.0F, 1.0F});
                spawned.slench_start_position = spawned.position;
                const auto initial_facing = normalized_or(
                    spawned.facing, {0.0F, 0.0F, 1.0F});
                spawned.slench_detached_position = add(
                    spawned.position, multiply(initial_facing, 3.0F));
                spawned.slench_detached_facing = add(
                    spawned.slench_detached_position, initial_facing);
                spawned.slench_destination1 = spawned.position;
                spawned.slench_destination2 = spawned.position;
                spawned.slench_hit_floor = false;
                spawned.slench_home_position = spawned.position;
                spawned.slench_shielded = true;
                spawned.invulnerable = true;
            }
            if (spawned.turret.supported) {
                spawned.health = spawned.health_max = spawned.turret.health;
                spawned.body_radius = 1.0F;
                const bool is_slench_turret = spawned.enemy_type == static_cast<
                    std::uint8_t>(formats::EnemyType::SlenchTurret);
                spawned.turret_enabled = !is_slench_turret;
                spawned.turret_index = spawned.turret.index;
                spawned.turret_initial_facing = spawned.facing;
                spawned.turret_aim = spawned.facing;
                const auto shot_range = static_cast<std::uint32_t>(
                    spawned.turret.max_shots >= spawned.turret.min_shots
                    ? spawned.turret.max_shots - spawned.turret.min_shots + 1u
                    : 1u);
                spawned.turret_burst_remaining = static_cast<std::uint16_t>(
                    spawned.turret.min_shots + rng_.random2(shot_range));
                spawned.turret_shot_timer = static_cast<std::uint32_t>(
                    spawned.turret.shot_cooldown_frames) * 2u;
                spawned.turret_delay_timer = is_slench_turret ? 0u
                    : static_cast<std::uint32_t>(
                        spawned.turret.delay_frames) * 2u;
                spawned.turret_salvo_cooldown = 0;
            }
            if (spawned.enemy_type == static_cast<std::uint8_t>(
                    formats::EnemyType::WarWasp)) {
                const auto movement_type =
                    spawned.authored_motion.movement_type;
                spawned.health = spawned.health_max =
                    movement_type == 3 ? 8 : 40;
            }
            if (spawned.barbed_warwasp.supported) {
                const auto& profile = spawned.barbed_warwasp;
                spawned.health = spawned.health_max = profile.health;
                spawned.body_radius = 1.0F;
                spawned.wasp_shot_count = static_cast<std::uint16_t>(
                    profile.min_shots + rng_.random2(
                        static_cast<std::uint32_t>(
                            profile.max_shots >= profile.min_shots
                            ? profile.max_shots - profile.min_shots + 1u : 1u)));
                spawned.wasp_shot_timer = 30u * 2u;
            }
            if (spawned.authored_motion.supported
                && (spawned.enemy_type == static_cast<std::uint8_t>(
                        formats::EnemyType::WarWasp)
                    || spawned.barbed_warwasp.supported)) {
                const auto& motion = spawned.authored_motion;
                spawned.wasp_initial_position = spawned.position;
                spawned.wasp_attack_target = spawned.position;
                spawned.wasp_move_index = motion.movement_type == 1 ? 1 : 0;
                spawned.wasp_max_move_index = motion.path_count > 0
                    ? static_cast<std::uint8_t>(motion.path_count - 1) : 0;
                spawned.wasp_final_move_index = spawned.wasp_max_move_index;
                spawned.wasp_move_positions = motion.path_positions;
                spawned.wasp_move_target = spawned.wasp_move_positions[
                    std::min<std::size_t>(spawned.wasp_move_index,
                                          spawned.wasp_move_positions.size() - 1)];
                spawned.wasp_attack_delay = 30u * 2u;
                spawned.wasp_circle_angle = 0.0F;
            }
            const auto enemy_type = static_cast<formats::EnemyType>(
                spawned.enemy_type);
            // Gorea classes use a 0xffff health sentinel and communicate
            // progress through linked child records. Initialize that shape
            // before the record is published so no generic enemy frame can
            // run against a boss part half-initialized.
            if (enemy_type == formats::EnemyType::Gorea1A) {
                spawned.health = spawned.health_max = 65535;
                spawned.body_radius = 4098.0F / 4096.0F;
                spawned.invulnerable = true;
                spawned.gorea_state_timer = 60u * 2u;
                spawned.gorea_aux_timer = 210u * 2u;
                spawned.gorea_attack_timer = 510u * 2u;
                spawned.gorea_phases_left = 3;
                spawned.gorea_weapon_index = 5;
                spawned.gorea_arm_bits = 3;
                spawned.gorea_model_animation = 17;
                spawned.gorea_animation_length = 51;
                spawned.gorea_field23e = 510 * 2;
                spawned.gorea_field240 = static_cast<std::int32_t>(
                    rng_.random2(90u) + 150u) * 2;
                spawned.gorea_speed_factor = 341.0F / 4096.0F;
                spawned.gorea_flags |= gorea::Gorea1AReveal
                    | gorea::Gorea1AArmInvulnerable
                    | gorea::Gorea1AHeadFlashPending;
                // Enemy24 uses the first of the two S11 spheres as its
                // movement volume.  The fields are local to the spawner
                // header, just like CollisionVolume.Move in the C# class.
                spawned.gorea_anchor = add(
                    spawned.position,
                    read_enemy_field_vec3(runtime.data.fields, 0));
                spawned.gorea_volume_radius = read_enemy_field_fixed(
                    runtime.data.fields, 12);
                spawned.gorea_activated = true;
            } else if (enemy_type == formats::EnemyType::Gorea1B) {
                spawned.health = spawned.health_max = 65535;
                spawned.body_radius = 4098.0F / 4096.0F;
                spawned.invulnerable = true;
                spawned.visible = false;
                spawned.gorea_state_timer = 120u * 2u;
                spawned.gorea_phases_left = 3;
                spawned.gorea_model_animation = 3;
                spawned.gorea_animation_length = 60;
                spawned.gorea_field240 = 30 * 2;
                spawned.gorea_anchor = add(
                    spawned.position,
                    read_enemy_field_vec3(runtime.data.fields, 16));
                spawned.gorea_volume_radius = read_enemy_field_fixed(
                    runtime.data.fields, 28);
                spawned.gorea_activated = false;
            } else if (enemy_type == formats::EnemyType::Gorea2) {
                spawned.health = spawned.health_max = 65535;
                spawned.body_radius = 1.0F;
                spawned.invulnerable = true;
                spawned.gorea_state_timer = 90u * 2u;
                spawned.gorea_aux_timer = 120u * 2u;
                spawned.gorea_attack_timer = 22u * 2u;
                spawned.gorea_model_animation = 7;
                spawned.gorea_animation_length = 43;
                spawned.gorea_light_mask = 0x7f;
                spawned.gorea_field22c = 90u * 2u;
                spawned.gorea_field22e = 120u * 2u;
                spawned.gorea_field234 = 22u * 2u;
                // Enemy31 reads S12.Field28 directly as the centre of its
                // movement volume.  Unlike Enemy24/28, it does not move this
                // vector by the spawner transform.
                spawned.gorea_anchor = read_enemy_field_vec3(
                    runtime.data.fields, 0);
                spawned.gorea_field23e = 180;
                spawned.gorea_field240 = 0;
                spawned.gorea_meteor_count = 0;
                spawned.gorea_flags |= gorea::Gorea2IntroDone
                    | gorea::Gorea2Subroutine;
                // _field20C starts at the entity position and is then moved
                // towards Func21405FC by the managed hover-centre helper.
                spawned.gorea_target_position = spawned.position;
                spawned.gorea_volume_radius = read_enemy_field_fixed(
                    runtime.data.fields, 12);
                spawned.gorea_volume_height = read_enemy_field_fixed(
                    runtime.data.fields, 16);
                spawned.gorea_activated = true;
            } else if (enemy_type == formats::EnemyType::GoreaHead) {
                spawned.health = spawned.health_max = 65535;
                spawned.invulnerable = true;
                spawned.visible = false;
            } else if (enemy_type == formats::EnemyType::GoreaArm) {
                spawned.health = 65535;
                spawned.health_max = 120;
                spawned.invulnerable = true;
                spawned.visible = false;
            } else if (enemy_type == formats::EnemyType::GoreaLeg) {
                spawned.health = 65535;
                spawned.health_max = 120;
                spawned.invulnerable = true;
                spawned.visible = false;
            } else if (enemy_type == formats::EnemyType::GoreaSealSphere1) {
                spawned.health = 65535;
                spawned.health_max = 3000;
                spawned.invulnerable = true;
                spawned.visible = false;
            } else if (enemy_type == formats::EnemyType::GoreaSealSphere2) {
                spawned.health = 65535;
                spawned.health_max = 840;
                spawned.invulnerable = true;
                spawned.visible = false;
            } else if (enemy_type == formats::EnemyType::Shriekbat) {
                enemy::module_11_shriekbat::EnemyInitialize(spawned);
            } else if (enemy_type == formats::EnemyType::Trocra) {
                spawned.health = spawned.health_max = 15;
                spawned.body_radius = 1.0F;
                spawned.trocra_state = 0;
                spawned.trocra_slot = 0xff;
                spawned.trocra_state_timer = 0;
                spawned.trocra_field174 = spawned.position;
                spawned.trocra_previous_position = spawned.position;
            } else if (enemy_type == formats::EnemyType::GoreaMeteor) {
                spawned.health = spawned.health_max = 8;
                spawned.body_radius = 1.0F;
                spawned.gorea_state_timer = 150u * 2u;
                spawned.gorea_aux_timer = 390u * 2u;
                spawned.gorea_meteor_base_position = spawned.position;
                spawned.gorea_meteor_previous_position = spawned.position;
                spawned.gorea_meteor_effect_up = {0.0F, 1.0F, 0.0F};
                spawned.gorea_meteor_effect_facing = spawned.facing;
                spawned.gorea_meteor_rotation = 0.0F;
                spawned.gorea_activated = true;
            }
            spawned.active = true;
            enemies_.push_back(spawned);
            if (enemy_type == formats::EnemyType::Gorea1A) {
                const auto parent_id = spawned.id;
                const auto add_gorea_child = [this, &spawned, &runtime, parent_id](
                                                 std::uint8_t type,
                                                 std::uint8_t index) {
                    EnemyState child;
                    child.id = next_enemy_id_++;
                    child.parent_enemy_id = parent_id;
                    child.enemy_type = type;
                    child.position = spawned.position;
                    child.behavior_origin = spawned.position;
                    child.facing = spawned.facing;
                    child.up = spawned.up;
                    child.spawner_entity_id = -1;
                    child.gorea_index = index;
                    child.gorea_activated = false;
                    child.gorea_model_animation = 0;
                    child.gorea_animation_length = type == static_cast<std::uint8_t>(
                        formats::EnemyType::Gorea1B) ? 60 : 30;
                    child.active = true;
                    child.visible = false;
                    child.invulnerable = true;
                    child.health = 65535;
                    child.health_max = 120;
                    switch (static_cast<formats::EnemyType>(type)) {
                    case formats::EnemyType::GoreaHead:
                        child.health_max = 65535;
                        break;
                    case formats::EnemyType::GoreaSealSphere1:
                        child.health_max = 3000;
                        break;
                    case formats::EnemyType::Gorea1B:
                        child.gorea_anchor = add(
                            spawned.position,
                            read_enemy_field_vec3(runtime.data.fields, 16));
                        child.gorea_volume_radius =
                            read_enemy_field_fixed(runtime.data.fields, 28);
                        // Enemy28Entity.InitializeCommon initializes these
                        // grapple-render/spring constants before the first
                        // State04 callback. They remain simulation data in
                        // native because the 24-point chain controls the
                        // player's position during States06-09.
                        child.gorea_grapple_field24 = 0.65F;
                        child.gorea_grapple_field28 = 1.0F / 3.0F;
                        child.gorea_grapple_field30 = 1.0F;
                        child.gorea_grapple_field34 = 0.25F;
                        child.gorea_grapple_timer = 120u * 2u;
                        child.gorea_grapple_damage_timer =
                            (rng_.random2(13u) + 7u) * 2u;
                        child.gorea_swing_direction =
                            rng_.random2(255u) % 2u != 0;
                        if (child.gorea_swing_direction) {
                            child.gorea_flags |= gorea::Gorea1BSwingDirection;
                        }
                        child.gorea_hold_timer = 150u * 2u;
                        child.gorea_swing_timer = 30u * 2u;
                        break;
                    default:
                        break;
                    }
                    const auto child_id = child.id;
                    const auto child_position = child.position;
                    const auto child_facing = child.facing;
                    const auto child_up = child.up;
                    enemies_.push_back(std::move(child));
                    if (type == static_cast<std::uint8_t>(
                                   formats::EnemyType::Gorea1B)) {
                        // Enemy28Entity.InitializeCommon creates its
                        // Enemy29Entity seal sphere as a linked child.  The
                        // native 1A spawner used to create only the 1B body,
                        // leaving every phase-damage and grapple test without
                        // the actual ChestBall1 target.
                        EnemyState sphere;
                        sphere.id = next_enemy_id_++;
                        sphere.parent_enemy_id = child_id;
                        sphere.enemy_type = static_cast<std::uint8_t>(
                            formats::EnemyType::GoreaSealSphere1);
                        sphere.position = child_position;
                        sphere.behavior_origin = child_position;
                        sphere.facing = child_facing;
                        sphere.up = child_up;
                        sphere.health = 65535;
                        sphere.health_max = 3000;
                        sphere.gorea_model_animation = 3;
                        sphere.gorea_animation_length = 60;
                        sphere.gorea_grapple_field24 = 0.65F;
                        sphere.gorea_grapple_field28 = 1.0F / 3.0F;
                        sphere.gorea_grapple_field30 = 1.0F;
                        sphere.gorea_grapple_field34 = 0.25F;
                        sphere.gorea_grapple_timer = 120u * 2u;
                        sphere.active = true;
                        sphere.visible = false;
                        sphere.invulnerable = true;
                        sphere.spawner_entity_id = -1;
                        enemies_.push_back(std::move(sphere));
                    }
                };
                add_gorea_child(static_cast<std::uint8_t>(
                                    formats::EnemyType::GoreaHead), 0);
                add_gorea_child(static_cast<std::uint8_t>(
                                    formats::EnemyType::GoreaArm), 0);
                add_gorea_child(static_cast<std::uint8_t>(
                                    formats::EnemyType::GoreaArm), 1);
                for (std::uint8_t index = 0; index < 3; ++index) {
                    add_gorea_child(static_cast<std::uint8_t>(
                                        formats::EnemyType::GoreaLeg), index);
                }
                add_gorea_child(static_cast<std::uint8_t>(
                                    formats::EnemyType::Gorea1B), 0);
            } else if (enemy_type == formats::EnemyType::Gorea1B) {
                EnemyState child;
                child.id = next_enemy_id_++;
                child.parent_enemy_id = spawned.id;
                child.enemy_type = static_cast<std::uint8_t>(
                    formats::EnemyType::GoreaSealSphere1);
                child.position = spawned.position;
                child.behavior_origin = spawned.position;
                child.facing = spawned.facing;
                child.up = spawned.up;
                child.health = 65535;
                child.health_max = 3000;
                child.gorea_index = 0;
                child.gorea_model_animation = 3;
                child.gorea_animation_length = 60;
                child.gorea_anchor = add(
                    spawned.position,
                    read_enemy_field_vec3(runtime.data.fields, 16));
                child.gorea_volume_radius = read_enemy_field_fixed(
                    runtime.data.fields, 28);
                child.gorea_grapple_field24 = 0.65F;
                child.gorea_grapple_field28 = 1.0F / 3.0F;
                child.gorea_grapple_field30 = 1.0F;
                child.gorea_grapple_field34 = 0.25F;
                child.gorea_grapple_timer = 120u * 2u;
                child.gorea_grapple_damage_timer =
                    (rng_.random2(13u) + 7u) * 2u;
                child.gorea_swing_direction =
                    rng_.random2(255u) % 2u != 0;
                if (child.gorea_swing_direction) {
                    child.gorea_flags |= gorea::Gorea1BSwingDirection;
                }
                child.gorea_hold_timer = 150u * 2u;
                child.gorea_swing_timer = 30u * 2u;
                child.active = true;
                child.visible = false;
                child.invulnerable = true;
                child.spawner_entity_id = -1;
                enemies_.push_back(std::move(child));
            } else if (enemy_type == formats::EnemyType::Gorea2) {
                EnemyState child;
                child.id = next_enemy_id_++;
                child.parent_enemy_id = spawned.id;
                child.enemy_type = static_cast<std::uint8_t>(
                    formats::EnemyType::GoreaSealSphere2);
                child.position = spawned.position;
                child.behavior_origin = spawned.position;
                child.facing = spawned.facing;
                child.up = spawned.up;
                child.health = 65535;
                child.health_max = 840;
                child.gorea_model_animation = 7;
                child.gorea_animation_length = 43;
                child.gorea_anchor = add(
                    spawned.position,
                    read_enemy_field_vec3(runtime.data.fields, 0));
                child.gorea_volume_radius = read_enemy_field_fixed(
                    runtime.data.fields, 12);
                child.gorea_volume_height = read_enemy_field_fixed(
                    runtime.data.fields, 16);
                child.active = true;
                child.visible = false;
                child.invulnerable = true;
                child.spawner_entity_id = -1;
                enemies_.push_back(std::move(child));
            }
            if (spawned.slench.supported) {
                const auto parent_id = spawned.id;
                const auto subtype = spawned.slench.subtype;
                const auto add_slench_part = [this, &spawned, parent_id](
                                                  std::uint8_t enemy_type,
                                                  std::uint8_t part_index) {
                    EnemyState child;
                    child.id = next_enemy_id_++;
                    child.parent_enemy_id = parent_id;
                    child.enemy_type = enemy_type;
                    child.position = spawned.position;
                    child.behavior_origin = spawned.position;
                    child.facing = spawned.facing;
                    child.up = spawned.up;
                    child.slench_part_index = part_index;
                    child.active = true;
                    child.invulnerable = true;
                    child.spawner_entity_id = -1;
                    if (enemy_type == static_cast<std::uint8_t>(
                            formats::EnemyType::SlenchShield)) {
                        child.slench_shield = enemy::slench_shield_profile();
                        child.health = child.health_max =
                            child.slench_shield.health;
                        child.body_radius = child.slench_shield.hurt_radius;
                        child.visible = true;
                    } else {
                        child.slench_synapse = enemy::slench_synapse_profile(
                            spawned.slench.subtype, part_index);
                        child.health = child.health_max =
                            child.slench_synapse.phases[0].health;
                        child.body_radius = child.slench_synapse.phases[0]
                            .collision_radius;
                        child.slench_part_state = 0; // Initial
                        child.visible = false;
                    }
                    enemies_.push_back(std::move(child));
                };
                add_slench_part(static_cast<std::uint8_t>(
                                    formats::EnemyType::SlenchShield),
                                0xff);
                for (std::uint8_t index = 0; index < 3; ++index) {
                    add_slench_part(static_cast<std::uint8_t>(
                                        formats::EnemyType::SlenchSynapse),
                                    index);
                }
                static_cast<void>(subtype);
            }
            if (spawned.cretaphid.supported) {
                const auto parent_id = spawned.id;
                const auto profile = spawned.cretaphid;
                const auto add_cretaphid_part =
                    [this, &spawned, parent_id, profile](
                        std::uint8_t part_kind,
                        std::uint8_t part_index) {
                        EnemyState child;
                        child.id = next_enemy_id_++;
                        child.parent_enemy_id = parent_id;
                        child.enemy_type = part_kind == 1
                            ? static_cast<std::uint8_t>(
                                formats::EnemyType::CretaphidEye)
                            : static_cast<std::uint8_t>(
                                formats::EnemyType::CretaphidCrystal);
                        child.position = spawned.position;
                        child.behavior_origin = spawned.position;
                        child.facing = spawned.facing;
                        child.up = spawned.up;
                        child.cretaphid = profile;
                        child.cretaphid_part_kind = part_kind;
                        child.cretaphid_part_index = part_index;
                        child.cretaphid_phase = 0;
                        child.cretaphid_part_state = 0;
                        child.cretaphid_beam_type = part_kind == 1
                            ? profile.phases[0].eye_beam_type[part_index]
                            : 0;
                        child.cretaphid_segment_index = part_kind == 1
                            ? static_cast<std::uint8_t>(part_index / 4u) : 0;
                        child.active = true;
                        child.visible = true;
                        child.invulnerable = true;
                        child.spawner_entity_id = -1;
                        if (part_kind == 1) {
                            child.health = child.health_max = profile.eye_health;
                            child.body_radius = 0.5F;
                            child.state = profile.phases[0].eye_state[
                                part_index];
                        } else {
                            child.health = child.health_max =
                                profile.crystal_health;
                            child.body_radius = 1.0F;
                            child.state = 0;
                        }
                        enemies_.push_back(std::move(child));
                    };
                for (std::uint8_t index = 0; index < 12; ++index) {
                    add_cretaphid_part(1, index);
                }
                add_cretaphid_part(2, 0);
            }
            if (spawned.firespawn.supported || spawned.ithrak.supported) {
                // Enemy50 is created by Enemy39/46/47 itself.  It carries the
                // collision volume that opens during the parent's state
                // machine; hits are forwarded to the parent below.
                EnemyState hit_zone;
                hit_zone.id = next_enemy_id_++;
                hit_zone.parent_enemy_id = spawned.id;
                hit_zone.enemy_type = static_cast<std::uint8_t>(
                    formats::EnemyType::HitZone);
                hit_zone.position = spawned.position;
                hit_zone.behavior_origin = spawned.position;
                hit_zone.facing = spawned.facing;
                hit_zone.up = spawned.up;
                hit_zone.health = hit_zone.health_max =
                    spawned.firespawn.supported ? 1 : spawned.health;
                hit_zone.body_radius = 1.0F;
                hit_zone.hit_zone_effectiveness =
                    spawned.firespawn.supported
                    ? spawned.firespawn.effectiveness : 0xffffu;
                hit_zone.hit_zone_collidable = spawned.ithrak.supported;
                hit_zone.active = true;
                hit_zone.visible = spawned.ithrak.supported;
                hit_zone.invulnerable = true;
                hit_zone.spawner_entity_id = -1;
                enemies_.push_back(std::move(hit_zone));
            }
        }
        if (!result.deactivated) {
            continue;
        }
        complete_enemy_spawner(runtime.entity_id,
                               runtime.spawner->enemy_type(), runtime.data);
    }
}

} // namespace fruityprime::gameplay

static_assert(fruityprime::enemy::module_40_enemy_spawner::kModule.id == 40);
