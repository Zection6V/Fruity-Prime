#pragma once

#include "enemy_module.hpp"
#include "Mods/Network/net_protocol.hpp"

namespace fruityprime::gameplay { struct EnemyState; }

namespace fruityprime::enemy::module_26_gorea_arm {

inline constexpr ModuleDescriptor kModule{
    26, "26_GoreaArm.cs", "Enemy26Entity",
    PortStatus::PartialController, "update_gorea_arm"};

// Enemy26Entity.UpdateWeapon: Gorea hands an arm a weapon, and which
// cooldown applies depends on which arm it is -- the left one fires
// single shots and the right one holds the trigger down.
void UpdateWeapon(gameplay::EnemyState& agent, std::uint16_t shot_cooldown,
                  std::uint16_t autofire_cooldown) noexcept;

// Enemy26Entity.GetNodeVectors and GetElbowNodeVectors: an arm's aim is
// read off a node of Gorea's own model, and the two arms read different
// rows of it -- one is mirrored, so what is "up" for one is sideways for
// the other.
struct NodeVectors {
    net::Vec3 position;
    net::Vec3 up;
    net::Vec3 facing;
};
[[nodiscard]] NodeVectors GetNodeVectors(const gameplay::EnemyState& agent,
                                         net::Vec3 node_position,
                                         net::Vec3 node_row0,
                                         net::Vec3 node_row1,
                                         net::Vec3 node_row2) noexcept;

// Enemy26Entity.SpawnShotEffect and StopShotEffect: the beam an arm is
// holding, which follows the elbow rather than the shoulder.
void SpawnShotEffect(gameplay::EnemyState& agent,
                     std::uint32_t effect_id) noexcept;
void StopShotEffect(gameplay::EnemyState& agent) noexcept;

// Enemy26Entity.EnemyTakeDamage.  An arm counts damage rather than
// losing energy, and at a hundred and twenty it comes off for good.
struct ArmHit {
    // A new multiple of ten within the arm's life, which is when it
    // sparks and cries out.  A hundred and twenty is the arm coming off
    // and is deliberately not one of them.
    bool milestone = false;
    bool destroyed = false;
};
[[nodiscard]] ArmHit EnemyTakeDamage(gameplay::EnemyState& agent,
                                     bool regenerating,
                                     bool imperialist_against_shock) noexcept;

} // namespace fruityprime::enemy::module_26_gorea_arm
