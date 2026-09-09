// Native counterpart of src/MphRead/Entities/ForceFieldEntity.cs.
#include "Entities/static_entities.hpp"
#include "static_entity_helpers.hpp"

#include <array>
#include <cmath>

namespace fruityprime::entities::static_entities {

using namespace detail;

ForceFieldEntity::ForceFieldEntity(const scene::EntityInstance& source)
    : Entity(source), data_(data_or_default<scene::ForceFieldData>(source)) {
    active_ = data_.active;
    const scene::VolumePoint cross{
        up_.y * facing_.z - up_.z * facing_.y,
        up_.z * facing_.x - up_.x * facing_.z,
        up_.x * facing_.y - up_.y * facing_.x};
    const float magnitude = length(cross);
    if (magnitude > 0.0F) {
        right_ = multiply(cross, 1.0F / magnitude);
    }
    plane_distance_ = facing_.x * position_.x + facing_.y * position_.y
        + facing_.z * position_.z;
    if (!active_) {
        alpha_ = 0.0F;
        scan_id_ = 0;
    } else {
        refresh_scan_id();
    }
}

void ForceFieldEntity::refresh_scan_id() noexcept {
    static constexpr std::array<std::uint16_t, 10> scan_ids{
        0, 294, 295, 291, 290, 292, 293, 296, 0, 267};
    scan_id_ = data_.type < scan_ids.size() ? scan_ids[data_.type] : 0;
}

bool ForceFieldEntity::process(float seconds) noexcept {
    static_cast<void>(Entity::process(seconds));
    constexpr float alpha_step = 1.0F / 31.0F / 2.0F;
    set_alpha(alpha_ + (active_ ? alpha_step : -alpha_step));
    return true;
}

void ForceFieldEntity::handle_message(
    const messaging::MessageInfo& message) noexcept {
    if (cartridge_message(message, 16)) { // Unlock
        active_ = false;
        scan_id_ = 0;
        return;
    }
    if (cartridge_message(message, 17)) { // Lock
        active_ = true;
        // Type 9 is deliberately scan-invisible when relocked, matching the
        // managed branch rather than its constructor's initial scan value.
        if (data_.type == 9) {
            scan_id_ = 0;
        } else {
            refresh_scan_id();
        }
        return;
    }
    Entity::handle_message(message);
}

} // namespace fruityprime::entities::static_entities
