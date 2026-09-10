#pragma once

#include "enemy_module.hpp"

namespace fruityprime::gameplay { struct EnemyState; }

namespace fruityprime::enemy::module_16_blastcap {

inline constexpr ModuleDescriptor kModule{
    16, "16_Blastcap.cs", "Enemy16Entity",
    PortStatus::ImplementedController, "update_blastcap"};

// Enemy16Entity.EnemyInitialize.
void EnemyInitialize(gameplay::EnemyState& agent) noexcept;

} // namespace fruityprime::enemy::module_16_blastcap

