#include "MatchWorldProfile.hpp"

#include "../../NativeRuntime/System/Enum.hpp"

#include <algorithm>
#include <array>

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

    std::string ToString(ResourceSpawnProfile value)
    {
        static constexpr std::array<::MphRead::NativeRuntime::EnumNameEntry, 3> Names = {{
            {0, "Low"}, {1, "Standard"}, {2, "High"}}};
        return ::MphRead::NativeRuntime::ManagedEnumToString(value, Names.data(), Names.size(), false);
    }
}
