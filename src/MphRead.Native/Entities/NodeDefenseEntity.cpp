// Native counterpart of src/MphRead/Entities/NodeDefenseEntity.cs.
#include "Entities/static_entities.hpp"
#include "static_entity_helpers.hpp"

namespace fruityprime::entities::static_entities {

using namespace detail;

NodeDefenseEntity::NodeDefenseEntity(const scene::EntityInstance& source)
    : VolumeEntity(source,
                   data_or_default<scene::NodeDefenseData>(source).volume) {}

void NodeDefenseEntity::set_position(scene::VolumePoint value) noexcept {
    VolumeEntity::set_position(value);
}

} // namespace fruityprime::entities::static_entities

