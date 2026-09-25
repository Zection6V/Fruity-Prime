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
}
