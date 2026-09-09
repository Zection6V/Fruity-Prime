#pragma once

#include "enemy_module.hpp"

namespace fruityprime::enemy::module_02_temroid {

inline constexpr ModuleDescriptor kModule{
    2, "02_Temroid.cs", "Enemy02Entity",
    PortStatus::ImplementedController, "update_temroid"};

} // namespace fruityprime::enemy::module_02_temroid

