#include "MatchStart.hpp"

#include "AdventureSave.hpp"
#include "GameFiles.hpp"
#include "LaunchPlan.hpp"
#include "LauncherPrefs.hpp"
#include "../../../Entities/Players/PlayerEntity.hpp"
#include "../../../Formats/Types.hpp"
#include "../../../Menu.hpp"
#include "../../../Renderer.hpp"
#include "../../MapGen/CustomRooms.hpp"
#include "../../Network/DemoPlayback.hpp"
#include "../../Network/NetLaunch.hpp"
#include "../../Network/NetSession.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <memory>
#include <optional>
#include <string>

namespace MphRead::Mods::Launcher::Detail
{
    // Direct-owner seams required by the current native dependency surface.
    // They carry no MatchStart policy: each operation is the C# call at that
    // exact evaluation point, and its owner supplies the behavior.
    void MatchStartSetGameMode(MphRead::GameMode mode);
    [[nodiscard]] bool MatchStartIsTeamMode(MphRead::GameMode mode);
    void MatchStartCommitSave();
    void MatchStartRunWindow(MphRead::RenderWindow& renderer);
    void MatchStartSetBotLevel(MphRead::Entities::PlayerEntity& player, std::int32_t level);
}

namespace MphRead::Mods::Launcher
{
    void MatchStart::Launch(
        std::shared_ptr<MphRead::MenuSettings> settings,
        LaunchPlan plan)
    {
        if (!GameFiles::Ready())
        {
            std::cout << "[launcher] no game files; nothing to load" << std::endl;
            return;
        }

        GameFiles::ApplyPaths();
        MphRead::Mods::MapGen::CustomRooms::GenerateMissing();

        if (plan.Kind() == LaunchKind::Adventure)
        {
            LaunchAdventure(plan);
            return;
        }
        if (plan.Kind() == LaunchKind::Demo)
        {
            LaunchDemo(plan);
            return;
        }

        MphRead::Menu::SaveSlot = 0;
        if (plan.Kind() == LaunchKind::Online || plan.Kind() == LaunchKind::Host)
        {
            std::optional<MphRead::Mods::Network::NetLaunchServerRoom> room
                = MphRead::Mods::Network::NetLaunch::ServerRoom();
            if (room.has_value())
            {
                if (!settings)
                {
                    throw System::NullReferenceException();
                }
                settings->RoomKey = room->RoomKey;
            }
            else
            {
                std::cout << "[net] no map reported by the server; loading the selected map instead"
                          << std::endl;
            }
        }

        const std::string* roomKey = nullptr;
        if (plan.Kind() == LaunchKind::Offline)
        {
            if (!plan.RoomKey().has_value())
            {
                throw System::NullReferenceException();
            }
            roomKey = plan.RoomKey().value();
        }
        else
        {
            if (!settings)
            {
                throw System::NullReferenceException();
            }
            roomKey = settings->RoomKey;
        }

        if (roomKey.empty() || roomKey == "none")
        {
            return;
        }

        std::optional<std::string> unplayable
            = MphRead::Mods::MapGen::CustomRooms::WhyUnplayable(roomKey);
        if (unplayable.has_value())
        {
            std::cout << "[launcher] " << unplayable.value() << std::endl;
            return;
        }

        MphRead::RenderWindow::LogCreatingWindow();
        MphRead::RenderWindow renderer;
        MphRead::GameMode mode = plan.Mode();
        if (MphRead::Mods::Network::NetSession::Active())
        {
            std::optional<MphRead::Mods::Network::NetLaunchServerRoom> serverRoom
                = MphRead::Mods::Network::NetLaunch::ServerRoom();
            if (serverRoom.has_value())
            {
                mode = serverRoom->Mode;
            }
        }

        bool teamPlay;
        if (MphRead::Mods::Network::NetSession::Active())
        {
            teamPlay = Detail::MatchStartIsTeamMode(mode);
        }
        else
        {
            if (!settings)
            {
                throw System::NullReferenceException();
            }
            teamPlay = settings->TeamPlay == "on" || Detail::MatchStartIsTeamMode(plan.Mode());
        }

        if (MphRead::Mods::Network::NetSession::Active())
        {
            MphRead::Mods::Network::NetLaunch::BuildPlayers(
                renderer.Scene(), plan.Hunter(), LauncherPrefs::LastColor(), teamPlay);
        }
        else
        {
            AddLocalPlayers(renderer, plan, teamPlay);
        }

        renderer.AddRoom(roomKey, mode,
            MphRead::Mods::Network::NetSession::Active()
                ? MphRead::Mods::Network::NetLaunch::RoomPlayerCount
                : 0);
        Detail::MatchStartRunWindow(renderer);
    }

    void MatchStart::LaunchAdventure(LaunchPlan plan)
    {
        std::string roomKey = AdventureSave::Begin(plan.SaveSlot(), plan.NewGame());
        if (roomKey.empty())
        {
            std::cout << "[launcher] no adventure room to load" << std::endl;
            return;
        }

        Detail::MatchStartSetGameMode(MphRead::GameMode::SinglePlayer);
        MphRead::RenderWindow::LogCreatingWindow();
        {
            MphRead::RenderWindow renderer;
            MphRead::Entities::PlayerEntity::SetMaxPlayers(4);
            renderer.AddPlayer(plan.Hunter(), LauncherPrefs::LastColor(), -1);
            renderer.AddRoom(roomKey, MphRead::GameMode::SinglePlayer);
            Detail::MatchStartRunWindow(renderer);
        }
        CommitAdventureSave();
    }

    void MatchStart::LaunchDemo(LaunchPlan plan)
    {
        MphRead::Entities::PlayerEntity::SetMaxPlayers(
            MphRead::Entities::PlayerEntity::SlotCapacity);
        if (!plan.DemoPath().has_value())
        {
            throw System::ArgumentNullException("path");
        }
        if (!MphRead::Mods::Network::DemoPlayback::Join(plan.DemoPath().value()))
        {
            std::cout << "[demo] could not open or read the demo file" << std::endl;
            return;
        }

        std::optional<MphRead::Mods::Network::NetLaunchServerRoom> room
            = MphRead::Mods::Network::NetLaunch::ServerRoom();
        if (!room.has_value())
        {
            std::cout << "[demo] the demo has no match info" << std::endl;
            MphRead::Mods::Network::DemoPlayback::Stop();
            return;
        }

        MphRead::Menu::SaveSlot = 0;
        MphRead::RenderWindow::LogCreatingWindow();
        MphRead::RenderWindow renderer;
        MphRead::Mods::Network::NetLaunch::BuildPlayers(
            renderer.Scene(), MphRead::Hunter::Samus, 0,
            Detail::MatchStartIsTeamMode(room->Mode), -1);
        renderer.AddRoom(room->RoomKey, room->Mode,
            MphRead::Mods::Network::NetLaunch::RoomPlayerCount);
        Detail::MatchStartRunWindow(renderer);
        MphRead::Mods::Network::DemoPlayback::Stop();
    }

    void MatchStart::CommitAdventureSave()
    {
        if (MphRead::Menu::NeededSave != MphRead::SaveWhen::Never
            && MphRead::Menu::SaveSlot != 0)
        {
            Detail::MatchStartCommitSave();
        }
        MphRead::Menu::NeededSave = MphRead::SaveWhen::Never;
    }

    void MatchStart::AddLocalPlayers(
        MphRead::RenderWindow& renderer,
        LaunchPlan plan,
        bool teamPlay)
    {
        const std::int32_t bots = std::clamp(
            plan.Bots(), 0, MphRead::Entities::PlayerEntity::SlotCapacity - 1);
        MphRead::Entities::PlayerEntity::SetMaxPlayers(std::max(4, bots + 1));
        renderer.AddPlayer(plan.Hunter(), LauncherPrefs::LastColor(), teamPlay ? 0 : -1);
        for (std::int32_t i = 1; i <= bots; ++i)
        {
            const MphRead::Hunter hunter = static_cast<MphRead::Hunter>(
                (static_cast<std::int32_t>(plan.Hunter()) + i) % 7);
            renderer.AddPlayer(hunter, 0, teamPlay ? i % 2 : -1);
        }

        const std::int32_t level = std::clamp(plan.BotLevel(), 0, 2);
        const auto& players = MphRead::Entities::PlayerEntity::Players();
        for (std::size_t i = 0; i < players.size(); ++i)
        {
            const std::shared_ptr<MphRead::Entities::PlayerEntity>& player = players[i];
            if (!player)
            {
                throw System::NullReferenceException();
            }
            if (player->IsBot())
            {
                Detail::MatchStartSetBotLevel(*player, level);
            }
        }
    }
}
