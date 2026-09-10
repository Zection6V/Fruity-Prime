// Native counterpart of src/MphRead/Entities/Enemies/21_CretaphidCrystal.cs.
// This file is deliberately present even when the managed class currently
// shares a native controller; the descriptor and entry point prevent a
// many-classes-in-one gameplay.cpp regression.
#include <cmath>
#include <functional>
#include "21_CretaphidCrystal.hpp"
#include "enemy_common.hpp"
#include "Entities/gameplay.hpp"

namespace fruityprime::gameplay {

namespace {

// Native counterpart of Enemy21Entity.
//
// A crystal is not an enemy on its own.  It hangs off a node of the
// Cretaphid's model, so it moves when the Cretaphid moves, and it fires
// when the Cretaphid tells it to.  Breaking one does not kill it: it
// keeps one energy and turns invulnerable, and the Cretaphid is told,
// because the crystals are how the fight is scored.
class Enemy21Entity final {
public:
    Enemy21Entity(std::function<void()> update_part,
                  EnemyState& agent) noexcept
        : update_part_(std::move(update_part)), agent_(agent) {}

    // Enemy21Entity.EnemyProcess.
    void EnemyProcess() {
        // The crystal's position is its attach node's, offset by the
        // Cretaphid's own -- which is why it is recomputed every frame
        // rather than moved.
        if (update_part_) {
            update_part_();
        }
        static_cast<void>(agent_);
    }

private:
    std::function<void()> update_part_;
    EnemyState& agent_;
};

} // namespace

void Session::update_cretaphid_crystal(EnemyState& agent) {
    Enemy21Entity([this, &agent]() { update_cretaphid_part(agent); },
                  agent).EnemyProcess();
}

} // namespace fruityprime::gameplay

static_assert(fruityprime::enemy::module_21_cretaphid_crystal::kModule.managed_class.size() != 0);

namespace fruityprime::enemy::module_21_cretaphid_crystal {

void SetUp(gameplay::EnemyState& agent, const std::uint8_t segment_index,
           const std::uint16_t health) noexcept {
    agent.health = agent.health_max = health;
    agent.cretaphid_segment_index = segment_index;
    // Invulnerable from the start: a crystal only becomes shootable when
    // the Cretaphid's current phase exposes it.
    agent.invulnerable = true;
    agent.body_radius = 1.0F;
}

Shot SpawnBeam(const gameplay::EnemyState& agent,
               const net::Vec3 player_position) noexcept {
    const net::Vec3 aim{player_position.x - agent.position.x,
                        player_position.y + 0.5F - agent.position.y,
                        player_position.z - agent.position.z};
    const float length = std::sqrt(aim.x * aim.x + aim.y * aim.y
                                   + aim.z * aim.z);
    if (length <= 0.0001F) {
        // Standing inside the crystal: there is no direction to fire in,
        // so it fires along its own facing rather than at nothing.
        return {agent.position, agent.facing};
    }
    return {agent.position,
            {aim.x / length, aim.y / length, aim.z / length}};
}

bool EnemyTakeDamage(gameplay::EnemyState& agent) noexcept {
    if (agent.health != 0) {
        return false;
    }
    // Broken, not dead.  One energy and no more damage, so the crystal
    // stays on its node as a broken one rather than vanishing.
    agent.health = 1;
    agent.invulnerable = true;
    return false;
}

} // namespace fruityprime::enemy::module_21_cretaphid_crystal

