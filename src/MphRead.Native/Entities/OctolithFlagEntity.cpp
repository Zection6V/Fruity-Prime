// Native counterpart of src/MphRead/Entities/OctolithFlagEntity.cs.
#include "Entities/static_entities.hpp"
#include "static_entity_helpers.hpp"

namespace fruityprime::entities::static_entities {

using namespace detail;

OctolithFlagEntity::OctolithFlagEntity(
    const scene::EntityInstance& source)
    : Entity(source),
      team_id_(data_or_default<scene::OctolithFlagData>(source).team_id) {}

} // namespace fruityprime::entities::static_entities

