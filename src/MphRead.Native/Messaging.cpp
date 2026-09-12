#include "Messaging.hpp"

#include "Entities/EntityBase.hpp"
#include "Formats/Types.hpp"
#include "GameState.hpp"
#include "Scene.hpp"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <utility>

namespace MphRead
{
    namespace
    {
        [[nodiscard]] std::int32_t UnboxInt32(const std::shared_ptr<std::any>& value)
        {
            if (!value)
            {
                throw System::NullReferenceException();
            }
            return std::any_cast<std::int32_t>(*value);
        }

        [[nodiscard]] std::uint8_t GetTriggerBit(std::int32_t index) noexcept
        {
            const std::int32_t remainder = index % 8;
            const std::uint32_t shift = static_cast<std::uint32_t>(remainder) & 0x1FU;
            return static_cast<std::uint8_t>(std::uint32_t{1} << shift);
        }
    }

    MessageInfo::MessageInfo(MphRead::Message message, Entities::EntityBase* sender,
        Entities::EntityBase* target, std::shared_ptr<std::any> param1,
        std::shared_ptr<std::any> param2, std::uint64_t executeFrame,
        std::uint64_t queuedFrame) noexcept
        : Message(message),
          Sender(sender),
          Target(target),
          Param1(std::move(param1)),
          Param2(std::move(param2)),
          ExecuteFrame(executeFrame),
          QueuedFrame(queuedFrame)
    {
    }

    std::int32_t MessageQueueView::Count() const noexcept
    {
        return static_cast<std::int32_t>(_queue->size());
    }

    MessageInfo MessageQueueView::operator[](std::int32_t index) const
    {
        return _queue->at(static_cast<std::size_t>(index));
    }

    MessageQueueView Scene::MessageQueue() const noexcept
    {
        return MessageQueueView(_queue);
    }

    void Scene::SendMessage(Message message, Entities::EntityBase* sender,
        Entities::EntityBase* target, std::shared_ptr<std::any> param1,
        std::shared_ptr<std::any> param2)
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
        Entities::EntityBase* target, std::shared_ptr<std::any> param1,
        std::shared_ptr<std::any> param2, std::int32_t delay)
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
        Entities::EntityBase* target, std::shared_ptr<std::any> param1,
        std::shared_ptr<std::any> param2, std::uint64_t frame)
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
                throw System::NullReferenceException();
            }
            std::uint8_t& trigger = storySave->TriggerState[
                static_cast<std::size_t>(index / 8)];
            trigger = static_cast<std::uint8_t>(trigger | GetTriggerBit(index));
        }
        else if (info.Message == Message::ClearTriggerState)
        {
            const std::int32_t index = UnboxInt32(info.Param1);
            auto& storySave = GameState::StorySave;
            if (!storySave)
            {
                throw System::NullReferenceException();
            }
            std::uint8_t& trigger = storySave->TriggerState[
                static_cast<std::size_t>(index / 8)];
            trigger = static_cast<std::uint8_t>(trigger
                & static_cast<std::uint8_t>(~GetTriggerBit(index)));
        }
        else if (info.Target != nullptr)
        {
            info.Target->HandleMessage(info);
        }
    }

    void Scene::QueueMessage(MessageInfo info)
    {
        if (_queue->size() < static_cast<std::size_t>(_queueSize))
        {
            _queue->push_back(info);
        }
    }

    void Scene::ProcessMessageQueue()
    {
        for (std::int32_t i = 0; i < static_cast<std::int32_t>(_queue->size()); ++i)
        {
            MessageInfo info = _queue->at(static_cast<std::size_t>(i));
            if (info.ExecuteFrame <= _frameCount)
            {
                DispatchMessage(info);
                const std::size_t index = static_cast<std::size_t>(i);
                static_cast<void>(_queue->at(index));
                _queue->erase(_queue->begin() + static_cast<std::ptrdiff_t>(index));
                --i;
            }
        }
    }

    void Scene::ClearMessageQueue()
    {
        _queue->clear();
    }
}
