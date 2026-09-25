#pragma once

// System.DateTime: the ticks clock, the local/UTC split, and the custom format
// strings this program asks for. Only the format specifiers the sources use
// are recognised; an unknown one is a FormatException rather than a silently
// different string.

#include <chrono>
#include <cstdint>
#include <string>
#include <string_view>

namespace MphRead::NativeRuntime
{
    struct ManagedDateTime final
    {
        // 100-nanosecond intervals since 0001-01-01 00:00:00, the DateTime
        // tick.
        std::int64_t Ticks = 0;
        // DateTimeKind: 0 Unspecified, 1 Utc, 2 Local.
        std::int32_t Kind = 0;

        [[nodiscard]] std::int64_t UtcOffsetSeconds() const;
    };

    // DateTime.UtcNow / DateTime.Now.
    [[nodiscard]] ManagedDateTime DateTimeUtcNow();
    [[nodiscard]] ManagedDateTime DateTimeNow();
    // A system_clock instant as a Utc DateTime.
    [[nodiscard]] ManagedDateTime DateTimeFromSystemClock(std::chrono::system_clock::time_point value);
    // value.ToLocalTime() for a Utc (or Unspecified) value.
    [[nodiscard]] ManagedDateTime DateTimeToLocalTime(const ManagedDateTime& value);
    // DateTime.UtcNow.Ticks.
    [[nodiscard]] std::int64_t DateTimeUtcNowTicks();

    // value.ToString(format): the custom format specifiers yyyy, MM, dd, HH,
    // mm, ss, fff and zzz, with any other character copied through.
    [[nodiscard]] std::string DateTimeToString(
        const ManagedDateTime& value, std::string_view format);
}
