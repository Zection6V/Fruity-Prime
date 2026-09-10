#pragma once

#include "enemy_module.hpp"

namespace fruityprime::gameplay { struct EnemyState; }

namespace fruityprime::enemy::module_05_petrasyl3 {

inline constexpr ModuleDescriptor kModule{
    5, "05_Petrasyl3.cs", "Enemy05Entity",
    PortStatus::SharedController, "update_petrasyl"};

// Enemy05Entity.EnemyInitialize.
void EnemyInitialize(gameplay::EnemyState& agent) noexcept;

} // namespace fruityprime::enemy::module_05_petrasyl3

