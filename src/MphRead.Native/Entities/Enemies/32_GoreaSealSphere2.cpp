#include "32_GoreaSealSphere2.hpp"
#include "gorea_common.hpp"

namespace fruityprime::gameplay {

void Session::update_gorea_seal_sphere_2(EnemyState& agent) {
    // Enemy32Entity follows Gorea2's ChestBall1 and stores the phase damage
    // that actually drives the second boss. It is never a generic contact
    // enemy and its native health remains the managed 0xffff sentinel.
    const auto parent = std::find_if(
        enemies_.begin(), enemies_.end(),
        [parent_id = agent.parent_enemy_id](const EnemyState& value) {
            return parent_id != 0 && value.id == parent_id
                && value.enemy_type == static_cast<std::uint8_t>(
                    formats::EnemyType::Gorea2);
        });
    if (parent == enemies_.end()) {
        agent.active = false;
        return;
    }

    const std::uint32_t step = gorea::frame_step(config_.tick_seconds);
    agent.active = true;
    agent.health = 65535;
    agent.health_max = 840;
    agent.facing = parent->facing;
    agent.up = parent->up;
    // Enemy32 only refreshes its node attachment while Gorea2 is Visible.
    // Preserve the last node position during the fade instead of letting a
    // hidden parent move the collision sphere underneath the player.
    if (parent->visible) {
        formats::Matrix4 node_transform{};
        if (sample_gorea_2_node(*parent, "ChestBall1", node_transform)) {
            agent.position = {
                node_transform.m41, node_transform.m42, node_transform.m43};
        } else {
            // Keep the headless gameplay path usable when no renderer-owned
            // Gorea2 model was bound to this Session.
            agent.position = gorea::local_position(
                *parent, {0.0F, 0.0F, 0.35F});
        }
    }
    agent.behavior_origin = agent.position;
    agent.velocity = {};
    agent.visible = parent->visible && agent.gorea_activated
        && agent.gorea_visibility;
    agent.invulnerable = !agent.gorea_activated
        || (parent->gorea_flags & (gorea::Gorea2Teleporting
                                   | gorea::Gorea2Teleporting2
                                   | gorea::Gorea2DamageFlash)) != 0;
    // Enemy32 keeps CollideBeam set even while hidden by LOS/teleport. The
    // combat boundary will reject the damage when invulnerable, but still
    // performs the managed side effects (notably stopping Gorea2's laser).
    agent.gorea_targetable = agent.visible && !agent.invulnerable;
    agent.gorea_beam_collidable = agent.gorea_activated;
    agent.state = parent->state;
    gorea::decrement(agent.gorea_damage_timer, step);
}

} // namespace fruityprime::gameplay

static_assert(fruityprime::enemy::module_32_gorea_seal_sphere_2::kModule.managed_class.size() != 0);
