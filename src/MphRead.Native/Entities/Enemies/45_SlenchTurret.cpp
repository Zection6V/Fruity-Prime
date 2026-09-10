// Native counterpart of src/MphRead/Entities/Enemies/45_SlenchTurret.cs.
// This member definition lives in its enemy module; Session only dispatches it.
#include <cmath>
#include <limits>
#include "Utility/rng.hpp"
#include "Metadata/enemy_subroutines.hpp"
#include "Metadata/enemy_values.hpp"
#include "enemy_scene.hpp"
#include "45_SlenchTurret.hpp"
#include "enemy_common.hpp"

namespace fruityprime::gameplay {

namespace {

// Scene.SpawnEffect(109): eyeTurretCharge, the flare a turret shows just
// before it fires.  It is the only warning the player gets.
constexpr std::uint32_t EyeTurretChargeEffect = 109u;

// Native counterpart of Enemy45Entity.
//
// A turret on a Slench's wall.  It does not aim: it watches one authored
// volume, and while a player is inside it, charges and fires straight at
// them until they leave.  Three of its four states run the same body --
// the machine's shape is in the subroutine table rather than in the
// states.
class Enemy45Entity final {
public:
    Enemy45Entity(const EnemyScene& scene, EnemyState& agent,
                  const net::PlayerState* main) noexcept
        : scene_(scene), agent_(agent), main_(main) {}

    // Enemy45Entity.EnemyProcess.
    void EnemyProcess();

    // Enemy45Entity.EnemyInitialize.
    void EnemyInitialize();

    // Enemy45Entity.HandleMessage.
    void HandleMessage(std::uint16_t message, std::int32_t parameter1);

    // Enemy45Entity.SetAnimation and SetAnimationReverse.
    void SetAnimation();
    void SetAnimationReverse();

    // Enemy45Entity.GetMaxFrameCount.
    [[nodiscard]] std::int32_t GetMaxFrameCount() const noexcept {
        return agent_.turret_anim_max_frame;
    }

    // Enemy45Entity.Index.
    [[nodiscard]] std::int32_t Index() const noexcept {
        return agent_.turret_index;
    }

    // Scene.FrameCount, which UpdateAnimationFrame reads: the lights move
    // on even frames only.
    void set_frame_count(std::uint64_t frames) noexcept {
        frame_count_ = frames;
    }

private:
    // Enemy45Entity.UpdateShotCount.
    void UpdateShotCount();
    // Enemy45Entity.UpdateAnimationFrame.
    void UpdateAnimationFrame();
    // Enemy45Entity.SpawnChargeEffect.
    void SpawnChargeEffect() const;

    void State0();
    void State2();

    [[nodiscard]] bool Behavior00();
    [[nodiscard]] bool Behavior01() const noexcept { return false; }
    [[nodiscard]] bool Behavior02() const noexcept;
    [[nodiscard]] bool Behavior03();
    [[nodiscard]] bool Behavior04();

    [[nodiscard]] bool CallSubroutine();
    void CallStateProcess();

    void ContactDamagePlayer(std::uint32_t damage, bool knockback) const;

    [[nodiscard]] const metadata::Enemy45Values& values() const noexcept {
        const std::size_t index = std::min<std::size_t>(
            agent_.turret_subtype, metadata::Enemy45ValuesTable.size() - 1);
        return metadata::Enemy45ValuesTable[index];
    }

    [[nodiscard]] bool InRange() const noexcept {
        return main_ != nullptr && main_->health != 0
            && agent_.turret.range_volume.contains(
                to_volume_point(main_->position));
    }

    const EnemyScene& scene_;
    EnemyState& agent_;
    const net::PlayerState* main_;
    std::uint64_t frame_count_ = 0;
};

void Enemy45Entity::EnemyInitialize() {
    // State three is off.  A Slench turret is created dark and is woken by
    // the synapse graph, unlike a standalone Alimbic one.
    agent_.state = agent_.next_state = agent_.sub_id = 3;
    agent_.turret_enabled = false;
    agent_.visible = true;
    agent_.invulnerable = true;
    agent_.body_radius = 1.0F;
    agent_.turret_subtype = static_cast<std::uint8_t>(agent_.turret.subtype);
    const metadata::Enemy45Values& v = values();
    agent_.health = agent_.health_max = v.Health;
    agent_.scan_id = static_cast<std::uint16_t>(v.ScanId);
    agent_.turret_index = agent_.turret.index;
    agent_.turret_shot_timer = 0;
    agent_.turret_salvo_cooldown = 0;
    UpdateShotCount();
    agent_.turret_anim_frame = 0;
    agent_.turret_anim_frame_count = agent_.turret_anim_max_frame;
    agent_.turret_anim_interval = 1;
    agent_.turret_anim_delay_timer = agent_.turret_anim_interval;
    agent_.turret_animating = false;
    agent_.turret_anim_reverse = false;
}

void Enemy45Entity::UpdateShotCount() {
    const metadata::Enemy45Values& v = values();
    const std::uint32_t span = v.MaxShots + 1u > v.MinShots
        ? static_cast<std::uint32_t>(v.MaxShots + 1 - v.MinShots) : 1u;
    agent_.turret_burst_remaining = static_cast<std::uint16_t>(
        v.MinShots + utility::get_random_int2(span));
}

void Enemy45Entity::EnemyProcess() {
    // State three does nothing at all -- not even take contact damage,
    // which is why walking into a dark turret is safe.
    if (agent_.state == 3) {
        return;
    }
    UpdateAnimationFrame();
    ContactDamagePlayer(values().ContactDamage, true);
    CallStateProcess();
}

// The lights, opening and closing.  Every other frame, because the
// cartridge runs at half this rate.
void Enemy45Entity::UpdateAnimationFrame() {
    if (!agent_.turret_animating || frame_count_ == 0
        || frame_count_ % 2 != 0) {  // todo: FPS stuff
        return;
    }
    if (!agent_.turret_anim_reverse) {
        const std::int32_t frame = agent_.turret_anim_frame;
        if (frame >= agent_.turret_anim_frame_count) {
            // Past the ceiling means a synapse lowered it while the lights
            // were still coming up; snap back down to it rather than
            // leaving one lit that should not be.
            if (frame > agent_.turret_anim_frame_count) {
                agent_.turret_anim_frame = agent_.turret_anim_frame_count;
            }
            agent_.turret_animating = false;
        } else if (agent_.turret_anim_delay_timer != 0) {
            --agent_.turret_anim_delay_timer;
        } else {
            agent_.turret_anim_frame = frame + 1;
            agent_.turret_anim_delay_timer = agent_.turret_anim_interval;
        }
    } else if (agent_.turret_anim_frame != 0) {
        if (agent_.turret_anim_delay_timer != 0) {
            --agent_.turret_anim_delay_timer;
        } else {
            --agent_.turret_anim_frame;
            agent_.turret_anim_delay_timer = agent_.turret_anim_interval;
        }
    } else {
        // All the way shut: open again.  That is what makes a firing
        // turret pulse rather than sit open.
        SetAnimation();
    }
}

void Enemy45Entity::SetAnimation() {
    agent_.turret_anim_interval = 1;
    agent_.turret_anim_reverse = false;
    agent_.turret_animating = true;
}

void Enemy45Entity::SetAnimationReverse() {
    agent_.turret_anim_interval = 1;
    agent_.turret_anim_reverse = true;
    agent_.turret_animating = true;
}

// States nought, one and three all run this.
void Enemy45Entity::State0() {
    static_cast<void>(CallSubroutine());
}

void Enemy45Entity::State2() {
    if (agent_.turret_burst_remaining != 0 && agent_.turret_shot_timer != 0) {
        --agent_.turret_shot_timer;
    } else {
        if (main_ != nullptr && scene_.SpawnProjectile) {
            // Half a unit up: it shoots at the player's chest rather than
            // at their feet.
            const net::Vec3 target{main_->position.x, main_->position.y + 0.5F,
                                   main_->position.z};
            net::Vec3 direction{target.x - agent_.position.x,
                                target.y - agent_.position.y,
                                target.z - agent_.position.z};
            const float length = std::sqrt(direction.x * direction.x
                                           + direction.y * direction.y
                                           + direction.z * direction.z);
            if (length > 0.0F) {
                direction = {direction.x / length, direction.y / length,
                             direction.z / length};
            } else {
                direction = agent_.facing;
            }
            SetAnimationReverse();
            scene_.SpawnProjectile(agent_, agent_.position, direction);
        }
        if (agent_.turret_burst_remaining != 0) {
            --agent_.turret_burst_remaining;
        }
        agent_.turret_shot_timer =
            static_cast<std::uint32_t>(values().ShotCooldown) * 2u;  // todo: FPS
    }
    static_cast<void>(CallSubroutine());
}

void Enemy45Entity::SpawnChargeEffect() const {
    if (scene_.SpawnEffect) {
        scene_.SpawnEffect(EyeTurretChargeEffect, agent_.position, agent_.id);
    }
}

// A player walked into the volume.
bool Enemy45Entity::Behavior00() {
    if (!InRange()) {
        return false;
    }
    SpawnChargeEffect();
    return true;
}

// A player left it.
bool Enemy45Entity::Behavior02() const noexcept {
    return !InRange();
}

// Out of shots: charge again and roll the next burst.
bool Enemy45Entity::Behavior03() {
    if (agent_.turret_burst_remaining != 0) {
        return false;
    }
    SpawnChargeEffect();
    UpdateShotCount();
    return true;
}

bool Enemy45Entity::Behavior04() {
    if (agent_.turret_salvo_cooldown != 0) {
        --agent_.turret_salvo_cooldown;
        return false;
    }
    agent_.turret_salvo_cooldown =
        static_cast<std::uint32_t>(values().SalvoCooldown) * 2u;  // todo: FPS
    return true;
}

void Enemy45Entity::HandleMessage(const std::uint16_t message,
                                  const std::int32_t parameter1) {
    if (message == static_cast<std::uint16_t>(
            formats::Message::ActivateTurret)) {
        agent_.sub_id = agent_.next_state = 0;
        agent_.turret_enabled = true;
        SetAnimation();
    } else if (message == static_cast<std::uint16_t>(
                   formats::Message::DeactivateTurret)) {
        agent_.sub_id = agent_.next_state = 3;
        agent_.turret_enabled = false;
    } else if (message == static_cast<std::uint16_t>(
                   formats::Message::DecreaseTurretLights)) {
        if (agent_.turret_anim_frame_count != 0) {
            agent_.turret_anim_frame_count -= parameter1;
        }
        if (!agent_.turret_animating) {
            agent_.turret_anim_frame = agent_.turret_anim_frame_count;
        }
    } else if (message == static_cast<std::uint16_t>(
                   formats::Message::IncreaseTurretLights)) {
        const std::int32_t max_frame = agent_.turret_anim_max_frame;
        if (agent_.turret_anim_frame_count < max_frame) {
            agent_.turret_anim_frame_count += parameter1;
        }
        if (agent_.turret_anim_frame_count > max_frame) {
            agent_.turret_anim_frame_count = max_frame;
        }
        if (!agent_.turret_animating) {
            agent_.turret_anim_frame = agent_.turret_anim_frame_count;
        }
    }
}

void Enemy45Entity::ContactDamagePlayer(const std::uint32_t damage,
                                        const bool knockback) const {
    if (main_ == nullptr || !scene_.ContactDamage) {
        return;
    }
    const float reach = agent_.body_radius + 0.45F;
    if (distance_squared(agent_.position, main_->position) > reach * reach) {
        return;
    }
    static_cast<void>(knockback);
    scene_.ContactDamage(agent_, main_->slot_index, damage);
}

bool Enemy45Entity::CallSubroutine() {
    return metadata::call_subroutine(
        metadata::Enemy45Subroutines, agent_.sub_id, agent_.next_state,
        [this](std::uint8_t index) {
            switch (index) {
            case 0: return Behavior00();
            case 1: return Behavior01();
            case 2: return Behavior02();
            case 3: return Behavior03();
            case 4: return Behavior04();
            default: return false;
            }
        });
}

void Enemy45Entity::CallStateProcess() {
    switch (agent_.state) {
    case 0: State0(); break;
    case 1: State0(); break;
    case 2: State2(); break;
    case 3: State0(); break;
    default: break;
    }
}

} // namespace

void Session::update_slench_turret(EnemyState& agent) {
    if (!agent.turret.supported) {
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
    if (main != nullptr && agent.state == 2) {
        agent.target_slot = main->slot_index;
    }

    // EnemyInstanceEntity.BaseProcess.  A turret is bolted to a wall, so
    // there is no movement half.
    agent.state = agent.next_state;
    agent.sub_id = agent.state;

    EnemyScene scene = build_enemy_scene(agent.position);
    scene.SpawnProjectile = [this](const EnemyState& shooter,
                                   net::Vec3 position, net::Vec3 direction) {
        spawn_enemy_projectile(shooter, position, direction,
                               SoundCue::TurretAttack);
    };
    scene.SpawnEffect = [this](std::uint32_t effect, net::Vec3 position,
                               std::uint32_t owner) {
        static_cast<void>(spawn_effect(
            static_cast<std::uint16_t>(effect), position, {1.0F, 0.0F, 0.0F},
            owner));
    };
    Enemy45Entity entity(scene, agent, main);
    entity.set_frame_count(tick_count_);
    entity.EnemyProcess();
}

} // namespace fruityprime::gameplay

static_assert(fruityprime::enemy::module_45_slench_turret::kModule.managed_class.size() != 0);

namespace fruityprime::enemy::module_45_slench_turret {

void EnemyInitialize(gameplay::EnemyState& agent) noexcept {
    gameplay::EnemyScene empty;
    gameplay::Enemy45Entity(empty, agent, nullptr).EnemyInitialize();
}

void HandleMessage(gameplay::EnemyState& agent, const std::uint16_t message,
                   const std::int32_t parameter1) noexcept {
    gameplay::EnemyScene empty;
    gameplay::Enemy45Entity(empty, agent, nullptr)
        .HandleMessage(message, parameter1);
}

void SetMaxFrameCount(gameplay::EnemyState& agent,
                      const std::int32_t frames) noexcept {
    agent.turret_anim_max_frame = frames;
    if (agent.turret_anim_frame_count > frames) {
        agent.turret_anim_frame_count = frames;
    }
}

} // namespace fruityprime::enemy::module_45_slench_turret
