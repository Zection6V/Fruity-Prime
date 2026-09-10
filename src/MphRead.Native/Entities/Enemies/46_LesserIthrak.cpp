#include "enemy_decode_common.hpp"
// Native port of src/MphRead/Entities/Enemies/46_LesserIthrak.cs.
// This member definition lives in its enemy module; Session only dispatches it.
#include <cmath>
#include <limits>
#include "Utility/rng.hpp"
#include "Metadata/enemy_subroutines.hpp"
#include "enemy_scene.hpp"
#include "gorea_common.hpp"
#include "46_LesserIthrak.hpp"
#include "47_GreaterIthrak.hpp"
#include "enemy_common.hpp"

namespace fruityprime::gameplay {

namespace {

constexpr float Pi46 = 3.14159265358979323846F;
// Enemy46Entity's own two constants.
constexpr float IthrakStepDistance = 0.5F / 2.0F;  // todo: FPS stuff
constexpr float IthrakAccelSteps = 10.0F * 2.0F;   // todo: FPS stuff
constexpr float IthrakWalkSpeed = 0.15F / 2.0F;    // todo: FPS stuff

}  // namespace

Enemy46Entity::Enemy46Entity(const EnemyScene& scene, EnemyState& agent,
                             net::PlayerState* main) noexcept
    : scene_(scene), agent_(agent), main_(main) {}

void Enemy46Entity::EnemyInitialize() {
    // The Lesser Ithrak is half-resistant to everything, which is 0x5555
    // read two bits at a time.
    Setup(0x5555u);
}

void Enemy46Entity::Setup(const std::uint32_t effectiveness) {
    agent_.health = agent_.health_max = 85;
    agent_.visible = true;
    agent_.body_radius = 0.5F;
    agent_.up = {0.0F, 1.0F, 0.0F};
    agent_.ithrak_effectiveness = effectiveness;
    SetNodeAnim(5);
    agent_.ithrak_delay_timer = 30u * 2u;   // todo: FPS stuff
    agent_.ithrak_move_start = agent_.position;
    agent_.ithrak_move_timer = 600u * 2u;   // todo: FPS stuff
    agent_.state = agent_.next_state = agent_.sub_id = 0;
    SpawnHitZone();
}

void Enemy46Entity::SetNodeAnim(const std::uint8_t id, const bool no_loop) {
    agent_.ithrak_animation = id;
    agent_.ithrak_animation_frame = 0;
    agent_.ithrak_animation_length = gorea::authored_animation_length(
        agent_.enemy_type, id, 1);
    agent_.ithrak_animation_ended = false;
    agent_.ithrak_animation_no_loop = no_loop;
}

bool Enemy46Entity::AnimEnded() const noexcept {
    return agent_.ithrak_animation_ended;
}

// The cursor runs at half this head's rate.  A looping animation reports
// Ended on the frame it wraps, which is what the states that wait on one
// are actually waiting for.
void Enemy46Entity::AdvanceAnimation() {
    if (agent_.ithrak_animation_length == 0 || frame_count_ == 0
        || frame_count_ % 2 != 0) {  // todo: FPS stuff
        return;
    }
    if (agent_.ithrak_animation_frame + 1
            >= agent_.ithrak_animation_length) {
        agent_.ithrak_animation_ended = true;
        if (!agent_.ithrak_animation_no_loop) {
            agent_.ithrak_animation_frame = 0;
        }
        return;
    }
    ++agent_.ithrak_animation_frame;
}

// Enemy50Entity, a sphere just in front of the Ithrak's head.  That is
// what a shot lands on: its own body is not collidable with beams at all,
// so hitting an Ithrak means hitting its face.
void Enemy46Entity::SpawnHitZone() const {
    if (scene_.SpawnHitZone) {
        scene_.SpawnHitZone(agent_, {0.0F, 0.93F, -0.8F}, 0.6F,
                            agent_.health);
    }
}

bool Enemy46Entity::EnemyTakeDamage() {
    if (agent_.health == 0 && scene_.SetHitZone) {
        scene_.SetHitZone(agent_, false);
    }
    return false;
}

net::Vec3 Enemy46Entity::FlatToPlayer() const {
    if (main_ == nullptr) {
        return agent_.facing;
    }
    const net::Vec3 flat{main_->position.x - agent_.position.x, 0.0F,
                         main_->position.z - agent_.position.z};
    return normalized_or(flat, agent_.facing);
}

// The ten-frame turn every "notice something" behaviour starts.  The step
// is the whole angle divided by the count, so a turn right round takes
// the same ten frames as a turn of a degree -- which is why an Ithrak
// spins on the spot rather than sweeping round.
void Enemy46Entity::BeginTurn(const net::Vec3 target) {
    agent_.ithrak_target_vec = target;
    const float cosine = std::clamp(
        agent_.facing.x * target.x + agent_.facing.y * target.y
            + agent_.facing.z * target.z, -1.0F, 1.0F);
    const float angle = std::acos(cosine) * 180.0F / Pi46;
    agent_.ithrak_step_count = 10u * 2u;  // todo: FPS stuff
    agent_.ithrak_aim_angle_step =
        angle / static_cast<float>(agent_.ithrak_step_count);
}

bool Enemy46Entity::SeekTargetFacing() {
    if (!scene_.SeekFacing) {
        return true;
    }
    auto steps = static_cast<std::uint16_t>(
        std::min<std::uint32_t>(agent_.ithrak_step_count, 0xffffu));
    const bool done = scene_.SeekFacing(agent_, agent_.ithrak_target_vec,
                                        steps, agent_.ithrak_aim_angle_step);
    agent_.ithrak_step_count = steps;
    return done;
}

void Enemy46Entity::StopHorizontal() {
    agent_.velocity.x = 0.0F;
    agent_.velocity.z = 0.0F;
}

void Enemy46Entity::EnemyProcess() {
    AdvanceAnimation();
    if (agent_.state == 9) {
        // The recoil hop.  It coasts to a stop rather than being cut off,
        // which is what makes the hop read as a hop.
        if (length_squared(agent_.velocity) <= 50.0F / 4096.0F / 2.0F) {
            agent_.velocity = {};
        } else {
            agent_.velocity.x -= agent_.ithrak_acceleration.x / 2.0F;
            agent_.velocity.z -= agent_.ithrak_acceleration.z / 2.0F;
        }
    } else if (agent_.state == 3 || agent_.state == 4 || agent_.state == 5
               || agent_.state == 6 || agent_.state == 8) {
        // The five states in which it tracks you continuously rather than
        // turning once and committing.
        agent_.facing = FlatToPlayer();
        agent_.up = {0.0F, 1.0F, 0.0F};
    }
    // States nought to two are up on the ceiling: no gravity, no
    // collision, no contact damage.
    if (agent_.state != 0 && agent_.state != 1 && agent_.state != 2) {
        if (agent_.state != 7 && agent_.state != 19
            && scene_.BlockingCollision) {
            const EnemyScene::Blocking blocking = scene_.BlockingCollision(
                agent_, agent_.ithrak.hurt_volume, true);
            agent_.ithrak_ground_collision = blocking.with_ground;
            agent_.ithrak_wall_collision =
                agent_.ithrak_wall_collision || blocking.with_wall;
        }
        if (agent_.state != 7 && agent_.state != 8) {
            ContactDamagePlayer(15, true);
        }
    }
    CallStateProcess();
    if (agent_.state != 0 && agent_.state != 1 && agent_.state != 2
        && !agent_.ithrak_ground_collision) {
        agent_.velocity.y -= 100.0F / 4096.0F / 4.0F;  // todo: FPS stuff
    }
}

// Where it is going next: a point inside the volume named, twenty units
// below itself, dropped onto whatever is underneath.  Twenty is not a
// distance -- it is "far enough down that the ray finds the floor".
void Enemy46Entity::PickMoveTarget(const scene::EntityVolume& volume) {
    agent_.ithrak_move_start = agent_.position;
    net::Vec3 move_target{};
    if (volume.kind == scene::VolumeKind::Cylinder
        || volume.kind == scene::VolumeKind::Sphere) {
        const bool cylinder = volume.kind == scene::VolumeKind::Cylinder;
        const float radius = cylinder ? volume.cylinder_radius
                                      : volume.sphere_radius;
        const auto centre = cylinder ? volume.cylinder_position
                                     : volume.sphere_position;
        const auto limit = static_cast<std::uint32_t>(std::max(
            1.0F, radius * 4096.0F));
        const float dist =
            static_cast<float>(utility::get_random_int2(limit)) / 4096.0F;
        agent_.ithrak_drop_angle_sign =
            static_cast<std::int8_t>(-agent_.ithrak_drop_angle_sign);
        const float angle =
            static_cast<float>(utility::get_random_int2(0xB4000u)) / 4096.0F
            * static_cast<float>(agent_.ithrak_drop_angle_sign)
            * Pi46 / 180.0F;
        move_target = {centre.x + dist * std::cos(angle),
                       agent_.position.y - 20.0F,
                       centre.z - dist * std::sin(angle)};
    } else {
        const auto limit_x = static_cast<std::uint32_t>(std::max(
            1.0F, volume.box_dot1 * 4096.0F));
        const auto limit_z = static_cast<std::uint32_t>(std::max(
            1.0F, volume.box_dot3 * 4096.0F));
        const float dist_x =
            static_cast<float>(utility::get_random_int2(limit_x)) / 4096.0F;
        const float dist_z =
            static_cast<float>(utility::get_random_int2(limit_z)) / 4096.0F;
        move_target = {
            volume.box_vector1.x * dist_x + volume.box_vector3.x * dist_z
                + volume.box_position.x,
            agent_.position.y - 20.0F,
            volume.box_vector1.z * dist_x + volume.box_vector3.z * dist_z
                + volume.box_position.z};
    }
    if (scene_.GroundBelow) {
        float ground_y = move_target.y;
        if (scene_.GroundBelow(agent_.position, move_target, ground_y)) {
            move_target.y = ground_y;
        }
    }
    agent_.ithrak_move_target = move_target;
    const float dx = move_target.x - agent_.position.x;
    const float dz = move_target.z - agent_.position.z;
    agent_.ithrak_move_distance_squared = dx * dx + dz * dz;
}

// Enemy46Entity's own collision, which is not the blocking one.  It reads
// every face within the body radius half a unit above the point given,
// pushes out of each, and answers whether it touched anything at all --
// which is how the drop knows it has landed.
bool Enemy46Entity::HandleCollision(net::Vec3 testPos) {
    agent_.ithrak_ground_collision = false;
    if (!scene_.CheckInRadius) {
        return false;
    }
    testPos.y += 0.5F;
    std::array<collision::Result, 30> results{};
    const std::size_t count = scene_.CheckInRadius(
        testPos, agent_.body_radius, results);
    if (count == 0) {
        return false;
    }
    for (std::size_t index = 0; index < count && index < results.size();
         ++index) {
        const collision::Result& result = results[index];
        const net::Vec3 normal{result.plane.x, result.plane.y,
                               result.plane.z};
        const float depth = result.field0 != 0
            ? agent_.body_radius - result.field14
            : agent_.body_radius + result.plane.w
                - (testPos.x * normal.x + testPos.y * normal.y
                   + testPos.z * normal.z);
        if (depth <= 0.0F) {
            continue;
        }
        agent_.position = {agent_.position.x + normal.x * depth,
                           agent_.position.y + normal.y * depth,
                           agent_.position.z + normal.z * depth};
        if (result.plane.y >= 0.1F || result.plane.y <= -0.1F) {
            agent_.ithrak_ground_collision = true;
        } else {
            agent_.ithrak_wall_collision = true;
        }
        const float along = agent_.velocity.x * normal.x
            + agent_.velocity.y * normal.y + agent_.velocity.z * normal.z;
        if (along < 0.0F) {
            agent_.velocity = {agent_.velocity.x + normal.x * -along,
                               agent_.velocity.y + normal.y * -along,
                               agent_.velocity.z + normal.z * -along};
        }
    }
    return true;
}

// The hop backwards, at an angle rolled between thirty and sixty degrees
// off straight back -- so it never quite goes where you expect and you
// cannot simply follow it.
void Enemy46Entity::StartRecoil() {
    constexpr float Factor = 500.0F / 4096.0F;
    const net::Vec3 facing = agent_.facing;
    agent_.ithrak_recoil_angle_sign =
        static_cast<std::int8_t>(-agent_.ithrak_recoil_angle_sign);
    const float degrees =
        (static_cast<float>(utility::get_random_int2(0x1E000u)) / 4096.0F
         + 30.0F) * static_cast<float>(agent_.ithrak_recoil_angle_sign);
    const float angle = degrees * Pi46 / 180.0F;
    const float x = -facing.x * Factor;
    const float z = -facing.z * Factor;
    agent_.velocity = {(x * std::cos(angle) + z * std::sin(angle)) / 2.0F,
                       1000.0F / 4096.0F / 2.0F,
                       (-x * std::sin(angle) + z * std::cos(angle)) / 2.0F};
}

void Enemy46Entity::ContactDamagePlayer(const std::uint32_t damage,
                                        const bool knockback) const {
    if (main_ == nullptr || !scene_.ContactDamage) {
        return;
    }
    const float reach = agent_.body_radius + 0.45F;
    if (distance_squared(agent_.position, main_->position) > reach * reach) {
        return;
    }
    if (knockback) {
        const float dx = main_->position.x - agent_.position.x;
        const float dy = main_->position.y - agent_.position.y;
        const float dz = main_->position.z - agent_.position.z;
        const float mag = std::sqrt(dx * dx + dy * dy + dz * dz) * 5.0F;
        if (mag > 0.0F) {
            main_->speed.x += dx / mag;
            main_->speed.z += dz / mag;
        }
    }
    scene_.ContactDamage(agent_, main_->slot_index, damage);
}

bool Enemy46Entity::CallSubroutine() {
    return metadata::call_subroutine(
        metadata::Enemy46Subroutines, agent_.sub_id, agent_.next_state,
        [this](std::uint8_t index) { return RunBehavior(index); });
}

void Enemy46Entity::UpdateMouthMaterial() {
    agent_.ithrak_mouth_brightness = 31;
}

// ---- the states ---------------------------------------------------------
// Most of them do nothing but stop and run the subroutine: what an Ithrak
// is doing at any moment is in the animation and in which behaviours the
// table lets it consider, not in the state bodies.

void Enemy46Entity::State00() { static_cast<void>(CallSubroutine()); }
void Enemy46Entity::State01() { static_cast<void>(CallSubroutine()); }
void Enemy46Entity::State02() { static_cast<void>(CallSubroutine()); }

void Enemy46Entity::State03() {
    agent_.velocity.x = agent_.facing.x * IthrakWalkSpeed;
    agent_.velocity.z = agent_.facing.z * IthrakWalkSpeed;
    static_cast<void>(CallSubroutine());
}

void Enemy46Entity::State04() { static_cast<void>(CallSubroutine()); }

void Enemy46Entity::State05() {
    StopHorizontal();
    static_cast<void>(CallSubroutine());
}

void Enemy46Entity::State06() {
    StopHorizontal();
    static_cast<void>(CallSubroutine());
}

void Enemy46Entity::State07() { static_cast<void>(CallSubroutine()); }

// The bite.  Fifteen frames of wind-up, then ten damage, then thirty
// before it can bite again -- so standing in front of one costs a bite
// every half-second and no more.
void Enemy46Entity::State08() {
    StopHorizontal();
    if (agent_.ithrak_delay_timer == 15u * 2u) {  // todo: FPS stuff
        SetNodeAnim(2, true);
    } else if (agent_.ithrak_delay_timer == 0) {
        if (main_ != nullptr && scene_.ContactDamage) {
            scene_.ContactDamage(agent_, main_->slot_index, 10);
        }
        agent_.ithrak_delay_timer = 30u * 2u;  // todo: FPS stuff
    }
    if (agent_.ithrak_delay_timer > 0) {
        --agent_.ithrak_delay_timer;
    }
    static_cast<void>(CallSubroutine());
}

void Enemy46Entity::State09() { static_cast<void>(CallSubroutine()); }

void Enemy46Entity::State10() {
    StopHorizontal();
    static_cast<void>(CallSubroutine());
}

void Enemy46Entity::State11() {
    StopHorizontal();
    static_cast<void>(CallSubroutine());
}

void Enemy46Entity::State12() {
    StopHorizontal();
    static_cast<void>(CallSubroutine());
}

void Enemy46Entity::State13() {
    StopHorizontal();
    static_cast<void>(CallSubroutine());
}

void Enemy46Entity::State14() { static_cast<void>(CallSubroutine()); }

void Enemy46Entity::State15() {
    StopHorizontal();
    static_cast<void>(CallSubroutine());
}

void Enemy46Entity::State16() {
    StopHorizontal();
    static_cast<void>(CallSubroutine());
}

void Enemy46Entity::State17() {
    StopHorizontal();
    static_cast<void>(CallSubroutine());
}

void Enemy46Entity::State18() {
    StopHorizontal();
    static_cast<void>(CallSubroutine());
}

void Enemy46Entity::State19() { static_cast<void>(CallSubroutine()); }

// ---- the behaviours -----------------------------------------------------

// Landed: turn towards somewhere inside its home volume and set off.
bool Enemy46Entity::Behavior00() {
    if (!AnimEnded()) {
        return false;
    }
    SetNodeAnim(16);
    PickMoveTarget(agent_.ithrak.home_volume);
    const net::Vec3 flat{
        agent_.ithrak_move_target.x - agent_.position.x, 0.0F,
        agent_.ithrak_move_target.z - agent_.position.z};
    BeginTurn(normalized_or(flat, agent_.facing));
    return true;
}

bool Enemy46Entity::Behavior01() {
    if (agent_.ithrak_step_count > 0) {
        --agent_.ithrak_step_count;
        return false;
    }
    SetNodeAnim(13);
    const net::Vec3 facing = FlatToPlayer();
    agent_.facing = facing;
    agent_.up = {0.0F, 1.0F, 0.0F};
    agent_.velocity = {facing.x * IthrakWalkSpeed, 0.0F,
                       facing.z * IthrakWalkSpeed};
    agent_.ithrak_step_count = 3u * 2u;  // todo: FPS stuff
    return true;
}

// Somebody came inside the volume it watches from the ceiling.  This is
// the only thing that wakes an Ithrak up.
bool Enemy46Entity::Behavior02() {
    if (main_ == nullptr
        || !agent_.ithrak.warn_volume.contains(
            to_volume_point(main_->position))) {
        return false;
    }
    SetNodeAnim(6);
    return true;
}

bool Enemy46Entity::Behavior03() {
    if (!SeekTargetFacing()) {
        return false;
    }
    SetNodeAnim(13);
    agent_.velocity = {agent_.facing.x * IthrakWalkSpeed, 0.0F,
                       agent_.facing.z * IthrakWalkSpeed};
    return true;
}

bool Enemy46Entity::Behavior04() {
    if (!SeekTargetFacing()) {
        return false;
    }
    SetNodeAnim(13);
    agent_.velocity = {agent_.facing.x * IthrakWalkSpeed, 0.0F,
                       agent_.facing.z * IthrakWalkSpeed};
    agent_.ithrak_step_count = 50u * 2u;  // todo: FPS stuff
    return true;
}

// The one behaviour that means two different things depending on which
// state asked it.  From state two it is landing after the drop; from
// state seven it is the lunge finding ground to push off.
bool Enemy46Entity::Behavior05() {
    if (agent_.ithrak_step_count > 0) {
        --agent_.ithrak_step_count;
        return false;
    }
    if (agent_.state == 2) {
        if (HandleCollision(agent_.position)) {
            SetNodeAnim(14, true);
            agent_.ithrak_move_target.y = agent_.position.y;
            StopHorizontal();
            agent_.ithrak_step_count = 60u * 2u;  // todo: FPS stuff
            return true;
        }
    } else if (agent_.state == 7) {
        const net::Vec3 facing = agent_.facing;
        const net::Vec3 dest{agent_.position.x + facing.x * 2.0F,
                             agent_.position.y + facing.y * 2.0F,
                             agent_.position.z + facing.z * 2.0F};
        if (HandleCollision(agent_.position) || HandleCollision(dest)) {
            SetNodeAnim(10, true);
            agent_.velocity = {facing.x * 0.18F / 2.0F, 0.0F,
                               facing.z * 0.18F / 2.0F};
            agent_.ithrak_acceleration = {
                agent_.velocity.x / IthrakAccelSteps,
                agent_.velocity.y / IthrakAccelSteps,
                agent_.velocity.z / IthrakAccelSteps};
            return true;
        }
    }
    return false;
}

bool Enemy46Entity::Behavior06() {
    if (!AnimEnded()) {
        return false;
    }
    SetNodeAnim(16);
    BeginTurn(FlatToPlayer());
    return true;
}

bool Enemy46Entity::Behavior07() {
    if (!HandleCollision(agent_.position)) {
        return false;
    }
    SetNodeAnim(13);
    agent_.velocity = {agent_.facing.x * IthrakWalkSpeed, 0.0F,
                       agent_.facing.z * IthrakWalkSpeed};
    return true;
}

bool Enemy46Entity::Behavior08() {
    if (!SeekTargetFacing()) {
        return false;
    }
    SetNodeAnim(14, true);
    return true;
}

bool Enemy46Entity::Behavior09() {
    if (!AnimEnded()) {
        return false;
    }
    SetNodeAnim(4, true);
    agent_.ithrak_wall_collision = false;
    return true;
}

bool Enemy46Entity::Behavior10() {
    if (!SeekTargetFacing()) {
        return false;
    }
    SetNodeAnim(14, true);
    agent_.ithrak_step_count = 50u * 2u;  // todo: FPS stuff
    return true;
}

// The lunge connecting.  The player's own sphere is grown by a quarter
// unit in every direction first, so a lunge that looks like a near miss
// still lands -- which is what makes an Ithrak's charge worth avoiding
// rather than sidestepping late.
bool Enemy46Entity::Behavior11() {
    if (main_ == nullptr) {
        return false;
    }
    constexpr float Grow = 1000.0F / 4096.0F;
    const net::Vec3 centre{main_->position.x, main_->position.y + Grow,
                           main_->position.z};
    const float reach = agent_.body_radius + 0.45F + Grow;
    if (distance_squared(agent_.position, centre) > reach * reach) {
        return false;
    }
    StopHorizontal();
    SetNodeAnim(9, true);
    if (scene_.ContactDamage) {
        scene_.ContactDamage(agent_, main_->slot_index, 15);
    }
    return true;
}

bool Enemy46Entity::Behavior12() {
    if (!AnimEnded()) {
        return false;
    }
    agent_.ithrak_step_count = 50u * 2u;  // todo: FPS stuff
    agent_.velocity = {agent_.facing.x * IthrakWalkSpeed, 0.0F,
                       agent_.facing.z * IthrakWalkSpeed};
    SetNodeAnim(13);
    return true;
}

// Both of them have to be inside the range volume: an Ithrak will not
// follow you out of its own room.
bool Enemy46Entity::Behavior13() {
    if (main_ == nullptr
        || !agent_.ithrak.range_volume.contains(
            to_volume_point(main_->position))
        || !agent_.ithrak.range_volume.contains(
            to_volume_point(agent_.position))) {
        return false;
    }
    SetNodeAnim(16);
    BeginTurn(FlatToPlayer());
    agent_.velocity = {};
    return true;
}

bool Enemy46Entity::Behavior14() {
    if (main_ != nullptr
        && agent_.ithrak.warn_volume.contains(
            to_volume_point(main_->position))) {
        return false;
    }
    SetNodeAnim(5);
    return true;
}

// The drop.  The step count is the number of frames the fall will take,
// worked out from the distance, so the Ithrak arrives exactly rather than
// falling until it hits something.
bool Enemy46Entity::Behavior15() {
    if (main_ == nullptr
        || !agent_.ithrak.range_volume.contains(
            to_volume_point(main_->position))) {
        return false;
    }
    PickMoveTarget(agent_.ithrak.home_volume);
    const net::Vec3 travel = subtract(agent_.ithrak_move_target,
                                      agent_.position);
    const float distance = std::sqrt(std::max(0.0F,
                                              length_squared(travel)));
    if (distance <= 0.0F) {
        return false;
    }
    agent_.ithrak_step_count = static_cast<std::uint32_t>(
        distance / IthrakStepDistance) + 1u;
    agent_.velocity = multiply(travel, IthrakStepDistance / distance);
    SetNodeAnim(12, true);
    return true;
}

// Too close.  It hops backwards rather than letting a player stand
// inside it -- but only if there is somewhere behind it to hop to.
bool Enemy46Entity::Behavior16() {
    if (main_ == nullptr
        || distance_squared(agent_.position, main_->position)
            >= 1.5F * 1.5F) {
        return false;
    }
    const net::Vec3 dest{agent_.position.x - agent_.facing.x * 2.0F,
                         agent_.position.y - agent_.facing.y * 2.0F,
                         agent_.position.z - agent_.facing.z * 2.0F};
    if (scene_.Blocked && scene_.Blocked(agent_.position, dest, 0.01F)) {
        return false;
    }
    StartRecoil();
    SetNodeAnim(7, true);
    return true;
}

// Start the lunge: forward hard and up a little, for fourteen frames.
bool Enemy46Entity::Behavior17() {
    if (agent_.ithrak_step_count > 0) {
        --agent_.ithrak_step_count;
        return false;
    }
    constexpr float Forward = 1800.0F / 4096.0F;
    constexpr float Up = 600.0F / 4096.0F;
    agent_.velocity = {agent_.facing.x * Forward / 2.0F, Up / 2.0F,
                       agent_.facing.z * Forward / 2.0F};
    agent_.ithrak_step_count = 7u * 2u;  // todo: FPS stuff
    return true;
}

// The bite range: between one and a half units and two.  Closer than that
// and it hops away instead; further and it lunges.
bool Enemy46Entity::Behavior18() {
    if (main_ == nullptr) {
        return false;
    }
    const float distance = distance_squared(agent_.position,
                                            main_->position);
    if (distance <= 1.5F * 1.5F || distance >= 2.0F * 2.0F) {
        return false;
    }
    agent_.ithrak_step_count = 0;
    agent_.velocity = {};
    agent_.ithrak_delay_timer = 15u * 2u;  // todo: FPS stuff
    return true;
}

// Arriving where it was going.  It takes two passes: the first clamps the
// last step so it lands exactly on the target, the second turns towards
// the next one.
bool Enemy46Entity::Behavior19() {
    if (agent_.ithrak_reaching_target) {
        SetNodeAnim(16);
        PickMoveTarget(agent_.ithrak.home_volume);
        agent_.velocity = {};
        const net::Vec3 flat{
            agent_.ithrak_move_target.x - agent_.position.x, 0.0F,
            agent_.ithrak_move_target.z - agent_.position.z};
        BeginTurn(normalized_or(flat, agent_.facing));
        agent_.ithrak_reaching_target = false;
        return true;
    }
    const net::Vec3 next = add(agent_.position, agent_.velocity);
    const net::Vec3 travel = subtract(agent_.ithrak_move_start, next);
    if (length_squared(travel) > agent_.ithrak_move_distance_squared) {
        agent_.velocity = subtract(next, agent_.position);
        agent_.ithrak_reaching_target = true;
    }
    return false;
}

// Ten seconds of walking without arriving anywhere is the timeout that
// stops an Ithrak grinding against geometry forever.
bool Enemy46Entity::Behavior20() {
    if (agent_.ithrak_move_timer > 0) {
        --agent_.ithrak_move_timer;
        return false;
    }
    SetNodeAnim(16);
    agent_.velocity = {};
    BeginTurn(FlatToPlayer());
    agent_.ithrak_move_timer = 600u * 2u;  // todo: FPS stuff
    return true;
}

bool Enemy46Entity::Behavior21() {
    if (agent_.ithrak_delay_timer > 0 || main_ == nullptr
        || distance_squared(agent_.position, main_->position)
            <= 2.0F * 2.0F) {
        return false;
    }
    SetNodeAnim(13);
    const net::Vec3 facing = FlatToPlayer();
    agent_.facing = facing;
    agent_.up = {0.0F, 1.0F, 0.0F};
    agent_.velocity = {facing.x * IthrakWalkSpeed, 0.0F,
                       facing.z * IthrakWalkSpeed};
    agent_.ithrak_delay_timer = 30u * 2u;  // todo: FPS stuff
    return true;
}

// A chance rather than a rule: while biting, it may break off and hop
// back.  The managed port halves the odds along with doubling the rate,
// which keeps the chance per second the same.
bool Enemy46Entity::Behavior22() {
    if (agent_.ithrak_delay_timer < 15u * 2u
        || utility::get_random_int2(0x64000u) >= 2048u / 2u) {
        return false;
    }
    StartRecoil();
    SetNodeAnim(7, true);
    return true;
}

bool Enemy46Entity::Behavior23() {
    if (agent_.ithrak_step_count > 0) {
        --agent_.ithrak_step_count;
        return false;
    }
    SetNodeAnim(8);
    agent_.ithrak_step_count = 12u * 2u;  // todo: FPS stuff
    return true;
}

// The scream: between three and a half units and five, it stops and makes
// the noise that tells you it has seen you.  Outside that band it just
// keeps walking, which is why an Ithrak that screams is one that has
// decided about you.
bool Enemy46Entity::Behavior24() {
    if (main_ == nullptr) {
        return false;
    }
    const float distance = distance_squared(agent_.position,
                                            main_->position);
    if (distance <= 3.5F * 3.5F || distance >= 5.0F * 5.0F) {
        return false;
    }
    SetNodeAnim(1);
    agent_.velocity = {};
    agent_.ithrak_step_count = 38u * 2u;  // todo: FPS stuff
    return true;
}

// Either of them leaving the range volume ends the chase.
bool Enemy46Entity::Behavior25() {
    if (main_ != nullptr
        && agent_.ithrak.range_volume.contains(
            to_volume_point(agent_.position))
        && agent_.ithrak.range_volume.contains(
            to_volume_point(main_->position))) {
        return false;
    }
    SetNodeAnim(14, true);
    agent_.velocity = {};
    return true;
}

bool Enemy46Entity::RunBehavior(const std::uint8_t index) {
    switch (index) {
    case 0: return Behavior00();
    case 1: return Behavior01();
    case 2: return Behavior02();
    case 3: return Behavior03();
    case 4: return Behavior04();
    case 5: return Behavior05();
    case 6: return Behavior06();
    case 7: return Behavior07();
    case 8: return Behavior08();
    case 9: return Behavior09();
    case 10: return Behavior10();
    case 11: return Behavior11();
    case 12: return Behavior12();
    case 13: return Behavior13();
    case 14: return Behavior14();
    case 15: return Behavior15();
    case 16: return Behavior16();
    case 17: return Behavior17();
    case 18: return Behavior18();
    case 19: return Behavior19();
    case 20: return Behavior20();
    case 21: return Behavior21();
    case 22: return Behavior22();
    case 23: return Behavior23();
    case 24: return Behavior24();
    case 25: return Behavior25();
    default: return false;
    }
}

void Enemy46Entity::CallStateProcess() {
    switch (agent_.state) {
    case 0: State00(); break;
    case 1: State01(); break;
    case 2: State02(); break;
    case 3: State03(); break;
    case 4: State04(); break;
    case 5: State05(); break;
    case 6: State06(); break;
    case 7: State07(); break;
    case 8: State08(); break;
    case 9: State09(); break;
    case 10: State10(); break;
    case 11: State11(); break;
    case 12: State12(); break;
    case 13: State13(); break;
    case 14: State14(); break;
    case 15: State15(); break;
    case 16: State16(); break;
    case 17: State17(); break;
    case 18: State18(); break;
    case 19: State19(); break;
    default: break;
    }
}

void Session::update_ithrak(EnemyState& agent) {
    if (!agent.ithrak.supported) {
        return;
    }
    net::PlayerState* main = nullptr;
    float nearest = std::numeric_limits<float>::max();
    for (auto& player : players_) {
        if (!objective_player(player)) {
            continue;
        }
        const float distance = distance_squared(player.position,
                                                agent.position);
        if (distance < nearest) {
            nearest = distance;
            main = &player;
        }
    }
    agent.target_slot = main != nullptr && agent.state > 2
        ? main->slot_index : 0xff;

    // EnemyInstanceEntity.BaseProcess.
    agent.state = agent.next_state;
    agent.sub_id = agent.state;
    const net::Vec3 prev_position = agent.position;
    agent.position = add(agent.position, agent.velocity);

    EnemyScene scene = build_ithrak_scene(prev_position);
    if (agent.enemy_type == static_cast<std::uint8_t>(
            formats::EnemyType::GreaterIthrak)) {
        Enemy47Entity entity(scene, agent, main);
        entity.set_frame_count(tick_count_);
        entity.EnemyProcess();
    } else {
        Enemy46Entity entity(scene, agent, main);
        entity.set_frame_count(tick_count_);
        entity.EnemyProcess();
    }
}

EnemyScene Session::build_ithrak_scene(const net::Vec3 prev_position) {
    EnemyScene scene = build_enemy_scene(prev_position);
    scene.SetHitZone = [this](const EnemyState& owner, bool collidable) {
        for (auto& child : enemies_) {
            if (child.parent_enemy_id != owner.id
                || child.enemy_type != static_cast<std::uint8_t>(
                    formats::EnemyType::HitZone)) {
                continue;
            }
            child.hit_zone_collidable = collidable;
            if (!collidable) {
                child.health = 0;
            }
        }
    };
    scene.GroundBelow = [this](net::Vec3 from, net::Vec3 to, float& ground_y) {
        const auto hit = collision::sweep_sphere(
            room_.collision(), collision::Vec3{from.x, from.y, from.z},
            collision::Vec3{to.x, to.y, to.z}, 0.01F, 0x2000);
        if (!hit.has_value()) {
            return false;
        }
        ground_y = hit->contact.y;
        return true;
    };
    return scene;
}

void Session::update_lesser_ithrak(EnemyState& agent) {
    update_ithrak(agent);
}

void Session::update_greater_ithrak(EnemyState& agent) {
    update_ithrak(agent);
}

} // namespace fruityprime::gameplay

static_assert(fruityprime::enemy::module_46_lesser_ithrak::kModule.managed_class.size() != 0);

namespace fruityprime::enemy::module_46_lesser_ithrak {

void EnemyInitialize(gameplay::EnemyState& agent) noexcept {
    gameplay::EnemyScene empty;
    gameplay::Enemy46Entity(empty, agent, nullptr).EnemyInitialize();
}

} // namespace fruityprime::enemy::module_46_lesser_ithrak

namespace fruityprime::enemy {

IthrakProfile decode_ithrak_profile(
    std::uint8_t id, std::span<const std::uint8_t> fields,
    net::Vec3 origin) noexcept {
    IthrakProfile result;
    const auto lesser = static_cast<std::uint8_t>(
        formats::EnemyType::LesserIthrak);
    const auto greater = static_cast<std::uint8_t>(
        formats::EnemyType::GreaterIthrak);
    if (id != lesser && id != greater) {
        return result;
    }
    // Enemy46 passes S00.Volume0, Volume2, Volume1, Volume3 to Setup
    // (hurt, home, range, warning). Enemy47 passes S05.Volume0..3 in order.
    // The two managed constructors intentionally differ; do not normalize
    // them into one sequential layout here.
    const std::size_t base = id == greater ? 4 : 0;
    if (fields.size() < base + 4 * 64) {
        return result;
    }
    result.variant = id;
    result.hurt_volume = detail::read_volume(fields, base, origin);
    const std::size_t home_index = id == lesser ? 2 : 1;
    const std::size_t range_index = id == lesser ? 1 : 2;
    result.home_volume = detail::read_volume(
        fields, base + home_index * 64, origin);
    result.range_volume = detail::read_volume(
        fields, base + range_index * 64, origin);
    result.warn_volume = detail::read_volume(fields, base + 192, origin);
    result.supported = result.hurt_volume.kind != scene::VolumeKind::Invalid
        && result.home_volume.kind != scene::VolumeKind::Invalid
        && result.range_volume.kind != scene::VolumeKind::Invalid
        && result.warn_volume.kind != scene::VolumeKind::Invalid;
    return result;
}

} // namespace fruityprime::enemy
