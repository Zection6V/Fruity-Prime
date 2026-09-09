// Native counterpart of src/MphRead/Entities/Enemies/12_Geemer.cs.
// This file is deliberately present even when the managed class currently
// shares a native controller; the descriptor and entry point prevent a
// many-classes-in-one gameplay.cpp regression.
#include "12_Geemer.hpp"
#include "enemy_common.hpp"

namespace fruityprime::gameplay {

void Session::update_geemer(EnemyState& agent) {
    const float seconds = config_.tick_seconds;
    agent.attack_timer = std::max(0.0F, agent.attack_timer - seconds);

    // Enemy12Entity performs contact damage before its animation/state branch.
    // The native Session has no animation object yet, so the explicit timer is
    // the state boundary and the collision radius remains the authored one.
    constexpr float contact_radius = 1.5F;
    for (auto& player : players_) {
        if (distance_squared(player.position, agent.position)
                > contact_radius * contact_radius
            || agent.attack_timer > 0.0F) {
            continue;
        }
        apply_enemy_contact_damage(agent, player, 15);
        agent.attack_timer = 0.5F;
    }

    bool player_near = false;
    const float activation_radius = config_.body_radius + 1.5F;
    for (const auto& player : players_) {
        if (objective_player(player)
            && distance_squared(player.position, agent.position)
                < activation_radius * activation_radius) {
            player_near = true;
            break;
        }
    }
    if (!agent.geemer_extended && player_near
        && agent.geemer_transition_timer == 0) {
        // The C# class changes to Extend and stops movement immediately.
        agent.geemer_extended = true;
        agent.geemer_transition_timer = 120;
        agent.velocity = {};
        agent.state = 1;
        return;
    }
    if (agent.geemer_extended && !player_near) {
        // The C# class changes to Retract and waits for the animation to end.
        agent.geemer_extended = false;
        agent.geemer_transition_timer = 0;
        agent.velocity = {};
        agent.state = 0;
        return;
    }
    if (agent.geemer_transition_timer > 0) {
        --agent.geemer_transition_timer;
        agent.velocity = {};
        agent.state = agent.geemer_extended ? 1 : 0;
        return;
    }

    // The post-animation Geemer path is intentionally shared with Zoomer.
    // It does not apply a second contact hit; Enemy12Entity's call happens at
    // the beginning of EnemyProcess, before this shared block.
    update_surface_enemy(agent);
}

} // namespace fruityprime::gameplay

static_assert(fruityprime::enemy::module_12_geemer::kModule.managed_class.size() != 0);
