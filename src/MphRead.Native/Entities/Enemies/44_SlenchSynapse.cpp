#include "45_SlenchTurret.hpp"
#include "enemy_decode_common.hpp"
// Native port of src/MphRead/Entities/Enemies/44_SlenchSynapse.cs.
// This member definition lives in its enemy module; Session only dispatches it.
#include "44_SlenchSynapse.hpp"
#include "enemy_common.hpp"

namespace fruityprime::gameplay {

void Session::update_slench_part(EnemyState& agent) {
    constexpr std::uint8_t SynapseInitial = 0;
    constexpr std::uint8_t SynapseAppear = 1;
    constexpr std::uint8_t SynapseIdle = 2;
    constexpr std::uint8_t SynapseDamaged = 3;
    constexpr std::uint8_t SynapseDying = 4;
    constexpr std::uint8_t SynapseDead = 5;

    auto parent = std::find_if(
        enemies_.begin(), enemies_.end(),
        [parent_id = agent.parent_enemy_id](const EnemyState& value) {
            return parent_id != 0 && value.id == parent_id;
        });
    if (agent.parent_enemy_id != 0 && parent == enemies_.end()) {
        // Child parts have no room spawner of their own.  If the owner has
        // gone away, remove the part on the next enemy update pass as well.
        agent.active = false;
        return;
    }

    if (agent.slench_shield.supported) {
        if (parent != enemies_.end()) {
            agent.facing = parent->facing;
            agent.up = parent->up;
            agent.position = add(
                parent->position,
                multiply(normalized_or(parent->facing,
                                       {0.0F, 0.0F, 1.0F}),
                         agent.slench_shield.follow_offset));
            agent.target_slot = parent->target_slot;
            agent.state = parent->state;
        }
        agent.velocity = {};
        agent.health = agent.health_max = agent.slench_shield.health;
        agent.body_radius = agent.slench_shield.hurt_radius;
        agent.invulnerable = true;
        agent.visible = true;
        return;
    }

    if (agent.slench_nest.supported) {
        if (parent != enemies_.end()) {
            agent.position = parent->position;
            agent.facing = parent->facing;
            agent.up = parent->up;
            agent.target_slot = parent->target_slot;
            agent.state = parent->state;
        }
        agent.velocity = {};
        agent.health = agent.health_max = agent.slench_nest.health;
        agent.body_radius = 0.0F;
        agent.invulnerable = true;
        agent.visible = true;
        return;
    }

    if (!agent.slench_synapse.supported) {
        return;
    }

    std::size_t phase_index = 0;
    if (parent != enemies_.end() && parent->slench.supported) {
        phase_index = std::min<std::size_t>(
            parent->slench_phase, agent.slench_synapse.phases.size() - 1);
        agent.facing = parent->facing;
        const float angle = agent.slench_synapse.index == 1
            ? 120.0F : agent.slench_synapse.index == 2 ? -120.0F : 0.0F;
        const net::Vec3 ring_up = normalized_or(
            rotate_about_axis(parent->up, parent->facing, angle),
            parent->up);
        agent.up = ring_up;
        agent.position = add(
            parent->position,
            multiply(ring_up, 2.9F));
        agent.target_slot = parent->target_slot;
    }

    const auto& phase = agent.slench_synapse.phases[phase_index];
    const float seconds = config_.tick_seconds;
    const auto frame_step = static_cast<std::uint32_t>(std::max(
        1.0F, std::round(std::max(0.0F, seconds * 60.0F))));
    const auto decrement = [frame_step](std::uint32_t& value) noexcept {
        value = value > frame_step ? value - frame_step : 0;
    };
    const auto reset_to_idle = [&agent, &phase]() noexcept {
        agent.slench_part_state = SynapseIdle;
        agent.slench_part_timer = 0;
        agent.slench_part_heal_timer = static_cast<std::uint32_t>(
            phase.heal_timer) * 2u;
        agent.health = std::min<std::uint16_t>(agent.health,
                                                agent.health_max);
        agent.invulnerable = false;
        agent.visible = true;
        agent.state = SynapseIdle;
    };
    const auto set_turret_active = [this, &agent](bool enabled) noexcept {
        if (agent.slench_part_index == 0xff) {
            return;
        }
        const auto turret_type = static_cast<std::uint8_t>(
            formats::EnemyType::SlenchTurret);
        for (auto& turret : enemies_) {
            if (!turret.active || turret.enemy_type != turret_type
                || turret.turret_index
                    != static_cast<std::int32_t>(agent.slench_part_index)) {
                continue;
            }
            enemy::module_45_slench_turret::HandleMessage(
                turret,
                static_cast<std::uint16_t>(
                    enabled ? formats::Message::ActivateTurret
                            : formats::Message::DeactivateTurret),
                0);
            turret.target_slot = 0xff;
            turret.turret_shot_timer = 0;
            if (enabled) {
                turret.turret_salvo_cooldown = 0;
            }
        }
    };

    switch (agent.slench_part_state) {
    case SynapseInitial:
        agent.state = SynapseInitial;
        agent.velocity = {};
        agent.visible = false;
        agent.invulnerable = true;
        return;
    case SynapseAppear:
        agent.state = SynapseAppear;
        agent.velocity = {};
        agent.visible = true;
        agent.invulnerable = true;
        if (agent.slench_part_timer > 0) {
            decrement(agent.slench_part_timer);
        }
        if (agent.slench_part_timer == 0) {
            agent.health = agent.health_max = phase.health;
            set_turret_active(true);
            reset_to_idle();
        }
        return;
    case SynapseIdle:
        agent.state = SynapseIdle;
        agent.velocity = {};
        agent.visible = true;
        agent.invulnerable = false;
        agent.health_max = phase.health;
        if (agent.health > agent.health_max) {
            agent.health = agent.health_max;
        }
        if (agent.health < agent.health_max) {
            if (agent.slench_part_heal_timer > 0) {
                decrement(agent.slench_part_heal_timer);
            }
            if (agent.slench_part_heal_timer == 0) {
                ++agent.health;
                agent.slench_part_heal_timer = static_cast<std::uint32_t>(
                    phase.heal_timer) * 2u;
            }
        }
        return;
    case SynapseDamaged:
        agent.state = SynapseDamaged;
        agent.velocity = {};
        agent.visible = true;
        agent.invulnerable = true;
        if (agent.slench_part_timer > 0) {
            decrement(agent.slench_part_timer);
        }
        if (agent.slench_part_timer == 0) {
            reset_to_idle();
        }
        return;
    case SynapseDying:
        agent.state = SynapseDying;
        agent.velocity = {};
        agent.visible = true;
        agent.invulnerable = true;
        if (agent.slench_part_timer > 0) {
            decrement(agent.slench_part_timer);
        }
        if (agent.slench_part_timer == 0) {
            agent.slench_part_state = SynapseDead;
            agent.state = SynapseDead;
            agent.visible = false;
            agent.invulnerable = true;
            set_turret_active(false);
            agent.slench_part_reappear_timer = static_cast<std::uint32_t>(
                phase.reappear_timer) * 2u;
        }
        return;
    case SynapseDead: {
        agent.state = SynapseDead;
        agent.velocity = {};
        agent.visible = false;
        agent.invulnerable = true;
        if (parent == enemies_.end() || !parent->slench.supported
            || parent->slench_state >= 5) {
            return;
        }

        // CanSynapsesRespawn() is true only when another synapse is still
        // alive.  When all three are dead the parent leaves its static state,
        // and the next phase resets them to Initial instead.
        const bool another_synapse_alive = std::any_of(
            enemies_.begin(), enemies_.end(),
            [parent_id = agent.parent_enemy_id, self_id = agent.id](
                const EnemyState& value) {
                return value.id != self_id
                    && value.parent_enemy_id == parent_id
                    && value.slench_synapse.supported && value.active
                    && value.slench_part_state != SynapseDying
                    && value.slench_part_state != SynapseDead;
            });
        if (!another_synapse_alive) {
            return;
        }
        if (agent.slench_part_reappear_timer > 0) {
            decrement(agent.slench_part_reappear_timer);
        }
        if (agent.slench_part_reappear_timer == 0) {
            agent.health = agent.health_max = phase.health;
            agent.slench_part_state = SynapseAppear;
            agent.slench_part_timer = 30u * 2u;
            agent.visible = true;
        }
        return;
    }
    default:
        agent.slench_part_state = SynapseInitial;
        agent.state = SynapseInitial;
        agent.visible = false;
        agent.invulnerable = true;
        return;
    }
}

void Session::update_slench_synapse(EnemyState& agent) {
    update_slench_part(agent);
}
} // namespace fruityprime::gameplay

namespace fruityprime::enemy {

SlenchSynapseProfile slench_synapse_profile(
    std::uint8_t subtype, std::uint8_t index) noexcept {
    SlenchSynapseProfile result;
    if (subtype >= 4 || index >= 3) {
        return result;
    }

    // Metadata.Enemy44Values is four room variants by three boss phases.
    // Keep the authored timers/radii here in the same frame units as the
    // Enemy41 profile; the gameplay layer converts them to fixed-step ticks.
    static constexpr std::array<std::array<SlenchSynapsePhase, 3>, 4> values{{
        {{
            {36, 120, 360, 6144.0F / 4096.0F},
            {36, 120, 300, 6144.0F / 4096.0F},
            {36, 120, 240, 6144.0F / 4096.0F}
        }},
        {{
            {72, 120, 600, 6144.0F / 4096.0F},
            {72, 120, 480, 6144.0F / 4096.0F},
            {96, 120, 360, 6144.0F / 4096.0F}
        }},
        {{
            {48, 120, 420, 6144.0F / 4096.0F},
            {48, 120, 360, 6144.0F / 4096.0F},
            {48, 120, 300, 6144.0F / 4096.0F}
        }},
        {{
            {90, 120, 600, 6144.0F / 4096.0F},
            {90, 120, 540, 6144.0F / 4096.0F},
            {90, 120, 480, 6144.0F / 4096.0F}
        }}
    }};
    result.supported = true;
    result.subtype = subtype;
    result.index = index;
    result.phases = values[subtype];
    return result;
}

} // namespace fruityprime::enemy
