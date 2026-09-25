#pragma once

// System.HashCode.Combine: xxHash32 over the values' own hash codes, seeded
// once per process from random bytes, as .NET seeds it. The seed is one for
// the whole program -- a hash computed in one file has to equal the same hash
// computed in another.

#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace MphRead::NativeRuntime
{
    namespace HashCodeDetail
    {
        inline constexpr std::uint32_t Prime1 = 2654435761U;
        inline constexpr std::uint32_t Prime2 = 2246822519U;
        inline constexpr std::uint32_t Prime3 = 3266489917U;
        inline constexpr std::uint32_t Prime4 = 668265263U;
        inline constexpr std::uint32_t Prime5 = 374761393U;

        // HashCode's s_seed.
        [[nodiscard]] std::uint32_t Seed() noexcept;

        [[nodiscard]] constexpr std::uint32_t Round(std::uint32_t hash, std::uint32_t input) noexcept
        {
            return std::rotl(hash + input * Prime2, 13) * Prime1;
        }

        [[nodiscard]] constexpr std::uint32_t QueueRound(std::uint32_t hash, std::uint32_t queued) noexcept
        {
            return std::rotl(hash + queued * Prime3, 17) * Prime4;
        }

        [[nodiscard]] constexpr std::uint32_t MixFinal(std::uint32_t hash) noexcept
        {
            hash ^= hash >> 15;
            hash *= Prime2;
            hash ^= hash >> 13;
            hash *= Prime3;
            hash ^= hash >> 16;
            return hash;
        }
    }

    // HashCode.Combine(value1, ..., valueN) for N from 1 to 8, given each
    // value's GetHashCode().
    template <typename... Hashes>
    requires (sizeof...(Hashes) >= 1 && sizeof...(Hashes) <= 8)
    [[nodiscard]] std::int32_t HashCodeCombine(Hashes... hashes) noexcept
    {
        using namespace HashCodeDetail;
        constexpr std::size_t Count = sizeof...(Hashes);
        const std::array<std::uint32_t, Count> values{std::bit_cast<std::uint32_t>(static_cast<std::int32_t>(hashes))...};
        const std::uint32_t seed = Seed();
        std::uint32_t hash = 0;
        std::size_t queued = 0;
        if constexpr (Count < 4)
        {
            hash = seed + Prime5;
        }
        else
        {
            std::uint32_t v1 = seed + Prime1 + Prime2;
            std::uint32_t v2 = seed + Prime2;
            std::uint32_t v3 = seed;
            std::uint32_t v4 = seed - Prime1;
            v1 = Round(v1, values[0]);
            v2 = Round(v2, values[1]);
            v3 = Round(v3, values[2]);
            v4 = Round(v4, values[3]);
            queued = 4;
            if constexpr (Count == 8)
            {
                v1 = Round(v1, values[4]);
                v2 = Round(v2, values[5]);
                v3 = Round(v3, values[6]);
                v4 = Round(v4, values[7]);
                queued = 8;
            }
            hash = std::rotl(v1, 1) + std::rotl(v2, 7) + std::rotl(v3, 12) + std::rotl(v4, 18);
        }
        hash += static_cast<std::uint32_t>(Count * 4);
        for (std::size_t i = queued; i < Count; ++i)
        {
            hash = QueueRound(hash, values[i]);
        }
        return std::bit_cast<std::int32_t>(MixFinal(hash));
    }

    // string.GetHashCode(): over the UTF-16 code units, seeded once per
    // process, so two equal strings hash alike within a run and nothing may
    // rely on the number across runs -- which is .NET's contract too.
    [[nodiscard]] std::int32_t StringGetHashCode(std::string_view value) noexcept;

    // RuntimeHelpers.GetHashCode(object) / object.GetHashCode() for a
    // reference type that does not override it: the identity of the object.
    [[nodiscard]] std::int32_t ReferenceGetHashCode(const void* value) noexcept;
}
