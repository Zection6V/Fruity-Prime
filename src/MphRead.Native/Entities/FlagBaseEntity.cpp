// Native counterpart of src/MphRead/Entities/FlagBaseEntity.cs.
#include "Entities/static_entities.hpp"
#include "static_entity_helpers.hpp"

namespace fruityprime::entities::static_entities {

using namespace detail;

FlagBaseEntity::FlagBaseEntity(const scene::EntityInstance& source)
    : VolumeEntity(source, data_or_default<scene::FlagBaseData>(source).volume),
      data_(data_or_default<scene::FlagBaseData>(source)),
      team_id_(data_.team_id) {}

void FlagBaseEntity::set_position(scene::VolumePoint value) noexcept {
    VolumeEntity::set_position(value);
}

} // namespace fruityprime::entities::static_entities
