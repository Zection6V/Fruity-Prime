#pragma once

#include "enemy_module.hpp"

namespace fruityprime::gameplay { struct EnemyState; }

namespace fruityprime::enemy::module_45_slench_turret {

inline constexpr ModuleDescriptor kModule{
    45, "45_SlenchTurret.cs", "Enemy45Entity",
    PortStatus::SharedController, "update_slench_turret"};

// Enemy45Entity.EnemyInitialize.
void EnemyInitialize(gameplay::EnemyState& agent) noexcept;

// Enemy45Entity.HandleMessage: the four messages a Slench's synapses send
// its turrets.  Activation and deactivation are the obvious two; the
// other pair moves the ceiling on the turret's lights, which is how the
// wall shows what is left of the boss.
void HandleMessage(gameplay::EnemyState& agent, std::uint16_t message,
                   std::int32_t parameter1) noexcept;

// Enemy45Entity.GetMaxFrameCount, from the other end: the model's last
// frame, handed in by whoever loaded it.
void SetMaxFrameCount(gameplay::EnemyState& agent,
                      std::int32_t frames) noexcept;

} // namespace fruityprime::enemy::module_45_slench_turret

