// Native counterpart of src/MphRead/Entities/Enemies/21_CretaphidCrystal.cs.
// This file is deliberately present even when the managed class currently
// shares a native controller; the descriptor and entry point prevent a
// many-classes-in-one gameplay.cpp regression.
#include "21_CretaphidCrystal.hpp"
#include "enemy_common.hpp"

namespace fruityprime::gameplay {

void Session::update_cretaphid_crystal(EnemyState& agent) {
    // The crystal is the second child path of the typed Cretaphid part controller.
    static_cast<void>(update_cretaphid_part(agent));
}

} // namespace fruityprime::gameplay

static_assert(fruityprime::enemy::module_21_cretaphid_crystal::kModule.managed_class.size() != 0);

