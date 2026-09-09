// Native counterpart of src/MphRead/Entities/Enemies/45_SlenchTurret.cs.
// This file is deliberately present even when the managed class currently
// shares a native controller; the descriptor and entry point prevent a
// many-classes-in-one gameplay.cpp regression.
#include "45_SlenchTurret.hpp"
#include "enemy_common.hpp"

namespace fruityprime::gameplay {

void Session::update_slench_turret(EnemyState& agent) {
    // The linked Slench turret shares the typed turret controller.
    static_cast<void>(update_turret(agent));
}

} // namespace fruityprime::gameplay

static_assert(fruityprime::enemy::module_45_slench_turret::kModule.managed_class.size() != 0);

