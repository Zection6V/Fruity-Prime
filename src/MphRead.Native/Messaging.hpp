#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <vector>

namespace fruityprime::messaging {

enum class Message : std::uint32_t {
    None = 0,
    Activate = 1,
    Deactivate = 2,
    SetTriggerState = 3,
    ClearTriggerState = 4,
    Open = 5,
    Close = 6,
    Start = 7,
    Stop = 8
};

struct MessageInfo {
    Message message = Message::None;
    // The managed cartridge message enum is broader than the small native
    // runtime queue enum.  Preserve the original numeric value when a
    // format-driven entity (for example CamSeq) emits it.
    std::uint32_t cartridge_message = 0;
    std::int32_t sender = -1;
    std::int32_t target = -1;
    std::int16_t target_type = -1;
    std::int32_t parameter1 = 0;
    std::int32_t parameter2 = 0;
    std::uint64_t execute_frame = 0;
    std::uint64_t queued_frame = 0;
};

class Queue {
public:
    static constexpr std::size_t Capacity = 40;

    // Returns false when the managed queue's fixed capacity is full.
    [[nodiscard]] bool post(MessageInfo info) noexcept;
    // Corresponds to Scene.SendMessage without a delay argument. A broadcast
    // is scheduled for the next frame; a targeted message is due this frame.
    [[nodiscard]] bool send(Message message, std::int32_t sender,
                             std::int32_t target, std::int32_t parameter1,
                             std::int32_t parameter2, std::uint64_t frame,
                             std::int16_t target_type = -1,
                             std::uint32_t cartridge_message = 0) noexcept;
    // Corresponds to Scene.SendMessage with its explicit delay argument.
    // Unlike the overload above, delay == 0 never adds the broadcast frame.
    [[nodiscard]] bool send_delayed(
        Message message, std::int32_t sender, std::int32_t target,
        std::int32_t parameter1, std::int32_t parameter2,
        std::uint64_t frame, int delay,
        std::int16_t target_type = -1,
        std::uint32_t cartridge_message = 0) noexcept;
    [[nodiscard]] std::size_t dispatch_due(
        std::uint64_t frame,
        const std::function<void(const MessageInfo&)>& dispatch);
    void clear() noexcept { queue_.clear(); }
    [[nodiscard]] std::size_t size() const noexcept { return queue_.size(); }
    [[nodiscard]] const std::vector<MessageInfo>& entries() const noexcept {
        return queue_;
    }

private:
    std::vector<MessageInfo> queue_;
};

} // namespace fruityprime::messaging

namespace MphReadNative {
namespace Messaging = ::fruityprime::messaging;
using MessageInfo = ::fruityprime::messaging::MessageInfo;
}
