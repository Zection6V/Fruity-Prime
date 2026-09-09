// Native counterpart of src/MphRead/Entities/JumpPadEntity.cs.
#include "Entities/static_entities.hpp"
#include "static_entity_helpers.hpp"

namespace fruityprime::entities::static_entities {

using namespace detail;

JumpPadEntity::JumpPadEntity(const scene::EntityInstance& source)
    : VolumeEntity(source, data_or_default<scene::JumpPadData>(source).volume),
      data_(data_or_default<scene::JumpPadData>(source)) {
    active_ = data_.active;
}

void JumpPadEntity::set_position(scene::VolumePoint value) noexcept {
    VolumeEntity::set_position(value);
    data_.volume = volume_;
}

void JumpPadEntity::trigger() noexcept {
    if (active_) {
        cooldown_ticks_ = data_.cooldown_time;
    }
}

bool JumpPadEntity::process(float seconds) noexcept {
    (void)Entity::process(seconds);
    if (cooldown_ticks_ > 0) {
        --cooldown_ticks_;
    }
    return true;
}

FhJumpPadEntity::FhJumpPadEntity(const scene::EntityInstance& source)
    : VolumeEntity(source, first_hunt_volume(
          first_hunt_volume_for(
              data_or_default<scene::FhJumpPadData>(source).volume_type,
              data_or_default<scene::FhJumpPadData>(source).box,
              data_or_default<scene::FhJumpPadData>(source).sphere,
              data_or_default<scene::FhJumpPadData>(source).cylinder),
          {source.position.x.to_float(), source.position.y.to_float(),
           source.position.z.to_float()})),
      data_(data_or_default<scene::FhJumpPadData>(source)) {}

} // namespace fruityprime::entities::static_entities
