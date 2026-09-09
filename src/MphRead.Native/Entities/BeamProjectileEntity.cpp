// Native counterpart of src/MphRead/Entities/BeamProjectileEntity.cs.
#include "Entities/runtime_entities.hpp"
#include "runtime_entity_helpers.hpp"

namespace fruityprime::runtime {

using namespace detail;

bool BeamProjectileEntity::step(float seconds) noexcept {
    seconds = finite_seconds(seconds);
    if (seconds == 0.0F) {
        return true;
    }

    velocity = add(velocity, multiply(acceleration, seconds));
    position = add(position, multiply(safe_velocity(*this), seconds));
    age += seconds;
    lifetime += seconds;
    if (max_lifetime > 0.0F && age >= max_lifetime) {
        flags |= Collided;
        return false;
    }
    return true;
}

void BeamProjectileEntity::reposition(net::Vec3 offset) noexcept {
    position = add(position, offset);
    spawn_position = add(spawn_position, offset);
    back_position = add(back_position, offset);
    for (auto& point : past_positions) {
        point = add(point, offset);
    }
}

} // namespace fruityprime::runtime
