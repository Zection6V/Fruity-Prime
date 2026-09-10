#include "enemy_decode_common.hpp"
// Native port of src/MphRead/Entities/Enemies/38_CrashPillar.cs.
// This member definition lives in its enemy module; Session only dispatches it.
#include "38_CrashPillar.hpp"
#include "enemy_common.hpp"
#include "enemy_scene.hpp"
#include "Metadata/enemy_subroutines.hpp"

#include <cmath>

namespace fruityprime::gameplay {
namespace {

// Enemy38Entity's own numbers, in the order the managed file gives them.
constexpr std::uint16_t StartingHealth = 150u;
constexpr std::uint16_t JumpFrames = 5u * 2u;    // todo: FPS stuff
constexpr std::uint16_t DelayFrames = 15u * 2u;  // todo: FPS stuff
constexpr std::uint16_t AimSteps = 8u * 2u;      // todo: FPS stuff
constexpr std::uint16_t TurnSteps = 10u * 2u;    // todo: FPS stuff
constexpr std::uint16_t LingerFrames = 40u * 2u; // todo: FPS stuff
constexpr float ChargeSpeed = 0.1465F / 2.0F;   // todo: FPS stuff
constexpr float DropSpeed = -3000.0F / 4096.0F / 2.0F;
constexpr float JumpRise = 2.8F;
constexpr std::uint32_t ContactDamageWalking = 10u;
constexpr std::uint32_t ContactDamageCharging = 40u;
// How close the player has to be for the pillar to linger before hopping.
constexpr float LingerRadius = 5.0F;
// How near its home it has to be before it turns back to its first facing.
constexpr float HomeRadius = 1.0F;
constexpr float Pi = 3.14159265358979323846F;

[[nodiscard]] float length_of(const net::Vec3 value) noexcept {
    return std::sqrt(value.x * value.x + value.y * value.y
                     + value.z * value.z);
}

// The horizontal direction from `from` to `to`, or nothing if they are on
// top of each other.
[[nodiscard]] net::Vec3 flat_direction(const net::Vec3 from,
                                       const net::Vec3 to,
                                       const net::Vec3 fallback) noexcept {
    const net::Vec3 delta{to.x - from.x, 0.0F, to.z - from.z};
    const float length = length_of(delta);
    return length > 0.0001F
        ? net::Vec3{delta.x / length, 0.0F, delta.z / length} : fallback;
}

// Native counterpart of Enemy38Entity.
//
// A crash pillar is a statue that comes alive when somebody walks in
// front of it: it turns to face them, hops across the room, lands hard
// enough to shake the camera, and when they leave it walks back to
// exactly where it started and turns to exactly the way it was facing.
// That going-home is most of the seventeen states.
//
// It is invulnerable except for a few frames early in each jump, which is
// the whole of how it is meant to be fought.
class Enemy38Entity final {
public:
    Enemy38Entity(const EnemyScene& scene, EnemyState& agent,
                  const net::PlayerState& main,
                  std::uint64_t frame_count) noexcept
        : scene_(scene), agent_(agent), main_(main),
          frame_count_(frame_count) {}

    // Enemy38Entity.EnemyProcess.
    void EnemyProcess();

private:
    // Seventeen states.  Most only run the subroutine; the handful that do
    // something first are the ones written out below.
    void State00() { CallSubroutine(); }
    void State01() { CallSubroutine(); }
    void State02() { CallSubroutine(); }
    void State03() { CallSubroutine(); }
    void State04() { DoThing(0.08F); CallSubroutine(); }
    void State05() { CallSubroutine(); }
    void State06();
    void State07();
    void State08() { State07(); }
    void State09() { CallSubroutine(); }
    void State10() { CallSubroutine(); }
    void State11() { CallSubroutine(); }
    void State12();
    void State13();
    void State14() { CallSubroutine(); }
    void State15() { CallSubroutine(); }
    void State16() { CallSubroutine(); }

    [[nodiscard]] bool Behavior00();
    [[nodiscard]] bool Behavior01();
    [[nodiscard]] bool Behavior02();
    [[nodiscard]] bool Behavior03() const;
    [[nodiscard]] bool Behavior04();
    [[nodiscard]] bool Behavior05();
    [[nodiscard]] bool Behavior06();
    [[nodiscard]] bool Behavior07();
    [[nodiscard]] bool Behavior08();
    [[nodiscard]] bool Behavior09();
    [[nodiscard]] bool Behavior10();
    [[nodiscard]] bool Behavior11();
    [[nodiscard]] bool Behavior12();
    [[nodiscard]] bool Behavior13();
    [[nodiscard]] bool Behavior14();
    [[nodiscard]] bool Behavior15();
    [[nodiscard]] bool Behavior16();
    [[nodiscard]] bool Behavior17();
    [[nodiscard]] bool Behavior18();

    void CallStateProcess();
    void CallSubroutine();

    // Enemy38Entity.SetCameraShake: harder the closer the player is, up to
    // a cap the caller chooses -- a landing across the room is a rumble
    // and one at your feet is not.
    void SetCameraShake(float shake_max) const;

    // Enemy38Entity's unnamed helper: drive forward while the charge
    // animation is running, and stop dead once it is past its own frame
    // seventeen.
    void DoThing(float shake_max);

    // Enemy38Entity.FaceInitialPosition: turn towards home, keeping
    // whatever vertical facing it already had.
    void FaceInitialPosition();

    // Aim at `target`, spreading the turn over ten steps.
    void BeginTurn(net::Vec3 target);

    [[nodiscard]] bool InLeash() const {
        return agent_.crash_pillar.leash_volume.contains(
            to_volume_point(main_.position));
    }
    [[nodiscard]] bool InActivation() const {
        return agent_.crash_pillar.activation_volume.contains(
            to_volume_point(main_.position));
    }
    void SetAnimation(std::uint8_t index) {
        agent_.crash_pillar_animation = index;
        agent_.crash_pillar_animation_frame = 0;
        agent_.crash_pillar_animation_ended = false;
    }

    const EnemyScene& scene_;
    EnemyState& agent_;
    const net::PlayerState& main_;
    std::uint64_t frame_count_ = 0;
};

void Enemy38Entity::SetCameraShake(const float shake_max) const {
    const net::Vec3 between{main_.position.x - agent_.position.x,
                            main_.position.y - agent_.position.y,
                            main_.position.z - agent_.position.z};
    const float squared = between.x * between.x + between.y * between.y
        + between.z * between.z;
    if (squared <= 0.0001F || !scene_.CameraShake) {
        return;
    }
    // Inverse square, so it drops away fast: the cap is what a landing at
    // your feet feels like and everything else is much less.
    scene_.CameraShake(std::min(3.0F / squared, shake_max));
}

void Enemy38Entity::DoThing(const float shake_max) {
    if (agent_.crash_pillar_animation_frame >= 17) {
        // Past the charge: stop dead, so it does not slide.
        agent_.velocity.x = 0.0F;
        agent_.velocity.z = 0.0F;
        return;
    }
    if (agent_.crash_pillar_animation_frame == 12 && frame_count_ % 2 == 0) {
        // The foot comes down on frame twelve.
        SetCameraShake(shake_max);
    }
    agent_.velocity.x = agent_.facing.x * ChargeSpeed;
    agent_.velocity.z = agent_.facing.z * ChargeSpeed;
}

void Enemy38Entity::FaceInitialPosition() {
    const net::Vec3 home = agent_.crash_pillar_initial_position;
    if (std::fabs(agent_.position.x - home.x) < 1.0F / 4096.0F
        && std::fabs(agent_.position.z - home.z) < 1.0F / 4096.0F) {
        return;
    }
    const net::Vec3 flat = flat_direction(agent_.position, home,
                                          agent_.facing);
    // The vertical part of the facing is kept: the statue leans as it
    // walks and turning should not stand it back up.
    agent_.facing = {flat.x, agent_.facing.y, flat.z};
}

void Enemy38Entity::BeginTurn(const net::Vec3 target) {
    agent_.crash_pillar_target_vector = target;
    const float dot = std::clamp(
        agent_.facing.x * target.x + agent_.facing.y * target.y
            + agent_.facing.z * target.z, -1.0F, 1.0F);
    const float degrees = std::acos(dot) * 180.0F / Pi;
    agent_.crash_pillar_aim_steps = TurnSteps;
    agent_.crash_pillar_aim_angle_step =
        degrees / static_cast<float>(TurnSteps);
}

void Enemy38Entity::State06() {
    if (agent_.crash_pillar_delay_timer == DelayFrames) {
        SetAnimation(6);
    }
    CallSubroutine();
}

void Enemy38Entity::State07() {
    if (agent_.crash_pillar_animation == 6
        && agent_.crash_pillar_animation_ended) {
        SetAnimation(4);
    }
    CallSubroutine();
}

void Enemy38Entity::State12() {
    if (!InActivation() || !InLeash()) {
        FaceInitialPosition();
    }
    CallSubroutine();
}

void Enemy38Entity::State13() {
    FaceInitialPosition();
    DoThing(0.08F);
    CallSubroutine();
}

bool Enemy38Entity::Behavior00() {
    if (agent_.crash_pillar_aim_steps > 0) {
        --agent_.crash_pillar_aim_steps;
        return false;
    }
    agent_.velocity.y = DropSpeed;
    agent_.crash_pillar_aim_steps = AimSteps;
    SetAnimation(3);
    return true;
}

bool Enemy38Entity::Behavior01() {
    if (!agent_.crash_pillar_animation_ended) {
        return false;
    }
    BeginTurn(flat_direction(agent_.position, main_.position,
                             agent_.facing));
    SetAnimation(7);
    return true;
}

bool Enemy38Entity::Behavior02() {
    if (!agent_.crash_pillar_animation_ended) {
        return false;
    }
    SetAnimation(1);
    return true;
}

bool Enemy38Entity::Behavior03() const {
    return agent_.crash_pillar_animation_ended;
}

bool Enemy38Entity::Behavior04() {
    if (!scene_.SeekFacing
        || !scene_.SeekFacing(agent_, agent_.crash_pillar_target_vector,
                              agent_.crash_pillar_aim_steps,
                              agent_.crash_pillar_aim_angle_step)) {
        return false;
    }
    SetAnimation(0);
    return true;
}

bool Enemy38Entity::Behavior05() {
    // Somebody has walked into the outer volume: wake up.
    if (!InLeash()) {
        return false;
    }
    SetAnimation(0);
    return true;
}

bool Enemy38Entity::Behavior06() {
    if (!scene_.Blocked
        || !scene_.Blocked(agent_.position,
                           add(agent_.position, agent_.velocity),
                           agent_.body_radius)) {
        return false;
    }
    // Landed.  This is the one moment the camera really shakes, and the
    // pillar is invulnerable again from here.
    agent_.velocity = {};
    agent_.invulnerable = true;
    SetCameraShake(0.7F);
    SetAnimation(5);
    return true;
}

bool Enemy38Entity::Behavior07() {
    return scene_.SeekFacing
        && scene_.SeekFacing(agent_, agent_.crash_pillar_target_vector,
                             agent_.crash_pillar_aim_steps,
                             agent_.crash_pillar_aim_angle_step);
}

bool Enemy38Entity::Behavior08() {
    // Top of the jump.
    if (agent_.position.y < agent_.crash_pillar_jump_height) {
        return false;
    }
    agent_.velocity = {};
    return true;
}

bool Enemy38Entity::Behavior09() {
    // A few frames into the jump the pillar becomes vulnerable, which is
    // the whole of how it is meant to be fought.
    if (agent_.crash_pillar_jump_timer > 0) {
        --agent_.crash_pillar_jump_timer;
        return false;
    }
    agent_.invulnerable = false;
    agent_.crash_pillar_jump_timer = JumpFrames;
    return true;
}

bool Enemy38Entity::Behavior10() {
    if (agent_.crash_pillar_delay_timer > 0) {
        --agent_.crash_pillar_delay_timer;
        return false;
    }
    // The jump is aimed flat at the player and lifted by a fixed amount,
    // so a pillar chasing somebody uphill falls short.
    const net::Vec3 between{main_.position.x - agent_.position.x,
                            main_.position.y - agent_.position.y,
                            main_.position.z - agent_.position.z};
    const float factor = std::sqrt((80.0F / 4096.0F)
                                   / (22937.0F / 4096.0F));
    agent_.velocity = {between.x * factor / 2.0F,
                       std::sqrt(448.0F / 4096.0F) / 2.0F,
                       between.z * factor / 2.0F};
    agent_.crash_pillar_jump_height = agent_.position.y + JumpRise;
    agent_.crash_pillar_delay_timer = DelayFrames;
    agent_.crash_pillar_aim_steps = AimSteps;
    return true;
}

bool Enemy38Entity::Behavior11() {
    return scene_.SeekFacing
        && scene_.SeekFacing(agent_, agent_.crash_pillar_target_vector,
                             agent_.crash_pillar_aim_steps,
                             agent_.crash_pillar_aim_angle_step);
}

bool Enemy38Entity::Behavior12() {
    if (!agent_.crash_pillar_animation_ended) {
        return false;
    }
    SetAnimation(2);
    return true;
}

bool Enemy38Entity::Behavior13() {
    if (!scene_.SeekFacing
        || !scene_.SeekFacing(agent_, agent_.crash_pillar_target_vector,
                              agent_.crash_pillar_aim_steps,
                              agent_.crash_pillar_aim_angle_step)) {
        return false;
    }
    SetAnimation(2);
    return true;
}

bool Enemy38Entity::Behavior14() {
    // The player has left the inner volume: stop, and turn for home.
    if (InActivation()) {
        return false;
    }
    agent_.velocity = {};
    BeginTurn(flat_direction(agent_.position,
                             agent_.crash_pillar_initial_position,
                             agent_.facing));
    SetAnimation(7);
    return true;
}

bool Enemy38Entity::Behavior15() {
    const net::Vec3 between{agent_.position.x - main_.position.x,
                            agent_.position.y - main_.position.y,
                            agent_.position.z - main_.position.z};
    if (between.x * between.x + between.y * between.y
            + between.z * between.z >= LingerRadius * LingerRadius) {
        return false;
    }
    // Close enough to be worth waiting a moment before hopping again.
    agent_.crash_pillar_delay_timer = LingerFrames;
    return true;
}

bool Enemy38Entity::Behavior16() {
    if (agent_.crash_pillar_delay_timer > 0) {
        --agent_.crash_pillar_delay_timer;
        return false;
    }
    agent_.crash_pillar_delay_timer = DelayFrames;
    SetAnimation(2);
    return true;
}

bool Enemy38Entity::Behavior17() {
    // Home again: turn back to exactly the way it was standing.
    const net::Vec3 between{
        agent_.position.x - agent_.crash_pillar_initial_position.x,
        agent_.position.y - agent_.crash_pillar_initial_position.y,
        agent_.position.z - agent_.crash_pillar_initial_position.z};
    if (between.x * between.x + between.y * between.y
            + between.z * between.z >= HomeRadius * HomeRadius) {
        return false;
    }
    BeginTurn(agent_.crash_pillar_initial_facing);
    return true;
}

bool Enemy38Entity::Behavior18() {
    if (!InActivation() || !InLeash()) {
        return false;
    }
    BeginTurn(flat_direction(agent_.position, main_.position,
                             agent_.facing));
    SetAnimation(7);
    agent_.crash_pillar_delay_timer = DelayFrames;
    return true;
}

void Enemy38Entity::CallSubroutine() {
    static_cast<void>(metadata::call_subroutine(
        metadata::Enemy38Subroutines, agent_.sub_id, agent_.next_state,
        [this](std::uint8_t index) {
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
            default: return false;
            }
        }));
}

void Enemy38Entity::CallStateProcess() {
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
    default: break;
    }
}

void Enemy38Entity::EnemyProcess() {
    if (agent_.state == 3 || agent_.state == 4) {
        // Turning to face the player as it charges.
        agent_.facing = flat_direction(agent_.position, main_.position,
                                       agent_.facing);
    }
    // Falling.  States seven and eight are the top of the jump, where the
    // pillar is much lighter; nine and ten are on the ground.
    if (agent_.state == 7 || agent_.state == 8) {
        agent_.velocity.y -= 0.02F / 4.0F;  // todo: FPS stuff
    } else if (agent_.state != 9 && agent_.state != 10) {
        agent_.velocity.y -= 100.0F / 4096.0F / 4.0F;  // todo: FPS stuff
    }
    if (agent_.state != 7 && agent_.state != 10 && scene_.Blocked
        && scene_.Blocked(agent_.position,
                          add(agent_.position, agent_.velocity),
                          agent_.body_radius)) {
        agent_.velocity = {};
    }
    // EnemyInstanceEntity.ContactDamagePlayer.  The managed base asks
    // HitPlayers, which the engine's own collision pass fills in; this
    // head has no such pass for enemies, so the overlap is tested here
    // against the hurt volume the spawner authored.
    // The authored volume is anchored where the pillar was spawned, so it
    // follows the pillar by however far it has walked -- which is
    // EnemyInstanceEntity.UpdateHurtVolume, in the one form this enemy
    // needs.
    const net::Vec3 walked{
        agent_.position.x - agent_.crash_pillar_initial_position.x,
        agent_.position.y - agent_.crash_pillar_initial_position.y,
        agent_.position.z - agent_.crash_pillar_initial_position.z};
    if (scene_.ContactDamage
        && agent_.crash_pillar.hurt_volume
               .moved({walked.x, walked.y, walked.z})
               .contains(to_volume_point(main_.position))) {
        // Forty while it is coming down on you, ten otherwise.
        scene_.ContactDamage(agent_, main_.slot_index,
                             agent_.state == 10 ? ContactDamageCharging
                                                : ContactDamageWalking);
    }
    CallStateProcess();
}

} // namespace

void Session::update_crash_pillar(EnemyState& agent) {
    if (!agent.crash_pillar.supported) {
        return;
    }
    const net::PlayerState* main = nullptr;
    float nearest = std::numeric_limits<float>::max();
    for (const auto& player : players_) {
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
    if (main == nullptr) {
        return;
    }
    // The managed game has only PlayerEntity.Main; the rest of this head
    // wants to know which player an enemy is dealing with, so it is
    // recorded even though Enemy38Entity itself never asks.
    agent.target_slot = main->slot_index;

    // EnemyInstanceEntity.BaseProcess.
    agent.state = agent.next_state;
    agent.sub_id = agent.state;
    agent.position = add(agent.position, agent.velocity);
    if (!agent.crash_pillar_animation_ended) {
        ++agent.crash_pillar_animation_frame;
    }

    EnemyScene scene;
    scene.Blocked = [this](net::Vec3 from, net::Vec3 to, float radius) {
        return collision::sweep_sphere(room_.collision(), to_collision(from),
                                       to_collision(to), radius, 0x2000)
            .has_value();
    };
    scene.ContactDamage = [this](EnemyState& target, std::uint8_t slot,
                                 std::uint32_t damage) {
        for (auto& player : players_) {
            if (player.slot_index == slot) {
                apply_enemy_contact_damage(
                    target, player, static_cast<std::uint16_t>(damage));
                return;
            }
        }
    };
    scene.CameraShake = [](float) {
        // The camera belongs to the player's own view, which the gameplay
        // session does not own.  Said here rather than shaken quietly.
    };
    scene.SeekFacing = [](EnemyState& target, net::Vec3 desired,
                          std::uint16_t& steps, float angle) {
        // EnemyInstanceEntity.SeekTargetFacing, on the session's own
        // enemy rather than on a runtime entity it does not hold.
        constexpr float Radians = 3.14159265358979323846F / 180.0F;
        const float radians = angle * Radians;
        const float dot = desired.x * target.facing.x
            + desired.y * target.facing.y + desired.z * target.facing.z;
        if (steps == 0 || dot >= std::cos(radians)) {
            target.facing = desired;
            return true;
        }
        // The shorter way round, which is the only one that reads as
        // deliberate.
        const float cross_y = desired.z * target.facing.x
            - desired.x * target.facing.z;
        const float turn = radians * (cross_y <= 0.0F ? 1.0F : -1.0F);
        const float sine = std::sin(turn);
        const float cosine = std::cos(turn);
        const net::Vec3 turned{
            target.facing.x * cosine + target.facing.z * sine,
            target.facing.y,
            -target.facing.x * sine + target.facing.z * cosine};
        const float length = std::sqrt(turned.x * turned.x
                                       + turned.y * turned.y
                                       + turned.z * turned.z);
        if (length > 0.0F) {
            target.facing = {turned.x / length, turned.y / length,
                             turned.z / length};
        }
        --steps;
        return false;
    };

    Enemy38Entity(scene, agent, *main, tick_count()).EnemyProcess();
}
} // namespace fruityprime::gameplay

namespace fruityprime::enemy::module_38_crash_pillar {

void EnemyInitialize(gameplay::EnemyState& agent) noexcept {
    // A hundred and fifty, but it only takes damage for a few frames of
    // each jump, so the number is not what makes it hard.
    agent.health = agent.health_max = 150;
    agent.body_radius = 0.5F;
    agent.invulnerable = true;
    agent.state = agent.next_state = agent.sub_id = 0;
    agent.crash_pillar_jump_timer = 5u * 2u;
    agent.crash_pillar_delay_timer = 15u * 2u;
    agent.crash_pillar_aim_steps = 8u * 2u;
    agent.crash_pillar_target_vector = {1.0F, 0.0F, 0.0F};
    agent.crash_pillar_initial_position = agent.position;
    agent.crash_pillar_initial_facing = agent.facing;
    agent.crash_pillar_animation = 1;
    agent.crash_pillar_animation_frame = 0;
    agent.crash_pillar_animation_ended = false;
    static_cast<void>(gameplay::StartingHealth);
}

} // namespace fruityprime::enemy::module_38_crash_pillar

namespace fruityprime::enemy {

CrashPillarProfile decode_crash_pillar_profile(
    std::uint8_t id, std::span<const std::uint8_t> fields,
    net::Vec3 origin) noexcept {
    CrashPillarProfile result;
    if (id != static_cast<std::uint8_t>(formats::EnemyType::CrashPillar)
        || fields.size() < 3 * 64) {
        return result;
    }
    // S00's three volumes in order: what the pillar is hit on, how far it
    // will chase, and how close somebody has to come to wake it.
    result.hurt_volume = detail::read_volume(fields, 0, origin);
    result.leash_volume = detail::read_volume(fields, 64, origin);
    result.activation_volume = detail::read_volume(fields, 128, origin);
    result.supported = result.hurt_volume.kind != scene::VolumeKind::Invalid
        && result.leash_volume.kind != scene::VolumeKind::Invalid
        && result.activation_volume.kind != scene::VolumeKind::Invalid;
    return result;
}

} // namespace fruityprime::enemy
