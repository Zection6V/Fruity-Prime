#include "enemy_decode_common.hpp"
// Native port of src/MphRead/Entities/Enemies/35_Voldrum.cs.
// This member definition lives in its enemy module; Session only dispatches it.
#include <cmath>
#include <limits>
#include "Utility/rng.hpp"
#include "Metadata/enemy_subroutines.hpp"
#include "enemy_scene.hpp"
#include "35_Voldrum.hpp"
#include "enemy_common.hpp"

namespace fruityprime::gameplay {

namespace {

constexpr float Pi35 = 3.14159265358979323846F;

}  // namespace

Enemy35Entity::Enemy35Entity(const EnemyScene& scene, EnemyState& agent,
                             net::PlayerState* main) noexcept
    : scene_(scene), agent_(agent), main_(main) {}

// Enemy35Entity.EnemyProcess.
void Enemy35Entity::EnemyProcess() {
    bool sfxGrounded = true;
    if (!agent_.voldrum_grounded) {
        // A quarter of the cartridge's gravity, because this head runs at
        // twice its rate and the pull is applied twice as often.
        agent_.velocity.y -= 110.0F / 4096.0F / 4.0F;  // todo: FPS stuff
    }
    if (agent_.state == 2) {
        if (!Blocking(true)) {
            sfxGrounded = false;
        }
    } else if (agent_.state == 0 || agent_.state == 6) {
        if (!HandleCollision()) {
            sfxGrounded = false;
        }
    }
    if (agent_.state != 3 && agent_.state != 4) {
        ContactDamagePlayer(2, true);
    }
    CallStateProcess();
    if (agent_.state != 0 && agent_.state != 6) {
        sfxGrounded = false;
    }
    const float amount = 65535.0F * agent_.voldrum_speed_factor * 2.0F / 0.2F;
    UpdateRollSfx(amount, sfxGrounded);
}

// Enemy35Entity.UpdateRollSfx.  The roll is a volume rather than a switch:
// it is eased towards where it should be, faster going up than coming
// down, so a Voldrum that stops rolling fades out rather than cutting off.
void Enemy35Entity::UpdateRollSfx(float newAmount, const bool grounded) {
    const float prevAmount = agent_.voldrum_roll_sfx_amount;
    if (!grounded) {
        newAmount = prevAmount * 0.5F;
    } else if (frame_count_ % 2 == 0) {  // todo: FPS stuff
        newAmount = newAmount < prevAmount
            ? prevAmount + (newAmount - prevAmount) / 4.0F
            : prevAmount + (newAmount - prevAmount) / 2.0F;
    } else {
        newAmount = prevAmount;
    }
    if (newAmount < 20.0F) {
        newAmount = 0.0F;
    }
    agent_.voldrum_roll_sfx_amount = newAmount;
}

// Enemy35Entity.HandleCollision.  The virtual one: the second Voldrum
// names different states.
bool Enemy35Entity::HandleCollision() {
    return HandleCollision(5, 6);
}

// Enemy35Entity.HandleCollision(int, int).
//
// This is not the blocking collision every other enemy uses.  It reads
// every face within the body radius and pushes out of each in turn, and
// what it does with a *wall* is the interesting part: outside the two
// states named here, hitting one sends the Voldrum backwards and up --
// it bounces off.  In those two it just counts the frames it has been in
// the air, which is what the hop's own exit condition reads.
bool Enemy35Entity::HandleCollision(const int stateA, const int stateB) {
    agent_.voldrum_grounded = false;
    if (!scene_.CheckInRadius) {
        return false;
    }
    std::array<collision::Result, 30> results{};
    const std::size_t count = scene_.CheckInRadius(
        agent_.position, agent_.body_radius, results);
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
                - (agent_.position.x * normal.x + agent_.position.y * normal.y
                   + agent_.position.z * normal.z);
        if (depth <= 0.0F) {
            continue;
        }
        agent_.position = {agent_.position.x + normal.x * depth,
                           agent_.position.y + normal.y * depth,
                           agent_.position.z + normal.z * depth};
        if (result.plane.y >= 0.1F || result.plane.y <= -0.1F) {
            agent_.voldrum_grounded = true;
        } else if (agent_.state != 1
                   && agent_.state != static_cast<std::uint8_t>(stateA)) {
            agent_.voldrum_airborne = true;
            if (agent_.state != 0
                && agent_.state != static_cast<std::uint8_t>(stateB)) {
                agent_.velocity = {-agent_.velocity.x, -agent_.velocity.y,
                                   -agent_.velocity.z};
                agent_.velocity.y = 1000.0F / 4096.0F / 2.0F;  // todo: FPS
            } else {
                ++agent_.voldrum_time_in_air;
            }
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

// Enemy35Entity.PickRoamTarget.  A point somewhere inside the home
// cylinder, at the height the Voldrum is already at.  The angle's sign
// alternates every time, so it does not spiral round the cylinder in one
// direction -- it works its way back and forth across it.
void Enemy35Entity::PickRoamTarget() {
    const auto& home = agent_.voldrum.home_volume;
    // Fixed.ToInt of the radius: the roll is over fixed-point units, and
    // the result is read back as fixed point, so the two cancel and what
    // comes out is a distance in [0, radius).
    const auto radius_limit = static_cast<std::uint32_t>(std::max(
        1.0F, home.cylinder_radius * 4096.0F));
    const float dist =
        static_cast<float>(utility::get_random_int2(radius_limit)) / 4096.0F;
    agent_.voldrum_roam_angle_sign =
        static_cast<std::int8_t>(-agent_.voldrum_roam_angle_sign);
    // [0, 180), either way round.
    const float angle =
        static_cast<float>(utility::get_random_int2(0xB4000u)) / 4096.0F
        * static_cast<float>(agent_.voldrum_roam_angle_sign) * Pi35 / 180.0F;
    // A rotation about Y of (dist, 0, 0): x picks up the cosine, z the
    // negative sine, which is what Matrix4.CreateRotationY does to a
    // vector down the x axis.
    const net::Vec3 vec{dist * std::cos(angle), 0.0F, -dist * std::sin(angle)};
    UpdateMoveTarget({home.cylinder_position.x + vec.x, agent_.position.y,
                      home.cylinder_position.z + vec.z});
}

// Enemy35Entity.UpdateMoveTarget: everything about one journey, decided
// when it starts.  The turn is spread over a fixed number of steps rather
// than a fixed rate, so a Voldrum that has to turn right round takes the
// same time to line up as one that barely has to turn at all.
void Enemy35Entity::UpdateMoveTarget(const net::Vec3 targetPoint) {
    agent_.voldrum_move_target = targetPoint;
    agent_.voldrum_move_target_valid = true;
    agent_.voldrum_move_start = agent_.position;
    net::Vec3 target_vec{targetPoint.x - agent_.position.x,
                         targetPoint.y - agent_.position.y,
                         targetPoint.z - agent_.position.z};
    agent_.voldrum_move_dist_sqr = target_vec.x * target_vec.x
        + target_vec.y * target_vec.y + target_vec.z * target_vec.z;
    agent_.voldrum_move_dist_sqr_half = agent_.voldrum_move_dist_sqr / 2.0F;
    agent_.voldrum_increase_speed = true;
    const float length = std::sqrt(agent_.voldrum_move_dist_sqr);
    if (length > 0.0F) {
        target_vec = {target_vec.x / length, target_vec.y / length,
                      target_vec.z / length};
    }
    agent_.voldrum_target_vec = target_vec;
    const float cosine = std::clamp(
        agent_.facing.x * target_vec.x + agent_.facing.y * target_vec.y
            + agent_.facing.z * target_vec.z, -1.0F, 1.0F);
    const float angle = std::acos(cosine) * 180.0F / Pi35;
    agent_.voldrum_aim_steps = agent_.voldrum_aim_step_count;
    agent_.voldrum_aim_angle_step = agent_.voldrum_aim_steps > 0
        ? angle / static_cast<float>(agent_.voldrum_aim_steps) : angle;
}

// Enemy35Entity.UpdateSpeed.  It accelerates until it is halfway there
// and decelerates after, measured against the distance the journey was
// when it started -- so a long trip has a long run-up and a short one
// never gets going.
void Enemy35Entity::UpdateSpeed() {
    if (agent_.voldrum_increase_speed) {
        const float dx = agent_.voldrum_move_target.x - agent_.position.x;
        const float dy = agent_.voldrum_move_target.y - agent_.position.y;
        const float dz = agent_.voldrum_move_target.z - agent_.position.z;
        if (dx * dx + dy * dy + dz * dz
                < agent_.voldrum_move_dist_sqr_half) {
            agent_.voldrum_increase_speed = false;
        }
    }
    if (agent_.voldrum_increase_speed) {
        agent_.voldrum_speed_factor += agent_.voldrum_speed_inc;
        if (agent_.voldrum_speed_factor > agent_.voldrum_max_speed_factor) {
            agent_.voldrum_speed_factor = agent_.voldrum_max_speed_factor;
        }
    } else {
        agent_.voldrum_speed_factor -= agent_.voldrum_speed_inc;
        if (agent_.voldrum_speed_factor < agent_.voldrum_min_speed_factor) {
            agent_.voldrum_speed_factor = agent_.voldrum_min_speed_factor;
        }
    }
    agent_.velocity.x = agent_.facing.x * agent_.voldrum_speed_factor;
    agent_.velocity.z = agent_.facing.z * agent_.voldrum_speed_factor;
}

void Enemy35Entity::State0() {
    UpdateSpeed();
    static_cast<void>(CallSubroutine());
}

void Enemy35Entity::State1() {
    static_cast<void>(CallSubroutine());
}

void Enemy35Entity::State2() {
    if (main_ != nullptr) {
        net::Vec3 facing{main_->position.x - agent_.position.x, 0.0F,
                         main_->position.z - agent_.position.z};
        const float length = std::sqrt(facing.x * facing.x
                                       + facing.z * facing.z);
        if (length > 0.0F) {
            agent_.facing = {facing.x / length, 0.0F, facing.z / length};
            agent_.up = {0.0F, 1.0F, 0.0F};
        }
    }
    static_cast<void>(CallSubroutine());
}

// The managed comment says this could be part of UpdateSpeed; it is the
// deceleration half of it and nothing else.
void Enemy35Entity::State3() {
    agent_.voldrum_speed_factor -= agent_.voldrum_speed_inc;
    if (agent_.voldrum_speed_factor < agent_.voldrum_min_speed_factor) {
        agent_.voldrum_speed_factor = agent_.voldrum_min_speed_factor;
    }
    agent_.velocity.x = agent_.facing.x * agent_.voldrum_speed_factor;
    agent_.velocity.z = agent_.facing.z * agent_.voldrum_speed_factor;
    static_cast<void>(CallSubroutine());
}

void Enemy35Entity::State4() {
    // The managed class notes this cannot be reached: setting the first
    // flag clears the second in the same breath.  Kept as written rather
    // than simplified away, because the branch that cannot happen is part
    // of what the state machine says.
    if (agent_.voldrum_handled_ram_col && agent_.voldrum_ram_damage_needed) {
        if (scene_.ContactDamage && main_ != nullptr) {
            scene_.ContactDamage(agent_, main_->slot_index, 15);
        }
        agent_.voldrum_ram_damage_needed = false;
    } else {
        static_cast<void>(CallSubroutine());
    }
}

void Enemy35Entity::State5() {
    static_cast<void>(CallSubroutine());
}

void Enemy35Entity::State6() {
    State0();
}

// Lined up with where it is going, and standing on something: set off.
bool Enemy35Entity::Behavior00() {
    const bool collided = HandleCollision();
    if (!SeekTargetFacing() || !collided) {
        return false;
    }
    agent_.voldrum_speed_inc = agent_.voldrum_speed_inc_amount;
    agent_.voldrum_speed_factor = agent_.voldrum_min_speed_factor;
    agent_.velocity = {agent_.facing.x * agent_.voldrum_speed_factor,
                       agent_.facing.y * agent_.voldrum_speed_factor,
                       agent_.facing.z * agent_.voldrum_speed_factor};
    return true;
}

bool Enemy35Entity::Behavior01() {
    if (!HandleCollision()) {
        return false;
    }
    PickRoamTarget();
    agent_.voldrum_ram_damage_needed = true;
    agent_.voldrum_handled_ram_col = false;
    agent_.voldrum_speed_inc = agent_.voldrum_speed_inc_amount;
    agent_.voldrum_speed_factor = agent_.voldrum_min_speed_factor;
    agent_.velocity = {agent_.facing.x * agent_.voldrum_speed_factor,
                       agent_.facing.y * agent_.voldrum_speed_factor,
                       agent_.facing.z * agent_.voldrum_speed_factor};
    return true;
}

// The hop.  Two ways out: it has gone as far as the journey was, or it
// has been bouncing off a wall for more than five frames -- which is what
// gets it unstuck when the point it picked is behind something.
bool Enemy35Entity::Behavior02() {
    const float dx = agent_.position.x - agent_.voldrum_move_start.x;
    const float dy = agent_.position.y - agent_.voldrum_move_start.y;
    const float dz = agent_.position.z - agent_.voldrum_move_start.z;
    if (dx * dx + dy * dy + dz * dz <= agent_.voldrum_move_dist_sqr
        && (!agent_.voldrum_airborne
            || agent_.voldrum_time_in_air <= 5u * 2u)) {  // todo: FPS stuff
        return false;
    }
    PickRoamTarget();
    agent_.velocity = {0.0F, 0.2F / 2.0F, 0.0F};  // todo: FPS stuff
    agent_.voldrum_time_in_air = 0;
    agent_.voldrum_airborne = false;
    return true;
}

// Prepare to ram: forty frames of facing the player, then a charge at
// three times the normal top speed that slows down very gradually.
bool Enemy35Entity::Behavior03() {
    if (agent_.voldrum_ram_delay > 0) {
        --agent_.voldrum_ram_delay;
        return false;
    }
    if (main_ == nullptr) {
        return false;
    }
    net::Vec3 facing{main_->position.x - agent_.position.x, 0.0F,
                     main_->position.z - agent_.position.z};
    const float facing_length = std::sqrt(facing.x * facing.x
                                          + facing.z * facing.z);
    if (facing_length > 0.0F) {
        agent_.facing = {facing.x / facing_length, 0.0F,
                         facing.z / facing_length};
        agent_.up = {0.0F, 1.0F, 0.0F};
    }
    agent_.voldrum_time_in_air = 0;
    agent_.voldrum_airborne = false;
    agent_.voldrum_move_target = {main_->position.x, agent_.position.y,
                                  main_->position.z};
    agent_.voldrum_move_target_valid = true;
    net::Vec3 target_vec{
        agent_.voldrum_move_target.x - agent_.position.x, 0.0F,
        agent_.voldrum_move_target.z - agent_.position.z};
    agent_.voldrum_move_dist_sqr = target_vec.x * target_vec.x
        + target_vec.z * target_vec.z;
    agent_.voldrum_move_dist_sqr_half = agent_.voldrum_move_dist_sqr / 2.0F;
    agent_.voldrum_target_vec = target_vec;
    agent_.voldrum_speed_inc = 0.005F / 2.0F;  // todo: FPS stuff
    agent_.voldrum_speed_factor = 0.6F / 2.0F;  // todo: FPS stuff
    agent_.voldrum_ram_delay = 40u * 2u;  // todo: FPS stuff
    return true;
}

// The charge is over when it has hit somebody or hit a wall.  Either way
// it bounces back and up, which is what gives the player the moment to
// get out of the way of the next one.
bool Enemy35Entity::Behavior04() {
    const bool collided = HandleCollision();
    if (!agent_.voldrum_handled_ram_col && main_ != nullptr
        && Touching()) {
        if (scene_.ContactDamage) {
            scene_.ContactDamage(agent_, main_->slot_index, 15);
        }
        agent_.voldrum_handled_ram_col = true;
        agent_.voldrum_ram_damage_needed = false;
        agent_.velocity = {-agent_.velocity.x, -agent_.velocity.y,
                           -agent_.velocity.z};
        agent_.velocity.y = 1000.0F / 4096.0F / 2.0F;  // todo: FPS stuff
        return true;
    }
    if (collided && agent_.voldrum_airborne) {
        agent_.voldrum_airborne = false;
        agent_.voldrum_time_in_air = 0;
        return true;
    }
    return false;
}

// Also the hop, from the other state.  It only hops once it has reached
// full speed *and* left its cylinder -- so a Voldrum whose point is
// inside the cylinder rolls the whole way rather than hopping there.
bool Enemy35Entity::Behavior05() {
    if (agent_.voldrum_speed_factor != agent_.voldrum_max_speed_factor
        && agent_.voldrum.home_volume.contains(
            to_volume_point(agent_.position))) {
        return false;
    }
    PickRoamTarget();
    agent_.velocity = {0.0F, 0.2F / 2.0F, 0.0F};  // todo: FPS stuff
    return true;
}

// Seeing the player.  The dot product against the facing could have been
// a field of view; written against -1 it can never fail, so what actually
// decides this is whether the player is inside the Voldrum's cylinder.
bool Enemy35Entity::Behavior06() {
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
    if (facing_dot <= -1.0F
        || !agent_.voldrum.home_volume.contains(
            to_volume_point(main_->position))) {
        return false;
    }
    agent_.velocity = {};
    return true;
}

bool Enemy35Entity::CallSubroutine() {
    return metadata::call_subroutine(
        metadata::Enemy35Subroutines, agent_.sub_id, agent_.next_state,
        [this](std::uint8_t index) {
            switch (index) {
            case 0: return Behavior00();
            case 1: return Behavior01();
            case 2: return Behavior02();
            case 3: return Behavior03();
            case 4: return Behavior04();
            case 5: return Behavior05();
            case 6: return Behavior06();
            default: return false;
            }
        });
}

void Enemy35Entity::CallStateProcess() {
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

bool Enemy35Entity::Blocking(const bool update_speed) {
    if (!scene_.BlockingCollision) {
        return false;
    }
    const EnemyScene::Blocking blocking = scene_.BlockingCollision(
        agent_, agent_.voldrum.hurt_volume, update_speed);
    agent_.voldrum_grounded = blocking.with_ground;
    return blocking.any;
}

bool Enemy35Entity::SeekTargetFacing() {
    return scene_.SeekFacing
        && scene_.SeekFacing(agent_, agent_.voldrum_target_vec,
                             agent_.voldrum_aim_steps,
                             agent_.voldrum_aim_angle_step);
}

bool Enemy35Entity::Touching() const noexcept {
    if (main_ == nullptr) {
        return false;
    }
    const float reach = agent_.body_radius + 0.45F;
    return distance_squared(agent_.position, main_->position)
        <= reach * reach;
}

void Enemy35Entity::ContactDamagePlayer(const std::uint32_t damage,
                                        const bool knockback) {
    if (!Touching() || main_ == nullptr) {
        return;
    }
    if (knockback) {
        // Away from the enemy, and weaker the further away the player is
        // -- the cartridge divides by five times the distance rather than
        // multiplying by it, which is the opposite of what a shove
        // normally does and is what stops a graze launching anybody.
        const float dx = main_->position.x - agent_.position.x;
        const float dy = main_->position.y - agent_.position.y;
        const float dz = main_->position.z - agent_.position.z;
        const float mag = std::sqrt(dx * dx + dy * dy + dz * dz) * 5.0F;
        if (mag > 0.0F) {
            main_->speed.x += dx / mag;
            main_->speed.z += dz / mag;
        }
    }
    if (scene_.ContactDamage) {
        scene_.ContactDamage(agent_, main_->slot_index, damage);
    }
}

// Enemy35Entity.EnemyInitialize is Setup, which Enemy36Entity overrides.
void Enemy35Entity::Setup() {
    agent_.body_radius = 0.5F;
    agent_.health = agent_.health_max = 42;
    agent_.voldrum_speed_inc = agent_.voldrum_speed_inc_amount;
    agent_.voldrum_speed_factor = agent_.voldrum_min_speed_factor;
    agent_.voldrum_ram_damage_needed = true;
    agent_.voldrum_ram_delay = 40u * 2u;  // todo: FPS stuff
    StartRoaming();
    agent_.state = agent_.next_state = agent_.sub_id = 1;
}

// The tail both Setups share: a Voldrum that was placed on its cylinder's
// axis roams from the start, and one placed off it walks to the middle
// first.  A spawner with no health of its own also roams -- that is the
// endlessly-respawning kind, whose children should not all converge.
void Enemy35Entity::StartRoaming() {
    const auto& home = agent_.voldrum.home_volume;
    const bool on_axis =
        std::fabs(agent_.position.x - home.cylinder_position.x) < 1.0F / 4096.0F
        && std::fabs(agent_.position.z - home.cylinder_position.z)
            < 1.0F / 4096.0F;
    if (spawner_health_ == 0 || on_axis) {
        PickRoamTarget();
    } else {
        UpdateMoveTarget({home.cylinder_position.x, agent_.position.y,
                          home.cylinder_position.z});
    }
}

// EnemyType::Voldrum2 is the melee one, which is Enemy35Entity: the
// cartridge's two names run the other way round from the file numbers.
void Session::update_voldrum2(EnemyState& agent) {
    if (!agent.voldrum.supported || agent.voldrum.ranged) {
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

    const EnemyScene scene = build_enemy_scene(prev_position);
    Enemy35Entity entity(scene, agent, main);
    entity.set_frame_count(tick_count_);
    entity.EnemyProcess();
}

// The name the older dispatch used, kept so nothing calling it silently
// stops working.
void Session::update_voldrum(EnemyState& agent) {
    if (agent.voldrum.ranged) {
        update_voldrum1(agent);
    } else {
        update_voldrum2(agent);
    }
}

} // namespace fruityprime::gameplay

static_assert(fruityprime::enemy::module_35_voldrum_2::kModule.managed_class.size() != 0);

namespace fruityprime::enemy::module_35_voldrum_2 {

void EnemyInitialize(gameplay::EnemyState& agent,
                     const std::uint16_t spawner_health) noexcept {
    gameplay::EnemyScene empty;
    gameplay::Enemy35Entity entity(empty, agent, nullptr);
    entity.set_spawner_health(spawner_health);
    entity.Setup();
}

} // namespace fruityprime::enemy::module_35_voldrum_2

namespace fruityprime::enemy {

VoldrumProfile decode_voldrum_profile(
    std::uint8_t id, std::span<const std::uint8_t> fields,
    net::Vec3 origin) noexcept {
    VoldrumProfile result;
    const auto voldrum2 = static_cast<std::uint8_t>(
        formats::EnemyType::Voldrum2);
    const auto voldrum1 = static_cast<std::uint8_t>(
        formats::EnemyType::Voldrum1);
    if (id == voldrum2) {
        if (fields.size() < 2 * 64) {
            return result;
        }
        result.variant = id;
        result.hurt_volume = detail::read_volume(fields, 0, origin);
        result.home_volume = detail::read_volume(fields, 64, origin);
        result.health = 42;
        result.contact_damage = 2;
        result.min_speed_factor = 0.1F / 2.0F;
        result.max_speed_factor = 0.2F / 2.0F;
        result.speed_increment = 410.0F / 4096.0F
            / 3.0F / 2.0F;
        result.jump_speed = 1000.0F / 4096.0F / 2.0F;
        result.aim_steps = 10;
        result.supported = result.hurt_volume.kind != scene::VolumeKind::Invalid
            && result.home_volume.kind != scene::VolumeKind::Invalid;
        return result;
    }
    if (id != voldrum1 || fields.size() < 2 * 64 + 8) {
        return result;
    }

    result.ranged = true;
    result.variant = static_cast<std::uint8_t>(std::min<std::uint32_t>(
        detail::read_u32(fields, 0), 4u));
    result.version = std::min<std::uint32_t>(detail::read_u32(fields, 4), 10u);
    // Enemy51Entity does not read a CollisionVolume from S07. It constructs a
    // fixed local sphere at (0, 409/4096, 0), then transforms it by the
    // spawner transform. Keep that authored detail instead of interpreting the
    // following bytes as a generic volume union.
    result.hurt_volume.kind = scene::VolumeKind::Sphere;
    result.hurt_volume.sphere_position = {
        origin.x, origin.y + 409.0F / 4096.0F, origin.z};
    result.hurt_volume.sphere_radius = 1843.0F / 4096.0F;
    result.home_volume = detail::read_volume(fields, 72, origin);

    struct Values {
        std::uint16_t health;
        std::uint16_t beam_damage;
        std::uint16_t splash_damage;
        std::uint16_t contact_damage;
        std::int32_t min_speed_factor;
        std::int32_t max_speed_factor;
        std::uint16_t delay_frames;
        std::uint16_t shot_frames;
        std::uint16_t min_shots;
        std::uint16_t max_shots;
        std::int32_t jump_speed;
        std::int32_t range_max_cosine;
        std::uint16_t aim_steps;
    };
    static constexpr std::array<Values, 5> values{{
        {55, 2, 0, 7, 819, 1433, 40, 15, 1, 2,
         819, -4096, 10},
        {100, 5, 1, 7, 819, 1433, 25, 8, 2, 3,
         819, -4096, 10},
        {150, 10, 2, 7, 819, 1433, 30, 25, 1, 1,
         819, -4096, 10},
        {150, 8, 2, 7, 1228, 2048, 25, 20, 1, 2,
         819, -4096, 10},
        {152, 8, 0, 7, 409, 1024, 25, 40, 1, 2,
         819, -4096, 10}
    }};
    const Values& selected = values[result.variant];
    result.health = selected.health;
    result.beam_damage = selected.beam_damage;
    result.splash_damage = selected.splash_damage;
    result.contact_damage = selected.contact_damage;
    result.min_speed_factor = static_cast<float>(selected.min_speed_factor)
        / 4096.0F / 2.0F;
    result.max_speed_factor = static_cast<float>(selected.max_speed_factor)
        / 4096.0F / 2.0F;
    result.speed_increment = (result.max_speed_factor
                              - result.min_speed_factor)
        / 7.0F;
    result.jump_speed = static_cast<float>(selected.jump_speed)
        / 4096.0F / 2.0F;
    result.range_max_cosine = static_cast<float>(selected.range_max_cosine)
        / 4096.0F;
    result.delay_frames = selected.delay_frames;
    result.shot_frames = selected.shot_frames;
    result.min_shots = selected.min_shots;
    result.max_shots = selected.max_shots;
    result.aim_steps = selected.aim_steps;

    // EnemyWeapons uses BeamType ordinals, while the native projectile record
    // uses the compact gameplay weapon slots. The first three versions are
    // the Voldrum variants present in the retail rooms; retain their exact
    // enemy draw/effect values rather than borrowing the player table.
    result.projectile_weapon = metadata::native_weapon_slot_from_beam(
        static_cast<std::int32_t>(result.version));
    struct ProjectileValues {
        std::uint8_t draw_func;
        std::uint16_t color;
        std::uint8_t collision_effect;
        std::uint8_t muzzle_effect;
        float speed;
        float lifetime;
    };
    static constexpr std::array<ProjectileValues, 3> projectile_values{{
        {21, 9055, 242, 65, 2662.0F / 4096.0F * 60.0F,
         255.0F / 60.0F},
        {2, 32767, 89, 60, 3276.0F / 4096.0F * 60.0F,
         90.0F / 60.0F},
        {7, 32140, 8, 65, 819.0F / 4096.0F * 60.0F,
         255.0F / 60.0F}
    }};
    if (result.version < projectile_values.size()) {
        const auto& projectile = projectile_values[result.version];
        result.projectile_draw_func = projectile.draw_func;
        result.projectile_color = projectile.color;
        result.projectile_collision_effect = projectile.collision_effect;
        result.projectile_muzzle_effect = projectile.muzzle_effect;
        result.projectile_speed = projectile.speed;
        result.projectile_lifetime = projectile.lifetime;
    } else {
        if (result.projectile_weapon == 0xff) {
            result.projectile_weapon = 0;
        }
        const auto& visual = metadata::weapon_visual_info(
            result.projectile_weapon);
        result.projectile_draw_func = visual.draw_func_ids[0];
        result.projectile_color = visual.colors[0];
        result.projectile_collision_effect = visual.collision_effects[0];
        result.projectile_muzzle_effect = visual.muzzle_effects[0];
        result.projectile_speed = metadata::weapon_info(
            result.projectile_weapon).projectile_speed;
        result.projectile_lifetime = metadata::weapon_info(
            result.projectile_weapon).lifetime_seconds;
    }
    result.supported = result.hurt_volume.kind != scene::VolumeKind::Invalid
        && result.home_volume.kind != scene::VolumeKind::Invalid;
    return result;
}

} // namespace fruityprime::enemy

