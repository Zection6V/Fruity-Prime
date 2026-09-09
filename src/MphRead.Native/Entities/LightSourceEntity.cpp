// Native counterpart of src/MphRead/Entities/LightSourceEntity.cs.
#include "Entities/static_entities.hpp"
#include "static_entity_helpers.hpp"

namespace fruityprime::entities::static_entities {

using namespace detail;

LightSourceEntity::LightSourceEntity(const scene::EntityInstance& source)
    : VolumeEntity(source,
                   data_or_default<scene::LightSourceData>(source).volume),
      data_(data_or_default<scene::LightSourceData>(source)) {}

void LightSourceEntity::set_position(scene::VolumePoint value) noexcept {
    VolumeEntity::set_position(value);
}

} // namespace fruityprime::entities::static_entities

