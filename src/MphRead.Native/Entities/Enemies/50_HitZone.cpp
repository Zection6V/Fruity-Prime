// Native port of src/MphRead/Entities/Enemies/50_HitZone.cs.
// This member definition lives in its enemy module; Session only dispatches it.
#include "50_HitZone.hpp"
#include "enemy_common.hpp"

namespace fruityprime::gameplay {

void Session::update_hit_zone(EnemyState& agent) {
    const auto parent = std::find_if(
        enemies_.begin(), enemies_.end(),
        [&agent](const EnemyState& value) {
            return agent.parent_enemy_id != 0 && value.id == agent.parent_enemy_id;
        });
    if (parent == enemies_.end() || !parent->active || parent->health == 0) {
        agent.active = false;
        agent.visible = false;
        agent.hit_zone_collidable = false;
        return;
    }

    agent.facing = parent->facing;
    agent.up = parent->up;
    agent.target_slot = parent->target_slot;
    agent.behavior_origin = parent->position;
    agent.velocity = {};
    agent.health = agent.health_max;
    agent.invulnerable = true;
    agent.active = true;

    if (parent->firespawn.supported) {
        agent.position = add(parent->position,
                             multiply(parent->up, -2867.0F / 4096.0F));
        agent.body_radius = 1.0F;
        agent.hit_zone_collidable = parent->visible
            && !parent->invulnerable
            && parent->firespawn_tangibility_timer >= 5u * 2u;
    } else if (parent->ithrak.supported) {
        agent.position = add(
            parent->position,
            add(multiply(parent->up, 0.93F),
                multiply(parent->facing, -0.8F)));
        agent.body_radius = 1.0F;
        agent.hit_zone_collidable = !parent->invulnerable;
    } else {
        agent.hit_zone_collidable = false;
    }
    agent.visible = agent.hit_zone_collidable;
    agent.state = agent.hit_zone_collidable ? 1 : 0;
}

} // namespace fruityprime::gameplay
