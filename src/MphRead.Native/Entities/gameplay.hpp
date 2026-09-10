#pragma once

#include "Mods/Network/net_protocol.hpp"
#include "Utility/rng.hpp"
#include "Entities/runtime_entities.hpp"
#include "Entities/scene.hpp"
#include "Entities/Enemies/enemy_scene.hpp"
#include "Formats/collision_query.hpp"
#include "Formats/collision_runtime.hpp"
#include "GameState.hpp"
#include "Formats/model_instance.hpp"
#include "Entities/Players/player_profile.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace fruityprime::gameplay {

struct Config {
    float tick_seconds = 1.0F / 60.0F;
    float walk_speed = 4.0F;
    float air_acceleration = 18.0F;
    float gravity = -18.0F;
    float jump_speed = 6.0F;
    float body_radius = 0.45F;
    float world_padding = 0.5F;
    std::uint16_t max_health = 100;
    std::uint32_t respawn_ticks = 90;
    std::uint8_t mode = 3;
    std::uint16_t point_goal = 7;
    bool team_mode = false;
    bool friendly_fire = false;
    bool survival_mode = false;
    std::uint16_t survival_lives = 2;
    bool objective_authority = true;
};

struct Input {
    net::IntentButtons buttons = net::IntentButtons::None;
    net::Vec3 aim{0.0F, 0.0F, 1.0F};
    // 0xff means no direct selection. This mirrors IntentState's explicit
    // weapon-select field and keeps keyboard/controller selection from being
    // confused with the edge-triggered cycle buttons.
    std::uint8_t weapon_select = 0xff;
};

using Projectile = runtime::BeamProjectile;

// ItemType values are the managed Formats.ItemType values stored directly in
// standard room ItemSpawn records. Keeping the cartridge numbering here makes
// the gameplay layer independent of the renderer's item-model catalogue.
enum class ItemType : std::int32_t {
    None = -1,
    HealthMedium = 0,
    HealthSmall = 1,
    HealthBig = 2,
    DoubleDamage = 3,
    EnergyTank = 4,
    VoltDriver = 5,
    MissileExpansion = 6,
    Battlehammer = 7,
    Imperialist = 8,
    Judicator = 9,
    Magmaul = 10,
    ShockCoil = 11,
    OmegaCannon = 12,
    UASmall = 13,
    UABig = 14,
    MissileSmall = 15,
    MissileBig = 16,
    Cloak = 17,
    UAExpansion = 18,
    ArtifactKey = 19,
    Deathalt = 20,
    AffinityWeapon = 21,
    PickWpnMissile = 22
};

struct ItemState {
    ItemType type = ItemType::None;
    net::Vec3 position;
    std::int16_t owner_entity_id = -1;
    std::uint32_t remaining_ticks = 0;
    bool timed = false;
};

// Local renderer event emitted by the deterministic simulation. Effects are
// deliberately not part of SnapshotPacket: they are presentation-only, while
// their trigger (a muzzle shot or a collision) is already replicated by the
// authoritative projectile/player state.
struct EffectState {
    std::uint32_t id = 0;
    std::uint16_t effect_id = 0;
    std::uint32_t source_entity_id = 0;
    net::Vec3 position;
    net::Vec3 direction{0.0F, 0.0F, 1.0F};
    net::Vec3 up{0.0F, 1.0F, 0.0F};
    float remaining_seconds = 0.0F;
    float lifespan_seconds = 0.0F;
    bool no_splat = false;
    // A managed EffectEntry can outlive the short gameplay event that created
    // it (for example Gorea's laser charge).  Persistent entries are kept in
    // this queue until the controller explicitly detaches them; the renderer
    // then applies the same lifetime/extension rules as the cartridge effect.
    bool persistent = false;
    bool detached = false;
    bool detached_set_expired = true;
    bool element_extension = false;
};

// Scene.AddSingleParticle is a render-frame queue in the managed renderer:
// entries are discarded after the current frame and are not network state.
// Keep the same boundary in the shared simulation so every frontend can
// consume transient particles without duplicating projectile timing logic.
enum class SingleParticleType : std::uint8_t {
    Death,
    Fuzzball,
    Lore,
    LoreDim,
    Enemy,
    EnemyDim,
    Object,
    ObjectDim,
    Equipment,
    EquipmentDim,
    Red,
    RedDim
};

struct SingleParticleState {
    SingleParticleType type = SingleParticleType::Fuzzball;
    net::Vec3 position;
    net::Vec3 color{1.0F, 1.0F, 1.0F};
    float alpha = 1.0F;
    float scale = 0.25F;
};

// Sound is a presentation side effect just like a single particle.  Keep the
// cue in the shared simulation so Win32, replay, and future frontends all hear
// the same authoritative shot/hit/pickup boundary without putting audio data
// on the network wire.
enum class SoundCue : std::uint8_t {
    BeamShot,
    TurretAttack,
    GoreaAttack3A,
    GoreaAttack3B,
    Gorea1BDamage,
    Gorea2Damage,
    BeamImpact,
    PlayerDamage,
    PlayerDeath,
    PlayerSpawn,
    PlayerJump,
    ItemSpawn,
    ItemPickup,
    EnemyDamage,
    EnemyDeath
};

enum class BeamImpactKind : std::uint8_t {
    Surface,
    Player,
    Enemy
};

struct SoundEvent {
    SoundCue cue = SoundCue::BeamShot;
    BeamImpactKind impact = BeamImpactKind::Surface;
    std::uint8_t slot = 0xff;
    std::uint8_t weapon = 0xff;
    std::uint8_t enemy_type = 0xff;
    ItemType item_type = ItemType::None;
    std::uint32_t entity_id = 0;
    net::Vec3 position;
};

// A lightweight native enemy instance record.  EnemySpawnerEntity owns the
// cartridge lifecycle counters; this record is the gameplay-facing result of
// a spawn until the individual enemy AI classes are migrated.
struct EnemyState {
    std::uint32_t id = 0;
    // Child parts created by a boss have no room spawner of their own.  Keep
    // their parent ID so destruction and message/state updates can follow the
    // same ownership boundary as the managed EnemyInstanceEntity tree.
    std::uint32_t parent_enemy_id = 0;
    std::int16_t spawner_entity_id = -1;
    std::uint8_t enemy_type = 0;
    net::Vec3 position;
    net::Vec3 velocity;
    net::Vec3 facing{0.0F, 0.0F, 1.0F};
    net::Vec3 up{0.0F, 1.0F, 0.0F};
    std::uint16_t health = 20;
    std::uint16_t health_max = 20;
    std::uint8_t state = 0;
    // EnemyInstanceEntity._state2 and _subId: what the enemy will be doing
    // next frame, and which state's behaviour list to read.  A transition
    // is decided during a frame and taken at the start of the next one,
    // which is why these are not the same field as `state`.
    std::uint8_t next_state = 0;
    std::uint8_t sub_id = 0;
    // EnemyInstanceEntity._scanId.  Most enemies take it from their
    // metadata and never change it; the ones with phases change it,
    // because a closed eye and an open one are different things to read
    // about.
    std::uint16_t scan_id = 0;
    // Enemy49Entity's own effectiveness table.  A force field lock is
    // immune to everything except the one beam its field is keyed to,
    // which is the whole puzzle -- so it does not share the enemy
    // metadata's table.
    std::array<std::uint8_t, 9> lock_effectiveness{};
    std::uint8_t lock_shot_frames = 0;
    // EntityBase.Recolor: which palette this one wears.  A few enemies
    // pick it from their authored version rather than wearing one.
    std::uint8_t recolor = 0;
    std::uint8_t target_slot = 0xff;
    float attack_timer = 0.0F;
    float body_radius = 0.65F;
    enemy::AuthoredMotion authored_motion;
    enemy::BarbedWarWaspProfile barbed_warwasp;
    enemy::ZoomerProfile zoomer;
    enemy::PsychoBitProfile psychobit;
    enemy::FireSpawnProfile firespawn;
    enemy::VoldrumProfile voldrum;
    enemy::TemroidProfile temroid;
    enemy::PetrasylProfile petrasyl;
    enemy::CarnivorousPlantProfile carnivorous_plant;
    enemy::IthrakProfile ithrak;
    enemy::QuadtroidProfile quadtroid;
    enemy::CrashPillarProfile crash_pillar;
    enemy::CretaphidProfile cretaphid;
    enemy::SlenchProfile slench;
    enemy::SlenchShieldProfile slench_shield;
    enemy::SlenchNestProfile slench_nest;
    enemy::SlenchSynapseProfile slench_synapse;
    enemy::ShriekbatProfile shriekbat;
    enemy::TurretProfile turret;
    std::uint8_t motion_index = 0;
    float circle_angle = 0.0F;
    net::Vec3 behavior_direction{0.0F, 0.0F, 1.0F};
    net::Vec3 behavior_intended_direction{0.0F, 0.0F, 1.0F};
    net::Vec3 behavior_surface_normal{0.0F, 1.0F, 0.0F};
    net::Vec3 behavior_origin;
    std::array<net::Vec3, 4> temroid_idle_points{};
    std::uint8_t temroid_idle_index = 0;
    std::uint8_t temroid_attached_slot = 0xff;
    std::uint32_t temroid_state_timer = 0;
    float temroid_phase = 0.0F;
    // Enemy02Entity keeps _state1 (the state being processed) separate from
    // _state2 (the state selected by its subroutine). Native controllers
    // normally expose only one state, but Temroid's 11-entry table observes
    // that one-frame delay, so retain both sides here.
    std::uint8_t temroid_next_state = 0;
    std::uint8_t temroid_sub_id = 0;
    std::uint32_t temroid_field170 = 0;
    std::uint16_t temroid_time_since_damage = 510;
    std::uint32_t temroid_drain_damage_timer = 0;
    net::Vec3 temroid_height_origin;
    std::uint8_t temroid_animation = 0;
    std::uint32_t temroid_animation_timer = 0;
    bool temroid_animation_no_loop = false;
    bool temroid_animation_ended = false;
    bool temroid_field1d0 = false;
    bool temroid_hit_by_bomb = false;
    bool temroid_state_initialized = false;
    // Enemy35Entity's members, which Enemy36Entity inherits.  A Voldrum
    // is a ball that rolls to a point, hops when it gets there, and rams
    // or shoots when it finds you -- so most of what it keeps is about
    // one journey: where it started, where it is going, how far that is,
    // and whether it is still speeding up.
    net::Vec3 voldrum_move_target;
    net::Vec3 voldrum_move_start;
    net::Vec3 voldrum_target_vec;
    float voldrum_speed_factor = 0.0F;
    float voldrum_speed_inc = 0.0F;
    // Enemy36Entity overwrites these three from its own values table;
    // Enemy35Entity leaves them at the constants it was written with.
    float voldrum_speed_inc_amount = 410.0F / 4096.0F / 3.0F / 2.0F;
    float voldrum_min_speed_factor = 0.1F / 2.0F;
    float voldrum_max_speed_factor = 0.2F / 2.0F;
    float voldrum_move_dist_sqr = 0.0F;
    float voldrum_move_dist_sqr_half = 0.0F;
    float voldrum_aim_angle_step = 0.0F;
    // How loud the rolling sound is.  It is a value rather than a switch
    // because it is eased towards where it should be rather than set, so
    // a Voldrum that stops rolling fades out instead of cutting off.
    float voldrum_roll_sfx_amount = 0.0F;
    std::uint16_t voldrum_aim_steps = 0;
    std::uint16_t voldrum_aim_step_count = 10u * 2u;  // todo: FPS stuff
    std::uint16_t voldrum_time_in_air = 0;
    std::uint16_t voldrum_ram_delay = 0;
    std::uint32_t voldrum_delay_timer = 0;
    std::uint32_t voldrum_shot_timer = 0;
    std::uint16_t voldrum_shots_remaining = 0;
    std::int8_t voldrum_roam_angle_sign = 1;
    bool voldrum_move_target_valid = false;
    bool voldrum_increase_speed = false;
    bool voldrum_airborne = false;
    bool voldrum_grounded = false;
    // Enemy35Entity's ram, which costs the player fifteen and happens
    // once per charge.  The cartridge keeps a count here and decrements
    // it, which is only ever one, so this is the bool that behaves the
    // same way.
    bool voldrum_handled_ram_col = false;
    bool voldrum_ram_damage_needed = false;
    // Which row of Metadata.Enemy36Values this one reads.  Only the
    // second Voldrum has a values table; the first is written with its
    // numbers inline.
    std::uint8_t voldrum_subtype = 0;
    net::Vec3 psychobit_move_target;
    float psychobit_speed_factor = 0.0F;
    std::uint32_t psychobit_delay_timer = 0;
    std::uint32_t psychobit_shot_timer = 0;
    std::uint16_t psychobit_shots_remaining = 0;
    std::int8_t psychobit_roam_angle_sign = 1;
    bool psychobit_move_target_valid = false;
    std::uint32_t firespawn_dive_timer = 0;
    std::uint32_t firespawn_attack_timer = 0;
    std::uint32_t firespawn_surface_timer = 0;
    std::uint32_t firespawn_tangibility_timer = 0;
    std::uint16_t firespawn_attacks_remaining = 0;
    bool firespawn_submerged = true;
    // Enemy39Entity's animation cursor.  Four animations: nought and one
    // are the two throws, alternating hands, two is the dive and three is
    // the rise.  Which one is running, and whether it has finished, is
    // what the state machine actually reads -- a Fire Spawn's timings are
    // its animations rather than counters.
    std::uint8_t firespawn_animation = 3;
    std::uint16_t firespawn_animation_frame = 0;
    std::uint16_t firespawn_animation_length = 0;
    bool firespawn_animation_ended = false;
    bool firespawn_animation_paused = true;
    // Enemy39Entity._animFrameCount: a countdown from the throw
    // animation's length, which the throw's own frame numbers are read
    // against.
    std::int32_t firespawn_anim_frame_count = 0;
    // Which hand the next fireball comes from.  It alternates, so a Fire
    // Spawn throws left, right, left.
    std::uint8_t firespawn_wrist_id = 1;
    // The sign on the angle it picks its next surfacing spot at, which
    // alternates so it works back and forth across its pool rather than
    // circling it.
    std::int8_t firespawn_surface_direction = 1;
    // The fireball held in its hand between frames 52 and 26.  Zero means
    // none; the effect is detached rather than left when it is thrown.
    std::uint32_t firespawn_effect_id = 0;
    std::uint8_t firespawn_subtype = 0;
    std::uint32_t shriekbat_timer = 0;
    net::Vec3 shriekbat_target;
    std::uint32_t petrasyl_timer = 0;
    std::uint32_t petrasyl_secondary_timer = 0;
    std::uint32_t petrasyl_turn_timer = 0;
    float petrasyl_bob_angle = 0.0F;
    float petrasyl_bob_offset = 0.0F;
    float petrasyl_bob_speed = 0.0F;
    float petrasyl_weave_angle = 0.0F;
    float petrasyl_target_y = 0.0F;
    net::Vec3 petrasyl_initial_position;
    net::Vec3 petrasyl_idle_limit;
    net::Vec3 petrasyl_direction{0.0F, 0.0F, 1.0F};
    bool petrasyl_teleport_initial = true;
    net::Vec3 ithrak_move_target;
    net::Vec3 ithrak_move_start;
    net::Vec3 ithrak_target_vec{0.0F, 0.0F, 1.0F};
    net::Vec3 ithrak_acceleration;
    float ithrak_move_distance_squared = 0.0F;
    float ithrak_aim_angle_step = 0.0F;
    std::uint32_t ithrak_step_count = 0;
    std::uint32_t ithrak_delay_timer = 0;
    std::uint32_t ithrak_move_timer = 0;
    std::int8_t ithrak_drop_angle_sign = 1;
    std::int8_t ithrak_recoil_angle_sign = 1;
    // Enemy46Entity's animation cursor.  Almost every one of its twenty
    // states waits on an animation ending rather than on a timer, so this
    // is the clock the whole machine runs off.
    std::uint8_t ithrak_animation = 5;
    std::uint32_t ithrak_animation_timer = 0;
    std::uint16_t ithrak_animation_frame = 0;
    std::uint16_t ithrak_animation_length = 0;
    bool ithrak_animation_ended = false;
    bool ithrak_animation_no_loop = false;
    bool ithrak_wall_collision = false;
    bool ithrak_ground_collision = false;
    bool ithrak_reaching_target = false;
    // Enemy46Entity's own effectiveness word, which the two Ithraks
    // differ on: the Lesser resists everything by half, the Greater
    // resists nothing.
    std::uint32_t ithrak_effectiveness = 0x5555;
    // How bright its mouth glows, which is the only thing that tells the
    // two apart at a glance.
    std::uint8_t ithrak_mouth_brightness = 31;
    // Enemy37Entity (Quadtroid/Dripstank) is a surface state machine rather
    // than a generic chase enemy. Keep the managed flags, two idle timers,
    // damage latch, and attachment state on its own record so the controller
    // can reproduce the grab/release/stun transitions without renderer data.
    std::uint8_t quadtroid_flags = 0;
    std::uint16_t quadtroid_previous_health = 120;
    std::uint16_t quadtroid_damage_taken = 0;
    std::uint32_t quadtroid_idle_timer = 0;
    std::uint32_t quadtroid_attack_timer = 0;
    std::uint32_t quadtroid_state_timer = 0;
    net::Vec3 quadtroid_right{1.0F, 0.0F, 0.0F};
    net::Vec3 quadtroid_turn_direction{0.0F, 0.0F, 1.0F};
    net::Vec3 quadtroid_launch_velocity;
    bool quadtroid_initialized = false;
    bool quadtroid_hit_by_bomb = false;
    bool quadtroid_hit_by_beam = false;
    bool quadtroid_attached = false;
    std::uint32_t quadtroid_grab_timer = 0;
    std::uint32_t crash_pillar_vulnerable_timer = 0;
    std::uint32_t crash_pillar_cooldown = 0;
    bool crash_pillar_airborne = false;
    // Enemy38Entity's own state: where it started, where it is turning to,
    // and the timers that pace its jump.  It always goes home afterwards,
    // which is why the starting position and facing are kept rather than
    // recomputed.
    net::Vec3 crash_pillar_initial_position{};
    net::Vec3 crash_pillar_initial_facing{0.0F, 0.0F, 1.0F};
    net::Vec3 crash_pillar_target_vector{1.0F, 0.0F, 0.0F};
    std::uint16_t crash_pillar_jump_timer = 0;
    std::uint16_t crash_pillar_delay_timer = 0;
    std::uint16_t crash_pillar_aim_steps = 0;
    float crash_pillar_jump_height = 0.0F;
    float crash_pillar_aim_angle_step = 0.0F;
    // Which animation is playing, how far into it, and whether it has
    // finished.  Several behaviours wait on an animation rather than a
    // timer, and one reads a particular frame of it.
    std::uint8_t crash_pillar_animation = 1;
    std::uint16_t crash_pillar_animation_frame = 0;
    bool crash_pillar_animation_ended = false;
    // ForceFieldEntity creates an Enemy49Entity lock without an enemy
    // spawner record. Keep the static-field geometry on that child so the
    // shared session can run the same floating/bounce and unlock path.
    std::int16_t force_field_entity_id = -1;
    std::uint8_t force_field_type = 9;
    net::Vec3 force_field_origin;
    net::Vec3 force_field_up{0.0F, 1.0F, 0.0F};
    net::Vec3 force_field_facing{0.0F, 0.0F, 1.0F};
    net::Vec3 force_field_right{1.0F, 0.0F, 0.0F};
    float force_field_width = 0.0F;
    float force_field_height = 0.0F;
    net::Vec3 force_field_velocity;
    std::uint32_t force_field_shot_timer = 0;
    bool force_field_bounce = false;
    // Enemy50Entity is a linked collision volume. Its damage is forwarded to
    // the owning Fire Spawn or Ithrak, but its effectiveness is local to the
    // hit-zone record (Fire Spawn inherits its owner table; Ithrak uses
    // 0xFFFF: double all non-Omega weapons).
    std::uint32_t hit_zone_effectiveness = 0x2AAAA;
    bool hit_zone_collidable = false;
    // Cretaphid keeps its twelve eyes and crystal as native child records.
    // part_kind 1 is an eye, 2 is the crystal; the parent uses kind 0.
    std::uint8_t cretaphid_part_kind = 0;
    std::uint8_t cretaphid_part_index = 0xff;
    std::uint8_t cretaphid_part_state = 0;
    std::uint8_t cretaphid_beam_type = 2;
    std::uint8_t cretaphid_segment_index = 0;
    std::uint16_t cretaphid_part_shots_remaining = 0;
    std::uint8_t cretaphid_phase = 0;
    std::uint8_t cretaphid_state = 0;
    std::uint32_t cretaphid_state_timer = 0;
    std::uint32_t cretaphid_part_timer = 0;
    std::uint32_t cretaphid_part_shot_timer = 0;
    bool cretaphid_crystal_open = false;
    std::uint8_t slench_part_index = 0xff;
    std::uint8_t slench_part_state = 0;
    std::uint32_t slench_part_timer = 0;
    std::uint32_t slench_part_heal_timer = 0;
    std::uint32_t slench_part_reappear_timer = 0;
    std::uint8_t slench_phase = 0;
    std::uint8_t slench_state = 0;
    std::uint8_t slench_static_shots_remaining = 0;
    std::uint8_t slench_synapses_alive = 3;
    std::uint32_t slench_state_timer = 0;
    std::uint32_t slench_shot_timer = 0;
    std::uint32_t slench_roam_timer = 0;
    std::uint32_t slench_cycle_count = 0;
    float slench_pattern_angle = 0.0F;
    float slench_target_angle = 0.0F;
    float slench_float_base_y = 0.0F;
    float slench_drop_speed = 0.0F;
    float slench_wobble_angle = 0.0F;
    std::uint16_t slench_flags = 0;
    std::uint8_t slench_state_after_slam = 0;
    std::uint32_t slench_recoil_timer = 1000;
    std::uint32_t slench_slam_timer = 0;
    std::uint32_t slench_wobble_timer = 0;
    std::uint32_t slench_static_shot_counter = 0;
    std::uint32_t slench_static_shot_cooldown = 0;
    std::uint32_t slench_static_shot_timer = 0;
    std::uint32_t slench_death_timer = 0;
    std::uint32_t slench_roll_timer = 0;
    bool slench_firing_tear = false;
    net::Vec3 slench_target_horizontal{0.0F, 0.0F, 1.0F};
    net::Vec3 slench_start_position;
    net::Vec3 slench_detached_position;
    net::Vec3 slench_detached_facing;
    net::Vec3 slench_destination1;
    net::Vec3 slench_destination2;
    bool slench_hit_floor = false;
    net::Vec3 slench_home_position;
    bool slench_shielded = true;
    bool invulnerable = false;
    // Enemy12Entity asks its animation rather than a flag which of the
    // four states it is in: folded, unfolding, unfolded, folding.  The
    // flag follows the animation rather than the other way round.
    std::uint8_t geemer_animation = 2;
    bool geemer_extended = false;
    std::uint32_t geemer_transition_timer = 0;
    // War Wasp/Barbed War Wasp keep the managed state-machine counters
    // separate from the generic authored-motion cursor.  The source classes
    // share movement helpers but have different subroutine tables and attack
    // timing, so collapsing them into EnemyInstance loses observable state.
    std::uint8_t wasp_pattern = 0;
    std::uint8_t wasp_next_pattern = 0;
    std::uint8_t wasp_final_move_index = 0;
    std::uint8_t wasp_move_index = 0;
    std::uint8_t wasp_max_move_index = 0;
    std::uint32_t wasp_step_count = 0;
    std::uint32_t wasp_windup_timer = 0;
    std::uint32_t wasp_attack_delay = 0;
    std::uint16_t wasp_shot_count = 0;
    std::uint32_t wasp_shot_timer = 0;
    float wasp_circle_angle = 0.0F;
    float wasp_step_distance = 0.2F;
    net::Vec3 wasp_attack_target;
    net::Vec3 wasp_move_target;
    net::Vec3 wasp_initial_position;
    net::Vec3 wasp_aim_vector{0.0F, 0.0F, 1.0F};
    std::array<net::Vec3, 16> wasp_move_positions{};
    // Gorea's managed classes are linked entities, not ordinary enemies.
    // Keep the counters and bitfields that are observable across the parent
    // and child state machines here instead of allowing a generic enemy
    // update to consume their 0xffff health sentinel.
    std::uint32_t gorea_flags = 0;
    std::uint32_t gorea_damage = 0;
    std::uint32_t gorea_state_timer = 0;
    std::uint32_t gorea_aux_timer = 0;
    std::uint32_t gorea_attack_timer = 0;
    std::uint32_t gorea_damage_timer = 0;
    std::uint8_t gorea_index = 0;
    std::uint8_t gorea_phase = 0;
    std::uint8_t gorea_phases_left = 3;
    std::uint8_t gorea_state = 0;
    std::uint8_t gorea_part_state = 0;
    std::uint8_t gorea_weapon_index = 5;
    std::uint16_t gorea_ammo = 65535;
    std::uint16_t gorea_cooldown = 0;
    // The managed Gorea classes use an animation index/frame as part of the
    // state transition contract (slam/swipe/shot frames are gameplay
    // events, not merely renderer state).  Keep that small playback cursor
    // in the simulation so a native controller can consume the same timing
    // without reaching into the renderer.
    std::uint8_t gorea_previous_state = 0xff;
    std::uint8_t gorea_next_state = 0;
    std::uint8_t gorea_model_animation = 0;
    std::uint8_t gorea_arm_bits = 0;
    // Enemy31Entity has a second set of state-machine counters in addition
    // to the generic enemy timers.  Keep the cartridge field names here so
    // teleport fades, laser proximity checks, and meteor cooldowns do not
    // accidentally consume one another's state.
    std::uint32_t gorea_field22c = 0;
    std::uint32_t gorea_field22e = 0;
    std::uint32_t gorea_field230 = 0;
    std::uint32_t gorea_field232 = 0;
    std::uint32_t gorea_field234 = 0;
    std::uint32_t gorea_field236 = 0;
    // Enemy24's _field23C can intentionally cross below zero for Behavior19;
    // Enemy31 only uses the same storage as a non-negative timer.
    std::int32_t gorea_field23c = 0;
    bool gorea_state_initialized = false;
    std::uint16_t gorea_animation_frame = 0;
    std::uint16_t gorea_animation_length = 1;
    bool gorea_animation_loop = false;
    // ModelInstance marks a no-loop animation ended while keeping its cursor
    // on the final authored frame. The native controller needs that bit
    // separately because gameplay callbacks observe the cursor before the
    // end-of-frame animation update.
    bool gorea_animation_ended = false;
    // Enemy32 keeps a private LOS result separate from its engine Visible
    // flag.  Enemy31's Behavior08/11 consume this value when selecting the
    // next teleport or attack.
    bool gorea_visibility = false;
    std::uint8_t gorea_return_state = 0;
    std::int32_t gorea_field23e = 0;
    std::int32_t gorea_field240 = 0;
    std::int32_t gorea_field244 = 0;
    std::int32_t gorea_field242 = 0;
    std::uint8_t gorea_light_mask = 0x7f;
    std::uint8_t gorea_meteor_count = 0;
    // Enemy26 owns one attached EffectEntry while a shoulder is charging.
    // Keep its handle in the gameplay record so the parent can stop it on a
    // weapon transition or arm death and the renderer can update its node
    // transform without turning it into a frame-only billboard.
    std::uint32_t gorea_arm_effect_id = 0;
    // Enemy25 keeps the eye-flash EffectEntry attached to the animated head
    // until the cartridge effect reports IsFinished.  This is separate from
    // Enemy31's laser flash handle below because both entities can coexist in
    // the same story session.
    std::uint32_t gorea_head_flash_effect_id = 0;
    std::uint32_t gorea_charge_effect_id = 0;
    std::uint32_t gorea_flash_effect_id = 0;
    std::uint32_t gorea_collision_effect_id = 0;
    float gorea_speed_factor = 0.0F;
    // Gorea1B keeps the managed 24-point grapple rope in the simulation.  It
    // is not renderer-only state: the segment chain determines the player's
    // pulled/held/slammed position and its accumulated length is used by the
    // original spring calculation.
    std::uint32_t gorea_grapple_timer = 0;
    std::uint32_t gorea_hold_timer = 0;
    std::uint32_t gorea_swing_timer = 0;
    std::uint32_t gorea_grapple_damage_timer = 0;
    std::uint8_t gorea_grapple_phase = 0;
    bool gorea_grappling = false;
    bool gorea_swing_direction = false;
    std::array<net::Vec3, 24> gorea_grapple_points{};
    net::Vec3 gorea_grapple_field10;
    float gorea_grapple_field24 = 0.0F;
    float gorea_grapple_field28 = 0.0F;
    float gorea_grapple_field30 = 0.0F;
    float gorea_grapple_field34 = 0.0F;
    float gorea_grapple_field38 = 0.0F;
    float gorea_grapple_field21c = 0.0F;
    float gorea_grapple_field224 = 0.0F;
    float gorea_grapple_int = 0.0F;
    std::int32_t gorea_grapple_field234 = 0;
    float gorea_volume_radius = 0.0F;
    float gorea_volume_height = 0.0F;
    std::int16_t gorea_current_trigger_id = -1;
    std::uint32_t gorea_laser_timer = 0;
    std::uint32_t gorea_laser_damage_timer = 0;
    std::uint32_t gorea_teleport_timer = 0;
    std::uint32_t gorea_meteor_timer = 0;
    std::uint32_t gorea_phase_timer = 0;
    std::uint8_t gorea_pending_state = 0;
    bool gorea_state_pending = false;
    bool gorea_hover_direction = false;
    net::Vec3 gorea_anchor;
    net::Vec3 gorea_target_position;
    net::Vec3 gorea_teleport_destination;
    net::Vec3 gorea_laser_target;
    net::Vec3 gorea_laser_normal{0.0F, 1.0F, 0.0F};
    net::Vec3 gorea_target_facing{0.0F, 0.0F, 1.0F};
    bool gorea_activated = false;
    bool gorea_targetable = false;
    // EnemyInstanceEntity keeps CollideBeam independent from Visible and
    // Invincible. Gorea's attached head, arms, and chest sphere rely on that
    // distinction to consume a beam and run EnemyTakeDamage while a phase is
    // hidden or temporarily immune.
    bool gorea_beam_collidable = false;
    // Enemy30Entity is a linked Gorea crystal.  State 0 is an available
    // crystal, states 1/2 are the Gorea1B launch choreography, and state 3
    // is the detached projectile.  The managed class also retains its
    // previous position for the beam/travel collision check.
    std::uint8_t trocra_state = 0;
    std::uint8_t trocra_slot = 0xff;
    std::uint32_t trocra_state_timer = 0;
    bool trocra_destroy_processed = false;
    net::Vec3 trocra_field174;
    net::Vec3 trocra_previous_position;
    net::Vec3 gorea_meteor_base_position;
    net::Vec3 gorea_meteor_previous_position;
    net::Vec3 gorea_meteor_effect_up{0.0F, 1.0F, 0.0F};
    net::Vec3 gorea_meteor_effect_facing{0.0F, 0.0F, 1.0F};
    float gorea_meteor_rotation = 0.0F;
    std::uint32_t gorea_meteor_shake_timer = 0;
    // Enemy33Entity's two fuses.  They do not run together: the long one
    // ticks only while the meteor is still looking for somebody, and the
    // short one only after it has found them.  That is the mechanic --
    // being noticed halves what is left, and the flashing says so.
    std::uint32_t gorea_meteor_long_fuse = 0;
    std::uint32_t gorea_meteor_short_fuse = 0;
    // The flash: an interval, a countdown, and which half of it we are in.
    std::uint8_t gorea_meteor_flash_interval = 0;
    std::uint8_t gorea_meteor_flash_timer = 0;
    bool gorea_meteor_flashing = false;
    // EnemyInstanceEntity._timeSinceDamage, which is what actually turns
    // the model red.  A meteor drives it deliberately rather than only
    // when hurt.
    std::uint16_t gorea_meteor_time_since_damage = 510;
    // The four weights Enemy33Entity rolls its drop against.  They are
    // authored per meteor rather than shared, so they live here.
    std::uint16_t gorea_meteor_item_chance1 = 0;
    std::uint16_t gorea_meteor_item_chance2 = 0;
    std::uint16_t gorea_meteor_item_chance3 = 0;
    std::uint16_t gorea_meteor_item_chance4 = 0;
    std::uint16_t turret_burst_remaining = 0;
    std::uint32_t turret_shot_timer = 0;
    std::uint32_t turret_delay_timer = 0;
    // Enemy45Entity has a separate salvo timer.  It is not the same as the
    // Alimbic turret's initial/idle delay: activating a Slench turret lets
    // its first shot use the current burst immediately, then gates the next
    // burst with SalvoCooldown.
    std::uint32_t turret_salvo_cooldown = 0;
    // S10.Index is the synapse slot that owns this Slench turret.
    std::int32_t turret_index = -1;
    // Which row of Metadata.Enemy45Values this Slench turret reads.
    std::uint8_t turret_subtype = 0;
    // Enemy45Entity's lights.  The animation frame is how many of them are
    // lit, and the synapses raise and lower the ceiling as they die and
    // come back -- so this is a number the player reads off the wall, not
    // a rendering detail.  It runs forward when the turret opens and
    // backward after every shot.
    std::int32_t turret_anim_frame = 0;
    std::int32_t turret_anim_frame_count = 0;
    // The model's own last frame, which is the most lights it can show.
    // Set by whoever owns the model; zero until then, and a zero ceiling
    // simply means the lights do not move.
    std::int32_t turret_anim_max_frame = 0;
    std::int32_t turret_anim_interval = 1;
    std::int32_t turret_anim_delay_timer = 1;
    bool turret_anim_reverse = false;
    bool turret_animating = false;
    std::uint32_t blastcap_cloud_tick = 0;
    std::uint32_t blastcap_cloud_timer = 0;
    // Enemy16Entity is a four-state subroutine machine. The cloud state is
    // entered by EnemyTakeDamage, while the live body still needs its
    // agitate/near/contact ordering and animation-ended cursor.
    std::uint32_t blastcap_agitate_timer = 0;
    std::uint8_t blastcap_next_state = 0;
    std::uint8_t blastcap_animation = 2;
    std::uint32_t blastcap_animation_timer = 0;
    bool blastcap_animation_no_loop = false;
    bool blastcap_animation_ended = false;
    bool blastcap_initial_cloud_hit = false;
    bool blastcap_state_initialized = false;
    net::Vec3 turret_initial_facing{0.0F, 0.0F, 1.0F};
    net::Vec3 turret_aim{0.0F, 0.0F, 1.0F};
    float turret_angle_x = 0.0F;
    float turret_angle_y = 0.0F;
    float turret_angle_x_sign = 1.0F;
    float turret_angle_y_sign = 1.0F;
    // Slench's S10 turret is created inactive and is enabled by the linked
    // synapse message graph. Standalone Alimbic turrets stay enabled.
    bool turret_enabled = true;
    bool active = true;
    bool visible = true;
    bool blastcap_exploded = false;
};

struct InventoryState {
    std::uint16_t health_max = 100;
    std::array<std::uint16_t, 2> ammo{};
    std::array<std::uint16_t, 2> ammo_max{599, 599};
    std::array<bool, 9> available_weapons{};
    bool infinite_ammo = false;
    std::uint32_t double_damage_ticks = 0;
    std::uint32_t cloak_ticks = 0;
    std::uint32_t deathalt_ticks = 0;
};

constexpr std::uint8_t NeutralObjectiveTeam = 0xff;

struct NodeObjectiveState {
    std::int16_t entity_id = -1;
    scene::EntityVolume volume;
    std::uint8_t current_team = NeutralObjectiveTeam;
    std::uint8_t occupying_team = NeutralObjectiveTeam;
    std::uint8_t captured_by_slot = 0xff;
    float progress = 0.0F;
    float score_timer = 0.0F;
    bool contested = false;
};

struct FlagObjectiveState {
    std::int16_t entity_id = -1;
    std::uint8_t team_id = 0;
    bool bounty = false;
    scene::VolumePoint base_position;
    scene::VolumePoint position;
    std::uint8_t carrier_slot = 0xff;
    bool at_base = true;
    float reset_timer = 0.0F;
};

struct ObjectiveState {
    // The managed GameState has eight slots for team-time bookkeeping even
    // though ordinary team matches use only teams 0 and 1. Keeping all eight
    // here also covers the free-for-all Defender mode.
    std::array<float, net::NetConfig::SlotCapacity> team_time{};
    std::array<float, net::NetConfig::SlotCapacity> player_time{};
    std::int8_t prime_hunter = -1;
    std::vector<NodeObjectiveState> nodes;
    std::vector<FlagObjectiveState> flags;
};

// The managed TeleporterEntity does not immediately move the player when its
// destination points at another room.  It records the destination and lets
// GameState/Scene perform the fade-and-load transition.  Keep that request on
// the engine-neutral gameplay boundary so the Win32 host, headless tests, and
// a future Android frontend can consume the same event.
struct RoomTransitionRequest {
    std::string room_name;
    std::uint8_t target_entity_id = 0;
    bool alt_form = false;
    std::uint32_t source_entity_id = 0;
};

// Deterministic fixed-timestep player simulation. This is deliberately
// independent of Win32/OpenGL: the window, network client, replay reader, and
// future Android frontend can all feed the same input and consume snapshots.
class Session {
public:
    using MessageSink = std::function<void(const messaging::MessageInfo&)>;

    explicit Session(const scene::Room& room, Config config = {});

    [[nodiscard]] std::size_t add_player(std::uint8_t slot,
                                         std::uint8_t hunter = 0);
    // Materialize one of SceneSetup's story hunter selections. The weapon
    // argument remains a persisted cartridge BeamType ordinal; the session
    // converts it to the native weapon table and marks the hunter's ammo as
    // infinite just like PlayerEntity.InitEnemyHunter.
    void configure_story_hunter(std::uint8_t slot, net::Vec3 position,
                                net::Vec3 facing, std::uint16_t health,
                                std::uint16_t health_max,
                                std::uint16_t health_threshold,
                                std::uint32_t cartridge_weapon);
    void remove_player(std::uint8_t slot);
    void set_player_hunter(std::uint8_t slot, std::uint8_t hunter) noexcept;
    [[nodiscard]] std::uint8_t player_hunter(std::uint8_t slot) const noexcept;
    void set_input(std::uint8_t slot, Input input);
    // Network aim is written after the action buttons have been assembled.
    // Keep it separate so a remote aim correction cannot clear the same
    // frame's movement, fire, or form state.
    void set_network_aim(std::uint8_t slot, net::Vec3 aim) noexcept;
    // The managed PlayerEntity uses this to keep controller/scripted/network
    // driven hunters out of the idle-gun path.  It intentionally does not
    // invent a new input packet; it only marks the existing runtime input as
    // having been touched this frame.
    void note_network_input(std::uint8_t slot) noexcept;
    // Reposition a carried player at a room-load destination. This is the
    // native equivalent of PlayerEntity.Reposition after a cross-room
    // teleporter has selected the destination entity.
    void place_player(std::uint8_t slot, net::Vec3 position,
                      net::Vec3 facing, bool alt_form);
    void rejoin_player(std::uint8_t slot);
    void set_match_mode(std::uint8_t mode,
                        std::uint16_t point_goal) noexcept;
    void set_objective_authority(bool enabled) noexcept {
        config_.objective_authority = enabled;
    }
    void set_friendly_fire(bool enabled) noexcept {
        config_.friendly_fire = enabled;
    }
    void set_team_mode(bool enabled) noexcept;
    void tick();

    [[nodiscard]] std::uint64_t tick_count() const noexcept {
        return tick_count_;
    }
    [[nodiscard]] const std::vector<net::PlayerState>& players() const noexcept {
        return players_;
    }
    [[nodiscard]] bool team_mode() const noexcept { return config_.team_mode; }
    // Formats.GameMode, as GameState.Mode reports it.
    [[nodiscard]] std::uint8_t match_mode() const noexcept {
        return config_.mode;
    }
    [[nodiscard]] bool has_player(std::uint8_t slot) const noexcept;
    [[nodiscard]] std::uint32_t respawn_ticks(std::uint8_t slot) const noexcept;
    void set_respawn_ticks(std::uint8_t slot, std::uint32_t value) noexcept;
    [[nodiscard]] std::uint32_t respawn_duration_ticks() const noexcept {
        return config_.respawn_ticks;
    }
    [[nodiscard]] const net::PlayerState& player(std::uint8_t slot) const;
    [[nodiscard]] net::PlayerState& mutable_player(std::uint8_t slot);
    [[nodiscard]] const players::Profile& player_profile(
        std::uint8_t slot) const;
    // MorphCameraEntity.Process: only an alt-form player's collision sphere
    // may select the authored external camera. Empty means the player is not
    // currently inside any morph-camera volume.
    [[nodiscard]] std::optional<net::Vec3> morph_camera_position(
        std::uint8_t slot) const noexcept;
    // Apply the persistent story inventory to an already-created local
    // player. Network matches deliberately keep using their clean spawn
    // inventory; only the Adventure launch boundary calls this.
    void apply_story_save(std::uint8_t slot, const game::StorySave& save);
    // Apply the room-scoped StorySave state to gameplay environment records
    // after the room has been constructed.  This is separate from inventory
    // restoration because spawners and volumes are initialized from authored
    // data during Session construction.
    void apply_story_room_state(std::int32_t room_id,
                                game::StorySave& save);
    // Forward environment messages to the scene queue so static and dynamic
    // entities receive the same delayed cartridge message graph.
    void set_message_sink(MessageSink sink) { message_sink_ = std::move(sink); }
    // The renderer owns the catalog Files.  These pointers are deliberately
    // non-owning: they bind gameplay attachment samplers to the same decoded
    // Gorea models that the frontend will draw, and are cleared with the
    // Session before a room's render resources are destroyed.
    void set_gorea1a_model(const model::File* model);
    void set_gorea1b_model(const model::File* model);
    void set_gorea2_model(const model::File* model);
    // Scene dispatches queued cartridge messages back into the gameplay-owned
    // dynamic records after static entities have seen the same message.
    void dispatch_message(const messaging::MessageInfo& info);
    void apply_snapshot(const net::SnapshotPacket& snapshot);
    [[nodiscard]] const std::vector<Projectile>& projectiles() const noexcept {
        return projectiles_;
    }
    [[nodiscard]] const std::vector<ItemState>& items() const noexcept {
        return items_;
    }
    [[nodiscard]] const std::vector<EffectState>& effects() const noexcept {
        return effects_;
    }
    // The frontend reports completion of a persistent EffectEntry after the
    // parsed element graph drains.  Keep the mutation at the Session boundary
    // so linked controllers can observe the detach on their next fixed tick.
    void detach_effect_from_runtime(std::uint32_t effect_id,
                                    bool set_expired = false) noexcept;
    [[nodiscard]] const std::vector<SingleParticleState>&
    single_particles() const noexcept {
        return single_particles_;
    }
    void add_single_particle(SingleParticleType type, net::Vec3 position,
                             net::Vec3 color, float alpha,
                             float scale) noexcept;
    [[nodiscard]] const std::vector<SoundEvent>& sound_events() const noexcept {
        return sound_events_;
    }
    [[nodiscard]] const std::vector<EnemyState>& enemies() const noexcept {
        return enemies_;
    }
    [[nodiscard]] bool damage_enemy(std::uint32_t id,
                                    std::uint32_t damage,
                                    std::uint8_t source_weapon = 0xff) noexcept;
    [[nodiscard]] bool destroy_enemy(std::uint32_t id,
                                     bool out_of_range = false) noexcept;
    [[nodiscard]] const InventoryState& inventory(std::uint8_t slot) const;
    [[nodiscard]] InventoryState& inventory(std::uint8_t slot);
    void set_network_ammo(std::uint8_t slot, std::uint16_t ua,
                          std::uint16_t missiles) noexcept;
    void set_network_damage_authority(bool enabled) noexcept;
    void replay_network_damage(std::uint8_t victim_slot,
                               std::uint16_t amount,
                               std::uint8_t attacker_slot,
                               std::uint8_t beam,
                               net::Vec3 impulse,
                               bool lethal) noexcept;
    [[nodiscard]] const ObjectiveState& objective_state() const noexcept {
        return objectives_;
    }
    [[nodiscard]] const std::optional<RoomTransitionRequest>&
    room_transition() const noexcept {
        return room_transition_;
    }
    void clear_room_transition() noexcept { room_transition_.reset(); }
    [[nodiscard]] net::SnapshotPacket snapshot(std::uint32_t frame,
                                               std::uint32_t rng1 = 0,
                                               std::uint32_t rng2 = 0) const;

private:
    struct RuntimeInput {
        std::uint8_t slot = 0;
        std::uint8_t hunter = 0;
        Input input;
        InventoryState inventory;
        std::uint16_t hunter_health_threshold = 0;
        net::IntentButtons previous_buttons = net::IntentButtons::None;
        std::size_t spawn_ordinal = 0;
        std::uint32_t respawn_ticks = 0;
        std::uint32_t fire_cooldown = 0;
        float gravity_override = 0.0F;
        std::uint32_t gravity_priority = 0;
        bool gravity_override_active = false;
        bool prevent_form_switch = false;
        // PlayerFlags2.Halfturret: Weavel has left half of himself
        // behind as a turret.  It is runtime-only -- the wire
        // PlayerState has no room for it -- so it lives here.
        bool halfturret = false;
        bool biped_lock = false;
        std::uint32_t control_lock_ticks = 0;
        bool eliminated = false;
        bool input_received = false;
    };

    [[nodiscard]] std::size_t player_index(std::uint8_t slot) const;
    [[nodiscard]] net::Vec3 spawn_position(std::size_t ordinal,
                                           std::uint8_t team = 0) const;
    struct PlayerSpawnRuntime {
        std::int16_t entity_id = -1;
        net::Vec3 position;
        net::Vec3 facing{0.0F, 0.0F, 1.0F};
        net::Vec3 up{0.0F, 1.0F, 0.0F};
        bool default_active = false;
        bool availability = false;
        bool active = false;
        std::uint16_t cooldown = 0;
    };
    [[nodiscard]] PlayerSpawnRuntime* select_respawn_spawn(
        const net::PlayerState& player);
    void apply_input(net::PlayerState& player, const Input& input);
    void spawn_projectile(const net::PlayerState& player, const Input& input);
    // The EnemyScene every ported enemy is handed: the outward calls
    // EnemyInstanceEntity makes, filled in from this session.  Built once
    // per enemy per frame rather than kept, because a seam that outlives
    // the call is a seam something can hold on to.
    [[nodiscard]] EnemyScene build_enemy_scene(net::Vec3 prev_position);
    // The meteor adds two calls of its own to that: the effects it leaves
    // behind, and the item it drops when it is shot down.
    [[nodiscard]] EnemyScene build_gorea_meteor_scene(
        const EnemyState& agent);

    // The Ithraks add two: the hit zone that is what a shot actually
    // lands on, and the ray that finds the floor under a drop.
    [[nodiscard]] EnemyScene build_ithrak_scene(net::Vec3 prev_position);

    // CollisionDetection's queries take the room's collision as a list of
    // parts, which is what a room is made of -- but scene::Room keeps the
    // decoded file rather than the float view they walk.  Built once per
    // room and kept, because building it per query would decode the whole
    // mesh every frame for every enemy.
    [[nodiscard]] std::span<const collision::Instance* const>
    room_collision_parts();

    void spawn_enemy_projectile(const EnemyState& enemy,
                                net::Vec3 position,
                                net::Vec3 direction,
                                SoundCue sound_cue = SoundCue::BeamShot);
    [[nodiscard]] bool consume_weapon_ammo(RuntimeInput& runtime,
                                            std::uint8_t weapon) noexcept;
    void update_projectiles();
    void update_effects();
    std::uint32_t spawn_effect(
        std::uint16_t effect_id, net::Vec3 position, net::Vec3 direction,
        std::uint32_t source_entity_id, float lifespan_seconds = 0.25F,
        bool no_splat = false, bool persistent = false,
        bool element_extension = false,
        net::Vec3 up = {0.0F, 1.0F, 0.0F});
    void update_effect_transform(std::uint32_t effect_id, net::Vec3 position,
                                 net::Vec3 direction,
                                 net::Vec3 up = {0.0F, 1.0F, 0.0F}) noexcept;
    void detach_effect(std::uint32_t effect_id,
                       bool set_expired = true) noexcept;
    void update_enemies();
    struct ForceFieldRuntime;
    // The dispatch names mirror the managed Entities/Enemies source files.
    // A few methods intentionally forward to a shared state machine while
    // that controller is being split further; the one-to-one module contract
    // makes those cases explicit instead of hiding them in gameplay.cpp.
    void update_warwasp(EnemyState& agent);
    void update_zoomer(EnemyState& agent);
    void update_surface_enemy(EnemyState& agent);
    void update_psychobit(EnemyState& agent);
    void update_firespawn(EnemyState& agent);
    void update_voldrum(EnemyState& agent);
    void update_voldrum1(EnemyState& agent);
    void update_voldrum2(EnemyState& agent);
    void update_temroid(EnemyState& agent);
    void update_petrasyl(EnemyState& agent);
    void update_petrasyl1(EnemyState& agent);
    void update_petrasyl2(EnemyState& agent);
    void update_petrasyl3(EnemyState& agent);
    void update_petrasyl4(EnemyState& agent);
    void update_barbed_warwasp(EnemyState& agent);
    void update_geemer(EnemyState& agent);
    void update_ithrak(EnemyState& agent);
    void update_lesser_ithrak(EnemyState& agent);
    void update_greater_ithrak(EnemyState& agent);
    void update_quadtroid(EnemyState& agent);
    void update_crash_pillar(EnemyState& agent);
    void update_force_field_lock(EnemyState& agent);
    void update_hit_zone(EnemyState& agent);
    void update_cretaphid(EnemyState& agent);
    void update_cretaphid_part(EnemyState& agent);
    void update_cretaphid_eye(EnemyState& agent);
    void update_cretaphid_crystal(EnemyState& agent);
    void update_slench(EnemyState& agent);
    void update_slench_part(EnemyState& agent);
    void update_slench_shield(EnemyState& agent);
    void update_slench_nest(EnemyState& agent);
    void update_slench_synapse(EnemyState& agent);
    void update_slench_turret(EnemyState& agent);
    void update_shriekbat(EnemyState& agent);
    void update_turret(EnemyState& agent);
    void update_alimbic_turret(EnemyState& agent);
    void update_blastcap(EnemyState& agent);
    void update_gorea_1a(EnemyState& agent);
    void update_gorea_head(EnemyState& agent);
    void update_gorea_arm(EnemyState& agent);
    void update_gorea_leg(EnemyState& agent);
    void update_gorea_1b(EnemyState& agent);
    void update_gorea_seal_sphere_1(EnemyState& agent);
    void update_trocra(EnemyState& agent);
    void update_gorea_2(EnemyState& agent);
    void update_gorea_seal_sphere_2(EnemyState& agent);
    void update_gorea_meteor(EnemyState& agent);
    [[nodiscard]] bool sample_gorea_model_node(
        const EnemyState& parent, std::uint8_t expected_type,
        const model::File* model,
        std::unique_ptr<model::ModelInstance>& instance,
        std::string_view node_name, formats::Matrix4& transform);
    [[nodiscard]] bool sample_gorea_1a_node(
        const EnemyState& parent, std::string_view node_name,
        formats::Matrix4& transform);
    [[nodiscard]] bool sample_gorea_1b_node(
        const EnemyState& parent, std::string_view node_name,
        formats::Matrix4& transform);
    [[nodiscard]] bool sample_gorea_2_node(
        const EnemyState& parent, std::string_view node_name,
        formats::Matrix4& transform);
    // Enemy33Entity.EnemyTakeDamage, on the meteor named.  Returns
    // whether the hit is to be ignored, which for a meteor is always: it
    // decides for itself whether it flashes or dies.
    [[nodiscard]] bool gorea_meteor_take_damage(EnemyState& agent);
    void detonate_gorea_meteor(EnemyState& agent,
                               std::uint16_t effect_id);
    void update_carnivorous_plant(EnemyState& agent);
    [[nodiscard]] bool update_generic_enemy(EnemyState& agent);
    void apply_enemy_contact_damage(EnemyState& agent,
                                    net::PlayerState& target,
                                    std::uint16_t damage);
    void initialize_objectives();
    void initialize_environment();
    void update_items();
    void update_enemy_spawns();
    [[nodiscard]] bool apply_item_pickup(ItemState& item,
                                         net::PlayerState& player,
                                         RuntimeInput& runtime);
    void spawn_item_drop(ItemType type, net::Vec3 position);
    void update_objectives();
    void update_flags();
    void update_environment();
    void spawn_force_field_lock(ForceFieldRuntime& field);
    void remove_force_field_lock(ForceFieldRuntime& field) noexcept;
    void apply_area_effect(const scene::AreaVolumeData& area,
                           net::PlayerState& player,
                           RuntimeInput& runtime);
    void dispatch_environment_message(std::int32_t target_id,
                                       std::uint32_t message,
                                       std::int32_t parameter1,
                                       std::int32_t parameter2,
                                       bool forward_to_scene = true,
                                       std::int32_t sender_id = -1);
    void complete_enemy_spawner(std::int16_t entity_id,
                                std::uint8_t enemy_type,
                                const scene::EnemySpawnData& data);
    void update_story_room_state(std::int32_t target_id,
                                 std::uint32_t message,
                                 std::int32_t parameter1) noexcept;
    void respawn_player(net::PlayerState& player, RuntimeInput& runtime);

public:
    // PlayerEntity.BlockFormSwitch: hold the player in whichever form they are
    // in.  It is set, never cleared here -- whatever imposed the lock clears
    // it when it ends, so two overlapping locks cannot release each other.
    void block_form_switch(std::uint8_t slot) noexcept;
    void allow_form_switch(std::uint8_t slot) noexcept;
    [[nodiscard]] bool form_switch_blocked(std::uint8_t slot) const noexcept;

    // PlayerEntity.OnHalfturretDied: the turret is gone, so the player is
    // whole again.  Called by the turret itself, which is why it does not also
    // destroy it.
    void on_halfturret_died(std::uint8_t slot) noexcept;
    void set_halfturret(std::uint8_t slot, bool active) noexcept;
    [[nodiscard]] bool has_halfturret(std::uint8_t slot) const noexcept;

    // PlayerEntity.SaveStatus: write what carries between story rooms into the
    // save.  A dead player writes nothing, so dying does not overwrite the
    // inventory they had when they were alive.  `fade_active` is false when no
    // screen fade is running, which is the case where the checkpoint is
    // cleared: a fade means a scripted transition that owns its own
    // checkpoint.
    void save_status(std::uint8_t slot, game::StorySave& save,
                     bool fade_active) const;

private:
    void constrain_to_world(net::PlayerState& player);
    void emit_sound(SoundEvent event) noexcept;

    struct JumpPadRuntime {
        std::int16_t entity_id = -1;
        scene::EntityVolume volume;
        net::Vec3 impulse;
        net::Vec3 facing{0.0F, 0.0F, 1.0F};
        std::uint32_t control_lock_ticks = 0;
        std::uint32_t cooldown_ticks = 0;
        std::uint32_t remaining_ticks = 0;
        std::uint32_t trigger_flags = 0;
        bool active = false;
    };

    struct TeleporterRuntime {
        std::int16_t entity_id = -1;
        net::Vec3 position;
        net::Vec3 target_position;
        net::Vec3 facing{0.0F, 0.0F, 1.0F};
        std::string target_room_name;
        std::uint8_t target_entity_id = 0;
        std::array<bool, net::NetConfig::SlotCapacity> triggered{};
        float activation_radius = 7.0F;
        float teleport_radius = 1.0F;
        bool active = false;
    };

    struct AreaRuntime {
        std::int16_t entity_id = -1;
        scene::AreaVolumeData data;
        std::array<bool, net::NetConfig::SlotCapacity> triggered{};
        std::array<std::uint32_t, net::NetConfig::SlotCapacity> cooldown{};
        std::uint32_t cooldown_ticks = 0;
        bool active = false;
    };

    struct TriggerRuntime {
        std::int16_t entity_id = -1;
        scene::TriggerVolumeData data;
        std::array<bool, net::NetConfig::SlotCapacity> inside{};
        std::uint32_t threshold_count = 0;
        std::uint32_t repeat_ticks = 0;
        bool pending = false;
        bool active = false;
    };

    struct ItemSpawnRuntime {
        std::int16_t entity_id = -1;
        scene::ItemSpawnData data;
        net::Vec3 position;
        std::uint32_t spawn_count = 0;
        std::uint32_t spawn_cooldown = 0;
        bool active = false;
    };

    struct EnemySpawnRuntime {
        std::int16_t entity_id = -1;
        scene::EnemySpawnData data;
        net::Vec3 facing{0.0F, 0.0F, 1.0F};
        net::Vec3 up{0.0F, 1.0F, 0.0F};
        std::unique_ptr<runtime::EnemySpawnerEntity> spawner;
    };

    struct ForceFieldRuntime {
        std::int16_t entity_id = -1;
        std::uint32_t type = 9;
        net::Vec3 position;
        net::Vec3 up{0.0F, 1.0F, 0.0F};
        net::Vec3 facing{0.0F, 0.0F, 1.0F};
        net::Vec3 right{1.0F, 0.0F, 0.0F};
        float width = 0.0F;
        float height = 0.0F;
        bool default_active = false;
        bool active = false;
        std::uint32_t lock_id = 0;
    };

    struct MorphCameraRuntime {
        std::int16_t entity_id = -1;
        scene::EntityVolume volume;
        net::Vec3 position;
    };

    const scene::Room& room_;
    Config config_;
    net::Vec3 world_min_;
    net::Vec3 world_max_;
    std::vector<net::PlayerState> players_;
    std::vector<RuntimeInput> inputs_;
    std::vector<Projectile> projectiles_;
    std::vector<ItemState> items_;
    std::vector<EffectState> effects_;
    std::vector<SingleParticleState> single_particles_;
    std::vector<SoundEvent> sound_events_;
    bool network_damage_authority_ = true;
    std::optional<RoomTransitionRequest> room_transition_;
    std::vector<EnemyState> enemies_;
    // Controllers may create linked entities during update_enemies(). Keep
    // those creations out of the live vector until the pass is complete so
    // references to the current EnemyState remain valid.
    std::vector<EnemyState> pending_enemy_spawns_;
    std::vector<ItemSpawnRuntime> item_spawns_;
    std::vector<EnemySpawnRuntime> enemy_spawns_;
    std::vector<ForceFieldRuntime> force_fields_;
    std::vector<MorphCameraRuntime> morph_cameras_;
    std::vector<PlayerSpawnRuntime> player_spawns_;
    std::vector<JumpPadRuntime> jump_pads_;
    std::vector<TeleporterRuntime> teleporters_;
    std::vector<AreaRuntime> area_volumes_;
    std::vector<TriggerRuntime> trigger_volumes_;
    ObjectiveState objectives_;
    MessageSink message_sink_;
    game::StorySave* story_save_ = nullptr;
    std::int32_t story_room_id_ = -1;
    std::int32_t story_area_id_ = -1;
    std::uint32_t next_effect_id_ = 0x40000000u;
    std::uint32_t next_enemy_id_ = 0x80000000u;
    utility::Rng rng_;
    const model::File* gorea_1a_model_ = nullptr;
    std::unique_ptr<model::ModelInstance> gorea_1a_model_instance_;
    const model::File* gorea_1b_model_ = nullptr;
    std::unique_ptr<model::ModelInstance> gorea_1b_model_instance_;
    const model::File* gorea_2_model_ = nullptr;
    std::unique_ptr<model::ModelInstance> gorea_2_model_instance_;
    std::unique_ptr<collision::Info> room_collision_info_;
    collision::Instance room_collision_instance_;
    std::array<const collision::Instance*, 1> room_collision_list_{};
    const collision::File* room_collision_source_ = nullptr;
    std::uint64_t tick_count_ = 0;
};

} // namespace fruityprime::gameplay
