#pragma once

#include <algorithm>
#include <cstdint>

namespace MphRead::Mods::Combat
{
    // EU1.1 02110288 / 021104E0: integer canonical state and fixed rounding.
    class HalfturretFireRate final
    {
    public:
        HalfturretFireRate() = delete;
        static constexpr std::int32_t Scale = 4096;
        static constexpr std::int32_t Normal = 6144;
        static constexpr std::int32_t Floor = 2867;
        static constexpr std::int32_t DamageStep = 61;

        [[nodiscard]] static constexpr std::int32_t AfterDamage(
            std::int32_t raw, std::uint32_t damage) noexcept
        {
            const auto next = static_cast<std::int64_t>(raw)
                - static_cast<std::int64_t>(damage) * DamageStep;
            return static_cast<std::int32_t>(std::max<std::int64_t>(next, Floor));
        }

        [[nodiscard]] static constexpr std::int32_t Recover(std::int32_t raw) noexcept
        {
            if (raw < Normal)
            {
                return static_cast<std::int32_t>(std::min<std::int64_t>(
                    static_cast<std::int64_t>(raw) + DamageStep, Normal));
            }
            // Truncating the ROM's positive raw - 61.44 subtracts 62.
            return raw > Normal ? std::max(raw - 62, Normal) : Normal;
        }

        [[nodiscard]] static constexpr std::uint32_t Threshold(
            std::uint32_t cooldown, std::int32_t raw) noexcept
        {
            const auto rounded = (static_cast<std::uint64_t>(cooldown)
                * static_cast<std::uint32_t>(raw) + 0x800U) >> 12;
            return static_cast<std::uint32_t>(std::max<std::uint64_t>(rounded, 7));
        }
    };
}
