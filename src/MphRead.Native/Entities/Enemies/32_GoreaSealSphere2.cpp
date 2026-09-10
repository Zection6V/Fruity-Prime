#include <algorithm>
#include "Entities/gameplay.hpp"
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

namespace fruityprime::enemy::module_32_gorea_seal_sphere_2 {

namespace {

// Metadata's goreaShoulderHits.
constexpr std::uint32_t ShoulderHitsEffect = 44u;
// Ten of the cartridge's frames, doubled for this head's rate.
constexpr std::uint32_t DamageFlashFrames = 10u * 2u;

} // namespace

void EnemyInitialize(gameplay::EnemyState& agent) noexcept {
    // Sixty-five thousand it never loses, against a cap of 840 that is
    // the phase's real health -- the sphere counts rather than dies.
    agent.health = 65535;
    agent.health_max = 840;
    agent.invulnerable = true;
    agent.visible = false;
    agent.body_radius = 1.0F;
    agent.gorea_damage = 0;
    agent.gorea_damage_timer = 0;
    agent.scan_id = 0;
}

void Activate(gameplay::EnemyState& agent,
              const std::uint16_t scan_id) noexcept {
    agent.scan_id = scan_id;
    agent.invulnerable = false;
    // Shootable but still not drawn: what the player aims at is Gorea's
    // own chest, and the sphere is the thing behind it that counts.
    agent.visible = false;
}

void Deactivate(gameplay::EnemyState& agent) noexcept {
    agent.scan_id = 0;
    agent.invulnerable = true;
    agent.visible = false;
}

void EnemyProcess(gameplay::EnemyState& agent,
                  const net::Vec3 node_position,
                  const bool gorea_visible) noexcept {
    if (gorea_visible) {
        agent.position = node_position;
    }
    if (agent.gorea_damage_timer > 0) {
        --agent.gorea_damage_timer;
    }
}

void SetDead(gameplay::EnemyState& agent) noexcept {
    agent.scan_id = 0;
    agent.invulnerable = true;
    agent.visible = false;
}

std::uint32_t DamageTimer(
    const gameplay::EnemyState& agent) noexcept {
    return agent.gorea_damage_timer;
}

Hit EnemyTakeDamage(gameplay::EnemyState& agent) noexcept {
    Hit hit;
    const std::uint32_t change = 65535u - agent.health;
    const std::uint32_t previous = agent.gorea_damage;
    agent.gorea_damage = std::min<std::uint32_t>(agent.gorea_damage + change,
                                                 agent.health_max);
    agent.health = 65535;
    hit.damage = agent.gorea_damage;
    if (agent.invulnerable) {
        return hit;
    }
    // A noise every ten damage -- but only within a phase.  Crossing a
    // thousand is a phase boundary, and that is already loud.
    if (agent.gorea_damage / 1000u == previous / 1000u
        && (agent.gorea_damage % 1000u) / 10u > (previous % 1000u) / 10u) {
        hit.milestone = true;
    }
    agent.gorea_damage_timer = DamageFlashFrames;
    static_cast<void>(ShoulderHitsEffect);
    return hit;
}

} // namespace fruityprime::enemy::module_32_gorea_seal_sphere_2


static_assert(fruityprime::enemy::module_32_gorea_seal_sphere_2::kModule.managed_class.size() != 0);
