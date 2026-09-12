#pragma once

#include "Formats/Enums.hpp"

#include <any>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <new>
#include <vector>

namespace MphRead::Entities
{
    class EntityBase;
}

namespace MphRead
{
    class Scene;

    struct MessageInfo
    {
        const MphRead::Message Message = MphRead::Message::None;
        Entities::EntityBase* const Sender = nullptr;
        Entities::EntityBase* const Target = nullptr;
        const std::shared_ptr<std::any> Param1{};
        const std::shared_ptr<std::any> Param2{};
        const std::uint64_t ExecuteFrame = 0;
        const std::uint64_t QueuedFrame = 0;

        MessageInfo() = default;
        MessageInfo(MphRead::Message message, Entities::EntityBase* sender,
            Entities::EntityBase* target, std::shared_ptr<std::any> param1,
            std::shared_ptr<std::any> param2, std::uint64_t executeFrame,
            std::uint64_t queuedFrame) noexcept;

        MessageInfo(const MessageInfo&) noexcept = default;
        MessageInfo& operator=(const MessageInfo& other) noexcept
        {
            if (this != &other)
            {
                this->~MessageInfo();
                ::new (static_cast<void*>(this)) MessageInfo(other);
            }
            return *this;
        }
    };

    class MessageQueueView
    {
    public:
        [[nodiscard]] std::int32_t Count() const noexcept;
        [[nodiscard]] MessageInfo operator[](std::int32_t index) const;

    private:
        friend class Scene;

        explicit MessageQueueView(const std::shared_ptr<std::vector<MessageInfo>>& queue) noexcept
            : _queue(queue)
        {
        }

        std::shared_ptr<const std::vector<MessageInfo>> _queue;
    };
}

#define MPHREAD_SCENE_MESSAGING_MEMBERS \
private: \
    static constexpr std::int32_t _queueSize = 40; \
    std::shared_ptr<std::vector<::MphRead::MessageInfo>> _queue = []() \
    { \
        auto queue = std::make_shared<std::vector<::MphRead::MessageInfo>>(); \
        queue->reserve(static_cast<std::size_t>(_queueSize)); \
        return queue; \
    }(); \
public: \
    [[nodiscard]] ::MphRead::MessageQueueView MessageQueue() const noexcept; \
    void SendMessage(::MphRead::Message message, ::MphRead::Entities::EntityBase* sender, \
        ::MphRead::Entities::EntityBase* target, std::shared_ptr<std::any> param1, \
        std::shared_ptr<std::any> param2); \
    void SendMessage(::MphRead::Message message, ::MphRead::Entities::EntityBase* sender, \
        ::MphRead::Entities::EntityBase* target, std::shared_ptr<std::any> param1, \
        std::shared_ptr<std::any> param2, std::int32_t delay); \
private: \
    void DispatchOrQueueMessage(::MphRead::Message message, \
        ::MphRead::Entities::EntityBase* sender, ::MphRead::Entities::EntityBase* target, \
        std::shared_ptr<std::any> param1, std::shared_ptr<std::any> param2, \
        std::uint64_t frame); \
    void DispatchMessage(::MphRead::MessageInfo info); \
    void QueueMessage(::MphRead::MessageInfo info); \
    void ProcessMessageQueue(); \
public: \
    void ClearMessageQueue();
