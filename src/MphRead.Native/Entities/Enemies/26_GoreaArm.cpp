#include "26_GoreaArm.hpp"
#include "gorea_common.hpp"

namespace fruityprime::gameplay {

void Session::update_gorea_arm(EnemyState& agent) {
    // Enemy26Entity is a linked shoulder target. Damage is accumulated by
    // damage_enemy(); this process only follows the parent's animated node
    // and maintains the two timers used by the arm material/effect code.
    const auto parent = std::find_if(
        enemies_.begin(), enemies_.end(),
        [parent_id = agent.parent_enemy_id](const EnemyState& value) {
            return parent_id != 0 && value.id == parent_id
                && value.enemy_type == static_cast<std::uint8_t>(
                    formats::EnemyType::Gorea1A);
        });
    if (parent == enemies_.end()) {
        if (agent.gorea_arm_effect_id != 0) {
            detach_effect(agent.gorea_arm_effect_id, true);
            agent.gorea_arm_effect_id = 0;
        }
        agent.active = false;
        return;
    }

    constexpr std::uint32_t ArmDead = 1u;
    constexpr float ElbowMuzzleOffset = 8343.0F / 4096.0F;
    const std::uint32_t step = gorea::frame_step(config_.tick_seconds);
    agent.active = true;
    agent.health = 65535;
    agent.health_max = 120;
    agent.invulnerable = true;
    agent.state = 0xff;
    agent.facing = parent->facing;
    agent.up = parent->up;
    // Enemy26 follows L_Shoulder/R_Shoulder from Gorea1A_lod0. The bind-pose
    // values remain the fallback for headless sessions without a model.
    const std::array<net::Vec3, 2> shoulder_positions{{
        {0.880849F, 3.632664F, 0.101482F}, // L_Shoulder
        {-0.880869F, 3.632592F, 0.101316F} // R_Shoulder
    }};
    const std::size_t index = std::min<std::size_t>(agent.gorea_index, 1);
    const char* const node_name = index == 0 ? "L_Shoulder" : "R_Shoulder";
    formats::Matrix4 node_transform{};
    if (sample_gorea_1a_node(*parent, node_name, node_transform)) {
        agent.position = {node_transform.m41, node_transform.m42,
                          node_transform.m43};
    } else {
        agent.position = gorea::local_position(
            *parent, shoulder_positions[index]);
    }
    agent.behavior_origin = agent.position;
    agent.velocity = {};
    if ((agent.gorea_flags & ArmDead) != 0) {
        // Enemy24.StopShots/Enemy26.StopShotEffect unlinks the held charge
        // entry before the shoulder-kill animation. Do the same at the
        // gameplay boundary so a dead arm cannot leave an immortal effect
        // attached to its last node.
        if (agent.gorea_arm_effect_id != 0) {
            detach_effect(agent.gorea_arm_effect_id, true);
            agent.gorea_arm_effect_id = 0;
        }
        agent.visible = false;
        agent.gorea_beam_collidable = false;
        return;
    }

    const auto update_arm_effect = [&]() {
        if (agent.gorea_arm_effect_id == 0) {
            return;
        }
        const char* const elbow_name = index == 0 ? "L_Elbow" : "R_Elbow";
        formats::Matrix4 transform{};
        net::Vec3 position = agent.position;
        net::Vec3 up = parent->up;
        net::Vec3 facing = parent->facing;
        if (sample_gorea_1a_node(*parent, elbow_name, transform)) {
            position = {transform.m41, transform.m42, transform.m43};
            up = index == 0
                ? net::Vec3{transform.m11, transform.m12, transform.m13}
                : net::Vec3{transform.m31, transform.m32, transform.m33};
            facing = {transform.m21, transform.m22, transform.m23};
        }
        up = normalized_or(up, parent->up);
        facing = normalized_or(facing, parent->facing);
        // Enemy26.EnemyProcess moves the held charge entry from the elbow to
        // the weapon muzzle along the selected elbow up axis every frame.
        position = add(position, multiply(up, ElbowMuzzleOffset));
        update_effect_transform(agent.gorea_arm_effect_id, position, facing,
                                up);
    };

    if (agent.gorea_damage > 60u) {
        if (agent.gorea_arm_effect_id == 0) {
            // goreaShoulderDamageLoop. The managed entry is held by the arm,
            // has ElementExtension enabled, and is only unlinked once the
            // accumulated shoulder damage drops back below the threshold.
            agent.gorea_arm_effect_id = spawn_effect(
                43, agent.position, parent->facing, agent.id, 0.25F, false,
                true, true, parent->up);
        }
        update_arm_effect();
    } else if (agent.gorea_arm_effect_id != 0) {
        detach_effect(agent.gorea_arm_effect_id, true);
        agent.gorea_arm_effect_id = 0;
    }
    agent.gorea_activated = parent->visible && parent->gorea_state != 13;
    agent.visible = agent.gorea_activated;
    agent.gorea_beam_collidable = agent.gorea_activated;
    agent.invulnerable = parent->gorea_state == 0
        && agent.gorea_state_timer == 0;
    gorea::decrement(agent.gorea_state_timer, step); // regeneration window
    gorea::decrement(agent.gorea_damage_timer, step); // shoulder flash
}

} // namespace fruityprime::gameplay

static_assert(fruityprime::enemy::module_26_gorea_arm::kModule.managed_class.size() != 0);
