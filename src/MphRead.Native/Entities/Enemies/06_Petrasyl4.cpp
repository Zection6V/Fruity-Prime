// Native counterpart of src/MphRead/Entities/Enemies/06_Petrasyl4.cs.
// This file is deliberately present even when the managed class currently
// shares a native controller; the descriptor and entry point prevent a
// many-classes-in-one gameplay.cpp regression.
#include <cmath>
#include <limits>
#include "Formats/Types.hpp"
#include "Utility/rng.hpp"
#include "Metadata/enemy_subroutines.hpp"
#include "enemy_scene.hpp"
#include "06_Petrasyl4.hpp"
#include "enemy_common.hpp"

namespace fruityprime::gameplay {

namespace {

// Enemy06Entity's own numbers.
//
// The fade is ten frames each way on the cartridge, which is what makes
// the interrupted-fade arithmetic below land on whole numbers.
constexpr std::uint32_t Petrasyl4FadeFrames = 10u * 2u;  // todo: FPS stuff
constexpr std::uint32_t Petrasyl4ContactDamage = 12u;
// Come within six units and it leaves.  Unlike its cousins this is not a
// radius it turns towards you inside of -- it is the radius it will not
// stay inside of.
constexpr float Petrasyl4FleeRadius = 6.0F;
constexpr float Petrasyl4Drift = 0.05F;
// How long it holds a heading after picking one, which is what stops it
// changing its mind every frame at a boundary.
constexpr std::uint32_t Petrasyl4HoldFrames = 5u * 2u;
constexpr float Pi06 = 3.14159265358979323846F;
constexpr float PlayerBody06 = 0.45F;

// Native counterpart of Enemy06Entity.
//
// It wanders a box exactly the way the third Petrasyl does -- the managed
// class says as much in a comment on its own UpdateMovement, and lists
// the three differences.  Two of them are here: it only looks for
// something to bump into while it is actually visible, and its turn
// towards the player is dead code (see UpdateMovement).
//
// The third difference is the whole creature: five states rather than
// two, because this one comes and goes.  Fade in, fly, fade out, gone,
// fade in again -- and what drives the fades is the player's distance,
// not a timer, so it is the one Petrasyl you cannot corner.
class Enemy06Entity final {
public:
    Enemy06Entity(const EnemyScene& scene, EnemyState& agent,
                  const net::PlayerState* main) noexcept
        : scene_(scene), agent_(agent), main_(main) {}

    // Enemy06Entity.EnemyProcess.
    void EnemyProcess() { CallStateProcess(); }

    // Enemy06Entity.UpdateState.
    void UpdateState();

private:
    // Enemy06Entity.UpdateMovement.
    void UpdateMovement();

    void State0();
    void State1();
    void State2();
    void State3();
    void State4();

    [[nodiscard]] bool Behavior00() const noexcept;
    [[nodiscard]] bool Behavior01() const noexcept { return !Behavior00(); }
    [[nodiscard]] bool Behavior02();
    [[nodiscard]] bool Behavior03();

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
        const float reach = agent_.body_radius + PlayerBody06;
        return distance_squared(agent_.position, main_->position)
            <= reach * reach;
    }

    const EnemyScene& scene_;
    EnemyState& agent_;
    const net::PlayerState* main_;
};

bool Enemy06Entity::CallSubroutine() {
    return metadata::call_subroutine(
        metadata::Enemy06Subroutines, agent_.sub_id, agent_.next_state,
        [this](std::uint8_t index) {
            switch (index) {
            case 0: return Behavior00();
            case 1: return Behavior01();
            case 2: return Behavior02();
            case 3: return Behavior03();
            default: return false;
            }
        });
}

void Enemy06Entity::UpdateState() {
    bool updateSpeed = false;
    if (agent_.next_state == 0) {
        agent_.visible = true;
        agent_.petrasyl_timer = Petrasyl4FadeFrames;
    } else if (agent_.next_state == 1) {
        // Solid: the only state it can be hurt or homed in on.
        agent_.invulnerable = false;
        if (agent_.state == 0) {
            updateSpeed = true;
        }
    } else if (agent_.next_state == 2) {
        agent_.invulnerable = true;
        if (agent_.state == 1) {
            agent_.petrasyl_secondary_timer = Petrasyl4FadeFrames;
        } else {
            // Interrupted halfway through fading in.  It fades back out
            // from where it actually got to rather than from solid, so a
            // player who walks up and backs off leaves it flickering
            // instead of popping.
            agent_.petrasyl_secondary_timer =
                Petrasyl4FadeFrames - agent_.petrasyl_timer;
            if (agent_.state == 0) {
                updateSpeed = true;
            }
        }
    } else if (agent_.next_state == 3) {
        agent_.visible = false;
    } else if (agent_.next_state == 4) {
        agent_.visible = true;
        agent_.petrasyl_timer = agent_.state == 3
            ? Petrasyl4FadeFrames
            : Petrasyl4FadeFrames - agent_.petrasyl_secondary_timer;
    }
    if (updateSpeed) {
        // The cartridge rolls three numbers here and then throws them
        // away: it writes them into the heading and immediately
        // overwrites it with the facing.  They are still rolled, because
        // every other enemy alive this frame draws from the same stream
        // and dropping them would move all of them.
        static_cast<void>(utility::get_random_int2(0x1000u));
        static_cast<void>(utility::get_random_int2(0x1000u));
        static_cast<void>(utility::get_random_int2(0x1000u));
        const net::Vec3 facing = agent_.facing;
        net::Vec3 heading{facing.x * Petrasyl4Drift, facing.y * Petrasyl4Drift,
                          facing.z * Petrasyl4Drift};
        if (heading.x == 0.0F && heading.y == 0.0F) {
            heading = facing;
        } else {
            const float length = std::sqrt(heading.x * heading.x
                                           + heading.y * heading.y
                                           + heading.z * heading.z);
            if (length > 0.0F) {
                heading = {heading.x / length, heading.y / length,
                           heading.z / length};
            }
        }
        agent_.petrasyl_direction = heading;
        agent_.velocity = {heading.x * Petrasyl4Drift / 2.0F,
                           heading.y * Petrasyl4Drift / 2.0F,
                           heading.z * Petrasyl4Drift / 2.0F};
    }
}

void Enemy06Entity::UpdateMovement() {
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
    const float bob = std::sin(agent_.petrasyl_bob_angle * Pi06 / 180.0F);
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
            agent_.petrasyl_turn_timer = Petrasyl4HoldFrames;
        } else if (agent_.petrasyl_target_y > origin.y + height) {
            // Above it: the same, downwards.
            heading = {Jitter(0x1000u) - to_target.x / weave,
                       -static_cast<float>(utility::get_random_int2(0x800u))
                           / 4096.0F - 0.5F,
                       Jitter(0x1000u) - to_target.z / weave};
            agent_.petrasyl_turn_timer = Petrasyl4HoldFrames;
        } else if (to_target.x * to_target.x + to_target.z * to_target.z
                       > weave * weave) {
            // Outside the box sideways: turn straight round, and aim the
            // vertical back towards the middle of the height range.
            heading = {-heading.x,
                       Jitter(0x1000u)
                           - (to_target.y - height / 2.0F) / height,
                       -heading.z};
            agent_.petrasyl_turn_timer = Petrasyl4HoldFrames;
        } else if (agent_.visible && scene_.NearbyKin) {
            // Inside the box: bumping into something is the only thing
            // that turns it -- and only while it is actually there, which
            // is the third Petrasyl's version plus the visibility test,
            // since this one spends half its life gone.
            //
            // It asks for Petrasyl3s rather than its own kind.  That is
            // the cartridge's own quirk and is kept: a room with both in
            // it has the fourth avoiding the third while the third avoids
            // nothing at all.
            net::Vec3 away{};
            if (scene_.NearbyKin(
                    agent_,
                    static_cast<std::uint8_t>(formats::EnemyType::Petrasyl3),
                    away)) {
                heading = away;
                agent_.petrasyl_turn_timer = Petrasyl4HoldFrames;
            }
        }
        agent_.petrasyl_direction = heading;
    }

    const net::Vec3 prev_facing = agent_.facing;
    if (agent_.petrasyl_direction.x == 0.0F
        && agent_.petrasyl_direction.y == 0.0F) {
        agent_.petrasyl_direction = prev_facing;
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

    // Where its cousins ask how far the player is and turn towards them
    // inside a radius, this one compares a squared length against a
    // *negative* number -- so the answer is always the same and it never
    // turns towards anybody.  The managed port asserts on the other
    // branch to say so.  Kept: the branch it cannot reach is not written,
    // because writing it would invent behaviour the game does not have.
    net::Vec3 wanted{agent_.petrasyl_direction.x, 0.0F,
                     agent_.petrasyl_direction.z};
    const float wanted_length = std::sqrt(wanted.x * wanted.x
                                          + wanted.z * wanted.z);
    if (wanted_length > 0.0F) {
        wanted = {wanted.x / wanted_length, 0.0F, wanted.z / wanted_length};
    }
    net::Vec3 turned{
        prev_facing.x + (wanted.x - prev_facing.x) / 8.0F / 2.0F,
        prev_facing.y,
        prev_facing.z + (wanted.z - prev_facing.z) / 8.0F / 2.0F};
    if (turned.x == 0.0F && turned.z == 0.0F) {
        turned = prev_facing;
    }
    const float length = std::sqrt(turned.x * turned.x + turned.z * turned.z);
    if (length > 0.0F) {
        turned = {turned.x / length, turned.y, turned.z / length};
    }
    if (std::fabs(turned.x - prev_facing.x) < 1.0F / 4096.0F
        && std::fabs(turned.z - prev_facing.z) < 1.0F / 4096.0F) {
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
        agent_.petrasyl_direction.x * Petrasyl4Drift / 2.0F,
        agent_.petrasyl_direction.y * Petrasyl4Drift / 2.0F,
        agent_.petrasyl_direction.z * Petrasyl4Drift / 2.0F};
    // The height it bobs about drifts with it, and the bob is added on
    // top -- so the two are independent, which is the whole reason a
    // swarm of these does not move as one.
    agent_.petrasyl_target_y += agent_.velocity.y / 2.0F;
    agent_.velocity.y += vertical / 2.0F;
}

void Enemy06Entity::State0() {
    // The managed class drives the fade animation's frame from the timer
    // here; there is no model on this side to drive.
    if (CallSubroutine()) {
        UpdateState();
    }
}

void Enemy06Entity::State1() {
    UpdateMovement();
    if (scene_.ContactDamage && main_ != nullptr && Touching()) {
        scene_.ContactDamage(agent_, main_->slot_index,
                             Petrasyl4ContactDamage);
    }
    if (CallSubroutine()) {
        UpdateState();
    }
}

void Enemy06Entity::State2() {
    UpdateMovement();
    if (CallSubroutine()) {
        UpdateState();
    }
}

void Enemy06Entity::State3() {
    // Gone, but still drifting: it keeps its place in the box while it is
    // away, which is why backing off does not bring it back where it
    // started.
    State2();
}

void Enemy06Entity::State4() {
    UpdateMovement();
    if (CallSubroutine()) {
        UpdateState();
    }
}

bool Enemy06Entity::Behavior00() const noexcept {
    if (main_ == nullptr) {
        return false;
    }
    return distance_squared(main_->position, agent_.position)
        < Petrasyl4FleeRadius * Petrasyl4FleeRadius;
}

bool Enemy06Entity::Behavior02() {
    if (agent_.petrasyl_timer == 0) {
        return true;
    }
    --agent_.petrasyl_timer;
    return false;
}

bool Enemy06Entity::Behavior03() {
    if (agent_.petrasyl_secondary_timer == 0) {
        return true;
    }
    --agent_.petrasyl_secondary_timer;
    return false;
}

void Enemy06Entity::CallStateProcess() {
    switch (agent_.state) {
    case 0: State0(); break;
    case 1: State1(); break;
    case 2: State2(); break;
    case 3: State3(); break;
    case 4: State4(); break;
    default: break;
    }
}

} // namespace

void Session::update_petrasyl4(EnemyState& agent) {
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
    scene.NearbyKin = [this](const EnemyState& self, const std::uint8_t kind,
                             net::Vec3& away) {
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
    Enemy06Entity(scene, agent, main).EnemyProcess();
}

} // namespace fruityprime::gameplay

static_assert(fruityprime::enemy::module_06_petrasyl4::kModule.managed_class.size() != 0);

namespace fruityprime::enemy::module_06_petrasyl4 {

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
    // Unlike the third, this one starts on the heading it was authored
    // facing rather than one rolled at random -- a placed row of them
    // sets off together and only drifts apart as the box turns them.
    agent.petrasyl_direction = agent.facing;
    agent.petrasyl_turn_timer = 0;
    agent.petrasyl_bob_offset =
        (static_cast<float>(utility::get_random_int2(0x1AABu)) + 1365.0F)
        / 4096.0F / 2.0F;
    // The same slow bob as the third: [1, 4) rather than [1, 7).
    agent.petrasyl_bob_speed =
        static_cast<float>(utility::get_random_int2(0x3000u)) / 4096.0F + 1.0F;
    agent.petrasyl_bob_angle = 0.0F;
    agent.petrasyl_secondary_timer = 10u * 2u;  // todo: FPS stuff
    // Enemy06Entity.EnemyInitialize ends by running UpdateState for state
    // zero, which is the fade-in arm: visible, and the fade timer set.
    agent.visible = true;
    agent.petrasyl_timer = 10u * 2u;  // todo: FPS stuff
}

} // namespace fruityprime::enemy::module_06_petrasyl4
