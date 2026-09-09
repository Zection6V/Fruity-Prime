// Native counterpart of src/MphRead/Entities/EntityBase.cs.
#include "Entities/entity_states.hpp"
#include "Entities/entity_records.hpp"
#include "Entities/static_entities.hpp"
#include "static_entity_helpers.hpp"

namespace fruityprime::entities::static_entities {

using namespace detail;

Entity::Entity(const scene::EntityInstance& source) noexcept
    : id_(source.entity_id), kind_(source.kind),
      position_{source.position.x.to_float(), source.position.y.to_float(),
                source.position.z.to_float()},
      up_{source.up_vector.x.to_float(), source.up_vector.y.to_float(),
          source.up_vector.z.to_float()},
      facing_{source.facing_vector.x.to_float(),
              source.facing_vector.y.to_float(),
              source.facing_vector.z.to_float()},
      node_name_(source.node_name) {}

void Entity::set_alpha(float value) noexcept {
    alpha_ = std::isfinite(value) ? std::clamp(value, 0.0F, 1.0F) : 0.0F;
}

void Entity::set_position(scene::VolumePoint value) noexcept {
    position_ = value;
}

void Entity::link_parent(const Entity& parent) noexcept {
    parent_relative_position_ = subtract(position_, parent.position_);
    parent_linked_ = true;
}

void Entity::sync_parent(const Entity& parent) noexcept {
    if (!parent_linked_) {
        link_parent(parent);
    }
    set_position(add(parent.position_, parent_relative_position_));
}

bool Entity::process(float seconds) noexcept {
    advance_age(seconds);
    return true;
}

void Entity::handle_message(const messaging::MessageInfo& message) noexcept {
    if (cartridge_message(message, 5)) { // SetActive
        active_ = message.parameter1 != 0;
    } else if (activation_message(message)
               || is_activation_message(message.message)) {
        active_ = true;
    } else if (deactivation_message(message)
               || is_deactivation_message(message.message)) {
        active_ = false;
    }
}

void Entity::advance_age(float seconds) noexcept {
    seconds = valid_seconds(seconds);
    if (seconds > 0.0F
        && age_seconds_ <= std::numeric_limits<float>::max() - seconds) {
        age_seconds_ += seconds;
    }
}

bool Entity::is_activation_message(messaging::Message message) noexcept {
    return message == messaging::Message::Activate
        || message == messaging::Message::Open
        || message == messaging::Message::Start;
}

bool Entity::is_deactivation_message(messaging::Message message) noexcept {
    return message == messaging::Message::Deactivate
        || message == messaging::Message::Close
        || message == messaging::Message::Stop;
}

} // namespace fruityprime::entities::static_entities

