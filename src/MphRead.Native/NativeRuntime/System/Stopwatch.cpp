#include "Stopwatch.hpp"

#include <bit>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <ctime>
#endif

namespace MphRead::NativeRuntime
{
    std::int64_t StopwatchGetTimestamp() noexcept
    {
#if defined(_WIN32)
        LARGE_INTEGER value{};
        (void)QueryPerformanceCounter(&value);
        return static_cast<std::int64_t>(value.QuadPart);
#else
        timespec value{};
        if (clock_gettime(CLOCK_MONOTONIC, &value) != 0)
        {
            return 0;
        }
        const std::uint64_t ticks = static_cast<std::uint64_t>(value.tv_sec) * 1'000'000'000ULL
            + static_cast<std::uint64_t>(value.tv_nsec);
        return std::bit_cast<std::int64_t>(ticks);
#endif
    }

    std::int64_t StopwatchFrequency() noexcept
    {
#if defined(_WIN32)
        static const std::int64_t frequency = []
        {
            LARGE_INTEGER value{};
            (void)QueryPerformanceFrequency(&value);
            return static_cast<std::int64_t>(value.QuadPart);
        }();
        return frequency;
#else
        return 1'000'000'000LL;
#endif
    }

    std::int64_t StopwatchGetElapsedTicks(std::int64_t start, std::int64_t end) noexcept
    {
        const double tickFrequency = 10000000.0 / static_cast<double>(StopwatchFrequency());
        return static_cast<std::int64_t>(static_cast<double>(end - start) * tickFrequency);
    }

    std::int64_t StopwatchGetElapsedTicks(std::int64_t start) noexcept
    {
        return StopwatchGetElapsedTicks(start, StopwatchGetTimestamp());
    }
}
