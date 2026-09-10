#pragma once

#include "enemy_module.hpp"

namespace fruityprime::gameplay { struct EnemyState; }

namespace fruityprime::enemy::module_06_petrasyl4 {

inline constexpr ModuleDescriptor kModule{
    6, "06_Petrasyl4.cs", "Enemy06Entity",
    PortStatus::SharedController, "update_petrasyl"};

// Enemy06Entity.EnemyInitialize.
void EnemyInitialize(gameplay::EnemyState& agent) noexcept;

} // namespace fruityprime::enemy::module_06_petrasyl4

