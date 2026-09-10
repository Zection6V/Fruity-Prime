// Native counterpart of src/MphRead/Entities/Enemies/04_Petrasyl2.cs.
// This file is deliberately present even when the managed class currently
// shares a native controller; the descriptor and entry point prevent a
// many-classes-in-one gameplay.cpp regression.
#include <cmath>
#include <algorithm>
#include "Utility/rng.hpp"
#include "Metadata/enemy_subroutines.hpp"
#include "enemy_scene.hpp"
#include "04_Petrasyl2.hpp"
#include "enemy_common.hpp"

namespace fruityprime::gameplay {

namespace {

// Enemy04Entity's own numbers.
constexpr std::uint32_t Petrasyl2TeleportFrames = 30u * 2u;  // todo: FPS
constexpr std::uint32_t Petrasyl2ContactDamage = 12u;
constexpr float Petrasyl2NoticeRadius = 7.0F;
constexpr float Petrasyl2WeaveStep = 1.5F / 2.0F;  // todo: FPS stuff
constexpr float Pi04 = 3.14159265358979323846F;
constexpr float PlayerBody04 = 0.45F;

// Native counterpart of Enemy04Entity.
//
// This Petrasyl does not travel: it circles the point it was placed on,
// weaving out to a radius the spawner authored and bobbing at the same
// time, and turns towards the player when they come within seven units.
//
// It teleports in once and then stays, which is why its state machine is
// two states rather than three -- and while it is arriving, both the
// circle and the weave are scaled by how far the animation has run, so it
// spirals outward into its orbit rather than snapping into it.
class Enemy04Entity final {
public:
    Enemy04Entity(const EnemyScene& scene, EnemyState& agent,
                  const net::PlayerState* main) noexcept
        : scene_(scene), agent_(agent), main_(main) {}

    // Enemy04Entity.EnemyProcess.
    void EnemyProcess() { CallStateProcess(); }

    // Enemy04Entity.UpdateState.
    void UpdateState();

private:
    // Enemy04Entity.UpdateMovement: the circle, the bob and the lazy turn,
    // which both states run.
    void UpdateMovement();

    void State0();
    void State1();

    [[nodiscard]] bool Behavior00() const noexcept { return false; }
    [[nodiscard]] bool Behavior01();

    void CallStateProcess();
    [[nodiscard]] bool CallSubroutine();

    [[nodiscard]] bool Touching() const noexcept {
        if (main_ == nullptr) {
            return false;
        }
        const float reach = agent_.body_radius + PlayerBody04;
        return distance_squared(agent_.position, main_->position)
            <= reach * reach;
    }

    const EnemyScene& scene_;
    EnemyState& agent_;
    const net::PlayerState* main_;
};

bool Enemy04Entity::CallSubroutine() {
    return metadata::call_subroutine(
        metadata::Enemy04Subroutines, agent_.sub_id, agent_.next_state,
        [this](std::uint8_t index) {
            switch (index) {
            case 0: return Behavior00();
            case 1: return Behavior01();
            default: return false;
            }
        });
}

void Enemy04Entity::UpdateState() {
    if (agent_.next_state == 0) {
        // Arrived: solid, and shootable.
        agent_.invulnerable = false;
    } else if (agent_.next_state == 1) {
        agent_.petrasyl_timer = Petrasyl2TeleportFrames;
    }
}

void Enemy04Entity::UpdateMovement() {
    // The heading it drifts along when nothing is near: a quarter turn
    // from the line back to where it was placed, which is what makes it
    // circle rather than face outward.
    net::Vec3 radial{agent_.petrasyl_initial_position.x - agent_.position.x,
                     0.0F,
                     agent_.petrasyl_initial_position.z - agent_.position.z};
    net::Vec3 tangent{-radial.z, 0.0F, radial.x};
    const float tangent_length = std::sqrt(tangent.x * tangent.x
                                           + tangent.z * tangent.z);
    tangent = tangent_length > 0.0F
        ? net::Vec3{tangent.x / tangent_length, 0.0F,
                    tangent.z / tangent_length}
        : agent_.facing;

    // State zero is the arrival.  Both the angle's rate and the radius
    // are scaled by how far the animation has run, so it spirals out.
    const float progress = agent_.state == 0
        ? std::clamp(static_cast<float>(agent_.petrasyl_turn_timer) / 30.0F,
                     0.0F, 1.0F)
        : 1.0F;
    agent_.petrasyl_weave_angle += agent_.state != 0
        ? Petrasyl2WeaveStep
        : (1.5F * progress * 30.0F + (30.0F - progress * 30.0F))
              / 30.0F / 2.0F;
    if (agent_.petrasyl_weave_angle >= 360.0F) {
        agent_.petrasyl_weave_angle -= 360.0F;
    }
    const float angle = agent_.petrasyl_weave_angle * Pi04 / 180.0F;
    const float radius = agent_.petrasyl.weave_offset * progress;
    agent_.velocity.x = agent_.petrasyl_initial_position.x
        + std::sin(angle) * radius - agent_.position.x;
    agent_.velocity.z = agent_.petrasyl_initial_position.z
        + std::cos(angle) * radius - agent_.position.z;

    agent_.petrasyl_bob_angle += agent_.petrasyl_bob_speed / 2.0F;
    if (agent_.petrasyl_bob_angle >= 360.0F) {
        agent_.petrasyl_bob_angle -= 360.0F;
    }
    const float bob = std::sin(agent_.petrasyl_bob_angle * Pi04 / 180.0F);
    agent_.velocity.y = agent_.petrasyl_initial_position.y
        + bob * agent_.petrasyl_bob_offset - agent_.position.y;
    // The whole step is halved for this head's rate.
    agent_.velocity = {agent_.velocity.x / 2.0F, agent_.velocity.y / 2.0F,
                       agent_.velocity.z / 2.0F};

    net::Vec3 wanted = tangent;
    if (main_ != nullptr
        && distance_squared(main_->position, agent_.position)
            < Petrasyl2NoticeRadius * Petrasyl2NoticeRadius) {
        wanted = {main_->position.x - agent_.position.x, 0.0F,
                  main_->position.z - agent_.position.z};
    }
    const float wanted_length = std::sqrt(wanted.x * wanted.x
                                          + wanted.z * wanted.z);
    if (wanted_length > 0.0F) {
        wanted = {wanted.x / wanted_length, 0.0F, wanted.z / wanted_length};
    }
    // The same lazy eighth-of-the-way turn the first Petrasyl uses, with
    // the same nudge to stop it locking to one heading.
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
}

void Enemy04Entity::State0() {
    UpdateMovement();
    if (CallSubroutine()) {
        UpdateState();
    }
}

void Enemy04Entity::State1() {
    UpdateMovement();
    if (scene_.ContactDamage && main_ != nullptr && Touching()) {
        scene_.ContactDamage(agent_, main_->slot_index,
                             Petrasyl2ContactDamage);
    }
    // State one never runs its subroutine: once this Petrasyl has
    // arrived it stays, and the machine has nowhere left to go.
}

bool Enemy04Entity::Behavior01() {
    if (agent_.petrasyl_timer == 0) {
        return true;
    }
    --agent_.petrasyl_timer;
    return false;
}

void Enemy04Entity::CallStateProcess() {
    switch (agent_.state) {
    case 0: State0(); break;
    case 1: State1(); break;
    default: break;
    }
}

} // namespace

void Session::update_petrasyl2(EnemyState& agent) {
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
    if (agent.petrasyl_turn_timer < 30u) {
        ++agent.petrasyl_turn_timer;
    }

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
    Enemy04Entity(scene, agent, main).EnemyProcess();
}

} // namespace fruityprime::gameplay

static_assert(fruityprime::enemy::module_04_petrasyl2::kModule.managed_class.size() != 0);

namespace fruityprime::enemy::module_04_petrasyl2 {

void EnemyInitialize(gameplay::EnemyState& agent) noexcept {
    agent.health = agent.health_max = 8;
    agent.body_radius = 0.5F;
    // It arrives mid-teleport and cannot be shot until it is solid.
    agent.invulnerable = true;
    agent.state = agent.next_state = agent.sub_id = 1;
    const auto& profile = agent.petrasyl;
    const net::Vec3 centre{
        agent.position.x + profile.position_offset.x,
        agent.position.y + profile.position_offset.y + 5461.0F / 4096.0F,
        agent.position.z + profile.position_offset.z};
    agent.petrasyl_initial_position = centre;
    agent.position = centre;
    agent.petrasyl_weave_angle = 0.0F;
    agent.petrasyl_turn_timer = 0;
    agent.petrasyl_timer = 30u * 2u;  // todo: FPS stuff
    // Rolled per Petrasyl, so a cluster of them does not pulse together.
    agent.petrasyl_bob_offset =
        (static_cast<float>(utility::get_random_int2(0x1AABu)) + 1365.0F)
        / 4096.0F / 2.0F;
    agent.petrasyl_bob_speed =
        static_cast<float>(utility::get_random_int2(0x6000u)) / 4096.0F + 1.0F;
    agent.petrasyl_bob_angle = 0.0F;
}

} // namespace fruityprime::enemy::module_04_petrasyl2
