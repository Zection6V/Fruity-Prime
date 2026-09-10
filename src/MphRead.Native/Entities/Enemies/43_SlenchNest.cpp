#include "enemy_decode_common.hpp"
// Native counterpart of src/MphRead/Entities/Enemies/43_SlenchNest.cs.
// This file is deliberately present even when the managed class currently
// shares a native controller; the descriptor and entry point prevent a
// many-classes-in-one gameplay.cpp regression.
#include "43_SlenchNest.hpp"
#include "Entities/gameplay.hpp"
#include "enemy_common.hpp"

namespace fruityprime::gameplay {

void Session::update_slench_nest(EnemyState& agent) {
    // The nest follows the typed Slench linked-part controller.
    static_cast<void>(update_slench_part(agent));
}
} // namespace fruityprime::gameplay

static_assert(fruityprime::enemy::module_43_slench_nest::kModule.managed_class.size() != 0);

namespace fruityprime::enemy::module_43_slench_nest {

void EnemyInitialize(gameplay::EnemyState& agent) noexcept {
    // The nest is scenery with a healthbar: a hundred energy it never
    // loses, no hurt volume at all, and no distance at which it stops
    // being processed -- everything about it is the Slench's, and the
    // Slench is what the player is actually shooting.
    agent.health = agent.health_max = 100;
    agent.invulnerable = true;
    agent.visible = true;
    agent.body_radius = 0.0F;
}

} // namespace fruityprime::enemy::module_43_slench_nest

namespace fruityprime::enemy {

SlenchNestProfile slench_nest_profile() noexcept {
    return SlenchNestProfile{true, 100};
}

} // namespace fruityprime::enemy

