// Native counterpart of src/MphRead/Entities/DoorEntity.cs.
#include "Entities/static_entities.hpp"
#include "static_entity_helpers.hpp"

namespace fruityprime::entities::static_entities {

using namespace detail;

DoorEntity::DoorEntity(const scene::EntityInstance& source)
    : Entity(source), data_(data_or_default<scene::DoorData>(source)),
      locked_(data_.locked) {}

bool DoorEntity::process(float seconds) noexcept {
    advance_age(seconds);
    seconds = valid_seconds(seconds);
    if (locked_ || seconds == 0.0F) {
        return true;
    }
    constexpr float transition_seconds = 0.25F;
    const float step = seconds / transition_seconds;
    if (opening_) {
        open_fraction_ = std::min(1.0F, open_fraction_ + step);
    } else {
        open_fraction_ = std::max(0.0F, open_fraction_ - step);
    }
    return true;
}

void DoorEntity::handle_message(
    const messaging::MessageInfo& message) noexcept {
    if (cartridge_message(message, 16)) { // Unlock
        locked_ = false;
        return;
    }
    if (cartridge_message(message, 17)) { // Lock
        locked_ = true;
        return;
    }
    if (message.message == messaging::Message::Open
        || message.message == messaging::Message::Activate
        || cartridge_message(message, 18)
        || cartridge_message(message, 44)) {
        if (!locked_) {
            active_ = true;
            opening_ = true;
        }
        return;
    }
    if (message.message == messaging::Message::Close
        || message.message == messaging::Message::Deactivate
        || cartridge_message(message, 6)
        || cartridge_message(message, 45)) {
        opening_ = false;
        active_ = true;
        return;
    }
    if (cartridge_message(message, 5)) {
        active_ = message.parameter1 != 0;
        return;
    }
    Entity::handle_message(message);
}

FhDoorEntity::FhDoorEntity(const scene::EntityInstance& source)
    : Entity(source), data_(data_or_default<scene::FhDoorData>(source)) {}

} // namespace fruityprime::entities::static_entities
