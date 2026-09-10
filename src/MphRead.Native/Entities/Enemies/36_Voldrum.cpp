// Native port of src/MphRead/Entities/Enemies/36_Voldrum.cs.
// This member definition lives in its enemy module; Session only dispatches it.
#include <array>
#include <cmath>
#include <limits>
#include "Utility/rng.hpp"
#include "Metadata/enemy_subroutines.hpp"
#include "Metadata/enemy_values.hpp"
#include "enemy_scene.hpp"
#include "36_Voldrum.hpp"
#include "enemy_common.hpp"

namespace fruityprime::gameplay {

namespace {

constexpr float Pi36 = 3.14159265358979323846F;

}  // namespace

Enemy36Entity::Enemy36Entity(const EnemyScene& scene, EnemyState& agent,
                             net::PlayerState* main) noexcept
    : Enemy35Entity(scene, agent, main) {}

const metadata::Enemy36Values& Enemy36Entity::values() const noexcept {
    const std::size_t index = std::min<std::size_t>(
        agent_.voldrum_subtype, metadata::Enemy36ValuesTable.size() - 1);
    return metadata::Enemy36ValuesTable[index];
}

// Enemy36Entity.Setup.  The managed class notes it could share the header
// half with Enemy35Entity; it does not, because what it reads the volumes
// out of is S06 rather than S00.
void Enemy36Entity::Setup() {
    const metadata::Enemy36Values& v = values();
    agent_.body_radius = 0.5F;
    agent_.health = agent_.health_max = v.HealthMax;
    agent_.scan_id = static_cast<std::uint16_t>(v.ScanId);
    // todo: FPS stuff -- every one of these is halved for this head's rate.
    const float minFactor = static_cast<float>(v.MinSpeedFactor) / 4096.0F;
    const float maxFactor = static_cast<float>(v.MaxSpeedFactor) / 4096.0F;
    agent_.voldrum_min_speed_factor = minFactor / 2.0F;
    agent_.voldrum_max_speed_factor = maxFactor / 2.0F;
    agent_.voldrum_speed_factor = minFactor / 2.0F;
    agent_.voldrum_speed_inc = v.SpeedSteps != 0
        ? (maxFactor - minFactor) / static_cast<float>(v.SpeedSteps) / 2.0F
        : 0.0F;
    agent_.voldrum_speed_inc_amount = agent_.voldrum_speed_inc;
    agent_.voldrum_delay_timer = static_cast<std::uint32_t>(v.DelayTime) * 2u;
    agent_.voldrum_shot_timer = static_cast<std::uint32_t>(v.ShotTime) * 2u;
    agent_.voldrum_shots_remaining = static_cast<std::uint16_t>(
        v.MinShots
        + utility::get_random_int2(static_cast<std::uint32_t>(
              v.MaxShots + 1 - v.MinShots)));
    agent_.voldrum_aim_step_count = static_cast<std::uint16_t>(v.AimSteps * 2);
    StartRoaming();
    agent_.state = agent_.next_state = agent_.sub_id = 1;
}

// Enemy36Entity.EnemyProcess.  The states it treats as "on the ground"
// are different from the first Voldrum's, because it has one fewer of
// them and they mean different things.
void Enemy36Entity::EnemyProcess() {
    bool sfxGrounded = true;
    if (!agent_.voldrum_grounded) {
        agent_.velocity.y -= 110.0F / 4096.0F / 4.0F;  // todo: FPS stuff
    }
    if (agent_.state == 2 || agent_.state == 3) {
        if (!Blocking(true)) {
            sfxGrounded = false;
        }
    } else if (agent_.state != 1 && agent_.state != 4) {  // 0 or 5
        if (!HandleCollision()) {
            sfxGrounded = false;
        }
    }
    if (agent_.state != 0 && agent_.state != 5) {
        ContactDamagePlayer(values().ContactDamage, true);
    }
    CallStateProcess();
    if (agent_.state != 0) {
        sfxGrounded = false;
    }
    const float top = agent_.voldrum_max_speed_factor * 2.0F;
    const float amount = top > 0.0F
        ? 65535.0F * agent_.voldrum_speed_factor * 2.0F / top : 0.0F;
    UpdateRollSfx(amount, sfxGrounded);
}

bool Enemy36Entity::HandleCollision() {
    return Enemy35Entity::HandleCollision(4, 5);
}

// Enemy36Entity.UpdateFacing.  Stop dead and look at the player, with the
// vertical clamped to half a unit of slope either way -- which is why a
// Voldrum cannot shoot at somebody directly above it and why standing on
// one is safe.
void Enemy36Entity::UpdateFacing() {
    agent_.velocity = {};
    if (main_ == nullptr) {
        return;
    }
    net::Vec3 facing{main_->position.x - agent_.position.x,
                     main_->position.y - agent_.position.y,
                     main_->position.z - agent_.position.z};
    float length = std::sqrt(facing.x * facing.x + facing.y * facing.y
                             + facing.z * facing.z);
    if (length <= 0.0F) {
        return;
    }
    facing = {facing.x / length, facing.y / length, facing.z / length};
    if (facing.y > 0.5F || facing.y < -0.5F) {
        facing.y = facing.y > 0.5F ? 0.5F : -0.5F;
        length = std::sqrt(facing.x * facing.x + facing.y * facing.y
                           + facing.z * facing.z);
        if (length > 0.0F) {
            facing = {facing.x / length, facing.y / length,
                      facing.z / length};
        }
    }
    agent_.facing = facing;
}

void Enemy36Entity::State0() {
    UpdateSpeed();
    static_cast<void>(CallSubroutine());
}

void Enemy36Entity::State1() {
    static_cast<void>(CallSubroutine());
}

void Enemy36Entity::State2() {
    UpdateFacing();
    static_cast<void>(CallSubroutine());
}

// The firing state.  Two shots at once, half a unit either side -- and
// the offset is along world x rather than the Voldrum's own right, which
// is the cartridge's own shortcut: a Voldrum shooting sideways fires from
// in front of and behind itself.
void Enemy36Entity::State3() {
    UpdateFacing();
    if (agent_.voldrum_shots_remaining > 0 && agent_.voldrum_shot_timer > 0) {
        --agent_.voldrum_shot_timer;
    } else {
        const metadata::Enemy36Values& v = values();
        if (scene_.SpawnProjectile) {
            scene_.SpawnProjectile(
                agent_, {agent_.position.x - 0.43F, agent_.position.y,
                         agent_.position.z}, agent_.facing);
            scene_.SpawnProjectile(
                agent_, {agent_.position.x + 0.43F, agent_.position.y,
                         agent_.position.z}, agent_.facing);
        }
        if (agent_.voldrum_shots_remaining > 0) {
            --agent_.voldrum_shots_remaining;
        }
        agent_.voldrum_delay_timer =
            static_cast<std::uint32_t>(v.DelayTime) * 2u;  // todo: FPS stuff
        agent_.voldrum_shot_timer =
            static_cast<std::uint32_t>(v.ShotTime) * 2u;  // todo: FPS stuff
    }
    static_cast<void>(CallSubroutine());
}

void Enemy36Entity::State4() {
    static_cast<void>(CallSubroutine());
}

void Enemy36Entity::State5() {
    State0();
}

bool Enemy36Entity::Behavior00() {
    const bool collided = HandleCollision();
    if (!SeekTargetFacing() || !collided) {
        return false;
    }
    agent_.voldrum_speed_inc = agent_.voldrum_speed_inc_amount;
    agent_.voldrum_speed_factor = agent_.voldrum_min_speed_factor;
    // Level: unlike the first Voldrum this one does not carry its
    // facing's vertical into the roll, so it cannot drive itself into the
    // floor while lined up on a player below it.
    agent_.velocity = {agent_.facing.x * agent_.voldrum_speed_factor, 0.0F,
                       agent_.facing.z * agent_.voldrum_speed_factor};
    agent_.voldrum_airborne = false;
    agent_.voldrum_time_in_air = 0;
    return true;
}

bool Enemy36Entity::Behavior01() {
    if (agent_.voldrum_delay_timer > 0) {
        --agent_.voldrum_delay_timer;
        return false;
    }
    agent_.voldrum_delay_timer =
        static_cast<std::uint32_t>(values().DelayTime) * 2u;  // todo: FPS
    return true;
}

// Out of shots: back to roaming, with a fresh magazine rolled for next
// time.
bool Enemy36Entity::Behavior02() {
    if (agent_.voldrum_shots_remaining > 0) {
        return false;
    }
    const metadata::Enemy36Values& v = values();
    PickRoamTarget();
    agent_.voldrum_delay_timer = static_cast<std::uint32_t>(v.DelayTime) * 2u;
    agent_.voldrum_shot_timer = static_cast<std::uint32_t>(v.ShotTime) * 2u;
    agent_.voldrum_shots_remaining = static_cast<std::uint16_t>(
        v.MinShots
        + utility::get_random_int2(static_cast<std::uint32_t>(
              v.MaxShots + 1 - v.MinShots)));
    return true;
}

// The hop, with the same two ways out as the first Voldrum's.
bool Enemy36Entity::Behavior03() {
    const float dx = agent_.position.x - agent_.voldrum_move_start.x;
    const float dy = agent_.position.y - agent_.voldrum_move_start.y;
    const float dz = agent_.position.z - agent_.voldrum_move_start.z;
    if (dx * dx + dy * dy + dz * dz <= agent_.voldrum_move_dist_sqr
        && (!agent_.voldrum_airborne
            || agent_.voldrum_time_in_air <= 5u * 2u)) {  // todo: FPS stuff
        return false;
    }
    PickRoamTarget();
    AimAtPlayerIfState5();
    agent_.velocity = {
        0.0F, static_cast<float>(values().JumpSpeed) / 4096.0F / 2.0F, 0.0F};
    agent_.voldrum_time_in_air = 0;
    agent_.voldrum_airborne = false;
    return true;
}

// Walked into.  It shoves the player aside and hops away rather than
// pressing the advantage, which is what keeps a Voldrum at gun range.
bool Enemy36Entity::Behavior04() {
    if (main_ == nullptr || !Touching()) {
        return false;
    }
    ContactDamagePlayer(values().ContactDamage, true);
    PickRoamTarget();
    AimAtPlayerIfState5();
    agent_.velocity = {
        0.0F, static_cast<float>(values().JumpSpeed) / 4096.0F / 2.0F, 0.0F};
    agent_.voldrum_time_in_air = 0;
    agent_.voldrum_airborne = false;
    return true;
}

// Seeing the player.  Unlike the first Voldrum this one really does test
// an angle -- but every row of the values table sets the cosine to -1, so
// in the cartridge as shipped it sees in every direction and through
// everything.  The comparison is kept because the table is data and a
// mod's row need not be -1.
bool Enemy36Entity::Behavior05() {
    if (main_ == nullptr || main_->health == 0) {
        return false;
    }
    net::Vec3 between{main_->position.x - agent_.position.x,
                      main_->position.y - agent_.position.y,
                      main_->position.z - agent_.position.z};
    const float length = std::sqrt(between.x * between.x
                                   + between.y * between.y
                                   + between.z * between.z);
    if (length > 0.0F) {
        between = {between.x / length, between.y / length,
                   between.z / length};
    }
    const float facing_dot = agent_.facing.x * between.x
        + agent_.facing.y * between.y + agent_.facing.z * between.z;
    if (facing_dot
            <= static_cast<float>(values().RangeMaxCosine) / 4096.0F) {
        return false;
    }
    agent_.velocity = {};
    return true;
}

// Hopping out of state five means it was watching a player, so it lines
// up on them again rather than on the point it just picked.  The managed
// class writes these three lines twice; they are one place here because
// they are one idea.
void Enemy36Entity::AimAtPlayerIfState5() {
    if (agent_.state != 5 || main_ == nullptr) {
        return;
    }
    net::Vec3 target{main_->position.x - agent_.position.x, 0.0F,
                     main_->position.z - agent_.position.z};
    const float length = std::sqrt(target.x * target.x + target.z * target.z);
    if (length > 0.0F) {
        target = {target.x / length, 0.0F, target.z / length};
    }
    agent_.voldrum_target_vec = target;
    const float cosine = std::clamp(
        agent_.facing.x * target.x + agent_.facing.z * target.z, -1.0F, 1.0F);
    const float angle = std::acos(cosine) * 180.0F / Pi36;
    agent_.voldrum_aim_steps = agent_.voldrum_aim_step_count;
    agent_.voldrum_aim_angle_step = agent_.voldrum_aim_steps > 0
        ? angle / static_cast<float>(agent_.voldrum_aim_steps) : angle;
}

bool Enemy36Entity::CallSubroutine() {
    return metadata::call_subroutine(
        metadata::Enemy36Subroutines, agent_.sub_id, agent_.next_state,
        [this](std::uint8_t index) {
            switch (index) {
            case 0: return Behavior00();
            case 1: return Behavior01();
            case 2: return Behavior02();
            case 3: return Behavior03();
            case 4: return Behavior04();
            case 5: return Behavior05();
            default: return false;
            }
        });
}

void Enemy36Entity::CallStateProcess() {
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

// EnemyType::Voldrum1 is the one with the gun, which is Enemy36Entity.
void Session::update_voldrum1(EnemyState& agent) {
    if (!agent.voldrum.supported || !agent.voldrum.ranged) {
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
    if (main != nullptr) {
        agent.target_slot = main->slot_index;
    }

    // EnemyInstanceEntity.BaseProcess.
    agent.state = agent.next_state;
    agent.sub_id = agent.state;
    const net::Vec3 prev_position = agent.position;
    agent.position = add(agent.position, agent.velocity);

    EnemyScene scene = build_enemy_scene(prev_position);
    scene.SpawnProjectile = [this](const EnemyState& shooter,
                                   net::Vec3 position, net::Vec3 direction) {
        spawn_enemy_projectile(shooter, position, direction);
    };
    Enemy36Entity entity(scene, agent, main);
    entity.set_frame_count(tick_count_);
    entity.EnemyProcess();
}

} // namespace fruityprime::gameplay

static_assert(fruityprime::enemy::module_36_voldrum_1::kModule.managed_class.size() != 0);

namespace fruityprime::enemy::module_36_voldrum_1 {

void EnemyInitialize(gameplay::EnemyState& agent, const std::uint8_t version,
                     const std::uint8_t subtype,
                     const std::uint16_t spawner_health) noexcept {
    // Enemy36Entity._recolors: which palette a version wears.  Kept
    // because it is authored data even though nothing native draws yet.
    static constexpr std::array<std::uint8_t, 11> Recolors{
        {0, 1, 0, 4, 0, 3, 2, 0, 0, 0, 0}};
    agent.recolor = version < Recolors.size() ? Recolors[version] : 0;
    agent.voldrum_subtype = subtype;
    gameplay::EnemyScene empty;
    gameplay::Enemy36Entity entity(empty, agent, nullptr);
    entity.set_spawner_health(spawner_health);
    entity.Setup();
}

} // namespace fruityprime::enemy::module_36_voldrum_1
