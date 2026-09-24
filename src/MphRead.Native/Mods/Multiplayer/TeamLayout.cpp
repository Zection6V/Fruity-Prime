#include "TeamLayout.hpp"

#include "../../GameState.hpp"

namespace MphRead::Mods::Multiplayer
{
    std::string TeamLayout::ToString() const
    {
        switch (TeamCount)
        {
        case 2:
            return std::to_string(TeamA) + "v" + std::to_string(TeamB);
        case 3:
            return std::to_string(TeamA) + "v" + std::to_string(TeamB)
                + "v" + std::to_string(TeamC);
        case 4:
            return std::to_string(TeamA) + "v" + std::to_string(TeamB)
                + "v" + std::to_string(TeamC) + "v" + std::to_string(TeamD);
        default:
            return "FFA";
        }
    }

    bool TeamRules::AreAllies(std::int32_t first, std::int32_t second)
    {
        return ::MphRead::GameState::Teams()
            && first >= 0 && first < ::MphRead::GameState::TeamCount() && first == second;
    }

    std::int8_t TeamRules::ChooseTeam(
        const TeamLayout& layout, std::span<const std::int32_t> counts)
    {
        std::int32_t best = -1;
        for (std::int32_t team = 0; team < layout.TeamCount; ++team)
        {
            const std::int32_t capacity = layout.Capacity(team);
            if (capacity == 0 || counts[static_cast<std::size_t>(team)] >= capacity)
            {
                continue;
            }
            const std::int32_t teamCount = counts[static_cast<std::size_t>(team)];
            if (best < 0)
            {
                best = team;
                continue;
            }
            const std::int32_t bestCount = counts[static_cast<std::size_t>(best)];
            const std::int32_t bestCapacity = layout.Capacity(best);
            if (teamCount * bestCapacity < bestCount * capacity
                || (teamCount * bestCapacity == bestCount * capacity && teamCount < bestCount))
            {
                best = team;
            }
        }
        return static_cast<std::int8_t>(best);
    }
}
