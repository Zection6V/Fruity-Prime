#include "NetLaunch.hpp"

#include "../../Features.hpp"
#include "../../NativeRuntime/System/Globalization.hpp"
#include "../../Scene.hpp"
#include "../Launcher/Portable/LaunchPlan.hpp"
#include "../Launcher/Portable/LauncherPrefs.hpp"
#include "../RespawnChoice.hpp"
#include "NetLog.hpp"
#include "NetSession.hpp"
#include "NetStatus.hpp"
#include "PlayerColors.hpp"
#include "../../Entities/Players/PlayerEntity.hpp"
#include "../../Formats/Formats.hpp"
#include "../../NativeRuntime/System/Console.hpp"
#include "NetProtocol.hpp"

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace
{
    using Clock = std::chrono::steady_clock;

    std::int64_t ElapsedMilliseconds(const Clock::time_point& start)
    {
        return std::chrono::duration_cast<std::chrono::milliseconds>(
            Clock::now() - start).count();
    }

    double ElapsedSeconds(const Clock::time_point& start)
    {
        return std::chrono::duration<double>(Clock::now() - start).count();
    }

    std::string Endpoint(const std::string& address, std::int32_t port)
    {
        return address + ":" + std::to_string(port);
    }

    std::string JoinNames(const std::vector<std::string>& names)
    {
        std::string result;
        for (std::size_t index = 0; index < names.size(); ++index)
        {
            if (index != 0)
            {
                result += ", ";
            }
            result += names[index];
        }
        return result;
    }
}

namespace MphRead::Mods::Network
{
    std::string NetLaunch::_lastJoinError;

    bool NetLaunch::Join(const std::string& address, std::int32_t port,
        const std::string& playerName, Hunter hunter,
        std::int32_t timeoutMs, std::int32_t color)
    {
        NetSession::SetPlayerName(playerName);
        NetSession::SetLocalHunter(Launcher::Hunters::Resolve(hunter));
        NetSession::SetLocalColor(PlayerColors::Clamp(
            color < 0 ? Launcher::LauncherPrefs::LastColor() : color));
        Entities::PlayerEntity::SetMaxPlayers(Entities::PlayerEntity::SlotCapacity);
        NetSession::StartClient(address, port);
        if (!NetSession::Active())
        {
            _lastJoinError = "Could not open a socket for " + Endpoint(address, port) + ".";
            return false;
        }

        const Clock::time_point clock = Clock::now();
        std::int32_t lastIdentify = 0;
        while (ElapsedMilliseconds(clock) < timeoutMs)
        {
            NetSession::Update(ElapsedSeconds(clock));
            if (NetSession::Refused())
            {
                _lastJoinError = DescribeJoinFailure(address, port);
                NativeRuntime::ConsoleWriteLine(("[net] " + _lastJoinError));
                return false;
            }

            const std::int32_t localSlot = NetSession::LocalSlot();
            bool hasRoom = false;
            if (localSlot >= 0)
            {
                std::optional<MatchStatePacket> match
                    = NetSession::ServerMatch();
                if (match.has_value())
                {
                    hasRoom = match->RoomKey.value().length() > 0;
                }
            }
            if (localSlot >= 0 && hasRoom)
            {
                MatchStatePacket state
                    = NetSession::ServerMatch().value();
                std::string message = "[net] joining ";
                message += state.RoomKey.value_or(std::string{});
                message += " (";
                message += ::MphRead::ToString(static_cast<GameMode>(state.Mode));
                message += "), ";
                message += ::MphRead::NativeRuntime::ToString(state.TimeRemaining, "0");
                message += " s remaining, slot ";
                message += std::to_string(NetSession::LocalSlot());
                NativeRuntime::ConsoleWriteLine(message);
                DisableCheatsForMatch();
                return true;
            }

            if (ElapsedMilliseconds(clock) - lastIdentify > 500)
            {
                lastIdentify = static_cast<std::int32_t>(ElapsedMilliseconds(clock));
                NetSession::SendIdentify();
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
        }

        _lastJoinError = DescribeJoinFailure(address, port);
        NativeRuntime::ConsoleWriteLine(("[net] " + _lastJoinError));
        return false;
    }

    const std::string& NetLaunch::LastJoinError()
    {
        return _lastJoinError;
    }

    std::string NetLaunch::DescribeJoinFailure(const std::string& address,
        std::int32_t port)
    {
        ::MphRead::Mods::Network::ServerStatus status
            = NetStatus::Query(address, port, false, 1500);
        const std::string endpoint = Endpoint(address, port);
        if (!status.Online)
        {
            return "No answer from " + endpoint + ". The server may be off, "
                "or UDP may be blocked between here and it.";
        }
        if (status.MaxPlayers > 0 && status.Players >= status.MaxPlayers)
        {
            return endpoint + " is full (" + std::to_string(status.Players) + "/"
                + std::to_string(status.MaxPlayers)
                + " players). Try again when somebody leaves.";
        }
        if (status.Protocol > 0 && status.Protocol != NetConfig::ProtocolVersion)
        {
            return endpoint + " is running protocol " + std::to_string(status.Protocol)
                + " and this build speaks "
                + std::to_string(NetConfig::ProtocolVersion)
                + ". One of you needs updating.";
        }
        return endpoint + " answered, but would not admit this client ("
            + std::to_string(status.Players) + "/" + std::to_string(status.MaxPlayers)
            + " players). It may have filled up while joining.";
    }

    void NetLaunch::DisableCheatsForMatch()
    {
        std::vector<std::string> turnedOff;
        for (const Cheats::BooleanProperty& property : Cheats::BooleanProperties())
        {
            if (!false
                || !false
                || !false)
            {
                continue;
            }
            if (property.Get())
            {
                turnedOff.push_back(property.Name);
                property.Set(false);
            }
        }
        if (!turnedOff.empty())
        {
            const std::string list = JoinNames(turnedOff);
            NativeRuntime::ConsoleWriteLine(("[net] cheats are off while connected (" + list + ")"));
            NetLog::Event(
                "cheats disabled for this session: " + list);
        }
    }

    std::optional<NetLaunchServerRoom> NetLaunch::ServerRoom()
    {
        std::optional<MatchStatePacket> state
            = NetSession::ServerMatch();
        if (!state.has_value() || state->RoomKey.value().length() == 0)
        {
            return std::nullopt;
        }
        const GameMode mode = ::MphRead::IsDefinedGameMode(state->Mode)
            ? static_cast<GameMode>(state->Mode)
            : GameMode::Battle;
        return NetLaunchServerRoom{state->RoomKey.value(), mode};
    }

    void NetLaunch::BuildPlayers(Scene& scene, Hunter localHunter,
        std::int32_t localRecolor, bool teams,
        std::optional<std::int32_t> localSlot)
    {
        std::int32_t resolvedSlot;
        if (localSlot.has_value())
        {
            resolvedSlot = localSlot.value();
        }
        else
        {
            resolvedSlot = std::max(NetSession::LocalSlot(), 0);
        }

        for (std::int32_t slot = 0;
            slot < Entities::PlayerEntity::MaxPlayers(); ++slot)
        {
            Hunter hunter = slot == resolvedSlot
                ? localHunter
                : NetSession::SlotHunter[slot];
            if (slot == resolvedSlot && slot >= 0
                && slot < static_cast<std::int32_t>(PlayerColors::Choice.size()))
            {
                PlayerColors::Choice[slot] = PlayerColors::Clamp(localRecolor);
            }
            const std::int32_t recolor = slot == resolvedSlot ? localRecolor : 0;
            const std::int32_t teamIndex = teams ? slot % 2 : -1;
            scene.AddPlayer(hunter, recolor, teamIndex);
        }

        for (std::int32_t slot = 0;
            slot < Entities::PlayerEntity::MaxPlayers(); ++slot)
        {
            std::optional<std::shared_ptr<Entities::PlayerEntity>> player;
            if (slot < static_cast<std::int32_t>(Entities::PlayerEntity::Players().size()))
            {
                player = Entities::PlayerEntity::Players().at(static_cast<std::size_t>(slot));
            }
            if (!player.has_value())
            {
                continue;
            }
            (player.value())->SetIsBot(false);
            (player.value())->SetBotLevel(0);
            if (slot == resolvedSlot)
            {
                continue;
            }
            bool occupied = false;
            if (slot < static_cast<std::int32_t>(NetSession::SlotOccupied.size()))
            {
                occupied = NetSession::SlotOccupied[slot];
            }
            if (!occupied)
            {
                (player.value())->SetLoadFlags((player.value())->LoadFlags() & ~Entities::LoadFlags::Active);
            }
        }

        Entities::PlayerEntity::SetPlayerCount(1);
        const std::int32_t mainIndex = resolvedSlot >= 0 ? resolvedSlot : 0;
        Entities::PlayerEntity::SetMainPlayerIndex(mainIndex);
        PlayerColors::Resolve();
        Mods::RespawnChoice::Reset();
        NativeRuntime::ConsoleWriteLine(("[net] player slots built, main player = slot " + std::to_string(mainIndex)));
        NetLog::Event(
            "player slots built, main = slot " + std::to_string(mainIndex));
    }
}
