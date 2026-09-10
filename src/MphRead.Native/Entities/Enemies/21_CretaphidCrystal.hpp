#pragma once

#include "enemy_module.hpp"
#include "Mods/Network/net_protocol.hpp"

namespace fruityprime::gameplay { struct EnemyState; }

namespace fruityprime::enemy::module_21_cretaphid_crystal {

inline constexpr ModuleDescriptor kModule{
    21, "21_CretaphidCrystal.cs", "Enemy21Entity",
    PortStatus::SharedController, "update_cretaphid_crystal"};

// Enemy21Entity.SetUp: the crystal is made by the Cretaphid rather than
// by a spawner, and is told where on its parent it hangs and how much of
// the fight it is worth.
void SetUp(gameplay::EnemyState& agent, std::uint8_t segment_index,
           std::uint16_t health) noexcept;

// Enemy21Entity.SpawnBeam: where a crystal's shot starts and where it is
// aimed.  Half a unit above the player rather than at them, so the shot
// arrives at head height on a target that is standing still.  The
// Cretaphid decides when; this decides where.
struct Shot {
    net::Vec3 position;
    net::Vec3 direction;
};
[[nodiscard]] Shot SpawnBeam(const gameplay::EnemyState& agent,
                             net::Vec3 player_position) noexcept;

// Enemy21Entity.EnemyTakeDamage: a broken crystal is not a dead one.
// Returns whether the crystal is unaffected, which is what the managed
// signature means.
[[nodiscard]] bool EnemyTakeDamage(gameplay::EnemyState& agent) noexcept;

} // namespace fruityprime::enemy::module_21_cretaphid_crystal

