#pragma once

#include "enemy_module.hpp"
#include "Mods/Network/net_protocol.hpp"

namespace fruityprime::gameplay { struct EnemyState; }

namespace fruityprime::enemy::module_49_force_field_lock {

inline constexpr ModuleDescriptor kModule{
    49, "49_ForceFieldLock.cs", "Enemy49Entity",
    PortStatus::ImplementedController, "update_force_field_lock"};

// Enemy49Entity.ClearEffectiveness and SetEffectiveness.  A lock is
// immune to everything except the one beam its field is keyed to, which
// is the whole puzzle: shooting it with anything else does nothing at
// all rather than a little.
void ClearEffectiveness(gameplay::EnemyState& agent) noexcept;
void SetEffectiveness(gameplay::EnemyState& agent, std::uint8_t beam,
                      std::uint8_t effectiveness) noexcept;

// Enemy49Entity.EnemyInitialize: which beam a field's type answers to.
// Type eight answers to nothing and is opened with a bomb instead.
void EnemyInitialize(gameplay::EnemyState& agent,
                     std::uint8_t field_type) noexcept;

// Enemy49Entity.EnemyProcess: the lock always faces the side of its
// field the player is on, so it can never be shot edge-on.  It fires
// back for as long as LockHit gave it.
struct LockFrame {
    // Whether the lock flipped to the player's side this pass.
    bool flipped = false;
    // Whether a shot leaves this pass.
    bool shooting = false;
};
[[nodiscard]] LockFrame EnemyProcess(gameplay::EnemyState& agent,
                                     net::Vec3 field_position,
                                     net::Vec3 field_facing,
                                     net::Vec3 camera_position) noexcept;

// Enemy49Entity.EnemyTakeDamage: a lock that has been opened is opened.
// Returns whether it is unaffected.
[[nodiscard]] bool EnemyTakeDamage(gameplay::EnemyState& agent) noexcept;

// Enemy49Entity.LockHit: a lock hit by a beam it is immune to fires that
// beam back at whoever sent it.  A Shock Coil lock does it for a full
// second; every other kind is a single shot.
struct LockShot {
    bool fired = false;
    std::uint8_t frames = 0;
};
[[nodiscard]] LockShot LockHit(gameplay::EnemyState& agent,
                               std::uint8_t beam,
                               std::uint8_t field_type) noexcept;

} // namespace fruityprime::enemy::module_49_force_field_lock

