#pragma once

// What an enemy's own methods need from the scene around it.
//
// The managed EnemyInstanceEntity subclasses reach into Scene directly --
// _scene.SpawnEffectGetEntry, TakeDamage, HandleBlockingCollision.  Those
// live on gameplay::Session here and most of them are private to it, so
// they cross to the enemy as a small set of calls: the same seam shape the
// HUD and the renderer already use, and the reason an enemy file can hold
// the managed method names and bodies without Session having to befriend
// fifty-two classes.
//
// Everything here is a call the enemy makes outwards.  Nothing reads back
// into Session's state, because an enemy that could would be a fifty-third
// place for the rules to live.

#include "Mods/Network/net_protocol.hpp"

#include <cstdint>
#include <functional>

namespace fruityprime::gameplay {

struct EnemyState;

struct EnemyScene {
    // EnemyInstanceEntity.TakeDamage, aimed at this enemy.  Enough damage
    // to finish it is how an enemy kills itself.
    std::function<void(std::uint32_t enemy_id, std::uint32_t damage)> Damage;

    // Scene.SpawnEffectGetEntry.  The entry the managed code keeps a handle
    // to is not returned: no ported enemy needs to move or unlink one yet,
    // and handing back something that cannot be used would only invite it.
    std::function<void(std::uint32_t effect, net::Vec3 position,
                       std::uint32_t owner_id)> SpawnEffect;

    // EnemyInstanceEntity.ContactDamagePlayer's outward half: the player has
    // been touched and this is what it costs them.
    std::function<void(EnemyState& agent, std::uint8_t slot,
                       std::uint32_t damage)> ContactDamage;

    // EnemyInstanceEntity.HandleBlockingCollision, reduced to the question
    // every ported enemy actually asks: is there something in the way
    // between here and there.
    std::function<bool(net::Vec3 from, net::Vec3 to, float radius)> Blocked;

    // PlayerEntity.CameraInfo.SetShake, which the camera's own view owns
    // rather than the gameplay session.
    std::function<void(float amount)> CameraShake;

    // EnemyInstanceEntity.SeekTargetFacing, applied to this enemy: turn
    // one step towards `desired` and say whether the turn is finished.
    std::function<bool(EnemyState& agent, net::Vec3 desired,
                       std::uint16_t& steps, float angle)> SeekFacing;

    // BeamProjectileEntity.Spawn from an enemy: the weapon it fires with
    // is the enemy's own, so only where the shot starts and where it is
    // pointed cross this seam.
    std::function<void(const EnemyState& agent, net::Vec3 position,
                       net::Vec3 direction)> SpawnProjectile;

    // Whether an enemy of the named kind is overlapping this one, and if
    // so which way is away from it.  The session owns the enemy list, so
    // this is a question rather than a walk.
    //
    // The kind is a parameter rather than "its own type" because the
    // cartridge does not always ask about its own: a Petrasyl4 looks for
    // Petrasyl3s, which is the game's own quirk and not a slip here.
    std::function<bool(const EnemyState& agent, std::uint8_t kind,
                       net::Vec3& away)> NearbyKin;
};

} // namespace fruityprime::gameplay
