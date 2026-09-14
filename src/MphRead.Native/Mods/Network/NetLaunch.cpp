#include "NetLaunch.hpp"

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace MphRead::Mods::Network::Detail
{
    struct NetLaunchMatchState
    {
        std::optional<std::string> RoomKey;
        std::int32_t Mode;
        double TimeRemaining;
    };

    struct NetLaunchServerStatus
    {
        bool Online;
        std::int32_t Players;
        std::int32_t MaxPlayers;
        std::int32_t Protocol;
    };

    using NetLaunchCheatProperty = std::uintptr_t;
    using NetLaunchPlayer = std::uintptr_t;

    Hunter NetLaunchResolveHunter(Hunter hunter);
    std::int32_t NetLaunchLastColor();
    std::int32_t NetLaunchClampPlayerColor(std::int32_t color);
    std::int32_t NetLaunchPlayerSlotCapacity();
    void NetLaunchSetPlayerMaxPlayers(std::int32_t value);

    void NetLaunchSetSessionPlayerName(const std::string& value);
    void NetLaunchSetSessionLocalHunter(Hunter value);
    void NetLaunchSetSessionLocalColor(std::int32_t value);
    void NetLaunchStartClient(const std::string& address, std::int32_t port);
    bool NetLaunchSessionActive();
    void NetLaunchSessionUpdate(double elapsedSeconds);
    bool NetLaunchSessionRefused();
    std::string NetLaunchDescribeRefusedReason(const std::string& where);
    std::int32_t NetLaunchSessionLocalSlot();
    std::optional<NetLaunchMatchState> NetLaunchSessionServerMatch();
    void NetLaunchSessionSendIdentify();

    NetLaunchServerStatus NetLaunchQueryStatus(const std::string& address,
        std::int32_t port, bool allowJoinProbe, std::int32_t timeoutMs);
    std::int32_t NetLaunchProtocolVersion();

    std::string NetLaunchGameModeToString(std::int32_t mode);
    std::string NetLaunchFormatZeroDecimals(double value);
    bool NetLaunchIsDefinedGameMode(std::int32_t value);
    GameMode NetLaunchBattleGameMode();

    std::vector<NetLaunchCheatProperty> NetLaunchPublicStaticCheatProperties();
    bool NetLaunchCheatPropertyTypeIsBoolean(NetLaunchCheatProperty property);
    bool NetLaunchCheatPropertyCanRead(NetLaunchCheatProperty property);
    bool NetLaunchCheatPropertyCanWrite(NetLaunchCheatProperty property);
    std::optional<bool> NetLaunchGetCheatPropertyValue(NetLaunchCheatProperty property);
    std::string NetLaunchCheatPropertyName(NetLaunchCheatProperty property);
    void NetLaunchSetCheatPropertyValue(NetLaunchCheatProperty property, bool value);

    void NetLaunchConsoleWriteLine(const std::string& value);
    void NetLaunchNetLogEvent(const std::string& value);

    std::int32_t NetLaunchPlayerMaxPlayers();
    Hunter NetLaunchSlotHunter(std::int32_t slot);
    std::int32_t NetLaunchPlayerColorChoiceLength();
    void NetLaunchSetPlayerColorChoice(std::int32_t slot, std::int32_t color);
    void NetLaunchSceneAddPlayer(Scene& scene, Hunter hunter,
        std::int32_t recolor, std::int32_t teamIndex);
    std::int32_t NetLaunchPlayersCount();
    std::optional<NetLaunchPlayer> NetLaunchPlayerAt(std::int32_t slot);
    void NetLaunchSetPlayerIsBot(NetLaunchPlayer player, bool value);
    void NetLaunchSetPlayerBotLevel(NetLaunchPlayer player, std::int32_t value);
    std::int32_t NetLaunchSlotOccupiedLength();
    bool NetLaunchSlotOccupied(std::int32_t slot);
    void NetLaunchClearPlayerActiveFlag(NetLaunchPlayer player);
    void NetLaunchSetPlayerCount(std::int32_t value);
    void NetLaunchSetMainPlayerIndex(std::int32_t value);
    void NetLaunchResolvePlayerColors();
    void NetLaunchResetRespawnChoice();

    struct NetConnectCommandServerRoom
    {
        std::string RoomKey;
        std::uint8_t Mode;
    };
}

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
        Detail::NetLaunchSetSessionPlayerName(playerName);
        Detail::NetLaunchSetSessionLocalHunter(Detail::NetLaunchResolveHunter(hunter));
        Detail::NetLaunchSetSessionLocalColor(Detail::NetLaunchClampPlayerColor(
            color < 0 ? Detail::NetLaunchLastColor() : color));
        Detail::NetLaunchSetPlayerMaxPlayers(Detail::NetLaunchPlayerSlotCapacity());
        Detail::NetLaunchStartClient(address, port);
        if (!Detail::NetLaunchSessionActive())
        {
            _lastJoinError = "Could not open a socket for " + Endpoint(address, port) + ".";
            return false;
        }

        const Clock::time_point clock = Clock::now();
        std::int32_t lastIdentify = 0;
        while (ElapsedMilliseconds(clock) < timeoutMs)
        {
            Detail::NetLaunchSessionUpdate(ElapsedSeconds(clock));
            if (Detail::NetLaunchSessionRefused())
            {
                _lastJoinError = Detail::NetLaunchDescribeRefusedReason(Endpoint(address, port));
                Detail::NetLaunchConsoleWriteLine("[net] " + _lastJoinError);
                return false;
            }

            const std::int32_t localSlot = Detail::NetLaunchSessionLocalSlot();
            bool hasRoom = false;
            if (localSlot >= 0)
            {
                std::optional<Detail::NetLaunchMatchState> match
                    = Detail::NetLaunchSessionServerMatch();
                if (match.has_value())
                {
                    hasRoom = match->RoomKey.value().length() > 0;
                }
            }
            if (localSlot >= 0 && hasRoom)
            {
                Detail::NetLaunchMatchState state
                    = Detail::NetLaunchSessionServerMatch().value();
                std::string message = "[net] joining ";
                message += state.RoomKey.value_or(std::string{});
                message += " (";
                message += Detail::NetLaunchGameModeToString(state.Mode);
                message += "), ";
                message += Detail::NetLaunchFormatZeroDecimals(state.TimeRemaining);
                message += " s remaining, slot ";
                message += std::to_string(Detail::NetLaunchSessionLocalSlot());
                Detail::NetLaunchConsoleWriteLine(message);
                DisableCheatsForMatch();
                return true;
            }

            if (ElapsedMilliseconds(clock) - lastIdentify > 500)
            {
                lastIdentify = static_cast<std::int32_t>(ElapsedMilliseconds(clock));
                Detail::NetLaunchSessionSendIdentify();
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
        }

        _lastJoinError = DescribeJoinFailure(address, port);
        Detail::NetLaunchConsoleWriteLine("[net] " + _lastJoinError);
        return false;
    }

    const std::string& NetLaunch::LastJoinError()
    {
        return _lastJoinError;
    }

    std::string NetLaunch::DescribeJoinFailure(const std::string& address,
        std::int32_t port)
    {
        Detail::NetLaunchServerStatus status
            = Detail::NetLaunchQueryStatus(address, port, false, 1500);
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
        if (status.Protocol > 0 && status.Protocol != Detail::NetLaunchProtocolVersion())
        {
            return endpoint + " is running protocol " + std::to_string(status.Protocol)
                + " and this build speaks "
                + std::to_string(Detail::NetLaunchProtocolVersion())
                + ". One of you needs updating.";
        }
        return endpoint + " answered, but would not admit this client ("
            + std::to_string(status.Players) + "/" + std::to_string(status.MaxPlayers)
            + " players). It may have filled up while joining.";
    }

    void NetLaunch::DisableCheatsForMatch()
    {
        std::vector<std::string> turnedOff;
        for (Detail::NetLaunchCheatProperty property
            : Detail::NetLaunchPublicStaticCheatProperties())
        {
            if (!Detail::NetLaunchCheatPropertyTypeIsBoolean(property)
                || !Detail::NetLaunchCheatPropertyCanRead(property)
                || !Detail::NetLaunchCheatPropertyCanWrite(property))
            {
                continue;
            }
            if (Detail::NetLaunchGetCheatPropertyValue(property).value_or(false))
            {
                turnedOff.push_back(Detail::NetLaunchCheatPropertyName(property));
                Detail::NetLaunchSetCheatPropertyValue(property, false);
            }
        }
        if (!turnedOff.empty())
        {
            const std::string list = JoinNames(turnedOff);
            Detail::NetLaunchConsoleWriteLine(
                "[net] cheats are off while connected (" + list + ")");
            Detail::NetLaunchNetLogEvent(
                "cheats disabled for this session: " + list);
        }
    }

    std::optional<NetLaunchServerRoom> NetLaunch::ServerRoom()
    {
        std::optional<Detail::NetLaunchMatchState> state
            = Detail::NetLaunchSessionServerMatch();
        if (!state.has_value() || state->RoomKey.value().length() == 0)
        {
            return std::nullopt;
        }
        const GameMode mode = Detail::NetLaunchIsDefinedGameMode(state->Mode)
            ? static_cast<GameMode>(state->Mode)
            : Detail::NetLaunchBattleGameMode();
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
            resolvedSlot = std::max(Detail::NetLaunchSessionLocalSlot(), 0);
        }

        for (std::int32_t slot = 0;
            slot < Detail::NetLaunchPlayerMaxPlayers(); ++slot)
        {
            Hunter hunter = slot == resolvedSlot
                ? localHunter
                : Detail::NetLaunchSlotHunter(slot);
            if (slot == resolvedSlot && slot >= 0
                && slot < Detail::NetLaunchPlayerColorChoiceLength())
            {
                Detail::NetLaunchSetPlayerColorChoice(
                    slot, Detail::NetLaunchClampPlayerColor(localRecolor));
            }
            const std::int32_t recolor = slot == resolvedSlot ? localRecolor : 0;
            const std::int32_t teamIndex = teams ? slot % 2 : -1;
            Detail::NetLaunchSceneAddPlayer(scene, hunter, recolor, teamIndex);
        }

        for (std::int32_t slot = 0;
            slot < Detail::NetLaunchPlayerMaxPlayers(); ++slot)
        {
            std::optional<Detail::NetLaunchPlayer> player;
            if (slot < Detail::NetLaunchPlayersCount())
            {
                player = Detail::NetLaunchPlayerAt(slot);
            }
            if (!player.has_value())
            {
                continue;
            }
            Detail::NetLaunchSetPlayerIsBot(player.value(), false);
            Detail::NetLaunchSetPlayerBotLevel(player.value(), 0);
            if (slot == resolvedSlot)
            {
                continue;
            }
            bool occupied = false;
            if (slot < Detail::NetLaunchSlotOccupiedLength())
            {
                occupied = Detail::NetLaunchSlotOccupied(slot);
            }
            if (!occupied)
            {
                Detail::NetLaunchClearPlayerActiveFlag(player.value());
            }
        }

        Detail::NetLaunchSetPlayerCount(1);
        const std::int32_t mainIndex = resolvedSlot >= 0 ? resolvedSlot : 0;
        Detail::NetLaunchSetMainPlayerIndex(mainIndex);
        Detail::NetLaunchResolvePlayerColors();
        Detail::NetLaunchResetRespawnChoice();
        Detail::NetLaunchConsoleWriteLine(
            "[net] player slots built, main player = slot " + std::to_string(mainIndex));
        Detail::NetLaunchNetLogEvent(
            "player slots built, main = slot " + std::to_string(mainIndex));
    }
}

namespace MphRead::Mods::Network::Detail
{
    bool NetConnectCommandNetLaunchJoin(const std::string& host, std::int32_t port,
        const std::string& playerName, Hunter hunter, std::int32_t color)
    {
        return NetLaunch::Join(host, port, playerName, hunter, 8000, color);
    }

    std::optional<NetConnectCommandServerRoom> NetConnectCommandNetLaunchServerRoom()
    {
        std::optional<NetLaunchServerRoom> room = NetLaunch::ServerRoom();
        if (!room.has_value())
        {
            return std::nullopt;
        }
        return NetConnectCommandServerRoom{room->RoomKey,
            static_cast<std::uint8_t>(room->Mode)};
    }

    void NetConnectCommandNetLaunchBuildPlayers(Scene& scene, Hunter hunter,
        std::int32_t recolor, bool teams)
    {
        NetLaunch::BuildPlayers(scene, hunter, recolor, teams);
    }
}
