#include "27_GoreaLeg.hpp"
#include "gorea_common.hpp"

namespace fruityprime::gameplay {
namespace {

// Enemy27Entity's hurt volume is not a sphere.  It is the 736/4096-radius,
// 9700/4096-long cylinder authored in EnemyInitialize and replaced every
// frame from the animated L/R/BK_Knee node's Row0.  The native gameplay
// session does not own enemy ModelInstance objects yet, so these are the
// model-space values from Gorea1A_lod0's bind pose. They are used only when a
// headless Session has no frontend ModelInstance bound; the normal window path
// samples the animated node transform directly.
constexpr float LegCylinderRadius = 736.0F / 4096.0F;
constexpr float LegCylinderLength = 9700.0F / 4096.0F;

constexpr std::array<net::Vec3, 3> GoreaKneePositions{{
    {0.971388F, 1.680810F, 0.807908F}, // L_Knee, Index 0
    {-0.972359F, 1.680884F, 0.806264F}, // R_Knee, Index 1
    {-0.000027F, 1.627308F, -0.878427F} // BK_Knee, Index 2
}};

constexpr std::array<net::Vec3, 3> GoreaKneeRow0{{
    {-0.072076F, 0.937776F, 0.003080F}, // -L_Knee.Row0, Index != 1
    {0.071597F, 0.938032F, 0.002273F}, // R_Knee.Row0
    {0.000095F, 0.991623F, 0.129164F} // -BK_Knee.Row0, Index != 1
}};

[[nodiscard]] net::Vec3 transform_direction(const EnemyState& parent,
                                             net::Vec3 local) noexcept {
    return normalized_or(
        subtract(gorea::local_position(parent, local), parent.position),
        parent.up);
}

// This is the same calculation as CollisionDetection.CheckSphereOverlapVolume
// for a managed sphere against a cylinder.  The endpoint padding by the
// player radius is intentional: the managed function accepts contact with
// either cap, not just the cylinder's centerline interval.
[[nodiscard]] bool sphere_overlaps_cylinder(net::Vec3 sphere_position,
                                            float sphere_radius,
                                            net::Vec3 cylinder_position,
                                            net::Vec3 cylinder_axis,
                                            float cylinder_length,
                                            float cylinder_radius) noexcept {
    const net::Vec3 between = subtract(sphere_position, cylinder_position);
    const float axial = dot(cylinder_axis, between);
    if (axial < -sphere_radius
        || axial > cylinder_length + sphere_radius) {
        return false;
    }
    const net::Vec3 radial = subtract(
        between, multiply(cylinder_axis, axial));
    const float radius = sphere_radius + cylinder_radius;
    return length_squared(radial) <= radius * radius;
}

} // namespace

void Session::update_gorea_leg(EnemyState& agent) {
    // Enemy27Entity is a cylinder attached to one of Gorea1A's knees. It is
    // invincible and has no target AI; its only gameplay work is the player
    // push/damage test performed while the parent is visible.
    const auto parent = std::find_if(
        enemies_.begin(), enemies_.end(),
        [parent_id = agent.parent_enemy_id](const EnemyState& value) {
            return parent_id != 0 && value.id == parent_id
                && value.enemy_type == static_cast<std::uint8_t>(
                    formats::EnemyType::Gorea1A);
        });
    if (parent == enemies_.end()) {
        agent.active = false;
        return;
    }

    const std::size_t index = std::min<std::size_t>(agent.gorea_index, 2);
    agent.active = true;
    agent.invulnerable = true;
    agent.gorea_activated = parent->visible && parent->gorea_state != 13;
    agent.visible = agent.gorea_activated;
    agent.gorea_targetable = agent.gorea_activated;
    agent.gorea_beam_collidable = agent.gorea_activated;
    agent.health = 65535;
    agent.health_max = 120;
    agent.state = 0xff;
    agent.facing = parent->facing;
    agent.up = parent->up;
    const char* const node_name = index == 0
        ? "L_Knee" : (index == 1 ? "R_Knee" : "BK_Knee");
    formats::Matrix4 node_transform{};
    const bool sampled = sample_gorea_1a_node(
        *parent, node_name, node_transform);
    if (sampled) {
        agent.position = {node_transform.m41, node_transform.m42,
                          node_transform.m43};
    } else {
        agent.position = gorea::local_position(*parent, GoreaKneePositions[index]);
    }
    agent.behavior_origin = agent.position;
    agent.velocity = {};
    if (!agent.visible) {
        return;
    }

    net::Vec3 cylinder_axis;
    if (sampled) {
        cylinder_axis = normalized_or(
            {node_transform.m11, node_transform.m12, node_transform.m13},
            parent->up);
        if (index != 1) {
            cylinder_axis = multiply(cylinder_axis, -1.0F);
        }
    } else {
        cylinder_axis = transform_direction(*parent, GoreaKneeRow0[index]);
    }
    const net::Vec3 cylinder_position = subtract(
        agent.position, multiply(cylinder_axis, LegCylinderLength));
    for (auto& player : players_) {
        if (!objective_player(player)
            || !sphere_overlaps_cylinder(
                player.position, config_.body_radius, cylinder_position,
                cylinder_axis, LegCylinderLength, LegCylinderRadius)) {
            continue;
        }
        const net::Vec3 between = normalized_or(
            {player.position.x - agent.position.x, 0.0F,
             player.position.z - agent.position.z}, agent.facing);
        player.speed = add(player.speed, multiply(between, 0.25F));
        apply_enemy_contact_damage(agent, player, 10);
    }
}

} // namespace fruityprime::gameplay

static_assert(fruityprime::enemy::module_27_gorea_leg::kModule.managed_class.size() != 0);
