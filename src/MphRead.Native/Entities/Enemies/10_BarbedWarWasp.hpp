#pragma once

#include "enemy_module.hpp"

namespace fruityprime::gameplay { struct EnemyState; }

namespace fruityprime::enemy::module_10_barbed_war_wasp {

inline constexpr ModuleDescriptor kModule{
    10, "10_BarbedWarWasp.cs", "Enemy10Entity",
    PortStatus::ImplementedController, "update_barbed_warwasp"};

// Enemy10Entity.EnemyInitialize.
void EnemyInitialize(gameplay::EnemyState& agent) noexcept;

} // namespace fruityprime::enemy::module_10_barbed_war_wasp
