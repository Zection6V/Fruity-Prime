#include "33_GoreaMeteor.hpp"
#include "enemy_common.hpp"
#include "gorea_common.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace fruityprime::gameplay {

void Session::detonate_gorea_meteor(EnemyState& agent,
                                    std::uint16_t effect_id) {
    // Enemy33Entity always has the main player as its target.  A native
    // multiplayer session can have several objective players, so use the
    // nearest one while retaining the managed two-unit blast radius.
    const std::size_t target_index = gorea::nearest_player(
        players_, agent.position, 2.0F);
    if (target_index != players_.size()) {
        auto& target = players_[target_index];
        const net::Vec3 to_target = subtract(target.position, agent.position);
        const float distance = std::sqrt(std::max(
            0.0F, length_squared(to_target)));
        const bool blocked = collision::sweep_sphere(
            room_.collision(), to_collision(agent.position),
            to_collision(target.position), 0.01F, 0x2000).has_value();
        if (distance < 2.0F && !blocked) {
            std::uint16_t damage = 15;
            float force = 1.0F;
            // Enemy33Entity receives HitPlayers from the base overlap pass.
            // An overlap is full damage; a nearby player that was not inside
            // the meteor's one-unit hurt sphere gets the authored falloff.
            const bool overlapping = distance_squared(
                target.position, agent.position)
                <= (agent.body_radius + config_.body_radius)
                    * (agent.body_radius + config_.body_radius);
            if (!overlapping) {
                const float factor = std::clamp(distance / 2.0F,
                                                0.0F, 1.0F);
                damage = static_cast<std::uint16_t>(
                    static_cast<float>(damage)
                    - static_cast<float>(damage) * factor);
                // The managed code stores this through an integer temporary.
                // Keep the truncation rather than applying a floating-point
                // impulse to the player at the edge of the blast.
                force = static_cast<float>(static_cast<int>(
                    force - force * factor));
            }
            const net::Vec3 direction = distance > 1.0F / 128.0F
                ? multiply(to_target, 1.0F / distance)
                : net::Vec3{0.0F, 1.0F, 0.0F};
            target.speed = add(target.speed, multiply(direction, force));
            apply_enemy_contact_damage(agent, target, damage);
        }
    }

    spawn_effect(effect_id, agent.position, agent.facing, agent.id);
    agent.health = 0;
    agent.visible = false;
    agent.velocity = {};
    // Keep the record alive until update_gorea_meteor performs the same
    // Destroyed/spawner bookkeeping as the managed scene's next entity pass.
    agent.position.y = 524288.0F;
}

void Session::update_gorea_meteor(EnemyState& agent) {
    // Enemy33Entity is a homing, short-lived projectile emitted by Gorea2.
    // It is not an ordinary EnemyInstanceEntity target-and-contact enemy:
    // movement, beam traversal, radial damage and the two authored timers are
    // all part of its own process callback.
    auto release_parent_slot = [this, &agent]() {
        if (agent.parent_enemy_id == 0) {
            return;
        }
        const auto parent = std::find_if(
            enemies_.begin(), enemies_.end(),
            [parent_id = agent.parent_enemy_id](const EnemyState& value) {
                return value.id == parent_id
                    && value.enemy_type == static_cast<std::uint8_t>(
                        formats::EnemyType::Gorea2);
            });
        if (parent != enemies_.end() && parent->gorea_meteor_count > 0) {
            --parent->gorea_meteor_count;
        }
    };

    if (!agent.active) {
        return;
    }
    if (agent.health == 0) {
        release_parent_slot();
        agent.active = false;
        return;
    }
    if (!agent.visible) {
        agent.velocity = {};
        return;
    }

    const std::uint32_t step = gorea::frame_step(config_.tick_seconds);
    const float frame_scale = static_cast<float>(step);
    if (length_squared(agent.gorea_meteor_previous_position) <=
            std::numeric_limits<float>::epsilon()
        && length_squared(agent.position) >
            std::numeric_limits<float>::epsilon()) {
        agent.gorea_meteor_base_position = agent.position;
    }

    agent.gorea_meteor_previous_position = agent.position;
    const std::size_t target_index = gorea::nearest_player(
        players_, agent.position, std::numeric_limits<float>::max());
    if (target_index != players_.size()) {
        const net::Vec3 direction = gorea::aim_at(
            agent.position, players_[target_index].position, agent.facing);
        agent.facing = direction;
        agent.gorea_meteor_effect_facing = direction;
        agent.velocity = multiply(direction, (0.125F / 2.0F)
                                            * frame_scale);
        agent.gorea_meteor_base_position = add(
            agent.gorea_meteor_base_position, agent.velocity);
    } else {
        agent.velocity = {};
    }

    agent.gorea_meteor_rotation += 6.0F * frame_scale;
    while (agent.gorea_meteor_rotation >= 360.0F) {
        agent.gorea_meteor_rotation -= 360.0F;
    }
    if (agent.gorea_meteor_shake_timer > 0) {
        const auto jitter = [this]() {
            return (static_cast<float>(rng_.random2(512u)) - 256.0F)
                / 4096.0F;
        };
        agent.position = add(agent.gorea_meteor_base_position,
                             {jitter(), jitter(), jitter()});
        gorea::decrement(agent.gorea_meteor_shake_timer, step);
    } else {
        agent.position = agent.gorea_meteor_base_position;
    }

    const bool travelled = distance_squared(
        agent.gorea_meteor_previous_position, agent.position)
        > 1.0F / 128.0F;
    if (travelled && collision::sweep_sphere(
            room_.collision(), to_collision(agent.gorea_meteor_previous_position),
            to_collision(agent.position), 0.01F, 0x2000).has_value()) {
        detonate_gorea_meteor(agent, 178); // goreaMeteorHit
        return;
    }

    if (target_index != players_.size()) {
        const auto& target = players_[target_index];
        const float radius = agent.body_radius + config_.body_radius;
        if (distance_squared(target.position, agent.position)
                <= radius * radius) {
            detonate_gorea_meteor(agent, 176); // goreaMeteorDamage
            return;
        }
    }

    // State00/02 feed the two authored subroutine clocks.  The shorter clock
    // wins in the original class and produces the environment-hit effect.
    gorea::decrement(agent.gorea_state_timer, step);
    gorea::decrement(agent.gorea_aux_timer, step);
    if (agent.gorea_state_timer == 0 || agent.gorea_aux_timer == 0) {
        detonate_gorea_meteor(agent, 178); // goreaMeteorHit
    }
}

} // namespace fruityprime::gameplay

static_assert(fruityprime::enemy::module_33_gorea_meteor::kModule.managed_class.size() != 0);
