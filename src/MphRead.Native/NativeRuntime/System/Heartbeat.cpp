#include "Heartbeat.hpp"

#include <atomic>
#include <chrono>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

namespace MphRead::NativeRuntime
{
    namespace
    {
        std::atomic<std::int64_t> LastBeat{0};
        std::atomic<std::uint64_t> BeatThread{0};
    }

    void FrameHeartbeat() noexcept
    {
        const auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count();
#if defined(_WIN32)
        BeatThread.store(::GetCurrentThreadId(), std::memory_order_relaxed);
#endif
        LastBeat.store(now == 0 ? 1 : now, std::memory_order_release);
    }

    std::int64_t LastFrameHeartbeat() noexcept
    {
        return LastBeat.load(std::memory_order_acquire);
    }

    std::uint64_t FrameHeartbeatThread() noexcept
    {
        return BeatThread.load(std::memory_order_relaxed);
    }
}
