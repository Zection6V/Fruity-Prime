#pragma once

#include "enemy_module.hpp"

namespace fruityprime::gameplay { struct EnemyState; }

namespace fruityprime::enemy::module_43_slench_nest {

inline constexpr ModuleDescriptor kModule{
    43, "43_SlenchNest.cs", "Enemy43Entity",
    PortStatus::SharedController, "update_slench_nest"};

// Enemy43Entity.EnemyInitialize.
void EnemyInitialize(gameplay::EnemyState& agent) noexcept;

} // namespace fruityprime::enemy::module_43_slench_nest

