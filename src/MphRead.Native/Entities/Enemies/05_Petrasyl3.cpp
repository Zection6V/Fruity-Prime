// Native counterpart of src/MphRead/Entities/Enemies/05_Petrasyl3.cs.
// This file is deliberately present even when the managed class currently
// shares a native controller; the descriptor and entry point prevent a
// many-classes-in-one gameplay.cpp regression.
#include "05_Petrasyl3.hpp"
#include "enemy_common.hpp"

namespace fruityprime::gameplay {

void Session::update_petrasyl3(EnemyState& agent) {
    // Petrasyl3 shares the typed Petrasyl state machine with the base module.
    static_cast<void>(update_petrasyl(agent));
}

} // namespace fruityprime::gameplay

static_assert(fruityprime::enemy::module_05_petrasyl3::kModule.managed_class.size() != 0);

