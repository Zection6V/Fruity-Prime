#pragma once

#include <cstdint>

namespace MphRead::Mods::Network
{
    // Combat participation is distinct from occupying a network session slot.
    class PlayerCombatPolicy final
    {
    public:
        PlayerCombatPolicy() = delete;

        [[nodiscard]] static constexpr bool IsInPlay(bool active, bool spawned,
            std::int32_t health, bool spectating) noexcept
        {
            return active && spawned && health > 0 && !spectating;
        }

        [[nodiscard]] static constexpr bool IntentIsInPlay(bool requestedInPlay,
            bool spectating) noexcept
        {
            return requestedInPlay && !spectating;
        }

        [[nodiscard]] static constexpr bool CanReceiveDamage(bool spectating) noexcept
        {
            return !spectating;
        }
    };
}
