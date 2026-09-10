#pragma once

#include "enemy_module.hpp"
#include "Mods/Network/net_protocol.hpp"

namespace fruityprime::gameplay { struct EnemyState; }

namespace fruityprime::enemy::module_50_hit_zone {

inline constexpr ModuleDescriptor kModule{
    50, "50_HitZone.cs", "Enemy50Entity",
    PortStatus::ImplementedController, "update_hit_zone"};

// Enemy50Entity.SetUp: a hit zone is made by whatever it belongs to,
// which tells it how much of that thing's energy it stands for.
void SetUp(gameplay::EnemyState& agent, std::uint16_t health,
           float bounding_radius) noexcept;

// Enemy50Entity.EnemyProcess: a hit zone takes its owner's placing, with
// any scaling on it dropped -- a scaled hurt volume is not what the
// cartridge tests against.
void EnemyProcess(gameplay::EnemyState& agent, net::Vec3 owner_position,
                  net::Vec3 owner_facing, net::Vec3 owner_up) noexcept;

// Enemy50Entity.EnemyTakeDamage.  A hit zone passes damage on to its
// owner, with its own beam effectiveness standing in for the owner's for
// the length of that call -- which is the whole point of it: a zone can
// be weak to something its owner is not.  Returns whether the zone
// survived, which is what the managed return means.
[[nodiscard]] bool EnemyTakeDamage(gameplay::EnemyState& agent) noexcept;

// Enemy50Entity.HandleMessage: a Fire Spawn switches its zone on and off
// as it emerges and submerges, and switching it off clears whoever it had
// already touched so they can be hurt again next time.
void HandleMessage(gameplay::EnemyState& agent, bool activate) noexcept;

} // namespace fruityprime::enemy::module_50_hit_zone

