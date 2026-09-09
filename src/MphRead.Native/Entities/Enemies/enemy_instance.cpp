// Native port of the shared EnemyInstanceEntity fallback used by the
// individual enemy classes.
#include "enemy_common.hpp"

namespace fruityprime::gameplay {

bool Session::update_generic_enemy(EnemyState& agent) {
    if (!agent.active || agent.health == 0) {
        agent.active = false;
        return true;
    }
    const float seconds = config_.tick_seconds;
    auto tuning = enemy::combat_tuning(agent.enemy_type);
    agent.attack_timer = std::max(0.0F, agent.attack_timer - seconds);

    std::size_t target_index = players_.size();
    float nearest_squared = std::numeric_limits<float>::max();
    const float detection_squared = tuning.detection_radius
        * tuning.detection_radius;
    for (std::size_t player_index = 0; player_index < players_.size();
         ++player_index) {
        const auto& player = players_[player_index];
        if (!objective_player(player)) {
            continue;
        }
        const net::Vec3 delta = subtract(player.position, agent.position);
        if (std::abs(delta.y) > 15.0F) {
            continue;
        }
        const float distance = length_squared(delta);
        if (distance <= detection_squared && distance < nearest_squared) {
            nearest_squared = distance;
            target_index = player_index;
        }
    }

    if (target_index == players_.size()) {
        if (update_authored_motion(agent, seconds)) {
            if (agent.health == 0) {
                agent.active = false;
                return true;
            }
            agent.state = 0;
            agent.target_slot = 0xff;
            return false;
        }
        agent.state = 0;
        agent.target_slot = 0xff;
        agent.velocity = multiply(agent.velocity,
                                  std::max(0.0F, 1.0F - seconds * 5.0F));
        return false;
    }

    auto& target = players_[target_index];
    agent.target_slot = target.slot_index;
    const bool authored_motion = update_authored_motion(agent, seconds);
    if (agent.health == 0) {
        agent.active = false;
        return true;
    }
    const net::Vec3 target_position = add(
        target.position, tuning.airborne
            ? net::Vec3{0.0F, 0.45F, 0.0F} : net::Vec3{});
    net::Vec3 to_target = subtract(target_position, agent.position);
    if (!tuning.airborne) {
        to_target.y = 0.0F;
    }
    const float distance = std::sqrt(std::max(0.0F,
        distance_squared(agent.position, target.position)));
    const bool in_attack_range = distance <= tuning.attack_radius;
    agent.state = in_attack_range ? 2 : 1;

    if (!authored_motion && tuning.move_speed > 0.0F && !in_attack_range) {
        const net::Vec3 direction = normalized_or(
            to_target, {0.0F, 0.0F, 1.0F});
        agent.velocity = multiply(direction, tuning.move_speed);
        const net::Vec3 next = add(
            agent.position, multiply(agent.velocity, seconds));
        if (tuning.airborne) {
            agent.position = next;
        } else {
            const auto hit = collision::sweep_sphere(
                room_.collision(), to_collision(agent.position),
                to_collision(next), tuning.body_radius, 0x2000);
            if (hit.has_value()) {
                agent.position = {
                    hit->center.x + hit->normal.x * 0.001F,
                    hit->center.y + hit->normal.y * 0.001F,
                    hit->center.z + hit->normal.z * 0.001F};
                agent.velocity = {};
            } else {
                agent.position = next;
            }
        }
    } else if (!authored_motion) {
        agent.velocity = {};
    }

    if (in_attack_range && tuning.contact_damage > 0
        && agent.attack_timer <= 0.0F) {
        const std::uint16_t old_health = target.health;
        target.health = old_health > tuning.contact_damage
            ? static_cast<std::uint16_t>(
                old_health - tuning.contact_damage) : 0;
        target.damage_sequence = static_cast<std::uint8_t>(
            target.damage_sequence + 1);
        target.attacker_slot = 0xff;
        target.damage_beam = agent.enemy_type;
        target.hit_direction = normalized_or(
            subtract(target.position, agent.position), {});
        if (target.health != old_health) {
            SoundEvent damage_event;
            damage_event.cue = SoundCue::PlayerDamage;
            damage_event.slot = target.slot_index;
            damage_event.position = target.position;
            emit_sound(damage_event);
        }
        if (target.health == 0 && old_health != 0) {
            SoundEvent death_event;
            death_event.cue = SoundCue::PlayerDeath;
            death_event.slot = target.slot_index;
            death_event.position = target.position;
            emit_sound(death_event);
            increment_saturating(target.deaths);
            target.flags &= static_cast<std::uint8_t>(
                ~(net::PlayerState::FlagSpawned
                  | net::PlayerState::FlagAltForm
                  | net::PlayerState::FlagZoomed));
            target.speed = {};
            const auto runtime = std::find_if(
                inputs_.begin(), inputs_.end(), [&target](
                    const RuntimeInput& value) {
                    return value.slot == target.slot_index;
                });
            if (runtime != inputs_.end()) {
                if (config_.survival_mode
                    && target.deaths > config_.survival_lives) {
                    runtime->eliminated = true;
                    runtime->respawn_ticks = 0;
                } else {
                    runtime->eliminated = false;
                    runtime->respawn_ticks = config_.respawn_ticks;
                }
                runtime->fire_cooldown = 0;
            }
        }
        agent.attack_timer = std::max(0.01F, tuning.attack_cooldown);
    }
    return false;
}

} // namespace fruityprime::gameplay
