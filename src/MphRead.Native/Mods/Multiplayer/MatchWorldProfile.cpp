#include "MatchWorldProfile.hpp"

#include <algorithm>

namespace MphRead::Mods::Multiplayer
{
    MatchWorldProfile MatchWorldProfile::Resolve(std::int32_t configuredPlayers) noexcept
    {
        const std::int32_t players = std::clamp(configuredPlayers, 2, 8);
        return MatchWorldProfile(
            static_cast<std::uint8_t>(std::min(players, 4)),
            players == 2 ? ResourceSpawnProfile::Low
                : players <= 4 ? ResourceSpawnProfile::Standard
                : ResourceSpawnProfile::High);
    }
}
