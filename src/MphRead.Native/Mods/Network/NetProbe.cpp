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
        for (std::size_t index = 0; index < 4; ++index)
        {
            const std::size_t dot = text.find('.', start);
            const bool last = index == 3;
            if ((last && dot != std::string_view::npos)
                || (!last && dot == std::string_view::npos))
            {
                return false;
            }
            const std::size_t end = last ? text.size() : dot;
            if (end == start)
            {
                return false;
            }
            std::uint32_t value = 0;
            for (std::size_t i = start; i < end; ++i)
            {
                const char ch = text[i];
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
            bytes[index] = value;
            start = end + 1;
        }
        high = static_cast<std::uint16_t>((bytes[0] << 8) | bytes[1]);
        low = static_cast<std::uint16_t>((bytes[2] << 8) | bytes[3]);
        return true;
    }

    [[nodiscard]] bool TryParseIPv6Side(
        std::string_view text,
        bool allowEmbeddedIPv4,
        std::vector<std::uint16_t>& words) noexcept
    {
        if (text.empty())
        {
            return true;
        }

        std::size_t start = 0;
        while (start <= text.size())
        {
            const std::size_t colon = text.find(':', start);
            const bool last = colon == std::string_view::npos;
            const std::size_t end = last ? text.size() : colon;
            if (end == start)
            {
                return false;
            }
            const std::string_view token = text.substr(start, end - start);
            if (token.find('.') != std::string_view::npos)
            {
                if (!allowEmbeddedIPv4 || !last)
                {
                    return false;
                }
                std::uint16_t high = 0;
                std::uint16_t low = 0;
                if (!TryParseEmbeddedIPv4(token, high, low))
                {
                    return false;
                }
                words.push_back(high);
                words.push_back(low);
            }
            else
            {
                if (token.size() > 4)
                {
                    return false;
                }
                std::uint16_t word = 0;
                for (char ch : token)
                {
                    if (!IsHexDigit(ch))
                    {
                        return false;
                    }
                    word = static_cast<std::uint16_t>(word * 16 + HexDigitValue(ch));
                }
                words.push_back(word);
            }
            if (last)
            {
                return true;
            }
            start = colon + 1;
        }
        return false;
    }

    [[nodiscard]] bool ValidateBracketPort(std::string_view suffix) noexcept
    {
        if (suffix.empty())
        {
            return true;
        }
        if (suffix.front() != ':')
        {
            return false;
        }
        if (suffix.size() >= 3 && suffix[1] == '0' && suffix[2] == 'x')
        {
            for (std::size_t i = 3; i < suffix.size(); ++i)
            {
                if (!IsHexDigit(suffix[i]))
                {
                    return false;
                }
            }
            return true;
        }
        for (std::size_t i = 1; i < suffix.size(); ++i)
        {
            if (suffix[i] < '0' || suffix[i] > '9')
            {
                return false;
            }
        }
        return true;
    }

    [[nodiscard]] bool TryParseDotNetIPv6(
        std::string_view text, bool& unspecified) noexcept
    {
        unspecified = false;
        if (text.find(':') == std::string_view::npos)
        {
            return false;
        }

        std::string_view address = text;
        if (!text.empty() && text.front() == '[')
        {
            const std::size_t close = text.find(']');
            if (close == std::string_view::npos || !ValidateBracketPort(text.substr(close + 1)))
            {
                return false;
            }
            address = text.substr(1, close - 1);
        }
        else if (text.find('[') != std::string_view::npos
            || text.find(']') != std::string_view::npos)
        {
            return false;
        }

        if (address.find('/') != std::string_view::npos)
        {
            return false;
        }

        std::uint32_t scope = 0;
        const std::size_t percent = address.find('%');
        if (percent != std::string_view::npos)
        {
            const std::string_view scopeText = address.substr(percent + 1);
            address = address.substr(0, percent);
            if (!scopeText.empty() && !TryParseUInt32Decimal(scopeText, scope))
            {
                const std::string interfaceName(scopeText);
                scope = if_nametoindex(interfaceName.c_str());
            }
        }

        const std::size_t compressor = address.find("::");
        std::vector<std::uint16_t> words;
        if (compressor == std::string_view::npos)
        {
            if (!TryParseIPv6Side(address, true, words) || words.size() != 8)
            {
                return false;
            }
        }
        else
        {
            if (address.find("::", compressor + 1) != std::string_view::npos)
            {
                return false;
            }
            std::vector<std::uint16_t> left;
            std::vector<std::uint16_t> right;
            if (!TryParseIPv6Side(address.substr(0, compressor), false, left)
                || !TryParseIPv6Side(address.substr(compressor + 2), true, right)
                || left.size() + right.size() >= 8)
            {
                return false;
            }
            words = std::move(left);
            words.resize(8 - right.size(), 0);
            words.insert(words.end(), right.begin(), right.end());
        }

        bool allZero = true;
        for (std::uint16_t word : words)
        {
            if (word != 0)
            {
                allZero = false;
                break;
            }
        }
        unspecified = allZero && scope == 0;
        return true;
    }

    [[nodiscard]] std::size_t DotNetUtf16Length(std::string_view text) noexcept
    {
        std::size_t units = 0;
        for (std::size_t i = 0; i < text.size();)
        {
            const unsigned char first = static_cast<unsigned char>(text[i]);
            if (first < 0x80)
            {
                ++units;
                ++i;
                continue;
            }

            std::size_t count = 0;
            std::uint32_t value = 0;
            if (first >= 0xC2 && first <= 0xDF)
            {
                count = 2;
                value = first & 0x1Fu;
            }
            else if (first >= 0xE0 && first <= 0xEF)
            {
                count = 3;
                value = first & 0x0Fu;
            }
            else if (first >= 0xF0 && first <= 0xF4)
            {
                count = 4;
                value = first & 0x07u;
            }
            else
            {
                return text.size();
            }

            if (i + count > text.size())
            {
                return text.size();
            }
            for (std::size_t j = 1; j < count; ++j)
            {
                const unsigned char next = static_cast<unsigned char>(text[i + j]);
                if ((next & 0xC0u) != 0x80u)
                {
                    return text.size();
                }
                value = (value << 6) | (next & 0x3Fu);
            }
            if ((count == 3 && value < 0x800u)
                || (count == 4 && value < 0x10000u)
                || (value >= 0xD800u && value <= 0xDFFFu)
                || value > 0x10FFFFu)
            {
                return text.size();
            }
            units += value >= 0x10000u ? 2u : 1u;
            i += count;
        }
        return units;
    }

    void ValidateHostName(const std::string& address)
    {
        const std::size_t length = DotNetUtf16Length(address);
        if (length > 255 || (length == 255 && (address.empty() || address.back() != '.')))
        {
            throw std::out_of_range(
                "The size of hostName is too long. It cannot be longer than 255 characters. "
                "(Parameter 'hostName')");
        }
    }

    [[nodiscard]] bool SameResolvedAddress(
        const ResolvedAddress& left, const ResolvedAddress& right) noexcept
    {
        if (left.IsIPv4 != right.IsIPv4)
        {
            return false;
        }
        return !left.IsIPv4 || left.Text == right.Text;
    }

    void AppendResolvedUnique(
        std::vector<ResolvedAddress>& resolved, ResolvedAddress address)
    {
        for (const ResolvedAddress& existing : resolved)
        {
            if (SameResolvedAddress(existing, address))
            {
                return;
            }
        }
        resolved.push_back(std::move(address));
    }

#ifndef _WIN32
    [[nodiscard]] std::string DotNetDnsErrorMessage(int error)
    {
        switch (error)
        {
        case EAI_AGAIN:
            return SocketErrorMessage(EAGAIN);
        case EAI_BADFLAGS:
            return SocketErrorMessage(EINVAL);
#ifdef EAI_FAIL
        case EAI_FAIL:
            return SocketErrorMessage(11003);
#endif
        case EAI_FAMILY:
            return SocketErrorMessage(EAFNOSUPPORT);
        case EAI_MEMORY:
            return "Exception of type 'System.OutOfMemoryException' was thrown.";
        case EAI_NONAME:
            return gai_strerror(EAI_NONAME);
#if defined(EAI_NODATA) && EAI_NODATA != EAI_NONAME
        case EAI_NODATA:
            return gai_strerror(EAI_NONAME);
#endif
        default:
            return "Unknown socket error";
        }
    }

    void AppendLocalHostAddresses(
        const std::string& query, std::vector<ResolvedAddress>& resolved)
    {
        std::array<char, 256> host{};
        if (gethostname(host.data(), host.size()) != 0
            || strcasecmp(query.c_str(), host.data()) != 0)
        {
            return;
        }

        ifaddrs* raw = nullptr;
        if (getifaddrs(&raw) != 0)
        {
            return;
        }
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
        std::unique_ptr<ifaddrs, IfAddrsDeleter> addresses(raw);

        bool includeIPv4Loopback = true;
        bool includeIPv6Loopback = true;
        for (ifaddrs* current = addresses.get(); current != nullptr; current = current->ifa_next)
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

        for (ifaddrs* current = addresses.get(); current != nullptr; current = current->ifa_next)
        {
            if (current->ifa_addr == nullptr || (current->ifa_flags & IFF_UP) == 0)
            {
                continue;
            }
            const int family = current->ifa_addr->sa_family;
            if (family == AF_INET)
            {
                if (!includeIPv4Loopback && (current->ifa_flags & IFF_LOOPBACK) != 0)
                {
                    continue;
                }
                const auto* ipv4 = reinterpret_cast<const sockaddr_in*>(current->ifa_addr);
                AppendResolvedUnique(resolved,
                    {true, FormatDotNetIPv4(ntohl(ipv4->sin_addr.s_addr))});
            }
            else if (family == AF_INET6)
            {
                if (!includeIPv6Loopback && (current->ifa_flags & IFF_LOOPBACK) != 0)
                {
                    continue;
                }
                AppendResolvedUnique(resolved, {false, {}});
            }
        }
    }
#else
    [[nodiscard]] std::wstring Utf8ToWide(std::string_view text)
    {
        if (text.empty())
        {
            return {};
        }
        const int length = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
            text.data(), static_cast<int>(text.size()), nullptr, 0);
        if (length == 0)
        {
            throw std::runtime_error(SocketErrorMessage(static_cast<int>(GetLastError())));
        }
        std::wstring result(static_cast<std::size_t>(length), L'\0');
        if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
                text.data(), static_cast<int>(text.size()), result.data(), length) == 0)
        {
            throw std::runtime_error(SocketErrorMessage(static_cast<int>(GetLastError())));
        }
        return result;
    }
#endif

    class NativeNetProbePlatform final : public INetProbePlatform
    {
    public:
        [[nodiscard]] std::vector<ResolvedAddress> Resolve(
            const std::string& address) override
        {
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

            bool ipv6Unspecified = false;
            if (TryParseDotNetIPv6(address, ipv6Unspecified))
            {
                if (ipv6Unspecified)
                {
                    throw std::invalid_argument(
                        "IPv4 address 0.0.0.0 and IPv6 address ::0 are unspecified addresses "
                        "that cannot be used as a target address. (Parameter 'hostNameOrAddress')");
                }
                return {{false, {}}};
            }

            ValidateHostName(address);
#ifdef _WIN32
            EnsureWinSock();
            const std::wstring query = Utf8ToWide(address);
            ADDRINFOW hints{};
            hints.ai_family = AF_UNSPEC;
            ADDRINFOW* raw = nullptr;
            const int error = GetAddrInfoW(query.c_str(), nullptr, &hints, &raw);
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
                    const auto* currentIPv4 = reinterpret_cast<const sockaddr_in*>(current->ai_addr);
                    AppendResolvedUnique(resolved,
                        {true, FormatDotNetIPv4(ntohl(currentIPv4->sin_addr.s_addr))});
                }
                else if (current->ai_family == AF_INET6)
                {
                    AppendResolvedUnique(resolved, {false, {}});
                }
            }
            return resolved;
#else
            std::string query = address;
            if (query.empty())
            {
                std::array<char, 256> host{};
                if (gethostname(host.data(), host.size()) != 0)
                {
                    const int error = errno;
                    throw std::runtime_error(SocketErrorMessage(error));
                }
                query = host.data();
            }

            addrinfo hints{};
            hints.ai_flags = AI_CANONNAME;
            hints.ai_family = AF_UNSPEC;
            addrinfo* raw = nullptr;
            const int error = getaddrinfo(query.c_str(), nullptr, &hints, &raw);
            if (error != 0)
            {
                throw std::runtime_error(DotNetDnsErrorMessage(error));
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
                    const auto* currentIPv4 = reinterpret_cast<const sockaddr_in*>(current->ai_addr);
                    AppendResolvedUnique(resolved,
                        {true, FormatDotNetIPv4(ntohl(currentIPv4->sin_addr.s_addr))});
                }
                else if (current->ai_family == AF_INET6)
                {
                    AppendResolvedUnique(resolved, {false, {}});
                }
            }
            AppendLocalHostAddresses(query, resolved);
            return resolved;
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
