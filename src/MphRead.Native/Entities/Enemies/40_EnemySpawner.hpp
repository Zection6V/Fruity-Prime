#pragma once

#include "enemy_module.hpp"

namespace fruityprime::enemy::module_40_enemy_spawner {

inline constexpr ModuleDescriptor kModule{
    40, "40_EnemySpawner.cs", "Enemy40Entity",
    PortStatus::SpawnerBoundary, "update_enemy_spawns"};

} // namespace fruityprime::enemy::module_40_enemy_spawner

