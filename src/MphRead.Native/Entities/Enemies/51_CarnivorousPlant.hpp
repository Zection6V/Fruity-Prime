#pragma once

#include "enemy_module.hpp"

namespace fruityprime::gameplay { struct EnemyState; }

namespace fruityprime::enemy::module_51_carnivorous_plant {

inline constexpr ModuleDescriptor kModule{
    51, "51_CarnivorousPlant.cs", "Enemy51Entity",
    PortStatus::ImplementedController, "update_carnivorous_plant"};

// Enemy51Entity.EnemyInitialize.
void EnemyInitialize(gameplay::EnemyState& agent) noexcept;

} // namespace fruityprime::enemy::module_51_carnivorous_plant
