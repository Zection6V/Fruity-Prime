#pragma once

// Native counterpart of Weapons.EquipInfo: what a shooter currently has
// equipped and how far its charge and smoke have built up.
//
// The managed class reaches the ammo pool through two delegates because the
// pool lives on whatever owns the equipment -- a player, a turret, a boss
// segment.  The native record keeps a pointer to the pool instead, which is
// the same indirection without a per-shot allocation.

#include "Metadata/weapon_metadata.hpp"

#include <array>
#include <cstdint>
#include <functional>
#include <limits>
#include <stdexcept>
#include <vector>

namespace fruityprime::metadata {

struct EquipInfo {
    bool Zoomed = false;
    const weapon_table::WeaponInfo* Weapon = nullptr;
    // The managed field holds the beams this equipment owns; the native
    // projectile pool is indexed, so the ids are kept instead of pointers.
    std::vector<std::int32_t> Beams;
    std::uint16_t ChargeLevel = 0;
    std::uint16_t SmokeLevel = 0;

    // Weapons.EquipInfo.GetAmmo / SetAmmo / InfiniteAmmo / Ammo.
    std::function<std::int32_t()> GetAmmo;
    std::function<void(std::int32_t)> SetAmmo;
    std::int32_t* AmmoPool = nullptr;
    bool InfiniteAmmo = false;

    [[nodiscard]] std::int32_t ammo() const {
        if (!InfiniteAmmo) {
            if (GetAmmo) {
                return GetAmmo();
            }
            // AmmoPool is the native storage adapter for managed GetAmmo.
            if (AmmoPool != nullptr) {
                return *AmmoPool;
            }
        }
        return (std::numeric_limits<std::int32_t>::max)();
    }
    void set_ammo(std::int32_t value) {
        if (InfiniteAmmo) {
            return;
        }
        if (SetAmmo) {
            SetAmmo(value);
        } else if (AmmoPool != nullptr) {
            AmmoPool[0] = value;
        }
    }

    // The overload pair mirrors the native getter/setter convention used for
    // the generated memory classes while keeping the managed property name
    // available to ported call sites.
    [[nodiscard]] std::int32_t Ammo() const { return ammo(); }
    void Ammo(std::int32_t value) { set_ammo(value); }

    // Weapons.EquipInfo.DrawFuncIds / DmgDirTypes.
    std::array<std::uint8_t, 2> DrawFuncIds{255, 255};
    std::array<std::uint8_t, 2> DmgDirTypes{255, 255};

private:
    static constexpr std::uint16_t UnsetDamage =
        (std::numeric_limits<std::uint16_t>::max)();
    static constexpr std::int32_t UnsetHomingTolerance =
        (std::numeric_limits<std::int32_t>::max)();

    std::uint16_t uncharged_damage_ = UnsetDamage;
    std::uint16_t min_charge_damage_ = UnsetDamage;
    std::uint16_t charged_damage_ = UnsetDamage;
    std::uint16_t headshot_damage_ = UnsetDamage;
    std::uint16_t min_charge_headshot_damage_ = UnsetDamage;
    std::uint16_t charged_headshot_damage_ = UnsetDamage;
    std::uint16_t splash_damage_ = UnsetDamage;
    std::uint16_t min_charge_splash_damage_ = UnsetDamage;
    std::uint16_t charged_splash_damage_ = UnsetDamage;
    std::int32_t homing_tolerance_ = UnsetHomingTolerance;

    [[nodiscard]] const weapon_table::WeaponInfo& weapon_or_throw() const {
        if (Weapon == nullptr) {
            throw std::logic_error("EquipInfo.Weapon is null");
        }
        return *Weapon;
    }

public:
    // The sentinel/default behavior is identical to the managed properties:
    // an unset override reads the value from Weapon, and assigning the
    // maximum value restores the sentinel just as it does in C#.
#define FRUITY_PRIME_EQUIP_DAMAGE_PROPERTY(Name, member, type)              \
    [[nodiscard]] type Name() const {                                       \
        const auto& weapon = weapon_or_throw();                             \
        return member##_ == UnsetDamage ? static_cast<type>(weapon.member)  \
                                        : member##_;                         \
    }                                                                        \
    void Name(type value) { member##_ = value; }

    FRUITY_PRIME_EQUIP_DAMAGE_PROPERTY(UnchargedDamage, uncharged_damage,
                                       std::uint16_t)
    FRUITY_PRIME_EQUIP_DAMAGE_PROPERTY(MinChargeDamage, min_charge_damage,
                                       std::uint16_t)
    FRUITY_PRIME_EQUIP_DAMAGE_PROPERTY(ChargedDamage, charged_damage,
                                       std::uint16_t)
    FRUITY_PRIME_EQUIP_DAMAGE_PROPERTY(HeadshotDamage, headshot_damage,
                                       std::uint16_t)
    FRUITY_PRIME_EQUIP_DAMAGE_PROPERTY(MinChargeHeadshotDamage,
                                       min_charge_headshot_damage,
                                       std::uint16_t)
    FRUITY_PRIME_EQUIP_DAMAGE_PROPERTY(ChargedHeadshotDamage,
                                       charged_headshot_damage,
                                       std::uint16_t)
    FRUITY_PRIME_EQUIP_DAMAGE_PROPERTY(SplashDamage, splash_damage,
                                       std::uint16_t)
    FRUITY_PRIME_EQUIP_DAMAGE_PROPERTY(MinChargeSplashDamage,
                                       min_charge_splash_damage,
                                       std::uint16_t)
    FRUITY_PRIME_EQUIP_DAMAGE_PROPERTY(ChargedSplashDamage,
                                       charged_splash_damage,
                                       std::uint16_t)

#undef FRUITY_PRIME_EQUIP_DAMAGE_PROPERTY

    [[nodiscard]] std::int32_t HomingTolerance() const {
        const auto& weapon = weapon_or_throw();
        return homing_tolerance_ == UnsetHomingTolerance
            ? weapon.homing_tolerance : homing_tolerance_;
    }
    void HomingTolerance(std::int32_t value) { homing_tolerance_ = value; }
};

} // namespace fruityprime::metadata
