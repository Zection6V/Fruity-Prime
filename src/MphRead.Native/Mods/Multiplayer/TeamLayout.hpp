#pragma once

#include <cstdint>
#include <span>
#include <string>

namespace MphRead::Mods::Multiplayer
{
    struct TeamLayout final
    {
        std::uint8_t TeamCount = 0;
        std::uint8_t TeamA = 0;
        std::uint8_t TeamB = 0;
        std::uint8_t TeamC = 0;
        std::uint8_t TeamD = 0;

        constexpr TeamLayout() noexcept = default;
        constexpr TeamLayout(std::uint8_t teamCount, std::uint8_t teamA, std::uint8_t teamB,
            std::uint8_t teamC = 0, std::uint8_t teamD = 0) noexcept
            : TeamCount(teamCount), TeamA(teamA), TeamB(teamB), TeamC(teamC), TeamD(teamD)
        {
        }

        [[nodiscard]] constexpr std::int32_t TotalPlayers() const noexcept
        {
            return TeamA + TeamB + TeamC + TeamD;
        }

        [[nodiscard]] constexpr std::uint8_t Capacity(std::int32_t team) const noexcept
        {
            switch (team)
            {
            case 0: return TeamA;
            case 1: return TeamB;
            case 2: return TeamC;
            case 3: return TeamD;
            default: return 0;
            }
        }

        [[nodiscard]] constexpr bool IsValid() const noexcept
        {
            if (TeamCount < 2 || TeamCount > 4 || TotalPlayers() < 2 || TotalPlayers() > 8)
            {
                return false;
            }
            for (std::int32_t team = 0; team < 4; ++team)
            {
                if (team < TeamCount ? Capacity(team) == 0 : Capacity(team) != 0)
                {
                    return false;
                }
            }
            return true;
        }

        [[nodiscard]] std::string ToString() const;

        [[nodiscard]] friend constexpr bool operator==(
            const TeamLayout& left, const TeamLayout& right) noexcept = default;
    };

    class TeamRules final
    {
    public:
        TeamRules() = delete;
        ~TeamRules() = delete;
        TeamRules(const TeamRules&) = delete;
        TeamRules& operator=(const TeamRules&) = delete;
        TeamRules(TeamRules&&) = delete;
        TeamRules& operator=(TeamRules&&) = delete;

        inline static constexpr std::int32_t NoTeam = -1;

        [[nodiscard]] static bool AreAllies(std::int32_t first, std::int32_t second);

        // Cross multiplication keeps normalized occupancy exact and
        // deterministic.
        [[nodiscard]] static std::int8_t ChooseTeam(
            const TeamLayout& layout, std::span<const std::int32_t> counts);
    };
}
