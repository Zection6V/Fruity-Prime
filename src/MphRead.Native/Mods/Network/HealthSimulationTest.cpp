#include "HealthSimulationTest.hpp"

#include "NetHealthSync.hpp"
#include "NetLaunch.hpp"
#include "NetMatchTimeSync.hpp"
#include "NetProtocol.hpp"
#include "NetSession.hpp"
#include "ServerSim.hpp"
#include "SessionProtocol.hpp"
#include "../Input/SyntheticInput.hpp"
#include "../Multiplayer/MatchWorldProfile.hpp"
#include "../../Entities/ItemInstanceEntity.hpp"
#include "../../Entities/ItemSpawnEntity.hpp"
#include "../../Entities/Players/PlayerEntity.hpp"
#include "../../NativeRuntime/System/Console.hpp"
#include "../../NativeRuntime/System/Managed.hpp"
#include "../../Program.hpp"
#include "../../Read.hpp"
#include "../../Scene.hpp"

#include <exception>
#include <memory>

namespace MphRead::Mods::Network
{
    namespace Runtime = ::MphRead::NativeRuntime;
    using Entities::ItemSpawnEntity;

    std::int32_t HealthSimulationTest::Run(const std::string& room)
    {
        ServerSim sim{};
        std::int32_t result = 1;
        try
        {
            SessionStatePacket session{};
            session.Phase = SessionPhase::InMatch;
            session.Policy = ServerSessionPolicy::Lobby;
            session.Revision = 1;
            session.MatchId = 1;
            session.AuthorityEpoch = 1;
            session.MaxPlayers = 8;
            session.OwnerSlot = 0;
            session.Match.RoomKey = room;
            session.Match.Mode = GameMode::Battle;
            session.Match.Format = MatchFormat::FreeForAll;
            session.Match.PointGoal = 999;
            session.Match.TimeLimitSeconds = 600;
            session.WorldProfile = Multiplayer::MatchWorldProfile::Resolve(8);
            RosterPacket roster = RosterPacket::Create();
            roster.MatchId = 1;
            roster.AuthorityEpoch = 1;
            roster.Revision = 1;
            roster.SessionRevision = 1;
            for (std::uint8_t i = 0; i < 8; i++)
            {
                (*roster.Slots)[i] = i;
                (*roster.Generations)[i] = 1;
                (*roster.Teams)[i] = -1;
                (*roster.Names)[i] = "Health" + std::to_string(i);
                roster.Count++;
            }
            std::vector<std::uint8_t> latest;
            if (!sim.Start(room, GameMode::Battle, 8,
                    [&latest](std::span<const std::uint8_t> payload) { latest.assign(payload.begin(), payload.end()); },
                    []() {}, roster, session))
            {
                throw ProgramException("Authority could not load the room.");
            }
            for (std::int32_t i = 0; i < 180; i++)
            {
                sim.Step();
            }
            std::shared_ptr<ItemSpawnEntity> spawn{};
            for (const std::shared_ptr<ItemSpawnEntity>& candidate : NetHealthSync::RegisteredSpawns())
            {
                if (candidate->Item() != nullptr && candidate->Active)
                {
                    spawn = candidate;
                    break;
                }
            }
            if (spawn == nullptr)
            {
                throw System::InvalidOperationException("Sequence contains no matching element");
            }
            const std::size_t count = NetHealthSync::RegisteredSpawns().size();
            const auto id = static_cast<std::int16_t>(spawn->Id);
            spawn->Item()->OnPickedUp();
            for (std::int32_t i = 0; i < 3; i++)
            {
                sim.Step();
            }
            if (spawn->Item() != nullptr)
            {
                throw ProgramException("Picked health did not despawn.");
            }
            const std::vector<std::uint8_t> unavailable = Tail(latest);
            for (std::int32_t i = 0; i < 1200 && spawn->Item() == nullptr; i++)
            {
                sim.Step();
            }
            if (spawn->Item() == nullptr)
            {
                throw ProgramException("Health did not respawn.");
            }
            const std::vector<std::uint8_t> available = Tail(latest);
            if (sim.StepFailures() != 0)
            {
                throw ProgramException("Authority simulation had failed steps.");
            }
            sim.Stop();

            NetSession::StartPlayback();
            NetSession::ApplySessionState(session);
            NetSession::ApplyRoster(roster);
            Entities::PlayerEntity::SetMaxPlayers(8);
            auto keyboard = Input::SyntheticInput::CreateKeyboard();
            auto mouse = Input::SyntheticInput::CreateMouse();
            auto scene = std::make_shared<Scene>(OpenTK::Mathematics::Vector2i(256, 192), *keyboard, *mouse,
                [](std::string) {}, []() {});
            NetLaunch::BuildPlayers(*scene, Hunter::Samus, 0, false, -1);
            scene->AddRoom(room, GameMode::Battle, NetLaunch::RoomPlayerCount());
            scene->OnLoad();
            if (NetHealthSync::RegisteredSpawns().size() != count)
            {
                throw ProgramException("Replica constructed different health entities.");
            }
            std::shared_ptr<ItemSpawnEntity> replica{};
            for (const std::shared_ptr<ItemSpawnEntity>& candidate : NetHealthSync::RegisteredSpawns())
            {
                if (candidate->Id == id)
                {
                    if (replica != nullptr)
                    {
                        throw System::InvalidOperationException("Sequence contains more than one matching element");
                    }
                    replica = candidate;
                }
            }
            if (replica == nullptr)
            {
                throw System::InvalidOperationException("Sequence contains no matching element");
            }
            NetHealthSync::Receive(unavailable);
            for (std::int32_t i = 0; i < 3; i++)
            {
                scene->OnSimulationFrame();
            }
            if (replica->Item() != nullptr)
            {
                throw ProgramException("Replica spawned unavailable health.");
            }
            NetHealthSync::Receive(available);
            for (std::int32_t i = 0; i < 3; i++)
            {
                scene->OnSimulationFrame();
            }
            if (replica->Item() == nullptr || NetHealthSync::OwnsPickup(*replica->Item()))
            {
                throw ProgramException("Replica did not restore authoritative availability.");
            }
            NetHealthSync::Receive(unavailable);
            for (std::int32_t i = 0; i < 3; i++)
            {
                scene->OnSimulationFrame();
            }
            if (replica->Item() != nullptr)
            {
                throw ProgramException("Replica did not remove consumed health.");
            }
            Runtime::ConsoleWriteLine("[healthsimtest] PASS " + room + ": " + std::to_string(count)
                + " health spawners; authority pickup/respawn and replica convergence.");
            result = 0;
        }
        catch (const std::exception& ex)
        {
            Runtime::ConsoleWriteLine("[healthsimtest] FAIL " + room + ": " + ex.what());
            result = 1;
        }
        sim.Stop();
        NetSession::Stop();
        Read::ClearCache();
        return result;
    }

    std::vector<std::uint8_t> HealthSimulationTest::Tail(const std::vector<std::uint8_t>& snapshot)
    {
        if (snapshot.size() < static_cast<std::size_t>(SnapshotHeader::Size))
        {
            throw ProgramException("No authoritative snapshot.");
        }
        const std::size_t start = SnapshotHeader::Size
            + static_cast<std::size_t>(SnapshotHeader::Read(snapshot).PlayerCount) * PlayerState::Size + NetMatchTimeSync::Size;
        if (snapshot.size() < start || !NetHealthSync::Validate(std::span<const std::uint8_t>(snapshot).subspan(start)))
        {
            throw ProgramException("Invalid health snapshot tail.");
        }
        return std::vector<std::uint8_t>(snapshot.begin() + static_cast<std::ptrdiff_t>(start), snapshot.end());
    }
}
