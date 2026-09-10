#include <cmath>
#include "Metadata/enemy_subroutines.hpp"
#include "enemy_scene.hpp"
#include "enemy_decode_common.hpp"
// Native port of src/MphRead/Entities/Enemies/00_WarWasp.cs.
//
// Enemy00 and Enemy10 deliberately share the movement helpers in the managed
// implementation, but not the subroutine table or ranged attack. Keeping the
// controller here and exposing a member entry point lets the two source
// modules share only that documented part of the implementation.
#include "00_WarWasp.hpp"
#include "enemy_common.hpp"

namespace fruityprime::gameplay {
namespace {

constexpr float Pi = 3.14159265358979323846F;

[[nodiscard]] std::uint32_t frame_count(float seconds) noexcept {
    return static_cast<std::uint32_t>(std::max(
        1.0F, std::round(std::max(0.0F, seconds * 60.0F))));
}

} // namespace

namespace {

// Enemy00Entity's own numbers.
constexpr float PatrolStep = 0.2F;
constexpr float DiveStep = 1.2F;
constexpr float ChaseStep = 0.15F;
constexpr std::uint16_t AttackDelayFrames = 30u * 2u;  // todo: FPS stuff
constexpr std::int32_t DiveFrames = 40 * 2;            // todo: FPS stuff
constexpr std::uint32_t ContactDamage = 3u;
constexpr std::uint32_t StingDamage = 25u;
constexpr float CircleStep = 1.5F / 2.0F;              // todo: FPS stuff
// A player's own body, which every overlap here counts.
constexpr float PlayerBodyRadius = 0.45F;

[[nodiscard]] float length_of(const net::Vec3 value) noexcept {
    return std::sqrt(value.x * value.x + value.y * value.y
                     + value.z * value.z);
}

// Native counterpart of Enemy00Entity.
//
// A war wasp patrols an authored loop -- a square, a list of up to sixteen
// points, or a circle around where it was placed -- and dives at whoever
// comes into its home volume.  The dive is the only fast thing it does:
// everything else is a slow walk between points, and a wasp that has been
// disturbed resumes its patrol going the way it was already going, which
// is what `pattern` and `next_pattern` are for.
class Enemy00Entity final {
public:
    Enemy00Entity(const EnemyScene& scene, EnemyState& agent,
                  const net::PlayerState* main) noexcept
        : scene_(scene), agent_(agent), main_(main) {}

    // Enemy00Entity.EnemyProcess.
    void EnemyProcess();

private:
    void State0();
    void State1();
    void State2() { State1(); }
    void State3() { State1(); }
    void State4();
    void State5();
    void State6();

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
    [[nodiscard]] bool Behavior10();

    void CallStateProcess();
    void CallSubroutine();

    // Enemy00Entity.StartMovingToward: set a speed that covers the
    // distance in whole steps, and count how many are left.  A wasp does
    // not steer -- it points itself at a place and goes.
    void StartMovingToward(net::Vec3 target, float step);
    void StartMovingTowardPosition();

    // Enemy00Entity.MoveInCircle: the movement-type-zero patrol, which is
    // a circle around where the wasp was placed rather than a path.
    void MoveInCircle();

    // Enemy00Entity.ReachTargetOrReversePattern and ReversePattern: how a
    // wasp picks up its patrol again.  Arriving exactly on the target
    // just stops the walk; anything else turns the loop around.
    void ReachTargetOrReversePattern();
    void ReversePattern();

    void FaceTowards(net::Vec3 target);

    [[nodiscard]] net::Vec3 PlayerPosition() const noexcept {
        return main_ != nullptr ? main_->position : agent_.wasp_attack_target;
    }
    // EnemyInstanceEntity.HitPlayers, which the engine's own collision
    // pass fills in and this head does not have for enemies: the overlap
    // is tested here against the wasp's own body instead.
    [[nodiscard]] bool Stung() const noexcept {
        if (main_ == nullptr) {
            return false;
        }
        const float reach = agent_.body_radius + PlayerBodyRadius;
        return distance_squared(agent_.position, main_->position)
            <= reach * reach;
    }

    [[nodiscard]] bool PlayerAtHome() const noexcept {
        return main_ != nullptr
            && agent_.authored_motion.home_volume.contains(
                to_volume_point(main_->position));
    }

    const EnemyScene& scene_;
    EnemyState& agent_;
    const net::PlayerState* main_;
};

void Enemy00Entity::FaceTowards(const net::Vec3 target) {
    const net::Vec3 delta{target.x - agent_.position.x,
                          target.y - agent_.position.y,
                          target.z - agent_.position.z};
    const float length = length_of(delta);
    if (length > 0.0F) {
        agent_.facing = {delta.x / length, delta.y / length,
                         delta.z / length};
    }
}

void Enemy00Entity::StartMovingToward(const net::Vec3 target,
                                      const float step) {
    const net::Vec3 travel{target.x - agent_.position.x,
                           target.y - agent_.position.y,
                           target.z - agent_.position.z};
    agent_.authored_motion.step_distance = step;
    const float distance = length_of(travel);
    if (distance == 0.0F) {
        agent_.velocity = {};
        agent_.wasp_step_count = 1;
        return;
    }
    // Whole steps plus one, doubled for this head's rate, with the speed
    // halved to match -- the wasp arrives at the same time either way.
    agent_.wasp_step_count =
        (static_cast<std::int32_t>(distance / step) + 1) * 2;
    const float scale = step / distance / 2.0F;
    agent_.velocity = {travel.x * scale, travel.y * scale,
                       travel.z * scale};
}

void Enemy00Entity::StartMovingTowardPosition() {
    const std::size_t index = std::min<std::size_t>(
        agent_.motion_index, agent_.authored_motion.path_positions.size() - 1);
    agent_.wasp_move_target = agent_.authored_motion.path_positions[index];
    StartMovingToward(agent_.wasp_move_target,
                      agent_.authored_motion.movement_type == 3 ? 0.25F
                                                                : PatrolStep);
}

void Enemy00Entity::MoveInCircle() {
    if (agent_.authored_motion.movement_type != 0) {
        return;
    }
    agent_.wasp_circle_angle += CircleStep;
    if (agent_.wasp_circle_angle >= 360.0F) {
        agent_.wasp_circle_angle -= 360.0F;
    }
    const float angle = agent_.wasp_circle_angle * Pi / 180.0F;
    const float radius = agent_.authored_motion.circle_radius;
    // The managed code writes the circle into _speed and then faces along
    // it, which is how a circling wasp always looks outward.
    agent_.velocity.x = agent_.authored_motion.initial_position.x
        + std::sin(angle) * radius;
    agent_.velocity.z = agent_.authored_motion.initial_position.z
        + std::cos(angle) * radius;
    const float length = length_of(agent_.velocity);
    if (length > 0.0F) {
        agent_.facing = {agent_.velocity.x / length,
                         agent_.velocity.y / length,
                         agent_.velocity.z / length};
    }
}

void Enemy00Entity::ReversePattern() {
    agent_.wasp_pattern = agent_.wasp_next_pattern;
    if (agent_.wasp_pattern == 0) {
        agent_.wasp_final_move_index = agent_.wasp_max_move_index;
        agent_.motion_index = agent_.motion_index >= agent_.wasp_max_move_index
            ? 0 : static_cast<std::uint8_t>(agent_.motion_index + 1);
    } else {
        agent_.wasp_final_move_index = 0;
        agent_.motion_index = agent_.motion_index == 0
            ? agent_.wasp_max_move_index
            : static_cast<std::uint8_t>(agent_.motion_index - 1);
    }
    StartMovingTowardPosition();
}

void Enemy00Entity::ReachTargetOrReversePattern() {
    const net::Vec3 remaining{
        agent_.wasp_move_target.x - agent_.position.x,
        agent_.wasp_move_target.y - agent_.position.y,
        agent_.wasp_move_target.z - agent_.position.z};
    if (length_of(remaining) == 0.0F) {
        // Already there: stop, and let the next behaviour pick it up.
        agent_.wasp_step_count = 0;
        return;
    }
    ReversePattern();
}

bool Enemy00Entity::Behavior00() {
    if (agent_.wasp_step_count > 0) {
        --agent_.wasp_step_count;
        return false;
    }
    // The dive itself, at six times the patrol speed.
    StartMovingToward(agent_.wasp_attack_target, DiveStep);
    return true;
}

bool Enemy00Entity::Behavior01() {
    agent_.wasp_attack_delay = AttackDelayFrames;
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

bool Enemy00Entity::Behavior02() {
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
        agent_.motion_index = agent_.motion_index >= agent_.wasp_max_move_index
            ? 0 : static_cast<std::uint8_t>(agent_.motion_index + 1);
        if (agent_.wasp_pattern == 0
            && agent_.authored_motion.movement_type == 3
            && agent_.motion_index == 0) {
            // A type-three wasp is on a one-way trip: getting back to the
            // start of its path is the end of it.
            agent_.health = 0;
        }
    } else if (agent_.wasp_pattern == 3 && agent_.motion_index > 0) {
        --agent_.motion_index;
    }
    const std::size_t index = std::min<std::size_t>(
        agent_.motion_index, agent_.authored_motion.path_positions.size() - 1);
    agent_.wasp_move_target = agent_.authored_motion.path_positions[index];
    StartMovingToward(agent_.wasp_move_target,
                      agent_.state == 6 ? PatrolStep
                                        : agent_.authored_motion.step_distance);
    FaceTowards(agent_.wasp_move_target);
    return true;
}

bool Enemy00Entity::Behavior03() {
    // A type-three wasp never notices anybody: it is going somewhere.
    if (agent_.authored_motion.movement_type == 3 || !PlayerAtHome()) {
        return false;
    }
    agent_.wasp_final_move_index = agent_.motion_index;
    agent_.wasp_next_pattern = 2;
    agent_.wasp_pattern = 2;
    agent_.authored_motion.step_distance = ChaseStep;
    return true;
}

bool Enemy00Entity::Behavior04() {
    if (agent_.wasp_step_count > 0) {
        --agent_.wasp_step_count;
        return false;
    }
    StartMovingToward(agent_.wasp_move_target, DiveStep);
    return true;
}

bool Enemy00Entity::Behavior05() {
    // Hit something on the way down: bounce off it and keep going.
    if (!scene_.Blocked
        || !scene_.Blocked(agent_.position,
                           add(agent_.position, agent_.velocity),
                           agent_.body_radius)) {
        return false;
    }
    StartMovingToward(agent_.wasp_move_target, DiveStep);
    FaceTowards(agent_.wasp_move_target);
    return true;
}

bool Enemy00Entity::Behavior06() {
    // Only stops once it is back at the point it left its patrol from.
    if (agent_.authored_motion.movement_type != 0
        && agent_.wasp_final_move_index != agent_.motion_index) {
        return false;
    }
    agent_.velocity = {};
    FaceTowards(PlayerPosition());
    return true;
}

bool Enemy00Entity::Behavior07() {
    // Wandered out of its own home volume: turn round.
    if (agent_.authored_motion.home_volume.contains(
            to_volume_point(agent_.position))) {
        return false;
    }
    ReachTargetOrReversePattern();
    return true;
}

bool Enemy00Entity::Behavior08() {
    // The player has left: go back to patrolling.
    if (PlayerAtHome()) {
        return false;
    }
    ReversePattern();
    return true;
}

bool Enemy00Entity::Behavior09() {
    if (agent_.wasp_attack_delay > 0) {
        --agent_.wasp_attack_delay;
        return false;
    }
    // The dive is aimed at where the player is now, and does not follow
    // them: stepping aside is the answer to it.
    agent_.wasp_attack_target = PlayerPosition();
    agent_.wasp_step_count = DiveFrames;
    return true;
}

bool Enemy00Entity::Behavior10() {
    // Lost sight of the player: give up rather than diving through a wall.
    if (main_ == nullptr || !scene_.Blocked
        || !scene_.Blocked(agent_.position, main_->position,
                           agent_.body_radius)) {
        return false;
    }
    ReachTargetOrReversePattern();
    return true;
}

void Enemy00Entity::State0() {
    MoveInCircle();
    if (agent_.authored_motion.movement_type != 0) {
        FaceTowards(agent_.wasp_move_target);
    }
    CallSubroutine();
}

void Enemy00Entity::State1() {
    FaceTowards(PlayerPosition());
    CallSubroutine();
}

void Enemy00Entity::State4() {
    if (scene_.ContactDamage && main_ != nullptr && Stung()) {
        scene_.ContactDamage(agent_, main_->slot_index, StingDamage);
        // Landed: the dive is over whether or not it had steps left.
        agent_.wasp_step_count = 0;
    }
    CallSubroutine();
}

void Enemy00Entity::State5() {
    if (scene_.ContactDamage && main_ != nullptr && Stung()) {
        scene_.ContactDamage(agent_, main_->slot_index, StingDamage);
    }
    CallSubroutine();
}

void Enemy00Entity::State6() {
    FaceTowards(agent_.wasp_move_target);
    CallSubroutine();
}

void Enemy00Entity::CallSubroutine() {
    static_cast<void>(metadata::call_subroutine(
        metadata::Enemy00Subroutines, agent_.sub_id, agent_.next_state,
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
            default: return false;
            }
        }));
}

void Enemy00Entity::CallStateProcess() {
    switch (agent_.state) {
    case 0: State0(); break;
    case 1: State1(); break;
    case 2: State2(); break;
    case 3: State3(); break;
    case 4: State4(); break;
    case 5: State5(); break;
    case 6: State6(); break;
    default: break;
    }
}

void Enemy00Entity::EnemyProcess() {
    // States four and five are the dive, which stings for twenty-five
    // instead of the three a wasp costs to bump into.
    if (agent_.state != 4 && agent_.state != 5 && scene_.ContactDamage
        && main_ != nullptr && Stung()) {
        scene_.ContactDamage(agent_, main_->slot_index, ContactDamage);
    }
    CallStateProcess();
}

} // namespace

void Session::update_warwasp(EnemyState& agent) {
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
    Enemy00Entity(scene, agent, main).EnemyProcess();
}

} // namespace fruityprime::gameplay

static_assert(fruityprime::enemy::module_00_war_wasp::kModule.managed_class.size() != 0);

namespace fruityprime::enemy::module_00_war_wasp {

void EnemyInitialize(gameplay::EnemyState& agent) noexcept {
    // Eight for the one-way kind and forty for a patroller: the type
    // three wasps are meant to be swatted as they pass.
    const bool one_way = agent.authored_motion.movement_type == 3;
    agent.health = agent.health_max = one_way ? 8 : 40;
    agent.body_radius = 1.0F;
    agent.state = agent.next_state = agent.sub_id = 0;
    agent.authored_motion.step_distance = 0.2F;
    agent.wasp_attack_delay = 30u * 2u;  // todo: FPS stuff
    agent.wasp_attack_target = agent.position;
    agent.wasp_move_target = agent.position;
    agent.authored_motion.initial_position = agent.position;
    agent.wasp_pattern = 0;
    agent.wasp_next_pattern = 0;
    if (agent.authored_motion.movement_type == 1) {
        // The square: four corners the decoder has already worked out.
        agent.motion_index = 1;
        agent.wasp_max_move_index = 3;
    } else if (agent.authored_motion.movement_type == 2 || one_way) {
        agent.motion_index = 0;
        agent.wasp_max_move_index = agent.authored_motion.path_count > 0
            ? static_cast<std::uint8_t>(agent.authored_motion.path_count - 1)
            : 0;
    }
    agent.wasp_final_move_index = agent.wasp_max_move_index;
}

} // namespace fruityprime::enemy::module_00_war_wasp

namespace fruityprime::enemy {

AuthoredMotion decode_authored_motion(
    std::uint8_t id, std::span<const std::uint8_t> fields,
    net::Vec3 origin) noexcept {
    AuthoredMotion result;
    if ((id != static_cast<std::uint8_t>(formats::EnemyType::WarWasp)
         && id != static_cast<std::uint8_t>(
             formats::EnemyType::BarbedWarWasp))
        || fields.size() < 400) {
        return result;
    }

    // S01 starts at the beginning of the union; S08 has two common scalar
    // fields before the same WarWasp payload.
    const std::size_t base = id == static_cast<std::uint8_t>(
        formats::EnemyType::BarbedWarWasp) ? 8 : 0;
    if (base + 392 > fields.size()) {
        return result;
    }

    result.supported = true;
    result.initial_position = origin;
    result.movement_type = detail::read_u32(fields, base + 388);
    result.step_distance = result.movement_type == 3 ? 0.25F : 0.2F;
    result.movement_volume = detail::read_volume(fields, base + 64, origin);
    result.home_volume = detail::read_volume(fields, base + 128, origin);

    const std::size_t path_count_offset = base + 384;
    const std::size_t path_offset = base + 192;
    if (result.movement_type == 2 || result.movement_type == 3) {
        result.path_count = static_cast<std::uint8_t>(std::min<std::uint32_t>(
            detail::read_u32(fields, path_count_offset) & 0xffu, 16u));
        for (std::size_t i = 0; i < result.path_count; ++i) {
            result.path_positions[i] = detail::add(
                origin, detail::read_vector(fields, path_offset + i * 12));
        }
    } else if (result.movement_type == 1) {
        // The managed class expands the authored box into four corners.
        // This is the S01/S08 Volume1 layout: vector3 and dot1/dot3 define
        // the two horizontal axes and position is the box center.
        const std::size_t volume = base + 64;
        if (detail::read_u32(fields, volume) == static_cast<std::uint32_t>(
                formats::VolumeType::Box)) {
            const net::Vec3 vector3 = detail::read_vector(fields, volume + 28);
            const net::Vec3 center = detail::add(
                origin, detail::read_vector(fields, volume + 40));
            const float dot1 = detail::read_fixed(fields, volume + 52);
            const float dot3 = detail::read_fixed(fields, volume + 60);
            const float xx = vector3.x * dot1;
            const float xz = vector3.x * dot3;
            const float zx = vector3.z * dot1;
            const float zz = vector3.z * dot3;
            result.path_count = 4;
            result.path_positions[0] = center;
            result.path_positions[1] = detail::add(center, {xz, 0.0F, zz});
            result.path_positions[2] = detail::add(
                center, {xz - zx, 0.0F, zz + xx});
            result.path_positions[3] = detail::add(center, {-zx, 0.0F, xx});
        }
    } else if (result.movement_type == 0) {
        const std::size_t volume = base + 128;
        if (detail::read_u32(fields, volume) == static_cast<std::uint32_t>(
                formats::VolumeType::Cylinder)) {
            result.circle_radius = std::abs(detail::read_fixed(fields, volume + 28));
        }
    }
    return result;
}

} // namespace fruityprime::enemy

