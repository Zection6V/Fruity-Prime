// Native counterpart of src/MphRead/Entities/EnemyInstanceEntity.cs.
#include "Entities/runtime_entities.hpp"
#include "runtime_entity_helpers.hpp"

namespace fruityprime::runtime {

using namespace detail;

EnemyInstanceEntity::EnemyInstanceEntity(std::uint32_t id,
                                         std::uint8_t enemy_type,
                                         net::Vec3 position) noexcept
    : Entity(id, Kind::EnemyInstance, position), enemy_type_(enemy_type) {}

void EnemyInstanceEntity::set_states(std::uint8_t state_a,
                                     std::uint8_t state_b) noexcept {
    state_a_ = state_a;
    state_b_ = state_b;
}

bool EnemyInstanceEntity::take_damage(std::uint32_t damage) noexcept {
    if (!active_ || (flags_ & Invincible) != 0 || damage == 0) {
        return false;
    }
    const auto amount = std::min<std::uint32_t>(damage, health_);
    health_ = static_cast<std::uint16_t>(health_ - amount);
    if (health_ == 0) {
        active_ = false;
    }
    return amount != 0;
}

void EnemyInstanceEntity::handle_message(
    const messaging::MessageInfo& message) noexcept {
    if (cartridge_message(message, 7)) { // Damage
        const std::uint32_t amount = message.parameter1 > 0
            ? static_cast<std::uint32_t>(message.parameter1) : 1u;
        static_cast<void>(take_damage(amount));
        return;
    }
    if (cartridge_message(message, 21) // Death
        || cartridge_message(message, 6)) { // Destroyed
        health_ = 0;
        active_ = false;
        return;
    }
    Entity::handle_message(message);
}

bool EnemyInstanceEntity::process(float seconds) noexcept {
    if (!active_) {
        return false;
    }
    seconds = finite_seconds(seconds);
    advance_age(seconds);
    if ((flags_ & Static) == 0) {
        position_ = add(position_, multiply(velocity_, seconds));
    }
    return active_;
}

} // namespace fruityprime::runtime

