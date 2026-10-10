#include "RemoteShotQueue.hpp"

#include "NetLifecycleTracker.hpp"
#include "../../Formats/Enums.hpp"

#include <algorithm>

namespace MphRead::Mods::Network
{
    void RemoteShotQueue::Receive(const IntentPacket& intent) noexcept
    {
        _active = true;
        if (_newestIntentFrame == 0 || NetLifecycleTracker::Newer(intent.Frame, _newestIntentFrame))
        {
            _newestIntentFrame = intent.Frame;
        }
        // The history is oldest first, so the queue keeps firing order
        // however much of it was seen before.
        const std::size_t length = std::min<std::size_t>(intent.ShotHistoryLength, intent.ShotHistory.size());
        for (std::size_t i = 0; i < length; ++i)
        {
            const IntentPacket::ShotEvent& event = intent.ShotHistory[i];
            if (event.Sequence == 0
                || event.WeaponId > static_cast<std::uint8_t>(::MphRead::BeamType::OmegaCannon)
                || (_lastSequence != 0 && !NetLifecycleTracker::Newer(event.Sequence, _lastSequence)))
            {
                continue;
            }
            if (_count == _queue.size())
            {
                PopFront();
            }
            _queue[_count++] = event;
            _lastSequence = event.Sequence;
        }
    }

    std::optional<IntentPacket::ShotEvent> RemoteShotQueue::Next() noexcept
    {
        while (_count > 0 && _newestIntentFrame - _queue[0].Frame > FreshFrames)
        {
            PopFront();
        }
        if (_count == 0)
        {
            return std::nullopt;
        }
        return _queue[0];
    }

    void RemoteShotQueue::Consume() noexcept
    {
        if (_count > 0)
        {
            PopFront();
        }
    }

    void RemoteShotQueue::PopFront() noexcept
    {
        const auto end = static_cast<std::ptrdiff_t>(_count);
        std::rotate(_queue.begin(), _queue.begin() + 1, _queue.begin() + end);
        --_count;
    }
}
