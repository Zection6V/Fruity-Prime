#pragma once

#include "enemy_module.hpp"
#include "enemy_catalog.hpp"
#include "Mods/Network/net_protocol.hpp"

namespace fruityprime::gameplay { struct EnemyState; }

namespace fruityprime::enemy::module_12_geemer {

inline constexpr ModuleDescriptor kModule{
    12, "12_Geemer.cs", "Enemy12Entity",
    PortStatus::SharedController, "update_geemer"};

// Enemy12Entity.GeemerAnim: a Geemer is folded up until somebody comes
// within a unit and a half of it, then unfolds and crawls.  Neither
// change is instant, which is what the two transition animations are --
// and it is the animation, not a flag, that the managed class asks about.
enum class GeemerAnim : std::uint8_t {
    Retract = 0,
    Extend = 1,
    WiggleRetracted = 2,
    WiggleExtended = 3
};

// Enemy12Entity.SetAnimation.
void SetAnimation(gameplay::EnemyState& agent, GeemerAnim anim) noexcept;
[[nodiscard]] GeemerAnim CurrentAnimation(
    const gameplay::EnemyState& agent) noexcept;

// Enemy12Entity.SpawnData and SpawnFields.
[[nodiscard]] const enemy::ZoomerProfile& SpawnFields(
    const gameplay::EnemyState& agent) noexcept;
[[nodiscard]] net::Vec3 SpawnData(
    const gameplay::EnemyState& agent) noexcept;

// Enemy12Entity.EnemyInitialize.
void EnemyInitialize(gameplay::EnemyState& agent) noexcept;

// Enemy12Entity.EnemyTakeDamage: a folded Geemer cannot be hurt, which is
// why it has to be caught out.  Returns whether it is unaffected.
[[nodiscard]] bool EnemyTakeDamage(
    const gameplay::EnemyState& agent) noexcept;

} // namespace fruityprime::enemy::module_12_geemer

