#pragma once

#include "enemy_module.hpp"

namespace fruityprime::gameplay { struct EnemyState; }

namespace fruityprime::enemy::module_39_fire_spawn {

inline constexpr ModuleDescriptor kModule{
    39, "39_FireSpawn.cs", "Enemy39Entity",
    PortStatus::ImplementedController, "update_firespawn"};

// Enemy39Entity.EnemyInitialize.
void EnemyInitialize(gameplay::EnemyState& agent) noexcept;

} // namespace fruityprime::enemy::module_39_fire_spawn

