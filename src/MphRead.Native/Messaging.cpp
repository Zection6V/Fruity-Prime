#include "Messaging.hpp"

#include "Entities/EntityBase.hpp"
#include "GameState.hpp"
#include "MemoryArrays.hpp"
#include "Scene.hpp"

#include <any>
#include <cstddef>
#include <cstdint>
#include <utility>

namespace MphRead
{
    namespace
    {
        [[nodiscard]] MessageObject NormalizeMessageObject(MessageObject value) noexcept
        {
            if (value && !value->has_value())
            {
                value.reset();
            }
            return value;
        }

        [[nodiscard]] std::int32_t UnboxInt32(const MessageObject& value)
        {
            if (!value || !value->has_value())
            {
                throw Memory::Detail::NullReferenceException();
            }
            try
            {
                return std::any_cast<std::int32_t>(*value);
            }
            catch (const std::bad_any_cast&)
            {
                throw Memory::Detail::InvalidCastException();
            }
        }

        [[nodiscard]] std::uint8_t GetTriggerBit(std::int32_t index) noexcept
        {
            const std::int32_t remainder = index % 8;
            const std::uint32_t shift = static_cast<std::uint32_t>(remainder) & 0x1FU;
            return static_cast<std::uint8_t>(std::uint32_t{1} << shift);
        }

        template <typename T>
        [[nodiscard]] std::uint8_t GetTriggerStateByte(T& triggerState, std::int32_t index)
        {
            if constexpr (requires { triggerState.Item(index); })
            {
                return triggerState.Item(index);
            }
            else
            {
                if (index < 0
                    || static_cast<std::size_t>(index) >= triggerState.size())
                {
                    throw Memory::Detail::IndexOutOfRangeException();
                }
                return triggerState[static_cast<std::size_t>(index)];
            }
        }

        template <typename T>
        void SetTriggerStateByte(T& triggerState, std::int32_t index, std::uint8_t value)
        {
            if constexpr (requires { triggerState.Item(index, value); })
            {
                triggerState.Item(index, value);
            }
            else
            {
                if (index < 0
                    || static_cast<std::size_t>(index) >= triggerState.size())
                {
                    throw Memory::Detail::IndexOutOfRangeException();
                }
                triggerState[static_cast<std::size_t>(index)] = value;
            }
        }
    }

    MessageInfo::MessageInfo(MphRead::Message message, Entities::EntityBase* sender,
        Entities::EntityBase* target, MessageObject param1, MessageObject param2,
        std::uint64_t executeFrame, std::uint64_t queuedFrame) noexcept
        : Message(message),
          Sender(sender),
          Target(target),
          Param1(NormalizeMessageObject(std::move(param1))),
          Param2(NormalizeMessageObject(std::move(param2))),
          ExecuteFrame(executeFrame),
          QueuedFrame(queuedFrame)
    {
    }

    std::int32_t MessagingDetail::MessageQueueStorage::Count() const noexcept
    {
        return _count;
    }

    const MessageInfo& MessagingDetail::MessageQueueStorage::Item(std::int32_t index) const
    {
        if (index < 0 || index >= _count)
        {
            throw Memory::Detail::ArgumentOutOfRangeException();
        }
        return _items[static_cast<std::size_t>(index)];
    }

    MessageInfo MessagingDetail::MessageQueueStorage::operator[](std::int32_t index) const
    {
        return Item(index);
    }

    void MessagingDetail::MessageQueueStorage::Add(MessageInfo info) noexcept
    {
        _items[static_cast<std::size_t>(_count)] = info;
        ++_count;
    }

    void MessagingDetail::MessageQueueStorage::RemoveAt(std::int32_t index)
    {
        if (index < 0 || index >= _count)
        {
            throw Memory::Detail::ArgumentOutOfRangeException();
        }
        for (std::int32_t i = index; i < _count - 1; ++i)
        {
            _items[static_cast<std::size_t>(i)] = _items[static_cast<std::size_t>(i + 1)];
        }
        --_count;
        _items[static_cast<std::size_t>(_count)] = MessageInfo{};
    }

    void MessagingDetail::MessageQueueStorage::Clear() noexcept
    {
        for (std::int32_t i = 0; i < _count; ++i)
        {
            _items[static_cast<std::size_t>(i)] = MessageInfo{};
        }
        _count = 0;
    }

    const MessageQueueReadOnly& Scene::MessageQueue() const noexcept
    {
        return _queue;
    }

    void Scene::SendMessage(Message message, Entities::EntityBase* sender,
        Entities::EntityBase* target, MessageObject param1, MessageObject param2)
    {
        std::uint64_t frame = _frameCount;
        if (target == nullptr)
        {
            ++frame;
        }
        DispatchOrQueueMessage(message, sender, target,
            std::move(param1), std::move(param2), frame);
    }

    void Scene::SendMessage(Message message, Entities::EntityBase* sender,
        Entities::EntityBase* target, MessageObject param1, MessageObject param2,
        std::int32_t delay)
    {
        if (delay < 0)
        {
            delay = 0;
        }
        DispatchOrQueueMessage(message, sender, target,
            std::move(param1), std::move(param2),
            _frameCount + static_cast<std::uint64_t>(delay));
    }

    void Scene::DispatchOrQueueMessage(Message message, Entities::EntityBase* sender,
        Entities::EntityBase* target, MessageObject param1, MessageObject param2,
        std::uint64_t frame)
    {
        MessageInfo info(message, sender, target, std::move(param1), std::move(param2),
            frame, _frameCount);
        if (frame <= _frameCount)
        {
            DispatchMessage(info);
        }
        else
        {
            QueueMessage(info);
        }
    }

    void Scene::DispatchMessage(MessageInfo info)
    {
        if (info.Message == Message::SetTriggerState)
        {
            const std::int32_t index = UnboxInt32(info.Param1);
            auto& storySave = GameState::StorySave;
            if (!storySave)
            {
                throw Memory::Detail::NullReferenceException();
            }
            const std::int32_t byteIndex = index / 8;
            const std::uint8_t trigger = GetTriggerStateByte(storySave->TriggerState, byteIndex);
            SetTriggerStateByte(storySave->TriggerState, byteIndex,
                static_cast<std::uint8_t>(trigger | GetTriggerBit(index)));
        }
        else if (info.Message == Message::ClearTriggerState)
        {
            const std::int32_t index = UnboxInt32(info.Param1);
            auto& storySave = GameState::StorySave;
            if (!storySave)
            {
                throw Memory::Detail::NullReferenceException();
            }
            const std::int32_t byteIndex = index / 8;
            const std::uint8_t trigger = GetTriggerStateByte(storySave->TriggerState, byteIndex);
            SetTriggerStateByte(storySave->TriggerState, byteIndex,
                static_cast<std::uint8_t>(trigger
                    & static_cast<std::uint8_t>(~GetTriggerBit(index))));
        }
        else if (info.Target != nullptr)
        {
            info.Target->HandleMessage(info);
        }
    }

    void Scene::QueueMessage(MessageInfo info)
    {
        if (_queue.Count() < _queueSize)
        {
            _queue.Add(info);
        }
    }

    void Scene::ProcessMessageQueue()
    {
        for (std::int32_t i = 0; i < _queue.Count(); ++i)
        {
            MessageInfo info = _queue[i];
            if (info.ExecuteFrame <= _frameCount)
            {
                DispatchMessage(info);
                _queue.RemoveAt(i);
                --i;
            }
        }
    }

    void Scene::ClearMessageQueue()
    {
        _queue.Clear();
    }
}
