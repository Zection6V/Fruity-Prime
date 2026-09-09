#pragma once

#include "enemy_module.hpp"

namespace fruityprime::enemy::module_01_zoomer {

inline constexpr ModuleDescriptor kModule{
    1, "01_Zoomer.cs", "Enemy01Entity",
    PortStatus::ImplementedController, "update_zoomer"};

} // namespace fruityprime::enemy::module_01_zoomer

