#pragma once

#include "enemy_module.hpp"

namespace fruityprime::gameplay { struct EnemyState; }

namespace fruityprime::enemy::module_11_shriekbat {

inline constexpr ModuleDescriptor kModule{
    11, "11_Shriekbat.cs", "Enemy11Entity",
    PortStatus::ImplementedController, "update_shriekbat"};

// Enemy11Entity.EnemyInitialize.  An enemy is made by its spawner here
// rather than constructing itself, so the initialise it would have run is
// reached from there -- but it lives in this file, where the managed one
// does, and not in the spawner beside fifty-one others.
void EnemyInitialize(gameplay::EnemyState& agent) noexcept;

} // namespace fruityprime::enemy::module_11_shriekbat

