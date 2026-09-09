#pragma once

#include "enemy_module.hpp"

namespace fruityprime::enemy::module_45_slench_turret {

inline constexpr ModuleDescriptor kModule{
    45, "45_SlenchTurret.cs", "Enemy45Entity",
    PortStatus::SharedController, "update_slench_turret"};

} // namespace fruityprime::enemy::module_45_slench_turret

