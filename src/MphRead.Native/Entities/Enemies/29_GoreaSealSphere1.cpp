#include "29_GoreaSealSphere1.hpp"
#include "gorea_common.hpp"

namespace fruityprime::gameplay {

void Session::update_gorea_seal_sphere_1(EnemyState& agent) {
    // Enemy29Entity is Gorea1B's hidden damage accumulator. Its health is a
    // 0xffff sentinel; the public 3000-point progress is stored separately
    // and is consumed by the phase controller.
    const auto parent = std::find_if(
        enemies_.begin(), enemies_.end(),
        [parent_id = agent.parent_enemy_id](const EnemyState& value) {
            return parent_id != 0 && value.id == parent_id
                && value.enemy_type == static_cast<std::uint8_t>(
                    formats::EnemyType::Gorea1B);
        });
    if (parent == enemies_.end()) {
        agent.active = false;
        return;
    }

    const std::uint32_t step = gorea::frame_step(config_.tick_seconds);
    agent.active = true;
    agent.visible = false;
    agent.invulnerable = !agent.gorea_activated;
    agent.health = 65535;
    agent.health_max = 3000;
    agent.state = 0xff;
    agent.facing = parent->facing;
    agent.up = parent->up;
    // Enemy29Entity only follows ChestBall1 while the owning Gorea1B is
    // visible.  During the hidden/deactivation phase the last attached
    // position is intentionally retained by the managed entity.
    if (parent->visible) {
        formats::Matrix4 node_transform{};
        if (sample_gorea_1b_node(*parent, "ChestBall1", node_transform)) {
            agent.position = {
                node_transform.m41, node_transform.m42, node_transform.m43};
        } else {
            // Headless sessions may not have a renderer-owned Gorea1B model.
            // Keep a deterministic attachment approximation for that path.
            agent.position = gorea::local_position(
                *parent, {0.0F, 0.0F, 0.35F});
        }
    }
    agent.behavior_origin = agent.position;
    agent.velocity = {};
    gorea::decrement(agent.gorea_damage_timer, step);
    agent.gorea_targetable = agent.gorea_activated && parent->visible;
    agent.gorea_beam_collidable = agent.gorea_activated;
}

} // namespace fruityprime::gameplay

static_assert(fruityprime::enemy::module_29_gorea_seal_sphere_1::kModule.managed_class.size() != 0);
