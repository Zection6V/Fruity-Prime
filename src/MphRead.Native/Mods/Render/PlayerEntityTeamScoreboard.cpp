#include "PlayerEntityTeamScoreboard.hpp"

#include "../Multiplayer/TeamVisuals.hpp"
#include "../../Entities/Players/PlayerEntity.hpp"
#include "../../GameState.hpp"
#include "../../NativeRuntime/System/Managed.hpp"

#include <algorithm>

namespace MphRead::Entities
{
    using ::MphRead::NativeRuntime::ManagedAt;
    using ::MphRead::NativeRuntime::RequireReference;

    void PlayerEntity::ModDrawTeamScoreboard()
    {
        const GameMode mode = GameState::Mode();
        const bool timed = mode == GameMode::SurvivalTeams || mode == GameMode::DefenderTeams;
        const bool deaths = mode == GameMode::BattleTeams || mode == GameMode::SurvivalTeams;
        const ColorRgba ink(239, 239, 247, 255);
        float y = 16;
        if (GameState::MatchState() != MatchState::InProgress)
        {
            std::string winners = GameState::IsResultTie() ? "TIE: " : "WINNER: ";
            std::int32_t last = -1;
            for (std::int32_t i = 0; i < GameState::ActivePlayers(); i++)
            {
                const std::int32_t slot = ManagedAt(GameState::ResultSlots(), i);
                const std::int32_t team = RequireReference(ManagedAt(Players(), slot)).TeamIndex();
                if (ManagedAt(GameState::Standings(), slot) != 0 || last == team)
                {
                    continue;
                }
                if (last != -1)
                {
                    winners += " / ";
                }
                winners += static_cast<char>('A' + team);
                last = team;
            }
            static_cast<void>(DrawText2D(ModScoreNameColumn() - 18, 4, Hud::Align::Left, 0, winners, ink, 1.0F, -1.0F, -1, 0.75F));
        }
        static_cast<void>(DrawText2D(ModScoreNameColumn() - 18, y, Hud::Align::Left, 0, "TEAMS", ink, 1.0F, -1.0F, -1, 0.75F));
        static_cast<void>(DrawText2D(ModScoreColumn1(), y, Hud::Align::Center, 0, timed ? "TIME" : "POINTS", ink));
        static_cast<void>(DrawText2D(ModScoreColumn2(), y, Hud::Align::Center, 0, deaths ? "DEATHS" : "KILLS", ink));
        ModDrawPingHeader(y);
        y += 14;
        std::int32_t previous = -1;
        for (std::int32_t i = 0; i < GameState::ActivePlayers(); i++)
        {
            const std::int32_t slot = ManagedAt(GameState::ResultSlots(), i);
            PlayerEntity& player = RequireReference(ManagedAt(Players(), slot));
            const std::int32_t team = player.TeamIndex();
            const Mods::Multiplayer::TeamPresentation& visual = Mods::Multiplayer::TeamVisuals::Get(team);
            if (team != previous)
            {
                static_cast<void>(DrawText2D(ModScoreNameColumn() - 18, y, Hud::Align::Left, 0, visual.Label, visual.Color,
                    1.0F, -1.0F, -1, 0.85F));
                static_cast<void>(DrawText2D(ModScoreColumn1(), y, Hud::Align::Center, 0,
                    TeamScoreValue(timed, ManagedAt(GameState::TeamTime(), team), ManagedAt(GameState::TeamPoints(), team)),
                    visual.Color));
                static_cast<void>(DrawText2D(ModScoreColumn2(), y, Hud::Align::Center, 0,
                    std::to_string(deaths ? ManagedAt(GameState::TeamDeaths(), team) : ManagedAt(GameState::TeamKills(), team)),
                    visual.Color));
                previous = team;
                y += 12;
            }
            const std::int32_t nameLength = std::clamp(static_cast<std::int32_t>(
                (ModScoreColumn1() - ModScoreNameColumn() - 6) / (6.4F * HudAspectFix())), 4, 20);
            const std::string name = (player.IsMainPlayer() ? "> " : "  ") + ManagedAt(GameState::Nicknames(), slot);
            static_cast<void>(DrawText2D(ModScoreNameColumn() - 18, y, Hud::Align::Left, 0, name, ink, 1.0F, -1.0F,
                nameLength, 0.8F));
            static_cast<void>(DrawText2D(ModScoreColumn1(), y, Hud::Align::Center, 0,
                TeamScoreValue(timed, ManagedAt(GameState::Time(), slot), ManagedAt(GameState::Points(), slot)), ink));
            static_cast<void>(DrawText2D(ModScoreColumn2(), y, Hud::Align::Center, 0,
                std::to_string(deaths ? ManagedAt(GameState::Deaths(), slot) : ManagedAt(GameState::Kills(), slot)), ink));
            ModDrawPingRow(y, ink, slot);
            y += 13;
        }
    }

    std::string PlayerEntity::TeamScoreValue(bool timed, float time, std::int32_t points) const
    {
        return !timed ? std::to_string(points) : time < 0 ? std::string("MAX") : FormatTime(time);
    }
}
