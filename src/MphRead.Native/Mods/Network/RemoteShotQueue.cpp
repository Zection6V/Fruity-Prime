#include "RemoteShotQueue.hpp"

#include "NetLifecycleTracker.hpp"
#include "../../Formats/Enums.hpp"

#include <algorithm>

namespace MphRead::Mods::Network
{
    ShotQueueStats& ShotQueueStats::operator+=(const ShotQueueStats& other) noexcept
    {
        Queued += other.Queued;
        Gaps += other.Gaps;
        Recovered += other.Recovered;
        Lost += other.Lost;
        Stale += other.Stale;
        Overflow += other.Overflow;
        return *this;
    }

    void RemoteShotQueue::Receive(const IntentPacket& intent) noexcept
    {
        _active = true;
        if (_newestIntentFrame == 0 || NetLifecycleTracker::Newer(intent.Frame, _newestIntentFrame))
        {
            _newestIntentFrame = intent.Frame;
        }
        const std::size_t length = std::min<std::size_t>(intent.ShotHistoryLength, intent.ShotHistory.size());
        for (std::size_t i = 0; i < length; ++i)
        {
            const IntentPacket::ShotEvent& event = intent.ShotHistory[i];
            if (event.Sequence == 0
                || event.WeaponId > static_cast<std::uint8_t>(::MphRead::BeamType::OmegaCannon))
            {
                continue;
            }
            if (_lastSequence == 0 || NetLifecycleTracker::Newer(event.Sequence, _lastSequence))
            {
                if (_lastSequence != 0)
                {
                    AwaitSkipped(_lastSequence + 1U, event.Sequence);
                }
                _lastSequence = event.Sequence;
                Enqueue(event);
            }
            else if (TakeSkipped(event.Sequence))
            {
                // Skipped by an intent that overtook this one: late, not lost.
                ++_stats.Recovered;
                Enqueue(event);
            }
        }
        ExpireSkipped();
    }

    std::optional<IntentPacket::ShotEvent> RemoteShotQueue::Next() noexcept
    {
        while (_count > 0 && _newestIntentFrame - _queue[0].Frame > FreshFrames)
        {
            ++_stats.Stale;
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

    void RemoteShotQueue::Reset() noexcept
    {
        _stats.Lost += _skippedCount;
        const ShotQueueStats stats = _stats;
        *this = RemoteShotQueue{};
        _stats = stats;
    }

    void RemoteShotQueue::Enqueue(const IntentPacket::ShotEvent& event) noexcept
    {
        if (_count == _queue.size())
        {
            ++_stats.Overflow;
            PopFront();
        }
        // In sequence order: a recovered event goes before the newer ones.
        std::size_t at = _count;
        while (at > 0 && NetLifecycleTracker::Newer(_queue[at - 1].Sequence, event.Sequence))
        {
            _queue[at] = _queue[at - 1];
            --at;
        }
        _queue[at] = event;
        ++_count;
        ++_stats.Queued;
    }

    void RemoteShotQueue::PopFront() noexcept
    {
        std::rotate(_queue.begin(), _queue.begin() + 1, _queue.begin() + static_cast<std::ptrdiff_t>(_count));
        --_count;
    }

    void RemoteShotQueue::AwaitSkipped(std::uint32_t from, std::uint32_t to) noexcept
    {
        if (to - from > MissingWindow)
        {
            // A jump no intent still in flight can fill: counted, not walked.
            const std::uint32_t span = to - from;
            _stats.Gaps += span;
            _stats.Lost += span - MissingWindow;
            from = to - MissingWindow;
        }
        for (std::uint32_t sequence = from; sequence != to; ++sequence)
        {
            ++_stats.Gaps;
            if (sequence == 0)
            {
                continue;
            }
            if (to - sequence > MissingWindow)
            {
                // Too far behind to be carried by anything still in flight.
                ++_stats.Lost;
                continue;
            }
            if (_skippedCount == _skipped.size())
            {
                ++_stats.Lost;
                std::rotate(_skipped.begin(), _skipped.begin() + 1, _skipped.end());
                --_skippedCount;
            }
            _skipped[_skippedCount++] = sequence;
        }
    }

    bool RemoteShotQueue::TakeSkipped(std::uint32_t sequence) noexcept
    {
        for (std::size_t i = 0; i < _skippedCount; ++i)
        {
            if (_skipped[i] == sequence)
            {
                std::rotate(_skipped.begin() + static_cast<std::ptrdiff_t>(i),
                    _skipped.begin() + static_cast<std::ptrdiff_t>(i) + 1,
                    _skipped.begin() + static_cast<std::ptrdiff_t>(_skippedCount));
                --_skippedCount;
                return true;
            }
        }
        return false;
    }

    void RemoteShotQueue::ExpireSkipped() noexcept
    {
        std::size_t kept = 0;
        for (std::size_t i = 0; i < _skippedCount; ++i)
        {
            if (_lastSequence - _skipped[i] > MissingWindow)
            {
                ++_stats.Lost;
            }
            else
            {
                _skipped[kept++] = _skipped[i];
            }
        }
        _skippedCount = kept;
    }
}
