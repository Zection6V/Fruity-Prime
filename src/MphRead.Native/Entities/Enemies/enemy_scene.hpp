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

#include "Entities/scene.hpp"
#include "Formats/collision_query.hpp"
#include "Mods/Network/net_protocol.hpp"

#include <cstdint>
#include <functional>
#include <span>

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

    // CollisionDetection.CheckInRadius.  The room's collision belongs to
    // the session, so the query crosses the seam and the results come
    // back; an enemy that walked the room parts itself would be reading
    // session state rather than asking it something.
    //
    // Returns how many results were written.
    std::function<std::size_t(net::Vec3 position, float radius,
                              std::span<collision::Result> results)>
        CheckInRadius;

    // EnemyInstanceEntity.HandleBlockingCollision, applied to this enemy.
    // It moves the enemy out of whatever it is inside and reports what it
    // was: the body of it lives in EnemyInstanceEntity.cpp, where the
    // managed method does, and this is how an enemy reaches it.
    struct Blocking {
        bool any = false;
        bool with_ground = false;
        bool with_wall = false;
    };
    std::function<Blocking(EnemyState& agent,
                           const scene::EntityVolume& volume,
                           bool update_speed)> BlockingCollision;

    // ItemInstanceEntity, created where this enemy died.  The despawn is
    // in frames because that is what the enemy authored, and the item
    // list belongs to the session rather than to the enemy.
    std::function<void(net::Vec3 position, std::uint8_t item_type,
                       std::uint32_t despawn_frames)> DropItem;

    // Scene.SpawnEffectGetEntry.  Unlike SpawnEffect this one hands back
    // a handle, because the enemy that asked for it intends to move it
    // about -- a fireball held in a hand is not where it was created.
    std::function<std::uint32_t(std::uint32_t effect, net::Vec3 position,
                                net::Vec3 direction,
                                std::uint32_t owner_id)> SpawnPersistentEffect;

    // EffectEntry.Transform: put one of those somewhere else.
    std::function<void(std::uint32_t effect, net::Vec3 position,
                       net::Vec3 direction)> MoveEffect;

    // Enemy50Entity, the linked hit zone.  Several enemies are not
    // shootable in their own right -- what a shot lands on is a volume
    // parented to them -- so becoming vulnerable means turning that on.
    std::function<void(const EnemyState& owner, bool collidable)> SetHitZone;
};


// EnemyInstanceEntity.HandleBlockingCollision.  Defined in
// Entities/EnemyInstanceEntity.cpp, where the managed method lives; it is
// declared here because EnemyScene.BlockingCollision is what hands it to
// an enemy, and the two belong together.
[[nodiscard]] EnemyScene::Blocking HandleBlockingCollision(
    EnemyState& agent, net::Vec3 prev_position,
    const scene::EntityVolume& volume, bool update_speed,
    const std::function<std::size_t(net::Vec3, float,
                                    std::span<collision::Result>)>&
        check_in_radius,
    const std::function<std::size_t(net::Vec3, net::Vec3, float,
                                    std::span<collision::Result>)>&
        sphere_between_points);

// EnemyInstanceEntity.SeekTargetFacing, on a gameplay enemy: turn one
// step of `angle` degrees about the up axis towards `target`, and say
// whether the turn is finished.  It gives up after `steps` and snaps,
// which is what stops an enemy circling a heading it cannot quite reach.
[[nodiscard]] bool SeekTargetFacing(EnemyState& agent, net::Vec3 target,
                                    std::uint16_t& steps, float angle);

} // namespace fruityprime::gameplay
