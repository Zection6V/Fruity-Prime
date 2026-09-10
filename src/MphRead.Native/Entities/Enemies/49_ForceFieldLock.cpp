// Native port of src/MphRead/Entities/Enemies/49_ForceFieldLock.cs.
// This member definition lives in its enemy module; Session only dispatches it.
#include <array>
#include "Entities/gameplay.hpp"
#include "49_ForceFieldLock.hpp"
#include "enemy_common.hpp"

namespace fruityprime::gameplay {

void Session::update_force_field_lock(EnemyState& agent) {
    const auto field = std::find_if(
        force_fields_.begin(), force_fields_.end(),
        [&agent](const ForceFieldRuntime& value) {
            return value.entity_id == agent.force_field_entity_id;
        });
    if (field == force_fields_.end() || !field->active || field->type > 8) {
        agent.active = false;
        agent.visible = false;
        return;
    }

    const float seconds = config_.tick_seconds;
    const auto frame_step = static_cast<std::uint32_t>(std::max(
        1.0F, std::round(std::max(0.0F, seconds * 60.0F))));

    // Enemy49 turns its lock toward the side of the field on which the main
    // hunter is standing.  A headless/native session has no camera object,
    // so the nearest active player is the deterministic equivalent.
    std::size_t target_index = players_.size();
    float nearest_squared = std::numeric_limits<float>::max();
    for (std::size_t index = 0; index < players_.size(); ++index) {
        const auto& player = players_[index];
        if (!objective_player(player)) {
            continue;
        }
        const float distance = distance_squared(player.position,
                                                field->position);
        if (distance < nearest_squared) {
            nearest_squared = distance;
            target_index = index;
        }
    }
    const net::Vec3 original_facing = field->facing;
    if (target_index != players_.size()
        && dot(subtract(players_[target_index].position, field->position),
               original_facing) < 0.0F) {
        agent.force_field_facing = multiply(original_facing, -1.0F);
    } else {
        agent.force_field_facing = original_facing;
    }
    agent.facing = agent.force_field_facing;
    agent.up = field->up;

    const net::Vec3 between = subtract(agent.position, field->position);
    const float width = std::max(0.1F, field->width - 0.3F);
    const float height = std::max(0.1F, field->height - 0.3F);
    const float right_percent = dot(between, field->right) / width;
    const float up_percent = dot(between, field->up) / height;
    const float percent = right_percent * right_percent
        + up_percent * up_percent;
    if (percent >= 1.0F) {
        const float facing_distance = dot(between, original_facing);
        const net::Vec3 tangent = normalized_or(
            subtract(between, multiply(original_facing, facing_distance)),
            field->right);
        const float reflected = dot(agent.force_field_velocity, tangent) * 2.0F;
        agent.force_field_velocity = subtract(agent.force_field_velocity,
                                              multiply(tangent, reflected));
        const float inverse = 1.0F / std::sqrt(percent);
        agent.position = add(
            field->position,
            add(multiply(field->right, right_percent * inverse * width),
                multiply(field->up, up_percent * inverse * height)));
    }

    if (length_squared(agent.force_field_velocity) <= 0.0004F) {
        if (agent.force_field_shot_timer == 0) {
            const float random_right = static_cast<float>(rng_.random2(0x666u))
                / 4096.0F - 0.2F;
            const float random_up = static_cast<float>(rng_.random2(0x666u))
                / 4096.0F - 0.2F;
            agent.force_field_velocity = add(
                multiply(field->right, random_right),
                multiply(field->up, random_up));
            agent.force_field_bounce = true;
        }
    } else {
        agent.force_field_velocity = multiply(
            agent.force_field_velocity, 3973.0F / 4096.0F);
    }
    agent.velocity = multiply(agent.force_field_velocity, 0.5F);
    agent.position = add(agent.position,
                         multiply(agent.force_field_velocity, seconds * 0.5F));

    if (agent.force_field_shot_timer > 0) {
        if (agent.target_slot != 0xff) {
            const auto target = std::find_if(
                players_.begin(), players_.end(),
                [slot = agent.target_slot](const net::PlayerState& player) {
                    return player.slot_index == slot && objective_player(player);
                });
            if (target != players_.end()) {
                const net::Vec3 direction = normalized_or(
                    subtract(add(target->position, {0.0F, 0.5F, 0.0F}),
                             agent.position), agent.facing);
                spawn_enemy_projectile(
                    agent, add(agent.position, multiply(direction, 0.1F)),
                    direction);
            }
        }
        agent.force_field_shot_timer = agent.force_field_shot_timer > frame_step
            ? agent.force_field_shot_timer - frame_step : 0;
    }
    agent.state = agent.force_field_shot_timer == 0 ? 0 : 2;
}

} // namespace fruityprime::gameplay

namespace fruityprime::enemy::module_49_force_field_lock {

void ClearEffectiveness(gameplay::EnemyState& agent) noexcept {
    agent.lock_effectiveness.fill(0);
}

void SetEffectiveness(gameplay::EnemyState& agent, const std::uint8_t beam,
                      const std::uint8_t effectiveness) noexcept {
    if (beam < agent.lock_effectiveness.size()) {
        agent.lock_effectiveness[beam] = effectiveness;
    }
}

void EnemyInitialize(gameplay::EnemyState& agent,
                     const std::uint8_t field_type) noexcept {
    // One energy: a lock is a switch, and the right beam throws it.
    agent.health = agent.health_max = 1;
    agent.body_radius = 0.5F;
    agent.lock_shot_frames = 0;
    ClearEffectiveness(agent);
    // Field types nought to seven each name one beam, in beam order.
    // Type eight names none and takes a bomb instead.
    static constexpr std::array<std::uint8_t, 8> BeamForType{
        {0, 2, 1, 3, 4, 5, 6, 7}};
    if (field_type < BeamForType.size()) {
        SetEffectiveness(agent, BeamForType[field_type], 2);
    }
}

LockFrame EnemyProcess(gameplay::EnemyState& agent,
                       const net::Vec3 field_position,
                       const net::Vec3 field_facing,
                       const net::Vec3 camera_position) noexcept {
    LockFrame frame;
    // Which side of the field the player is on.  A lock always turns to
    // face them, so there is no angle from which it cannot be shot.
    const float side = (camera_position.x - field_position.x) * field_facing.x
        + (camera_position.y - field_position.y) * field_facing.y
        + (camera_position.z - field_position.z) * field_facing.z;
    net::Vec3 facing = field_facing;
    if (side < 0.0F) {
        facing = {-field_facing.x, -field_facing.y, -field_facing.z};
        frame.flipped = true;
    }
    // A tenth of a unit off the field itself, so the lock does not sit
    // inside it.
    constexpr float Offset = 409.0F / 4096.0F;
    agent.facing = facing;
    agent.position = {field_position.x + facing.x * Offset,
                      field_position.y + facing.y * Offset,
                      field_position.z + facing.z * Offset};
    if (agent.lock_shot_frames > 0) {
        --agent.lock_shot_frames;
        frame.shooting = true;
    }
    return frame;
}

bool EnemyTakeDamage(gameplay::EnemyState& agent) noexcept {
    return agent.health > 0;
}

LockShot LockHit(gameplay::EnemyState& agent, const std::uint8_t beam,
                 const std::uint8_t field_type) noexcept {
    LockShot shot;
    if (agent.lock_shot_frames != 0) {
        return shot;
    }
    // Only a beam the lock is immune to comes back: the one that opens it
    // opens it instead.
    if (beam < agent.lock_effectiveness.size()
        && agent.lock_effectiveness[beam] != 0) {
        return shot;
    }
    shot.fired = true;
    // A Shock Coil lock returns fire for a full second; every other kind
    // sends back one shot.
    shot.frames = field_type == 7 ? static_cast<std::uint8_t>(30u * 2u) : 1u;
    agent.lock_shot_frames = shot.frames;
    return shot;
}

} // namespace fruityprime::enemy::module_49_force_field_lock

