#pragma once

#include "enemy_module.hpp"
#include "Mods/Network/net_protocol.hpp"

namespace fruityprime::gameplay { struct EnemyState; }

namespace fruityprime::enemy::module_29_gorea_seal_sphere_1 {

inline constexpr ModuleDescriptor kModule{
    29, "29_GoreaSealSphere1.cs", "Enemy29Entity",
    PortStatus::PartialController, "update_gorea_seal_sphere_1"};

// Enemy29Entity.EnemyInitialize.
void EnemyInitialize(gameplay::EnemyState& agent) noexcept;

// Enemy29Entity.Activate and Deactivate: Gorea opens and closes the
// sphere as its phases come round, and a closed one is not shootable at
// all rather than merely tough.
void Activate(gameplay::EnemyState& agent, std::uint16_t scan_id) noexcept;
void Deactivate(gameplay::EnemyState& agent) noexcept;

// Enemy29Entity.EnemyProcess: the sphere sits on a node of Gorea's
// chest, so it is placed where that node is.
void EnemyProcess(gameplay::EnemyState& agent, net::Vec3 node_position,
                  bool gorea_visible) noexcept;

// Enemy29Entity.DamageTimer: how long the chest core stays lit red
// after a hit.  Gorea reads it rather than being told.
[[nodiscard]] std::uint32_t DamageTimer(
    const gameplay::EnemyState& agent) noexcept;

// Enemy29Entity.EnemyTakeDamage.  The sphere never loses energy: it
// counts what it has been hit for, capped at the phase's real health.
struct Hit {
    // Whether this hit crossed a new multiple of ten within the phase,
    // which is when the sphere makes a noise and throws sparks.  A
    // thousand is a phase boundary and is deliberately not counted: a
    // phase change is loud enough on its own.
    bool milestone = false;
    // The running total after this hit.
    std::uint32_t damage = 0;
};
[[nodiscard]] Hit EnemyTakeDamage(gameplay::EnemyState& agent) noexcept;

} // namespace fruityprime::enemy::module_29_gorea_seal_sphere_1
