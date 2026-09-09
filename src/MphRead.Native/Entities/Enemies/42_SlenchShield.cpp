#include "enemy_decode_common.hpp"
// Native counterpart of src/MphRead/Entities/Enemies/42_SlenchShield.cs.
// This file is deliberately present even when the managed class currently
// shares a native controller; the descriptor and entry point prevent a
// many-classes-in-one gameplay.cpp regression.
#include "42_SlenchShield.hpp"
#include "enemy_common.hpp"

namespace fruityprime::gameplay {

void Session::update_slench_shield(EnemyState& agent) {
    // The shield follows the typed Slench linked-part controller.
    static_cast<void>(update_slench_part(agent));
}
} // namespace fruityprime::gameplay

static_assert(fruityprime::enemy::module_42_slench_shield::kModule.managed_class.size() != 0);

namespace fruityprime::enemy {

SlenchShieldProfile slench_shield_profile() noexcept {
    return SlenchShieldProfile{true, 255, 1.0F, 2.9F};
}

} // namespace fruityprime::enemy

