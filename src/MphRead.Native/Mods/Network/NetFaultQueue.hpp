#pragma once

#include "../../NativeRuntime/System/Exceptions.hpp"
#include "../../NativeRuntime/System/Random.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <queue>
#include <utility>
#include <vector>

namespace MphRead::Mods::Network
{
    // A bounded, seeded datagram scheduler. Time is supplied by the caller so
    // tests need no sleeps.
    //
    // A C# generic, so the whole class is here; NetFaultQueue.cpp is its pair
    // and holds nothing else.
    template <typename T>
    class NetFaultQueue final
    {
    public:
        NetFaultQueue(std::int32_t seed, double delayMs, double jitterMs, double loss,
            double reorder, double duplicate, std::int32_t capacity = 2048)
            : _random(seed)
        {
            if (!std::isfinite(delayMs) || !std::isfinite(jitterMs) || delayMs < 0 || jitterMs < 0
                || !Probability(loss) || !Probability(reorder) || !Probability(duplicate) || capacity < 1)
            {
                throw System::ArgumentOutOfRangeException("delayMs");
            }
            _delay = delayMs;
            _jitter = jitterMs;
            _loss = loss;
            _reorder = reorder;
            _duplicate = duplicate;
            _capacity = capacity;
        }

        [[nodiscard]] std::int32_t Count() const noexcept
        {
            return static_cast<std::int32_t>(_queue.size());
        }
        [[nodiscard]] std::int64_t Dropped() const noexcept { return _dropped; }
        [[nodiscard]] std::int64_t Duplicated() const noexcept { return _duplicated; }
        [[nodiscard]] std::int64_t Reordered() const noexcept { return _reordered; }

        void Enqueue(double nowMs, T value, double extraDelayMs = 0)
        {
            if (_random.NextDouble() < _loss)
            {
                ++_dropped;
                return;
            }
            // Jitter alone retains the old FIFO contract. Explicit reordering
            // delays one datagram without holding the following datagrams back.
            const double jitter = _random.NextDouble() * _jitter;
            double due = std::max(_lastDue, nowMs + _delay + jitter + extraDelayMs);
            _lastDue = due;
            if (_random.NextDouble() < _reorder)
            {
                const double spread = 1 + _random.NextDouble();
                due += std::max(20.0, _jitter + _delay) * spread;
                ++_reordered;
            }
            Add(value, due);
            if (_random.NextDouble() < _duplicate)
            {
                const double extra = 1 + _random.NextDouble() * std::max(1.0, _jitter);
                Add(value, due + extra);
                ++_duplicated;
            }
        }

        [[nodiscard]] bool TryDequeue(double nowMs, T& value)
        {
            if (!_queue.empty() && _queue.top().Due <= nowMs)
            {
                value = _queue.top().Value;
                _queue.pop();
                return true;
            }
            value = T{};
            return false;
        }

    private:
        struct Entry final
        {
            double Due = 0;
            std::int64_t Order = 0;
            T Value{};
        };

        // PriorityQueue<T, (double Due, long Order)>: the smallest (Due,
        // Order) first. Order is unique, so the ordering is total and any
        // correct heap dequeues in the same sequence as .NET's.
        struct Later final
        {
            [[nodiscard]] bool operator()(const Entry& left, const Entry& right) const noexcept
            {
                return left.Due != right.Due ? left.Due > right.Due : left.Order > right.Order;
            }
        };

        [[nodiscard]] static bool Probability(double value) noexcept
        {
            return std::isfinite(value) && value >= 0 && value <= 1;
        }

        void Add(const T& value, double due)
        {
            if (static_cast<std::int32_t>(_queue.size()) >= _capacity)
            {
                ++_dropped;
                return;
            }
            _queue.push(Entry{due, _order++, value});
        }

        std::priority_queue<Entry, std::vector<Entry>, Later> _queue;
        ::MphRead::NativeRuntime::Random _random;
        double _delay = 0;
        double _jitter = 0;
        double _loss = 0;
        double _reorder = 0;
        double _duplicate = 0;
        std::int32_t _capacity = 0;
        std::int64_t _order = 0;
        double _lastDue = 0;
        std::int64_t _dropped = 0;
        std::int64_t _duplicated = 0;
        std::int64_t _reordered = 0;
    };
}
