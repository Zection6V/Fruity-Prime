// Native counterpart of src/MphRead/Entities/ArtifactEntity.cs.
#include "Entities/static_entities.hpp"
#include "static_entity_helpers.hpp"

#include <array>

namespace fruityprime::entities::static_entities {

using namespace detail;

ArtifactEntity::ArtifactEntity(const scene::EntityInstance& source)
    : Entity(source), data_(data_or_default<scene::ArtifactData>(source)) {
    active_ = data_.active;
}

void ArtifactEntity::collect() noexcept {
    collected_ = true;
    active_ = false;
}

bool ArtifactEntity::process(float seconds) noexcept {
    static_cast<void>(Entity::process(seconds));
    if (!active_) {
        scan_id_ = 0;
        return true;
    }
    static constexpr std::array<std::uint16_t, 32> scan_ids{
        48, 48, 48, 48, 48, 48, 48, 48, 40, 41, 42, 40, 41, 42,
        40, 41, 42, 40, 41, 42, 40, 41, 42, 40, 41, 42, 40, 41,
        42, 40, 41, 42};
    const std::size_t index = data_.model_id >= 8
        ? static_cast<std::size_t>(data_.artifact_id)
        : static_cast<std::size_t>(data_.model_id) * 3u + 8u
            + data_.artifact_id;
    scan_id_ = index < scan_ids.size() ? scan_ids[index] : 0;
    return true;
}

void ArtifactEntity::handle_message(
    const messaging::MessageInfo& message) noexcept {
    if (message.message == messaging::Message::Deactivate
        || message.message == messaging::Message::Close) {
        active_ = false;
        return;
    }
    Entity::handle_message(message);
}

} // namespace fruityprime::entities::static_entities
