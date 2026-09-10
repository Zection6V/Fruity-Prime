#include "NetProbe.hpp"

#include <array>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#ifdef _MSC_VER
#pragma comment(lib, "Ws2_32.lib")
#endif
#else
#include <cerrno>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>
#endif

namespace MphRead::Mods::Network::Detail
{
    constexpr std::uint8_t Hello = 1;
    constexpr std::uint8_t Welcome = 2;
    constexpr std::uint8_t Bye = 5;
    constexpr std::uint8_t Authority = 12;
    constexpr std::uint8_t ProtocolVersion = 6;

    struct ResolvedAddress
    {
        bool IsIPv4;
        std::string Text;
    };

    class SocketException final : public std::runtime_error
    {
    public:
        SocketException(std::string message, bool timedOut)
            : std::runtime_error(std::move(message)), _timedOut(timedOut)
        {
        }

        [[nodiscard]] bool TimedOut() const noexcept
        {
            return _timedOut;
        }

    private:
        bool _timedOut;
    };

    class IUdpClient
    {
    public:
        virtual ~IUdpClient() = default;
        virtual void SetReceiveTimeout(std::int32_t timeoutMs) = 0;
        virtual void Send(const std::uint8_t* data, std::size_t length,
                          const std::string& address, std::int32_t port) = 0;
        [[nodiscard]] virtual std::vector<std::uint8_t> Receive() = 0;
    };

    class INetProbePlatform
    {
    public:
        virtual ~INetProbePlatform() = default;
        [[nodiscard]] virtual std::vector<ResolvedAddress> Resolve(
            const std::string& address) = 0;
        [[nodiscard]] virtual std::unique_ptr<IUdpClient> CreateUdpIPv4() = 0;
        [[nodiscard]] virtual std::chrono::system_clock::time_point UtcNow() = 0;
    };

#ifdef _WIN32
    class WinSockRuntime final
    {
    public:
        WinSockRuntime()
        {
            WSADATA data{};
            const int error = WSAStartup(MAKEWORD(2, 2), &data);
            if (error != 0)
            {
                throw std::runtime_error("WSAStartup failed with error " + std::to_string(error));
            }
        }

        ~WinSockRuntime()
        {
            WSACleanup();
        }

        WinSockRuntime(const WinSockRuntime&) = delete;
        WinSockRuntime& operator=(const WinSockRuntime&) = delete;
    };

    void EnsureWinSock()
    {
        static WinSockRuntime runtime;
        (void)runtime;
    }

    [[nodiscard]] std::string SocketErrorMessage(int error)
    {
        char* buffer = nullptr;
        const DWORD flags = FORMAT_MESSAGE_ALLOCATE_BUFFER
            | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS;
        const DWORD length = FormatMessageA(flags, nullptr, static_cast<DWORD>(error),
            MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
            reinterpret_cast<char*>(&buffer), 0, nullptr);
        if (length == 0 || buffer == nullptr)
        {
            return "Socket error " + std::to_string(error);
        }
        std::string message(buffer, length);
        LocalFree(buffer);
        while (!message.empty() && (message.back() == '\r' || message.back() == '\n'))
        {
            message.pop_back();
        }
        return message;
    }

    [[nodiscard]] bool IsTimeoutError(int error) noexcept
    {
        return error == WSAETIMEDOUT;
    }

    using SocketHandle = SOCKET;
    constexpr SocketHandle InvalidSocket = INVALID_SOCKET;

    void CloseSocket(SocketHandle socket) noexcept
    {
        if (socket != InvalidSocket)
        {
            closesocket(socket);
        }
    }
#else
    [[nodiscard]] std::string SocketErrorMessage(int error)
    {
        return std::strerror(error);
    }

    [[nodiscard]] bool IsTimeoutError(int error) noexcept
    {
        return error == EAGAIN || error == EWOULDBLOCK || error == ETIMEDOUT;
    }

    using SocketHandle = int;
    constexpr SocketHandle InvalidSocket = -1;

    void CloseSocket(SocketHandle socket) noexcept
    {
        if (socket != InvalidSocket)
        {
            close(socket);
        }
    }
#endif

    class NativeUdpClient final : public IUdpClient
    {
    public:
        NativeUdpClient()
        {
#ifdef _WIN32
            EnsureWinSock();
#endif
            _socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
            if (_socket == InvalidSocket)
            {
#ifdef _WIN32
                const int error = WSAGetLastError();
#else
                const int error = errno;
#endif
                throw SocketException(SocketErrorMessage(error), IsTimeoutError(error));
            }
        }

        ~NativeUdpClient() override
        {
            CloseSocket(_socket);
        }

        NativeUdpClient(const NativeUdpClient&) = delete;
        NativeUdpClient& operator=(const NativeUdpClient&) = delete;

        void SetReceiveTimeout(std::int32_t timeoutMs) override
        {
            if (timeoutMs < -1)
            {
                throw std::out_of_range(
                    "Specified argument was out of the range of valid values. (Parameter 'value')");
            }
            if (timeoutMs == -1)
            {
                timeoutMs = 0;
            }
#ifdef _WIN32
            const DWORD timeout = static_cast<DWORD>(timeoutMs);
            if (setsockopt(_socket, SOL_SOCKET, SO_RCVTIMEO,
                    reinterpret_cast<const char*>(&timeout), sizeof(timeout)) == SOCKET_ERROR)
            {
                const int error = WSAGetLastError();
                throw SocketException(SocketErrorMessage(error), IsTimeoutError(error));
            }
#else
            timeval timeout{};
            timeout.tv_sec = timeoutMs / 1000;
            timeout.tv_usec = (timeoutMs % 1000) * 1000;
            if (setsockopt(_socket, SOL_SOCKET, SO_RCVTIMEO, &timeout,
                    static_cast<socklen_t>(sizeof(timeout))) != 0)
            {
                const int error = errno;
                throw SocketException(SocketErrorMessage(error), IsTimeoutError(error));
            }
#endif
        }

        void Send(const std::uint8_t* data, std::size_t length,
                  const std::string& address, std::int32_t port) override
        {
            sockaddr_in destination{};
            destination.sin_family = AF_INET;
            destination.sin_port = htons(static_cast<std::uint16_t>(port));
            const int parsed = inet_pton(AF_INET, address.c_str(), &destination.sin_addr);
            if (parsed != 1)
            {
                throw std::runtime_error("Invalid IPv4 endpoint address");
            }
#ifdef _WIN32
            const int sent = sendto(_socket,
                reinterpret_cast<const char*>(data), static_cast<int>(length), 0,
                reinterpret_cast<const sockaddr*>(&destination), sizeof(destination));
            if (sent == SOCKET_ERROR)
            {
                const int error = WSAGetLastError();
                throw SocketException(SocketErrorMessage(error), IsTimeoutError(error));
            }
#else
            const ssize_t sent = sendto(_socket, data, length, 0,
                reinterpret_cast<const sockaddr*>(&destination), sizeof(destination));
            if (sent < 0)
            {
                const int error = errno;
                throw SocketException(SocketErrorMessage(error), IsTimeoutError(error));
            }
#endif
        }

        [[nodiscard]] std::vector<std::uint8_t> Receive() override
        {
            std::array<std::uint8_t, 65536> buffer{};
            sockaddr_storage from{};
#ifdef _WIN32
            int fromLength = sizeof(from);
            const int received = recvfrom(_socket, reinterpret_cast<char*>(buffer.data()),
                static_cast<int>(buffer.size()), 0,
                reinterpret_cast<sockaddr*>(&from), &fromLength);
            if (received == SOCKET_ERROR)
            {
                const int error = WSAGetLastError();
                throw SocketException(SocketErrorMessage(error), IsTimeoutError(error));
            }
#else
            socklen_t fromLength = sizeof(from);
            const ssize_t received = recvfrom(_socket, buffer.data(), buffer.size(), 0,
                reinterpret_cast<sockaddr*>(&from), &fromLength);
            if (received < 0)
            {
                const int error = errno;
                throw SocketException(SocketErrorMessage(error), IsTimeoutError(error));
            }
#endif
            return std::vector<std::uint8_t>(buffer.begin(), buffer.begin() + received);
        }

    private:
        SocketHandle _socket = InvalidSocket;
    };

    [[nodiscard]] bool TryParseDotNetIPv4(
        std::string_view text, std::uint32_t& value) noexcept
    {
        if (text.empty())
        {
            return false;
        }

        std::uint64_t parts[4]{};
        int dotCount = 0;
        std::size_t current = 0;
        while (current < text.size())
        {
            int numberBase = 10;
            std::uint64_t currentValue = 0;
            bool atLeastOneChar = false;
            char ch = text[current];
            if (ch == '0')
            {
                numberBase = 8;
                ++current;
                atLeastOneChar = true;
                if (current < text.size()
                    && (text[current] == 'x' || text[current] == 'X'))
                {
                    numberBase = 16;
                    ++current;
                    atLeastOneChar = false;
                }
            }

            for (; current < text.size(); ++current)
            {
                ch = text[current];
                int digit = -1;
                if ((numberBase == 10 || numberBase == 16) && ch >= '0' && ch <= '9')
                {
                    digit = ch - '0';
                }
                else if (numberBase == 8 && ch >= '0' && ch <= '7')
                {
                    digit = ch - '0';
                }
                else if (numberBase == 16 && ch >= 'a' && ch <= 'f')
                {
                    digit = ch + 10 - 'a';
                }
                else if (numberBase == 16 && ch >= 'A' && ch <= 'F')
                {
                    digit = ch + 10 - 'A';
                }
                else
                {
                    break;
                }

                currentValue = currentValue * static_cast<unsigned int>(numberBase)
                    + static_cast<unsigned int>(digit);
                if (currentValue > 0xFFFFFFFFull)
                {
                    return false;
                }
                atLeastOneChar = true;
            }

            if (current < text.size() && text[current] == '.')
            {
                if (dotCount >= 3 || !atLeastOneChar || currentValue > 0xFF)
                {
                    return false;
                }
                parts[dotCount++] = currentValue;
                ++current;
                continue;
            }
            if (!atLeastOneChar || current != text.size())
            {
                return false;
            }

            parts[dotCount] = currentValue;
            switch (dotCount)
            {
            case 0:
                value = static_cast<std::uint32_t>(parts[0]);
                return true;
            case 1:
                if (parts[1] > 0xFFFFFF)
                {
                    return false;
                }
                value = static_cast<std::uint32_t>((parts[0] << 24) | parts[1]);
                return true;
            case 2:
                if (parts[2] > 0xFFFF)
                {
                    return false;
                }
                value = static_cast<std::uint32_t>(
                    (parts[0] << 24) | (parts[1] << 16) | parts[2]);
                return true;
            case 3:
                if (parts[3] > 0xFF)
                {
                    return false;
                }
                value = static_cast<std::uint32_t>(
                    (parts[0] << 24) | (parts[1] << 16)
                    | (parts[2] << 8) | parts[3]);
                return true;
            default:
                return false;
            }
        }
        return false;
    }

    [[nodiscard]] bool IsUnspecifiedIpLiteral(const std::string& address) noexcept
    {
        std::uint32_t ipv4 = 0;
        if (TryParseDotNetIPv4(address, ipv4) && ipv4 == 0)
        {
            return true;
        }

        in6_addr ipv6{};
        if (inet_pton(AF_INET6, address.c_str(), &ipv6) == 1)
        {
            const auto* bytes = reinterpret_cast<const unsigned char*>(&ipv6);
            for (std::size_t i = 0; i < sizeof(ipv6); ++i)
            {
                if (bytes[i] != 0)
                {
                    return false;
                }
            }
            return true;
        }
        return false;
    }

    void ValidateDnsArgument(const std::string& address)
    {
        if (IsUnspecifiedIpLiteral(address))
        {
            throw std::invalid_argument(
                "IPv4 address 0.0.0.0 and IPv6 address ::0 are unspecified addresses "
                "that cannot be used as a target address. (Parameter 'hostNameOrAddress')");
        }
        if (address.size() > 255 || (address.size() == 255 && address.back() != '.'))
        {
            throw std::out_of_range(
                "The size of hostName is too long. It cannot be longer than 255 characters. "
                "(Parameter 'hostName')");
        }
    }

    class NativeNetProbePlatform final : public INetProbePlatform
    {
    public:
        [[nodiscard]] std::vector<ResolvedAddress> Resolve(
            const std::string& address) override
        {
            ValidateDnsArgument(address);
#ifdef _WIN32
            EnsureWinSock();
#endif
            std::string query = address;
            if (query.empty())
            {
                std::array<char, 256> host{};
                if (gethostname(host.data(), static_cast<int>(host.size())) != 0)
                {
#ifdef _WIN32
                    const int error = WSAGetLastError();
#else
                    const int error = errno;
#endif
                    throw std::runtime_error(SocketErrorMessage(error));
                }
                query = host.data();
            }

            addrinfo hints{};
            hints.ai_family = AF_UNSPEC;
            hints.ai_socktype = SOCK_DGRAM;
            addrinfo* raw = nullptr;
            const int error = getaddrinfo(query.c_str(), nullptr, &hints, &raw);
            if (error != 0)
            {
#ifdef _WIN32
                throw std::runtime_error(gai_strerrorA(error));
#else
                throw std::runtime_error(gai_strerror(error));
#endif
            }

            struct AddrInfoDeleter
            {
                void operator()(addrinfo* value) const noexcept
                {
                    if (value != nullptr)
                    {
                        freeaddrinfo(value);
                    }
                }
            };
            std::unique_ptr<addrinfo, AddrInfoDeleter> results(raw);
            std::vector<ResolvedAddress> resolved;
            for (addrinfo* current = results.get(); current != nullptr; current = current->ai_next)
            {
                if (current->ai_family == AF_INET)
                {
                    const auto* ipv4 = reinterpret_cast<const sockaddr_in*>(current->ai_addr);
                    std::array<char, INET_ADDRSTRLEN> text{};
                    if (inet_ntop(AF_INET, &ipv4->sin_addr, text.data(), text.size()) == nullptr)
                    {
#ifdef _WIN32
                        const int conversionError = WSAGetLastError();
#else
                        const int conversionError = errno;
#endif
                        throw std::runtime_error(SocketErrorMessage(conversionError));
                    }
                    resolved.push_back({true, text.data()});
                }
                else
                {
                    resolved.push_back({false, {}});
                }
            }
            return resolved;
        }

        [[nodiscard]] std::unique_ptr<IUdpClient> CreateUdpIPv4() override
        {
            return std::make_unique<NativeUdpClient>();
        }

        [[nodiscard]] std::chrono::system_clock::time_point UtcNow() override
        {
            return std::chrono::system_clock::now();
        }
    };

    struct EndPoint
    {
        std::string Address;
        std::int32_t Port;
    };

    [[nodiscard]] std::string Where(const EndPoint& endPoint)
    {
        return endPoint.Address + ":" + std::to_string(endPoint.Port);
    }

    [[nodiscard]] std::pair<bool, std::string> ProbeImpl(
        const std::string& address,
        std::int32_t port,
        std::int32_t timeoutMs,
        INetProbePlatform& platform)
    {
        EndPoint endPoint;
        try
        {
            std::vector<ResolvedAddress> resolved = platform.Resolve(address);
            if (resolved.empty())
            {
                return {false, "Could not resolve " + address + "."};
            }

            const ResolvedAddress* ipv4 = nullptr;
            for (const ResolvedAddress& candidate : resolved)
            {
                if (candidate.IsIPv4)
                {
                    ipv4 = &candidate;
                    break;
                }
            }
            if (ipv4 == nullptr)
            {
                return {false, address + " has no IPv4 address."};
            }
            if (port < 0 || port > 65535)
            {
                throw std::out_of_range(
                    "Specified argument was out of the range of valid values. (Parameter 'port')");
            }
            endPoint = {ipv4->Text, port};
        }
        catch (const std::exception& ex)
        {
            return {false, "Could not resolve " + address + ": " + ex.what()};
        }

        std::unique_ptr<IUdpClient> socket = platform.CreateUdpIPv4();
        socket->SetReceiveTimeout(timeoutMs);
        try
        {
            const std::array<std::uint8_t, 2> hello{Hello, ProtocolVersion};
            socket->Send(hello.data(), hello.size(), endPoint.Address, endPoint.Port);

            const auto deadline = platform.UtcNow() + std::chrono::milliseconds(timeoutMs);
            while (platform.UtcNow() < deadline)
            {
                std::vector<std::uint8_t> reply = socket->Receive();
                if (reply.size() >= 2 && reply[0] == Welcome)
                {
                    const std::array<std::uint8_t, 1> bye{Bye};
                    socket->Send(bye.data(), bye.size(), endPoint.Address, endPoint.Port);
                    return {true, "Connected to " + Where(endPoint)
                        + " -- server assigned slot "
                        + std::to_string(static_cast<unsigned int>(reply[1])) + "."};
                }
                if (!reply.empty() && reply[0] >= Hello && reply[0] <= Authority)
                {
                    continue;
                }
                return {false, Where(endPoint)
                    + " replied, but not as an MphRead server."};
            }

            const std::array<std::uint8_t, 1> bye{Bye};
            socket->Send(bye.data(), bye.size(), endPoint.Address, endPoint.Port);
            return {false, Where(endPoint) + " answered but never assigned a slot."};
        }
        catch (const SocketException& ex)
        {
            if (ex.TimedOut())
            {
                return {false, "No reply from " + Where(endPoint)
                    + ". The server may be down, or UDP " + std::to_string(port)
                    + " may be blocked by a firewall."};
            }
            return {false, "Could not reach " + Where(endPoint) + ": " + ex.what()};
        }
        catch (const std::exception& ex)
        {
            return {false, "Could not reach " + Where(endPoint) + ": " + ex.what()};
        }
    }
}

namespace MphRead::Mods::Network
{
    std::pair<bool, std::string> NetProbe::Probe(
        const std::string& address,
        std::int32_t port,
        std::int32_t timeoutMs)
    {
        Detail::NativeNetProbePlatform platform;
        return Detail::ProbeImpl(address, port, timeoutMs, platform);
    }
}
