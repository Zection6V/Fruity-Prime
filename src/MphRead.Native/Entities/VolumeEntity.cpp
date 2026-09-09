// Native counterpart of src/MphRead/Entities/VolumeEntity.cs.
#include "Entities/static_entities.hpp"
#include "static_entity_helpers.hpp"

namespace fruityprime::entities::static_entities {

using namespace detail;

VolumeEntity::VolumeEntity(const scene::EntityInstance& source,
                           const scene::EntityVolume& volume)
    : Entity(source), volume_(volume) {}

void VolumeEntity::set_position(scene::VolumePoint value) noexcept {
    const scene::VolumePoint delta = subtract(value, position_);
    Entity::set_position(value);
    translate_volume(volume_, delta);
}

bool VolumeEntity::process(float seconds) noexcept {
    return Entity::process(seconds);
}

void VolumeEntity::handle_message(
    const messaging::MessageInfo& message) noexcept {
    Entity::handle_message(message);
}

} // namespace fruityprime::entities::static_entities

