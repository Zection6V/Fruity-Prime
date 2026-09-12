#pragma once

#include "Formats/Enums.hpp"

#include <any>
#include <cstddef>
#include <cstdint>
#include <new>
#include <vector>

namespace MphRead::Entities
{
    class EntityBase;
}

namespace MphRead
{
    struct MessageInfo
    {
        const MphRead::Message Message = MphRead::Message::None;
        Entities::EntityBase* const Sender = nullptr;
        Entities::EntityBase* const Target = nullptr;
        const std::any Param1{};
        const std::any Param2{};
        const std::uint64_t ExecuteFrame = 0;
        const std::uint64_t QueuedFrame = 0;

        MessageInfo() = default;
        MessageInfo(MphRead::Message message, Entities::EntityBase* sender,
            Entities::EntityBase* target, std::any param1, std::any param2,
            std::uint64_t executeFrame, std::uint64_t queuedFrame);

        MessageInfo(const MessageInfo&) = default;
        MessageInfo& operator=(const MessageInfo& other)
        {
            if (this != &other)
            {
                this->~MessageInfo();
                ::new (static_cast<void*>(this)) MessageInfo(other);
            }
            return *this;
        }
    };
}

#define MPHREAD_SCENE_MESSAGING_MEMBERS \
private: \
    static constexpr std::int32_t _queueSize = 40; \
    std::vector<::MphRead::MessageInfo> _queue = []() \
    { \
        std::vector<::MphRead::MessageInfo> queue; \
        queue.reserve(static_cast<std::size_t>(_queueSize)); \
        return queue; \
    }(); \
public: \
    [[nodiscard]] const std::vector<::MphRead::MessageInfo>& MessageQueue() const noexcept; \
    void SendMessage(::MphRead::Message message, ::MphRead::Entities::EntityBase* sender, \
        ::MphRead::Entities::EntityBase* target, std::any param1, std::any param2); \
    void SendMessage(::MphRead::Message message, ::MphRead::Entities::EntityBase* sender, \
        ::MphRead::Entities::EntityBase* target, std::any param1, std::any param2, \
        std::int32_t delay); \
private: \
    void DispatchOrQueueMessage(::MphRead::Message message, \
        ::MphRead::Entities::EntityBase* sender, ::MphRead::Entities::EntityBase* target, \
        std::any param1, std::any param2, std::uint64_t frame); \
    void DispatchMessage(::MphRead::MessageInfo info); \
    void QueueMessage(::MphRead::MessageInfo info); \
    void ProcessMessageQueue(); \
public: \
    void ClearMessageQueue();
