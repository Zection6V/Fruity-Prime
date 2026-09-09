#pragma once

// Public facade for Metadata/Weapons.cs. The complete rows live in
// Metadata/weapon_metadata.hpp; this file supplies the managed static
// class's table names and its process-wide Current selection without making a
// second copy of any weapon data.

#include "Metadata/bot_weapons.hpp"
#include "Metadata/weapon_metadata.hpp"

#include <array>
#include <cstddef>
#include <span>
#include <stdexcept>

namespace fruityprime::weapon_catalog {

using WeaponInfo = ::fruityprime::metadata::weapon_table::WeaponInfo;
using BotWeaponValues = ::fruityprime::metadata::BotWeaponValues;
using BeamType = formats::BeamType;

inline constexpr std::array<BeamType, 8> AffinityWeapons{{
    BeamType::Missile,
    BeamType::VoltDriver,
    BeamType::Imperialist,
    BeamType::ShockCoil,
    BeamType::Judicator,
    BeamType::Magmaul,
    BeamType::Battlehammer,
    BeamType::PowerBeam
}};

// The managed property is null until SceneSetup selects a room. An empty span
// is the corresponding native state.
inline std::span<const WeaponInfo> Current{};

inline constexpr auto& Weapons1P =
    ::fruityprime::metadata::weapon_table::Weapons1P;
inline constexpr auto& WeaponsMP =
    ::fruityprime::metadata::weapon_table::WeaponsMP;
inline constexpr auto& EnemyWeapons =
    ::fruityprime::metadata::weapon_table::EnemyWeapons;
inline constexpr auto& BossWeapons =
    ::fruityprime::metadata::weapon_table::BossWeapons;
inline constexpr auto& GoreaWeapons =
    ::fruityprime::metadata::weapon_table::GoreaWeapons;
inline constexpr auto& PlatformWeapons =
    ::fruityprime::metadata::weapon_table::PlatformWeapons;
inline constexpr auto& Ricochets =
    ::fruityprime::metadata::weapon_table::Ricochets;
inline constexpr auto& BotWeapons = ::fruityprime::metadata::BotWeapons;

inline BeamType GetAffinityBeam(formats::Hunter hunter) {
    const auto index = static_cast<std::size_t>(hunter);
    if (index >= AffinityWeapons.size()) {
        // C# array indexing throws for an invalid enum value. Do not silently
        // turn a bad hunter id into Samus's affinity weapon.
        throw std::out_of_range("Weapons.AffinityWeapons");
    }
    return AffinityWeapons[index];
}

inline void SetCurrent(bool multiplayer) noexcept {
    if (multiplayer) {
        Current = std::span<const WeaponInfo>(WeaponsMP.data(),
                                               WeaponsMP.size());
    } else {
        Current = std::span<const WeaponInfo>(Weapons1P.data(),
                                               Weapons1P.size());
    }
}

} // namespace fruityprime::weapon_catalog

namespace MphReadNative {
namespace Weapons = ::fruityprime::weapon_catalog;
}
