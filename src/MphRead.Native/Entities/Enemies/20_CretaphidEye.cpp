// Native port of src/MphRead/Entities/Enemies/20_CretaphidEye.cs.
// This member definition lives in its enemy module; Session only dispatches it.
#include "20_CretaphidEye.hpp"
#include "enemy_common.hpp"

namespace fruityprime::gameplay {

void Session::update_cretaphid_part(EnemyState& agent) {
    constexpr std::uint8_t EyePart = 1;
    constexpr std::uint8_t CrystalPart = 2;
    constexpr std::uint8_t Dead = 2;
    const auto parent = std::find_if(
        enemies_.begin(), enemies_.end(),
        [parent_id = agent.parent_enemy_id](const EnemyState& value) {
            return parent_id != 0 && value.id == parent_id;
        });
    if (parent == enemies_.end() || !parent->active
        || !parent->cretaphid.supported) {
        agent.active = false;
        return;
    }

    const auto& profile = parent->cretaphid;
    const std::size_t phase_index = std::min<std::size_t>(
        parent->cretaphid_phase, profile.phases.size() - 1);
    const auto& phase = profile.phases[phase_index];
    agent.cretaphid_phase = static_cast<std::uint8_t>(phase_index);
    agent.target_slot = parent->target_slot;
    agent.velocity = {};

    if (parent->cretaphid_state == 4) {
        agent.invulnerable = true;
        agent.cretaphid_part_state = Dead;
        agent.state = 7;
        agent.visible = agent.cretaphid_part_kind == CrystalPart;
        return;
    }

    const float seconds = config_.tick_seconds;
    const auto frame_step = static_cast<std::uint32_t>(std::max(
        1.0F, std::round(std::max(0.0F, seconds * 60.0F))));
    const auto decrement = [frame_step](std::uint32_t& value) noexcept {
        value = value > frame_step ? value - frame_step : 0;
    };

    if (agent.cretaphid_part_kind == EyePart) {
        const std::size_t eye_index = std::min<std::size_t>(
            agent.cretaphid_part_index, 11);
        const net::Vec3 axis = normalized_or(parent->up,
                                              {0.0F, 1.0F, 0.0F});
        const float angle = static_cast<float>(eye_index % 4u) * 90.0F
            + 45.0F;
        const net::Vec3 radial = rotate_about_axis(
            multiply(normalized_or(parent->facing, {0.0F, 0.0F, 1.0F}),
                     2.9F),
            axis, angle);
        const float segment_height = static_cast<float>(eye_index / 4u - 1u)
            * 0.85F;
        agent.facing = parent->facing;
        agent.up = axis;
        agent.position = add(
            add(parent->position, radial), multiply(axis, segment_height));

        if (agent.cretaphid_part_state == Dead || agent.health == 0) {
            agent.cretaphid_part_state = Dead;
            agent.state = 7;
            agent.visible = false;
            agent.invulnerable = true;
            return;
        }
        const auto restore_eye_state = [&agent, &phase, eye_index]() {
            agent.cretaphid_part_state = 0;
            agent.state = phase.eye_state[eye_index];
            agent.cretaphid_beam_type = phase.eye_beam_type[eye_index];
            agent.cretaphid_part_shots_remaining = 0;
            agent.cretaphid_part_shot_timer = 0;
            agent.invulnerable = !(agent.state == 1 || agent.state == 2
                                   || agent.state == 4);
            agent.visible = true;
        };
        if (agent.cretaphid_part_state == 1) {
            agent.invulnerable = true;
            agent.state = 3;
            agent.visible = true;
            decrement(agent.cretaphid_part_timer);
            if (agent.cretaphid_part_timer == 0) {
                restore_eye_state();
            }
            return;
        }

        agent.state = phase.eye_state[eye_index];
        agent.cretaphid_beam_type = phase.eye_beam_type[eye_index];
        agent.health_max = profile.eye_health;
        if (agent.health > agent.health_max) {
            agent.health = agent.health_max;
        }
        const bool vulnerable = (agent.state == 1 || agent.state == 2
                                 || agent.state == 4)
            && parent->cretaphid_state != 3;
        agent.invulnerable = !vulnerable;
        agent.visible = true;
        agent.attack_timer = std::max(0.0F, agent.attack_timer - seconds);

        std::size_t target_index = players_.size();
        float nearest_squared = std::numeric_limits<float>::max();
        for (std::size_t index = 0; index < players_.size(); ++index) {
            const auto& player = players_[index];
            if (!objective_player(player)) {
                continue;
            }
            const float distance = distance_squared(player.position,
                                                    agent.position);
            if (distance < nearest_squared && distance <= 45.0F * 45.0F) {
                nearest_squared = distance;
                target_index = index;
            }
        }
        if (!vulnerable || target_index == players_.size()) {
            return;
        }

        if (agent.cretaphid_beam_type == 2) {
            if (agent.attack_timer <= 0.0F
                && distance_squared(players_[target_index].position,
                                    agent.position)
                    <= (profile.collision_radius + config_.body_radius)
                        * (profile.collision_radius + config_.body_radius)) {
                apply_enemy_contact_damage(
                    agent, players_[target_index],
                    phase.eye_contact_damage);
                agent.attack_timer = 0.25F;
            }
            return;
        }

        if (agent.cretaphid_part_shots_remaining == 0) {
            if (agent.cretaphid_part_timer > 0) {
                decrement(agent.cretaphid_part_timer);
                return;
            }
            const auto minimum = phase.eye_beam_spawn_min[eye_index];
            const auto maximum = phase.eye_beam_spawn_max[eye_index];
            const auto range = maximum >= minimum
                ? static_cast<std::uint32_t>(maximum - minimum + 1u) : 1u;
            agent.cretaphid_part_shots_remaining = static_cast<std::uint16_t>(
                minimum + rng_.random2(range));
        }
        if (agent.cretaphid_part_shot_timer > 0) {
            decrement(agent.cretaphid_part_shot_timer);
            return;
        }
        const auto target = add(players_[target_index].position,
                                {0.0F, 0.5F, 0.0F});
        spawn_enemy_projectile(agent, agent.position,
                               normalized_or(subtract(target, agent.position),
                                             agent.facing));
        --agent.cretaphid_part_shots_remaining;
        agent.cretaphid_part_shot_timer = static_cast<std::uint32_t>(
            phase.eye_beam_cooldown[eye_index]) * 2u;
        if (agent.cretaphid_part_shots_remaining == 0) {
            agent.cretaphid_part_timer = static_cast<std::uint32_t>(
                phase.eye_state_timer2[eye_index]) * 2u;
        }
        return;
    }

    if (agent.cretaphid_part_kind != CrystalPart) {
        return;
    }
    agent.position = parent->position;
    agent.facing = parent->facing;
    agent.up = parent->up;
    agent.state = parent->cretaphid_state;
    agent.visible = true;
    agent.health_max = profile.crystal_health;
    if (!parent->cretaphid_crystal_open || agent.health == 0
        || parent->cretaphid_state == 3) {
        agent.invulnerable = true;
        return;
    }

    agent.invulnerable = false;
    if (agent.cretaphid_part_shot_timer > 0) {
        decrement(agent.cretaphid_part_shot_timer);
        return;
    }
    std::size_t target_index = players_.size();
    float nearest_squared = std::numeric_limits<float>::max();
    for (std::size_t index = 0; index < players_.size(); ++index) {
        const auto& player = players_[index];
        if (!objective_player(player)) {
            continue;
        }
        const float distance = distance_squared(player.position,
                                                agent.position);
        if (distance < nearest_squared && distance <= 45.0F * 45.0F) {
            nearest_squared = distance;
            target_index = index;
        }
    }
    if (target_index == players_.size()) {
        return;
    }
    const auto target = add(players_[target_index].position,
                            {0.0F, 0.5F, 0.0F});
    spawn_enemy_projectile(agent, agent.position,
                           normalized_or(subtract(target, agent.position),
                                         agent.facing));
    agent.cretaphid_part_shot_timer = static_cast<std::uint32_t>(
        phase.crystal_shot_time) * 2u;
}

void Session::update_cretaphid_eye(EnemyState& agent) {
    update_cretaphid_part(agent);
}

} // namespace fruityprime::gameplay
