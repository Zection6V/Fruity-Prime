#pragma once

#include "Metadata/metadata.hpp"
#include "Mods/Network/net_protocol.hpp"
#include "Entities/scene.hpp"

#include <array>
#include <cstdint>
#include <span>
#include <string_view>

// Native counterpart of Entities/Enemies.  The managed project has one class
// per cartridge enemy ID; the native migration keeps those IDs in one table
// first so room spawning, damage, HUD metadata, and later individual AI
// controllers all use the same source-of-truth boundary.
namespace fruityprime::enemy {

using Info = ::fruityprime::metadata::EnemyInfo;
using Effectiveness = ::fruityprime::metadata::Effectiveness;

enum class BehaviorFamily : std::uint8_t {
    Unknown,
    Flying,
    Ground,
    Stationary,
    Boss,
    BossPart,
    Plant,
    Spawner,
    Hunter
};

enum class ProfileFlags : std::uint32_t {
    None = 0,
    UsesTurretMessages = 1u << 0,
    HasLinkedParts = 1u << 1,
    UsesHitZone = 1u << 2,
    IgnoresPlayerRange = 1u << 3,
    StaticCollision = 1u << 4
};

[[nodiscard]] constexpr ProfileFlags operator|(
    ProfileFlags left, ProfileFlags right) noexcept {
    return static_cast<ProfileFlags>(
        static_cast<std::uint32_t>(left) | static_cast<std::uint32_t>(right));
}

[[nodiscard]] constexpr bool has_flag(
    ProfileFlags value, ProfileFlags flag) noexcept {
    return (static_cast<std::uint32_t>(value)
            & static_cast<std::uint32_t>(flag)) != 0;
}

struct Profile {
    std::uint8_t id = 0;
    std::string_view source_file;
    std::string_view managed_class;
    BehaviorFamily family = BehaviorFamily::Unknown;
    ProfileFlags flags = ProfileFlags::None;
};

// The managed enemy classes all expose the same runtime shape to Scene:
// acquire a player target, move or aim according to their family, make a
// contact/ranged attack, and eventually report Destroyed to their spawner.
// Keep the family defaults in one native table so the generic first pass can
// execute real story encounters while the individual state-machine ports are
// still being added.
struct CombatTuning {
    float move_speed = 0.0F;
    float detection_radius = 35.0F;
    float attack_radius = 1.25F;
    float attack_cooldown = 1.0F;
    std::uint16_t contact_damage = 8;
    std::uint16_t health = 20;
    float body_radius = 0.65F;
    bool airborne = false;
    bool ranged = false;
};

// Movement data shared by the managed WarWasp and BarbedWarWasp classes.
// Their S01/S08 payloads contain either a cylinder used for circular motion,
// or a list of fixed-point path positions. Decode that data once at spawn so
// the gameplay session can run the authored motion instead of reducing both
// classes to the generic family controller.
struct AuthoredMotion {
    bool supported = false;
    std::uint32_t movement_type = 0;
    std::uint8_t path_count = 0;
    std::array<net::Vec3, 16> path_positions{};
    net::Vec3 initial_position{};
    scene::EntityVolume movement_volume;
    scene::EntityVolume home_volume;
    float circle_radius = 0.0F;
    float step_distance = 0.2F;
};

// Enemy10 keeps the War Wasp movement payload but adds the S08 subtype table
// used by its ranged attack.  Keep this data separate from AuthoredMotion so
// the two managed classes can share movement without sharing combat values.
struct BarbedWarWaspProfile {
    bool supported = false;
    std::uint8_t variant = 0xff;
    std::uint32_t version = 0;
    std::uint16_t health = 0;
    std::uint16_t beam_damage = 0;
    std::uint16_t splash_damage = 0;
    std::uint16_t contact_damage = 0;
    std::int32_t step_distance1 = 0;
    std::int32_t step_distance2 = 0;
    std::int32_t step_distance3 = 0;
    std::int32_t circle_increment = 0;
    std::uint16_t min_shots = 0;
    std::uint16_t max_shots = 0;
    std::uint16_t scan_id = 0;
    std::uint32_t effectiveness = 0;
};

[[nodiscard]] BarbedWarWaspProfile decode_barbed_warwasp_profile(
    std::uint8_t id, std::span<const std::uint8_t> fields) noexcept;

// S00 is the Zoomer-specific common spawn layout.  The managed constructor
// keeps Volume1 as a home volume while the live enemy moves over room
// collision; retaining both decoded volumes lets the native controller make
// the same in/out-of-home decision without depending on the raw union.
struct ZoomerProfile {
    bool supported = false;
    scene::EntityVolume hurt_volume;
    scene::EntityVolume home_volume;
};

[[nodiscard]] ZoomerProfile decode_zoomer_profile(
    std::uint8_t id, std::span<const std::uint8_t> fields,
    net::Vec3 origin) noexcept;

// S06 is the PsychoBit spawn layout. The first two volumes are the hurt and
// roaming volumes, while Volume3 gates target acquisition. Volume2 is kept in
// the raw spawner data for compatibility, but the managed Enemy23Entity uses a
// one-unit near volume that follows the enemy at runtime. Store the complete
// combat table here so the native state machine does not fall back to the
// generic flying-enemy controller.
struct PsychoBitProfile {
    bool supported = false;
    std::uint8_t variant = 0xff;
    std::uint32_t version = 0;
    scene::EntityVolume hurt_volume;
    scene::EntityVolume home_volume;
    scene::EntityVolume range_volume;
    std::uint16_t health = 0;
    std::uint16_t beam_damage = 0;
    std::uint16_t splash_damage = 0;
    std::uint16_t contact_damage = 0;
    float min_speed_factor1 = 0.0F;
    float max_speed_factor1 = 0.0F;
    float min_speed_factor2 = 0.0F;
    float max_speed_factor2 = 0.0F;
    float speed_increment1 = 0.0F;
    float speed_increment2 = 0.0F;
    float range_max_cosine = -1.0F;
    std::uint16_t delay_frames = 0;
    std::uint16_t shot_frames = 0;
    std::uint16_t min_shots = 0;
    std::uint16_t max_shots = 0;
    std::uint16_t aim_steps = 0;
    std::uint16_t speed_steps = 0;
    std::uint8_t projectile_weapon = 0;
    std::uint8_t projectile_draw_func = 21;
    std::uint16_t projectile_color = 9055;
    std::uint8_t projectile_collision_effect = 242;
    std::uint8_t projectile_muzzle_effect = 65;
    float projectile_speed = 2662.0F / 4096.0F * 60.0F;
    float projectile_lifetime = 255.0F / 60.0F;
};

[[nodiscard]] PsychoBitProfile decode_psychobit_profile(
    std::uint8_t id, std::span<const std::uint8_t> fields,
    net::Vec3 origin) noexcept;

// Fire Spawn/Arctic Spawn also use S06. Volume1 is the authored underground
// location and Volume2 is the activation volume; the managed class uses a
// fixed local hurt volume and a temporary hit-zone entity, so the native
// profile keeps the two cartridge volumes plus the attack/dive table.
struct FireSpawnProfile {
    bool supported = false;
    std::uint32_t subtype = 0;
    std::uint32_t version = 0;
    scene::EntityVolume location_volume;
    scene::EntityVolume active_volume;
    std::uint16_t health = 0;
    std::uint16_t beam_damage = 0;
    std::uint16_t splash_damage = 0;
    std::uint16_t contact_damage = 0;
    std::uint32_t effectiveness = 0x8955;
    std::uint16_t attack_delay_frames = 0;
    std::uint16_t attack_count_min = 0;
    std::uint16_t attack_count_max = 0;
    std::uint16_t dive_timer_min = 0;
    std::uint16_t dive_timer_max = 0;
    std::uint8_t projectile_weapon = 0;
    std::uint8_t projectile_draw_func = 21;
    std::uint16_t projectile_color = 9055;
    std::uint8_t projectile_collision_effect = 242;
    std::uint8_t projectile_muzzle_effect = 65;
    float projectile_speed = 2662.0F / 4096.0F * 60.0F;
    float projectile_lifetime = 255.0F / 60.0F;
};

[[nodiscard]] FireSpawnProfile decode_firespawn_profile(
    std::uint8_t id, std::span<const std::uint8_t> fields,
    net::Vec3 origin) noexcept;

// S00/S06 are the two Voldrum spawn layouts. Voldrum2 (enemy 35) uses the
// common hurt/home volumes and a fixed ram profile; Voldrum1 (enemy 36) adds
// subtype/version values and fires the selected enemy weapon in paired shots.
// Keep the values beside the decoder so the gameplay controller consumes the
// same cartridge data that the managed Enemy35Entity/Enemy36Entity classes do.
struct VoldrumProfile {
    bool supported = false;
    bool ranged = false;
    std::uint8_t variant = 0xff;
    std::uint32_t version = 0;
    scene::EntityVolume hurt_volume;
    scene::EntityVolume home_volume;
    std::uint16_t health = 0;
    std::uint16_t beam_damage = 0;
    std::uint16_t splash_damage = 0;
    std::uint16_t contact_damage = 0;
    float min_speed_factor = 0.0F;
    float max_speed_factor = 0.0F;
    float speed_increment = 0.0F;
    float jump_speed = 0.0F;
    float range_max_cosine = -1.0F;
    std::uint16_t delay_frames = 0;
    std::uint16_t shot_frames = 0;
    std::uint16_t min_shots = 0;
    std::uint16_t max_shots = 0;
    std::uint16_t aim_steps = 0;
    std::uint8_t projectile_weapon = 0;
    std::uint8_t projectile_draw_func = 21;
    std::uint16_t projectile_color = 9055;
    std::uint8_t projectile_collision_effect = 242;
    std::uint8_t projectile_muzzle_effect = 65;
    float projectile_speed = 2662.0F / 4096.0F * 60.0F;
    float projectile_lifetime = 255.0F / 60.0F;
};

[[nodiscard]] VoldrumProfile decode_voldrum_profile(
    std::uint8_t id, std::span<const std::uint8_t> fields,
    net::Vec3 origin) noexcept;

// S03 is shared by Temroid and the related flying spawn layouts.  Temroid
// uses the relative position/facing and idle rectangle during its state
// machine, so expose those values at the same boundary as the other typed
// enemy payload decoders.
struct TemroidProfile {
    bool supported = false;
    scene::EntityVolume hurt_volume;
    net::Vec3 facing{0.0F, 0.0F, 1.0F};
    net::Vec3 position_offset;
    net::Vec3 idle_range;
};

[[nodiscard]] TemroidProfile decode_temroid_profile(
    std::uint8_t id, std::span<const std::uint8_t> fields,
    net::Vec3 origin) noexcept;

// S03/S04 are the four Petrasyl layouts. Petrasyl1 uses the S03 facing and
// idle rectangle; Petrasyl2-4 use the shared S04 weave/vertical fields. The
// position remains relative to the spawner so the gameplay layer can apply
// the same header-origin rule as the managed constructors.
struct PetrasylProfile {
    bool supported = false;
    std::uint8_t variant = 0xff;
    scene::EntityVolume hurt_volume;
    net::Vec3 facing{0.0F, 0.0F, 1.0F};
    net::Vec3 position_offset;
    net::Vec3 idle_range;
    float weave_offset = 0.0F;
    float vertical_range = 0.0F;
};

[[nodiscard]] PetrasylProfile decode_petrasyl_profile(
    std::uint8_t id, std::span<const std::uint8_t> fields,
    net::Vec3 origin) noexcept;

// S07 is the static CarnivorousPlant layout. The managed entity takes its
// health/damage and model object ID directly from this record and deliberately
// remains active outside the normal spawner distance gate.
struct CarnivorousPlantProfile {
    bool supported = false;
    std::uint16_t health = 0;
    std::uint16_t damage = 0;
    std::uint32_t subtype = 0;
    scene::EntityVolume hurt_volume;
    std::string_view model_name;
};

[[nodiscard]] CarnivorousPlantProfile decode_carnivorous_plant_profile(
    std::uint8_t id, std::span<const std::uint8_t> fields,
    net::Vec3 origin) noexcept;

// Lesser/Greater Ithrak share the same state machine in the managed tree.
// Their S00/S05 records differ only in the union prefix, so retain the four
// authored volumes here: home, attack range, warning range, and hurt volume.
// The runtime controller uses these volumes to keep the hanging enemy inside
// its authored room area instead of treating it as an unconstrained ground
// target.
struct IthrakProfile {
    bool supported = false;
    std::uint8_t variant = 0xff;
    std::uint16_t health = 85;
    std::uint16_t contact_damage = 15;
    scene::EntityVolume hurt_volume;
    scene::EntityVolume home_volume;
    scene::EntityVolume range_volume;
    scene::EntityVolume warn_volume;
};

[[nodiscard]] IthrakProfile decode_ithrak_profile(
    std::uint8_t id, std::span<const std::uint8_t> fields,
    net::Vec3 origin) noexcept;

// Quadtroid uses the common S00 layout. Volume0 is the local hurt volume and
// Volume1 is the authored encounter volume used both for target acquisition
// and for deciding when the crawler should return to its room area.
struct QuadtroidProfile {
    bool supported = false;
    scene::EntityVolume hurt_volume;
    scene::EntityVolume encounter_volume;
};

[[nodiscard]] QuadtroidProfile decode_quadtroid_profile(
    std::uint8_t id, std::span<const std::uint8_t> fields,
    net::Vec3 origin) noexcept;

// CrashPillar also uses S00. The managed class calls S00.Volume2 its
// activation volume and S00.Volume1 its return/leash volume; keeping that
// naming explicit avoids accidentally swapping the two runtime gates.
struct CrashPillarProfile {
    bool supported = false;
    scene::EntityVolume hurt_volume;
    scene::EntityVolume leash_volume;
    scene::EntityVolume activation_volume;
};

[[nodiscard]] CrashPillarProfile decode_crash_pillar_profile(
    std::uint8_t id, std::span<const std::uint8_t> fields,
    net::Vec3 origin) noexcept;

// Enemy19Entity is the cylinder boss. Its S05 payload supplies the body hurt
// volume; the rest of the encounter is the authored four-variant table in
// Metadata.Enemies.cs. Keep the eye state/timing arrays here so the native
// session can create the twelve linked eyes and the crystal without reducing
// the boss to a generic contact enemy.
struct CretaphidPhase {
    std::uint16_t crystal_health = 0;
    std::uint16_t crystal_shot_time = 0;
    std::uint16_t crystal_shot_delay = 0;
    std::uint16_t crystal_up_time = 0;
    std::uint16_t crystal_beam_damage = 0;
    std::uint16_t eye_beam_damage = 0;
    std::uint16_t eye_splash_damage = 0;
    std::uint16_t eye_contact_damage = 0;
    std::array<std::uint8_t, 12> eye_state{};
    std::array<std::uint8_t, 12> eye_beam_type{};
    std::array<std::uint8_t, 12> eye_beam_spawn_min{};
    std::array<std::uint8_t, 12> eye_beam_spawn_max{};
    std::array<std::uint16_t, 12> eye_beam_cooldown{};
    std::array<std::uint16_t, 12> eye_state_timer0{};
    std::array<std::uint16_t, 12> eye_state_timer1{};
    std::array<std::uint16_t, 12> eye_state_timer2{};
    std::array<std::uint16_t, 12> eye_state_timer3{};
};

struct CretaphidProfile {
    bool supported = false;
    std::uint8_t subtype = 0xff;
    std::uint16_t health = 100;
    std::uint16_t crystal_health = 0;
    std::uint16_t eye_health = 0;
    std::uint16_t phase_flash_time = 60;
    float collision_radius = 3276.0F / 4096.0F;
    scene::EntityVolume hurt_volume;
    std::array<CretaphidPhase, 3> phases{};
};

[[nodiscard]] CretaphidProfile decode_cretaphid_profile(
    std::uint8_t id, std::span<const std::uint8_t> fields,
    net::Vec3 origin) noexcept;

// Enemy41Entity is the shared Slench boss body.  The common S00 record only
// carries its hurt volume; the room name selects one of the four cartridge
// variants and the managed Enemy41Values table supplies three phase rows for
// each variant.  Keep every authored field typed so the native controller can
// follow the same shot, movement, slam, wobble, and phase cadence without
// reaching into raw bytes or silently replacing the state machine with a
// generic chase.
struct SlenchPhase {
    std::uint16_t scan_id1 = 0;
    std::uint16_t scan_id2 = 0;
    std::int32_t angle_increment1 = 0;
    std::int32_t health = 0;
    std::int32_t angle_increment2 = 0;
    std::uint16_t min_static_shot_timer = 0;
    std::uint16_t max_static_shot_timer = 0;
    std::int16_t static_shot_cooldown = 0;
    std::uint8_t static_shot_count = 0;
    std::uint8_t padding17 = 0;
    std::int32_t angle_increment3 = 0;
    std::int32_t move_increment1 = 0;
    std::int32_t move_increment2 = 0;
    std::int32_t angle_increment4 = 0;
    std::int32_t roam_time = 0;
    std::int32_t move_increment3 = 0;
    std::int32_t roll_time = 0;
    std::int32_t floating_angle_increment = 0;
    std::int32_t rolling_angle_increment = 0;
    float floating_speed = 0.0F;
    float rolling_speed = 0.0F;
    std::int32_t angle_increment5 = 0;
    float slam_range = 0.0F;
    std::int32_t move_increment4 = 0;
    std::int32_t move_increment5 = 0;
    std::uint16_t slam_delay = 0;
    std::uint8_t wobble_cycles = 0;
    std::uint8_t wobble_rotation_increment = 0;
    float max_wobble_distance = 0.0F;
    std::uint16_t magic = 0;
    std::uint16_t padding5e = 0;
};

struct SlenchProfile {
    bool supported = false;
    std::uint8_t subtype = 0;
    std::uint16_t health = 0;
    std::uint16_t contact_damage = 35;
    scene::EntityVolume hurt_volume;
    std::array<SlenchPhase, 3> phases{};
    // BossWeapons[3] is the shared Slench Tear fired during the static eye
    // phase.  The four projectile_* fields below are the subtype-specific
    // Slench Beam used while the detached body is targeting a player.
    std::uint8_t tear_weapon = 1;
    std::uint16_t tear_damage = 10;
    std::uint8_t tear_draw_func = 12;
    std::uint16_t tear_color = 32140;
    std::uint8_t tear_collision_effect = 71;
    std::uint8_t tear_muzzle_effect = 65;
    float tear_speed = 10.0F;
    float tear_lifetime = 30.0F;
    std::uint8_t projectile_weapon = 0;
    std::uint16_t projectile_damage = 0;
    std::uint8_t projectile_draw_func = 21;
    std::uint16_t projectile_color = 9055;
    std::uint8_t projectile_collision_effect = 242;
    std::uint8_t projectile_muzzle_effect = 65;
    float projectile_speed = 0.0F;
    float projectile_lifetime = 0.0F;
};

// Enemy41Entity creates the shield and three synapses as child entities at
// initialization time.  They do not have independent room-spawner payloads;
// their fixed values live in Metadata.Enemies.cs, so expose those values as
// native child profiles instead of dropping the parent/child relationship at
// the room boundary.
struct SlenchShieldProfile {
    bool supported = false;
    std::uint16_t health = 255;
    float hurt_radius = 1.0F;
    float follow_offset = 2.9F;
};

struct SlenchNestProfile {
    bool supported = false;
    std::uint16_t health = 100;
};

struct SlenchSynapsePhase {
    std::uint16_t health = 36;
    std::uint16_t heal_timer = 120;
    std::uint16_t reappear_timer = 360;
    float collision_radius = 6144.0F / 4096.0F;
};

struct SlenchSynapseProfile {
    bool supported = false;
    std::uint8_t subtype = 0;
    std::uint8_t index = 0;
    std::array<SlenchSynapsePhase, 3> phases{};
};

[[nodiscard]] SlenchShieldProfile slench_shield_profile() noexcept;
[[nodiscard]] SlenchNestProfile slench_nest_profile() noexcept;
[[nodiscard]] SlenchSynapseProfile slench_synapse_profile(
    std::uint8_t subtype, std::uint8_t index) noexcept;

[[nodiscard]] SlenchProfile decode_slench_profile(
    std::uint8_t id, std::span<const std::uint8_t> fields,
    net::Vec3 origin, std::string_view room_name) noexcept;

// S02 is the Shriekbat layout.  The first volume is the hurt volume, the
// fixed-point vector at 0x40 is the authored dive target relative to the
// spawner, and the two following volumes gate the attack sequence.  Keeping
// these three volumes in a typed profile lets the native state machine follow
// the same range/active checks as Enemy11Entity instead of treating a
// Shriekbat as an ordinary flying target.
struct ShriekbatProfile {
    bool supported = false;
    scene::EntityVolume hurt_volume;
    scene::EntityVolume range_volume;
    scene::EntityVolume active_volume;
    net::Vec3 path_vector;
};

[[nodiscard]] ShriekbatProfile decode_shriekbat_profile(
    std::uint8_t id, std::span<const std::uint8_t> fields,
    net::Vec3 origin) noexcept;

[[nodiscard]] AuthoredMotion decode_authored_motion(
    std::uint8_t id, std::span<const std::uint8_t> fields,
    net::Vec3 origin) noexcept;

// Enemy18Values and the S06 range volume used by 18_AlimbicTurret.cs.  The
// values are kept with the decoder rather than in gameplay so the cartridge
// layout can be tested without constructing a room or a renderer.
struct TurretProfile {
    bool supported = false;
    std::uint32_t subtype = 0;
    std::uint32_t version = 0;
    std::uint16_t health = 0;
    std::uint16_t beam_damage = 0;
    std::uint16_t splash_damage = 0;
    std::uint16_t contact_damage = 0;
    float min_angle_y = -45.0F;
    float max_angle_y = 45.0F;
    float angle_increment_y = 1.0F;
    float min_angle_x = 0.0F;
    float max_angle_x = 60.0F;
    float angle_increment_x = 1.0F;
    std::uint16_t shot_cooldown_frames = 0;
    std::uint16_t delay_frames = 0;
    std::uint16_t min_shots = 0;
    std::uint16_t max_shots = 0;
    // S10.Index pairs an Enemy45 turret with its Enemy44 synapse.
    std::int32_t index = -1;
    float shot_offset = 1.0F;
    float projectile_speed = 39.0F;
    float projectile_lifetime = 4.25F;
    std::uint8_t projectile_draw_func = 0xff;
    std::uint16_t projectile_color = 9055;
    std::uint8_t projectile_collision_effect = 242;
    std::uint8_t projectile_muzzle_effect = 65;
    scene::EntityVolume range_volume;
};

[[nodiscard]] TurretProfile decode_turret_profile(
    std::uint8_t id, std::span<const std::uint8_t> fields,
    net::Vec3 origin) noexcept;

[[nodiscard]] inline const auto& all() noexcept {
    return ::fruityprime::metadata::enemies();
}

[[nodiscard]] inline const Info& info(std::uint8_t id) noexcept {
    return ::fruityprime::metadata::enemy_info(id);
}

[[nodiscard]] const std::array<Profile, metadata::EnemyCount>&
profiles() noexcept;

[[nodiscard]] const Profile& profile(std::uint8_t id) noexcept;

// EnemyModelNames in Metadata.Enemies.cs. Empty entries are deliberate:
// those IDs are support records, linked boss parts, or the Hunter player
// slot and do not construct an independent model in the room loader.
[[nodiscard]] const std::array<std::string_view, metadata::EnemyCount>&
model_names() noexcept;
[[nodiscard]] std::string_view model_name(std::uint8_t id) noexcept;

[[nodiscard]] CombatTuning combat_tuning(std::uint8_t id) noexcept;

} // namespace fruityprime::enemy

namespace MphReadNative {
namespace EnemyCatalog = ::fruityprime::enemy;
}
