// Native counterpart of src/MphRead/Entities/Enemies/47_GreaterIthrak.cs.
// This file is deliberately present even when the managed class currently
// shares a native controller; the descriptor and entry point prevent a
// many-classes-in-one gameplay.cpp regression.
#include "47_GreaterIthrak.hpp"
#include "enemy_common.hpp"

namespace fruityprime::gameplay {

void Session::update_greater_ithrak(EnemyState& agent) {
    // Greater Ithrak shares the typed Ithrak controller with Lesser Ithrak.
    static_cast<void>(update_ithrak(agent));
}

} // namespace fruityprime::gameplay

static_assert(fruityprime::enemy::module_47_greater_ithrak::kModule.managed_class.size() != 0);

