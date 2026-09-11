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
#include <ifaddrs.h>
#include <netdb.h>
#include <net/if.h>
#include <arpa/inet.h>
#include <strings.h>
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

            if (!_isBroadcast && destination.sin_addr.s_addr == htonl(INADDR_BROADCAST))
            {
                _isBroadcast = true;
                const int enabled = 1;
#ifdef _WIN32
                if (setsockopt(_socket, SOL_SOCKET, SO_BROADCAST,
                        reinterpret_cast<const char*>(&enabled), sizeof(enabled)) == SOCKET_ERROR)
                {
                    const int error = WSAGetLastError();
                    throw SocketException(SocketErrorMessage(error), IsTimeoutError(error));
                }
#else
                if (setsockopt(_socket, SOL_SOCKET, SO_BROADCAST, &enabled,
                        static_cast<socklen_t>(sizeof(enabled))) != 0)
                {
                    const int error = errno;
                    throw SocketException(SocketErrorMessage(error), IsTimeoutError(error));
                }
#endif
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
            ssize_t sent;
#if defined(__APPLE__) && __APPLE__
            int maxProtoRetry = 4;
            while ((sent = sendto(_socket, data, length, 0,
                    reinterpret_cast<const sockaddr*>(&destination), sizeof(destination))) < 0
                && (errno == EINTR || (errno == EPROTOTYPE && --maxProtoRetry > 0)))
            {
            }
#else
            while ((sent = sendto(_socket, data, length, 0,
                    reinterpret_cast<const sockaddr*>(&destination), sizeof(destination))) < 0
                && errno == EINTR)
            {
            }
#endif
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
            ssize_t received;
            while ((received = recvfrom(_socket, buffer.data(), buffer.size(), 0,
                    reinterpret_cast<sockaddr*>(&from), &fromLength)) < 0
                && errno == EINTR)
            {
                fromLength = sizeof(from);
            }
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
        bool _isBroadcast = false;
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

    [[nodiscard]] std::string FormatDotNetIPv4(std::uint32_t value)
    {
        return std::to_string((value >> 24) & 0xFFu) + "."
            + std::to_string((value >> 16) & 0xFFu) + "."
            + std::to_string((value >> 8) & 0xFFu) + "."
            + std::to_string(value & 0xFFu);
    }

    [[nodiscard]] bool IsHexDigit(char ch) noexcept
    {
        return (ch >= '0' && ch <= '9')
            || (ch >= 'a' && ch <= 'f')
            || (ch >= 'A' && ch <= 'F');
    }

    [[nodiscard]] int HexDigitValue(char ch) noexcept
    {
        if (ch >= '0' && ch <= '9')
        {
            return ch - '0';
        }
        if (ch >= 'a' && ch <= 'f')
        {
            return ch + 10 - 'a';
        }
        return ch + 10 - 'A';
    }

    [[nodiscard]] bool TryParseUInt32Decimal(
        std::string_view text, std::uint32_t& value) noexcept
    {
        if (text.empty())
        {
            return false;
        }
        std::uint64_t parsed = 0;
        for (char ch : text)
        {
            if (ch < '0' || ch > '9')
            {
                return false;
            }
            parsed = parsed * 10 + static_cast<unsigned int>(ch - '0');
            if (parsed > 0xFFFFFFFFull)
            {
                return false;
            }
        }
        value = static_cast<std::uint32_t>(parsed);
        return true;
    }

    [[nodiscard]] bool TryParseEmbeddedIPv4(
        std::string_view text, std::uint16_t& high, std::uint16_t& low) noexcept
    {
        std::uint32_t bytes[4]{};
        std::size_t start = 0;
        for (int i = 0; i < 4; ++i)
        {
            const std::size_t end = i == 3 ? text.size() : text.find('.', start);
            if (end == std::string_view::npos || end == start)
            {
                return false;
            }
            std::uint32_t value = 0;
            for (std::size_t j = start; j < end; ++j)
            {
                const char ch = text[j];
                if (ch < '0' || ch > '9')
                {
                    return false;
                }
                value = value * 10 + static_cast<unsigned int>(ch - '0');
                if (value > 255)
                {
                    return false;
                }
            }
            bytes[i] = value;
            start = end + 1;
        }
        if (start != text.size() + 1)
        {
            return false;
        }
        high = static_cast<std::uint16_t>((bytes[0] << 8) | bytes[1]);
        low = static_cast<std::uint16_t>((bytes[2] << 8) | bytes[3]);
        return true;
    }

    [[nodiscard]] bool TryParseDotNetIPv6(
        std::string_view text, in6_addr& address, std::uint32_t& scope) noexcept
    {
        if (text.empty())
        {
            return false;
        }

        std::string_view input = text;
        bool bracketed = false;
        if (input.front() == '[')
        {
            const std::size_t close = input.find(']');
            if (close == std::string_view::npos)
            {
                return false;
            }
            bracketed = true;
            const std::string_view suffix = input.substr(close + 1);
            if (!suffix.empty())
            {
                if (suffix.front() != ':')
                {
                    return false;
                }
                const std::string_view port = suffix.substr(1);
                if (port.empty())
                {
                    return false;
                }
                if (port.size() > 2 && port[0] == '0' && port[1] == 'x')
                {
                    if (port.size() == 2)
                    {
                        return false;
                    }
                    for (std::size_t i = 2; i < port.size(); ++i)
                    {
                        if (!IsHexDigit(port[i]))
                        {
                            return false;
                        }
                    }
                }
                else
                {
                    for (char ch : port)
                    {
                        if (ch < '0' || ch > '9')
                        {
                            return false;
                        }
                    }
                }
            }
            input = input.substr(1, close - 1);
        }
        else if (input.find(']') != std::string_view::npos)
        {
            return false;
        }

        std::string_view scopeText;
        const std::size_t percent = input.find('%');
        if (percent != std::string_view::npos)
        {
            scopeText = input.substr(percent + 1);
            input = input.substr(0, percent);
        }

        if (input.find(':') == std::string_view::npos)
        {
            return false;
        }

        std::array<std::uint16_t, 8> groups{};
        std::size_t groupCount = 0;
        std::size_t compressor = std::string_view::npos;
        std::size_t pos = 0;
        if (input.size() >= 2 && input[0] == ':' && input[1] == ':')
        {
            compressor = 0;
            pos = 2;
            if (pos == input.size())
            {
                groupCount = 0;
            }
        }
        else if (!input.empty() && input.front() == ':')
        {
            return false;
        }

        while (pos < input.size())
        {
            if (groupCount >= 8)
            {
                return false;
            }
            const std::size_t nextColon = input.find(':', pos);
            const std::size_t tokenEnd = nextColon == std::string_view::npos
                ? input.size() : nextColon;
            std::string_view token = input.substr(pos, tokenEnd - pos);
            if (token.empty())
            {
                if (nextColon == std::string_view::npos || compressor != std::string_view::npos)
                {
                    return false;
                }
                compressor = groupCount;
                pos = nextColon + 1;
                continue;
            }

            if (token.find('.') != std::string_view::npos)
            {
                if (nextColon != std::string_view::npos || groupCount > 6)
                {
                    return false;
                }
                std::uint16_t high = 0;
                std::uint16_t low = 0;
                if (!TryParseEmbeddedIPv4(token, high, low))
                {
                    return false;
                }
                groups[groupCount++] = high;
                groups[groupCount++] = low;
                pos = input.size();
                break;
            }

            if (token.size() > 4)
            {
                return false;
            }
            std::uint16_t value = 0;
            for (char ch : token)
            {
                if (!IsHexDigit(ch))
                {
                    return false;
                }
                value = static_cast<std::uint16_t>(value * 16 + HexDigitValue(ch));
            }
            groups[groupCount++] = value;

            if (nextColon == std::string_view::npos)
            {
                pos = input.size();
                break;
            }
            pos = nextColon + 1;
            if (pos == input.size())
            {
                return false;
            }
            if (input[pos] == ':')
            {
                if (compressor != std::string_view::npos)
                {
                    return false;
                }
                compressor = groupCount;
                ++pos;
                if (pos == input.size())
                {
                    break;
                }
            }
        }

        if (compressor == std::string_view::npos)
        {
            if (groupCount != 8)
            {
                return false;
            }
        }
        else
        {
            if (groupCount >= 8)
            {
                return false;
            }
            const std::size_t zeros = 8 - groupCount;
            for (std::size_t i = groupCount; i > compressor; --i)
            {
                groups[i + zeros - 1] = groups[i - 1];
            }
            for (std::size_t i = compressor; i < compressor + zeros; ++i)
            {
                groups[i] = 0;
            }
            groupCount = 8;
        }

        if (scopeText.empty())
        {
            scope = 0;
        }
        else if (!TryParseUInt32Decimal(scopeText, scope))
        {
            std::string scopeName(scopeText);
#ifdef _WIN32
            scope = if_nametoindex(scopeName.c_str());
#else
            scope = if_nametoindex(scopeName.c_str());
#endif
            if (scope == 0)
            {
                scope = 0;
            }
        }

        auto* bytes = reinterpret_cast<unsigned char*>(&address);
        for (std::size_t i = 0; i < 8; ++i)
        {
            bytes[i * 2] = static_cast<unsigned char>(groups[i] >> 8);
            bytes[i * 2 + 1] = static_cast<unsigned char>(groups[i] & 0xFF);
        }
        (void)bracketed;
        return true;
    }

    [[nodiscard]] std::string FormatIPv6(const in6_addr& address)
    {
        std::array<char, INET6_ADDRSTRLEN> text{};
        if (inet_ntop(AF_INET6, &address, text.data(), text.size()) == nullptr)
        {
#ifdef _WIN32
            const int error = WSAGetLastError();
#else
            const int error = errno;
#endif
            throw std::runtime_error(SocketErrorMessage(error));
        }
        return text.data();
    }

    [[nodiscard]] std::size_t Utf16Length(std::string_view value)
    {
        std::size_t count = 0;
        for (std::size_t i = 0; i < value.size();)
        {
            const unsigned char lead = static_cast<unsigned char>(value[i]);
            std::uint32_t codePoint = 0;
            std::size_t width = 1;
            if (lead < 0x80)
            {
                codePoint = lead;
            }
            else if ((lead & 0xE0) == 0xC0 && i + 1 < value.size())
            {
                codePoint = lead & 0x1F;
                width = 2;
            }
            else if ((lead & 0xF0) == 0xE0 && i + 2 < value.size())
            {
                codePoint = lead & 0x0F;
                width = 3;
            }
            else if ((lead & 0xF8) == 0xF0 && i + 3 < value.size())
            {
                codePoint = lead & 0x07;
                width = 4;
            }
            else
            {
                ++count;
                ++i;
                continue;
            }
            for (std::size_t j = 1; j < width; ++j)
            {
                codePoint = (codePoint << 6)
                    | (static_cast<unsigned char>(value[i + j]) & 0x3F);
            }
            count += codePoint > 0xFFFF ? 2 : 1;
            i += width;
        }
        return count;
    }

#ifdef _WIN32
    [[nodiscard]] std::wstring Utf8ToWide(const std::string& value)
    {
        if (value.empty())
        {
            return {};
        }
        const int count = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
            value.data(), static_cast<int>(value.size()), nullptr, 0);
        if (count == 0)
        {
            throw std::runtime_error(SocketErrorMessage(GetLastError()));
        }
        std::wstring wide(static_cast<std::size_t>(count), L'\0');
        if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.data(),
                static_cast<int>(value.size()), wide.data(), count) == 0)
        {
            throw std::runtime_error(SocketErrorMessage(GetLastError()));
        }
        return wide;
    }

    [[nodiscard]] std::vector<ResolvedAddress> ResolveWindows(const std::string& query)
    {
        std::wstring wide = Utf8ToWide(query);
        ADDRINFOW hints{};
        hints.ai_family = AF_UNSPEC;
        ADDRINFOW* raw = nullptr;
        const int error = GetAddrInfoW(wide.c_str(), nullptr, &hints, &raw);
        if (error != 0)
        {
            throw std::runtime_error(SocketErrorMessage(error));
        }

        struct AddrInfoDeleter
        {
            void operator()(ADDRINFOW* value) const noexcept
            {
                if (value != nullptr)
                {
                    FreeAddrInfoW(value);
                }
            }
        };
        std::unique_ptr<ADDRINFOW, AddrInfoDeleter> results(raw);
        std::vector<ResolvedAddress> resolved;
        for (ADDRINFOW* current = results.get(); current != nullptr; current = current->ai_next)
        {
            if (current->ai_family == AF_INET)
            {
                const auto* ipv4 = reinterpret_cast<const sockaddr_in*>(current->ai_addr);
                std::array<char, INET_ADDRSTRLEN> text{};
                if (inet_ntop(AF_INET, &ipv4->sin_addr, text.data(), text.size()) == nullptr)
                {
                    throw std::runtime_error(SocketErrorMessage(WSAGetLastError()));
                }
                resolved.push_back({true, text.data()});
            }
            else if (current->ai_family == AF_INET6)
            {
                resolved.push_back({false, {}});
            }
        }
        return resolved;
    }
#else
    enum class DnsSocketError
    {
        HostNotFound,
        TryAgain,
        InvalidArgument,
        NoRecovery,
        AddressFamilyNotSupported,
        SocketError
    };

    [[nodiscard]] int NativeErrorForDnsSocketError(DnsSocketError error)
    {
        switch (error)
        {
        case DnsSocketError::HostNotFound:
#ifdef EAI_NONAME
            return EAI_NONAME;
#else
            return -1;
#endif
        case DnsSocketError::TryAgain:
            return EAGAIN;
        case DnsSocketError::InvalidArgument:
            return EINVAL;
        case DnsSocketError::NoRecovery:
            return 11003;
        case DnsSocketError::AddressFamilyNotSupported:
            return EAFNOSUPPORT;
        case DnsSocketError::SocketError:
            return -1;
        }
        return -1;
    }

    [[nodiscard]] std::string DnsSocketErrorMessage(DnsSocketError error)
    {
        if (error == DnsSocketError::HostNotFound)
        {
#ifdef EAI_NONAME
            return gai_strerror(EAI_NONAME);
#else
            return "Name lookup failed";
#endif
        }
        const int nativeError = NativeErrorForDnsSocketError(error);
        if (nativeError > 0)
        {
            return SocketErrorMessage(nativeError);
        }
        return "Unknown socket error";
    }

    [[nodiscard]] DnsSocketError MapGaiError(int error)
    {
#ifdef EAI_AGAIN
        if (error == EAI_AGAIN)
        {
            return DnsSocketError::TryAgain;
        }
#endif
#ifdef EAI_BADFLAGS
        if (error == EAI_BADFLAGS)
        {
            return DnsSocketError::InvalidArgument;
        }
#endif
#ifdef EAI_FAIL
        if (error == EAI_FAIL)
        {
            return DnsSocketError::NoRecovery;
        }
#endif
#ifdef EAI_FAMILY
        if (error == EAI_FAMILY)
        {
            return DnsSocketError::AddressFamilyNotSupported;
        }
#endif
#ifdef EAI_NONAME
        if (error == EAI_NONAME)
        {
            return DnsSocketError::HostNotFound;
        }
#endif
#ifdef EAI_NODATA
        if (error == EAI_NODATA)
        {
            return DnsSocketError::HostNotFound;
        }
#endif
        return DnsSocketError::SocketError;
    }

    void AppendUnixAddress(std::vector<ResolvedAddress>& resolved, const sockaddr* address)
    {
        if (address == nullptr)
        {
            return;
        }
        if (address->sa_family == AF_INET)
        {
            const auto* ipv4 = reinterpret_cast<const sockaddr_in*>(address);
            std::array<char, INET_ADDRSTRLEN> text{};
            if (inet_ntop(AF_INET, &ipv4->sin_addr, text.data(), text.size()) == nullptr)
            {
                throw std::runtime_error(SocketErrorMessage(errno));
            }
            for (const ResolvedAddress& existing : resolved)
            {
                if (existing.IsIPv4 && existing.Text == text.data())
                {
                    return;
                }
            }
            resolved.push_back({true, text.data()});
        }
        else if (address->sa_family == AF_INET6)
        {
            resolved.push_back({false, {}});
        }
    }

    [[nodiscard]] std::vector<ResolvedAddress> ResolveUnix(const std::string& query)
    {
        addrinfo hints{};
        hints.ai_family = AF_UNSPEC;
        hints.ai_flags = AI_CANONNAME;
        addrinfo* raw = nullptr;
        const int error = getaddrinfo(query.c_str(), nullptr, &hints, &raw);
        if (error != 0)
        {
            throw std::runtime_error(DnsSocketErrorMessage(MapGaiError(error)));
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
            AppendUnixAddress(resolved, current->ai_addr);
        }

        std::array<char, 256> host{};
        if (gethostname(host.data(), host.size()) == 0
            && strcasecmp(query.c_str(), host.data()) == 0)
        {
            ifaddrs* rawInterfaces = nullptr;
            if (getifaddrs(&rawInterfaces) == 0)
            {
                struct IfAddrsDeleter
                {
                    void operator()(ifaddrs* value) const noexcept
                    {
                        if (value != nullptr)
                        {
                            freeifaddrs(value);
                        }
                    }
                };
                std::unique_ptr<ifaddrs, IfAddrsDeleter> interfaces(rawInterfaces);
                bool includeIPv4Loopback = true;
                bool includeIPv6Loopback = true;
                for (ifaddrs* current = interfaces.get(); current != nullptr;
                     current = current->ifa_next)
                {
                    if (current->ifa_addr == nullptr || (current->ifa_flags & IFF_UP) == 0)
                    {
                        continue;
                    }
                    if (current->ifa_addr->sa_family == AF_INET
                        && (current->ifa_flags & IFF_LOOPBACK) == 0)
                    {
                        includeIPv4Loopback = false;
                    }
                    else if (current->ifa_addr->sa_family == AF_INET6
                        && (current->ifa_flags & IFF_LOOPBACK) == 0)
                    {
                        includeIPv6Loopback = false;
                    }
                }
                for (ifaddrs* current = interfaces.get(); current != nullptr;
                     current = current->ifa_next)
                {
                    if (current->ifa_addr == nullptr || (current->ifa_flags & IFF_UP) == 0)
                    {
                        continue;
                    }
                    if ((!includeIPv4Loopback && current->ifa_addr->sa_family == AF_INET
                            && (current->ifa_flags & IFF_LOOPBACK) != 0)
                        || (!includeIPv6Loopback && current->ifa_addr->sa_family == AF_INET6
                            && (current->ifa_flags & IFF_LOOPBACK) != 0))
                    {
                        continue;
                    }
                    AppendUnixAddress(resolved, current->ifa_addr);
                }
            }
        }
        return resolved;
    }
#endif

    [[nodiscard]] bool IsUnspecifiedIPv6(const in6_addr& address) noexcept
    {
        const auto* bytes = reinterpret_cast<const unsigned char*>(&address);
        for (std::size_t i = 0; i < sizeof(address); ++i)
        {
            if (bytes[i] != 0)
            {
                return false;
            }
        }
        return true;
    }

    class NativeNetProbePlatform final : public INetProbePlatform
    {
    public:
        [[nodiscard]] std::vector<ResolvedAddress> Resolve(
            const std::string& address) override
        {
#ifdef _WIN32
            EnsureWinSock();
#endif
            std::uint32_t ipv4 = 0;
            if (TryParseDotNetIPv4(address, ipv4))
            {
                if (ipv4 == 0)
                {
                    throw std::invalid_argument(
                        "IPv4 address 0.0.0.0 and IPv6 address ::0 are unspecified addresses "
                        "that cannot be used as a target address. (Parameter 'hostNameOrAddress')");
                }
                return {{true, FormatDotNetIPv4(ipv4)}};
            }

            in6_addr ipv6{};
            std::uint32_t scope = 0;
            if (TryParseDotNetIPv6(address, ipv6, scope))
            {
                if (IsUnspecifiedIPv6(ipv6))
                {
                    throw std::invalid_argument(
                        "IPv4 address 0.0.0.0 and IPv6 address ::0 are unspecified addresses "
                        "that cannot be used as a target address. (Parameter 'hostNameOrAddress')");
                }
                (void)scope;
                return {{false, FormatIPv6(ipv6)}};
            }

            const std::size_t length = Utf16Length(address);
            if (length > 255 || (length == 255 && !address.empty() && address.back() != '.'))
            {
                throw std::out_of_range(
                    "The size of hostName is too long. It cannot be longer than 255 characters. "
                    "(Parameter 'hostName')");
            }

            std::string query = address;
            if (query.empty())
            {
                std::array<char, 256> host{};
                if (gethostname(host.data(), static_cast<int>(host.size())) != 0)
                {
#ifdef _WIN32
                    throw std::runtime_error(SocketErrorMessage(WSAGetLastError()));
#else
                    throw std::runtime_error(SocketErrorMessage(errno));
#endif
                }
                query = host.data();
            }
#ifdef _WIN32
            return ResolveWindows(query);
#else
            return ResolveUnix(query);
#endif
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
