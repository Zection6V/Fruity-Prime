// Native counterpart of src/MphRead/Entities/Enemies/36_Voldrum.cs.
// This file is deliberately present even when the managed class currently
// shares a native controller; the descriptor and entry point prevent a
// many-classes-in-one gameplay.cpp regression.
#include "36_Voldrum.hpp"
#include "enemy_common.hpp"

namespace fruityprime::gameplay {

void Session::update_voldrum1(EnemyState& agent) {
    // Voldrum1 shares the typed Voldrum controller with Voldrum2.
    static_cast<void>(update_voldrum(agent));
}

} // namespace fruityprime::gameplay

static_assert(fruityprime::enemy::module_36_voldrum_1::kModule.managed_class.size() != 0);

