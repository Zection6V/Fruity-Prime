#pragma once

#include "enemy_module.hpp"
#include "Mods/Network/net_protocol.hpp"

namespace fruityprime::gameplay { struct EnemyState; }

namespace fruityprime::enemy::module_42_slench_shield {

inline constexpr ModuleDescriptor kModule{
    42, "42_SlenchShield.cs", "Enemy42Entity",
    PortStatus::SharedController, "update_slench_shield"};

// Enemy42Entity.EnemyInitialize.
void EnemyInitialize(gameplay::EnemyState& agent) noexcept;

// Enemy42Entity.EnemyProcess: the shield sits a fixed distance in front
// of the Slench's eye, so it is placed rather than moved.
void EnemyProcess(gameplay::EnemyState& agent, net::Vec3 slench_position,
                  net::Vec3 slench_facing, float shield_offset) noexcept;

// Enemy42Entity.UpdateScanId.  Which scan entry the shield answers with
// changes with the Slench's phase, because a closed eye and an open one
// are different things to read about.
void UpdateScanId(gameplay::EnemyState& agent, std::uint16_t scan_id) noexcept;

// Enemy42Entity.EnemyTakeDamage: what a hit on the shield does.  The
// shield itself never loses energy -- it is a switch, not a target.
enum class ShieldHit : std::uint8_t {
    // The eye is open and exposed: the damage goes through to the Slench.
    ThroughToSlench,
    // The eye is shut: the shield takes it instead, which is what turns
    // the Slench between its phases.
    AgainstShield
};
[[nodiscard]] ShieldHit EnemyTakeDamage(gameplay::EnemyState& agent,
                                        bool eye_closed,
                                        bool vulnerable) noexcept;

} // namespace fruityprime::enemy::module_42_slench_shield

