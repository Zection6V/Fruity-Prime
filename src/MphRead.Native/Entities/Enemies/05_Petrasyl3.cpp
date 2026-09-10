// Native counterpart of src/MphRead/Entities/Enemies/05_Petrasyl3.cs.
// This file is deliberately present even when the managed class currently
// shares a native controller; the descriptor and entry point prevent a
// many-classes-in-one gameplay.cpp regression.
#include "Formats/Types.hpp"
#include <cmath>
#include "Utility/rng.hpp"
#include "Metadata/enemy_subroutines.hpp"
#include "enemy_scene.hpp"
#include "05_Petrasyl3.hpp"
#include "enemy_common.hpp"

namespace fruityprime::gameplay {

namespace {

// Enemy05Entity's own numbers.
constexpr std::uint32_t Petrasyl3TeleportFrames = 20u * 2u;  // todo: FPS
constexpr std::uint32_t Petrasyl3ContactDamage = 12u;
// This one turns towards the player much later than its cousins: two
// units rather than seven, so it mostly ignores you.
constexpr float Petrasyl3NoticeRadius = 2.0F;
constexpr float Petrasyl3Drift = 0.05F;
// How long it holds a heading after picking one, which is what stops it
// changing its mind every frame at a boundary.
constexpr std::uint32_t Petrasyl3HoldFrames = 5u * 2u;
constexpr float Pi05 = 3.14159265358979323846F;
constexpr float PlayerBody05 = 0.45F;

// Native counterpart of Enemy05Entity.
//
// This Petrasyl wanders inside a box rather than following a path or an
// orbit: it drifts on a heading until it reaches the edge of the volume
// the spawner authored, then picks a new one that points back inwards --
// with a random component, so a swarm of them mills about rather than
// bouncing like billiard balls.
//
// The vertical is separate from the horizontal.  It bobs about a target
// height that itself drifts, which is what makes them rise and fall
// independently of where they are going.
class Enemy05Entity final {
public:
    Enemy05Entity(const EnemyScene& scene, EnemyState& agent,
                  const net::PlayerState* main) noexcept
        : scene_(scene), agent_(agent), main_(main) {}

    // Enemy05Entity.EnemyProcess.
    void EnemyProcess() { CallStateProcess(); }

    // Enemy05Entity.UpdateState.
    void UpdateState();

private:
    // Enemy05Entity.UpdateMovement.
    void UpdateMovement();

    void State0();
    void State1();

    [[nodiscard]] bool Behavior00() const noexcept { return false; }
    [[nodiscard]] bool Behavior01();

    void CallStateProcess();
    [[nodiscard]] bool CallSubroutine();

    // A random component in [-0.5, 0.5), which every new heading gets.
    [[nodiscard]] static float Jitter(std::uint32_t range) noexcept {
        return static_cast<float>(utility::get_random_int2(range)) / 4096.0F
            - 0.5F;
    }

    [[nodiscard]] bool Touching() const noexcept {
        if (main_ == nullptr) {
            return false;
        }
        const float reach = agent_.body_radius + PlayerBody05;
        return distance_squared(agent_.position, main_->position)
            <= reach * reach;
    }

    const EnemyScene& scene_;
    EnemyState& agent_;
    const net::PlayerState* main_;
};

bool Enemy05Entity::CallSubroutine() {
    return metadata::call_subroutine(
        metadata::Enemy05Subroutines, agent_.sub_id, agent_.next_state,
        [this](std::uint8_t index) {
            switch (index) {
            case 0: return Behavior00();
            case 1: return Behavior01();
            default: return false;
            }
        });
}

void Enemy05Entity::UpdateState() {
    if (agent_.next_state == 0) {
        agent_.petrasyl_timer = Petrasyl3TeleportFrames;
    } else if (agent_.next_state == 1) {
        // Arrived: solid, and off in a direction picked at random -- in
        // all three axes, unlike the other two, which is why these drift
        // up and down as freely as they drift about.
        agent_.invulnerable = false;
        net::Vec3 heading{Jitter(4096u), Jitter(4096u), Jitter(4096u)};
        const float length = std::sqrt(heading.x * heading.x
                                       + heading.y * heading.y
                                       + heading.z * heading.z);
        agent_.petrasyl_direction = (heading.x == 0.0F && heading.z == 0.0F)
                || length == 0.0F
            ? agent_.facing
            : net::Vec3{heading.x / length, heading.y / length,
                        heading.z / length};
        agent_.velocity = {
            agent_.petrasyl_direction.x * Petrasyl3Drift / 2.0F,
            agent_.petrasyl_direction.y * Petrasyl3Drift / 2.0F,
            agent_.petrasyl_direction.z * Petrasyl3Drift / 2.0F};
    }
}

void Enemy05Entity::UpdateMovement() {
    const float weave = agent_.petrasyl.weave_offset;
    const float height = agent_.petrasyl.vertical_range;
    const net::Vec3 origin = agent_.petrasyl_initial_position;
    const net::Vec3 to_target{agent_.position.x - origin.x,
                              agent_.petrasyl_target_y - origin.y,
                              agent_.position.z - origin.z};

    agent_.petrasyl_bob_angle += agent_.petrasyl_bob_speed / 2.0F;
    if (agent_.petrasyl_bob_angle >= 360.0F) {
        // A full turn of the bob resets the height it is bobbing about to
        // wherever it has actually drifted to, so the two never fight.
        agent_.petrasyl_target_y = agent_.position.y;
        agent_.petrasyl_bob_angle -= 360.0F;
    }
    const float bob = std::sin(agent_.petrasyl_bob_angle * Pi05 / 180.0F);
    const float vertical = agent_.petrasyl_target_y
        + bob * agent_.petrasyl_bob_offset - agent_.position.y;

    if (agent_.petrasyl_turn_timer > 0) {
        --agent_.petrasyl_turn_timer;
    }
    if (agent_.petrasyl_turn_timer == 0) {
        net::Vec3 heading = agent_.petrasyl_direction;
        if (agent_.petrasyl_target_y < origin.y) {
            // Below the box: head back up, and turn inwards by however
            // far out it has drifted.
            heading = {Jitter(0x1000u) - to_target.x / weave,
                       static_cast<float>(utility::get_random_int2(0x800u))
                           / 4096.0F + 0.5F,
                       Jitter(0x1000u) - to_target.z / weave};
            agent_.petrasyl_turn_timer = Petrasyl3HoldFrames;
        } else if (agent_.petrasyl_target_y > origin.y + height) {
            // Above it: the same, downwards.
            heading = {Jitter(0x1000u) - to_target.x / weave,
                       -static_cast<float>(utility::get_random_int2(0x800u))
                           / 4096.0F - 0.5F,
                       Jitter(0x1000u) - to_target.z / weave};
            agent_.petrasyl_turn_timer = Petrasyl3HoldFrames;
        } else if (to_target.x * to_target.x + to_target.z * to_target.z
                       > weave * weave) {
            // Outside the box sideways: turn straight round, and aim the
            // vertical back towards the middle of the height range.
            heading = {-heading.x,
                       Jitter(0x1000u)
                           - (to_target.y - height / 2.0F) / height,
                       -heading.z};
            agent_.petrasyl_turn_timer = Petrasyl3HoldFrames;
        } else if (scene_.NearbyKin) {
            // Inside the box: the only thing that turns it is bumping
            // into another of its own kind, which is what keeps a swarm
            // spread out instead of piling up in the middle.
            net::Vec3 away{};
            if (scene_.NearbyKin(
                    agent_,
                    static_cast<std::uint8_t>(formats::EnemyType::Petrasyl3),
                    away)) {
                heading = away;
                agent_.petrasyl_turn_timer = Petrasyl3HoldFrames;
            }
        }
        agent_.petrasyl_direction = heading;
    }

    if (agent_.petrasyl_direction.x == 0.0F
        && agent_.petrasyl_direction.y == 0.0F) {
        agent_.petrasyl_direction = agent_.facing;
    } else {
        const float length = std::sqrt(
            agent_.petrasyl_direction.x * agent_.petrasyl_direction.x
            + agent_.petrasyl_direction.y * agent_.petrasyl_direction.y
            + agent_.petrasyl_direction.z * agent_.petrasyl_direction.z);
        if (length > 0.0F) {
            agent_.petrasyl_direction = {
                agent_.petrasyl_direction.x / length,
                agent_.petrasyl_direction.y / length,
                agent_.petrasyl_direction.z / length};
        }
    }

    net::Vec3 wanted{agent_.petrasyl_direction.x, 0.0F,
                     agent_.petrasyl_direction.z};
    if (main_ != nullptr
        && distance_squared(main_->position, agent_.position)
            < Petrasyl3NoticeRadius * Petrasyl3NoticeRadius) {
        wanted = {main_->position.x - agent_.position.x, 0.0F,
                  main_->position.z - agent_.position.z};
    }
    const float wanted_length = std::sqrt(wanted.x * wanted.x
                                          + wanted.z * wanted.z);
    if (wanted_length > 0.0F) {
        wanted = {wanted.x / wanted_length, 0.0F, wanted.z / wanted_length};
    }
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

    agent_.velocity = {
        agent_.petrasyl_direction.x * Petrasyl3Drift / 2.0F,
        agent_.petrasyl_direction.y * Petrasyl3Drift / 2.0F,
        agent_.petrasyl_direction.z * Petrasyl3Drift / 2.0F};
    // The height it bobs about drifts with it, and the bob is added on
    // top -- so the two are independent, which is the whole reason a
    // swarm of these does not move as one.
    agent_.petrasyl_target_y += agent_.velocity.y / 2.0F;
    agent_.velocity.y += vertical / 2.0F;
}

void Enemy05Entity::State0() {
    if (CallSubroutine()) {
        UpdateState();
    }
}

void Enemy05Entity::State1() {
    UpdateMovement();
    if (scene_.ContactDamage && main_ != nullptr && Touching()) {
        scene_.ContactDamage(agent_, main_->slot_index,
                             Petrasyl3ContactDamage);
    }
    // Like the second Petrasyl, state one has nowhere left to go.
}

bool Enemy05Entity::Behavior01() {
    if (agent_.petrasyl_timer == 0) {
        return true;
    }
    --agent_.petrasyl_timer;
    return false;
}

void Enemy05Entity::CallStateProcess() {
    switch (agent_.state) {
    case 0: State0(); break;
    case 1: State1(); break;
    default: break;
    }
}

} // namespace

void Session::update_petrasyl3(EnemyState& agent) {
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
    scene.NearbyKin = [this](const EnemyState& self,
                            const std::uint8_t kind, net::Vec3& away) {
        for (const auto& other : enemies_) {
            if (other.id == self.id || other.enemy_type != kind
                || other.health == 0) {
                continue;
            }
            const float reach = self.body_radius + other.body_radius;
            if (distance_squared(self.position, other.position)
                    > reach * reach) {
                continue;
            }
            away = {self.position.x - other.position.x,
                    self.position.y - other.position.y,
                    self.position.z - other.position.z};
            return true;
        }
        return false;
    };
    Enemy05Entity(scene, agent, main).EnemyProcess();
}

} // namespace fruityprime::gameplay

static_assert(fruityprime::enemy::module_05_petrasyl3::kModule.managed_class.size() != 0);

namespace fruityprime::enemy::module_05_petrasyl3 {

void EnemyInitialize(gameplay::EnemyState& agent) noexcept {
    agent.health = agent.health_max = 8;
    agent.body_radius = 0.5F;
    agent.invulnerable = true;
    agent.state = agent.next_state = agent.sub_id = 0;
    const auto& profile = agent.petrasyl;
    const net::Vec3 centre{
        agent.position.x + profile.position_offset.x,
        agent.position.y + profile.position_offset.y + 5461.0F / 4096.0F,
        agent.position.z + profile.position_offset.z};
    agent.petrasyl_initial_position = centre;
    agent.position = centre;
    agent.petrasyl_target_y = centre.y;
    agent.petrasyl_direction = agent.facing;
    agent.petrasyl_turn_timer = 0;
    agent.petrasyl_timer = 20u * 2u;  // todo: FPS stuff
    agent.petrasyl_bob_offset =
        (static_cast<float>(utility::get_random_int2(0x1AABu)) + 1365.0F)
        / 4096.0F / 2.0F;
    // A slower bob than the other two: [1, 4) rather than [1, 7).
    agent.petrasyl_bob_speed =
        static_cast<float>(utility::get_random_int2(0x3000u)) / 4096.0F + 1.0F;
    agent.petrasyl_bob_angle = 0.0F;
}

} // namespace fruityprime::enemy::module_05_petrasyl3
