// Native counterpart of src/MphRead/Entities/Players/PlayerCollision.cs.
#include "Entities/gameplay.hpp"
#include "../gameplay_helpers.hpp"
#include "PlayerCollision.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace fruityprime::gameplay {

using namespace detail;

void Session::respawn_player(net::PlayerState& player,
                             RuntimeInput& runtime) {
    if (const auto* spawn = select_respawn_spawn(player); spawn != nullptr) {
        player.position = spawn->position;
        player.facing = spawn->facing;
    } else {
        player.position = spawn_position(runtime.spawn_ordinal, player.team);
        player.facing = {0.0F, 0.0F, 1.0F};
    }
    player.speed = {};
    player.health = runtime.inventory.health_max;
    player.flags = net::PlayerState::FlagActive
        | net::PlayerState::FlagSpawned;
    player.attacker_slot = 0xff;
    player.damage_beam = 0xff;
    player.damage_flags = 0;
    player.hit_direction = {};
    SoundEvent spawn_event;
    spawn_event.cue = SoundCue::PlayerSpawn;
    spawn_event.slot = player.slot_index;
    spawn_event.position = player.position;
    emit_sound(spawn_event);
}

void Session::constrain_to_world(net::PlayerState& player) {
    if (player.position.x < world_min_.x) {
        player.position.x = world_min_.x;
        player.speed.x = 0.0F;
    } else if (player.position.x > world_max_.x) {
        player.position.x = world_max_.x;
        player.speed.x = 0.0F;
    }
    if (player.position.z < world_min_.z) {
        player.position.z = world_min_.z;
        player.speed.z = 0.0F;
    } else if (player.position.z > world_max_.z) {
        player.position.z = world_max_.z;
        player.speed.z = 0.0F;
    }
    if (player.position.y < world_min_.y) {
        player.position.y = world_min_.y;
        player.speed.y = 0.0F;
    }

    // PlayerCollision also treats an active ForceFieldEntity as a finite
    // two-sided plane. Resolve the player's sphere against every authored
    // field after ordinary room bounds so a room script cannot be bypassed by
    // walking through its edge.
    for (const auto& field : force_fields_) {
        if (!field.active) {
            continue;
        }
        const net::Vec3 between = subtract(player.position, field.position);
        const float plane_distance = dot(between, field.facing);
        if (std::abs(plane_distance) > config_.body_radius) {
            continue;
        }
        if (std::abs(dot(between, field.up))
                > field.height + config_.body_radius
            || std::abs(dot(between, field.right))
                > field.width + config_.body_radius) {
            continue;
        }
        const float side = plane_distance < 0.0F ? -1.0F
            : plane_distance > 0.0F ? 1.0F
            : dot(player.speed, field.facing) >= 0.0F ? -1.0F : 1.0F;
        const float desired_distance = side * config_.body_radius;
        player.position = add(
            player.position,
            multiply(field.facing, desired_distance - plane_distance));
        const float normal_speed = dot(player.speed, field.facing);
        if (normal_speed * side < 0.0F) {
            player.speed = subtract(player.speed,
                                    multiply(field.facing, normal_speed));
        }
    }
}

} // namespace fruityprime::gameplay

namespace fruityprime::players {

namespace {

[[nodiscard]] bool untouchable(const AltAttackTarget& target) noexcept {
    return target.invincible || target.no_bomb_damage;
}

// CollisionDetection.CheckSphereOverlapVolume. Hurt volumes are not always
// spheres, so preserve all three managed branches rather than reading the
// inactive Sphere fields from a cylinder or box.
[[nodiscard]] bool sphere_overlaps(const formats::CollisionVolume& volume,
                                   formats::Vector3 point,
                                   float radius) noexcept {
    if (volume.Type == formats::VolumeType::Cylinder) {
        formats::Vector3 between = point - volume.CylinderPosition;
        const float axial = formats::dot(volume.CylinderVector, between);
        if (axial < -radius || axial > volume.CylinderDot + radius) {
            return false;
        }
        between = between - volume.CylinderVector * axial;
        const float radii = radius + volume.CylinderRadius;
        return formats::dot(between, between) <= radii * radii;
    }
    if (volume.Type == formats::VolumeType::Sphere) {
        const formats::Vector3 between = volume.SpherePosition - point;
        const float radii = volume.SphereRadius + radius;
        return formats::dot(between, between) <= radii * radii;
    }
    const formats::Vector3 between = point - volume.BoxPosition;
    const float dot1 = formats::dot(volume.BoxVector1, between);
    if (dot1 < -radius || dot1 > volume.BoxDot1 + radius) {
        return false;
    }
    const float dot2 = formats::dot(volume.BoxVector2, between);
    if (dot2 < -radius || dot2 > volume.BoxDot2 + radius) {
        return false;
    }
    const float dot3 = formats::dot(volume.BoxVector3, between);
    return dot3 >= -radius && dot3 <= volume.BoxDot3 + radius;
}

} // namespace

AltAttackHit check_alt_attack_hit_enemy1(
    const AltAttackAttacker& attacker,
    const AltAttackTarget& target) noexcept {
    if (untouchable(target)) {
        return AltAttackHit::None;
    }
    if (attacker.hunter == formats::Hunter::Spire && attacker.alt_attack) {
        // Either rock landing is a hit, and the attack keeps going: the rocks
        // are still orbiting.
        constexpr float RockRadius = 0.5F;
        if (sphere_overlaps(target.hurt_volume, attacker.spire_rock_left,
                            RockRadius)
            || sphere_overlaps(target.hurt_volume, attacker.spire_rock_right,
                               RockRadius)) {
            return AltAttackHit::SpireRock;
        }
        return AltAttackHit::None;
    }
    // The cartridge assumes the hunter is Noxus here purely from the attack
    // timer being past its startup, with no hunter check of its own.
    if (attacker.hunter == formats::Hunter::Noxus
        && attacker.alt_attack_time >= attacker.alt_attack_startup * 2.0F) {
        const formats::Vector3 between{
            target.hurt_volume.SpherePosition.x
                - attacker.volume.SpherePosition.x,
            target.hurt_volume.SpherePosition.y
                - attacker.volume.SpherePosition.y,
            target.hurt_volume.SpherePosition.z
                - attacker.volume.SpherePosition.z};
        const float radius = target.hurt_volume.SphereRadius;
        // The disc is flat: the vertical gate is the target's own radius, not
        // a combined one, so something directly overhead is missed.
        if (between.y > -radius && between.y < radius) {
            const float horizontal_squared =
                between.x * between.x + between.z * between.z;
            const float reach = radius + 1.8F;
            if (horizontal_squared < reach * reach) {
                return AltAttackHit::NoxusDisc;
            }
        }
    }
    return AltAttackHit::None;
}

AltAttackHit check_alt_attack_hit_enemy2(
    const AltAttackAttacker& attacker,
    const AltAttackTarget& target) noexcept {
    if (untouchable(target)) {
        return AltAttackHit::None;
    }
    if ((attacker.hunter != formats::Hunter::Trace
         && attacker.hunter != formats::Hunter::Weavel)
        || !attacker.alt_attack) {
        return AltAttackHit::None;
    }
    formats::Vector3 between{
        attacker.volume.SpherePosition.x - target.hurt_volume.SpherePosition.x,
        attacker.volume.SpherePosition.y - target.hurt_volume.SpherePosition.y,
        attacker.volume.SpherePosition.z
            - target.hurt_volume.SpherePosition.z};
    float distance_squared = between.x * between.x + between.y * between.y
        + between.z * between.z;
    if (distance_squared == 0.0F) {
        // Standing exactly inside the target: the managed code substitutes a
        // unit vector so the overlap still registers rather than leaving a
        // zero-length direction behind for the caller.
        between = {1.0F, 0.0F, 0.0F};
        distance_squared = 1.0F;
    }
    const float radii =
        attacker.volume.SphereRadius + target.hurt_volume.SphereRadius;
    if (distance_squared < radii * radii) {
        return attacker.hunter == formats::Hunter::Weavel
            ? AltAttackHit::WeavelSpin
            : AltAttackHit::TraceSpin;
    }
    return AltAttackHit::None;
}

} // namespace fruityprime::players
