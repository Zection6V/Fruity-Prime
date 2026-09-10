#include <cmath>
#include "Utility/rng.hpp"
#include "Metadata/enemy_subroutines.hpp"
#include "enemy_scene.hpp"
#include "enemy_decode_common.hpp"
// Native counterpart of src/MphRead/Entities/Enemies/10_BarbedWarWasp.cs.
// This file is deliberately present even when the managed class currently
// shares a native controller; the descriptor and entry point prevent a
// many-classes-in-one gameplay.cpp regression.
#include "10_BarbedWarWasp.hpp"
#include "enemy_common.hpp"

namespace fruityprime::gameplay {

namespace {

// A player's own body, which the contact overlap counts.
constexpr float PlayerBodyRadius = 0.45F;
constexpr float Pi10 = 3.14159265358979323846F;
constexpr float CircleStep10 = 1.5F / 2.0F;  // todo: FPS stuff
constexpr std::uint32_t ShotCycleFrames = 30u * 2u;  // todo: FPS stuff
// The frame of the wind-up the shot actually leaves on, and the frame the
// recoil animation is started at.
constexpr std::uint32_t ShotFireFrame = 10u * 2u;
constexpr std::uint32_t ShotRecoilFrame = 15u * 2u;

[[nodiscard]] float length10(const net::Vec3 value) noexcept {
    return std::sqrt(value.x * value.x + value.y * value.y
                     + value.z * value.z);
}

// Native counterpart of Enemy10Entity.
//
// A barbed war wasp flies the same authored patrol as a plain one and
// carries a beam.  Everything about the movement is the wasp's; what is
// its own is the shooting: it fires a run of shots decided at random
// between the subtype's minimum and maximum, one every thirty frames,
// and then goes back to its patrol until the run is spent.
//
// It also has one thing the plain wasp does not: hitting a wall while it
// is shooting knocks it into its recover state directly, without waiting
// for a behaviour to notice.
class Enemy10Entity final {
public:
    Enemy10Entity(const EnemyScene& scene, EnemyState& agent,
                  const net::PlayerState* main) noexcept
        : scene_(scene), agent_(agent), main_(main) {}

    // Enemy10Entity.EnemyProcess.
    void EnemyProcess();

private:
    void State0();
    void State1();
    void State2() { State1(); }
    void State3();
    void State4();
    void State5() { CallSubroutine(); }

    [[nodiscard]] bool Behavior00();
    [[nodiscard]] bool Behavior01();
    [[nodiscard]] bool Behavior02();
    [[nodiscard]] bool Behavior03();
    [[nodiscard]] bool Behavior04();
    [[nodiscard]] bool Behavior05();
    [[nodiscard]] bool Behavior06();
    [[nodiscard]] bool Behavior07();
    [[nodiscard]] bool Behavior08();
    [[nodiscard]] bool Behavior09();

    void CallStateProcess();
    void CallSubroutine();
    void StartMovingToward(net::Vec3 target, float step);
    void StartMovingTowardPosition();
    void MoveInCircle();
    void ReachTargetOrReversePattern();
    void ReversePattern();
    void FaceTowards(net::Vec3 target);

    // Enemy10Entity.PlayBeamShotSfx: the beam's own shot sound, which the
    // managed code shares with the player's.
    void PlayBeamShotSfx() const {}

    // How many shots this run: somewhere between the subtype's minimum
    // and maximum, rolled fresh each time.
    [[nodiscard]] std::uint16_t RollShotCount() const noexcept {
        const auto& values = agent_.barbed_warwasp;
        const std::uint16_t spread = values.max_shots >= values.min_shots
            ? static_cast<std::uint16_t>(values.max_shots + 1
                                         - values.min_shots) : 1u;
        return static_cast<std::uint16_t>(
            values.min_shots + utility::get_random_int2(
                static_cast<std::uint32_t>(spread)));
    }

    [[nodiscard]] net::Vec3 PlayerPosition() const noexcept {
        return main_ != nullptr ? main_->position : agent_.position;
    }
    [[nodiscard]] bool PlayerAtHome() const noexcept {
        return main_ != nullptr
            && agent_.authored_motion.home_volume.contains(
                to_volume_point(main_->position));
    }
    [[nodiscard]] bool Touching() const noexcept {
        if (main_ == nullptr) {
            return false;
        }
        const float reach = agent_.body_radius + PlayerBodyRadius;
        return distance_squared(agent_.position, main_->position)
            <= reach * reach;
    }
    [[nodiscard]] float Step(std::int32_t fixed_value) const noexcept {
        return static_cast<float>(fixed_value) / 4096.0F;
    }

    const EnemyScene& scene_;
    EnemyState& agent_;
    const net::PlayerState* main_;
};

void Enemy10Entity::FaceTowards(const net::Vec3 target) {
    const net::Vec3 delta{target.x - agent_.position.x,
                          target.y - agent_.position.y,
                          target.z - agent_.position.z};
    const float length = length10(delta);
    if (length > 0.0F) {
        agent_.facing = {delta.x / length, delta.y / length,
                         delta.z / length};
    }
}

void Enemy10Entity::StartMovingToward(const net::Vec3 target,
                                      const float step) {
    const net::Vec3 travel{target.x - agent_.position.x,
                           target.y - agent_.position.y,
                           target.z - agent_.position.z};
    agent_.authored_motion.step_distance = step;
    const float distance = length10(travel);
    if (distance == 0.0F || step <= 0.0F) {
        agent_.velocity = {};
        agent_.wasp_step_count = 1;
        return;
    }
    agent_.wasp_step_count =
        (static_cast<std::int32_t>(distance / step) + 1) * 2;
    const float scale = step / distance / 2.0F;
    agent_.velocity = {travel.x * scale, travel.y * scale,
                       travel.z * scale};
}

void Enemy10Entity::StartMovingTowardPosition() {
    const std::size_t index = std::min<std::size_t>(
        agent_.motion_index,
        agent_.authored_motion.path_positions.size() - 1);
    agent_.wasp_move_target = agent_.authored_motion.path_positions[index];
    StartMovingToward(agent_.wasp_move_target,
                      Step(agent_.barbed_warwasp.step_distance1));
}

void Enemy10Entity::MoveInCircle() {
    if (agent_.authored_motion.movement_type != 0) {
        return;
    }
    agent_.wasp_circle_angle += CircleStep10;
    if (agent_.wasp_circle_angle >= 360.0F) {
        agent_.wasp_circle_angle -= 360.0F;
    }
    const float angle = agent_.wasp_circle_angle * Pi10 / 180.0F;
    const float radius = agent_.authored_motion.circle_radius;
    agent_.velocity.x = agent_.authored_motion.initial_position.x
        + std::sin(angle) * radius;
    agent_.velocity.z = agent_.authored_motion.initial_position.z
        + std::cos(angle) * radius;
    const float length = length10(agent_.velocity);
    if (length > 0.0F) {
        agent_.facing = {agent_.velocity.x / length,
                         agent_.velocity.y / length,
                         agent_.velocity.z / length};
    }
}

void Enemy10Entity::ReversePattern() {
    agent_.wasp_pattern = agent_.wasp_next_pattern;
    if (agent_.wasp_pattern == 0) {
        agent_.wasp_final_move_index = agent_.wasp_max_move_index;
        agent_.motion_index =
            agent_.motion_index >= agent_.wasp_max_move_index
            ? 0 : static_cast<std::uint8_t>(agent_.motion_index + 1);
    } else {
        agent_.wasp_final_move_index = 0;
        agent_.motion_index = agent_.motion_index == 0
            ? agent_.wasp_max_move_index
            : static_cast<std::uint8_t>(agent_.motion_index - 1);
    }
    StartMovingTowardPosition();
}

void Enemy10Entity::ReachTargetOrReversePattern() {
    const net::Vec3 remaining{
        agent_.wasp_move_target.x - agent_.position.x,
        agent_.wasp_move_target.y - agent_.position.y,
        agent_.wasp_move_target.z - agent_.position.z};
    if (length10(remaining) == 0.0F) {
        agent_.wasp_step_count = 0;
        return;
    }
    ReversePattern();
}

bool Enemy10Entity::Behavior00() {
    // The wind-up has finished: lock the aim and start the shot.
    if (!agent_.crash_pillar_animation_ended) {
        return false;
    }
    const net::Vec3 aim{PlayerPosition().x - agent_.position.x,
                        PlayerPosition().y + 0.5F - agent_.position.y,
                        PlayerPosition().z - agent_.position.z};
    const float length = length10(aim);
    agent_.wasp_aim_vector = length > 0.0F
        ? net::Vec3{aim.x / length, aim.y / length, aim.z / length}
        : agent_.facing;
    agent_.crash_pillar_animation_ended = false;
    return true;
}

bool Enemy10Entity::Behavior01() {
    if (agent_.authored_motion.movement_type == 0
        && (agent_.state == 0 || agent_.state == 1)) {
        return false;
    }
    if (agent_.wasp_step_count > 0) {
        --agent_.wasp_step_count;
        return false;
    }
    if (agent_.authored_motion.movement_type == 0) {
        return true;
    }
    if (agent_.wasp_pattern == 1) {
        agent_.motion_index = agent_.motion_index == 0
            ? agent_.wasp_max_move_index
            : static_cast<std::uint8_t>(agent_.motion_index - 1);
    } else if (agent_.wasp_pattern == 2 || agent_.wasp_pattern == 0) {
        agent_.motion_index =
            agent_.motion_index >= agent_.wasp_max_move_index
            ? 0 : static_cast<std::uint8_t>(agent_.motion_index + 1);
        if (agent_.wasp_pattern == 0
            && agent_.authored_motion.movement_type == 3
            && agent_.motion_index == 0) {
            agent_.health = 0;
        }
    } else if (agent_.wasp_pattern == 3 && agent_.motion_index > 0) {
        --agent_.motion_index;
    }
    const std::size_t index = std::min<std::size_t>(
        agent_.motion_index,
        agent_.authored_motion.path_positions.size() - 1);
    agent_.wasp_move_target = agent_.authored_motion.path_positions[index];
    // State four is the walk back, which uses the patrol speed rather than
    // whatever the chase left behind.
    StartMovingToward(agent_.wasp_move_target,
                      agent_.state == 4
                          ? Step(agent_.barbed_warwasp.step_distance1)
                          : agent_.authored_motion.step_distance);
    FaceTowards(agent_.wasp_move_target);
    return true;
}

bool Enemy10Entity::Behavior02() {
    if (agent_.wasp_step_count > 0) {
        --agent_.wasp_step_count;
        return false;
    }
    if (agent_.authored_motion.movement_type == 0) {
        agent_.velocity = {};
    } else {
        StartMovingTowardPosition();
    }
    return true;
}

bool Enemy10Entity::Behavior03() {
    if (agent_.authored_motion.movement_type == 3 || !PlayerAtHome()) {
        return false;
    }
    agent_.wasp_final_move_index = agent_.motion_index;
    agent_.wasp_next_pattern = 2;
    agent_.wasp_pattern = 2;
    agent_.authored_motion.step_distance =
        Step(agent_.barbed_warwasp.step_distance2);
    return true;
}

bool Enemy10Entity::Behavior04() {
    // The run is spent: roll a new one and go back to patrolling until it
    // is time to use it.
    if (agent_.wasp_shot_count > 0 || agent_.wasp_shot_timer > 0) {
        return false;
    }
    agent_.wasp_shot_count = RollShotCount();
    agent_.wasp_shot_timer = ShotCycleFrames;
    if (agent_.authored_motion.movement_type == 0) {
        agent_.velocity = {};
    } else {
        StartMovingTowardPosition();
    }
    return true;
}

bool Enemy10Entity::Behavior05() {
    // Shots left and the last one has finished: wind up again.
    if (agent_.wasp_shot_count == 0 || agent_.wasp_shot_timer > 0) {
        return false;
    }
    agent_.wasp_shot_timer = ShotCycleFrames;
    agent_.crash_pillar_animation_ended = false;
    return true;
}

bool Enemy10Entity::Behavior06() {
    if (agent_.authored_motion.movement_type != 0
        && agent_.wasp_final_move_index != agent_.motion_index) {
        return false;
    }
    agent_.velocity = {};
    FaceTowards(PlayerPosition());
    agent_.crash_pillar_animation_ended = false;
    return true;
}

bool Enemy10Entity::Behavior07() {
    if (agent_.authored_motion.home_volume.contains(
            to_volume_point(agent_.position))) {
        return false;
    }
    ReachTargetOrReversePattern();
    return true;
}

bool Enemy10Entity::Behavior08() {
    if (PlayerAtHome()) {
        return false;
    }
    ReversePattern();
    return true;
}

bool Enemy10Entity::Behavior09() {
    if (main_ == nullptr || !scene_.Blocked
        || !scene_.Blocked(agent_.position, main_->position,
                           agent_.body_radius)) {
        return false;
    }
    ReachTargetOrReversePattern();
    return true;
}

void Enemy10Entity::State0() {
    MoveInCircle();
    if (agent_.authored_motion.movement_type != 0) {
        FaceTowards(agent_.wasp_move_target);
    }
    CallSubroutine();
}

void Enemy10Entity::State1() {
    FaceTowards(PlayerPosition());
    CallSubroutine();
}

void Enemy10Entity::State3() {
    if (agent_.wasp_shot_count > 0
        && agent_.wasp_shot_timer == ShotFireFrame) {
        // The shot leaves half a unit below the wasp, along the aim it
        // locked when the wind-up finished -- not at where the player is
        // now, which is what makes the wind-up worth watching for.
        if (scene_.SpawnProjectile) {
            scene_.SpawnProjectile(agent_,
                                   {agent_.position.x,
                                    agent_.position.y - 0.5F,
                                    agent_.position.z},
                                   agent_.wasp_aim_vector);
        }
        --agent_.wasp_shot_count;
        PlayBeamShotSfx();
    }
    if (agent_.wasp_shot_timer > 0) {
        --agent_.wasp_shot_timer;
    }
    if (agent_.wasp_shot_timer == 0) {
        agent_.crash_pillar_animation_ended = true;
    }
    CallSubroutine();
}

void Enemy10Entity::State4() {
    FaceTowards(agent_.wasp_move_target);
    CallSubroutine();
}

void Enemy10Entity::CallSubroutine() {
    static_cast<void>(metadata::call_subroutine(
        metadata::Enemy10Subroutines, agent_.sub_id, agent_.next_state,
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
            default: return false;
            }
        }));
}

void Enemy10Entity::CallStateProcess() {
    switch (agent_.state) {
    case 0: State0(); break;
    case 1: State1(); break;
    case 2: State2(); break;
    case 3: State3(); break;
    case 4: State4(); break;
    case 5: State5(); break;
    default: break;
    }
}

void Enemy10Entity::EnemyProcess() {
    // Hitting a wall while shooting knocks the wasp straight into its
    // recover state, without waiting for a behaviour to notice -- the
    // plain wasp has nothing like this.
    if (scene_.Blocked
        && scene_.Blocked(agent_.position,
                          add(agent_.position, agent_.velocity),
                          agent_.body_radius)
        && agent_.state == 3) {
        agent_.next_state = 5;
        agent_.sub_id = 5;
        StartMovingToward(agent_.wasp_move_target,
                          Step(agent_.barbed_warwasp.step_distance3));
        FaceTowards(agent_.wasp_move_target);
    }
    if (scene_.ContactDamage && main_ != nullptr && Touching()) {
        scene_.ContactDamage(agent_, main_->slot_index,
                             agent_.barbed_warwasp.contact_damage);
    }
    CallStateProcess();
}

} // namespace

void Session::update_barbed_warwasp(EnemyState& agent) {
    if (!agent.authored_motion.supported) {
        static_cast<void>(update_generic_enemy(agent));
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
    if (main != nullptr) {
        agent.target_slot = main->slot_index;
    }

    // EnemyInstanceEntity.BaseProcess.
    agent.state = agent.next_state;
    agent.sub_id = agent.state;
    agent.position = add(agent.position, agent.velocity);

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
    scene.SpawnProjectile = [this](const EnemyState& shooter,
                                   net::Vec3 position, net::Vec3 direction) {
        spawn_enemy_projectile(shooter, position, direction);
    };
    Enemy10Entity(scene, agent, main).EnemyProcess();
}
} // namespace fruityprime::gameplay

static_assert(fruityprime::enemy::module_10_barbed_war_wasp::kModule.managed_class.size() != 0);

namespace fruityprime::enemy::module_10_barbed_war_wasp {

void EnemyInitialize(gameplay::EnemyState& agent) noexcept {
    // Everything but the movement comes off the subtype table: how much
    // energy, how hard the beam hits, how many shots a run is, and three
    // different walking speeds for patrolling, chasing and recovering.
    const auto& values = agent.barbed_warwasp;
    agent.health = agent.health_max = values.health;
    agent.body_radius = 1.0F;
    agent.state = agent.next_state = agent.sub_id = 0;
    agent.authored_motion.step_distance =
        static_cast<float>(values.step_distance1) / 4096.0F;
    agent.authored_motion.initial_position = agent.position;
    agent.wasp_move_target = agent.position;
    agent.wasp_aim_vector = agent.facing;
    agent.wasp_pattern = 0;
    agent.wasp_next_pattern = 0;
    const std::uint16_t spread = values.max_shots >= values.min_shots
        ? static_cast<std::uint16_t>(values.max_shots + 1 - values.min_shots)
        : 1u;
    agent.wasp_shot_count = static_cast<std::uint16_t>(
        values.min_shots
        + utility::get_random_int2(static_cast<std::uint32_t>(spread)));
    agent.wasp_shot_timer = 30u * 2u;  // todo: FPS stuff
    if (agent.authored_motion.movement_type == 1) {
        agent.motion_index = 1;
        agent.wasp_max_move_index = 3;
    } else if (agent.authored_motion.movement_type == 2
               || agent.authored_motion.movement_type == 3) {
        agent.motion_index = 0;
        agent.wasp_max_move_index = agent.authored_motion.path_count > 0
            ? static_cast<std::uint8_t>(agent.authored_motion.path_count - 1)
            : 0;
    }
    agent.wasp_final_move_index = agent.wasp_max_move_index;
}

} // namespace fruityprime::enemy::module_10_barbed_war_wasp

namespace fruityprime::enemy {

BarbedWarWaspProfile decode_barbed_warwasp_profile(
    std::uint8_t id, std::span<const std::uint8_t> fields) noexcept {
    BarbedWarWaspProfile result;
    if (id != static_cast<std::uint8_t>(formats::EnemyType::BarbedWarWasp)
        || fields.size() < 8) {
        return result;
    }

    // Enemy10Entity reads EnemyVersion and EnemySubtype from the first two
    // words of S08, then indexes the fixed Enemy10Values table by subtype.
    result.version = std::min<std::uint32_t>(detail::read_u32(fields, 0), 10u);
    result.variant = static_cast<std::uint8_t>(std::min<std::uint32_t>(
        detail::read_u32(fields, 4), 2u));
    struct Values {
        std::uint16_t health;
        std::uint16_t beam_damage;
        std::uint16_t splash_damage;
        std::uint16_t contact_damage;
        std::int32_t step_distance1;
        std::int32_t step_distance2;
        std::int32_t step_distance3;
        std::int32_t circle_increment;
        std::uint16_t min_shots;
        std::uint16_t max_shots;
        std::uint16_t scan_id;
        std::uint32_t effectiveness;
    };
    static constexpr std::array<Values, 3> table = {
        Values{50, 3, 0, 15, 1024, 819, 2457, 6144, 1, 2, 215, 0xEABA},
        Values{120, 10, 2, 10, 1433, 1638, 2457, 6144, 1, 3, 191, 0xCEAA},
        Values{120, 8, 0, 10, 614, 409, 1024, 6144, 1, 1, 192, 0xF2AA}};
    const auto& values = table[result.variant];
    result.health = values.health;
    result.beam_damage = values.beam_damage;
    result.splash_damage = values.splash_damage;
    result.contact_damage = values.contact_damage;
    result.step_distance1 = values.step_distance1;
    result.step_distance2 = values.step_distance2;
    result.step_distance3 = values.step_distance3;
    result.circle_increment = values.circle_increment;
    result.min_shots = values.min_shots;
    result.max_shots = values.max_shots;
    result.scan_id = values.scan_id;
    result.effectiveness = values.effectiveness;
    result.supported = true;
    return result;
}

} // namespace fruityprime::enemy

