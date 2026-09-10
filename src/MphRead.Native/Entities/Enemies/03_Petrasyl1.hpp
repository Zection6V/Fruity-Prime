#pragma once

#include "enemy_module.hpp"

namespace fruityprime::gameplay { struct EnemyState; }

namespace fruityprime::enemy::module_03_petrasyl1 {

inline constexpr ModuleDescriptor kModule{
    3, "03_Petrasyl1.cs", "Enemy03Entity",
    PortStatus::ImplementedController, "update_petrasyl1"};

// Enemy03Entity.EnemyInitialize.
void EnemyInitialize(gameplay::EnemyState& agent) noexcept;

} // namespace fruityprime::enemy::module_03_petrasyl1

