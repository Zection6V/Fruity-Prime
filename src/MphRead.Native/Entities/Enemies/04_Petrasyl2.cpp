// Native counterpart of src/MphRead/Entities/Enemies/04_Petrasyl2.cs.
// This file is deliberately present even when the managed class currently
// shares a native controller; the descriptor and entry point prevent a
// many-classes-in-one gameplay.cpp regression.
#include "04_Petrasyl2.hpp"
#include "enemy_common.hpp"

namespace fruityprime::gameplay {

void Session::update_petrasyl2(EnemyState& agent) {
    // Petrasyl2 shares the typed Petrasyl state machine with the base module.
    static_cast<void>(update_petrasyl(agent));
}

} // namespace fruityprime::gameplay

static_assert(fruityprime::enemy::module_04_petrasyl2::kModule.managed_class.size() != 0);

