#pragma once

#include <cstdint>

namespace MphRead::Mods::Render
{
    // Cosmetic trail offsets derived only from simulation state.
    class LockjawTrailNoise final
    {
    public:
        LockjawTrailNoise() = delete;

        [[nodiscard]] static float Sample(std::uint64_t tick, std::int32_t ownerSlot,
            std::int32_t sourceBomb, std::int32_t targetBomb, std::int32_t segment,
            std::int32_t axis) noexcept;
    };
}
