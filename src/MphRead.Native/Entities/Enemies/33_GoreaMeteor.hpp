#pragma once

#include "enemy_module.hpp"
#include "Mods/Network/net_protocol.hpp"

namespace fruityprime::gameplay { struct EnemyState; }

namespace fruityprime::enemy::module_33_gorea_meteor {

inline constexpr ModuleDescriptor kModule{
    33, "33_GoreaMeteor.cs", "Enemy33Entity",
    PortStatus::PartialController, "update_gorea_meteor"};

// Enemy33Entity.EnemyInitialize.
void EnemyInitialize(gameplay::EnemyState& agent) noexcept;

// Enemy33Entity.InitializePosition: where Gorea put it.  Separate from
// the constructor because the base position has to move with it.
void InitializePosition(gameplay::EnemyState& agent,
                        net::Vec3 position) noexcept;

} // namespace fruityprime::enemy::module_33_gorea_meteor
