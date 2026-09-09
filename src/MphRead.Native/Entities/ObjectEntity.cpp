// Native counterpart of src/MphRead/Entities/ObjectEntity.cs.
#include "Entities/static_entities.hpp"
#include "static_entity_helpers.hpp"

namespace fruityprime::entities::static_entities {

using namespace detail;

ObjectEntity::ObjectEntity(const scene::EntityInstance& source)
    : Entity(source), data_(data_or_default<scene::ObjectData>(source)),
      state_(static_cast<std::uint8_t>(data_.flags & 0x03u)) {}

bool ObjectEntity::process(float seconds) noexcept {
    if (!active_) {
        return true;
    }
    advance_age(seconds);
    seconds = valid_seconds(seconds);
    if (data_.effect_flags == 0 || data_.effect_interval == 0
        || seconds == 0.0F) {
        return true;
    }
    effect_timer_ -= seconds;
    const float interval = static_cast<float>(data_.effect_interval) / 60.0F;
    while (effect_timer_ <= 0.0F) {
        ++effect_count_;
        effect_timer_ += std::max(1.0F / 60.0F, interval);
    }
    return true;
}

void ObjectEntity::handle_message(
    const messaging::MessageInfo& message) noexcept {
    if (cartridge_message(message, 5)) { // SetActive carries Object state.
        set_state(static_cast<std::uint8_t>(std::clamp(
            message.parameter1, 0, 2)));
        return;
    }
    if (activation_message(message) || is_activation_message(message.message)) {
        set_state(2);
        if (effect_timer_ < 0.0F) {
            effect_timer_ = 0.0F;
        }
        return;
    }
    if (deactivation_message(message)
        || is_deactivation_message(message.message)) {
        set_state(0);
        return;
    }
    Entity::handle_message(message);
}

void ObjectEntity::set_position(scene::VolumePoint value) noexcept {
    const scene::VolumePoint delta = subtract(value, position_);
    Entity::set_position(value);
    translate_volume(data_.volume, delta);
}

} // namespace fruityprime::entities::static_entities

