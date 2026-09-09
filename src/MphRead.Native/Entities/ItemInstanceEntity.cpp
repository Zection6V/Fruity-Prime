// Native counterpart of src/MphRead/Entities/ItemInstanceEntity.cs.
#include "Entities/runtime_entities.hpp"
#include "runtime_entity_helpers.hpp"

namespace fruityprime::runtime {

using namespace detail;

ItemInstanceEntity::ItemInstanceEntity(std::uint32_t id,
                                       std::int32_t item_type,
                                       net::Vec3 position) noexcept
    : Entity(id, Kind::ItemInstance, position), item_type_(item_type) {}

void ItemInstanceEntity::on_picked_up() noexcept {
    picked_up_ = true;
    active_ = false;
}

void ItemInstanceEntity::handle_message(
    const messaging::MessageInfo& message) noexcept {
    if (cartridge_message(message, 5)) { // SetActive
        active_ = message.parameter1 != 0;
        if (active_) {
            picked_up_ = false;
        }
        return;
    }
    if (cartridge_message(message, 6)) { // Destroyed
        active_ = false;
        return;
    }
    Entity::handle_message(message);
}

bool ItemInstanceEntity::process(float seconds) noexcept {
    if (!active_) {
        return false;
    }
    seconds = finite_seconds(seconds);
    advance_age(seconds);
    spin_degrees_ = std::fmod(spin_degrees_ + seconds * 126.0F, 360.0F);
    if (spin_degrees_ < 0.0F) {
        spin_degrees_ += 360.0F;
    }
    float_offset_ = std::sin(spin_degrees_ * 0.01745329251994329577F) * 0.125F;
    if (despawn_seconds_ >= 0.0F) {
        despawn_seconds_ -= seconds;
        if (despawn_seconds_ <= 0.0F) {
            despawn_seconds_ = 0.0F;
            active_ = false;
        }
    }
    return active_;
}

std::uint16_t SpinningEntityBase::next_item_rotation_ = 0;

SpinningEntityBase::SpinningEntityBase(
    std::uint32_t id, Kind kind, net::Vec3 position, float spin_speed,
    net::Vec3 spin_axis, int spin_model_index, int float_model_index) noexcept
    : Entity(id, kind, position),
      spin_(static_cast<float>(next_item_rotation_) / 65536.0F * 360.0F),
      spin_speed_(spin_speed), spin_axis_(spin_axis),
      spin_model_index_(spin_model_index),
      float_model_index_(float_model_index) {
    next_item_rotation_ = static_cast<std::uint16_t>(
        next_item_rotation_ + 0x2000u);
}

bool SpinningEntityBase::process(float seconds) noexcept {
    seconds = finite_seconds(seconds);
    advance_age(seconds);
    spin_ = std::fmod(spin_ + seconds * 360.0F * spin_speed_, 360.0F);
    if (spin_ < 0.0F) {
        spin_ += 360.0F;
    }
    return active_;
}

FhItemEntity::FhItemEntity(
    std::uint32_t id, const FhItemInstanceEntityData& data) noexcept
    : SpinningEntityBase(id, Kind::ItemInstance,
                         {data.position.x, data.position.y + 0.5F,
                          data.position.z},
                         0.35F, {0.0F, 1.0F, 0.0F}, 0, 0),
      item_type_(data.item_type) {}

} // namespace fruityprime::runtime
