#pragma once

#include "enemy_module.hpp"
#include "Mods/Network/net_protocol.hpp"

namespace fruityprime::gameplay { struct EnemyState; }

namespace fruityprime::enemy::module_27_gorea_leg {

inline constexpr ModuleDescriptor kModule{
    27, "27_GoreaLeg.cs", "Enemy27Entity",
    PortStatus::PartialController, "update_gorea_leg"};

// Enemy27Entity.EnemyInitialize.
void EnemyInitialize(gameplay::EnemyState& agent) noexcept;

// Enemy27Entity.SetKneeNode: which of Gorea's knees this leg is, which
// decides both the model node it follows and which way its hurt cylinder
// points -- the middle leg is mirrored against the other two.
void SetKneeNode(gameplay::EnemyState& agent, std::uint8_t index,
                 net::Vec3 knee_position) noexcept;

// Enemy27Entity.EnemyProcess: the leg is placed at its knee, and its
// hurt cylinder is laid along the knee's own sideways axis.
void EnemyProcess(gameplay::EnemyState& agent, net::Vec3 knee_position,
                  net::Vec3 knee_right) noexcept;

// Enemy27Entity.CheckPlayerCollision: walking into a leg shoves the
// player away from it horizontally and costs them ten.  The shove is
// level however the leg is leaning.
struct LegContact {
    bool hit = false;
    int damage = 0;
    net::Vec3 push{};
};
[[nodiscard]] LegContact CheckPlayerCollision(
    const gameplay::EnemyState& agent, net::Vec3 player_position,
    bool touching, float factor, int damage) noexcept;

// Enemy27Entity.EnemyTakeDamage: a leg cannot be hurt at all.  Returns
// true, meaning unaffected.
[[nodiscard]] bool EnemyTakeDamage(gameplay::EnemyState& agent) noexcept;

} // namespace fruityprime::enemy::module_27_gorea_leg
