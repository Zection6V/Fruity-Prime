#include "25_GoreaHead.hpp"
#include "gorea_common.hpp"

namespace fruityprime::gameplay {

void Session::update_gorea_head(EnemyState& agent) {
    // Enemy25Entity is an attached, invincible head target. It never acquires
    // a player or runs EnemyInstanceEntity's generic movement/contact path.
    const auto parent = std::find_if(
        enemies_.begin(), enemies_.end(),
        [parent_id = agent.parent_enemy_id](const EnemyState& value) {
            return parent_id != 0 && value.id == parent_id
                && value.enemy_type == static_cast<std::uint8_t>(
                    formats::EnemyType::Gorea1A);
    });
    if (parent == enemies_.end()) {
        if (agent.gorea_head_flash_effect_id != 0) {
            detach_effect(agent.gorea_head_flash_effect_id, false);
            agent.gorea_head_flash_effect_id = 0;
        }
        agent.active = false;
        return;
    }

    agent.active = true;
    agent.visible = false;
    agent.invulnerable = true;
    // Enemy25 clears Visible but keeps CollideBeam while Gorea1A is active.
    // The native projectile pass uses this explicit bit for the same
    // invisible-but-collidable linked-part case.  State13 clears the managed
    // head's beam flag when the first body hands control to Gorea1B.
    agent.gorea_targetable = parent->visible && parent->gorea_state != 13;
    agent.gorea_beam_collidable = agent.gorea_targetable;
    agent.body_radius = 1314.0F / 4096.0F;
    agent.state = 0xff;
    agent.health = 65535;
    agent.health_max = 65535;
    agent.facing = parent->facing;
    agent.up = parent->up;
    // This is Gorea1A.GetNodeTransform("Head").  The bind-pose value is kept
    // for headless sessions which have no frontend model catalog bound.
    formats::Matrix4 node_transform{};
    if (sample_gorea_1a_node(*parent, "Head", node_transform)) {
        agent.position = {node_transform.m41, node_transform.m42,
                          node_transform.m43};
    } else {
        agent.position = gorea::local_position(
            *parent, {-0.000086F, 3.783119F, 0.875706F});
    }
    agent.behavior_origin = agent.position;
    agent.velocity = {};
    if (agent.gorea_head_flash_effect_id != 0) {
        const auto flash = std::find_if(
            effects_.begin(), effects_.end(),
            [effect_id = agent.gorea_head_flash_effect_id](
                const EffectState& value) {
                return value.id == effect_id;
            });
        if (flash == effects_.end() || flash->detached) {
            agent.gorea_head_flash_effect_id = 0;
        } else {
            constexpr float EyeFlashFacingOffset = 2949.0F / 4096.0F;
            constexpr float EyeFlashUpOffset = -939.0F / 4096.0F;
            const net::Vec3 flash_position = add(
                agent.position,
                add(multiply(parent->facing, EyeFlashFacingOffset),
                    multiply(parent->up, EyeFlashUpOffset)));
            update_effect_transform(agent.gorea_head_flash_effect_id,
                                    flash_position, parent->facing,
                                    parent->up);
        }
    }
    gorea::decrement(agent.gorea_damage_timer, gorea::frame_step(
        config_.tick_seconds));
}

} // namespace fruityprime::gameplay

static_assert(fruityprime::enemy::module_25_gorea_head::kModule.managed_class.size() != 0);
