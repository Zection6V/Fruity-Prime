#include "NetTransport.hpp"

#include "NetLag.hpp"
#include "NetProtocol.hpp"

#include <bit>
#include <chrono>
#include <condition_variable>
#include <cstring>
#include <deque>
#include <exception>
#include <limits>
#include <mutex>
#include <new>
#include <stdexcept>
#include <system_error>
#include <thread>
#include <utility>

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
#include <ctime>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <pthread.h>
#endif

namespace
{
#if defined(_WIN32)
    using SocketHandle = SOCKET;
    constexpr SocketHandle InvalidSocket = INVALID_SOCKET;
#else
    using SocketHandle = int;
    constexpr SocketHandle InvalidSocket = -1;
#endif

    class NativeObjectDisposedException final : public std::runtime_error
    {
    public:
        explicit NativeObjectDisposedException(std::string objectName)
            : std::runtime_error("Cannot access a disposed object. Object name: '"
                + std::move(objectName) + "'.")
        {
        }
    };

    class NativeSocketException final : public std::system_error
    {
    public:
        NativeSocketException(int error, std::string operation)
#if defined(_WIN32)
            : std::system_error(error, std::system_category(), std::move(operation)),
#else
            : std::system_error(error, std::generic_category(), std::move(operation)),
#endif
              _error(error)
        {
        }

        [[nodiscard]] int NativeError() const noexcept
        {
            return _error;
        }

        [[nodiscard]] bool TimedOut() const noexcept
        {
#if defined(_WIN32)
            return _error == WSAETIMEDOUT;
#else
            return _error == EAGAIN || _error == EWOULDBLOCK || _error == ETIMEDOUT;
#endif
        }

    private:
        int _error;
    };

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
                throw NativeSocketException(result, "WSAStartup");
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

    [[nodiscard]] std::int64_t AddInt64Unchecked(std::int64_t left, std::int64_t right) noexcept
    {
        const std::uint64_t a = std::bit_cast<std::uint64_t>(left);
        const std::uint64_t b = std::bit_cast<std::uint64_t>(right);
        return std::bit_cast<std::int64_t>(a + b);
    }

    [[nodiscard]] std::int64_t StopwatchTimestamp() noexcept
    {
#if defined(_WIN32)
        LARGE_INTEGER value{};
        (void)QueryPerformanceCounter(&value);
        return static_cast<std::int64_t>(value.QuadPart);
#else
        timespec value{};
        if (clock_gettime(CLOCK_MONOTONIC, &value) != 0)
        {
            return 0;
        }
        const std::uint64_t ticks = static_cast<std::uint64_t>(value.tv_sec) * 1'000'000'000ULL
            + static_cast<std::uint64_t>(value.tv_nsec);
        return std::bit_cast<std::int64_t>(ticks);
#endif
    }

    void SleepOneMillisecond()
    {
#if defined(_WIN32)
        Sleep(1);
#else
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
#endif
    }

    void SetCurrentThreadName(const char* name) noexcept
    {
#if defined(_WIN32)
        wchar_t wide[32]{};
        int count = MultiByteToWideChar(CP_UTF8, 0, name, -1, wide,
            static_cast<int>(sizeof(wide) / sizeof(wide[0])));
        if (count > 0)
        {
            using SetThreadDescriptionFn = HRESULT (WINAPI*)(HANDLE, PCWSTR);
            HMODULE kernel = GetModuleHandleW(L"Kernel32.dll");
            if (kernel != nullptr)
            {
                auto function = reinterpret_cast<SetThreadDescriptionFn>(
                    GetProcAddress(kernel, "SetThreadDescription"));
                if (function != nullptr)
                {
                    (void)function(GetCurrentThread(), wide);
                }
            }
        }
#elif defined(__APPLE__)
        (void)pthread_setname_np(name);
#else
        (void)pthread_setname_np(pthread_self(), name);
#endif
    }

    [[nodiscard]] sockaddr_in ToSockAddr(const System::Net::IPEndPoint& endpoint)
    {
        sockaddr_in address{};
        address.sin_family = AF_INET;
        const std::array<std::uint8_t, 4> bytes = endpoint.AddressBytes();
        std::memcpy(&address.sin_addr.s_addr, bytes.data(), bytes.size());
        address.sin_port = htons(static_cast<std::uint16_t>(endpoint.Port()));
        return address;
    }

    [[nodiscard]] std::shared_ptr<System::Net::IPEndPoint> FromSockAddr(
        const sockaddr_in& address)
    {
        std::array<std::uint8_t, 4> bytes{};
        std::memcpy(bytes.data(), &address.sin_addr.s_addr, bytes.size());
        return std::make_shared<System::Net::IPEndPoint>(
            bytes, static_cast<std::int32_t>(ntohs(address.sin_port)));
    }

    class UdpSocket final
    {
    public:
        UdpSocket()
        {
            EnsureWinsock();
            const SocketHandle handle = ::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
            if (handle == InvalidSocket)
            {
                throw NativeSocketException(LastSocketError(), "socket");
            }
            _handle.store(ToBits(handle), std::memory_order_seq_cst);
        }

        ~UdpSocket()
        {
            Dispose();
        }

        UdpSocket(const UdpSocket&) = delete;
        UdpSocket& operator=(const UdpSocket&) = delete;

        void DisableUdpConnectionResetOnWindows()
        {
#if defined(_WIN32)
            const SocketHandle handle = GetHandleOrThrow();
            DWORD disabled = FALSE;
            DWORD bytesReturned = 0;
            constexpr DWORD SioUdpConnReset = 0x9800000CU;
            const int result = WSAIoctl(handle, SioUdpConnReset,
                &disabled, static_cast<DWORD>(sizeof(disabled)), nullptr, 0,
                &bytesReturned, nullptr, nullptr);
            if (result == SOCKET_ERROR)
            {
                throw NativeSocketException(LastSocketError(), "WSAIoctl(SIO_UDP_CONNRESET)");
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
            const SocketHandle handle = GetHandleOrThrow();
#if defined(_WIN32)
            const DWORD timeout = static_cast<DWORD>(milliseconds);
            if (setsockopt(handle, SOL_SOCKET, SO_RCVTIMEO,
                    reinterpret_cast<const char*>(&timeout), sizeof(timeout)) == SOCKET_ERROR)
            {
                throw NativeSocketException(LastSocketError(), "setsockopt(SO_RCVTIMEO)");
            }
#else
            timeval timeout{};
            timeout.tv_sec = milliseconds / 1000;
            timeout.tv_usec = (milliseconds % 1000) * 1000;
            if (setsockopt(handle, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) != 0)
            {
                throw NativeSocketException(LastSocketError(), "setsockopt(SO_RCVTIMEO)");
            }
#endif
        }

        void Bind(std::int32_t port)
        {
            if (port < 0 || port > 65535)
            {
                throw std::out_of_range("port");
            }
            const SocketHandle handle = GetHandleOrThrow();
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
                throw NativeSocketException(LastSocketError(), "bind");
            }
        }

        [[nodiscard]] std::int32_t LocalPort() const
        {
            const SocketHandle handle = GetHandleOrThrow();
            sockaddr_in address{};
#if defined(_WIN32)
            int length = sizeof(address);
            if (getsockname(handle, reinterpret_cast<sockaddr*>(&address), &length) == SOCKET_ERROR)
#else
            socklen_t length = sizeof(address);
            if (getsockname(handle, reinterpret_cast<sockaddr*>(&address), &length) != 0)
#endif
            {
                throw NativeSocketException(LastSocketError(), "getsockname");
            }
            return static_cast<std::int32_t>(ntohs(address.sin_port));
        }

        [[nodiscard]] std::shared_ptr<std::vector<std::uint8_t>> Receive(
            std::shared_ptr<System::Net::IPEndPoint>& sender)
        {
            const SocketHandle handle = GetHandleOrThrow();
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
                    throw NativeObjectDisposedException("System.Net.Sockets.Socket");
                }
                throw NativeSocketException(error, "recvfrom");
            }
            data->resize(static_cast<std::size_t>(count));
            sender = FromSockAddr(from);
            return data;
        }

        void Send(std::span<const std::uint8_t> datagram,
            const std::shared_ptr<System::Net::IPEndPoint>& target)
        {
            if (!target)
            {
                throw std::invalid_argument("target");
            }
            std::lock_guard sendLock(_sendLock);
            const SocketHandle handle = GetHandleOrThrow();
            const sockaddr_in address = ToSockAddr(*target);
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
                    throw NativeObjectDisposedException("System.Net.Sockets.Socket");
                }
                throw NativeSocketException(error, "sendto");
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
            const SocketHandle handle = FromBits(oldBits);
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

    private:
        [[nodiscard]] static std::uintptr_t ToBits(SocketHandle handle) noexcept
        {
#if defined(_WIN32)
            return static_cast<std::uintptr_t>(handle);
#else
            return static_cast<std::uintptr_t>(static_cast<std::intptr_t>(handle));
#endif
        }

        [[nodiscard]] static SocketHandle FromBits(std::uintptr_t bits) noexcept
        {
#if defined(_WIN32)
            return static_cast<SocketHandle>(bits);
#else
            return static_cast<SocketHandle>(static_cast<std::intptr_t>(bits));
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

        [[nodiscard]] SocketHandle GetHandleOrThrow() const
        {
            const SocketHandle handle = FromBits(_handle.load(std::memory_order_seq_cst));
            if (handle == InvalidSocket || _disposed.load(std::memory_order_seq_cst))
            {
                throw NativeObjectDisposedException("System.Net.Sockets.Socket");
            }
            return handle;
        }

        void SetIntOption(int option, std::int32_t value, const char* operation)
        {
            const SocketHandle handle = GetHandleOrThrow();
#if defined(_WIN32)
            if (setsockopt(handle, SOL_SOCKET, option,
                    reinterpret_cast<const char*>(&value), sizeof(value)) == SOCKET_ERROR)
#else
            if (setsockopt(handle, SOL_SOCKET, option, &value, sizeof(value)) != 0)
#endif
            {
                throw NativeSocketException(LastSocketError(), operation);
            }
        }

        std::atomic<std::uintptr_t> _handle{InvalidBits()};
        std::atomic<bool> _disposed{false};
        std::mutex _sendLock;
    };
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

namespace MphRead::Mods::Network
{
    ReceivedPacket::ReceivedPacket(std::shared_ptr<System::Net::IPEndPoint> sender,
        std::shared_ptr<std::vector<std::uint8_t>> data, std::int32_t length) noexcept
        : Sender(std::move(sender)), Data(std::move(data)), Length(length)
    {
    }

    ReceivedPacket::ReceivedPacket(ReceivedPacket&& other) noexcept
        : Sender(other.Sender), Data(other.Data), Length(other.Length)
    {
    }

    ReceivedPacket& ReceivedPacket::operator=(const ReceivedPacket& other) noexcept
    {
        if (this != &other)
        {
            this->~ReceivedPacket();
            new (this) ReceivedPacket(other);
        }
        return *this;
    }

    ReceivedPacket& ReceivedPacket::operator=(ReceivedPacket&& other) noexcept
    {
        if (this != &other)
        {
            this->~ReceivedPacket();
            new (this) ReceivedPacket(std::move(other));
        }
        return *this;
    }

    PacketType ReceivedPacket::Type() const
    {
        if (Length <= 0)
        {
            return static_cast<PacketType>(0);
        }
        if (!Data)
        {
            throw System::NullReferenceException();
        }
        return static_cast<PacketType>(Data->at(0));
    }

    std::span<const std::uint8_t> ReceivedPacket::Payload() const
    {
        if (!Data || Length < 1
            || static_cast<std::size_t>(Length) > Data->size())
        {
            throw std::out_of_range("length");
        }
        return std::span<const std::uint8_t>(Data->data() + 1,
            static_cast<std::size_t>(Length - 1));
    }

    struct NetTransport::State
    {
        struct HeldIn
        {
            std::int64_t DueAt = 0;
            ReceivedPacket Packet{};
        };

        struct HeldOut
        {
            std::int64_t DueAt = 0;
            std::shared_ptr<System::Net::IPEndPoint> Target{};
            std::shared_ptr<std::vector<std::uint8_t>> Data{};
            std::int32_t Length = 0;
        };

        [[nodiscard]] bool TryDequeueInbox(std::optional<ReceivedPacket>& packet)
        {
            {
                std::lock_guard lock(InboxLock);
                if (Inbox.empty())
                {
                    packet.reset();
                    return false;
                }
                packet.emplace(Inbox.front());
                Inbox.pop_front();
            }
            InboxCount.fetch_sub(1, std::memory_order_seq_cst);
            return true;
        }

        [[nodiscard]] bool TryDropOldestInbox()
        {
            {
                std::lock_guard lock(InboxLock);
                if (Inbox.empty())
                {
                    return false;
                }
                Inbox.pop_front();
            }
            InboxCount.fetch_sub(1, std::memory_order_seq_cst);
            return true;
        }

        void EnqueueInbox(const ReceivedPacket& packet)
        {
            InboxCount.fetch_add(1, std::memory_order_seq_cst);
            std::lock_guard lock(InboxLock);
            Inbox.push_back(packet);
        }

        void PromoteHeldArrivals()
        {
            const std::int64_t now = StopwatchTimestamp();
            while (true)
            {
                std::optional<ReceivedPacket> packet;
                {
                    std::lock_guard lock(HeldLock);
                    if (HeldInput.empty() || HeldInput.front().DueAt > now)
                    {
                        return;
                    }
                    packet.emplace(HeldInput.front().Packet);
                    HeldInput.pop_front();
                }
                EnqueueInbox(*packet);
            }
        }

        void SendNow(const std::shared_ptr<System::Net::IPEndPoint>& target,
            std::span<const std::uint8_t> datagram)
        {
            try
            {
                Socket->Send(datagram, target);
                NetTransport::TotalPacketsSent.fetch_add(1, std::memory_order_seq_cst);
            }
            catch (const NativeSocketException&)
            {
            }
            catch (const NativeObjectDisposedException&)
            {
            }
        }

        void SendPacket(const std::shared_ptr<System::Net::IPEndPoint>& target,
            PacketType type, std::span<const std::uint8_t> payload,
            std::int64_t extraHoldTicks)
        {
            if (payload.size() > static_cast<std::size_t>(NetConfig::MaxPacketSize - 1))
            {
                throw std::invalid_argument("Destination is too short. (Parameter 'destination')");
            }

            std::array<std::uint8_t, NetConfig::MaxPacketSize> buffer;
            buffer[0] = static_cast<std::uint8_t>(type);
            if (!payload.empty())
            {
                std::memcpy(buffer.data() + 1, payload.data(), payload.size());
            }
            const std::int32_t length = static_cast<std::int32_t>(payload.size() + 1);

            if (LagWorkerPresent.load(std::memory_order_seq_cst))
            {
                if (NetLag::Drops())
                {
                    return;
                }
                const std::int64_t holdFor = AddInt64Unchecked(NetLag::HoldTicks(), extraHoldTicks);
                if (holdFor > 0)
                {
                    auto copy = std::make_shared<std::vector<std::uint8_t>>(
                        buffer.begin(), buffer.begin() + length);
                    std::lock_guard lock(HeldLock);
                    HeldOutput.push_back(HeldOut{
                        AddInt64Unchecked(StopwatchTimestamp(), holdFor),
                        target, std::move(copy), length});
                    return;
                }
            }

            SendNow(target,
                std::span<const std::uint8_t>(buffer.data(), static_cast<std::size_t>(length)));
        }

        void LagLoop()
        {
            SetCurrentThreadName("MphRead net lag");
            while (Running.load(std::memory_order_seq_cst))
            {
                const std::int64_t now = StopwatchTimestamp();
                while (true)
                {
                    std::optional<HeldOut> held;
                    {
                        std::lock_guard lock(HeldLock);
                        if (HeldOutput.empty() || HeldOutput.front().DueAt > now)
                        {
                            break;
                        }
                        held.emplace(std::move(HeldOutput.front()));
                        HeldOutput.pop_front();
                    }
                    SendNow(held->Target,
                        std::span<const std::uint8_t>(held->Data->data(),
                            static_cast<std::size_t>(held->Length)));
                }
                SleepOneMillisecond();
            }
        }

        void SignalWorkerExited()
        {
            {
                std::lock_guard lock(WorkerExitLock);
                WorkerExited = true;
            }
            WorkerExitCondition.notify_all();
        }

        void ReceiveLoopCore()
        {
            SetCurrentThreadName("MphRead net");
            const std::shared_ptr<System::Net::IPEndPoint> any
                = System::Net::IPEndPoint::Any(0);

            while (Running.load(std::memory_order_seq_cst))
            {
                try
                {
                    std::shared_ptr<System::Net::IPEndPoint> sender = any;
                    std::shared_ptr<std::vector<std::uint8_t>> data;
                    try
                    {
                        data = Socket->Receive(sender);
                    }
                    catch (const NativeSocketException& ex)
                    {
                        if (ex.TimedOut())
                        {
                            continue;
                        }
                        throw;
                    }

                    if (data->empty())
                    {
                        continue;
                    }

                    if (AutoPong.load(std::memory_order_seq_cst)
                        && static_cast<PacketType>((*data)[0]) == PacketType::Ping)
                    {
                        const std::int64_t extraHold = LagWorkerPresent.load(
                            std::memory_order_seq_cst) ? NetLag::HoldTicks() : 0;
                        SendPacket(sender, PacketType::Pong,
                            std::span<const std::uint8_t>(data->data() + 1, data->size() - 1),
                            extraHold);
                        continue;
                    }

                    if (InboxCount.load(std::memory_order_seq_cst)
                        >= NetTransport::MaxQueuedPackets)
                    {
                        (void)TryDropOldestInbox();
                        Dropped.fetch_add(1, std::memory_order_seq_cst);
                        NetTransport::TotalPacketsDropped.fetch_add(1, std::memory_order_seq_cst);
                    }

                    if (LagWorkerPresent.load(std::memory_order_seq_cst))
                    {
                        if (NetLag::Drops())
                        {
                            continue;
                        }
                        const std::int64_t holdFor = NetLag::HoldTicks();
                        if (holdFor > 0)
                        {
                            std::lock_guard lock(HeldLock);
                            HeldInput.push_back(HeldIn{
                                AddInt64Unchecked(StopwatchTimestamp(), holdFor),
                                ReceivedPacket(sender, data,
                                    static_cast<std::int32_t>(data->size()))});
                            continue;
                        }
                    }

                    EnqueueInbox(ReceivedPacket(sender, data,
                        static_cast<std::int32_t>(data->size())));
                }
                catch (const NativeSocketException&)
                {
                }
                catch (const NativeObjectDisposedException&)
                {
                    break;
                }
            }
        }

        void ReceiveLoop()
        {
            try
            {
                ReceiveLoopCore();
            }
            catch (...)
            {
                SignalWorkerExited();
                throw;
            }
            SignalWorkerExited();
        }

        std::shared_ptr<UdpSocket> Socket{};
        std::atomic<bool> Running{false};
        std::atomic<bool> AutoPong{false};
        std::atomic<bool> LagWorkerPresent{false};
        std::atomic<std::int32_t> InboxCount{0};
        std::atomic<std::int64_t> Dropped{0};
        std::int32_t LocalPort = 0;

        std::mutex InboxLock;
        std::deque<ReceivedPacket> Inbox;

        std::mutex HeldLock;
        std::deque<HeldIn> HeldInput;
        std::deque<HeldOut> HeldOutput;

        std::mutex WorkerExitLock;
        std::condition_variable WorkerExitCondition;
        bool WorkerExited = false;

        std::mutex CancellationLock;
        bool CancellationDisposed = false;
        bool CancellationRequested = false;
    };

    std::atomic<std::int64_t> NetTransport::TotalPacketsDropped{0};
    std::atomic<std::int64_t> NetTransport::TotalPacketsSent{0};

    const std::shared_ptr<System::Net::IPEndPoint> NetTransport::_playbackSender
        = System::Net::IPEndPoint::Loopback(0);

    NetTransport::DrainEnumerable::iterator::iterator(std::shared_ptr<State> state)
        : _state(std::move(state))
    {
        if (_state && _state->LagWorkerPresent.load(std::memory_order_seq_cst))
        {
            _state->PromoteHeldArrivals();
        }
        Advance();
    }

    ReceivedPacket NetTransport::DrainEnumerable::iterator::operator*() const
    {
        return *_current;
    }

    NetTransport::DrainEnumerable::iterator&
        NetTransport::DrainEnumerable::iterator::operator++()
    {
        Advance();
        return *this;
    }

    void NetTransport::DrainEnumerable::iterator::operator++(int)
    {
        Advance();
    }

    bool NetTransport::DrainEnumerable::iterator::AtEnd() const noexcept
    {
        return !_current.has_value();
    }

    void NetTransport::DrainEnumerable::iterator::Advance()
    {
        if (!_state || !_state->TryDequeueInbox(_current))
        {
            _current.reset();
        }
    }

    NetTransport::DrainEnumerable::DrainEnumerable(std::shared_ptr<State> state) noexcept
        : _state(std::move(state))
    {
    }

    NetTransport::DrainEnumerable::iterator NetTransport::DrainEnumerable::begin() const
    {
        return iterator(_state);
    }

    std::default_sentinel_t NetTransport::DrainEnumerable::end() const noexcept
    {
        return {};
    }

    NetTransport::NetTransport(std::int32_t port)
        : _state(std::make_shared<State>())
    {
        auto socket = std::make_shared<UdpSocket>();
#if defined(_WIN32)
        socket->DisableUdpConnectionResetOnWindows();
#endif
        try
        {
            socket->SetReceiveBufferSize(SocketBufferBytes);
            socket->SetSendBufferSize(SocketBufferBytes);
        }
        catch (const NativeSocketException&)
        {
        }

        socket->SetReceiveTimeout(500);
        socket->Bind(port);

        const std::shared_ptr<State> state = _state;
        state->Socket = socket;
        state->LocalPort = socket->LocalPort();
        state->Running.store(true, std::memory_order_seq_cst);

        std::thread([state]()
        {
            state->ReceiveLoop();
        }).detach();

        if (NetLag::Active())
        {
            state->LagWorkerPresent.store(true, std::memory_order_seq_cst);
            std::thread([state]()
            {
                state->LagLoop();
            }).detach();
        }
    }

    void NetTransport::AnswerPingsImmediately() noexcept
    {
        _state->AutoPong.store(true, std::memory_order_seq_cst);
    }

    std::int32_t NetTransport::LocalPort() const noexcept
    {
        return _state->LocalPort;
    }

    std::int64_t NetTransport::PacketsDropped() const noexcept
    {
        return _state->Dropped.load(std::memory_order_seq_cst);
    }

    NetTransport::DrainEnumerable NetTransport::Drain()
    {
        return DrainEnumerable(_state);
    }

    void NetTransport::EnqueueForPlayback(
        const std::shared_ptr<std::vector<std::uint8_t>>& data, std::int32_t length)
    {
        if (_state->InboxCount.load(std::memory_order_seq_cst) >= MaxQueuedPackets)
        {
            return;
        }
        _state->EnqueueInbox(ReceivedPacket(_playbackSender, data, length));
    }

    void NetTransport::Send(const std::shared_ptr<System::Net::IPEndPoint>& target,
        PacketType type, std::span<const std::uint8_t> payload, std::int64_t extraHoldTicks)
    {
        _state->SendPacket(target, type, payload, extraHoldTicks);
    }

    void NetTransport::Dispose()
    {
        const std::shared_ptr<State> state = _state;
        state->Running.store(false, std::memory_order_seq_cst);

        {
            std::lock_guard lock(state->CancellationLock);
            if (state->CancellationDisposed)
            {
                throw NativeObjectDisposedException("System.Threading.CancellationTokenSource");
            }
            state->CancellationRequested = true;
        }

        state->Socket->Dispose();

        {
            std::unique_lock lock(state->WorkerExitLock);
            if (!state->WorkerExited)
            {
                (void)state->WorkerExitCondition.wait_for(lock, std::chrono::seconds(1),
                    [&state]() { return state->WorkerExited; });
            }
        }

        {
            std::lock_guard lock(state->CancellationLock);
            state->CancellationDisposed = true;
        }
    }
}
