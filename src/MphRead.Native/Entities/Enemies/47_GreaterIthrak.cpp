// Native port of src/MphRead/Entities/Enemies/47_GreaterIthrak.cs.
// This member definition lives in its enemy module; Session only dispatches it.
#include "Metadata/enemy_subroutines.hpp"
#include "enemy_scene.hpp"
#include "47_GreaterIthrak.hpp"
#include "enemy_common.hpp"

namespace fruityprime::gameplay {

Enemy47Entity::Enemy47Entity(const EnemyScene& scene, EnemyState& agent,
                             net::PlayerState* main) noexcept
    : Enemy46Entity(scene, agent, main) {}

// Enemy47Entity.EnemyInitialize.  The Greater Ithrak reads S05 rather
// than S00 -- the same four volumes out of a different record -- and
// passes nought for effectiveness, meaning it resists nothing at all.
// That is the difference between the two as a fight: the Greater has more
// to shoot at rather than more armour.
void Enemy47Entity::EnemyInitialize() {
    Setup(0u);
}

bool Enemy47Entity::CallSubroutine() {
    return metadata::call_subroutine(
        metadata::Enemy47Subroutines, agent_.sub_id, agent_.next_state,
        [this](std::uint8_t index) { return RunBehavior(index); });
}

void Enemy47Entity::UpdateMouthMaterial() {
    // Fourteen against the Lesser's thirty-one: a Greater Ithrak's mouth
    // is dim, which is what tells them apart across a room.
    agent_.ithrak_mouth_brightness = 14;
}

} // namespace fruityprime::gameplay

static_assert(fruityprime::enemy::module_47_greater_ithrak::kModule.managed_class.size() != 0);

namespace fruityprime::enemy::module_47_greater_ithrak {

void EnemyInitialize(gameplay::EnemyState& agent) noexcept {
    gameplay::EnemyScene empty;
    gameplay::Enemy47Entity(empty, agent, nullptr).EnemyInitialize();
}

} // namespace fruityprime::enemy::module_47_greater_ithrak
