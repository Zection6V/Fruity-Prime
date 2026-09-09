#include "enemy_decode_common.hpp"
// Native port of src/MphRead/Entities/Enemies/01_Zoomer.cs.
// This member definition lives in its enemy module; Session only dispatches it.
#include "01_Zoomer.hpp"
#include "enemy_common.hpp"

namespace fruityprime::gameplay {

void Session::update_zoomer(EnemyState& agent) {
    update_surface_enemy(agent);

    // ContactDamagePlayer is not target-acquisition based: every live player
    // whose hurt volume overlaps the Zoomer can be damaged on this frame.
    constexpr float contact_radius = 1.25F;
    for (auto& player : players_) {
        if (distance_squared(player.position, agent.position)
                > contact_radius * contact_radius
            || agent.attack_timer > 0.0F) {
            continue;
        }
        apply_enemy_contact_damage(agent, player, 15);
        agent.attack_timer = 0.5F;
    }
}

void Session::update_surface_enemy(EnemyState& agent) {
    const float seconds = config_.tick_seconds;
    const float frames = std::max(0.0F, seconds * 60.0F);
    agent.attack_timer = std::max(0.0F, agent.attack_timer - seconds);

    // Enemy01Entity's motion is a small surface follower.  Its fixed-point
    // constants are converted to per-second values here so a Session with a
    // different fixed tick still follows the same authored path.
    constexpr float gravity_speed = -245.0F / 4096.0F / 2.0F * 60.0F;
    constexpr float surface_speed = 204.0F / 4096.0F / 2.0F * 60.0F;
    constexpr float surface_blend = 819.0F / 4096.0F / 2.0F;
    constexpr float surface_alignment = 3712.0F / 4096.0F;
    constexpr float reverse_dot = -4091.0F / 4096.0F;
    constexpr float turn_angle = 3.0F;

    const net::Vec3 current_up = normalized_or(
        agent.behavior_surface_normal, agent.up);
    if (!agent.zoomer.home_volume.contains(to_volume_point(agent.position))) {
        net::Vec3 radial = subtract(agent.position, agent.behavior_origin);
        radial.y = 0.0F;
        radial = normalized_or(radial,
                              {agent.facing.x, 0.0F, agent.facing.z});
        agent.behavior_intended_direction = normalized_or(
            cross(current_up, radial), agent.behavior_direction);
        if (dot(agent.behavior_intended_direction,
                agent.behavior_direction) < reverse_dot) {
            agent.behavior_direction = rotate_about_axis(
                agent.behavior_direction, current_up, 0.5F);
        }
        agent.behavior_direction = normalized_or(
            agent.behavior_direction, agent.behavior_intended_direction);
    }

    const net::Vec3 facing = normalized_or(
        cross(current_up, agent.behavior_direction), agent.facing);
    agent.facing = facing;
    agent.up = current_up;
    agent.velocity = multiply(current_up, gravity_speed);

    const net::Vec3 next = add(agent.position,
                               multiply(agent.velocity, seconds));
    const auto hit = collision::sweep_sphere(
        room_.collision(), to_collision(agent.position), to_collision(next),
        agent.body_radius, 0x2000);
    bool contacted_surface = false;
    if (hit.has_value()) {
        agent.position = {hit->center.x + hit->normal.x * 0.001F,
                          hit->center.y + hit->normal.y * 0.001F,
                          hit->center.z + hit->normal.z * 0.001F};
        agent.behavior_surface_normal = normalized_or(
            {hit->normal.x, hit->normal.y, hit->normal.z}, current_up);
        agent.up = agent.behavior_surface_normal;
        contacted_surface = true;
    } else {
        agent.position = next;
    }

    if (frames > 0.0F) {
        const float blend = std::clamp(surface_blend * frames, 0.0F, 1.0F);
        agent.behavior_direction = normalized_or(
            add(multiply(agent.behavior_direction, 1.0F - blend),
                multiply(agent.behavior_intended_direction, blend)),
            agent.behavior_direction);
        if (dot(agent.behavior_intended_direction,
                agent.behavior_direction)
            > std::cos(turn_angle * 3.14159265358979323846F / 180.0F)) {
            agent.behavior_direction = agent.behavior_intended_direction;
        }
    }

    if (contacted_surface
        && dot(agent.behavior_surface_normal, current_up)
            >= surface_alignment
        && !agent.behavior_intended_direction.x
        && !agent.behavior_intended_direction.z) {
        agent.velocity = add(agent.velocity,
                             multiply(agent.facing, surface_speed));
    } else if (contacted_surface
               && dot(agent.behavior_surface_normal, current_up)
                   >= surface_alignment) {
        agent.velocity = add(agent.velocity,
                             multiply(agent.facing, surface_speed));
    }
    agent.state = 0;
    agent.target_slot = 0xff;
}
} // namespace fruityprime::gameplay

namespace fruityprime::enemy {

ZoomerProfile decode_zoomer_profile(
    std::uint8_t id, std::span<const std::uint8_t> fields,
    net::Vec3 origin) noexcept {
    ZoomerProfile result;
    if ((id != static_cast<std::uint8_t>(formats::EnemyType::Zoomer)
         && id != static_cast<std::uint8_t>(formats::EnemyType::Geemer))
        || fields.size() < 2 * 64) {
        return result;
    }
    result.hurt_volume = detail::read_volume(fields, 0, origin);
    result.home_volume = detail::read_volume(fields, 64, origin);
    result.supported = result.home_volume.kind != scene::VolumeKind::Invalid;
    return result;
}

} // namespace fruityprime::enemy

