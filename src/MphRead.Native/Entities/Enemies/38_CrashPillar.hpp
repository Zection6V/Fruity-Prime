#pragma once

#include "enemy_module.hpp"

namespace fruityprime::gameplay { struct EnemyState; }

namespace fruityprime::enemy::module_38_crash_pillar {

inline constexpr ModuleDescriptor kModule{
    38, "38_CrashPillar.cs", "Enemy38Entity",
    PortStatus::ImplementedController, "update_crash_pillar"};

// Enemy38Entity.EnemyInitialize.
void EnemyInitialize(gameplay::EnemyState& agent) noexcept;

} // namespace fruityprime::enemy::module_38_crash_pillar

