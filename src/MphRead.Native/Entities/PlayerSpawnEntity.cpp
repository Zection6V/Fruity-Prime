// Native counterpart of src/MphRead/Entities/PlayerSpawnEntity.cs.
#include "Entities/static_entities.hpp"
#include "static_entity_helpers.hpp"

namespace fruityprime::entities::static_entities {

using namespace detail;

PlayerSpawnEntity::PlayerSpawnEntity(const scene::EntityInstance& source)
    : Entity(source), data_(data_or_default<scene::PlayerSpawnData>(source)) {
    active_ = data_.active;
}

bool PlayerSpawnEntity::process(float seconds) noexcept {
    (void)seconds;
    advance_age(seconds);
    if (cooldown_ticks_ > 0) {
        --cooldown_ticks_;
    }
    return true;
}

void PlayerSpawnEntity::handle_message(
    const messaging::MessageInfo& message) noexcept {
    if (cartridge_message(message, 5)) { // SetActive
        active_ = message.parameter1 != 0;
        return;
    }
    if (activation_message(message)) {
        active_ = true;
        return;
    }
    if (deactivation_message(message)) {
        active_ = false;
        return;
    }
    Entity::handle_message(message);
}

} // namespace fruityprime::entities::static_entities

