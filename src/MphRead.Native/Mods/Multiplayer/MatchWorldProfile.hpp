#pragma once

#include <cstdint>
#include <string>

namespace MphRead::Mods::Multiplayer
{
    enum class ResourceSpawnProfile : std::uint8_t
    {
        Low = 0,
        Standard = 1,
        High = 2
    };

    // ResourceSpawnProfile.ToString().
    [[nodiscard]] std::string ToString(ResourceSpawnProfile value);

    struct MatchWorldProfile final
    {
        std::uint8_t EntityLayerPlayers = 0;
        ResourceSpawnProfile Resources = ResourceSpawnProfile::Low;

        constexpr MatchWorldProfile() noexcept = default;
        constexpr MatchWorldProfile(
            std::uint8_t entityLayerPlayers, ResourceSpawnProfile resources) noexcept
            : EntityLayerPlayers(entityLayerPlayers), Resources(resources)
        {
        }

        [[nodiscard]] constexpr bool IsValid() const noexcept
        {
            // Enum.IsDefined(Resources).
            const bool defined = Resources == ResourceSpawnProfile::Low
                || Resources == ResourceSpawnProfile::Standard
                || Resources == ResourceSpawnProfile::High;
            return EntityLayerPlayers >= 2 && EntityLayerPlayers <= 4 && defined
                && (EntityLayerPlayers == 2 ? Resources == ResourceSpawnProfile::Low
                    : EntityLayerPlayers == 3 ? Resources == ResourceSpawnProfile::Standard
                    : Resources != ResourceSpawnProfile::Low);
        }

        [[nodiscard]] static MatchWorldProfile Resolve(std::int32_t configuredPlayers) noexcept;

        [[nodiscard]] friend constexpr bool operator==(
            MatchWorldProfile left, MatchWorldProfile right) noexcept = default;
    };
}
