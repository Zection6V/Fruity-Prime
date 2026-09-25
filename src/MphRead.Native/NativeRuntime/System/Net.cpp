#include "Net.hpp"
#include "Exceptions.hpp"

#include <array>
#include <atomic>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <mutex>
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
#include <windows.h>
#else
#include <cerrno>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>
#endif

namespace MphRead::NativeRuntime
{
#if defined(_WIN32)
    using NativeSocket = SOCKET;
    constexpr NativeSocket InvalidSocket = INVALID_SOCKET;
#else
    using NativeSocket = int;
    constexpr NativeSocket InvalidSocket = -1;
#endif

    struct SocketState
    {
        SocketState()
            : ReceiveBuffer(0x10000U)
        {
        }

        NativeSocket Native = InvalidSocket;
        bool BroadcastEnabled = false;
        std::vector<std::uint8_t> ReceiveBuffer;
    };

    SocketException::SocketException(int error, std::string operation)
#if defined(_WIN32)
        : std::system_error(error, std::system_category(), std::move(operation))
#else
        : std::system_error(error, std::generic_category(), std::move(operation))
#endif
    {
    }

    [[nodiscard]] int LastSocketError() noexcept
    {
#if defined(_WIN32)
        return WSAGetLastError();
#else
        return errno;
#endif
    }

#if defined(_WIN32)
    class WinsockRuntime final
    {
    public:
        WinsockRuntime()
        {
            WSADATA data{};
            const int result = WSAStartup(MAKEWORD(2, 2), &data);
            if (result != 0)
            {
                throw SocketException(result, "WSAStartup");
            }
        }

        ~WinsockRuntime()
        {
            WSACleanup();
        }

        WinsockRuntime(const WinsockRuntime&) = delete;
        WinsockRuntime& operator=(const WinsockRuntime&) = delete;
    };

    void EnsureWinsock()
    {
        static WinsockRuntime runtime;
        (void)runtime;
    }
#else
    void EnsureWinsock()
    {
    }
#endif

    [[nodiscard]] sockaddr_in ToSockAddr(
        const EndPoint& endPoint) noexcept
    {
        sockaddr_in address{};
        address.sin_family = AF_INET;
        std::memcpy(&address.sin_addr.s_addr,
            endPoint.Address.Bytes.data(), endPoint.Address.Bytes.size());
        address.sin_port = htons(static_cast<std::uint16_t>(endPoint.Port));
        return address;
    }

    [[nodiscard]] bool TryParseManagedIPv4(
        const std::string& text, std::array<std::uint8_t, 4>& bytes) noexcept
    {
        if (text.empty() || text.find(':') != std::string::npos)
        {
            return false;
        }

        std::array<std::uint64_t, 4> parts{};
        std::size_t part = 0;
        std::size_t index = 0;
        while (true)
        {
            if (part >= parts.size() || index >= text.size())
            {
                return false;
            }

            std::uint32_t base = 10;
            bool haveDigit = false;
            std::uint64_t value = 0;
            if (text[index] == '0')
            {
                base = 8;
                ++index;
                haveDigit = true;
                if (index < text.size()
                    && (text[index] == 'x' || text[index] == 'X'))
                {
                    base = 16;
                    ++index;
                    haveDigit = false;
                }
            }

            while (index < text.size())
            {
                const unsigned char ch = static_cast<unsigned char>(text[index]);
                std::uint32_t digit = 0;
                bool isDigit = false;
                if ((base == 10 || base == 16) && ch >= '0' && ch <= '9')
                {
                    digit = ch - '0';
                    isDigit = true;
                }
                else if (base == 8 && ch >= '0' && ch <= '7')
                {
                    digit = ch - '0';
                    isDigit = true;
                }
                else if (base == 16 && ch >= 'a' && ch <= 'f')
                {
                    digit = ch + 10U - 'a';
                    isDigit = true;
                }
                else if (base == 16 && ch >= 'A' && ch <= 'F')
                {
                    digit = ch + 10U - 'A';
                    isDigit = true;
                }

                if (!isDigit)
                {
                    break;
                }

                value = value * base + digit;
                if (value > 0xFFFFFFFFULL)
                {
                    return false;
                }
                haveDigit = true;
                ++index;
            }

            if (!haveDigit)
            {
                return false;
            }

            parts[part] = value;
            if (index == text.size())
            {
                break;
            }
            if (text[index] != '.' || part >= 3 || value > 0xFFU)
            {
                return false;
            }
            ++part;
            ++index;
        }

        std::uint64_t value = 0;
        switch (part)
        {
            case 0:
                value = parts[0];
                break;
            case 1:
                if (parts[1] > 0xFFFFFFU)
                {
                    return false;
                }
                value = (parts[0] << 24) | parts[1];
                break;
            case 2:
                if (parts[2] > 0xFFFFU)
                {
                    return false;
                }
                value = (parts[0] << 24) | (parts[1] << 16) | parts[2];
                break;
            case 3:
                if (parts[3] > 0xFFU)
                {
                    return false;
                }
                value = (parts[0] << 24) | (parts[1] << 16)
                    | (parts[2] << 8) | parts[3];
                break;
            default:
                return false;
        }

        bytes = {
            static_cast<std::uint8_t>((value >> 24) & 0xFFU),
            static_cast<std::uint8_t>((value >> 16) & 0xFFU),
            static_cast<std::uint8_t>((value >> 8) & 0xFFU),
            static_cast<std::uint8_t>(value & 0xFFU)
        };
        return true;
    }

    [[nodiscard]] std::size_t ManagedUtf16Length(
        const std::string& value) noexcept
    {
        std::size_t length = 0;
        for (std::size_t index = 0; index < value.size();)
        {
            const auto first = static_cast<unsigned char>(value[index]);
            std::size_t consumed = 1;
            std::uint32_t codePoint = first;

            if (first >= 0xC2U && first <= 0xDFU
                && index + 1 < value.size()
                && (static_cast<unsigned char>(value[index + 1]) & 0xC0U) == 0x80U)
            {
                codePoint = (static_cast<std::uint32_t>(first & 0x1FU) << 6)
                    | static_cast<std::uint32_t>(
                        static_cast<unsigned char>(value[index + 1]) & 0x3FU);
                consumed = 2;
            }
            else if (first >= 0xE0U && first <= 0xEFU
                && index + 2 < value.size())
            {
                const auto b1 = static_cast<unsigned char>(value[index + 1]);
                const auto b2 = static_cast<unsigned char>(value[index + 2]);
                if ((b1 & 0xC0U) == 0x80U && (b2 & 0xC0U) == 0x80U
                    && (first != 0xE0U || b1 >= 0xA0U)
                    && (first != 0xEDU || b1 <= 0x9FU))
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
                if ((b1 & 0xC0U) == 0x80U && (b2 & 0xC0U) == 0x80U
                    && (b3 & 0xC0U) == 0x80U
                    && (first != 0xF0U || b1 >= 0x90U)
                    && (first != 0xF4U || b1 <= 0x8FU))
                {
                    codePoint = (static_cast<std::uint32_t>(first & 0x07U) << 18)
                        | (static_cast<std::uint32_t>(b1 & 0x3FU) << 12)
                        | (static_cast<std::uint32_t>(b2 & 0x3FU) << 6)
                        | static_cast<std::uint32_t>(b3 & 0x3FU);
                    consumed = 4;
                }
            }

            length += codePoint > 0xFFFFU ? 2U : 1U;
            index += consumed;
        }
        return length;
    }

    std::string AddressToString(const Address& address)
    {
        std::string result;
        for (std::size_t i = 0; i < address.Bytes.size(); ++i)
        {
            if (i != 0)
            {
                result += '.';
            }
            result += std::to_string(static_cast<unsigned>(address.Bytes[i]));
        }
        return result;
    }

    std::vector<Address> DnsGetHostAddresses(
        const std::string& address)
    {
        // Dns.GetHostAddresses first runs IPAddress.TryParse. For IPv4 this
        // accepts .NET's legacy decimal/octal/hex forms and bypasses DNS.
        std::array<std::uint8_t, 4> parsedIPv4{};
        if (TryParseManagedIPv4(address, parsedIPv4))
        {
            if (parsedIPv4 == std::array<std::uint8_t, 4>{0, 0, 0, 0})
            {
                // Dns.GetHostAddresses rejects IPAddress.Any before resolution.
                throw std::invalid_argument("hostNameOrAddress");
            }
            return {
                Address{AddressFamily::InterNetwork, parsedIPv4}
            };
        }

        const std::size_t managedLength = ManagedUtf16Length(address);
        if (managedLength > 255U
            || (managedLength == 255U
                && (address.empty() || address.back() != '.')))
        {
            throw std::out_of_range("hostName");
        }

        EnsureWinsock();
        std::vector<Address> resolved;

#if defined(_WIN32)
        // Dns.GetHostAddresses on the net9.0 Windows target reaches
        // GetAddrInfoW. Native strings carry managed text as UTF-8, so use the
        // wide Winsock entry point rather than the ANSI getaddrinfo wrapper.
        const int wideLength = MultiByteToWideChar(
            CP_UTF8, MB_ERR_INVALID_CHARS, address.c_str(), -1, nullptr, 0);
        if (wideLength == 0)
        {
            throw std::runtime_error("MultiByteToWideChar failed");
        }
        std::wstring wideAddress(static_cast<std::size_t>(wideLength), L'\0');
        if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
                address.c_str(), -1, wideAddress.data(), wideLength) == 0)
        {
            throw std::runtime_error("MultiByteToWideChar failed");
        }

        ADDRINFOW hints{};
        hints.ai_family = AF_UNSPEC;
        ADDRINFOW* raw = nullptr;
        const int result = GetAddrInfoW(wideAddress.c_str(), nullptr, &hints, &raw);
        if (result != 0)
        {
            throw SocketException(result, "GetAddrInfoW");
        }

        std::unique_ptr<ADDRINFOW, decltype(&FreeAddrInfoW)> owner(raw, &FreeAddrInfoW);
        for (ADDRINFOW* current = raw; current != nullptr; current = current->ai_next)
        {
            if (current->ai_family != AF_INET || current->ai_addr == nullptr
                || current->ai_addrlen != sizeof(sockaddr_in))
            {
                continue;
            }
            const auto* ipv4 = reinterpret_cast<const sockaddr_in*>(current->ai_addr);
            Address item;
            item.Family = AddressFamily::InterNetwork;
            std::memcpy(item.Bytes.data(), &ipv4->sin_addr.s_addr, item.Bytes.size());
            resolved.push_back(item);
        }
#else
        addrinfo hints{};
        hints.ai_family = AF_UNSPEC;
        addrinfo* raw = nullptr;
        const int result = getaddrinfo(address.c_str(), nullptr, &hints, &raw);
        if (result != 0)
        {
            const char* message = gai_strerror(result);
            throw std::runtime_error(message != nullptr ? message : "getaddrinfo failed");
        }

        std::unique_ptr<addrinfo, decltype(&freeaddrinfo)> owner(raw, &freeaddrinfo);
        for (addrinfo* current = raw; current != nullptr; current = current->ai_next)
        {
            if (current->ai_family != AF_INET || current->ai_addr == nullptr
                || current->ai_addrlen
                    != static_cast<decltype(current->ai_addrlen)>(sizeof(sockaddr_in)))
            {
                continue;
            }
            const auto* ipv4 = reinterpret_cast<const sockaddr_in*>(current->ai_addr);
            Address item;
            item.Family = AddressFamily::InterNetwork;
            std::memcpy(item.Bytes.data(), &ipv4->sin_addr.s_addr, item.Bytes.size());
            resolved.push_back(item);
        }
#endif
        return resolved;
    }

    EndPoint CreateIPEndPoint(
        const Address& address, std::int32_t port)
    {
        if (port < 0 || port > 65535)
        {
            throw std::out_of_range("port");
        }
        return EndPoint{address, port};
    }

    EndPoint CreateIPv4AnyEndPoint()
    {
        Address address;
        address.Family = AddressFamily::InterNetwork;
        address.Bytes = {0, 0, 0, 0};
        return EndPoint{address, 0};
    }

    SocketHandle UdpClientCreateInterNetwork()
    {
        auto state = std::make_unique<SocketState>();
        EnsureWinsock();
        state->Native = ::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        if (state->Native == InvalidSocket)
        {
            throw SocketException(
                LastSocketError(), "socket");
        }
        return state.release();
    }

    void UdpClientSetReceiveTimeout(
        SocketHandle socket, std::int32_t timeoutMs)
    {
        if (timeoutMs < -1)
        {
            throw std::out_of_range("timeoutMs");
        }
        const std::int32_t value = timeoutMs == -1 ? 0 : timeoutMs;
        const NativeSocket native = socket->Native;
#if defined(_WIN32)
        const DWORD timeout = static_cast<DWORD>(value);
        if (setsockopt(native, SOL_SOCKET, SO_RCVTIMEO,
                reinterpret_cast<const char*>(&timeout), sizeof(timeout)) == SOCKET_ERROR)
        {
            throw SocketException(
                LastSocketError(), "setsockopt(SO_RCVTIMEO)");
        }
#else
        timeval timeout{};
        timeout.tv_sec = value / 1000;
        timeout.tv_usec = (value % 1000) * 1000;
        if (setsockopt(native, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) != 0)
        {
            throw SocketException(
                LastSocketError(), "setsockopt(SO_RCVTIMEO)");
        }
#endif
    }

    void UdpClientSend(SocketHandle socket,
        const std::uint8_t* data, std::int32_t length,
        const EndPoint& endPoint)
    {
        const NativeSocket native = socket->Native;
        const bool broadcast = endPoint.Address.Family == AddressFamily::InterNetwork
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
                throw SocketException(
                    LastSocketError(), "setsockopt(SO_BROADCAST)");
            }
        }

        const sockaddr_in target = ToSockAddr(endPoint);
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
            throw SocketException(
                LastSocketError(), "sendto");
        }
    }

    std::vector<std::uint8_t> UdpClientReceive(
        SocketHandle socket, EndPoint& from)
    {
        const NativeSocket native = socket->Native;
        sockaddr_in sender{};
#if defined(_WIN32)
        int senderLength = sizeof(sender);
        const int received = recvfrom(native,
            reinterpret_cast<char*>(socket->ReceiveBuffer.data()),
            static_cast<int>(socket->ReceiveBuffer.size()), 0,
            reinterpret_cast<sockaddr*>(&sender), &senderLength);
        if (received == SOCKET_ERROR)
#else
        socklen_t senderLength = sizeof(sender);
        const ssize_t received = recvfrom(native,
            socket->ReceiveBuffer.data(), socket->ReceiveBuffer.size(), 0,
            reinterpret_cast<sockaddr*>(&sender), &senderLength);
        if (received < 0)
#endif
        {
            throw SocketException(
                LastSocketError(), "recvfrom");
        }

        from.Address.Family = AddressFamily::InterNetwork;
        std::memcpy(from.Address.Bytes.data(),
            &sender.sin_addr.s_addr, from.Address.Bytes.size());
        from.Port = static_cast<std::int32_t>(ntohs(sender.sin_port));
        return std::vector<std::uint8_t>(
            socket->ReceiveBuffer.begin(),
            socket->ReceiveBuffer.begin() + static_cast<std::size_t>(received));
    }

    void UdpClientDispose(SocketHandle socket)
    {
        std::unique_ptr<SocketState> state(socket);
        if (!state || state->Native == InvalidSocket)
        {
            return;
        }
#if defined(_WIN32)
        (void)shutdown(state->Native, SD_BOTH);
        (void)closesocket(state->Native);
#else
        (void)shutdown(state->Native, SHUT_RDWR);
        (void)::close(state->Native);
#endif
        state->Native = InvalidSocket;
    }


    bool SocketErrorIsTimeout(const std::system_error& error) noexcept
    {
#if defined(_WIN32)
        return error.code().value() == WSAETIMEDOUT;
#else
        return error.code().value() == EAGAIN || error.code().value() == EWOULDBLOCK
            || error.code().value() == ETIMEDOUT;
#endif
    }
}
namespace System::Net
{
    struct IPEndPoint::State
    {
        State(std::array<std::uint8_t, 4> addressBytes, std::int32_t port)
            : AddressBytes(addressBytes), Port(port)
        {
        }

        mutable std::mutex Lock;
        std::array<std::uint8_t, 4> AddressBytes{};
        std::int32_t Port = 0;
    };

    IPEndPoint::IPEndPoint(std::array<std::uint8_t, 4> addressBytes, std::int32_t port)
    {
        if (port < 0 || port > 65535)
        {
            throw std::out_of_range("port");
        }
        _state = std::make_shared<State>(addressBytes, port);
    }

    std::shared_ptr<IPEndPoint> IPEndPoint::Any(std::int32_t port)
    {
        return std::make_shared<IPEndPoint>(std::array<std::uint8_t, 4>{0, 0, 0, 0}, port);
    }

    std::shared_ptr<IPEndPoint> IPEndPoint::Loopback(std::int32_t port)
    {
        return std::make_shared<IPEndPoint>(std::array<std::uint8_t, 4>{127, 0, 0, 1}, port);
    }

    std::array<std::uint8_t, 4> IPEndPoint::AddressBytes() const
    {
        std::lock_guard lock(_state->Lock);
        return _state->AddressBytes;
    }

    void IPEndPoint::SetAddressBytes(std::array<std::uint8_t, 4> addressBytes)
    {
        std::lock_guard lock(_state->Lock);
        _state->AddressBytes = addressBytes;
    }

    std::int32_t IPEndPoint::Port() const
    {
        std::lock_guard lock(_state->Lock);
        return _state->Port;
    }

    void IPEndPoint::SetPort(std::int32_t port)
    {
        if (port < 0 || port > 65535)
        {
            throw std::out_of_range("port");
        }
        std::lock_guard lock(_state->Lock);
        _state->Port = port;
    }

    bool IPEndPoint::Equals(const IPEndPoint& other) const
    {
        if (this == &other)
        {
            return true;
        }
        std::scoped_lock lock(_state->Lock, other._state->Lock);
        return _state->Port == other._state->Port
            && _state->AddressBytes == other._state->AddressBytes;
    }

    std::string IPEndPoint::ToString() const
    {
        std::lock_guard lock(_state->Lock);
        return std::to_string(_state->AddressBytes[0]) + "."
            + std::to_string(_state->AddressBytes[1]) + "."
            + std::to_string(_state->AddressBytes[2]) + "."
            + std::to_string(_state->AddressBytes[3]) + ":"
            + std::to_string(_state->Port);
    }
}

namespace MphRead::NativeRuntime
{
    void UdpClientSend(
        SocketHandle socket, const std::uint8_t* data, std::int32_t length,
        const std::shared_ptr<System::Net::IPEndPoint>& endPoint)
    {
        EndPoint target;
        target.Address.Family = AddressFamily::InterNetwork;
        target.Address.Bytes = endPoint->AddressBytes();
        target.Port = endPoint->Port();
        UdpClientSend(socket, data, length, target);
    }

    std::vector<std::uint8_t> UdpClientReceive(
        SocketHandle socket, std::shared_ptr<System::Net::IPEndPoint>& remote)
    {
        EndPoint from = CreateIPv4AnyEndPoint();
        std::vector<std::uint8_t> datagram = UdpClientReceive(socket, from);
        // UdpClient.Receive replaces the caller's reference with the sender's.
        remote = std::make_shared<System::Net::IPEndPoint>(from.Address.Bytes, from.Port);
        return datagram;
    }
}

namespace MphRead::NativeRuntime
{
    void UdpClientSend(
        SocketHandle socket, std::span<const std::uint8_t> datagram, const EndPoint& endPoint)
    {
        UdpClientSend(socket, datagram.data(), static_cast<std::int32_t>(datagram.size()), endPoint);
    }

    void UdpClientSend(
        SocketHandle socket, std::span<const std::uint8_t> datagram,
        const std::shared_ptr<System::Net::IPEndPoint>& endPoint)
    {
        UdpClientSend(socket, datagram.data(), static_cast<std::int32_t>(datagram.size()), endPoint);
    }

    struct UdpSocket::Impl final
    {
        Impl()
        {
            EnsureWinsock();
            const NativeSocket handle = ::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
            if (handle == InvalidSocket)
            {
                throw SocketException(LastSocketError(), "socket");
            }
            _handle.store(ToBits(handle), std::memory_order_seq_cst);
        }

        ~Impl()
        {
            Dispose();
        }

        Impl(const Impl&) = delete;
        Impl& operator=(const Impl&) = delete;

        void DisableUdpConnectionResetOnWindows()
        {
#if defined(_WIN32)
            const NativeSocket handle = GetHandleOrThrow();
            DWORD disabled = FALSE;
            DWORD bytesReturned = 0;
            constexpr DWORD SioUdpConnReset = 0x9800000CU;
            const int result = WSAIoctl(handle, SioUdpConnReset,
                &disabled, static_cast<DWORD>(sizeof(disabled)), nullptr, 0,
                &bytesReturned, nullptr, nullptr);
            if (result == SOCKET_ERROR)
            {
                throw SocketException(LastSocketError(), "WSAIoctl(SIO_UDP_CONNRESET)");
            }
#endif
        }

        void SetReceiveBufferSize(std::int32_t bytes)
        {
            SetIntOption(SO_RCVBUF, bytes, "setsockopt(SO_RCVBUF)");
        }

        void SetSendBufferSize(std::int32_t bytes)
        {
            SetIntOption(SO_SNDBUF, bytes, "setsockopt(SO_SNDBUF)");
        }

        void SetReceiveTimeout(std::int32_t milliseconds)
        {
            const NativeSocket handle = GetHandleOrThrow();
#if defined(_WIN32)
            const DWORD timeout = static_cast<DWORD>(milliseconds);
            if (setsockopt(handle, SOL_SOCKET, SO_RCVTIMEO,
                    reinterpret_cast<const char*>(&timeout), sizeof(timeout)) == SOCKET_ERROR)
            {
                throw SocketException(LastSocketError(), "setsockopt(SO_RCVTIMEO)");
            }
#else
            timeval timeout{};
            timeout.tv_sec = milliseconds / 1000;
            timeout.tv_usec = (milliseconds % 1000) * 1000;
            if (setsockopt(handle, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) != 0)
            {
                throw SocketException(LastSocketError(), "setsockopt(SO_RCVTIMEO)");
            }
#endif
        }

        void Bind(std::int32_t port)
        {
            if (port < 0 || port > 65535)
            {
                throw System::ArgumentOutOfRangeException("port");
            }
            const NativeSocket handle = GetHandleOrThrow();
            sockaddr_in address{};
            address.sin_family = AF_INET;
            address.sin_addr.s_addr = htonl(INADDR_ANY);
            address.sin_port = htons(static_cast<std::uint16_t>(port));
#if defined(_WIN32)
            if (::bind(handle, reinterpret_cast<const sockaddr*>(&address), sizeof(address)) == SOCKET_ERROR)
#else
            if (::bind(handle, reinterpret_cast<const sockaddr*>(&address), sizeof(address)) != 0)
#endif
            {
                throw SocketException(LastSocketError(), "bind");
            }
        }

        [[nodiscard]] std::int32_t LocalPort() const
        {
            const NativeSocket handle = GetHandleOrThrow();
            sockaddr_in address{};
#if defined(_WIN32)
            int length = sizeof(address);
            if (getsockname(handle, reinterpret_cast<sockaddr*>(&address), &length) == SOCKET_ERROR)
#else
            socklen_t length = sizeof(address);
            if (getsockname(handle, reinterpret_cast<sockaddr*>(&address), &length) != 0)
#endif
            {
                throw SocketException(LastSocketError(), "getsockname");
            }
            return static_cast<std::int32_t>(ntohs(address.sin_port));
        }

        [[nodiscard]] std::shared_ptr<std::vector<std::uint8_t>> Receive(
            std::shared_ptr<System::Net::IPEndPoint>& sender)
        {
            const NativeSocket handle = GetHandleOrThrow();
            auto data = std::make_shared<std::vector<std::uint8_t>>(65535);
            sockaddr_in from{};
#if defined(_WIN32)
            int fromLength = sizeof(from);
            const int count = recvfrom(handle,
                reinterpret_cast<char*>(data->data()), static_cast<int>(data->size()), 0,
                reinterpret_cast<sockaddr*>(&from), &fromLength);
            if (count == SOCKET_ERROR)
#else
            socklen_t fromLength = sizeof(from);
            const ssize_t count = recvfrom(handle, data->data(), data->size(), 0,
                reinterpret_cast<sockaddr*>(&from), &fromLength);
            if (count < 0)
#endif
            {
                const int error = LastSocketError();
                if (_disposed.load(std::memory_order_seq_cst))
                {
                    throw System::ObjectDisposedException("System.Net.Sockets.Socket", "Cannot access a disposed object.");
                }
                throw SocketException(error, "recvfrom");
            }
            data->resize(static_cast<std::size_t>(count));
            std::array<std::uint8_t, 4> bytes{};
            std::memcpy(bytes.data(), &from.sin_addr.s_addr, bytes.size());
            sender = std::make_shared<System::Net::IPEndPoint>(
                bytes, static_cast<std::int32_t>(ntohs(from.sin_port)));
            return data;
        }

        void Send(std::span<const std::uint8_t> datagram,
            const std::shared_ptr<System::Net::IPEndPoint>& target)
        {
            if (!target)
            {
                throw System::ArgumentNullException("remoteEP");
            }
            std::lock_guard sendLock(_sendLock);
            const NativeSocket handle = GetHandleOrThrow();
            const sockaddr_in address = ToSockAddr(CreateIPEndPoint(Address{AddressFamily::InterNetwork, target->AddressBytes()}, target->Port()));
#if defined(_WIN32)
            const int count = sendto(handle,
                reinterpret_cast<const char*>(datagram.data()),
                static_cast<int>(datagram.size()), 0,
                reinterpret_cast<const sockaddr*>(&address), sizeof(address));
            if (count == SOCKET_ERROR)
#else
            const ssize_t count = sendto(handle, datagram.data(), datagram.size(), 0,
                reinterpret_cast<const sockaddr*>(&address), sizeof(address));
            if (count < 0)
#endif
            {
                const int error = LastSocketError();
                if (_disposed.load(std::memory_order_seq_cst))
                {
                    throw System::ObjectDisposedException("System.Net.Sockets.Socket", "Cannot access a disposed object.");
                }
                throw SocketException(error, "sendto");
            }
        }

        void Dispose() noexcept
        {
            if (_disposed.exchange(true, std::memory_order_seq_cst))
            {
                return;
            }
            std::lock_guard sendLock(_sendLock);
            const std::uintptr_t oldBits = _handle.exchange(InvalidBits(), std::memory_order_seq_cst);
            const NativeSocket handle = FromBits(oldBits);
            if (handle == InvalidSocket)
            {
                return;
            }
#if defined(_WIN32)
            (void)shutdown(handle, SD_BOTH);
            (void)closesocket(handle);
#else
            (void)shutdown(handle, SHUT_RDWR);
            (void)::close(handle);
#endif
        }

        [[nodiscard]] static std::uintptr_t ToBits(NativeSocket handle) noexcept
        {
#if defined(_WIN32)
            return static_cast<std::uintptr_t>(handle);
#else
            return static_cast<std::uintptr_t>(static_cast<std::intptr_t>(handle));
#endif
        }

        [[nodiscard]] static NativeSocket FromBits(std::uintptr_t bits) noexcept
        {
#if defined(_WIN32)
            return static_cast<NativeSocket>(bits);
#else
            return static_cast<NativeSocket>(static_cast<std::intptr_t>(bits));
#endif
        }

        [[nodiscard]] static constexpr std::uintptr_t InvalidBits() noexcept
        {
#if defined(_WIN32)
            return static_cast<std::uintptr_t>(INVALID_SOCKET);
#else
            return static_cast<std::uintptr_t>(static_cast<std::intptr_t>(-1));
#endif
        }

        [[nodiscard]] NativeSocket GetHandleOrThrow() const
        {
            const NativeSocket handle = FromBits(_handle.load(std::memory_order_seq_cst));
            if (handle == InvalidSocket || _disposed.load(std::memory_order_seq_cst))
            {
                throw System::ObjectDisposedException("System.Net.Sockets.Socket", "Cannot access a disposed object.");
            }
            return handle;
        }

        void SetIntOption(int option, std::int32_t value, const char* operation)
        {
            const NativeSocket handle = GetHandleOrThrow();
#if defined(_WIN32)
            if (setsockopt(handle, SOL_SOCKET, option,
                    reinterpret_cast<const char*>(&value), sizeof(value)) == SOCKET_ERROR)
#else
            if (setsockopt(handle, SOL_SOCKET, option, &value, sizeof(value)) != 0)
#endif
            {
                throw SocketException(LastSocketError(), operation);
            }
        }

        std::atomic<std::uintptr_t> _handle{InvalidBits()};
        std::atomic<bool> _disposed{false};
        std::mutex _sendLock;
    };

    UdpSocket::UdpSocket()
        : _impl(std::make_unique<Impl>())
    {
    }

    UdpSocket::~UdpSocket() = default;

    void UdpSocket::DisableUdpConnectionResetOnWindows() { _impl->DisableUdpConnectionResetOnWindows(); }
    void UdpSocket::SetReceiveBufferSize(std::int32_t bytes) { _impl->SetReceiveBufferSize(bytes); }
    void UdpSocket::SetSendBufferSize(std::int32_t bytes) { _impl->SetSendBufferSize(bytes); }
    void UdpSocket::SetReceiveTimeout(std::int32_t milliseconds) { _impl->SetReceiveTimeout(milliseconds); }
    void UdpSocket::Bind(std::int32_t port) { _impl->Bind(port); }
    std::int32_t UdpSocket::LocalPort() const { return _impl->LocalPort(); }

    std::shared_ptr<std::vector<std::uint8_t>> UdpSocket::Receive(
        std::shared_ptr<System::Net::IPEndPoint>& sender)
    {
        return _impl->Receive(sender);
    }

    void UdpSocket::Send(std::span<const std::uint8_t> datagram,
        const std::shared_ptr<System::Net::IPEndPoint>& target)
    {
        _impl->Send(datagram, target);
    }

    void UdpSocket::Dispose() noexcept { _impl->Dispose(); }
}
