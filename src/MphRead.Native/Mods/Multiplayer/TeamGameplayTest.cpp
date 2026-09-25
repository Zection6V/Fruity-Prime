#include "TeamGameplayTest.hpp"

#include "TeamLayout.hpp"
#include "TeamVisuals.hpp"

#include "../../Entities/NodeDefenseEntity.hpp"
#include "../../Entities/Players/PlayerEntity.hpp"
#include "../../GameState.hpp"
#include "../../NativeRuntime/System/Managed.hpp"

#include <algorithm>
#include <array>

namespace MphRead::Mods::Multiplayer
{
    namespace
    {
        using ::MphRead::GameState;
        using ::MphRead::Entities::LoadFlags;
        using ::MphRead::Entities::PlayerEntity;

        [[nodiscard]] PlayerEntity& Player(std::int32_t slot)
        {
            return ::MphRead::NativeRuntime::RequireReference(
                ::MphRead::NativeRuntime::ManagedAt(PlayerEntity::Players(), slot));
        }
    }

    void TeamGameplayTest::Run(const std::function<void(bool, std::string_view)>& check)
    {
        // These checks never initialize/render an entity or process a world frame.
        PlayerEntity::Construct(nullptr);
        try
        {
            GameState::Teams(true);
            GameState::TeamCount(4);
            GameState::Mode(::MphRead::GameMode::BattleTeams);
            const std::array<std::int32_t, 8> assignments = {0, 1, 2, 3, 3, 2, 1, 0};
            for (std::int32_t i = 0; i < PlayerEntity::SlotCapacity; ++i)
            {
                PlayerEntity& player = Player(i);
                player.SetTeamIndex(assignments[static_cast<std::size_t>(i)]);
                player.SetLoadFlags(LoadFlags::Active | LoadFlags::Initial);
                player.SetHealth(99);
                const auto slot = static_cast<std::size_t>(i);
                GameState::Points()[slot] = GameState::Kills()[slot] = GameState::Deaths()[slot] = 0;
                GameState::TeamPoints()[slot] = GameState::TeamKills()[slot]
                    = GameState::TeamDeaths()[slot] = 0;
                GameState::Time()[slot] = GameState::TeamTime()[slot] = 0.0F;
            }
            GameState::TeamPoints()[0] = 10;
            GameState::TeamPoints()[1] = 8;
            GameState::TeamPoints()[2] = 9;
            GameState::TeamPoints()[3] = 11;
            GameState::Points()[7] = 20; // Individual score must not reorder teams.
            GameState::UpdateStandings();
            check(GameState::ActivePlayers() == 8 && Player(GameState::ResultSlots()[0]).TeamIndex() == 3,
                "standings include all four teams and prioritize team score");
            check(GameState::Standings()[7] == 1 && GameState::Standings()[4] == 0
                && GameState::TeamStandings()[7] == 0 && GameState::TeamStandings()[0] == 1,
                "non-parity teams, slot-seven rank and individual rank");
            for (std::size_t team = 0; team < 4; ++team)
            {
                GameState::TeamPoints()[team] = 5;
            }
            GameState::UpdateStandings();
            check(GameState::IsResultTie()
                && std::all_of(GameState::Standings().begin(), GameState::Standings().end(),
                    [](std::int32_t rank) { return rank == 0; }),
                "four-way team tie");
            GameState::TeamPoints()[3] = 4;
            GameState::UpdateStandings();
            check(GameState::IsResultTie() && GameState::Standings()[3] == 3, "three-way team tie");
            GameState::TeamPoints()[2] = 3;
            GameState::UpdateStandings();
            check(GameState::IsResultTie() && GameState::Standings()[5] == 3, "two-way team tie");
            GameState::Mode(::MphRead::GameMode::BountyTeams);
            GameState::TeamKills()[1] = 1;
            GameState::UpdateStandings();
            check(!GameState::IsResultTie() && Player(GameState::ResultSlots()[0]).TeamIndex() == 1,
                "Bounty team kills break equal objective score");

            GameState::Mode(::MphRead::GameMode::SurvivalTeams);
            GameState::PointGoal(2);
            for (std::int32_t i = 0; i < 8; ++i)
            {
                Player(i).SetHealth(assignments[static_cast<std::size_t>(i)] == 0 ? 0 : 99);
                GameState::TeamDeaths()[static_cast<std::size_t>(i)] = 3;
            }
            GameState::MatchTime(60.0F);
            GameState::UpdateSurvival(1.0F / 60.0F);
            check(GameState::MatchTime() == 60.0F,
                "Survival continues with three surviving teams after A eliminated");
            for (std::int32_t i = 0; i < 8; ++i)
            {
                Player(i).SetHealth(assignments[static_cast<std::size_t>(i)] == 3 ? 99 : 0);
            }
            GameState::UpdateSurvival(1.0F / 60.0F);
            check(GameState::MatchTime() == 0.0F && GameState::Time()[3] == -1.0F
                && GameState::Time()[4] == -1.0F,
                "Survival ends with team D and preserves both winners");

            check(TeamRules::AreAllies(0, 0) && TeamRules::AreAllies(3, 3)
                && !TeamRules::AreAllies(0, 3) && !TeamRules::AreAllies(-1, -1),
                "validated allied combat identities");
            GameState::Teams(false);
            check(!TeamRules::AreAllies(0, 0), "FFA never grants team immunity");
            for (std::int32_t i = 0; i < 8; ++i)
            {
                Player(i).SetTeamIndex(i);
            }
            GameState::Mode(::MphRead::GameMode::Battle);
            GameState::UpdateStandings();
            check(GameState::ResultSlots()[0] == 7 && GameState::Standings()[7] == 0,
                "FFA slot seven ranks first");
            for (std::int32_t i = 0; i < 8; ++i)
            {
                Player(i).SetLoadFlags(LoadFlags::None);
            }
            GameState::UpdateStandings();
            check(GameState::ActivePlayers() == 0 && !GameState::IsResultTie(),
                "empty result list is safe");
            check(::MphRead::Entities::NodeDefenseEntity::NoTeam < 0
                && TeamVisuals::Get(2).Label == "Team C"
                && TeamVisuals::Get(3).ModelRecolor == -1,
                "neutral ownership and C/D model fallback");
        }
        catch (...)
        {
            PlayerEntity::Reset();
            GameState::Teams(false);
            GameState::TeamCount(2);
            GameState::Mode(::MphRead::GameMode::SinglePlayer);
            GameState::ActivePlayers(0);
            throw;
        }
        PlayerEntity::Reset();
        GameState::Teams(false);
        GameState::TeamCount(2);
        GameState::Mode(::MphRead::GameMode::SinglePlayer);
        GameState::ActivePlayers(0);
    }
}
