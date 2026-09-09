#pragma once

#include "enemy_module.hpp"

namespace fruityprime::enemy::module_30_trocra {

inline constexpr ModuleDescriptor kModule{
    30, "30_Trocra.cs", "Enemy30Entity",
    PortStatus::ImplementedController, "update_trocra"};

} // namespace fruityprime::enemy::module_30_trocra
