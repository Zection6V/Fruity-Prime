#pragma once

// The window loop saying it is still turning over.
//
// A process that stops drawing leaves nothing behind: no exception, no fault,
// no line in any log, just a window Windows eventually calls "not
// responding". The debug log's freeze watchdog reads this to tell that case
// apart from a slow frame, and to know which thread to look at. It is a
// timestamp and a thread id, written once a loop iteration and read by nobody
// else, so it changes nothing about what the loop does.

#include <cstdint>

namespace MphRead::NativeRuntime
{
    // Called by the window loop once an iteration.
    void FrameHeartbeat() noexcept;
    // Steady-clock milliseconds of the last heartbeat, or 0 if there has been none.
    [[nodiscard]] std::int64_t LastFrameHeartbeat() noexcept;
    // The native id of the thread that last beat.
    [[nodiscard]] std::uint64_t FrameHeartbeatThread() noexcept;
}
