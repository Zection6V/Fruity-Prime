#include "enemy_decode_common.hpp"
// Native port of src/MphRead/Entities/Enemies/38_CrashPillar.cs.
// This member definition lives in its enemy module; Session only dispatches it.
#include "38_CrashPillar.hpp"
#include "enemy_common.hpp"

namespace fruityprime::gameplay {

void Session::update_crash_pillar(EnemyState& agent) {
    const auto& profile = agent.crash_pillar;
    if (!profile.supported) {
        return;
    }
    const float seconds = config_.tick_seconds;
    agent.attack_timer = std::max(0.0F, agent.attack_timer - seconds);
    if (agent.crash_pillar_vulnerable_timer > 0) {
        --agent.crash_pillar_vulnerable_timer;
        if (agent.crash_pillar_vulnerable_timer == 0) {
            agent.invulnerable = false;
        }
    }
    if (agent.crash_pillar_cooldown > 0) {
        --agent.crash_pillar_cooldown;
    }

    std::size_t target_index = players_.size();
    float nearest_squared = std::numeric_limits<float>::max();
    for (std::size_t index = 0; index < players_.size(); ++index) {
        const auto& player = players_[index];
        if (!objective_player(player)
            || !profile.activation_volume.contains(to_volume_point(
                player.position))) {
            continue;
        }
        const float distance = distance_squared(player.position,
                                                agent.position);
        if (distance <= 35.0F * 35.0F && distance < nearest_squared) {
            nearest_squared = distance;
            target_index = index;
        }
    }

    if (agent.crash_pillar_airborne) {
        // Enemy38's jump uses the world gravity even when the player preview
        // disables player gravity for a deterministic room probe.
        agent.velocity.y -= 18.0F * seconds;
        const net::Vec3 next = add(agent.position,
                                   multiply(agent.velocity, seconds));
        const auto hit = collision::sweep_sphere(
            room_.collision(), to_collision(agent.position),
            to_collision(next), agent.body_radius, 0x2000);
        if (hit.has_value()) {
            agent.position = {hit->center.x + hit->normal.x * 0.001F,
                              hit->center.y + hit->normal.y * 0.001F,
                              hit->center.z + hit->normal.z * 0.001F};
            agent.velocity = {};
            agent.crash_pillar_airborne = false;
            agent.invulnerable = true;
            agent.crash_pillar_cooldown = 30u;
            agent.state = 10;
            agent.attack_timer = 0.0F;
        } else {
            agent.position = next;
            agent.state = 7;
            agent.invulnerable = false;
        }
        if (target_index != players_.size()
            && distance_squared(players_[target_index].position,
                                agent.position) <= 1.25F * 1.25F
            && agent.attack_timer <= 0.0F) {
            apply_enemy_contact_damage(agent, players_[target_index], 40);
            agent.attack_timer = 0.5F;
        }
        return;
    }

    if (target_index == players_.size()
        || !profile.leash_volume.contains(to_volume_point(
            players_[target_index].position))) {
        agent.state = 0;
        agent.target_slot = 0xff;
        agent.velocity = {};
        agent.invulnerable = true;
        return;
    }

    auto& target = players_[target_index];
    agent.target_slot = target.slot_index;
    const net::Vec3 target_position = add(target.position,
                                          {0.0F, 0.5F, 0.0F});
    const net::Vec3 delta = subtract(target_position, agent.position);
    const float distance = std::sqrt(std::max(0.0F,
        length_squared(delta)));
    const net::Vec3 horizontal = normalized_or(
        {delta.x, 0.0F, delta.z}, agent.facing);
    agent.facing = horizontal;

    if (distance > 1.25F && agent.crash_pillar_cooldown == 0) {
        agent.velocity = add(multiply(horizontal, 2.4F),
                             {0.0F, 4.6F, 0.0F});
        agent.crash_pillar_airborne = true;
        agent.invulnerable = false;
        agent.state = 7;
        return;
    }

    agent.velocity = {};
    agent.state = distance <= 1.25F ? 10 : 3;
    if (distance <= 1.25F && agent.attack_timer <= 0.0F) {
        apply_enemy_contact_damage(agent, target, 10);
        agent.attack_timer = 0.5F;
    }
}
} // namespace fruityprime::gameplay

namespace fruityprime::enemy {

CrashPillarProfile decode_crash_pillar_profile(
    std::uint8_t id, std::span<const std::uint8_t> fields,
    net::Vec3 origin) noexcept {
    CrashPillarProfile result;
    if (id != static_cast<std::uint8_t>(formats::EnemyType::CrashPillar)
        || fields.size() < 3 * 64) {
        return result;
    }
    result.hurt_volume = detail::read_volume(fields, 0, origin);
    result.leash_volume = detail::read_volume(fields, 64, origin);
    result.activation_volume = detail::read_volume(fields, 128, origin);
    result.supported = result.hurt_volume.kind != scene::VolumeKind::Invalid
        && result.leash_volume.kind != scene::VolumeKind::Invalid
        && result.activation_volume.kind != scene::VolumeKind::Invalid;
    return result;
}

} // namespace fruityprime::enemy

