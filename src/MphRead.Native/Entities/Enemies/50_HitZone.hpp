#pragma once

#include "enemy_module.hpp"

namespace fruityprime::enemy::module_50_hit_zone {

inline constexpr ModuleDescriptor kModule{
    50, "50_HitZone.cs", "Enemy50Entity",
    PortStatus::ImplementedController, "update_hit_zone"};

} // namespace fruityprime::enemy::module_50_hit_zone

