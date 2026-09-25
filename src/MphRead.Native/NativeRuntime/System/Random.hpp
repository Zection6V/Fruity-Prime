#pragma once

// System.Random: `new Random()` (xoshiro, seeded from the OS, as .NET 6+
// does) and `new Random(seed)` (the Knuth subtractive generator .NET keeps
// for seeded instances, so a seed gives the sequence it gives in C#), plus
// Random.Shared.

#include <array>
#include <cstdint>

namespace MphRead::NativeRuntime
{
    class Random final
    {
    public:
        // new Random(): throws when the OS has no randomness to give.
        Random();
        // new Random(seed).
        explicit Random(std::int32_t seed) noexcept;

        [[nodiscard]] double NextDouble() noexcept;
        // Next(maxValue): 0 <= result < maxValue; ArgumentOutOfRangeException
        // when maxValue is negative.
        [[nodiscard]] std::int32_t Next(std::int32_t maxValue);

    private:
        [[nodiscard]] std::int32_t InternalSample() noexcept;
        [[nodiscard]] std::uint64_t NextUInt64() noexcept;
        [[nodiscard]] std::uint32_t NextUInt32() noexcept;

        bool _seeded = false;
        std::array<std::int32_t, 56> _seedArray{};
        std::int32_t _inext = 0;
        std::int32_t _inextp = 0;
        std::array<std::uint64_t, 4> _state64{};
        std::array<std::uint32_t, 4> _state32{};
    };

    // Random.Shared.Next(maxValue): 0 <= result < maxValue, and 0 when
    // maxValue is 0.
    [[nodiscard]] std::int32_t RandomSharedNext(std::int32_t maxValue);

    // RandomNumberGenerator.Fill: the OS's cryptographic source.
    void RandomNumberGeneratorFill(void* buffer, std::size_t size);
}
