// Native counterpart of src/MphRead/Entities/TeleporterEntity.cs.
#include "Entities/static_entities.hpp"
#include "static_entity_helpers.hpp"

namespace fruityprime::entities::static_entities {

using namespace detail;

TeleporterEntity::TeleporterEntity(const scene::EntityInstance& source)
    : Entity(source), data_(data_or_default<scene::TeleporterData>(source)) {
    active_ = data_.active;
}

void TeleporterEntity::trigger() noexcept {
    if (active_ && ready()) {
        cooldown_seconds_ = 0.5F;
    }
}

bool TeleporterEntity::process(float seconds) noexcept {
    (void)Entity::process(seconds);
    cooldown_seconds_ = std::max(0.0F,
                                 cooldown_seconds_ - valid_seconds(seconds));
    return true;
}

} // namespace fruityprime::entities::static_entities

