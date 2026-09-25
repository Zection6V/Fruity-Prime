#include "LobbyRules.hpp"

#include "NetProtocol.hpp"

#include "../../GameState.hpp"
#include "../../NativeRuntime/System/Globalization.hpp"
#include "../../NativeRuntime/System/Managed.hpp"

#include <array>

namespace MphRead::Mods::Network
{
    namespace Runtime = ::MphRead::NativeRuntime;
    using Multiplayer::TeamLayout;

    TeamLayout LobbyRules::ResolveTeamLayout(const MatchDefinition& match)
    {
        if (!::MphRead::GameState::IsTeamMode(match.Mode))
        {
            return {};
        }
        switch (match.Format)
        {
        case MatchFormat::OneVsOne: return TeamLayout(2, 1, 1);
        case MatchFormat::TwoVsTwo: return TeamLayout(2, 2, 2);
        case MatchFormat::ThreeVsThree: return TeamLayout(2, 3, 3);
        case MatchFormat::TwoVsTwoVsTwoVsTwo: return TeamLayout(4, 2, 2, 2, 2);
        case MatchFormat::Custom: return match.CustomTeams;
        default: return TeamLayout(2, 4, 4);
        }
    }

    std::int32_t LobbyRules::TeamCount(const MatchDefinition& match)
    {
        return ResolveTeamLayout(match).TeamCount;
    }

    std::int32_t LobbyRules::TeamCapacity(const MatchDefinition& match, std::int32_t team)
    {
        return ResolveTeamLayout(match).Capacity(team);
    }

    bool LobbyRules::ExactTeams(const MatchDefinition& match)
    {
        return ::MphRead::GameState::IsTeamMode(match.Mode) && match.Format != MatchFormat::Auto;
    }

    Multiplayer::MatchWorldProfile LobbyRules::ResolveWorldProfile(const MatchDefinition& match, std::int32_t maxPlayers)
    {
        return Multiplayer::MatchWorldProfile::Resolve(
            ExactTeams(match) ? ResolveTeamLayout(match).TotalPlayers() : maxPlayers);
    }

    LobbyResultCode LobbyRules::ValidateDefinition(const MatchDefinition& match, std::string& reason)
    {
        reason.clear();
        const bool formatDefined = static_cast<std::uint8_t>(match.Format) <= static_cast<std::uint8_t>(MatchFormat::Custom);
        if (Runtime::StringIsNullOrWhiteSpace(match.RoomKey)
            || static_cast<std::int32_t>(match.RoomKey->size()) > HostRequestPacket::MaxRoomBytes
            || !formatDefined || !::MphRead::IsDefinedGameMode(static_cast<std::uint64_t>(match.Mode))
            || match.Mode == ::MphRead::GameMode::SinglePlayer || match.Mode == ::MphRead::GameMode::None
            || match.Mode == ::MphRead::GameMode::Unknown15)
        {
            reason = "Choose a multiplayer map and mode.";
        }
        else if (match.Format != MatchFormat::Auto
            && ::MphRead::GameState::IsTeamMode(match.Mode) != (match.Format != MatchFormat::FreeForAll))
        {
            reason = "Choose a team mode for a team format, or a free-for-all mode for FFA.";
        }
        else if (::MphRead::GameState::IsTeamMode(match.Mode) && !ResolveTeamLayout(match).IsValid())
        {
            reason = "Use 2 to 4 nonempty teams, zero inactive capacities, and at most 8 players.";
        }
        else if (match.Mode == ::MphRead::GameMode::Capture && TeamCount(match) != 2)
        {
            reason = "Capture requires exactly two teams because maps have two bases.";
        }
        return reason.empty() ? LobbyResultCode::Ok : LobbyResultCode::InvalidConfiguration;
    }

    LobbyResultCode LobbyRules::Validate(const MatchDefinition& match, const RosterPacket& roster,
        bool requireReady, std::string& reason)
    {
        const LobbyResultCode result = ValidateDefinition(match, reason);
        if (result != LobbyResultCode::Ok)
        {
            return result;
        }
        const TeamLayout layout = ResolveTeamLayout(match);
        const bool exact = ExactTeams(match);
        const std::int32_t required = exact ? layout.TotalPlayers() : match.Format == MatchFormat::FreeForAll ? 2 : 1;
        if (roster.Count < required || (exact && roster.Count != required))
        {
            reason = exact ? layout.ToString() + " requires exactly " + std::to_string(required) + " players."
                : "At least " + std::to_string(required) + " players must join.";
            return LobbyResultCode::NotEnoughPlayers;
        }
        std::array<std::int32_t, 4> counts{};
        const auto& teams = Runtime::RequireReference(roster.Teams);
        const auto& ready = Runtime::RequireReference(roster.LobbyReady);
        const auto& names = Runtime::RequireReference(roster.Names);
        for (std::int32_t i = 0; i < roster.Count; i++)
        {
            if (layout.TeamCount > 0)
            {
                const std::int32_t team = Runtime::ManagedAt(teams, i);
                if (team < 0 || team >= layout.TeamCount)
                {
                    reason = "Every player needs a valid team.";
                    return LobbyResultCode::InvalidTeam;
                }
                counts[static_cast<std::size_t>(team)]++;
            }
            if (requireReady && !Runtime::ManagedAt(ready, i))
            {
                reason = "Waiting for " + Runtime::ManagedAt(names, i).value_or("") + " to ready.";
                return LobbyResultCode::PlayersNotReady;
            }
        }
        for (std::int32_t team = 0; team < layout.TeamCount; team++)
        {
            const std::int32_t capacity = layout.Capacity(team);
            const std::int32_t count = counts[static_cast<std::size_t>(team)];
            if (count > capacity || (exact && count != capacity))
            {
                reason = std::string("Team ") + static_cast<char>('A' + team) + " needs " + std::to_string(capacity)
                    + " players (currently " + std::to_string(count) + ").";
                return LobbyResultCode::InvalidTeam;
            }
        }
        reason = "Ready to start.";
        return LobbyResultCode::Ok;
    }
}
