#pragma once

#include "enemy_module.hpp"

namespace fruityprime::enemy::module_00_war_wasp {

inline constexpr ModuleDescriptor kModule{
    0, "00_WarWasp.cs", "Enemy00Entity",
    PortStatus::ImplementedController, "update_warwasp"};

} // namespace fruityprime::enemy::module_00_war_wasp
