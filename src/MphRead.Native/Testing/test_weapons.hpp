#pragma once

#include "Formats/Types.hpp"

#include <array>
#include <cstdint>
#include <iosfwd>
#include <string>
#include <vector>

namespace fruityprime::testing::weapons {

enum class WeaponFlags : std::uint32_t {
    None = 0x00000000,
    PartialCharge = 0x00000100,
    CanCharge = 0x00000200,
    RepeatFire = 0x00000400,
    CanZoom = 0x00000800,
    RicochetUncharged = 0x00001000,
    RicochetCharged = 0x00002000,
    AutoRelease = 0x00004000,
    SelfDamageUncharged = 0x00008000,
    SelfDamageCharged = 0x00010000,
    ForceEffectUncharged = 0x00020000,
    ForceEffectCharged = 0x00040000,
    AoeUncharged = 0x00080000,
    AoeCharged = 0x00100000,
    Continuous = 0x00200000,
    DestroyableUncharged = 0x00400000,
    DestroyableCharged = 0x00800000,
    RadIdx1Uncharged = 0x01000000,
    RadIdx2Uncharged = 0x02000000,
    RadIdx1Charged = 0x04000000,
    RadIdx2Charged = 0x08000000,
    LifeDrainUncharged = 0x10000000,
    LifeDrainCharged = 0x20000000,
    SurfaceCollision = 0x40000000
};

struct RawWeaponInfo {
    formats::BeamType beam = formats::BeamType::None;
    formats::BeamType beam_kind = formats::BeamType::None;
    std::array<std::uint8_t, 2> draw_func_ids{};
    std::array<std::uint16_t, 2> colors{};
    std::uint32_t flags_and_priority;
    std::uint16_t splash_damage;
    std::uint16_t min_charge_splash_damage;
    std::uint16_t charged_splash_damage;
    std::array<std::uint8_t, 2> splash_dmg_types{};
    std::uint8_t shot_cooldown;
    std::uint8_t shot_cooldown_related;
    std::uint8_t ammo_type;
    std::array<std::uint8_t, 2> beam_types{};
    std::array<std::uint8_t, 2> muzzle_effects{};
    std::array<std::uint8_t, 2> dmg_dir_types{};
    std::array<std::uint8_t, 2> damage_interpolations{};
    std::array<formats::Affliction, 2> afflictions{};
    std::uint8_t padding21;
    std::uint16_t min_charge;
    std::uint16_t full_charge;
    std::uint16_t ammo_cost;
    std::uint16_t min_charge_cost;
    std::uint16_t charge_cost;
    std::uint16_t uncharged_damage;
    std::uint16_t min_charge_damage;
    std::uint16_t charged_damage;
    std::uint16_t headshot_damage;
    std::uint16_t min_charge_headshot_damage;
    std::uint16_t charged_headshot_damage;
    std::uint16_t uncharged_lifespan;
    std::uint16_t min_charge_lifespan;
    std::uint16_t charged_lifespan;
    std::array<std::uint16_t, 2> speed_decay{};
    std::uint16_t padding42;
    std::array<std::uint16_t, 2> speed_interp{};
    std::int32_t uncharged_dmg_dir_mag;
    std::int32_t min_charge_dmg_dir_mag;
    std::int32_t charged_dmg_dir_mag;
    std::int32_t zoom_fov;
    std::int32_t uncharged_cyl_radius;
    std::int32_t min_charge_cyl_radius;
    std::int32_t charged_cyl_radius;
    std::int32_t uncharged_speed;
    std::int32_t min_charge_speed;
    std::int32_t charged_speed;
    std::int32_t uncharged_final_speed;
    std::int32_t min_charge_final_speed;
    std::int32_t charged_final_speed;
    std::int32_t uncharged_gravity;
    std::int32_t min_charge_gravity;
    std::int32_t charged_gravity;
    std::int32_t uncharged_homing;
    std::int32_t min_charge_homing;
    std::int32_t charged_homing;
    std::int32_t homing_range;
    std::int32_t homing_tolerance;
    std::int32_t uncharged_scale;
    std::int32_t min_charge_scale;
    std::int32_t charged_scale;
    std::int32_t uncharged_distance;
    std::int32_t min_charge_distance;
    std::int32_t charged_distance;
    std::int32_t uncharged_spread;
    std::int32_t min_charge_spread;
    std::int32_t charged_spread;
    std::int32_t uncharged_ricochet_loss_h;
    std::int32_t min_charge_ricochet_loss_h;
    std::int32_t charged_ricochet_loss_h;
    std::int32_t uncharged_ricochet_loss_v;
    std::int32_t min_charge_ricochet_loss_v;
    std::int32_t charged_ricochet_loss_v;
    std::array<std::uint32_t, 2> ricochet_weapon_ptr{};
    std::uint16_t projectile_count;
    std::uint16_t min_charged_projectile_count;
    std::uint16_t charge_projectile_count;
    std::uint16_t smoke_start;
    std::uint16_t smoke_minimum;
    std::uint16_t smoke_drain;
    std::uint16_t smoke_shot_amount;
    std::uint16_t smoke_charge_amount;

    [[nodiscard]] constexpr std::uint8_t priority() const noexcept {
        return static_cast<std::uint8_t>(flags_and_priority & 0xffU);
    }

    [[nodiscard]] constexpr WeaponFlags flags() const noexcept {
        return static_cast<WeaponFlags>(flags_and_priority & 0xffffff00U);
    }
};

static_assert(sizeof(RawWeaponInfo) == 0xf0);

[[nodiscard]] std::vector<RawWeaponInfo> get_ricochets();
[[nodiscard]] std::vector<RawWeaponInfo> get_1p_weapons();
[[nodiscard]] std::vector<RawWeaponInfo> get_mp_weapons();
[[nodiscard]] std::vector<RawWeaponInfo> get_enemy_weapons();
[[nodiscard]] std::vector<RawWeaponInfo> get_platform_weapons();

[[nodiscard]] std::string dump_weapon_info(const RawWeaponInfo& weapon);
void test_weapon_info(std::ostream& output);

} // namespace fruityprime::testing::weapons

