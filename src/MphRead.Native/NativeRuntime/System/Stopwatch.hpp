#pragma once

// System.Diagnostics.Stopwatch's static members: the monotonic timestamp and
// how many of its ticks make a second (QueryPerformanceCounter on Windows,
// nanoseconds of CLOCK_MONOTONIC elsewhere).

#include <cstdint>

namespace MphRead::NativeRuntime
{
    // Stopwatch.GetTimestamp().
    [[nodiscard]] std::int64_t StopwatchGetTimestamp() noexcept;
    // Stopwatch.Frequency.
    [[nodiscard]] std::int64_t StopwatchFrequency() noexcept;
    // Stopwatch.GetElapsedTime(start, end), as TimeSpan ticks: the tick
    // difference scaled to 100-nanosecond units and truncated, as .NET does.
    [[nodiscard]] std::int64_t StopwatchGetElapsedTicks(std::int64_t start, std::int64_t end) noexcept;
    // Stopwatch.GetElapsedTime(start) -- against the timestamp now.
    [[nodiscard]] std::int64_t StopwatchGetElapsedTicks(std::int64_t start) noexcept;
    // TimeSpan.TotalMilliseconds of a TimeSpan with this many ticks.
    [[nodiscard]] constexpr double TimeSpanTotalMilliseconds(std::int64_t ticks) noexcept
    {
        return static_cast<double>(ticks) / 10000.0;
    }
}
