#include "Messaging.hpp"

#include <algorithm>

namespace fruityprime::messaging {

bool Queue::post(MessageInfo info) noexcept {
    if (queue_.size() >= Capacity) {
        return false;
    }
    queue_.push_back(info);
    return true;
}

bool Queue::send(Message message, std::int32_t sender, std::int32_t target,
                 std::int32_t parameter1, std::int32_t parameter2,
                 std::uint64_t frame, std::int16_t target_type,
                 std::uint32_t cartridge_message) noexcept {
    MessageInfo info;
    info.message = message;
    info.cartridge_message = cartridge_message;
    info.sender = sender;
    info.target = target;
    info.target_type = target_type;
    info.parameter1 = parameter1;
    info.parameter2 = parameter2;
    info.queued_frame = frame;
    info.execute_frame = frame + (target < 0 ? 1u : 0u);
    return post(info);
}

bool Queue::send_delayed(Message message, std::int32_t sender,
                         std::int32_t target, std::int32_t parameter1,
                         std::int32_t parameter2, std::uint64_t frame,
                         int delay,
                         std::int16_t target_type,
                         std::uint32_t cartridge_message) noexcept {
    if (delay < 0) {
        delay = 0;
    }
    MessageInfo info;
    info.message = message;
    info.cartridge_message = cartridge_message;
    info.sender = sender;
    info.target = target;
    info.target_type = target_type;
    info.parameter1 = parameter1;
    info.parameter2 = parameter2;
    info.queued_frame = frame;
    info.execute_frame = frame + static_cast<std::uint64_t>(delay);
    return post(info);
}

std::size_t Queue::dispatch_due(
    std::uint64_t frame,
    const std::function<void(const MessageInfo&)>& dispatch) {
    std::size_t dispatched = 0;
    for (std::size_t index = 0; index < queue_.size();) {
        if (queue_[index].execute_frame > frame) {
            ++index;
            continue;
        }
        const MessageInfo info = queue_[index];
        queue_.erase(queue_.begin() + static_cast<std::ptrdiff_t>(index));
        if (dispatch) {
            dispatch(info);
        }
        ++dispatched;
    }
    return dispatched;
}

} // namespace fruityprime::messaging
