#pragma once

// System.Net and System.Net.Sockets: IPAddress, IPEndPoint, Dns and the
// UdpClient members this program uses. The parsing and the exceptions follow
// the managed ones, including IPAddress.TryParse's legacy IPv4 forms.

#include <array>
#include <cstdint>
#include <span>
#include <memory>
#include <string>
#include <system_error>
#include <vector>

namespace System::Net
{
    // Thin IPv4 IPEndPoint equivalent. Instances are passed by shared_ptr so
    // queued packets retain the same reference identity as System.Net.IPEndPoint
    // references in C#.
    class IPEndPoint final
    {
    public:
        IPEndPoint(std::array<std::uint8_t, 4> addressBytes, std::int32_t port);
        ~IPEndPoint() = default;

        IPEndPoint(const IPEndPoint&) = delete;
        IPEndPoint(IPEndPoint&&) = delete;
        IPEndPoint& operator=(const IPEndPoint&) = delete;
        IPEndPoint& operator=(IPEndPoint&&) = delete;

        [[nodiscard]] static std::shared_ptr<IPEndPoint> Any(std::int32_t port = 0);
        [[nodiscard]] static std::shared_ptr<IPEndPoint> Loopback(std::int32_t port = 0);

        [[nodiscard]] std::array<std::uint8_t, 4> AddressBytes() const;
        void SetAddressBytes(std::array<std::uint8_t, 4> addressBytes);
        [[nodiscard]] std::int32_t Port() const;
        void SetPort(std::int32_t port);

        [[nodiscard]] bool Equals(const IPEndPoint& other) const;
        [[nodiscard]] std::string ToString() const;

    private:
        struct State;
        std::shared_ptr<State> _state;
    };
}

namespace MphRead::NativeRuntime
{
    enum class AddressFamily : std::uint8_t
    {
        InterNetwork,
        Other
    };

    struct Address
    {
        AddressFamily Family = AddressFamily::Other;
        std::array<std::uint8_t, 4> Bytes{};
    };

    struct EndPoint
    {
        ::MphRead::NativeRuntime::Address Address{};
        std::int32_t Port = 0;
    };

    // IPEndPoint.Equals(other): the same address family, bytes and port.
    [[nodiscard]] inline bool EndPointEquals(const EndPoint& left, const EndPoint& right) noexcept
    {
        return left.Address.Family == right.Address.Family && left.Address.Bytes == right.Address.Bytes
            && left.Port == right.Port;
    }

    struct SocketState;
    using SocketHandle = SocketState*;

    // SocketException: the managed exception every socket call throws on
    // failure, carrying the platform's own error number.
    class SocketException final : public std::system_error
    {
    public:
        SocketException(int error, std::string operation);
    };

    // IPAddress.ToString(): the dotted-quad form.
    [[nodiscard]] std::string AddressToString(const Address& address);
    // Dns.GetHostAddresses(address).
    [[nodiscard]] std::vector<Address> DnsGetHostAddresses(const std::string& address);
    // new IPEndPoint(address, port).
    [[nodiscard]] EndPoint CreateIPEndPoint(const Address& address, std::int32_t port);
    // new IPEndPoint(IPAddress.Any, 0).
    [[nodiscard]] EndPoint CreateIPv4AnyEndPoint();

    // new UdpClient(AddressFamily.InterNetwork).
    [[nodiscard]] SocketHandle UdpClientCreateInterNetwork();
    // client.Client.ReceiveTimeout = timeoutMs.
    void UdpClientSetReceiveTimeout(SocketHandle socket, std::int32_t timeoutMs);
    // client.Send(datagram, length, endPoint).
    void UdpClientSend(
        SocketHandle socket, const std::uint8_t* data, std::int32_t length,
        const EndPoint& endPoint);
    // client.Receive(ref remote).
    [[nodiscard]] std::vector<std::uint8_t> UdpClientReceive(
        SocketHandle socket, EndPoint& remote);
    // The same UdpClient members against a reference-typed IPEndPoint.
    void UdpClientSend(
        SocketHandle socket, const std::uint8_t* data, std::int32_t length,
        const std::shared_ptr<System::Net::IPEndPoint>& endPoint);
    [[nodiscard]] std::vector<std::uint8_t> UdpClientReceive(
        SocketHandle socket, std::shared_ptr<System::Net::IPEndPoint>& remote);
    void UdpClientSend(
        SocketHandle socket, std::span<const std::uint8_t> datagram, const EndPoint& endPoint);
    void UdpClientSend(
        SocketHandle socket, std::span<const std::uint8_t> datagram,
        const std::shared_ptr<System::Net::IPEndPoint>& endPoint);
    // client.Dispose().
    void UdpClientDispose(SocketHandle socket);

    // Whether a SocketException carries SocketError.TimedOut.
    [[nodiscard]] bool SocketErrorIsTimeout(const std::system_error& error) noexcept;

    // The one socket behind `new UdpClient(port)` and its `Client` options,
    // as the network transport drives it: bound once, received from on one
    // thread and sent to from others, and disposed from any of them -- which
    // is what unblocks a receive, as closing a managed Socket does.
    class UdpSocket final
    {
    public:
        UdpSocket();
        ~UdpSocket();
        UdpSocket(const UdpSocket&) = delete;
        UdpSocket& operator=(const UdpSocket&) = delete;

        // SIO_UDP_CONNRESET off, so an ICMP port-unreachable does not fail
        // the next receive. Nothing elsewhere.
        void DisableUdpConnectionResetOnWindows();
        void SetReceiveBufferSize(std::int32_t bytes);
        void SetSendBufferSize(std::int32_t bytes);
        void SetReceiveTimeout(std::int32_t milliseconds);
        void Bind(std::int32_t port);
        [[nodiscard]] std::int32_t LocalPort() const;
        // Receive(ref sender): SocketException on failure, ObjectDisposedException
        // once disposed.
        [[nodiscard]] std::shared_ptr<std::vector<std::uint8_t>> Receive(
            std::shared_ptr<System::Net::IPEndPoint>& sender);
        void Send(std::span<const std::uint8_t> datagram,
            const std::shared_ptr<System::Net::IPEndPoint>& target);
        void Dispose() noexcept;

    private:
        struct Impl;
        std::unique_ptr<Impl> _impl;
    };
}
