#pragma once

#include "enemy_module.hpp"
#include "Mods/Network/net_protocol.hpp"

namespace fruityprime::gameplay { struct EnemyState; }

namespace fruityprime::enemy::module_30_trocra {

inline constexpr ModuleDescriptor kModule{
    30, "30_Trocra.cs", "Enemy30Entity",
    PortStatus::ImplementedController, "update_trocra"};

// Enemy30Entity.EnemyInitialize.
void EnemyInitialize(gameplay::EnemyState& agent) noexcept;

// Enemy30Entity.SetSpeed: Gorea throws these, so their speed is given to
// them rather than chosen.
void SetSpeed(gameplay::EnemyState& agent, net::Vec3 speed) noexcept;

// Enemy30Entity.EnemyProcess.  A crystal in flight dies on whatever it
// touches: a player it has reached, or a wall it has passed through since
// last frame.  `blocked` is the caller's trace between the two positions,
// because only it can make one.
[[nodiscard]] bool EnemyProcess(gameplay::EnemyState& agent, bool touching,
                                bool blocked) noexcept;

// Enemy30Entity.DieAndSpawnEffect's damage half.  A crystal going off
// hurts whoever is within two units, falling off with distance -- unless
// they were actually touching it, in which case they take all of it.  A
// wall between them cancels it entirely, which the caller decides because
// only it can trace one.
struct Burst {
    bool hit = false;
    int damage = 0;
    // Away from the crystal, scaled by how much of the blast landed.  Up,
    // if the player is standing exactly on it.
    net::Vec3 knockback{};
};
[[nodiscard]] Burst DieAndSpawnEffect(gameplay::EnemyState& agent,
                                      net::Vec3 player_position,
                                      bool touching, bool blocked) noexcept;

// Enemy30Entity.Explode: the same burst, with Gorea's own effect.
void Explode(gameplay::EnemyState& agent) noexcept;

// Enemy30Entity.EnemyTakeDamage's drop roll.  Ten in a hundred and ninety
// for a small health, sixty for small ammo, and nothing the rest of the
// time -- so a crystal is worth breaking about a third of the time.
enum class Drop : std::uint8_t { None, HealthSmall, UASmall };
[[nodiscard]] Drop EnemyTakeDamage(gameplay::EnemyState& agent) noexcept;

} // namespace fruityprime::enemy::module_30_trocra
