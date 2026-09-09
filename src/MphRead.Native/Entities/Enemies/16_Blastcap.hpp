#pragma once

#include "enemy_module.hpp"

namespace fruityprime::enemy::module_16_blastcap {

inline constexpr ModuleDescriptor kModule{
    16, "16_Blastcap.cs", "Enemy16Entity",
    PortStatus::ImplementedController, "update_blastcap"};

} // namespace fruityprime::enemy::module_16_blastcap

