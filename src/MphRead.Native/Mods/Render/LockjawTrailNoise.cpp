#include "LockjawTrailNoise.hpp"

namespace MphRead::Mods::Render
{
    float LockjawTrailNoise::Sample(std::uint64_t tick, std::int32_t ownerSlot,
        std::int32_t sourceBomb, std::int32_t targetBomb, std::int32_t segment,
        std::int32_t axis) noexcept
    {
        // unchecked: every step wraps, which is what uint arithmetic does here.
        std::uint32_t value = 0x9E3779B9U;
        value = (value ^ static_cast<std::uint32_t>(tick)) * 0x01000193U;
        value = (value ^ static_cast<std::uint32_t>(tick >> 32)) * 0x01000193U;
        value = (value ^ static_cast<std::uint32_t>(ownerSlot)) * 0x01000193U;
        value = (value ^ static_cast<std::uint32_t>(sourceBomb)) * 0x01000193U;
        value = (value ^ static_cast<std::uint32_t>(targetBomb)) * 0x01000193U;
        value = (value ^ static_cast<std::uint32_t>(segment)) * 0x01000193U;
        value = (value ^ static_cast<std::uint32_t>(axis)) * 0x01000193U;
        value ^= value >> 16;
        value *= 0x7FEB352DU;
        value ^= value >> 15;
        value *= 0x846CA68BU;
        value ^= value >> 16;
        return static_cast<float>(value & 0xFFFFU) / 65536.0F * 0.5F - 0.25F;
    }
}
