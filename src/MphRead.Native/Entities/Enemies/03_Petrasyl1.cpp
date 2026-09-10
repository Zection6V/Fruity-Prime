#include "Utility/rng.hpp"
#include <cmath>
#include "Metadata/enemy_subroutines.hpp"
#include "enemy_scene.hpp"
#include "enemy_decode_common.hpp"
// Native port of src/MphRead/Entities/Enemies/03_Petrasyl1.cs.
// This member definition lives in its enemy module; Session only dispatches it.
#include "03_Petrasyl1.hpp"
#include "enemy_common.hpp"

namespace fruityprime::gameplay {

void Session::update_petrasyl(EnemyState& agent) {
    const auto variant = agent.petrasyl.variant;
    const auto petrasyl1 = static_cast<std::uint8_t>(
        formats::EnemyType::Petrasyl1);
    const auto petrasyl2 = static_cast<std::uint8_t>(
        formats::EnemyType::Petrasyl2);
    const auto petrasyl3 = static_cast<std::uint8_t>(
        formats::EnemyType::Petrasyl3);
    const auto petrasyl4 = static_cast<std::uint8_t>(
        formats::EnemyType::Petrasyl4);
    if (variant < petrasyl1 || variant > petrasyl4
        || !agent.petrasyl.supported) {
        return;
    }

    const float seconds = config_.tick_seconds;
    const float frames = std::max(1.0F, std::round(
        std::max(0.0F, seconds * 60.0F)));
    const auto frame_step = static_cast<std::uint32_t>(frames);
    agent.attack_timer = std::max(0.0F, agent.attack_timer - seconds);

    std::size_t target_index = players_.size();
    float target_distance = std::numeric_limits<float>::max();
    for (std::size_t index = 0; index < players_.size(); ++index) {
        const auto& player = players_[index];
        if (!objective_player(player)) {
            continue;
        }
        const float distance = distance_squared(player.position,
                                                agent.position);
        if (distance >= target_distance) {
            continue;
        }
        target_distance = distance;
        target_index = index;
    }

    const auto decrement = [frame_step](std::uint32_t& value) noexcept {
        value = value > frame_step ? value - frame_step : 0;
    };
    const auto random_signed = [this](std::uint32_t range) noexcept {
        return static_cast<float>(rng_.random2(range)) / 4096.0F - 0.5F;
    };
    const auto random_positive = [this](std::uint32_t range) noexcept {
        return static_cast<float>(rng_.random2(range)) / 4096.0F;
    };
    const auto smooth_facing = [&agent](net::Vec3 desired) {
        desired.y = 0.0F;
        desired = normalized_or(desired, agent.facing);
        const net::Vec3 previous = agent.facing;
        net::Vec3 next = previous;
        next.x += (desired.x - previous.x) / 16.0F;
        next.z += (desired.z - previous.z) / 16.0F;
        next.y = 0.0F;
        next = normalized_or(next, previous);
        if (std::abs(next.x - previous.x) < 1.0F / 4096.0F
            && std::abs(next.z - previous.z) < 1.0F / 4096.0F) {
            next.x += 0.0625F;
            next.z -= 0.0625F;
            next = normalized_or(next, previous);
        }
        agent.facing = next;
    };
    const auto apply_contact = [this, &agent](std::uint16_t damage) {
        constexpr float contact_radius = 1.25F;
        if (agent.attack_timer > 0.0F) {
            return;
        }
        for (auto& player : players_) {
            if (!objective_player(player)
                || distance_squared(player.position, agent.position)
                    > contact_radius * contact_radius) {
                continue;
            }
            apply_enemy_contact_damage(agent, player, damage);
            agent.attack_timer = 0.5F;
            break;
        }
    };

    if (variant == petrasyl1) {
        if (agent.state == 0) {
            agent.velocity = {};
            decrement(agent.petrasyl_timer);
            if (agent.petrasyl_timer == 0) {
                agent.state = 1;
                agent.invulnerable = false;
                const float travel = std::max(0.0F,
                    agent.petrasyl.idle_range.z);
                agent.petrasyl_secondary_timer = static_cast<std::uint32_t>(
                    std::max(0.0F, std::floor(travel / 0.7F))) * 2u;
            }
            return;
        }
        if (agent.state == 2) {
            agent.velocity = {};
            decrement(agent.petrasyl_secondary_timer);
            if (agent.petrasyl_secondary_timer == 0) {
                if (agent.petrasyl_teleport_initial) {
                    agent.position = agent.petrasyl_initial_position;
                    agent.petrasyl_teleport_initial = false;
                } else {
                    agent.position = agent.petrasyl_idle_limit;
                    agent.petrasyl_teleport_initial = true;
                }
                agent.petrasyl_direction = multiply(
                    agent.petrasyl_direction, -1.0F);
                agent.petrasyl_timer = 20u * 2u;
                agent.invulnerable = true;
                agent.state = 0;
            }
            return;
        }

        // Enemy03Entity.State01: bob on the authored line, turn toward the
        // player, then teleport out after the idle travel counter expires.
        agent.petrasyl_bob_angle += agent.petrasyl_bob_speed
            * 0.5F * frames;
        if (agent.petrasyl_bob_angle >= 360.0F) {
            agent.petrasyl_bob_angle = std::fmod(
                agent.petrasyl_bob_angle, 360.0F);
        }
        const float bob = std::sin(agent.petrasyl_bob_angle
                                   * 3.14159265358979323846F / 180.0F);
        const float desired_y = agent.petrasyl_initial_position.y
            + bob * agent.petrasyl_bob_offset;
        const float horizontal_step = 0.7F / 2.0F * frames;
        const float vertical_step = (desired_y - agent.position.y)
            / 2.0F * frames;
        const net::Vec3 delta = add(
            multiply(agent.petrasyl_direction, horizontal_step),
            {0.0F, vertical_step, 0.0F});
        agent.position = add(agent.position, delta);
        agent.velocity = seconds > 0.0F
            ? multiply(delta, 1.0F / seconds) : net::Vec3{};
        if (target_index != players_.size()) {
            const auto between = subtract(
                players_[target_index].position, agent.position);
            const auto desired = length_squared(between) >= 7.0F * 7.0F
                ? agent.petrasyl_direction
                : net::Vec3{between.x, 0.0F, between.z};
            smooth_facing(desired);
        } else {
            smooth_facing(agent.petrasyl_direction);
        }
        apply_contact(12);
        decrement(agent.petrasyl_secondary_timer);
        if (agent.petrasyl_secondary_timer == 0) {
            agent.velocity = {};
            agent.invulnerable = true;
            agent.state = 2;
            agent.petrasyl_secondary_timer = 20u * 2u;
        }
        return;
    }

    if (variant == petrasyl2) {
        // Enemy04Entity has a one-frame state-0 setup; the native spawn starts
        // directly in its perpetual weave state.
        agent.state = 1;
        const net::Vec3 to_spawner = subtract(agent.behavior_origin,
                                               agent.position);
        net::Vec3 lateral{-to_spawner.z, 0.0F, to_spawner.x};
        lateral = normalized_or(lateral, agent.facing);
        agent.petrasyl_weave_angle += 0.75F * frames;
        if (agent.petrasyl_weave_angle >= 360.0F) {
            agent.petrasyl_weave_angle = std::fmod(
                agent.petrasyl_weave_angle, 360.0F);
        }
        const float angle = agent.petrasyl_weave_angle
            * 3.14159265358979323846F / 180.0F;
        const net::Vec3 weave_target = add(
            agent.petrasyl_initial_position,
            {std::sin(angle) * agent.petrasyl.weave_offset, 0.0F,
             std::cos(angle) * agent.petrasyl.weave_offset});
        const float bob_angle = agent.petrasyl_bob_angle
            + agent.petrasyl_bob_speed * 0.5F * frames;
        agent.petrasyl_bob_angle = std::fmod(bob_angle, 360.0F);
        const float bob = std::sin(agent.petrasyl_bob_angle
                                   * 3.14159265358979323846F / 180.0F);
        const float desired_y = agent.petrasyl_initial_position.y
            + bob * agent.petrasyl_bob_offset;
        const net::Vec3 delta{
            (weave_target.x - agent.position.x) / 2.0F * frames,
            (desired_y - agent.position.y) / 2.0F * frames,
            (weave_target.z - agent.position.z) / 2.0F * frames};
        agent.position = add(agent.position, delta);
        agent.velocity = seconds > 0.0F
            ? multiply(delta, 1.0F / seconds) : net::Vec3{};
        const net::Vec3 between = target_index == players_.size()
            ? net::Vec3{} : subtract(players_[target_index].position,
                                     agent.position);
        const net::Vec3 desired = target_index == players_.size()
            || length_squared(between) >= 7.0F * 7.0F
            ? lateral : net::Vec3{between.x, 0.0F, between.z};
        smooth_facing(desired);
        apply_contact(12);
        return;
    }

    const bool is_petrasyl4 = variant == petrasyl4;
    const auto player_is_close = [&]() noexcept {
        return target_index != players_.size()
            && target_distance < 6.0F * 6.0F;
    };
    const auto enter_state = [&agent, is_petrasyl4](std::uint8_t state) {
        agent.state = state;
        if (!is_petrasyl4) {
            return;
        }
        switch (state) {
        case 0:
            agent.visible = true;
            agent.invulnerable = true;
            agent.velocity = {};
            agent.petrasyl_timer = 10u * 2u;
            break;
        case 1:
            agent.visible = true;
            agent.invulnerable = false;
            break;
        case 2:
            agent.visible = true;
            agent.invulnerable = true;
            agent.petrasyl_secondary_timer = 10u * 2u;
            break;
        case 3:
            agent.visible = false;
            agent.invulnerable = true;
            break;
        case 4:
            agent.visible = true;
            agent.invulnerable = true;
            agent.petrasyl_timer = 10u * 2u;
            break;
        default:
            agent.visible = true;
            agent.invulnerable = true;
            agent.petrasyl_timer = 10u * 2u;
            agent.state = 0;
            break;
        }
    };
    const auto update_wander = [&]() {
        const auto& profile = agent.petrasyl;
        const net::Vec3 to_target{
            agent.position.x - agent.petrasyl_initial_position.x,
            agent.petrasyl_target_y - agent.petrasyl_initial_position.y,
            agent.position.z - agent.petrasyl_initial_position.z};
        agent.petrasyl_bob_angle += agent.petrasyl_bob_speed
            * 0.5F * frames;
        if (agent.petrasyl_bob_angle >= 360.0F) {
            agent.petrasyl_target_y = agent.position.y;
            agent.petrasyl_bob_angle = std::fmod(
                agent.petrasyl_bob_angle, 360.0F);
        }
        const float bob = std::sin(agent.petrasyl_bob_angle
                                   * 3.14159265358979323846F / 180.0F);
        const float y_speed_increment = agent.petrasyl_target_y
            + bob * agent.petrasyl_bob_offset - agent.position.y;
        if (agent.petrasyl_turn_timer > 0) {
            decrement(agent.petrasyl_turn_timer);
        }
        if (agent.petrasyl_turn_timer == 0) {
            const float weave = std::abs(profile.weave_offset) > 0.0001F
                ? profile.weave_offset : 1.0F;
            const float vertical = std::abs(profile.vertical_range)
                > 0.0001F ? profile.vertical_range : 1.0F;
            if (agent.petrasyl_target_y >= agent.petrasyl_initial_position.y
                && agent.petrasyl_target_y
                    <= agent.petrasyl_initial_position.y + vertical) {
                if (to_target.x * to_target.x + to_target.z * to_target.z
                        <= weave * weave) {
                    bool found_overlap = false;
                    if (agent.visible) {
                        for (const auto& other : enemies_) {
                            if (!other.active || other.id == agent.id
                                || other.enemy_type != petrasyl3) {
                                continue;
                            }
                            const float radius = agent.body_radius
                                + other.body_radius;
                            if (distance_squared(agent.position,
                                                 other.position)
                                    <= radius * radius) {
                                agent.petrasyl_direction = subtract(
                                    agent.position, other.position);
                                agent.petrasyl_turn_timer = 5u * 2u;
                                found_overlap = true;
                                break;
                            }
                        }
                    }
                    if (found_overlap) {
                        // The managed controller keeps the selected vector
                        // for the turn delay; no new random direction here.
                    }
                } else {
                    agent.petrasyl_direction.x *= -1.0F;
                    agent.petrasyl_direction.y = random_signed(0x1000u)
                        - (to_target.y - vertical / 2.0F) / vertical;
                    agent.petrasyl_direction.z *= -1.0F;
                    agent.petrasyl_turn_timer = 5u * 2u;
                }
            } else if (agent.petrasyl_target_y
                           > agent.petrasyl_initial_position.y + vertical) {
                agent.petrasyl_direction = {
                    random_signed(0x1000u) - to_target.x / weave,
                    -random_positive(0x800u) - 0.5F,
                    random_signed(0x1000u) - to_target.z / weave};
                agent.petrasyl_turn_timer = 5u * 2u;
            } else {
                agent.petrasyl_direction = {
                    random_signed(0x1000u) - to_target.x / weave,
                    random_positive(0x800u) + 0.5F,
                    random_signed(0x1000u) - to_target.z / weave};
                agent.petrasyl_turn_timer = 5u * 2u;
            }
        }

        agent.petrasyl_direction = normalized_or(
            agent.petrasyl_direction, agent.facing);
        net::Vec3 desired_facing = {agent.petrasyl_direction.x, 0.0F,
                                    agent.petrasyl_direction.z};
        if (!is_petrasyl4 && target_index != players_.size()
            && target_distance < 2.0F * 2.0F) {
            const auto between = subtract(players_[target_index].position,
                                           agent.position);
            desired_facing = {between.x, 0.0F, between.z};
        }
        // Enemy06Entity's original comparison is against a negative fixed
        // value, so its field190 always follows the movement vector. Preserve
        // that quirk for Petrasyl4 while using the intended 2-unit gate for
        // Petrasyl3.
        smooth_facing(desired_facing);
        const net::Vec3 per_frame = multiply(agent.petrasyl_direction,
                                              0.05F / 2.0F);
        agent.petrasyl_target_y += per_frame.y / 2.0F * frames;
        const net::Vec3 step = {
            per_frame.x * frames,
            (per_frame.y + y_speed_increment / 2.0F) * frames,
            per_frame.z * frames};
        agent.position = add(agent.position, step);
        agent.velocity = seconds > 0.0F
            ? multiply(step, 1.0F / seconds) : net::Vec3{};
    };

    if (!is_petrasyl4) {
        if (agent.state == 0) {
            decrement(agent.petrasyl_timer);
            if (agent.petrasyl_timer == 0) {
                agent.state = 1;
                agent.invulnerable = false;
                agent.petrasyl_direction = normalized_or(
                    {random_signed(0x1000u), random_signed(0x1000u),
                     random_signed(0x1000u)}, agent.facing);
            }
            agent.velocity = {};
            return;
        }
        update_wander();
        apply_contact(12);
        return;
    }

    switch (agent.state) {
    case 0:
        decrement(agent.petrasyl_timer);
        if (agent.petrasyl_timer == 0) {
            enter_state(1);
            agent.petrasyl_direction = normalized_or(agent.facing,
                                                     agent.petrasyl_direction);
        } else if (player_is_close()) {
            enter_state(2);
        } else {
            agent.velocity = {};
        }
        break;
    case 1:
        update_wander();
        apply_contact(12);
        if (player_is_close()) {
            enter_state(2);
        }
        break;
    case 2:
        update_wander();
        decrement(agent.petrasyl_secondary_timer);
        if (agent.petrasyl_secondary_timer == 0) {
            enter_state(3);
        } else if (!player_is_close()) {
            enter_state(4);
        }
        break;
    case 3:
        update_wander();
        if (!player_is_close()) {
            enter_state(4);
        }
        break;
    case 4:
        update_wander();
        decrement(agent.petrasyl_timer);
        if (agent.petrasyl_timer == 0) {
            enter_state(1);
        } else if (player_is_close()) {
            enter_state(2);
        }
        break;
    default:
        enter_state(0);
        break;
    }
}

namespace {

// Enemy03Entity's own numbers.
constexpr std::uint32_t TeleportFrames = 20u * 2u;  // todo: FPS stuff
constexpr float DriftSpeed = 0.7F;
constexpr std::uint32_t ContactDamage03 = 12u;
// Beyond this the Petrasyl stops caring about the player and goes back to
// drifting along its own line.
constexpr float NoticeRadius = 7.0F;
constexpr float Pi03 = 3.14159265358979323846F;
constexpr float PlayerBody03 = 0.45F;

// Native counterpart of Enemy03Entity.
//
// A Petrasyl drifts back and forth along a line the spawner authored,
// bobbing as it goes, and turns lazily towards the player when they come
// within seven units.  It cannot be hurt while it is teleporting: it
// appears at one end of its line, drifts to the other, and vanishes --
// and the only window to shoot it is the drift in between.
class Enemy03Entity final {
public:
    Enemy03Entity(const EnemyScene& scene, EnemyState& agent,
                  const net::PlayerState* main) noexcept
        : scene_(scene), agent_(agent), main_(main) {}

    // Enemy03Entity.EnemyProcess.
    void EnemyProcess() { CallStateProcess(); }

    // Enemy03Entity.UpdateState: what changes when the machine moves on.
    // The managed class calls this itself rather than leaving it to the
    // subroutine, because a state here is a whole change of being rather
    // than a change of mind.
    void UpdateState();

private:
    void State00();
    void State01();
    void State02() { State00(); }

    [[nodiscard]] bool Behavior00();
    [[nodiscard]] bool Behavior01();
    [[nodiscard]] bool Behavior02();

    void CallStateProcess();
    [[nodiscard]] bool CallSubroutine();

    [[nodiscard]] bool Touching() const noexcept {
        if (main_ == nullptr) {
            return false;
        }
        const float reach = agent_.body_radius + PlayerBody03;
        return distance_squared(agent_.position, main_->position)
            <= reach * reach;
    }

    const EnemyScene& scene_;
    EnemyState& agent_;
    const net::PlayerState* main_;
};

bool Enemy03Entity::CallSubroutine() {
    return metadata::call_subroutine(
        metadata::Enemy03Subroutines, agent_.sub_id, agent_.next_state,
        [this](std::uint8_t index) {
            switch (index) {
            case 0: return Behavior00();
            case 1: return Behavior01();
            case 2: return Behavior02();
            default: return false;
            }
        });
}

void Enemy03Entity::UpdateState() {
    if (agent_.next_state == 0) {
        // Appearing.  It alternates ends of its line, so a Petrasyl you
        // shot at once comes back from the other side.
        agent_.position = agent_.petrasyl_teleport_initial
            ? net::Vec3{agent_.petrasyl_initial_position.x,
                        agent_.position.y,
                        agent_.petrasyl_initial_position.z}
            : net::Vec3{agent_.petrasyl_idle_limit.x, agent_.position.y,
                        agent_.petrasyl_idle_limit.z};
        agent_.petrasyl_teleport_initial = !agent_.petrasyl_teleport_initial;
        agent_.petrasyl_direction = {-agent_.petrasyl_direction.x,
                                     -agent_.petrasyl_direction.y,
                                     -agent_.petrasyl_direction.z};
        agent_.facing = agent_.petrasyl_direction;
        agent_.petrasyl_turn_timer = TeleportFrames;
    } else if (agent_.next_state == 1) {
        // Solid.  This is the only state it can be hurt in.
        agent_.invulnerable = false;
        const float range = agent_.petrasyl.idle_range.z;
        agent_.petrasyl_timer = static_cast<std::uint32_t>(
            range / DriftSpeed) * 2u;
        agent_.velocity = {agent_.petrasyl_direction.x * DriftSpeed / 2.0F,
                           0.0F,
                           agent_.petrasyl_direction.z * DriftSpeed / 2.0F};
    } else if (agent_.next_state == 2) {
        // Going again.
        agent_.invulnerable = true;
        agent_.velocity = {};
        agent_.petrasyl_secondary_timer = TeleportFrames;
    }
}

void Enemy03Entity::State00() {
    if (CallSubroutine()) {
        UpdateState();
    }
}

void Enemy03Entity::State01() {
    // The bob: a sine about the height it appeared at, at a speed and
    // depth rolled per Petrasyl so a room full of them does not pulse in
    // unison.
    agent_.petrasyl_bob_angle += agent_.petrasyl_bob_speed / 2.0F;
    if (agent_.petrasyl_bob_angle >= 360.0F) {
        agent_.petrasyl_bob_angle -= 360.0F;
    }
    const float sine = std::sin(agent_.petrasyl_bob_angle * Pi03 / 180.0F);
    agent_.velocity.y =
        (agent_.petrasyl_initial_position.y
         + sine * agent_.petrasyl_bob_offset - agent_.position.y) / 2.0F;

    if (scene_.ContactDamage && main_ != nullptr && Touching()) {
        scene_.ContactDamage(agent_, main_->slot_index, ContactDamage03);
    }

    // Within seven units it turns towards the player; beyond that it goes
    // back to its own line.
    net::Vec3 wanted = agent_.petrasyl_direction;
    if (main_ != nullptr
        && distance_squared(main_->position, agent_.position)
            < NoticeRadius * NoticeRadius) {
        wanted = {main_->position.x - agent_.position.x, 0.0F,
                  main_->position.z - agent_.position.z};
    }
    const float wanted_length = std::sqrt(wanted.x * wanted.x
                                          + wanted.z * wanted.z);
    if (wanted_length > 0.0F) {
        wanted = {wanted.x / wanted_length, 0.0F, wanted.z / wanted_length};
    }
    // An eighth of the way each frame, so the turn is lazy enough to
    // drift past a player who keeps moving.
    net::Vec3 turned{
        agent_.facing.x + (wanted.x - agent_.facing.x) / 8.0F / 2.0F,
        agent_.facing.y,
        agent_.facing.z + (wanted.z - agent_.facing.z) / 8.0F / 2.0F};
    if (turned.x == 0.0F && turned.z == 0.0F) {
        turned = agent_.facing;
    }
    const float length = std::sqrt(turned.x * turned.x + turned.z * turned.z);
    if (length > 0.0F) {
        turned = {turned.x / length, turned.y, turned.z / length};
    }
    // Nudged off dead centre when the turn has all but stopped, which is
    // what stops a Petrasyl locking to one heading and sitting there.
    if (std::fabs(turned.x - agent_.facing.x) < 1.0F / 4096.0F
        && std::fabs(turned.z - agent_.facing.z) < 1.0F / 4096.0F) {
        turned.x += 0.125F / 2.0F;
        turned.z -= 0.125F / 2.0F;
        const float nudged = std::sqrt(turned.x * turned.x
                                       + turned.z * turned.z);
        if (nudged > 0.0F) {
            turned = {turned.x / nudged, turned.y, turned.z / nudged};
        }
    }
    agent_.facing = turned;

    if (CallSubroutine()) {
        UpdateState();
    }
}

bool Enemy03Entity::Behavior00() {
    if (agent_.petrasyl_secondary_timer == 0) {
        return true;
    }
    --agent_.petrasyl_secondary_timer;
    return false;
}

bool Enemy03Entity::Behavior01() {
    if (agent_.petrasyl_timer == 0) {
        return true;
    }
    --agent_.petrasyl_timer;
    return false;
}

bool Enemy03Entity::Behavior02() {
    if (agent_.petrasyl_turn_timer == 0) {
        return true;
    }
    --agent_.petrasyl_turn_timer;
    return false;
}

void Enemy03Entity::CallStateProcess() {
    switch (agent_.state) {
    case 0: State00(); break;
    case 1: State01(); break;
    case 2: State02(); break;
    default: break;
    }
}

} // namespace

void Session::update_petrasyl1(EnemyState& agent) {
    if (!agent.petrasyl.supported) {
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
    Enemy03Entity(scene, agent, main).EnemyProcess();
}
} // namespace fruityprime::gameplay

namespace fruityprime::enemy::module_03_petrasyl1 {

void EnemyInitialize(gameplay::EnemyState& agent) noexcept {
    agent.health = agent.health_max = 8;
    agent.body_radius = 0.5F;
    // It starts mid-teleport, so it cannot be shot before it has even
    // appeared.
    agent.invulnerable = true;
    agent.state = agent.next_state = agent.sub_id = 0;

    const auto& profile = agent.petrasyl;
    // The authored line: a facing and a range either side of where the
    // spawner put it.  A Petrasyl drifts from one end to the other and
    // teleports back, so both ends are worked out once here.
    const float length = std::sqrt(profile.facing.x * profile.facing.x
                                   + profile.facing.z * profile.facing.z);
    const net::Vec3 flat = length > 0.0F
        ? net::Vec3{profile.facing.x / length, 0.0F,
                    profile.facing.z / length}
        : net::Vec3{0.0F, 0.0F, 1.0F};
    const net::Vec3 start{
        agent.position.x + profile.position_offset.x,
        agent.position.y + profile.position_offset.y + 5461.0F / 4096.0F,
        agent.position.z + profile.position_offset.z};
    agent.petrasyl_initial_position = start;
    agent.petrasyl_idle_limit = {
        start.x + flat.x * profile.idle_range.z - flat.z * profile.idle_range.x,
        start.y,
        start.z + flat.z * profile.idle_range.z + flat.x * profile.idle_range.x};
    agent.position = start;
    // It faces back along its own line to begin with.
    agent.petrasyl_direction = {-flat.x, 0.0F, -flat.z};
    agent.facing = agent.petrasyl_direction;
    // Depth and speed of the bob are rolled per Petrasyl, so a room full
    // of them does not pulse in unison.
    agent.petrasyl_bob_offset =
        (static_cast<float>(utility::get_random_int2(0x1800u)) + 2048.0F)
        / 4096.0F / 2.0F;
    agent.petrasyl_bob_speed =
        static_cast<float>(utility::get_random_int2(0x6000u)) / 4096.0F + 1.0F;
    agent.petrasyl_bob_angle = 0.0F;
    agent.petrasyl_turn_timer = 20u * 2u;       // todo: FPS stuff
    agent.petrasyl_secondary_timer = 20u * 2u;  // todo: FPS stuff
    agent.petrasyl_teleport_initial = true;
}

} // namespace fruityprime::enemy::module_03_petrasyl1

namespace fruityprime::enemy {

PetrasylProfile decode_petrasyl_profile(
    std::uint8_t id, std::span<const std::uint8_t> fields,
    net::Vec3 origin) noexcept {
    PetrasylProfile result;
    if (id < static_cast<std::uint8_t>(formats::EnemyType::Petrasyl1)
        || id > static_cast<std::uint8_t>(formats::EnemyType::Petrasyl4)
        || fields.size() < 64) {
        return result;
    }

    result.variant = id;
    result.hurt_volume = detail::read_volume(fields, 0, origin);
    if (id == static_cast<std::uint8_t>(formats::EnemyType::Petrasyl1)) {
        if (fields.size() < 128) {
            return result;
        }
        result.facing = detail::read_vector(fields, 92);
        result.position_offset = detail::read_vector(fields, 104);
        result.idle_range = detail::read_vector(fields, 116);
    } else {
        if (fields.size() < 100) {
            return result;
        }
        result.position_offset = detail::read_vector(fields, 80);
        result.weave_offset = detail::read_fixed(fields, 92);
        result.vertical_range = detail::read_fixed(fields, 96);
    }
    result.supported = result.hurt_volume.kind != scene::VolumeKind::Invalid;
    return result;
}

} // namespace fruityprime::enemy

