#pragma once

// Native counterpart of the alt-attack contact tests in
// src/MphRead/Entities/Players/PlayerCollision.cs.
//
// Four hunters have an alt-form attack that damages by touching, and each one
// is a different shape:
//
//   * Spire's is two rocks orbiting him, each a half-unit sphere, and either
//     one landing is a hit;
//   * Noxus's is a horizontal disc, so it hits anything within 1.8 units
//     sideways that is also within the target's own radius vertically -- an
//     enemy directly above him is missed however close it is;
//   * Trace's and Weavel's are plain sphere overlaps against the player's own
//     volume.
//
// The two managed entry points split them by *when* they are tested rather
// than by hunter, which is why Spire and Noxus share one and Trace and Weavel
// the other.

#include "Formats/enum_tables.hpp"
#include "Formats/formats_layouts.hpp"
#include "Formats/Types.hpp"

#include <cstdint>

namespace fruityprime::players {

// Public value type declared by PlayerCollision.cs. Contact callers set
// TakeDamage independently from Damage, including a true zero-damage hit.
struct DamageResult {
    bool TakeDamage = false;
    std::uint32_t Damage = 0;
};

// What the alt-attack tests read about the attacker.
struct AltAttackAttacker {
    formats::Hunter hunter = formats::Hunter::Samus;
    // PlayerFlags2.AltAttack
    bool alt_attack = false;
    // The player's own collision volume, which Trace and Weavel hit with.
    formats::CollisionVolume volume{};
    // Spire's two orbiting rocks.
    formats::Vector3 spire_rock_left{};
    formats::Vector3 spire_rock_right{};
    // How long Noxus's attack has been going, in doubled frames, against
    // Values.AltAttackStartup * 2.
    float alt_attack_time = 0.0F;
    float alt_attack_startup = 0.0F;
};

// What they read about the target.
struct AltAttackTarget {
    formats::CollisionVolume hurt_volume{};
    // EnemyFlags.Invincible | EnemyFlags.NoBombDamage: either one makes the
    // enemy untouchable by an alt attack.
    bool invincible = false;
    bool no_bomb_damage = false;
};

// Which sound a hit plays, and whether the attack ends on contact.  Spire's
// rocks keep spinning; everyone else's attack is spent.
enum class AltAttackHit : std::uint8_t {
    None,
    SpireRock,
    NoxusDisc,
    TraceSpin,
    WeavelSpin,
};

// PlayerCollision.CheckAltAttackHitEnemy1: Spire's rocks and Noxus's disc.
[[nodiscard]] AltAttackHit check_alt_attack_hit_enemy1(
    const AltAttackAttacker& attacker, const AltAttackTarget& target) noexcept;

// PlayerCollision.CheckAltAttackHitEnemy2: Trace's and Weavel's spins.
[[nodiscard]] AltAttackHit check_alt_attack_hit_enemy2(
    const AltAttackAttacker& attacker, const AltAttackTarget& target) noexcept;

// True for the hits that end the attack; Spire's rocks are the exception.
[[nodiscard]] constexpr bool alt_attack_ends_on_hit(AltAttackHit hit) noexcept {
    return hit == AltAttackHit::NoxusDisc || hit == AltAttackHit::TraceSpin
        || hit == AltAttackHit::WeavelSpin;
}

// Effect 235, "noxHit", spawned at the target when Noxus's disc lands.
inline constexpr int NoxusHitEffectId = 235;

} // namespace fruityprime::players

namespace MphReadNative::Entities {
using DamageResult = ::fruityprime::players::DamageResult;
}
