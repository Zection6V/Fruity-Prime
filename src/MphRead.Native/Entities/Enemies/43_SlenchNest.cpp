#include "enemy_decode_common.hpp"
// Native counterpart of src/MphRead/Entities/Enemies/43_SlenchNest.cs.
// This file is deliberately present even when the managed class currently
// shares a native controller; the descriptor and entry point prevent a
// many-classes-in-one gameplay.cpp regression.
#include "43_SlenchNest.hpp"
#include "enemy_common.hpp"

namespace fruityprime::gameplay {

void Session::update_slench_nest(EnemyState& agent) {
    // The nest follows the typed Slench linked-part controller.
    static_cast<void>(update_slench_part(agent));
}
} // namespace fruityprime::gameplay

static_assert(fruityprime::enemy::module_43_slench_nest::kModule.managed_class.size() != 0);

namespace fruityprime::enemy {

SlenchNestProfile slench_nest_profile() noexcept {
    return SlenchNestProfile{true, 100};
}

} // namespace fruityprime::enemy

