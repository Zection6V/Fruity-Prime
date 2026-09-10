#pragma once

#include "enemy_module.hpp"
#include "Mods/Network/net_protocol.hpp"

namespace fruityprime::gameplay { struct EnemyState; }

namespace fruityprime::enemy::module_25_gorea_head {

inline constexpr ModuleDescriptor kModule{
    25, "25_GoreaHead.cs", "Enemy25Entity",
    PortStatus::PartialController, "update_gorea_head"};

// Enemy25Entity.EnemyInitialize.
void EnemyInitialize(gameplay::EnemyState& agent) noexcept;

// Enemy25Entity.EnemyProcess: the head is a hurt volume on a model node,
// so it is placed where that node is rather than moved.
void EnemyProcess(gameplay::EnemyState& agent,
                  net::Vec3 node_position) noexcept;

// Enemy25Entity.EnemyTakeDamage.  The head does not lose energy: it
// accumulates what it has been hit for and hands that to Gorea, which is
// what the fight is actually counting.  Returns true, meaning unaffected.
[[nodiscard]] bool EnemyTakeDamage(gameplay::EnemyState& agent) noexcept;

// Enemy25Entity.RemoveFlashEffect and RespawnFlashEffect: the eye flash,
// which sits a way out in front of Gorea's face rather than on the head.
void RemoveFlashEffect(gameplay::EnemyState& agent) noexcept;
[[nodiscard]] net::Vec3 RespawnFlashEffect(gameplay::EnemyState& agent,
                                           net::Vec3 gorea_facing,
                                           net::Vec3 gorea_up) noexcept;

} // namespace fruityprime::enemy::module_25_gorea_head
