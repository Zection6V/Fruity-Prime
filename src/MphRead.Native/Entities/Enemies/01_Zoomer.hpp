#pragma once

#include "enemy_module.hpp"
#include "enemy_catalog.hpp"
#include "Mods/Network/net_protocol.hpp"

namespace fruityprime::gameplay { struct EnemyState; }

namespace fruityprime::enemy::module_01_zoomer {

inline constexpr ModuleDescriptor kModule{
    1, "01_Zoomer.cs", "Enemy01Entity",
    PortStatus::ImplementedController, "update_zoomer"};

// Enemy01Entity.SpawnData and SpawnFields: what the spawner authored
// for this Zoomer.  The managed class reaches through its spawner for
// them every time; here they are already decoded onto the enemy, and
// these say so rather than leaving the reader to find out.
[[nodiscard]] const enemy::ZoomerProfile& SpawnFields(
    const gameplay::EnemyState& agent) noexcept;
[[nodiscard]] net::Vec3 SpawnData(
    const gameplay::EnemyState& agent) noexcept;

// Enemy01Entity.EnemyInitialize.
void EnemyInitialize(gameplay::EnemyState& agent) noexcept;

} // namespace fruityprime::enemy::module_01_zoomer

