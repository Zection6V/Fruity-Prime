#pragma once

#include "enemy_module.hpp"

namespace fruityprime::enemy::module_18_alimbic_turret {

inline constexpr ModuleDescriptor kModule{
    18, "18_AlimbicTurret.cs", "Enemy18Entity",
    PortStatus::ImplementedController, "update_alimbic_turret"};

} // namespace fruityprime::enemy::module_18_alimbic_turret

