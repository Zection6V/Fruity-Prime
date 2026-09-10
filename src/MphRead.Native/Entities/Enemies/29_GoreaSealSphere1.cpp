#include <algorithm>
#include "Entities/gameplay.hpp"
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

namespace fruityprime::enemy::module_29_gorea_seal_sphere_1 {

namespace {

// Metadata's goreaShoulderHits.
constexpr std::uint32_t ShoulderHitsEffect = 44u;
// Ten of the cartridge's frames, doubled for this head's rate.
constexpr std::uint32_t DamageFlashFrames = 10u * 2u;

} // namespace

void EnemyInitialize(gameplay::EnemyState& agent) noexcept {
    // Sixty-five thousand it never loses, against a cap of 3000 that is
    // the phase's real health -- the sphere counts rather than dies.
    agent.health = 65535;
    agent.health_max = 3000;
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

} // namespace fruityprime::enemy::module_29_gorea_seal_sphere_1


static_assert(fruityprime::enemy::module_29_gorea_seal_sphere_1::kModule.managed_class.size() != 0);
