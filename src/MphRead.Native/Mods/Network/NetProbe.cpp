#include "NetProbe.hpp"

#include <cstdint>
#include <exception>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace MphRead::Mods::Network::Detail
{
    enum class NetProbeAddressFamily : std::uint8_t
    {
        InterNetwork,
        Other
    };

    struct NetProbeAddress
    {
        NetProbeAddressFamily Family;
        std::string Text;
    };

    struct NetProbeEndPoint
    {
        NetProbeAddress Address;
        std::int32_t Port;
    };

    using NetProbeSocketHandle = std::uintptr_t;

    enum class NetProbeSocketError : std::uint8_t
    {
        TimedOut,
        Other
    };

    class NetProbeSocketException final : public std::runtime_error
    {
    public:
        NetProbeSocketException(NetProbeSocketError error, std::string message)
            : std::runtime_error(std::move(message)), _error(error)
        {
        }

        [[nodiscard]] NetProbeSocketError ErrorCode() const noexcept
        {
            return _error;
        }

    private:
        NetProbeSocketError _error;
    };

    // Narrow integration boundary for the C# APIs and protocol declarations
    // this isolated port touches. NetProtocol.cs is not yet present in Native,
    // and C++20 has no standard DNS/UDP API, so this pair declares rather than
    // duplicates those owners. Implementations must preserve the corresponding
    // System.Net/System.Net.Sockets/DateTime semantics and exception messages.
    std::vector<NetProbeAddress> NetProbeDnsGetHostAddresses(const std::string& address);
    NetProbeEndPoint NetProbeCreateIPEndPoint(const NetProbeAddress& address, std::int32_t port);
    NetProbeEndPoint NetProbeCreateIPv4AnyEndPoint();

    NetProbeSocketHandle NetProbeUdpClientCreateInterNetwork();
    void NetProbeUdpClientSetReceiveTimeout(NetProbeSocketHandle socket, std::int32_t timeoutMs);
    void NetProbeUdpClientSend(NetProbeSocketHandle socket, const std::uint8_t* data,
        std::int32_t length, const NetProbeEndPoint& endPoint);
    std::vector<std::uint8_t> NetProbeUdpClientReceive(NetProbeSocketHandle socket,
        NetProbeEndPoint& from);
    void NetProbeUdpClientDispose(NetProbeSocketHandle socket);

    std::int64_t NetProbeDateTimeUtcNowTicks();

    std::uint8_t NetProbePacketTypeHello();
    std::uint8_t NetProbePacketTypeWelcome();
    std::uint8_t NetProbePacketTypeBye();
    std::uint8_t NetProbePacketTypeAuthority();
    std::int32_t NetProbeProtocolVersion();
}

namespace
{
    using MphRead::Mods::Network::NetProbeResult;

    template <typename TBody, typename TFinally>
    NetProbeResult CSharpTryFinally(TBody&& body, TFinally&& finalizer)
    {
        std::optional<NetProbeResult> result;
        std::exception_ptr bodyException;
        try
        {
            result.emplace(body());
        }
        catch (...)
        {
            bodyException = std::current_exception();
        }

        // A C# finally runs for both returns and exceptions. If the finalizer
        // itself throws, that exception replaces the pending result/exception.
        finalizer();

        if (bodyException != nullptr)
        {
            std::rethrow_exception(bodyException);
        }
        return std::move(*result);
    }

    std::string EndPointPrefix(
        const MphRead::Mods::Network::Detail::NetProbeEndPoint& endPoint,
        std::int32_t port)
    {
        return endPoint.Address.Text + ":" + std::to_string(port);
    }
}

namespace MphRead::Mods::Network
{
    NetProbeResult NetProbe::Probe(const std::string& address, std::int32_t port,
        std::int32_t timeoutMs)
    {
        Detail::NetProbeEndPoint endPoint;
        try
        {
            std::vector<Detail::NetProbeAddress> resolved
                = Detail::NetProbeDnsGetHostAddresses(address);
            if (resolved.empty())
            {
                return {false, "Could not resolve " + address + "."};
            }

            const Detail::NetProbeAddress* ipv4 = nullptr;
            for (const Detail::NetProbeAddress& candidate : resolved)
            {
                if (candidate.Family == Detail::NetProbeAddressFamily::InterNetwork)
                {
                    ipv4 = &candidate;
                    break;
                }
            }
            if (ipv4 == nullptr)
            {
                return {false, address + " has no IPv4 address."};
            }
            endPoint = Detail::NetProbeCreateIPEndPoint(*ipv4, port);
        }
        catch (const std::exception& ex)
        {
            return {false, "Could not resolve " + address + ": " + ex.what()};
        }

        const Detail::NetProbeSocketHandle socket
            = Detail::NetProbeUdpClientCreateInterNetwork();
        return CSharpTryFinally(
            [&]() -> NetProbeResult
            {
                // This assignment is deliberately outside the socket-operation
                // try/catch, matching the C# source. A setter failure propagates,
                // but the using-scope still disposes the socket.
                Detail::NetProbeUdpClientSetReceiveTimeout(socket, timeoutMs);

                try
                {
                    const std::uint8_t hello[] = {
                        Detail::NetProbePacketTypeHello(),
                        static_cast<std::uint8_t>(Detail::NetProbeProtocolVersion())
                    };
                    Detail::NetProbeUdpClientSend(socket, hello, 2, endPoint);

                    Detail::NetProbeEndPoint from = Detail::NetProbeCreateIPv4AnyEndPoint();
                    constexpr std::int64_t ticksPerMillisecond = 10'000;
                    const std::int64_t deadline = Detail::NetProbeDateTimeUtcNowTicks()
                        + static_cast<std::int64_t>(timeoutMs) * ticksPerMillisecond;

                    while (Detail::NetProbeDateTimeUtcNowTicks() < deadline)
                    {
                        std::vector<std::uint8_t> reply
                            = Detail::NetProbeUdpClientReceive(socket, from);
                        if (reply.size() >= 2
                            && reply[0] == Detail::NetProbePacketTypeWelcome())
                        {
                            const std::uint8_t bye[] = {Detail::NetProbePacketTypeBye()};
                            Detail::NetProbeUdpClientSend(socket, bye, 1, endPoint);
                            return {true, "Connected to " + EndPointPrefix(endPoint, port)
                                + " -- server assigned slot "
                                + std::to_string(reply[1]) + "."};
                        }

                        if (!reply.empty()
                            && reply[0] >= Detail::NetProbePacketTypeHello()
                            && reply[0] <= Detail::NetProbePacketTypeAuthority())
                        {
                            continue;
                        }
                        return {false, EndPointPrefix(endPoint, port)
                            + " replied, but not as an MphRead server."};
                    }

                    const std::uint8_t bye[] = {Detail::NetProbePacketTypeBye()};
                    Detail::NetProbeUdpClientSend(socket, bye, 1, endPoint);
                    return {false, EndPointPrefix(endPoint, port)
                        + " answered but never assigned a slot."};
                }
                catch (const Detail::NetProbeSocketException& ex)
                {
                    if (ex.ErrorCode() == Detail::NetProbeSocketError::TimedOut)
                    {
                        return {false, "No reply from " + EndPointPrefix(endPoint, port)
                            + ". The server may be down, or UDP " + std::to_string(port)
                            + " may be blocked by a firewall."};
                    }
                    return {false, "Could not reach " + EndPointPrefix(endPoint, port)
                        + ": " + ex.what()};
                }
                catch (const std::exception& ex)
                {
                    return {false, "Could not reach " + EndPointPrefix(endPoint, port)
                        + ": " + ex.what()};
                }
            },
            [&]()
            {
                Detail::NetProbeUdpClientDispose(socket);
            });
    }
}
