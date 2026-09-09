// Native counterpart of src/MphRead/Entities/PlatformEntity.cs.
#include "Entities/static_entities.hpp"
#include "static_entity_helpers.hpp"

namespace fruityprime::entities::static_entities {

using namespace detail;

PlatformEntity::PlatformEntity(const scene::EntityInstance& source)
    : Entity(source), data_(data_or_default<scene::PlatformData>(source)) {
    active_ = data_.active;
    moving_ = active_;
    if (data_.position_count > 0) {
        path_index_ = 0;
        position_ = data_.positions[0];
    }
}

void PlatformEntity::set_path_position(std::size_t index) noexcept {
    const std::size_t count = std::min<std::size_t>(
        data_.position_count, data_.positions.size());
    if (count == 0) {
        return;
    }
    path_index_ = std::min(index, count - 1);
    path_progress_ = 0.0F;
    position_ = data_.positions[path_index_];
}

bool PlatformEntity::process(float seconds) noexcept {
    if (!active_) {
        return true;
    }
    advance_age(seconds);
    seconds = valid_seconds(seconds);
    const std::size_t count = std::min<std::size_t>(
        data_.position_count, data_.positions.size());
    if (!moving_ || seconds == 0.0F || count < 2) {
        return true;
    }

    const float speed = std::max(
        0.01F, reverse_ ? data_.backward_speed : data_.forward_speed);
    float remaining = seconds;
    while (remaining > 0.0F) {
        const std::size_t next_index = reverse_
            ? (path_index_ == 0 ? count - 1 : path_index_ - 1)
            : (path_index_ + 1 >= count ? 0 : path_index_ + 1);
        const scene::VolumePoint delta = subtract(
            data_.positions[next_index], data_.positions[path_index_]);
        const float distance = length(delta);
        if (distance <= 0.0001F) {
            path_index_ = next_index;
            path_progress_ = 0.0F;
            remaining = 0.0F;
            break;
        }
        const float distance_left = distance * (1.0F - path_progress_);
        const float distance_step = speed * remaining;
        if (distance_step < distance_left) {
            path_progress_ += distance_step / distance;
            remaining = 0.0F;
            break;
        }
        remaining = std::max(0.0F, remaining - distance_left / speed);
        path_index_ = next_index;
        path_progress_ = 0.0F;
        if (path_index_ + 1 >= count && !reverse_
            && data_.reverse_type != 0) {
            reverse_ = true;
        } else if (path_index_ == 0 && reverse_) {
            reverse_ = false;
        }
    }
    const std::size_t target_index = reverse_
        ? (path_index_ == 0 ? count - 1 : path_index_ - 1)
        : (path_index_ + 1 >= count ? 0 : path_index_ + 1);
    position_ = add(data_.positions[path_index_], multiply(
        subtract(data_.positions[target_index], data_.positions[path_index_]),
        path_progress_));
    return true;
}

void PlatformEntity::handle_message(
    const messaging::MessageInfo& message) noexcept {
    Entity::handle_message(message);
    if (activation_message(message) || is_activation_message(message.message)) {
        moving_ = true;
    } else if (deactivation_message(message)
               || is_deactivation_message(message.message)) {
        moving_ = false;
    }
}

FhPlatformEntity::FhPlatformEntity(const scene::EntityInstance& source)
    : Entity(source), data_(data_or_default<scene::FhPlatformData>(source)),
      delay_(static_cast<std::uint32_t>(data_.delay) * 2u),
      move_timer_(delay_) {}

void FhPlatformEntity::update_movement() noexcept {
    const std::size_t count = std::min<std::size_t>(
        data_.position_count, data_.positions.size());
    if (from_index_ >= count || to_index_ >= count) {
        velocity_ = {};
        move_timer_ = 0;
        return;
    }
    const auto delta = subtract(data_.positions[to_index_],
                                data_.positions[from_index_]);
    const float distance = length(delta);
    const float speed = data_.speed / 2.0F;
    if (distance <= 0.0F || speed <= 0.0F) {
        velocity_ = {};
        move_timer_ = 0;
        return;
    }
    move_timer_ = static_cast<std::uint32_t>(distance / speed);
    velocity_ = multiply(delta, speed / distance);
}

bool FhPlatformEntity::process(float seconds) noexcept {
    static_cast<void>(seconds);
    position_ = add(position_, velocity_);
    const std::size_t count = std::min<std::size_t>(
        data_.position_count, data_.positions.size());
    if (count < 2) {
        return true;
    }
    if (move_timer_ > 0) {
        --move_timer_;
        return true;
    }
    if (state_ == MoveState::Sleep) {
        state_ = MoveState::MoveForward;
        update_movement();
        ++from_index_;
    } else if (state_ == MoveState::MoveForward) {
        state_ = MoveState::Wait;
        velocity_ = {};
        position_ = data_.positions[to_index_];
        if (from_index_ == count - 1) {
            to_index_ = from_index_ - 1;
        } else {
            ++from_index_;
            to_index_ = from_index_ + 1;
        }
        move_timer_ = delay_;
    } else if (state_ == MoveState::Wait) {
        state_ = to_index_ >= from_index_ ? MoveState::MoveForward
                                         : MoveState::MoveBackward;
        update_movement();
    } else {
        velocity_ = {};
        position_ = data_.positions[to_index_];
        if (to_index_ > 0) {
            state_ = MoveState::Wait;
            --from_index_;
            to_index_ = from_index_ - 1;
        } else {
            state_ = MoveState::Sleep;
            from_index_ = 0;
            to_index_ = 1;
        }
        move_timer_ = delay_;
    }
    return true;
}

} // namespace fruityprime::entities::static_entities
