#include "NetworkCommands.hpp"

#include "CommandLine.hpp"
#include "OptionValues.hpp"

#include "../../Entities/Players/PlayerEntity.hpp"
#include "../../Formats/Enums.hpp"
#include "../../Formats/Formats.hpp"
#include "../../NativeRuntime/System/Globalization.hpp"
#include "../../NativeRuntime/System/Number.hpp"
#include "../../NativeRuntime/System/Runtime.hpp"
#include "../../NativeRuntime/System/Tasks.hpp"
#include "../Network/Bots/ServerBots.hpp"
#include "../Network/Discovery/NetMaster.hpp"
#include "../Network/Discovery/NetStatus.hpp"
#include "../Network/Integration/Launch/NetConnectCommand.hpp"
#include "../Network/Replay/DemoInfo.hpp"
#include "../Network/Server/Core/LocalServer.hpp"
#include "../Network/Testing/Harness/NetCheckClient.hpp"
#include "../Update/UpdateCheck.hpp"

#include <algorithm>
#include <cstdint>
#include <iostream>
#include <optional>
#include <string_view>
#include <utility>

using ::MphRead::NativeRuntime::Int32TryParseCurrentCulture;
using ::MphRead::NativeRuntime::StringEqualsOrdinalIgnoreCase;

namespace
{
    void WriteLine(std::string_view value)
    {
        std::cout << value << '\n';
    }

    void SetExitCode(std::int32_t value) noexcept
    {
        MphRead::NativeRuntime::SetEnvironmentExitCode(value);
    }
}

namespace MphRead::Mods::Entry
{
    bool TryRunNetworkCommands(const std::vector<std::string>& args, int width, int height)
    {
        if (HasFlag(args, "installserver"))
        {
            const std::string serverRid = Update::UpdateCheck::ServerRid();
            WriteLine("[server] this platform takes the \"" + serverRid + "\" package");
            if (!Network::LocalServer::CanInstall())
            {
                WriteLine("[server] no server package is published for it");
                SetExitCode(1);
                return true;
            }
            int lastPercent = -1;
            const bool installed = Network::LocalServer::Install(
                [&lastPercent](float fraction)
                {
                    const int percent = static_cast<int>(fraction * 100.0F);
                    if (percent >= lastPercent + 10)
                    {
                        lastPercent = percent;
                        WriteLine("[server] " + std::to_string(percent) + "%");
                    }
                });
            if (!installed)
            {
                WriteLine("[server] " + Network::LocalServer::LastError().value_or(""));
                SetExitCode(1);
                return true;
            }
            WriteLine("[server] installed " + Network::LocalServer::InstalledTag()
                + " into " + Network::LocalServer::Directory());
            return true;
        }

        const std::optional<std::string> hostLocal = ValueAfter(args, "hostlocal");
        if (hostLocal.has_value())
        {
            GameMode localMode = GameMode::Battle;
            (void)TryParseGameMode(ValueAfter(args, "mode"), localMode);
            std::vector<std::pair<std::string, GameMode>> localMaps;
            for (const std::string& entry : NativeRuntime::StringSplit(*hostLocal, ',', true, true))
            {
                localMaps.emplace_back(entry, localMode);
            }

            const std::optional<Network::ServerBinary> binary = Network::LocalServer::Available();
            if (!binary.has_value())
            {
                WriteLine("[hostlocal] nothing on this machine can run a server");
            }
            else
            {
                WriteLine("[hostlocal] using " + binary->Describe() + " (" + binary->Executable + ")");
            }

            double holdSeconds = 8.0;
            double parsedHold = 0.0;
            if (TryParseDoubleInvariant(ValueAfter(args, "seconds"), parsedHold))
            {
                holdSeconds = parsedHold;
            }
            const std::string serverName = ValueAfter(args, "servername").value_or("Local test server");
            const std::string masterHost = ValueAfter(args, "master").value_or(
                std::string(Network::NetMasterConfig::DefaultHost));
            const std::int32_t localPort = Network::LocalServer::Start(serverName, localMaps,
                Entities::PlayerEntity::SlotCapacity, 7.0F * 60.0F, 7,
                masterHost, ParseMasterPort(args), !HasFlag(args, "nomaster"));
            if (localPort < 0)
            {
                WriteLine("[hostlocal] " + Network::LocalServer::LastError().value_or(""));
                SetExitCode(1);
                return true;
            }
            WriteLine("[hostlocal] listening on 127.0.0.1:" + std::to_string(localPort));
            const Network::ServerStatus localStatus = Network::NetStatus::Query(
                "127.0.0.1", localPort, false);
            WriteLine("[hostlocal] it answers: " + localStatus.RoomKey + " ("
                + Network::NetStatus::ModeName(localStatus.Mode) + "), "
                + std::to_string(localStatus.Players) + "/"
                + std::to_string(localStatus.MaxPlayers) + " players");
            NativeRuntime::ThreadSleep(static_cast<std::int32_t>(holdSeconds * 1000.0));
            Network::LocalServer::Stop();
            WriteLine("[hostlocal] stopped");
            return true;
        }

        const std::optional<std::string> hostGame = ValueAfter(args, "hostgame");
        if (hostGame.has_value())
        {
            const std::string masterHost = ValueAfter(args, "master").value_or(
                std::string(Network::NetMasterConfig::DefaultHost));
            int masterPort = Network::NetMasterConfig::DefaultPort;
            const std::optional<std::string> masterPortValue = ValueAfter(args, "masterport");
            std::int32_t parsedMasterPort = 0;
            if (masterPortValue.has_value() && Int32TryParseCurrentCulture(*masterPortValue, parsedMasterPort))
            {
                masterPort = parsedMasterPort;
            }
            GameMode hostMode = GameMode::Battle;
            (void)TryParseGameMode(ValueAfter(args, "mode"), hostMode);
            const std::string hostName = ParseName(args);
            std::vector<std::pair<std::string, GameMode>> hostRotation{{*hostGame, hostMode}};
            const std::optional<std::string> rotationValue = ValueAfter(args, "maprotation");
            if (rotationValue.has_value())
            {
                for (const std::string& entry : NativeRuntime::StringSplit(*rotationValue, ',', true, true))
                {
                    if (!StringEqualsOrdinalIgnoreCase(entry, *hostGame))
                    {
                        hostRotation.emplace_back(entry, hostMode);
                    }
                }
            }
            WriteLine("[net] asking " + masterHost + ":" + std::to_string(masterPort)
                + " to run " + *hostGame
                + (hostRotation.size() > 1
                    ? " and " + std::to_string(hostRotation.size() - 1) + " more"
                    : ""));
            const auto game = Network::NetMasterClient::RequestGame(masterHost,
                masterPort, *hostGame, hostMode, 420, 7,
                Entities::PlayerEntity::SlotCapacity, hostName + "'s game",
                Network::NetMasterConfig::HostedRequestTimeoutMs, hostRotation);
            if (!game.Started)
            {
                WriteLine("[net] it would not: " + game.Reason);
                SetExitCode(1);
                return true;
            }
            WriteLine("[net] running on " + game.Host + ":" + std::to_string(game.Port)
                + "; joining it");
            Network::NetConnectCommand::Run(game.Host, game.Port, hostName,
                ParseHunter(args), ParseRecolor(args));
            return true;
        }

        const std::optional<std::string> connect = ValueAfter(args, "connect");
        if (connect.has_value())
        {
            Network::NetConnectCommand::Run(*connect, ParsePort(args), ParseName(args),
                ParseHunter(args), ParseRecolor(args));
            return true;
        }

        const std::optional<std::string> check = ValueAfter(args, "netcheck");
        if (check.has_value())
        {
            const std::optional<std::string> shots = ValueAfter(args, "shots");
            double seconds = 30;
            double parsedSeconds = 0;
            if (TryParseDoubleInvariant(ValueAfter(args, "seconds"), parsedSeconds))
            {
                seconds = parsedSeconds;
            }
            double spectateAt = -1;
            double rejoinAt = -1;
            if (HasFlag(args, "spectate"))
            {
                spectateAt = 0;
                double parsedSpectate = 0;
                if (TryParseDoubleInvariant(ValueAfter(args, "spectate"), parsedSpectate))
                {
                    spectateAt = parsedSpectate;
                }
            }
            double parsedRejoin = 0;
            if (TryParseDoubleInvariant(ValueAfter(args, "rejoin"), parsedRejoin))
            {
                rejoinAt = parsedRejoin;
            }
            const bool showHudWindow = HasFlag(args, "hudshots");
            Network::NetCheckClient::SetShowWindow(showHudWindow);
            if (HasFlag(args, "ai"))
            {
                std::int32_t aiLevel = Network::ServerBots::DefaultLevel;
                if (const std::optional<std::string> value = ValueAfter(args, "ai"); value.has_value())
                {
                    static_cast<void>(Int32TryParseCurrentCulture(*value, aiLevel));
                }
                Network::NetCheckClient::SetAiLevel(aiLevel);
            }
            Network::NetCheckClient::SetHeadless(HasFlag(args, "headless") && !showHudWindow);
            if (const std::optional<std::string> value = ValueAfter(args, "drawrate"); value.has_value())
            {
                std::int32_t drawRate = 1;
                static_cast<void>(Int32TryParseCurrentCulture(*value, drawRate));
                Network::NetCheckClient::SetDrawRate(drawRate);
            }
            const std::optional<std::string> mapVote = ValueAfter(args, "mapvote");
            std::int32_t mapVoteRow = -1;
            if (mapVote.has_value() && Int32TryParseCurrentCulture(*mapVote, mapVoteRow))
            {
                Network::NetCheckClient::SetMapVoteRow(std::max(0, mapVoteRow));
            }
            const int color = ValueAfter(args, "recolor").has_value()
                ? ParseRecolor(args)
                : -1;
            SetExitCode(Network::NetCheckClient::Run(*check, ParsePort(args),
                ParseName(args), ParseHunter(args), seconds, shots, width, height,
                HasFlag(args, "recorddemo"), spectateAt, rejoinAt, color));
            return true;
        }

        const std::optional<std::string> demoInfo = ValueAfter(args, "demoinfo");
        if (demoInfo.has_value())
        {
            SetExitCode(Network::DemoInfo::Print(*demoInfo, HasFlag(args, "replay")));
            return true;
        }
        return false;
    }
}
