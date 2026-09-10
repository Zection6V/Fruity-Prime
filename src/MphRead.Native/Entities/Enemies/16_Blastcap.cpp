// Native port of src/MphRead/Entities/Enemies/16_Blastcap.cs.
// This member definition lives in its enemy module; Session only dispatches it.
#include "16_Blastcap.hpp"
#include "enemy_common.hpp"
#include "enemy_scene.hpp"
#include "Metadata/enemy_subroutines.hpp"

namespace fruityprime::gameplay {

// Enemy16Entity's own numbers.  The two frame counts are out here because
// the initialise below sets them.
constexpr std::uint32_t BlastcapAgitateFrames = 60u * 2u;  // todo: FPS stuff
constexpr std::uint32_t BlastcapCloudFrames = 150u * 2u;   // todo: FPS stuff

namespace {

constexpr float NearRadius = 8.0F;
constexpr float CloudRadius = 2.0F;
constexpr std::uint32_t AgitateFrames = BlastcapAgitateFrames;
constexpr std::uint32_t CloudTickPeriod = 10u * 2u;
constexpr std::uint16_t CloudDamage = 2u;
// Metadata's blastCapBlow.
constexpr std::uint32_t BlowEffect = 4u;
// A player's own body, which every proximity test here counts.
constexpr float PlayerBodyRadius = 0.45F;

// Native counterpart of Enemy16Entity.
//
// A Blastcap does nothing but wait.  Shoot it, walk into it, or stand near
// it long enough and it bursts into a cloud that keeps hurting for five
// seconds -- and the burst is not a death: the enemy survives at one energy
// with its collision and visibility switched off, which is what lets the
// cloud outlive the mushroom.
class Enemy16Entity final {
public:
    Enemy16Entity(const EnemyScene& scene, EnemyState& agent,
                  const net::PlayerState* main) noexcept
        : scene_(scene), agent_(agent), main_(main) {}

    // Enemy16Entity.EnemyProcess.
    void EnemyProcess() { CallStateProcess(); }

    // Enemy16Entity.EnemyTakeDamage.  Returns whether the enemy is
    // unaffected, which is what the managed signature means.
    bool EnemyTakeDamage();

private:
    void State0();
    void State1() { State0(); }
    void State2() { State0(); }
    // The cloud counts its own ticks before running the subroutine, which
    // is what makes its damage periodic rather than every frame.
    void State3() {
        ++agent_.blastcap_cloud_tick;
        State0();
    }

    [[nodiscard]] bool Behavior00();
    [[nodiscard]] bool Behavior01();
    [[nodiscard]] bool Behavior02();
    [[nodiscard]] bool Behavior03();
    [[nodiscard]] bool Behavior04();
    [[nodiscard]] bool Behavior05();
    [[nodiscard]] bool Behavior06();

    void CallStateProcess();

    // Whether the player is within `radius` of this Blastcap, counting
    // their own body.  Every one of the behaviours asks this.
    [[nodiscard]] bool PlayerWithin(float radius) const noexcept;

    const EnemyScene& scene_;
    EnemyState& agent_;
    const net::PlayerState* main_;
};

bool Enemy16Entity::PlayerWithin(const float radius) const noexcept {
    if (main_ == nullptr) {
        return false;
    }
    const float combined = radius + PlayerBodyRadius;
    return distance_squared(agent_.position, main_->position)
        < combined * combined;
}

bool Enemy16Entity::EnemyTakeDamage() {
    if (agent_.health != 0) {
        return false;
    }
    // The burst is not a death.  One energy, no collision, no model: the
    // cloud is what is left, and it has its own state to run.
    agent_.health = 1;
    agent_.blastcap_next_state = 3;
    agent_.sub_id = 3;
    agent_.invulnerable = true;
    agent_.visible = false;
    agent_.blastcap_exploded = true;
    if (scene_.SpawnEffect) {
        scene_.SpawnEffect(BlowEffect, agent_.position, agent_.id);
    }
    if (!agent_.blastcap_initial_cloud_hit && PlayerWithin(CloudRadius)
        && main_ != nullptr && scene_.ContactDamage) {
        // The first tick of the cloud ignores damage invulnerability, so
        // shooting one at point blank always costs something.
        agent_.blastcap_initial_cloud_hit = true;
        scene_.ContactDamage(agent_, main_->slot_index, CloudDamage);
    }
    return false;
}

bool Enemy16Entity::Behavior00() {
    // The cloud, every ten of the cartridge's frames.
    if (agent_.blastcap_cloud_tick % CloudTickPeriod != 0) {
        return false;
    }
    if (!PlayerWithin(CloudRadius) || main_ == nullptr) {
        return false;
    }
    agent_.blastcap_initial_cloud_hit = true;
    if (scene_.ContactDamage) {
        scene_.ContactDamage(agent_, main_->slot_index, CloudDamage);
    }
    return true;
}

bool Enemy16Entity::Behavior01() {
    // The cloud running out.  Leaving the health at zero is how it asks to
    // be destroyed, on the following pass rather than this one.
    if (agent_.blastcap_cloud_timer > 0) {
        --agent_.blastcap_cloud_timer;
    } else {
        agent_.health = 0;
    }
    return false;
}

bool Enemy16Entity::Behavior02() {
    // The player has left: settle back down.
    if (PlayerWithin(NearRadius)) {
        return false;
    }
    agent_.blastcap_animation = 2;
    return true;
}

bool Enemy16Entity::Behavior03() {
    // Walked into.
    if (main_ == nullptr || !PlayerWithin(CloudRadius + PlayerBodyRadius)) {
        return false;
    }
    agent_.blastcap_initial_cloud_hit = true;
    if (scene_.ContactDamage) {
        scene_.ContactDamage(agent_, main_->slot_index, CloudDamage);
    }
    if (scene_.Damage) {
        // A hundred, which is well past its twelve: a Blastcap walked into
        // bursts rather than being whittled down.
        scene_.Damage(agent_.id, 100u);
    }
    return true;
}

bool Enemy16Entity::Behavior04() {
    // The agitated wobble has finished playing.
    if (!agent_.blastcap_animation_ended) {
        return false;
    }
    agent_.blastcap_animation = 2;
    return true;
}

bool Enemy16Entity::Behavior05() {
    // The player has come near: start swelling.
    if (!PlayerWithin(NearRadius)) {
        return false;
    }
    agent_.blastcap_animation = 1;
    return true;
}

bool Enemy16Entity::Behavior06() {
    // Nothing has happened for two seconds, so wobble.
    if (agent_.blastcap_agitate_timer > 0) {
        --agent_.blastcap_agitate_timer;
        return false;
    }
    agent_.blastcap_agitate_timer = AgitateFrames;
    agent_.blastcap_animation = 0;
    agent_.blastcap_animation_no_loop = true;
    agent_.blastcap_animation_ended = false;
    return true;
}

void Enemy16Entity::State0() {
    static_cast<void>(metadata::call_subroutine(
        metadata::Enemy16Subroutines, agent_.sub_id,
        agent_.blastcap_next_state,
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
        }));
}

void Enemy16Entity::CallStateProcess() {
    switch (agent_.state) {
    case 0: State0(); break;
    case 1: State1(); break;
    case 2: State2(); break;
    case 3: State3(); break;
    default: break;
    }
}

} // namespace

void Session::update_blastcap(EnemyState& agent) {
    // EnemyInstanceEntity.BaseProcess: last frame's decision is taken now.
    agent.state = agent.blastcap_next_state;
    agent.sub_id = agent.state;

    // The cloud's expiry leaves the health at zero and the destruction
    // happens on the pass after, which is the managed boundary.
    if (agent.blastcap_exploded && agent.health == 0) {
        static_cast<void>(destroy_enemy(agent.id));
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
                apply_enemy_contact_damage(
                    target, player, static_cast<std::uint16_t>(damage));
                return;
            }
        }
    };

    Enemy16Entity blastcap(scene, agent, main);
    // Enemy16Entity.EnemyTakeDamage runs when something has brought the
    // health to zero, which is what starts the cloud.
    if (agent.health == 0 && !agent.blastcap_exploded) {
        static_cast<void>(blastcap.EnemyTakeDamage());
    }
    blastcap.EnemyProcess();
}
} // namespace fruityprime::gameplay

namespace fruityprime::enemy::module_16_blastcap {

void EnemyInitialize(gameplay::EnemyState& agent) noexcept {
    agent.health = agent.health_max = 12;
    agent.body_radius = 1.0F;
    agent.state = agent.blastcap_next_state = agent.sub_id = 0;
    agent.blastcap_agitate_timer = gameplay::BlastcapAgitateFrames;
    agent.blastcap_cloud_timer = gameplay::BlastcapCloudFrames;
    agent.blastcap_cloud_tick = 0;
    agent.blastcap_animation = 2;
    agent.blastcap_animation_no_loop = false;
    agent.blastcap_animation_ended = false;
    agent.blastcap_initial_cloud_hit = false;
    agent.blastcap_exploded = false;
}

} // namespace fruityprime::enemy::module_16_blastcap
