#include "NetStatus.hpp"

#include "NetProtocol.hpp"
#include "../../Formats/Formats.hpp"
#include "../../Metadata/Metadata.hpp"

#include <array>
#include <bit>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <exception>
#include <memory>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <cerrno>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>
#endif

namespace MphRead::Mods::Network::Detail
{
    enum class NetStatusAddressFamily : std::uint8_t
    {
        InterNetwork,
        Other
    };

    struct NetStatusAddress
    {
        NetStatusAddressFamily Family = NetStatusAddressFamily::Other;
        std::array<std::uint8_t, 4> Bytes{};
    };

    struct NetStatusEndPoint
    {
        NetStatusAddress Address{};
        std::int32_t Port = 0;
    };

#if defined(_WIN32)
    using NetStatusNativeSocket = SOCKET;
    constexpr NetStatusNativeSocket NetStatusInvalidSocket = INVALID_SOCKET;
#else
    using NetStatusNativeSocket = int;
    constexpr NetStatusNativeSocket NetStatusInvalidSocket = -1;
#endif

    struct NetStatusSocketState
    {
        NetStatusNativeSocket Native = NetStatusInvalidSocket;
        bool BroadcastEnabled = false;
    };

    class NetStatusSocketException final : public std::system_error
    {
    public:
        NetStatusSocketException(int error, std::string operation)
#if defined(_WIN32)
            : std::system_error(error, std::system_category(), std::move(operation))
#else
            : std::system_error(error, std::generic_category(), std::move(operation))
#endif
        {
        }
    };

    [[nodiscard]] int NetStatusLastSocketError() noexcept
    {
#if defined(_WIN32)
        return WSAGetLastError();
#else
        return errno;
#endif
    }

#if defined(_WIN32)
    class NetStatusWinsockRuntime final
    {
    public:
        NetStatusWinsockRuntime()
        {
            WSADATA data{};
            const int result = WSAStartup(MAKEWORD(2, 2), &data);
            if (result != 0)
            {
                throw NetStatusSocketException(result, "WSAStartup");
            }
        }

        ~NetStatusWinsockRuntime()
        {
            WSACleanup();
        }

        NetStatusWinsockRuntime(const NetStatusWinsockRuntime&) = delete;
        NetStatusWinsockRuntime& operator=(const NetStatusWinsockRuntime&) = delete;
    };

    void NetStatusEnsureWinsock()
    {
        static NetStatusWinsockRuntime runtime;
        (void)runtime;
    }
#else
    void NetStatusEnsureWinsock()
    {
    }
#endif

    [[nodiscard]] sockaddr_in NetStatusToSockAddr(
        const NetStatusEndPoint& endPoint) noexcept
    {
        sockaddr_in address{};
        address.sin_family = AF_INET;
        std::memcpy(&address.sin_addr.s_addr,
            endPoint.Address.Bytes.data(), endPoint.Address.Bytes.size());
        address.sin_port = htons(static_cast<std::uint16_t>(endPoint.Port));
        return address;
    }

    std::vector<NetStatusAddress> NetStatusDnsGetHostAddresses(
        const std::string& address)
    {
        NetStatusEnsureWinsock();

        addrinfo hints{};
        hints.ai_family = AF_UNSPEC;
        addrinfo* raw = nullptr;
        const int result = getaddrinfo(address.c_str(), nullptr, &hints, &raw);
        if (result != 0)
        {
#if defined(_WIN32)
            const char* message = gai_strerrorA(result);
#else
            const char* message = gai_strerror(result);
#endif
            throw std::runtime_error(message != nullptr ? message : "getaddrinfo failed");
        }

        std::unique_ptr<addrinfo, decltype(&freeaddrinfo)> owner(raw, &freeaddrinfo);
        std::vector<NetStatusAddress> resolved;
        for (addrinfo* current = raw; current != nullptr; current = current->ai_next)
        {
            if (current->ai_family != AF_INET || current->ai_addr == nullptr
                || current->ai_addrlen
                    < static_cast<decltype(current->ai_addrlen)>(sizeof(sockaddr_in)))
            {
                continue;
            }

            const auto* ipv4 = reinterpret_cast<const sockaddr_in*>(current->ai_addr);
            NetStatusAddress item;
            item.Family = NetStatusAddressFamily::InterNetwork;
            std::memcpy(item.Bytes.data(), &ipv4->sin_addr.s_addr, item.Bytes.size());
            resolved.push_back(item);
        }
        return resolved;
    }

    NetStatusEndPoint NetStatusCreateIPEndPoint(
        const NetStatusAddress& address, std::int32_t port)
    {
        if (port < 0 || port > 65535)
        {
            throw std::out_of_range("port");
        }
        return NetStatusEndPoint{address, port};
    }

    NetStatusEndPoint NetStatusCreateIPv4AnyEndPoint()
    {
        NetStatusAddress address;
        address.Family = NetStatusAddressFamily::InterNetwork;
        address.Bytes = {0, 0, 0, 0};
        return NetStatusEndPoint{address, 0};
    }

    NetStatusSocketHandle NetStatusUdpClientCreateInterNetwork()
    {
        auto state = std::make_unique<NetStatusSocketState>();
        NetStatusEnsureWinsock();
        state->Native = ::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        if (state->Native == NetStatusInvalidSocket)
        {
            throw NetStatusSocketException(
                NetStatusLastSocketError(), "socket");
        }
        return state.release();
    }

    void NetStatusUdpClientSetReceiveTimeout(
        NetStatusSocketHandle socket, std::int32_t timeoutMs)
    {
        if (timeoutMs < -1)
        {
            throw std::out_of_range("timeoutMs");
        }
        const std::int32_t value = timeoutMs == -1 ? 0 : timeoutMs;
        const NetStatusNativeSocket native = socket->Native;
#if defined(_WIN32)
        const DWORD timeout = static_cast<DWORD>(value);
        if (setsockopt(native, SOL_SOCKET, SO_RCVTIMEO,
                reinterpret_cast<const char*>(&timeout), sizeof(timeout)) == SOCKET_ERROR)
        {
            throw NetStatusSocketException(
                NetStatusLastSocketError(), "setsockopt(SO_RCVTIMEO)");
        }
#else
        timeval timeout{};
        timeout.tv_sec = value / 1000;
        timeout.tv_usec = (value % 1000) * 1000;
        if (setsockopt(native, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) != 0)
        {
            throw NetStatusSocketException(
                NetStatusLastSocketError(), "setsockopt(SO_RCVTIMEO)");
        }
#endif
    }

    void NetStatusUdpClientSend(NetStatusSocketHandle socket,
        const std::uint8_t* data, std::int32_t length,
        const NetStatusEndPoint& endPoint)
    {
        const NetStatusNativeSocket native = socket->Native;
        const bool broadcast = endPoint.Address.Family == NetStatusAddressFamily::InterNetwork
            && endPoint.Address.Bytes[0] == 0xFFU
            && endPoint.Address.Bytes[1] == 0xFFU
            && endPoint.Address.Bytes[2] == 0xFFU
            && endPoint.Address.Bytes[3] == 0xFFU;
        if (broadcast && !socket->BroadcastEnabled)
        {
            // UdpClient.CheckForBroadcast marks the instance before setting the
            // socket option, and never retries that option on later sends.
            socket->BroadcastEnabled = true;
#if defined(_WIN32)
            const BOOL enabled = TRUE;
            if (setsockopt(native, SOL_SOCKET, SO_BROADCAST,
                    reinterpret_cast<const char*>(&enabled), sizeof(enabled)) == SOCKET_ERROR)
#else
            const int enabled = 1;
            if (setsockopt(native, SOL_SOCKET, SO_BROADCAST,
                    &enabled, sizeof(enabled)) != 0)
#endif
            {
                throw NetStatusSocketException(
                    NetStatusLastSocketError(), "setsockopt(SO_BROADCAST)");
            }
        }

        const sockaddr_in target = NetStatusToSockAddr(endPoint);
#if defined(_WIN32)
        const int sent = sendto(native,
            reinterpret_cast<const char*>(data), length, 0,
            reinterpret_cast<const sockaddr*>(&target), sizeof(target));
        if (sent == SOCKET_ERROR)
#else
        const ssize_t sent = sendto(native,
            data, static_cast<std::size_t>(length), 0,
            reinterpret_cast<const sockaddr*>(&target), sizeof(target));
        if (sent < 0)
#endif
        {
            throw NetStatusSocketException(
                NetStatusLastSocketError(), "sendto");
        }
    }

    std::vector<std::uint8_t> NetStatusUdpClientReceive(
        NetStatusSocketHandle socket, NetStatusEndPoint& from)
    {
        const NetStatusNativeSocket native = socket->Native;
        std::vector<std::uint8_t> data(65535);
        sockaddr_in sender{};
#if defined(_WIN32)
        int senderLength = sizeof(sender);
        const int received = recvfrom(native,
            reinterpret_cast<char*>(data.data()), static_cast<int>(data.size()), 0,
            reinterpret_cast<sockaddr*>(&sender), &senderLength);
        if (received == SOCKET_ERROR)
#else
        socklen_t senderLength = sizeof(sender);
        const ssize_t received = recvfrom(native,
            data.data(), data.size(), 0,
            reinterpret_cast<sockaddr*>(&sender), &senderLength);
        if (received < 0)
#endif
        {
            throw NetStatusSocketException(
                NetStatusLastSocketError(), "recvfrom");
        }

        data.resize(static_cast<std::size_t>(received));
        from.Address.Family = NetStatusAddressFamily::InterNetwork;
        std::memcpy(from.Address.Bytes.data(),
            &sender.sin_addr.s_addr, from.Address.Bytes.size());
        from.Port = static_cast<std::int32_t>(ntohs(sender.sin_port));
        return data;
    }

    void NetStatusUdpClientDispose(NetStatusSocketHandle socket)
    {
        std::unique_ptr<NetStatusSocketState> state(socket);
        if (!state || state->Native == NetStatusInvalidSocket)
        {
            return;
        }
#if defined(_WIN32)
        (void)closesocket(state->Native);
#else
        (void)::close(state->Native);
#endif
        state->Native = NetStatusInvalidSocket;
    }

    std::int64_t NetStatusDateTimeUtcNowTicks()
    {
        constexpr std::int64_t unixEpochTicks = 621355968000000000LL;
        const auto sinceUnixEpoch = std::chrono::system_clock::now().time_since_epoch();
        const std::int64_t ticks = std::chrono::duration_cast<std::chrono::nanoseconds>(
            sinceUnixEpoch).count() / 100;
        return unixEpochTicks + ticks;
    }

    std::int32_t NetStatusProtocolVersion()
    {
        return NetConfig::ProtocolVersion;
    }

    bool NetStatusIsDefinedGameMode(std::int32_t value)
    {
        switch (value)
        {
            case static_cast<std::int32_t>(GameMode::None):
            case static_cast<std::int32_t>(GameMode::SinglePlayer):
            case static_cast<std::int32_t>(GameMode::Battle):
            case static_cast<std::int32_t>(GameMode::BattleTeams):
            case static_cast<std::int32_t>(GameMode::Survival):
            case static_cast<std::int32_t>(GameMode::SurvivalTeams):
            case static_cast<std::int32_t>(GameMode::Capture):
            case static_cast<std::int32_t>(GameMode::Bounty):
            case static_cast<std::int32_t>(GameMode::BountyTeams):
            case static_cast<std::int32_t>(GameMode::Nodes):
            case static_cast<std::int32_t>(GameMode::NodesTeams):
            case static_cast<std::int32_t>(GameMode::Defender):
            case static_cast<std::int32_t>(GameMode::DefenderTeams):
            case static_cast<std::int32_t>(GameMode::PrimeHunter):
            case static_cast<std::int32_t>(GameMode::Unknown15):
                return true;
        }
        return false;
    }

    GameMode NetStatusBattleGameMode()
    {
        return GameMode::Battle;
    }

    std::string NetStatusGameModeToString(GameMode mode)
    {
        switch (mode)
        {
            case GameMode::None: return "None";
            case GameMode::SinglePlayer: return "SinglePlayer";
            case GameMode::Battle: return "Battle";
            case GameMode::BattleTeams: return "BattleTeams";
            case GameMode::Survival: return "Survival";
            case GameMode::SurvivalTeams: return "SurvivalTeams";
            case GameMode::Capture: return "Capture";
            case GameMode::Bounty: return "Bounty";
            case GameMode::BountyTeams: return "BountyTeams";
            case GameMode::Nodes: return "Nodes";
            case GameMode::NodesTeams: return "NodesTeams";
            case GameMode::Defender: return "Defender";
            case GameMode::DefenderTeams: return "DefenderTeams";
            case GameMode::PrimeHunter: return "PrimeHunter";
            case GameMode::Unknown15: return "Unknown15";
        }
        return std::to_string(
            static_cast<unsigned int>(static_cast<std::uint8_t>(mode)));
    }
}
namespace
{
    using MphRead::Mods::Network::ServerStatus;

    template <typename TBody, typename TFinally>
    ServerStatus CSharpTryFinally(TBody&& body, TFinally&& finalizer)
    {
        std::optional<ServerStatus> result;
        std::exception_ptr bodyException;
        try
        {
            result.emplace(body());
        }
        catch (...)
        {
            bodyException = std::current_exception();
        }

        // A C# using declaration lowers to try/finally. The finalizer runs on
        // both return and throw, and a finalizer failure replaces the pending
        // result or exception.
        finalizer();

        if (bodyException != nullptr)
        {
            std::rethrow_exception(bodyException);
        }
        return std::move(*result);
    }

    [[nodiscard]] bool IsManagedWhiteSpace(std::uint32_t codePoint) noexcept
    {
        return (codePoint >= 0x0009U && codePoint <= 0x000DU)
            || codePoint == 0x0020U
            || codePoint == 0x0085U
            || codePoint == 0x00A0U
            || codePoint == 0x1680U
            || (codePoint >= 0x2000U && codePoint <= 0x200AU)
            || codePoint == 0x2028U
            || codePoint == 0x2029U
            || codePoint == 0x202FU
            || codePoint == 0x205FU
            || codePoint == 0x3000U;
    }

    [[nodiscard]] bool IsNullOrWhiteSpace(const std::string& value) noexcept
    {
        if (value.empty())
        {
            return true;
        }

        for (std::size_t index = 0; index < value.size();)
        {
            const auto first = static_cast<unsigned char>(value[index]);
            std::uint32_t codePoint = 0;
            std::size_t consumed = 1;
            bool valid = true;

            if (first < 0x80U)
            {
                codePoint = first;
            }
            else if (first >= 0xC2U && first <= 0xDFU
                && index + 1 < value.size())
            {
                const auto b1 = static_cast<unsigned char>(value[index + 1]);
                if ((b1 & 0xC0U) != 0x80U)
                {
                    valid = false;
                }
                else
                {
                    codePoint = (static_cast<std::uint32_t>(first & 0x1FU) << 6)
                        | static_cast<std::uint32_t>(b1 & 0x3FU);
                    consumed = 2;
                }
            }
            else if (first >= 0xE0U && first <= 0xEFU
                && index + 2 < value.size())
            {
                const auto b1 = static_cast<unsigned char>(value[index + 1]);
                const auto b2 = static_cast<unsigned char>(value[index + 2]);
                const bool continuation = (b1 & 0xC0U) == 0x80U
                    && (b2 & 0xC0U) == 0x80U;
                const bool notOverlong = first != 0xE0U || b1 >= 0xA0U;
                const bool notSurrogate = first != 0xEDU || b1 <= 0x9FU;
                if (!continuation || !notOverlong || !notSurrogate)
                {
                    valid = false;
                }
                else
                {
                    codePoint = (static_cast<std::uint32_t>(first & 0x0FU) << 12)
                        | (static_cast<std::uint32_t>(b1 & 0x3FU) << 6)
                        | static_cast<std::uint32_t>(b2 & 0x3FU);
                    consumed = 3;
                }
            }
            else if (first >= 0xF0U && first <= 0xF4U
                && index + 3 < value.size())
            {
                const auto b1 = static_cast<unsigned char>(value[index + 1]);
                const auto b2 = static_cast<unsigned char>(value[index + 2]);
                const auto b3 = static_cast<unsigned char>(value[index + 3]);
                const bool continuation = (b1 & 0xC0U) == 0x80U
                    && (b2 & 0xC0U) == 0x80U
                    && (b3 & 0xC0U) == 0x80U;
                const bool notOverlong = first != 0xF0U || b1 >= 0x90U;
                const bool inRange = first != 0xF4U || b1 <= 0x8FU;
                if (!continuation || !notOverlong || !inRange)
                {
                    valid = false;
                }
                else
                {
                    codePoint = (static_cast<std::uint32_t>(first & 0x07U) << 18)
                        | (static_cast<std::uint32_t>(b1 & 0x3FU) << 12)
                        | (static_cast<std::uint32_t>(b2 & 0x3FU) << 6)
                        | static_cast<std::uint32_t>(b3 & 0x3FU);
                    consumed = 4;
                }
            }
            else
            {
                valid = false;
            }

            // A non-ASCII scalar above U+FFFF occupies surrogate chars in the
            // managed string, and neither surrogate is whitespace.
            if (!valid || codePoint > 0xFFFFU || !IsManagedWhiteSpace(codePoint))
            {
                return false;
            }
            index += consumed;
        }
        return true;
    }

    [[nodiscard]] std::int32_t StopwatchElapsedMilliseconds(
        std::chrono::steady_clock::time_point started) noexcept
    {
        const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - started).count();
        const std::uint32_t low = static_cast<std::uint32_t>(
            static_cast<std::uint64_t>(elapsed));
        return std::bit_cast<std::int32_t>(low);
    }
}

namespace MphRead::Mods::Network
{
    ServerStatus ServerStatus::Offline(const std::string& message)
    {
        ServerStatus status;
        status.RoomKey = "";
        status.ServerName = "";
        status.Latency = -1;
        status.Message = message;
        return status;
    }

    ServerStatus NetStatus::Query(const std::string& address, std::int32_t port,
        bool allowJoinProbe, std::int32_t timeoutMs)
    {
        if (IsNullOrWhiteSpace(address))
        {
            return ServerStatus::Offline("No server address.");
        }

        Detail::NetStatusEndPoint endPoint;
        try
        {
            std::vector<Detail::NetStatusAddress> resolved
                = Detail::NetStatusDnsGetHostAddresses(address);
            const Detail::NetStatusAddress* ipv4 = nullptr;
            for (const Detail::NetStatusAddress& candidate : resolved)
            {
                if (candidate.Family == Detail::NetStatusAddressFamily::InterNetwork)
                {
                    ipv4 = &candidate;
                    break;
                }
            }
            if (ipv4 == nullptr)
            {
                return ServerStatus::Offline("Cannot find " + address + ".");
            }
            endPoint = Detail::NetStatusCreateIPEndPoint(*ipv4, port);
        }
        catch (...)
        {
            return ServerStatus::Offline("Cannot find " + address + ".");
        }

        const Detail::NetStatusSocketHandle socket
            = Detail::NetStatusUdpClientCreateInterNetwork();
        return CSharpTryFinally(
            [&]() -> ServerStatus
            {
                // Deliberately outside the operation try/catch, like the C#
                // ReceiveTimeout assignment. A setter failure propagates.
                Detail::NetStatusUdpClientSetReceiveTimeout(socket, timeoutMs);

                try
                {
                    const auto started = std::chrono::steady_clock::now();
                    const std::uint8_t query[] = {
                        static_cast<std::uint8_t>(PacketType::StatusQuery),
                        static_cast<std::uint8_t>(Detail::NetStatusProtocolVersion())
                    };
                    Detail::NetStatusUdpClientSend(socket, query, 2, endPoint);

                    Detail::NetStatusEndPoint from
                        = Detail::NetStatusCreateIPv4AnyEndPoint();
                    constexpr std::int64_t ticksPerMillisecond = 10'000;
                    const std::int64_t deadline = Detail::NetStatusDateTimeUtcNowTicks()
                        + static_cast<std::int64_t>(timeoutMs) * ticksPerMillisecond;
                    while (Detail::NetStatusDateTimeUtcNowTicks() < deadline)
                    {
                        std::vector<std::uint8_t> reply
                            = Detail::NetStatusUdpClientReceive(socket, from);
                        if (reply.size() >= 1U
                                + static_cast<std::size_t>(ServerStatusPacket::Size)
                            && reply[0]
                                == static_cast<std::uint8_t>(PacketType::StatusReply))
                        {
                            ServerStatusPacket status = ServerStatusPacket::Read(
                                std::span<const std::uint8_t>(reply).subspan(1));
                            const std::int32_t latency
                                = StopwatchElapsedMilliseconds(started);
                            return Describe(std::move(status), false, latency);
                        }
                    }
                }
                catch (const Detail::NetStatusSocketException&)
                {
                }
                catch (const std::exception& ex)
                {
                    return ServerStatus::Offline(
                        "Cannot reach " + address + ": " + ex.what());
                }

                return allowJoinProbe
                    ? JoinProbe(socket, endPoint, address, timeoutMs)
                    : ServerStatus::Offline(
                        "No answer from " + address + ":" + std::to_string(port) + ".");
            },
            [&]()
            {
                Detail::NetStatusUdpClientDispose(socket);
            });
    }

    ServerStatus NetStatus::JoinProbe(Detail::NetStatusSocketHandle socket,
        const Detail::NetStatusEndPoint& endPoint, const std::string& address,
        std::int32_t timeoutMs)
    {
        try
        {
            const std::uint8_t hello[] = {
                static_cast<std::uint8_t>(PacketType::Hello),
                static_cast<std::uint8_t>(Detail::NetStatusProtocolVersion()),
                0xFFU
            };
            Detail::NetStatusUdpClientSend(socket, hello, 3, endPoint);

            Detail::NetStatusEndPoint from
                = Detail::NetStatusCreateIPv4AnyEndPoint();
            constexpr std::int64_t ticksPerMillisecond = 10'000;
            const std::int64_t deadline = Detail::NetStatusDateTimeUtcNowTicks()
                + static_cast<std::int64_t>(timeoutMs) * ticksPerMillisecond;
            bool welcomed = false;
            while (Detail::NetStatusDateTimeUtcNowTicks() < deadline)
            {
                std::vector<std::uint8_t> reply
                    = Detail::NetStatusUdpClientReceive(socket, from);
                if (!reply.empty()
                    && reply[0] == static_cast<std::uint8_t>(PacketType::Welcome))
                {
                    welcomed = true;
                    continue;
                }
                if (reply.size() >= 1U
                        + static_cast<std::size_t>(MatchStatePacket::Size)
                    && reply[0] == static_cast<std::uint8_t>(PacketType::MatchState))
                {
                    const std::uint8_t bye[] = {
                        static_cast<std::uint8_t>(PacketType::Bye)
                    };
                    Detail::NetStatusUdpClientSend(socket, bye, 1, endPoint);

                    MatchStatePacket match = MatchStatePacket::Read(
                        std::span<const std::uint8_t>(reply).subspan(1));
                    if (match.PlayerCount > 0)
                    {
                        --match.PlayerCount;
                    }

                    ServerStatusPacket status;
                    status.Match = std::move(match);
                    status.MaxPlayers = 0;
                    status.ServerName = std::string{};
                    return Describe(std::move(status), true, -1);
                }
            }

            if (welcomed)
            {
                const std::uint8_t bye[] = {
                    static_cast<std::uint8_t>(PacketType::Bye)
                };
                Detail::NetStatusUdpClientSend(socket, bye, 1, endPoint);

                ServerStatus status;
                status.Online = true;
                status.RoomKey = "";
                status.ServerName = "";
                status.Latency = -1;
                status.Legacy = true;
                status.Message
                    = "Online \xC2\xB7 the server did not say what is running.";
                return status;
            }
        }
        catch (...)
        {
        }

        return ServerStatus::Offline(
            "No answer from " + address + ". It may be off, "
            + "or a firewall may be blocking UDP.");
    }

    ServerStatus NetStatus::Describe(
        ServerStatusPacket status, bool legacy, std::int32_t latency)
    {
        MatchStatePacket match = status.Match;
        GameMode mode = Detail::NetStatusIsDefinedGameMode(
                static_cast<std::int32_t>(match.Mode))
            ? static_cast<GameMode>(static_cast<std::int32_t>(match.Mode))
            : Detail::NetStatusBattleGameMode();

        const std::string rawRoom = match.RoomKey.value();
        std::string room = rawRoom;
        if (!room.empty())
        {
            auto [meta, ignored] = Metadata::GetRoomByName(room);
            (void)ignored;
            if (meta != nullptr && meta->InGameName.has_value())
            {
                room = *meta->InGameName;
            }
        }

        std::string players;
        if (status.MaxPlayers > 0)
        {
            players = std::to_string(static_cast<unsigned int>(match.PlayerCount))
                + "/" + std::to_string(static_cast<unsigned int>(status.MaxPlayers))
                + " players";
        }
        else if (match.PlayerCount == 0)
        {
            players = "nobody playing yet";
        }
        else
        {
            players = match.PlayerCount == 1
                ? "1 player"
                : std::to_string(static_cast<unsigned int>(match.PlayerCount))
                    + " players";
        }

        std::string message = !room.empty()
            ? room + " \xC2\xB7 " + ModeName(mode) + " \xC2\xB7 " + players
            : players;
        const std::string serverName = status.ServerName.value_or("");
        if (!serverName.empty())
        {
            message = serverName + " \xC2\xB7 " + message;
        }

        ServerStatus result;
        result.Online = true;
        result.RoomKey = rawRoom;
        result.ServerName = serverName;
        result.Mode = mode;
        result.Players = static_cast<std::int32_t>(match.PlayerCount);
        result.MaxPlayers = static_cast<std::int32_t>(status.MaxPlayers);
        result.TimeRemaining = match.TimeRemaining;
        result.Latency = latency;
        result.Legacy = legacy;
        result.Protocol = static_cast<std::int32_t>(status.Protocol);
        result.Message = std::move(message);
        return result;
    }

    std::string NetStatus::ModeName(GameMode mode)
    {
        const std::string name = Detail::NetStatusGameModeToString(mode);
        std::string builder;
        builder.reserve(name.size() + 4U);
        for (std::size_t index = 0; index < name.size(); ++index)
        {
            const char ch = name[index];
            if (index > 0 && ch >= 'A' && ch <= 'Z')
            {
                builder.push_back(' ');
            }
            builder.push_back(ch);
        }
        return builder;
    }
}

namespace MphRead::Mods::Network::Detail
{
    // Existing NetLaunch.cpp consumes NetStatus through this narrow bridge so
    // this slice can be added without modifying that already-owned file.
    struct NetLaunchServerStatus
    {
        bool Online;
        std::int32_t Players;
        std::int32_t MaxPlayers;
        std::int32_t Protocol;
    };

    NetLaunchServerStatus NetLaunchQueryStatus(
        const std::string& address, std::int32_t port,
        bool allowJoinProbe, std::int32_t timeoutMs)
    {
        const ServerStatus status = NetStatus::Query(
            address, port, allowJoinProbe, timeoutMs);
        return {
            status.Online,
            status.Players,
            status.MaxPlayers,
            status.Protocol
        };
    }
}
