#pragma once

#include "enemy_module.hpp"

namespace fruityprime::gameplay { struct EnemyState; }

namespace fruityprime::enemy::module_04_petrasyl2 {

inline constexpr ModuleDescriptor kModule{
    4, "04_Petrasyl2.cs", "Enemy04Entity",
    PortStatus::SharedController, "update_petrasyl"};

// Enemy04Entity.EnemyInitialize.
void EnemyInitialize(gameplay::EnemyState& agent) noexcept;

} // namespace fruityprime::enemy::module_04_petrasyl2

