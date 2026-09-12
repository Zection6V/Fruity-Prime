#pragma once

#include "Formats/Enums.hpp"

#include <any>
#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <new>

namespace MphRead::Entities
{
    class EntityBase;
}

namespace MphRead
{
    class Scene;

    using MessageObject = std::shared_ptr<const std::any>;

    struct MessageInfo
    {
        const MphRead::Message Message = MphRead::Message::None;
        Entities::EntityBase* const Sender = nullptr;
        Entities::EntityBase* const Target = nullptr;
        const MessageObject Param1{};
        const MessageObject Param2{};
        const std::uint64_t ExecuteFrame = 0;
        const std::uint64_t QueuedFrame = 0;

        MessageInfo() = default;
        MessageInfo(MphRead::Message message, Entities::EntityBase* sender,
            Entities::EntityBase* target, MessageObject param1, MessageObject param2,
            std::uint64_t executeFrame, std::uint64_t queuedFrame) noexcept;

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

    class MessageQueueReadOnly
    {
    public:
        MessageQueueReadOnly() = default;
        MessageQueueReadOnly(const MessageQueueReadOnly&) = delete;
        MessageQueueReadOnly(MessageQueueReadOnly&&) = delete;
        MessageQueueReadOnly& operator=(const MessageQueueReadOnly&) = delete;
        MessageQueueReadOnly& operator=(MessageQueueReadOnly&&) = delete;
        virtual ~MessageQueueReadOnly() = default;

        [[nodiscard]] virtual std::int32_t Count() const noexcept = 0;
        [[nodiscard]] virtual MessageInfo operator[](std::int32_t index) const = 0;
    };

    namespace MessagingDetail
    {
        class MessageQueueStorage final : public MessageQueueReadOnly
        {
        public:
            [[nodiscard]] std::int32_t Count() const noexcept override;
            [[nodiscard]] MessageInfo operator[](std::int32_t index) const override;

        private:
            friend class ::MphRead::Scene;

            static constexpr std::size_t Capacity = 40;

            void Add(MessageInfo info) noexcept;
            void RemoveAt(std::int32_t index);
            void Clear() noexcept;

            [[nodiscard]] const MessageInfo& Item(std::int32_t index) const;

            std::array<MessageInfo, Capacity> _items{};
            std::int32_t _count = 0;
        };
    }
}

#define MPHREAD_SCENE_MESSAGING_MEMBERS \
private: \
    static constexpr std::int32_t _queueSize = 40; \
    ::MphRead::MessagingDetail::MessageQueueStorage _queue{}; \
public: \
    [[nodiscard]] const ::MphRead::MessageQueueReadOnly& MessageQueue() const noexcept; \
    void SendMessage(::MphRead::Message message, ::MphRead::Entities::EntityBase* sender, \
        ::MphRead::Entities::EntityBase* target, ::MphRead::MessageObject param1, \
        ::MphRead::MessageObject param2); \
    void SendMessage(::MphRead::Message message, ::MphRead::Entities::EntityBase* sender, \
        ::MphRead::Entities::EntityBase* target, ::MphRead::MessageObject param1, \
        ::MphRead::MessageObject param2, std::int32_t delay); \
private: \
    void DispatchOrQueueMessage(::MphRead::Message message, \
        ::MphRead::Entities::EntityBase* sender, ::MphRead::Entities::EntityBase* target, \
        ::MphRead::MessageObject param1, ::MphRead::MessageObject param2, \
        std::uint64_t frame); \
    void DispatchMessage(::MphRead::MessageInfo info); \
    void QueueMessage(::MphRead::MessageInfo info); \
    void ProcessMessageQueue(); \
public: \
    void ClearMessageQueue();
