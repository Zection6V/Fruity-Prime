// Native counterpart of src/MphRead/Entities/TriggerVolumeEntity.cs.
#include "Entities/static_entities.hpp"
#include "static_entity_helpers.hpp"

namespace fruityprime::entities::static_entities {

using namespace detail;

TriggerVolumeEntity::TriggerVolumeEntity(const scene::EntityInstance& source)
    : VolumeEntity(source,
                   data_or_default<scene::TriggerVolumeData>(source).volume),
      data_(data_or_default<scene::TriggerVolumeData>(source)) {
    active_ = data_.active || data_.always_active;
}

void TriggerVolumeEntity::set_position(scene::VolumePoint value) noexcept {
    VolumeEntity::set_position(value);
    data_.volume = volume_;
}

FhTriggerVolumeEntity::FhTriggerVolumeEntity(
    const scene::EntityInstance& source)
    : VolumeEntity(source, first_hunt_volume(
          first_hunt_volume_for(
              data_or_default<scene::FhTriggerVolumeData>(source).subtype,
              data_or_default<scene::FhTriggerVolumeData>(source).box,
              data_or_default<scene::FhTriggerVolumeData>(source).sphere,
              data_or_default<scene::FhTriggerVolumeData>(source).cylinder),
          {source.position.x.to_float(), source.position.y.to_float(),
           source.position.z.to_float()})),
      data_(data_or_default<scene::FhTriggerVolumeData>(source)) {}

} // namespace fruityprime::entities::static_entities
