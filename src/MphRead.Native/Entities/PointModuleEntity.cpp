// Native counterpart of src/MphRead/Entities/PointModuleEntity.cs.
#include "Entities/static_entities.hpp"
#include "static_entity_helpers.hpp"

namespace fruityprime::entities::static_entities {

using namespace detail;

PointModuleEntity::PointModuleEntity(const scene::EntityInstance& source)
    : Entity(source), data_(data_or_default<scene::PointModuleData>(source)) {
    // The managed constructor always hides all point-module models until the
    // five-module current chain is selected; the authored Active field is not
    // consulted here.
    active_ = false;
}

bool PointModuleEntity::process(float seconds) noexcept {
    if (world_ != nullptr && world_->current_point_module() == nullptr
        && id_ == StartId) {
        set_current();
    }
    return Entity::process(seconds);
}

void PointModuleEntity::set_current() noexcept {
    if (world_ != nullptr) {
        world_->set_current(this);
    }
}

} // namespace fruityprime::entities::static_entities
