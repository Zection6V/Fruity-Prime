#pragma once

#include "enemy_module.hpp"

namespace fruityprime::gameplay { struct EnemyState; }

namespace fruityprime::enemy::module_00_war_wasp {

inline constexpr ModuleDescriptor kModule{
    0, "00_WarWasp.cs", "Enemy00Entity",
    PortStatus::ImplementedController, "update_warwasp"};

// Enemy00Entity.EnemyInitialize.
void EnemyInitialize(gameplay::EnemyState& agent) noexcept;

} // namespace fruityprime::enemy::module_00_war_wasp
