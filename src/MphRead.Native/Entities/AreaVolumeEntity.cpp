// Native counterpart of src/MphRead/Entities/AreaVolumeEntity.cs.
#include "Entities/static_entities.hpp"
#include "static_entity_helpers.hpp"

namespace fruityprime::entities::static_entities {

using namespace detail;

AreaVolumeEntity::AreaVolumeEntity(const scene::EntityInstance& source)
    : VolumeEntity(source, data_or_default<scene::AreaVolumeData>(source).volume),
      data_(data_or_default<scene::AreaVolumeData>(source)) {
    active_ = data_.active || data_.always_active;
}

void AreaVolumeEntity::set_position(scene::VolumePoint value) noexcept {
    VolumeEntity::set_position(value);
    data_.volume = volume_;
}

FhAreaVolumeEntity::FhAreaVolumeEntity(const scene::EntityInstance& source)
    : VolumeEntity(source, first_hunt_volume(
          first_hunt_volume_for(
              data_or_default<scene::FhAreaVolumeData>(source).subtype,
              data_or_default<scene::FhAreaVolumeData>(source).box,
              data_or_default<scene::FhAreaVolumeData>(source).sphere,
              data_or_default<scene::FhAreaVolumeData>(source).cylinder),
          {source.position.x.to_float(), source.position.y.to_float(),
           source.position.z.to_float()})),
      data_(data_or_default<scene::FhAreaVolumeData>(source)) {}

} // namespace fruityprime::entities::static_entities
