// Native counterpart of src/MphRead/Entities/BombEntity.cs.
#include "Entities/runtime_entities.hpp"
#include "runtime_entity_helpers.hpp"

namespace fruityprime::runtime {

using namespace detail;

BombEntity::BombEntity(std::uint32_t id, BombType type, std::uint8_t owner_slot,
                       net::Vec3 position, net::Vec3 velocity) noexcept
    : Entity(id, Kind::Bomb, position), bomb_type_(type),
      owner_slot_(owner_slot), velocity_(velocity) {}

void BombEntity::trigger() noexcept {
    if (!active_ || exploding_ || exploded_) {
        return;
    }
    exploding_ = true;
    exploded_ = true;
    active_ = false;
}

void BombEntity::handle_message(
    const messaging::MessageInfo& message) noexcept {
    if (cartridge_message(message, 20) // Impact
        || cartridge_message(message, 21) // Death
        || internal_message(message, messaging::Message::Stop)) {
        trigger();
        return;
    }
    Entity::handle_message(message);
}

bool BombEntity::process(float seconds) noexcept {
    if (!active_) {
        return false;
    }
    seconds = finite_seconds(seconds);
    advance_age(seconds);
    position_ = add(position_, multiply(velocity_, seconds));
    if (countdown_seconds_ > 0.0F) {
        countdown_seconds_ -= seconds;
        if (countdown_seconds_ <= 0.0F) {
            countdown_seconds_ = 0.0F;
            trigger();
        }
    }
    return active_;
}

} // namespace fruityprime::runtime

