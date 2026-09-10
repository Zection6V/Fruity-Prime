#include "enemy_decode_common.hpp"
// Native port of src/MphRead/Entities/Enemies/11_Shriekbat.cs.
// This member definition lives in its enemy module; Session only dispatches it.
#include "11_Shriekbat.hpp"
#include "enemy_common.hpp"
#include "enemy_scene.hpp"
#include "Metadata/enemy_subroutines.hpp"

namespace fruityprime::gameplay {
namespace {

// Enemy11Entity's own numbers, in the order the managed file gives them.
constexpr float DescendSpeed = 0.3F;
constexpr float LungeSpeed = 0.6F;
constexpr std::uint32_t PauseFrames = 20u * 2u;  // todo: FPS stuff
constexpr std::uint32_t ContactDamageAmount = 20u;
// Metadata's shriekBatTrail.
constexpr std::uint32_t TrailEffect = 29u;
// The managed contact test is a volume overlap; this head has the enemy's
// body radius and the player's, which come to about this.
constexpr float ContactRadius = 1.6F;

// Native counterpart of Enemy11Entity.
//
// The managed class is an entity in a scene.  Here it is a view over the
// session's EnemyState for the frame being processed, carrying the same
// method names and the same bodies: the state is somebody else's storage,
// which is the one shape change the architecture forces.
//
// A Shriekbat hangs on the ceiling, drops to an authored point when a
// player enters its outer volume, pauses -- which is what makes it
// dodgeable -- and then dives at where the player is about to be.  It dies
// on whatever it hits, player or wall.  That is the whole enemy.
class Enemy11Entity final {
public:
    Enemy11Entity(const EnemyScene& scene, EnemyState& agent,
                  const net::PlayerState& main) noexcept
        : scene_(scene), agent_(agent), main_(main) {}

    // Enemy11Entity.EnemyProcess.
    void EnemyProcess();

private:
    // The five states, all of which run the subroutine.  The managed file
    // says so itself: "really no need for these to be separate functions".
    void State0();
    void State1() { State0(); }
    void State2() { State0(); }
    void State3() { State0(); }
    void State4() { State0(); }

    [[nodiscard]] bool Behavior00() const noexcept;
    [[nodiscard]] bool Behavior01();
    [[nodiscard]] bool Behavior02();
    [[nodiscard]] bool Behavior03();
    [[nodiscard]] bool Behavior04() const;

    void Die();
    void CallStateProcess();
    void StartMove(net::Vec3 destination, float per_frame);

    // EnemyInstanceEntity._moveTimer, which the descent and the dive share:
    // one covers both because the enemy is never doing them at once.
    [[nodiscard]] std::uint32_t& MoveTimer() const noexcept {
        return agent_.shriekbat_timer;
    }

    const EnemyScene& scene_;
    EnemyState& agent_;
    const net::PlayerState& main_;
};

void Enemy11Entity::StartMove(const net::Vec3 destination,
                              const float per_frame) {
    agent_.shriekbat_target = destination;
    const net::Vec3 delta = subtract(destination, agent_.position);
    const float magnitude = std::sqrt(std::max(0.0F, length_squared(delta)));
    if (magnitude <= 0.0001F) {
        MoveTimer() = 1u;
        agent_.velocity = {};
        return;
    }
    // The cartridge's own `(int)(mag / speed) + 1`, doubled because this
    // head runs at twice its rate -- and the speed halved to match, which
    // is the managed multiply-then-halve written once.
    MoveTimer() = (static_cast<std::uint32_t>(magnitude / per_frame) + 1u)
        * 2u;
    agent_.velocity = multiply(delta, per_frame / magnitude / 2.0F);
}

void Enemy11Entity::Die() {
    if (scene_.Damage) {
        scene_.Damage(agent_.id, std::max<std::uint32_t>(agent_.health, 1u));
    }
}

bool Enemy11Entity::Behavior00() const noexcept {
    // Attacking.  Never passes, which is why a Shriekbat that misses stays
    // in its dive rather than climbing back to the ceiling.
    return false;
}

bool Enemy11Entity::Behavior01() {
    // Start the dive, at where the player is about to be rather than where
    // they are: one unit behind their facing, half a unit up.
    if (MoveTimer() > 0) {
        --MoveTimer();
        return false;
    }
    StartMove({main_.position.x - main_.facing.x,
               main_.position.y + 0.5F,
               main_.position.z - main_.facing.z},
              LungeSpeed);
    return true;
}

bool Enemy11Entity::Behavior02() {
    // The pause before the dive.
    if (MoveTimer() > 0) {
        --MoveTimer();
        return false;
    }
    MoveTimer() = PauseFrames;
    agent_.velocity = {};
    return true;
}

bool Enemy11Entity::Behavior03() {
    // Drop to the authored attack position once the player is inside the
    // inner volume, trailing an effect on the way down.
    if (!agent_.shriekbat.active_volume.contains(
            to_volume_point(main_.position))) {
        return false;
    }
    if (scene_.SpawnEffect) {
        scene_.SpawnEffect(TrailEffect, agent_.position, agent_.id);
    }
    StartMove(add(agent_.behavior_origin, agent_.shriekbat.path_vector),
              DescendSpeed);
    return true;
}

bool Enemy11Entity::Behavior04() const {
    // Hang on the ceiling until the player is inside the outer volume.
    return agent_.shriekbat.range_volume.contains(
        to_volume_point(main_.position));
}

void Enemy11Entity::State0() {
    static_cast<void>(metadata::call_subroutine(
        metadata::Enemy11Subroutines, agent_.sub_id, agent_.next_state,
        [this](std::uint8_t index) {
            switch (index) {
            case 0: return Behavior00();
            case 1: return Behavior01();
            case 2: return Behavior02();
            case 3: return Behavior03();
            case 4: return Behavior04();
            default: return false;
            }
        }));
}

void Enemy11Entity::CallStateProcess() {
    switch (agent_.state) {
    case 0: State0(); break;
    case 1: State1(); break;
    case 2: State2(); break;
    case 3: State3(); break;
    case 4: State4(); break;
    default: break;
    }
}

void Enemy11Entity::EnemyProcess() {
    agent_.facing = normalized_or(
        subtract(main_.position, agent_.position), agent_.facing);
    if (distance_squared(main_.position, agent_.position)
            <= ContactRadius * ContactRadius) {
        if (scene_.ContactDamage) {
            scene_.ContactDamage(agent_, main_.slot_index,
                                 ContactDamageAmount);
        }
        Die();
        return;
    }
    if (agent_.state == 4 && scene_.Blocked
        && scene_.Blocked(agent_.position,
                          add(agent_.position, agent_.velocity),
                          agent_.body_radius)) {
        Die();
        return;
    }
    CallStateProcess();
}

} // namespace

void Session::update_shriekbat(EnemyState& agent) {
    if (!agent.shriekbat.supported) {
        return;
    }
    // A Shriekbat aims at one player and keeps aiming at them.  The managed
    // game has only PlayerEntity.Main; here the nearest live player stands
    // in for it, chosen once and then held, because a dive that changes its
    // mind halfway is not a dive the cartridge ever makes.
    const net::PlayerState* main = nullptr;
    if (agent.target_slot != 0xff) {
        for (const auto& player : players_) {
            if (player.slot_index == agent.target_slot
                && objective_player(player)) {
                main = &player;
                break;
            }
        }
    }
    if (main == nullptr) {
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
    }
    if (main == nullptr) {
        return;
    }
    agent.target_slot = main->slot_index;

    // EnemyInstanceEntity.BaseProcess: the transition decided last frame is
    // taken now, and then the enemy moves by the speed a behaviour set.
    agent.state = agent.next_state;
    agent.sub_id = agent.state;
    agent.position = add(agent.position, agent.velocity);

    EnemyScene scene;
    scene.Damage = [this](std::uint32_t id, std::uint32_t damage) {
        static_cast<void>(damage_enemy(id, damage));
    };
    scene.SpawnEffect = [this](std::uint32_t effect, net::Vec3 position,
                               std::uint32_t owner) {
        static_cast<void>(spawn_effect(effect, position, {1.0F, 0.0F, 0.0F},
                                       owner, 0.75F));
    };
    scene.ContactDamage = [this](EnemyState& target, std::uint8_t slot,
                                 std::uint32_t damage) {
        for (auto& player : players_) {
            if (player.slot_index == slot) {
                apply_enemy_contact_damage(target, player, damage);
                return;
            }
        }
    };
    scene.Blocked = [this](net::Vec3 from, net::Vec3 to, float radius) {
        return collision::sweep_sphere(room_.collision(), to_collision(from),
                                       to_collision(to), radius, 0x2000)
            .has_value();
    };

    Enemy11Entity(scene, agent, *main).EnemyProcess();
}
} // namespace fruityprime::gameplay

namespace fruityprime::enemy::module_11_shriekbat {

void EnemyInitialize(gameplay::EnemyState& agent) noexcept {
    // Twelve energy, which one charged shot covers: a Shriekbat is meant
    // to be killed on the way in or not at all.
    agent.health = agent.health_max = 12;
    agent.body_radius = 1.0F;
    agent.state = agent.next_state = agent.sub_id = 0;
    agent.shriekbat_timer = 0;
    agent.shriekbat_target = agent.position;
    agent.behavior_origin = agent.position;
}

} // namespace fruityprime::enemy::module_11_shriekbat

namespace fruityprime::enemy {

ShriekbatProfile decode_shriekbat_profile(
    std::uint8_t id, std::span<const std::uint8_t> fields,
    net::Vec3 origin) noexcept {
    ShriekbatProfile result;
    if (id != static_cast<std::uint8_t>(formats::EnemyType::Shriekbat)
        || fields.size() < 204) {
        return result;
    }
    result.hurt_volume = detail::read_volume(fields, 0, origin);
    result.path_vector = detail::read_vector(fields, 64);
    result.range_volume = detail::read_volume(fields, 76, origin);
    result.active_volume = detail::read_volume(fields, 140, origin);
    result.supported = result.hurt_volume.kind != scene::VolumeKind::Invalid
        && result.range_volume.kind != scene::VolumeKind::Invalid
        && result.active_volume.kind != scene::VolumeKind::Invalid;
    return result;
}

} // namespace fruityprime::enemy
