// Native counterpart of src/MphRead/Entities/MorphCameraEntity.cs.
#include "Entities/static_entities.hpp"
#include "static_entity_helpers.hpp"

namespace fruityprime::entities::static_entities {

using namespace detail;

MorphCameraEntity::MorphCameraEntity(const scene::EntityInstance& source)
    : VolumeEntity(source,
                   data_or_default<scene::MorphCameraData>(source).volume) {}

void MorphCameraEntity::set_position(scene::VolumePoint value) noexcept {
    VolumeEntity::set_position(value);
}

FhMorphCameraEntity::FhMorphCameraEntity(
    const scene::EntityInstance& source)
    : VolumeEntity(source, first_hunt_volume(
          data_or_default<scene::FhMorphCameraData>(source).volume,
          {source.position.x.to_float(), source.position.y.to_float(),
           source.position.z.to_float()})),
      data_(data_or_default<scene::FhMorphCameraData>(source)) {}

} // namespace fruityprime::entities::static_entities
