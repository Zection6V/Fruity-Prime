#include "ServerCommands.hpp"

#include "CommandLine.hpp"
#include "OptionValues.hpp"

#include "../../NativeRuntime/System/Encoding.hpp"
#include "../../NativeRuntime/System/Globalization.hpp"
#include "../../NativeRuntime/System/IO.hpp"
#include "../../NativeRuntime/System/Managed.hpp"
#include "../../NativeRuntime/System/Number.hpp"
#include "../../NativeRuntime/System/Runtime.hpp"
#include "../../Program.hpp"
#include "../Branding.hpp"
#include "../ConsoleWindow.hpp"
#include "../Network/Discovery/NetMaster.hpp"
#include "../Network/Bots/ServerBots.hpp"
#include "../Network/Discovery/NetStatus.hpp"
#include "../Network/Protocol/Packets/PacketTypes.hpp"
#include "../Network/Server/Core/DedicatedServer.hpp"
#include "../Network/Server/Core/ServerLaunchOptions.hpp"
#include "../Network/Server/Lobby/ServerMapUpload.hpp"
#include "../Network/Server/Match/MapRotation.hpp"
#include "../Platform/AppPaths.hpp"
#include "../ShutdownSignals.hpp"
#include "../Update/ServerUpdate.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <memory>
#include <mutex>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <stop_token>
#include <string_view>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <conio.h>
#include <windows.h>
#elif defined(__APPLE__)
#include <crt_externs.h>
#endif

using ::MphRead::NativeRuntime::EnvironmentProcessPath;
using ::MphRead::NativeRuntime::Int32TryParseCurrentCulture;
using ::MphRead::NativeRuntime::PathCombine;
using ::MphRead::NativeRuntime::PathFromUtf8;
using ::MphRead::NativeRuntime::PathToUtf8;
using ::MphRead::NativeRuntime::RequireReference;
using ::MphRead::NativeRuntime::StringEqualsOrdinalIgnoreCase;
using ::MphRead::NativeRuntime::WideToWtf8;

namespace
{
    void WriteLine(std::string_view value)
    {
        std::cout << value << '\n';
    }

    [[nodiscard]] std::size_t DotNetUtf16Length(std::string_view value) noexcept
    {
        // Native strings are UTF-8; C# alignment pads by System.String.Length,
        // i.e. UTF-16 code units rather than UTF-8 bytes or Unicode scalars.
        std::size_t units = 0;
        for (std::size_t i = 0; i < value.size();)
        {
            const unsigned char lead = static_cast<unsigned char>(value[i]);
            std::uint32_t codePoint = lead;
            std::size_t length = 1;
            if ((lead & 0xE0) == 0xC0 && i + 1 < value.size())
            {
                codePoint = lead & 0x1F;
                length = 2;
            }
            else if ((lead & 0xF0) == 0xE0 && i + 2 < value.size())
            {
                codePoint = lead & 0x0F;
                length = 3;
            }
            else if ((lead & 0xF8) == 0xF0 && i + 3 < value.size())
            {
                codePoint = lead & 0x07;
                length = 4;
            }
            for (std::size_t j = 1; j < length; ++j)
            {
                const unsigned char next = static_cast<unsigned char>(value[i + j]);
                if ((next & 0xC0) != 0x80)
                {
                    length = 1;
                    codePoint = lead;
                    break;
                }
                codePoint = (codePoint << 6) | (next & 0x3F);
            }
            units += codePoint > 0xFFFF ? 2 : 1;
            i += length;
        }
        return units;
    }

    [[nodiscard]] std::string LeftAlign(std::string_view value, std::size_t width)
    {
        const std::size_t length = DotNetUtf16Length(value);
        if (length >= width)
        {
            return std::string(value);
        }
        std::string result(value);
        result.append(width - length, ' ');
        return result;
    }

    void ListServers(const std::string& masterHost,
        const std::optional<std::string>& portValue)
    {
        using namespace MphRead::Mods::Network;

        int port = NetMasterConfig::DefaultPort;
        std::int32_t parsedPort = 0;
        if (portValue.has_value() && Int32TryParseCurrentCulture(*portValue, parsedPort))
        {
            port = parsedPort;
        }
        WriteLine("[servers] asking " + masterHost + ":" + std::to_string(port));
        const auto result = NetMasterClient::Query(masterHost, port);
        if (!result.Answered)
        {
            WriteLine("[servers] no answer from " + masterHost + ":" + std::to_string(port)
                + " -- it may be down, or UDP may not reach it");
            return;
        }
        if (result.CanHost == true)
        {
            WriteLine("[servers] this directory will start games for players");
        }
        else if (result.CanHost == false)
        {
            WriteLine("[servers] this directory starts no games (no host port range)");
        }
        else
        {
            WriteLine("[servers] this directory is from before it could say whether it "
                "starts games; it is offered anyway, since hosting is the default");
        }
        if (RequireReference(result.Servers).empty())
        {
            WriteLine("[servers] the directory is up and has nobody listed");
            return;
        }
        WriteLine("[servers] " + std::to_string(RequireReference(result.Servers).size())
            + " listed; asking each one");
        for (const auto& listing : RequireReference(result.Servers))
        {
            const auto status = NetStatus::Query(listing.Address, listing.Port,
                false /* allowJoinProbe */);
            std::string name;
            if (!status.ServerName.empty())
            {
                name = status.ServerName;
            }
            else if (!listing.ServerName.empty())
            {
                name = listing.ServerName;
            }
            else
            {
                name = listing.Endpoint();
            }
            if (!status.Online)
            {
                WriteLine("  " + LeftAlign(name, 24) + " "
                    + LeftAlign(listing.Endpoint(), 26) + " did not answer");
                continue;
            }
            const std::string players = status.MaxPlayers > 0
                ? std::to_string(status.Players) + "/" + std::to_string(status.MaxPlayers)
                : std::to_string(status.Players);
            const std::string ping = status.Latency >= 0
                ? std::to_string(status.Latency) + " ms"
                : std::string("-- ms");
            WriteLine("  " + LeftAlign(name, 24) + " "
                + LeftAlign(listing.Endpoint(), 26) + " "
                + LeftAlign(status.RoomKey, 20) + " "
                + LeftAlign(NetStatus::ModeName(status.Mode), 14) + " "
                + LeftAlign(players, 6) + " " + ping);
        }
    }

    [[nodiscard]] std::vector<std::string> GetCommandLineArguments()
    {
#if defined(_WIN32)
        using CommandLineToArgvWFunction = LPWSTR* (WINAPI*)(LPCWSTR, int*);
        HMODULE shell = LoadLibraryW(L"shell32.dll");
        if (shell == nullptr)
        {
            throw std::runtime_error("LoadLibraryW(shell32.dll) failed");
        }
        auto commandLineToArgvW = reinterpret_cast<CommandLineToArgvWFunction>(
            GetProcAddress(shell, "CommandLineToArgvW"));
        if (commandLineToArgvW == nullptr)
        {
            FreeLibrary(shell);
            throw std::runtime_error("CommandLineToArgvW is unavailable");
        }
        int argc = 0;
        LPWSTR* argv = commandLineToArgvW(GetCommandLineW(), &argc);
        if (argv == nullptr)
        {
            FreeLibrary(shell);
            throw std::runtime_error("CommandLineToArgvW failed");
        }
        std::vector<std::string> result;
        result.reserve(static_cast<std::size_t>(argc));
        try
        {
            for (int i = 0; i < argc; ++i)
            {
                result.push_back(WideToWtf8(argv[i]));
            }
        }
        catch (...)
        {
            LocalFree(argv);
            FreeLibrary(shell);
            throw;
        }
        LocalFree(argv);
        FreeLibrary(shell);
        return result;
#elif defined(__APPLE__)
        const int argc = *_NSGetArgc();
        char** argv = *_NSGetArgv();
        std::vector<std::string> result;
        result.reserve(static_cast<std::size_t>(std::max(argc, 0)));
        for (int i = 0; i < argc; ++i)
        {
            result.emplace_back(argv[i] == nullptr ? "" : argv[i]);
        }
        return result;
#else
        std::ifstream input("/proc/self/cmdline", std::ios::binary);
        if (!input)
        {
            throw std::runtime_error("could not read /proc/self/cmdline");
        }
        const std::string data((std::istreambuf_iterator<char>(input)),
            std::istreambuf_iterator<char>());
        std::vector<std::string> result;
        std::size_t start = 0;
        while (start < data.size())
        {
            const std::size_t end = data.find('\0', start);
            if (end == std::string::npos)
            {
                result.emplace_back(data.substr(start));
                break;
            }
            result.emplace_back(data.substr(start, end - start));
            start = end + 1;
        }
        return result;
#endif
    }

    [[maybe_unused, nodiscard]] std::string FileNameWithoutExtension(const std::string& path)
    {
        return PathToUtf8(PathFromUtf8(path).stem());
    }

#if defined(MPHREAD_SERVER)
    void ServerUsage()
    {
        using MphRead::Mods::Branding;
        using MphRead::Mods::ConsoleWindow;
        using MphRead::Mods::Network::NetConfig;
        using MphRead::Mods::Network::NetMasterConfig;

        const std::optional<std::string> processPath = EnvironmentProcessPath();
        std::string executable = processPath.has_value()
            ? FileNameWithoutExtension(*processPath)
            : std::string("MphReadServer");

        WriteLine("");
        WriteLine(std::string(Branding::Name) + " dedicated server. It needs no game files.");
        WriteLine("");
        WriteLine("  " + executable + " -server -port " + std::to_string(NetConfig::DefaultPort)
            + " -players 8 -servername \"My server\"");
        WriteLine("      run a server. Maps come from maprotation.txt, written");
        WriteLine("      beside this program on first run.");
        WriteLine("");
        WriteLine("  " + executable + " -masterserver -port "
            + std::to_string(NetMasterConfig::DefaultPort));
        WriteLine("      run a server directory of your own.");
        WriteLine("");
        WriteLine("  " + executable + " -servers");
        WriteLine("      list the servers that are up right now.");
        WriteLine("");
        WriteLine("A server lists itself on " + std::string(NetMasterConfig::DefaultHost)
            + " so players can find it;");
        WriteLine("-nomaster keeps it off every list. See SERVER.txt.");
        WriteLine("");
#if defined(_WIN32)
        if (ConsoleWindow::OwnsItsConsole())
        {
            WriteLine("Press any key to close this window...");
            (void)_getche();
        }
#endif
    }
#endif
}

namespace MphRead::Mods::Entry
{
    namespace
    {
        bool TryRunMasterServer(const std::vector<std::string>& args)
        {
            if (HasFlag(args, "masterserver"))
            {
                int masterPort = Network::NetMasterConfig::DefaultPort;
                std::optional<std::string> masterPortValue = ValueAfter(args, "port");
                if (!masterPortValue.has_value())
                {
                    masterPortValue = ValueAfter(args, "masterport");
                }
                std::int32_t parsedMasterPort = 0;
                if (masterPortValue.has_value() && Int32TryParseCurrentCulture(*masterPortValue, parsedMasterPort))
                {
                    masterPort = parsedMasterPort;
                }
                Network::MasterServer master(masterPort);
                ShutdownSignals signals;

                const std::string hostPorts = ValueAfter(args, "hostports").value_or("27900-27919");
                if (!StringEqualsOrdinalIgnoreCase(hostPorts, "none"))
                {
                    const std::size_t dash = hostPorts.find('-');
                    std::string firstText;
                    std::string lastText;
                    if (dash == std::string::npos)
                    {
                        firstText = hostPorts;
                    }
                    else
                    {
                        firstText = hostPorts.substr(0, dash);
                        lastText = hostPorts.substr(dash + 1);
                    }
                    std::int32_t first = 0;
                    std::int32_t last = 0;
                    if (dash != std::string::npos && Int32TryParseCurrentCulture(firstText, first)
                        && Int32TryParseCurrentCulture(lastText, last) && first > 0 && last >= first)
                    {
                        master.SetHostPorts(first, last);
                    }
                    else
                    {
                        WriteLine("[master] ignoring -hostports " + hostPorts
                            + " (expected e.g. 27900-27919, or none)");
                    }
                }

                std::optional<std::string> publicHost = ValueAfter(args, "public");
                if (!publicHost.has_value())
                {
                    publicHost = ValueAfter(args, "publicaddress");
                }
                if (publicHost.has_value())
                {
                    master.SetPublicAddress(*publicHost);
                }

                std::stop_source cancel;
                signals.OnShutdown([&cancel, &master]()
                {
                    cancel.request_stop();
                    master.Stop();
                });
                master.Run(cancel.get_token());
                return true;
            }
            return false;
        }

        bool TryRunDirectoryQueries(const std::vector<std::string>& args)
        {
            if (HasFlag(args, "servers"))
            {
                ListServers(ValueAfter(args, "master").value_or(
                    std::string(Network::NetMasterConfig::DefaultHost)),
                    ValueAfter(args, "masterport"));
                return true;
            }

            if (HasFlag(args, "hosts"))
            {
                const std::string askHost = ValueAfter(args, "master").value_or(
                    std::string(Network::NetMasterConfig::DefaultHost));
                std::int32_t askPort = ParseMasterPort(args);
                WriteLine("[hosts] asking " + askHost + ":" + std::to_string(askPort)
                    + " and everyone it names");

                struct HostsQueryState
                {
                    std::vector<Network::HostCandidate> Candidates;
                    std::mutex Mutex;
                    std::condition_variable Finished;
                    bool Done = false;
                };
                const auto state = std::make_shared<HostsQueryState>();
                Network::NetMasterClient::FindHosts(askHost, askPort,
                    [state](Network::HostCandidate candidate)
                    {
                        std::lock_guard lock(state->Mutex);
                        const std::size_t before = state->Candidates.size();
                        Network::NetMasterClient::Merge(state->Candidates, candidate);
                        if (state->Candidates.size() > before)
                        {
                            const std::string endpoint = candidate.Host + ":"
                                + std::to_string(candidate.Port);
                            std::ostringstream row;
                            row << "  " << std::left << std::setw(28) << candidate.Label << ' '
                                << std::setw(26) << endpoint << ' ' << candidate.Describe();
                            WriteLine(row.str());
                        }
                    },
                    [state]()
                    {
                        {
                            std::lock_guard lock(state->Mutex);
                            state->Done = true;
                        }
                        state->Finished.notify_one();
                    });

                {
                    std::unique_lock lock(state->Mutex);
                    (void)state->Finished.wait_for(lock, std::chrono::seconds(20),
                        [&state]() { return state->Done; });
                    std::int32_t usable = 0;
                    for (const Network::HostCandidate& candidate : state->Candidates)
                    {
                        if (candidate.WillHost())
                        {
                            ++usable;
                        }
                    }
                    WriteLine("[hosts] " + std::to_string(usable) + " of "
                        + std::to_string(state->Candidates.size()) + " can run a match");
                }
                return true;
            }
            return false;
        }
    }

    bool TryRunServerCommands(const std::vector<std::string>& args)
    {
#if defined(MPHREAD_SERVER)
        if (args.empty())
        {
            ServerUsage();
            return true;
        }
#endif

        if (HasFlag(args, "masterserver") || HasFlag(args, "server")
            || HasFlag(args, "dedicated"))
        {
            Update::ServerUpdate::Enabled(!HasFlag(args, "noautoupdate"));
            std::vector<std::string> commandLine = GetCommandLineArguments();
            std::vector<std::string> typed;
            if (commandLine.size() > 1)
            {
                typed.assign(commandLine.begin() + 1, commandLine.end());
            }
            if (Update::ServerUpdate::AtStartup(typed))
            {
                return true;
            }
        }

        if (TryRunMasterServer(args))
        {
            return true;
        }

        if (TryRunDirectoryQueries(args))
        {
            return true;
        }

        if (!HasFlag(args, "server") && !HasFlag(args, "dedicated"))
        {
            return false;
        }

        int port = Network::NetConfig::DefaultPort;
        const std::optional<std::string> portValue = ValueAfter(args, "port");
        std::int32_t parsedPort = 0;
        if (portValue.has_value() && Int32TryParseCurrentCulture(*portValue, parsedPort))
        {
            port = parsedPort;
        }
        int maxPlayers = 4;
        const std::optional<std::string> playersValue = ValueAfter(args, "players");
        std::int32_t parsedPlayers = 0;
        if (playersValue.has_value() && Int32TryParseCurrentCulture(*playersValue, parsedPlayers))
        {
            maxPlayers = parsedPlayers;
        }
        std::string rotationPath;
        if (const std::optional<std::string> value = ValueAfter(args, "rotation"); value.has_value())
        {
            rotationPath = *value;
        }
        else
        {
            rotationPath = PathCombine(Mods::Platform::AppPaths::UserDataDirectory(), "maprotation.txt");
        }
        std::shared_ptr<Network::MapRotation> rotation = Network::MapRotation::LoadOrCreate(rotationPath);
        Network::DedicatedServer server(port, maxPlayers, rotation);
        Network::ServerLaunchOptions::Apply(server, args);
        std::string serverName;
        if (const std::optional<std::string> value = ValueAfter(args, "servername"); value.has_value())
        {
            serverName = *value;
        }
        else if (const std::optional<std::string> value = ValueAfter(args, "name"); value.has_value())
        {
            serverName = *value;
        }
        else
        {
            serverName = MachineName();
        }
        server.ServerName(serverName);
        server.FriendlyFire(HasFlag(args, "friendlyfire"));
        server.ShadowFreeze(!HasFlag(args, "noshadowfreeze"));
        server.AffinityWeapons(HasFlag(args, "affinityweapons"));
        // -bots N [-botlevel L]: the game's own AI in the top N slots.
        if (const std::optional<std::string> bots = ValueAfter(args, "bots"); bots.has_value())
        {
            std::int32_t count = 0;
            std::int32_t level = Network::ServerBots::DefaultLevel;
            if (const std::optional<std::string> botLevel = ValueAfter(args, "botlevel"); botLevel.has_value())
            {
                static_cast<void>(Int32TryParseCurrentCulture(*botLevel, level));
            }
            if (Int32TryParseCurrentCulture(*bots, count))
            {
                server.Bots(count, level);
            }
        }
        server.AllowMapVotes(!HasFlag(args, "novote"));
        Network::ServerMapUpload::Enabled(!HasFlag(args, "nomapupload"));
        server.AutoUpdate(true);
        // Whether this server will also open *extra* matches, on ports of its
        // own, for players who ask. Off unless an admin says a range.
        if (const std::optional<std::string> serverHostPorts = ValueAfter(args, "hostports");
            serverHostPorts.has_value() && !NativeRuntime::StringEqualsOrdinalIgnoreCase(*serverHostPorts, "none"))
        {
            const std::vector<std::string> halves = NativeRuntime::StringSplit(*serverHostPorts, '-');
            std::int32_t hostFirst = 0;
            std::int32_t hostLast = 0;
            if (halves.size() == 2 && Int32TryParseCurrentCulture(halves[0], hostFirst)
                && Int32TryParseCurrentCulture(halves[1], hostLast) && hostLast >= hostFirst)
            {
                server.Hosts().SetPorts(hostFirst, hostLast);
            }
            else
            {
                WriteLine("[server] ignoring -hostports " + *serverHostPorts + " (expected FIRST-LAST)");
            }
        }
        // -simulate and -authority are accepted and ignored: a server always
        // runs the match itself now.
        if (HasFlag(args, "simulate") || HasFlag(args, "authority"))
        {
            WriteLine("[net] -simulate is the default now and does "
                "nothing; a server always runs the match itself");
        }

        if (!HasFlag(args, "nomaster") && !HasFlag(args, "unlisted"))
        {
            const std::string masterHost = ValueAfter(args, "master").value_or(
                std::string(Network::NetMasterConfig::DefaultHost));
            int reportPort = Network::NetMasterConfig::DefaultPort;
            const std::optional<std::string> reportPortValue = ValueAfter(args, "masterport");
            std::int32_t parsedReportPort = 0;
            if (reportPortValue.has_value() && Int32TryParseCurrentCulture(*reportPortValue, parsedReportPort))
            {
                reportPort = parsedReportPort;
            }
            server.Reporter(std::make_unique<Network::MasterReporter>(masterHost, reportPort));
            WriteLine("[server] listing on " + masterHost + ":" + std::to_string(reportPort)
                + " as \"" + server.ServerName() + "\" (-nomaster to stay private)");
        }

        std::stop_source cancel;
        ShutdownSignals signals;
        signals.OnShutdown([&cancel, &server]()
        {
            cancel.request_stop();
            server.Stop();
        });
        try
        {
            server.Run(cancel.get_token());
        }
        catch (const ProgramException& ex)
        {
            // The reason and what to do about it are already on the log.
            WriteLine(std::string("[server] ") + ex.what());
            std::exit(1);
        }
        return true;
    }
}
