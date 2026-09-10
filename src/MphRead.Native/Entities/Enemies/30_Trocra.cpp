#include "Utility/rng.hpp"
#include "Entities/gameplay.hpp"
#include "30_Trocra.hpp"
#include "gorea_common.hpp"

#include <algorithm>
#include <cmath>

namespace fruityprime::gameplay {

void Session::update_trocra(EnemyState& agent) {
    // Enemy30Entity is a power-bomb/crystal projectile. Its process method
    // only checks player/beam traversal and detonates; it does not run the
    // normal EnemyInstance target-and-contact controller.
    if (agent.health == 0) {
        // Enemy30 reports Destroyed from EnemyTakeDamage on the frame after
        // DieAndSpawnEffect hides it. Keep that one-frame boundary so Gorea1B
        // can still observe the crystal and so item rolls happen at the same
        // lifecycle point as the managed class.
        if (!agent.trocra_destroy_processed) {
            agent.trocra_destroy_processed = true;
            const auto drop_roll = rng_.random2(190u);
            if (drop_roll < 10u) {
                spawn_item_drop(ItemType::HealthSmall, agent.position);
            } else if (drop_roll < 70u) {
                spawn_item_drop(ItemType::UASmall, agent.position);
            }
            const auto id = agent.id;
            static_cast<void>(destroy_enemy(id));
        }
        return;
    }
    if (!agent.visible) {
        agent.velocity = {};
        return;
    }

    const std::uint32_t step = gorea::frame_step(config_.tick_seconds);
    agent.trocra_previous_position = agent.position;
    if (agent.trocra_state == 3) {
        // Enemy30 uses EnemyInstanceEntity's ordinary per-frame speed.  The
        // native boss controllers store that same quantity in velocity, so
        // this is deliberately a direct per-frame advance rather than a
        // second * velocity conversion.
        agent.position = add(agent.position, agent.velocity);
    }

    const bool travelled = distance_squared(agent.trocra_previous_position,
                                            agent.position) > 1.0F / 128.0F;
    const bool hit_surface = travelled
        && collision::sweep_sphere(
               room_.collision(), to_collision(agent.trocra_previous_position),
               to_collision(agent.position), 0.01F, 0x2000)
               .has_value();

    const std::size_t target_index = gorea::nearest_player(
        players_, agent.position, 2.0F);
    bool detonate = hit_surface;
    if (target_index != players_.size()) {
        const float distance = std::sqrt(distance_squared(
            players_[target_index].position, agent.position));
        detonate = detonate || distance <= 2.0F;
    }
    if (!detonate) {
        return;
    }

    // DieAndSpawnEffect(164) is used both for a player hit and for a crystal
    // crossing a beam collision surface.  The travel collision is also the
    // visibility gate for the radial player damage in the managed code.
    spawn_effect(164, agent.position, {1.0F, 0.0F, 0.0F}, agent.id);
    SoundEvent sound;
    sound.cue = SoundCue::GoreaAttack3B;
    sound.enemy_type = agent.enemy_type;
    sound.entity_id = agent.id;
    sound.position = agent.position;
    emit_sound(sound);

    if (target_index != players_.size()) {
        auto& target = players_[target_index];
        const float distance = std::sqrt(distance_squared(
            target.position, agent.position));
        const auto line_hit = travelled
            && collision::sweep_sphere(
                   room_.collision(),
                   to_collision(agent.trocra_previous_position),
                   to_collision(agent.position), 0.01F, 0x2000)
                   .has_value();
        // HitPlayers is set by the managed overlap pass before EnemyProcess.
        // The native simulation has no separate overlap array, so the same
        // condition is represented by the authored Trocra hurt sphere plus
        // the player body radius.
        const bool overlapping = distance <= agent.body_radius
            + config_.body_radius;
        if (!line_hit) {
            float factor = std::clamp(distance / 2.0F, 0.0F, 1.0F);
            std::uint16_t damage = 15;
            float force = 1.0F;
            if (!overlapping) {
                damage = static_cast<std::uint16_t>(std::max(
                    0L, 15L - std::lround(15.0F - 15.0F * factor)));
                force -= factor;
            }
            net::Vec3 impulse;
            if (distance > 1.0F / 128.0F) {
                impulse = multiply(
                    multiply(subtract(target.position, agent.position),
                             1.0F / distance), force);
            } else {
                impulse = {0.0F, force, 0.0F};
            }
            apply_enemy_contact_damage(agent, target, damage);
            target.speed = add(target.speed, impulse);
            target.hit_direction = impulse;
        }
    }

    agent.health = 0;
    agent.visible = false;
    agent.trocra_destroy_processed = false;
    agent.velocity = {};
    agent.position.y = 524288.0F;
}

} // namespace fruityprime::gameplay

namespace fruityprime::enemy::module_30_trocra {

namespace {

// Metadata's goreaCrystalHit and goreaCrystalExplode.
constexpr std::uint32_t CrystalHitEffect = 164u;
constexpr std::uint32_t CrystalExplodeEffect = 75u;
constexpr float BurstRadius = 2.0F;
constexpr int BurstDamage = 15;
// Far enough away that nothing will reach it again: the cartridge parks a
// spent crystal rather than removing it.
constexpr float ParkedHeight = 524288.0F;

} // namespace

void EnemyInitialize(gameplay::EnemyState& agent) noexcept {
    agent.health = agent.health_max = 15;
    agent.body_radius = 1.0F;
    agent.trocra_state = 0;
    agent.trocra_previous_position = agent.position;
}

void SetSpeed(gameplay::EnemyState& agent, const net::Vec3 speed) noexcept {
    agent.velocity = speed;
}

bool EnemyProcess(gameplay::EnemyState& agent, const bool touching,
                  const bool blocked) noexcept {
    if (!agent.visible || agent.health == 0) {
        return false;
    }
    // Travelled far enough to be worth tracing: a crystal moving a
    // fraction of a unit cannot have gone through anything.
    const float x = agent.trocra_previous_position.x - agent.position.x;
    const float y = agent.trocra_previous_position.y - agent.position.y;
    const float z = agent.trocra_previous_position.z - agent.position.z;
    const bool travelled = x * x + y * y + z * z > 1.0F / 128.0F;
    const bool dies = touching || (travelled && blocked);
    agent.trocra_previous_position = agent.position;
    return dies;
}

Burst DieAndSpawnEffect(gameplay::EnemyState& agent,
                        const net::Vec3 player_position,
                        const bool touching, const bool blocked) noexcept {
    Burst burst;
    const float x = player_position.x - agent.position.x;
    const float y = player_position.y - agent.position.y;
    const float z = player_position.z - agent.position.z;
    const float distance = std::sqrt(x * x + y * y + z * z);
    if (distance < BurstRadius && !blocked) {
        burst.hit = true;
        burst.damage = BurstDamage;
        float force = 1.0F;
        if (!touching) {
            // Not actually touched: the blast falls off over the two
            // units, in damage and in shove alike.
            const float factor = std::clamp(distance / BurstRadius, 0.0F,
                                            1.0F);
            burst.damage -= static_cast<int>(std::lround(
                static_cast<float>(BurstDamage)
                - static_cast<float>(BurstDamage) * factor));
            force -= factor;
        }
        if (distance > 1.0F / 128.0F) {
            burst.knockback = {x / distance * force, y / distance * force,
                               z / distance * force};
        } else {
            // Standing exactly on it: there is no direction, so up.
            burst.knockback = {0.0F, force, 0.0F};
        }
    }
    // Spent.  Parked out of reach rather than removed, which is what the
    // cartridge does.
    agent.health = 0;
    agent.visible = false;
    agent.invulnerable = true;
    agent.position = {agent.position.x, ParkedHeight, agent.position.z};
    agent.velocity = {};
    agent.trocra_state = 1;
    return burst;
}

void Explode(gameplay::EnemyState& agent) noexcept {
    static_cast<void>(CrystalHitEffect);
    static_cast<void>(CrystalExplodeEffect);
    static_cast<void>(agent);
}

Drop EnemyTakeDamage(gameplay::EnemyState& agent) noexcept {
    if (agent.health != 0) {
        return Drop::None;
    }
    const std::uint32_t roll = utility::get_random_int2(190u);
    if (roll < 10u) {
        return Drop::HealthSmall;
    }
    if (roll < 70u) {
        return Drop::UASmall;
    }
    return Drop::None;
}

} // namespace fruityprime::enemy::module_30_trocra


static_assert(fruityprime::enemy::module_30_trocra::kModule.managed_class.size() != 0);
