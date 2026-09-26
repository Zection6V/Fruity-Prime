#pragma once

// A C# `event Action<...>`: handlers added with +=, taken away with -=, and
// raised in the order they were added. A delegate has no identity to take
// away by here, so += hands back a token that -= takes.

#include <cstdint>
#include <functional>
#include <mutex>
#include <utility>
#include <vector>

namespace MphRead::NativeRuntime
{
    template <typename... Args>
    class Event final
    {
    public:
        using Handler = std::function<void(Args...)>;

        std::int64_t Add(Handler handler)
        {
            const std::lock_guard<std::mutex> guard(_lock);
            const std::int64_t token = ++_next;
            _handlers.emplace_back(token, std::move(handler));
            return token;
        }

        void Remove(std::int64_t token)
        {
            const std::lock_guard<std::mutex> guard(_lock);
            std::erase_if(_handlers, [token](const auto& entry) { return entry.first == token; });
        }

        // handler?.Invoke(args): the list as it stood when raised.
        void Invoke(Args... args) const
        {
            std::vector<std::pair<std::int64_t, Handler>> handlers;
            {
                const std::lock_guard<std::mutex> guard(_lock);
                handlers = _handlers;
            }
            for (const auto& entry : handlers)
            {
                entry.second(args...);
            }
        }

    private:
        mutable std::mutex _lock;
        std::vector<std::pair<std::int64_t, Handler>> _handlers;
        std::int64_t _next = 0;
    };
}
