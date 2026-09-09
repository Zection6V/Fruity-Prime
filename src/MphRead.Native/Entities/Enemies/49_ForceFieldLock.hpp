#pragma once

#include "enemy_module.hpp"

namespace fruityprime::enemy::module_49_force_field_lock {

inline constexpr ModuleDescriptor kModule{
    49, "49_ForceFieldLock.cs", "Enemy49Entity",
    PortStatus::ImplementedController, "update_force_field_lock"};

} // namespace fruityprime::enemy::module_49_force_field_lock

