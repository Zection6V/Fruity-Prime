#pragma once

// System.Random.Shared: the process-wide instance .NET exposes, with the
// thread-safety that makes it usable without a lock.

#include <cstdint>

namespace MphRead::NativeRuntime
{
    // Random.Shared.Next(maxValue): 0 <= result < maxValue, and 0 when
    // maxValue is 0.
    [[nodiscard]] std::int32_t RandomSharedNext(std::int32_t maxValue);
}
