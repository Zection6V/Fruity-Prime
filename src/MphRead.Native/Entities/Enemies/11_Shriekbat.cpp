#include "enemy_decode_common.hpp"
// Native port of src/MphRead/Entities/Enemies/11_Shriekbat.cs.
// This member definition lives in its enemy module; Session only dispatches it.
#include "11_Shriekbat.hpp"
#include "enemy_common.hpp"

namespace fruityprime::gameplay {

void Session::update_shriekbat(EnemyState& agent) {
    const float seconds = config_.tick_seconds;
    const float frames = std::max(0.0F, seconds * 60.0F);
    const std::uint32_t frame_step = static_cast<std::uint32_t>(std::max(
        1.0F, std::round(frames)));
    agent.attack_timer = std::max(0.0F, agent.attack_timer - seconds);

    // Enemy11Entity is driven by a five-state metadata subroutine.  Keep the
    // same state numbers here: wait for the range volume, wait for the active
    // volume, descend to the authored path point, pause, then make the dive.
    // The native session can have more than one player, so the first four
    // states choose the nearest live player that satisfies the current volume
    // gate; once the dive has started, target_slot keeps that choice stable.
    std::size_t target_index = players_.size();
    if (agent.state >= 2 && agent.target_slot != 0xff) {
        const auto found = std::find_if(
            players_.begin(), players_.end(), [&agent](
                const net::PlayerState& player) {
                return player.slot_index == agent.target_slot
                    && objective_player(player);
            });
        if (found != players_.end()) {
            target_index = static_cast<std::size_t>(
                std::distance(players_.begin(), found));
        }
    }
    if (target_index == players_.size()) {
        float nearest_squared = std::numeric_limits<float>::max();
        for (std::size_t index = 0; index < players_.size(); ++index) {
            const auto& player = players_[index];
            if (!objective_player(player)) {
                continue;
            }
            const bool gated = agent.state == 0
                ? agent.shriekbat.range_volume.contains(
                    to_volume_point(player.position))
                : agent.state == 1
                    ? agent.shriekbat.active_volume.contains(
                        to_volume_point(player.position))
                    : true;
            if (!gated) {
                continue;
            }
            const float distance = distance_squared(
                player.position, agent.position);
            if (distance < nearest_squared) {
                nearest_squared = distance;
                target_index = index;
            }
        }
    }

    if (target_index == players_.size()) {
        if (agent.state < 2) {
            agent.state = 0;
            agent.target_slot = 0xff;
            agent.velocity = {};
        }
        return;
    }

    auto& target = players_[target_index];
    agent.target_slot = target.slot_index;
    agent.facing = normalized_or(
        subtract(target.position, agent.position), agent.facing);

    bool blocked = false;
    const auto move_for_frames = [this, &agent, frames, &blocked](
                                     net::Vec3 destination, float per_frame,
                                     bool check_collision) {
        const net::Vec3 delta = subtract(destination, agent.position);
        const float distance = std::sqrt(std::max(0.0F, length_squared(delta)));
        if (distance <= 0.0001F) {
            agent.position = destination;
            agent.velocity = {};
            return true;
        }
        const net::Vec3 direction = multiply(delta, 1.0F / distance);
        agent.facing = direction;
        agent.velocity = multiply(direction, per_frame);
        const net::Vec3 next = add(agent.position,
                                   multiply(agent.velocity, frames));
        if (check_collision) {
            const auto hit = collision::sweep_sphere(
                room_.collision(), to_collision(agent.position),
                to_collision(next), agent.body_radius, 0x2000);
            if (hit.has_value()) {
                agent.position = {hit->center.x + hit->normal.x * 0.001F,
                                  hit->center.y + hit->normal.y * 0.001F,
                                  hit->center.z + hit->normal.z * 0.001F};
                agent.velocity = {};
                blocked = true;
                return true;
            }
        }
        agent.position = next;
        return distance <= per_frame * frames;
    };

    switch (agent.state) {
    case 0:
        // Behavior04: range-volume gate.
        agent.velocity = {};
        if (agent.shriekbat.range_volume.contains(to_volume_point(
                target.position))) {
            agent.state = 1;
        }
        break;
    case 1: {
        // Behavior03: active-volume gate followed by the descent to S02's
        // path vector.  The managed code uses a half-step at 60 Hz.
        agent.velocity = {};
        if (!agent.shriekbat.active_volume.contains(to_volume_point(
                target.position))) {
            break;
        }
        agent.shriekbat_target = add(agent.behavior_origin,
                                     agent.shriekbat.path_vector);
        const float distance = std::sqrt(std::max(0.0F, distance_squared(
            agent.position, agent.shriekbat_target)));
        agent.shriekbat_timer = static_cast<std::uint32_t>(
            std::max(1.0F, std::ceil(distance / 0.3F) + 1.0F)) * 2u;
        agent.state = 2;
        spawn_effect(29, agent.position, {1.0F, 0.0F, 0.0F}, agent.id,
                     0.75F);
        break;
    }
    case 2:
        // Behavior02: move toward the authored pre-attack point, then pause.
        if (agent.shriekbat_timer > 0) {
            const bool arrived = move_for_frames(
                agent.shriekbat_target, 0.15F, false);
            agent.shriekbat_timer = agent.shriekbat_timer > frame_step
                ? agent.shriekbat_timer - frame_step : 0;
            if (!arrived && agent.shriekbat_timer != 0) {
                break;
            }
        }
        agent.position = agent.shriekbat_target;
        agent.velocity = {};
        agent.shriekbat_timer = 40u;
        agent.state = 3;
        break;
    case 3:
        // Behavior02's pause completes here; Behavior01 starts the lunge at
        // the player's rear-facing position.
        if (agent.shriekbat_timer > frame_step) {
            agent.shriekbat_timer -= frame_step;
            agent.velocity = {};
            break;
        }
        agent.shriekbat_timer = 0;
        agent.shriekbat_target = {
            target.position.x - target.facing.x,
            target.position.y + 0.5F,
            target.position.z - target.facing.z};
        {
            const float distance = std::sqrt(std::max(0.0F,
                distance_squared(agent.position, agent.shriekbat_target)));
            agent.shriekbat_timer = static_cast<std::uint32_t>(
                std::max(1.0F, std::ceil(distance / 0.6F) + 1.0F)) * 2u;
        }
        agent.state = 4;
        break;
    case 4:
        // Behavior01/00: lunge.  Enemy11Entity remains in state 4 after the
        // move; a blocking collision is lethal to the Shriekbat.
        if (agent.shriekbat_timer > 0) {
            const bool arrived = move_for_frames(
                agent.shriekbat_target, 0.3F, true);
            agent.shriekbat_timer = agent.shriekbat_timer > frame_step
                ? agent.shriekbat_timer - frame_step : 0;
            if (arrived || agent.shriekbat_timer == 0) {
                agent.position = agent.shriekbat_target;
                agent.velocity = {};
            }
        } else {
            agent.velocity = {};
        }
        break;
    default:
        agent.state = 0;
        agent.shriekbat_timer = 0;
        agent.velocity = {};
        break;
    }

    if (blocked && agent.state == 4) {
        const auto id = agent.id;
        static_cast<void>(damage_enemy(
            id, std::max<std::uint32_t>(agent.health, 1u)));
        return;
    }

    // ContactDamagePlayer(20, knockback: false), with a short native cooldown
    // to represent the managed hit-player gating across fixed frames.
    constexpr float contact_radius = 1.6F;
    if (agent.attack_timer <= 0.0F
        && distance_squared(target.position, agent.position)
            <= contact_radius * contact_radius) {
        apply_enemy_contact_damage(agent, target, 20);
        agent.attack_timer = 0.5F;
    }
}
} // namespace fruityprime::gameplay

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

