#pragma once

#include "enemy_module.hpp"

namespace fruityprime::enemy::module_42_slench_shield {

inline constexpr ModuleDescriptor kModule{
    42, "42_SlenchShield.cs", "Enemy42Entity",
    PortStatus::SharedController, "update_slench_shield"};

} // namespace fruityprime::enemy::module_42_slench_shield

