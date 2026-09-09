#include "Mods/Network/net_transport.hpp"
#include "Mods/Network/net_lag.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstring>
#include <iomanip>
#include <iterator>
#include <sstream>
#include <stdexcept>
#include <system_error>

#ifdef _WIN32
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <cerrno>
#include <fcntl.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

namespace fruityprime::net {
namespace {

constexpr std::size_t MaxQueuedPackets = 2048;
constexpr int SocketBufferBytes = 1 << 20;

#ifdef _WIN32
using Socket = SOCKET;
constexpr Socket InvalidSocket = INVALID_SOCKET;
using SocketLength = int;
#else
using Socket = int;
constexpr Socket InvalidSocket = -1;
using SocketLength = socklen_t;
#endif

[[nodiscard]] bool socket_error_is_transient() {
#ifdef _WIN32
    const int error = WSAGetLastError();
    return error == WSAEINTR || error == WSAEWOULDBLOCK
        || error == WSAETIMEDOUT || error == WSAECONNRESET;
#else
    return errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK
        || errno == ECONNRESET;
#endif
}

[[nodiscard]] std::string socket_error_message(const char* operation) {
#ifdef _WIN32
    return std::string(operation) + " failed (WSA error "
        + std::to_string(WSAGetLastError()) + ")";
#else
    return std::string(operation) + " failed: " + std::strerror(errno);
#endif
}

void set_socket_buffer(Socket socket, int option, int value) {
#ifdef _WIN32
    const char* data = reinterpret_cast<const char*>(&value);
    const int size = sizeof(value);
#else
    const void* data = &value;
    const socklen_t size = sizeof(value);
#endif
    static_cast<void>(setsockopt(socket, SOL_SOCKET, option, data, size));
}

void close_native_socket(Socket socket) noexcept {
    if (socket == InvalidSocket) {
        return;
    }
#ifdef _WIN32
    closesocket(socket);
#else
    close(socket);
#endif
}

} // namespace

bool Endpoint::operator==(const Endpoint& other) const noexcept {
    return address.sin_family == other.address.sin_family
        && address.sin_port == other.address.sin_port
        && address.sin_addr.s_addr == other.address.sin_addr.s_addr;
}

std::string Endpoint::to_string() const {
    std::array<char, INET_ADDRSTRLEN> host{};
    const char* text = inet_ntop(AF_INET, &address.sin_addr, host.data(),
                                 static_cast<socklen_t>(host.size()));
    if (text == nullptr) {
        return "<invalid endpoint>";
    }
    return std::string(text) + ":" + std::to_string(ntohs(address.sin_port));
}

PacketType ReceivedPacket::type() const noexcept {
    return data.empty() ? PacketType::Hello
                        : static_cast<PacketType>(data.front());
}

std::span<const std::uint8_t> ReceivedPacket::payload() const noexcept {
    if (data.size() <= 1) {
        return {};
    }
    return std::span<const std::uint8_t>(data).subspan(1);
}

NetworkConditions NetworkConditions::from_options(std::string_view netlag,
                                                  std::string_view netloss) {
    return NetLag::from_options(netlag, netloss);
}

std::string NetworkConditions::describe() const {
    return NetLag::describe(*this);
}

NetTransport::NetTransport(std::uint16_t port)
    : NetTransport(port, NetworkConditions{}) {}

NetTransport::NetTransport(std::uint16_t port, NetworkConditions conditions)
    : conditions_(conditions) {
#ifdef _WIN32
    WSADATA wsa_data{};
    const int wsa_result = WSAStartup(MAKEWORD(2, 2), &wsa_data);
    if (wsa_result != 0) {
        throw std::runtime_error("WSAStartup failed (error "
                                 + std::to_string(wsa_result) + ")");
    }
#endif

    socket_ = ::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (socket_ == InvalidSocket) {
#ifdef _WIN32
        WSACleanup();
#endif
        throw std::runtime_error(socket_error_message("socket"));
    }

    set_socket_buffer(socket_, SO_RCVBUF, SocketBufferBytes);
    set_socket_buffer(socket_, SO_SNDBUF, SocketBufferBytes);

    sockaddr_in local{};
    local.sin_family = AF_INET;
    local.sin_addr.s_addr = htonl(INADDR_ANY);
    local.sin_port = htons(port);
    if (::bind(socket_, reinterpret_cast<const sockaddr*>(&local),
               static_cast<SocketLength>(sizeof(local))) != 0) {
        const std::string message = socket_error_message("bind");
        close_native_socket(socket_);
        socket_ = InvalidSocket;
#ifdef _WIN32
        WSACleanup();
#endif
        throw std::runtime_error(message);
    }

    sockaddr_in bound{};
    SocketLength bound_length = static_cast<SocketLength>(sizeof(bound));
    if (getsockname(socket_, reinterpret_cast<sockaddr*>(&bound), &bound_length)
        != 0) {
        const std::string message = socket_error_message("getsockname");
        close_native_socket(socket_);
        socket_ = InvalidSocket;
#ifdef _WIN32
        WSACleanup();
#endif
        throw std::runtime_error(message);
    }
    local_port_ = ntohs(bound.sin_port);
    running_.store(true);
    worker_ = std::thread(&NetTransport::receive_loop, this);
    if (conditions_.round_trip_ms > 0 || conditions_.jitter_ms > 0) {
        lag_worker_ = std::thread(&NetTransport::lag_loop, this);
    }
}

NetTransport::~NetTransport() {
    running_.store(false);
    if (worker_.joinable()) {
        worker_.join();
    }
    if (lag_worker_.joinable()) {
        lag_worker_.join();
    }
    close_socket();
#ifdef _WIN32
    WSACleanup();
#endif
}

void NetTransport::close_socket() noexcept {
    if (socket_ == InvalidSocket) {
        return;
    }
    close_native_socket(socket_);
    socket_ = InvalidSocket;
}

void NetTransport::receive_loop() {
    std::array<std::uint8_t, NetConfig::MaxPacketSize> buffer{};
    while (running_.load()) {
        fd_set read_set;
        FD_ZERO(&read_set);
        FD_SET(socket_, &read_set);
        timeval timeout{};
        timeout.tv_sec = 0;
        timeout.tv_usec = 100'000;

#ifdef _WIN32
        const int selected = select(0, &read_set, nullptr, nullptr, &timeout);
#else
        const int selected = select(socket_ + 1, &read_set, nullptr, nullptr,
                                    &timeout);
#endif
        if (selected == 0) {
            continue;
        }
        if (selected < 0) {
            if (running_.load() && !socket_error_is_transient()) {
                continue;
            }
            continue;
        }

        sockaddr_in sender{};
        SocketLength sender_length = static_cast<SocketLength>(sizeof(sender));
#ifdef _WIN32
        const int received = recvfrom(socket_, reinterpret_cast<char*>(buffer.data()),
                                      static_cast<int>(buffer.size()), 0,
                                      reinterpret_cast<sockaddr*>(&sender),
                                      &sender_length);
#else
        const ssize_t received = recvfrom(socket_, buffer.data(), buffer.size(), 0,
                                          reinterpret_cast<sockaddr*>(&sender),
                                          &sender_length);
#endif
        if (received <= 0) {
            if (!running_.load() || !socket_error_is_transient()) {
                continue;
            }
            continue;
        }

        ReceivedPacket packet;
        packet.sender.address = sender;
        packet.data.assign(buffer.begin(), buffer.begin() + received);
        if (conditions_.active()) {
            if (should_drop()) {
                continue;
            }
            const auto hold_for = hold_duration();
            if (hold_for > Clock::duration::zero()) {
                std::lock_guard lock(held_mutex_);
                held_in_.push_back(PendingIncoming{
                    Clock::now() + hold_for, std::move(packet)});
                continue;
            }
        }
        enqueue_received(std::move(packet));
    }
}

void NetTransport::lag_loop() {
    while (running_.load()) {
        const auto now = Clock::now();
        while (true) {
            PendingOutgoing pending;
            {
                std::lock_guard lock(held_mutex_);
                if (held_out_.empty() || held_out_.front().due_at > now) {
                    break;
                }
                pending = std::move(held_out_.front());
                held_out_.pop_front();
            }
            try {
                send_now(pending.target, pending.datagram);
            } catch (...) {
                // A delayed datagram has no caller left to report to. A
                // transient unreachable peer must not kill the worker.
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}

void NetTransport::promote_held_arrivals() {
    const auto now = Clock::now();
    while (true) {
        PendingIncoming pending;
        {
            std::lock_guard lock(held_mutex_);
            if (held_in_.empty() || held_in_.front().due_at > now) {
                return;
            }
            pending = std::move(held_in_.front());
            held_in_.pop_front();
        }
        enqueue_received(std::move(pending.packet));
    }
}

void NetTransport::enqueue_received(ReceivedPacket packet) {
    std::lock_guard lock(inbox_mutex_);
    if (inbox_.size() >= MaxQueuedPackets) {
        // A real-time stream must discard the oldest state, not the newest
        // aim/position packet that just arrived.
        inbox_.pop_front();
        packets_dropped_.fetch_add(1);
    }
    inbox_.push_back(std::move(packet));
}

std::vector<ReceivedPacket> NetTransport::drain() {
    if (conditions_.round_trip_ms > 0 || conditions_.jitter_ms > 0) {
        promote_held_arrivals();
    }
    std::deque<ReceivedPacket> pending;
    {
        std::lock_guard lock(inbox_mutex_);
        pending.swap(inbox_);
    }
    return {std::make_move_iterator(pending.begin()),
            std::make_move_iterator(pending.end())};
}

void NetTransport::send(const Endpoint& target, PacketType type,
                        std::span<const std::uint8_t> payload) {
    if (payload.size() + 1 > NetConfig::MaxPacketSize) {
        throw std::invalid_argument("UDP payload exceeds NetConfig::MaxPacketSize");
    }
    auto datagram = make_datagram(type, payload);
    if (conditions_.active()) {
        if (should_drop()) {
            return;
        }
        const auto hold_for = hold_duration();
        if (hold_for > Clock::duration::zero()) {
            std::lock_guard lock(held_mutex_);
            held_out_.push_back(PendingOutgoing{
                Clock::now() + hold_for, target, std::move(datagram)});
            return;
        }
    }
    send_now(target, datagram);
}

bool NetTransport::should_drop() {
    if (conditions_.loss_percent <= 0.0) {
        return false;
    }
    std::lock_guard lock(random_mutex_);
    std::uniform_real_distribution<double> distribution(0.0, 1.0);
    return distribution(random_) < conditions_.loss_percent / 100.0;
}

NetTransport::Clock::duration NetTransport::hold_duration() {
    double milliseconds = conditions_.round_trip_ms / 2.0;
    if (conditions_.jitter_ms > 0) {
        std::lock_guard lock(random_mutex_);
        std::uniform_real_distribution<double> distribution(
            0.0, static_cast<double>(conditions_.jitter_ms));
        milliseconds += distribution(random_);
    }
    if (milliseconds <= 0.0) {
        return Clock::duration::zero();
    }
    return std::chrono::duration_cast<Clock::duration>(
        std::chrono::duration<double, std::milli>(milliseconds));
}

void NetTransport::send_now(const Endpoint& target,
                            std::span<const std::uint8_t> datagram) {
    if (socket_ == InvalidSocket) {
        return;
    }
#ifdef _WIN32
    const int sent = sendto(socket_, reinterpret_cast<const char*>(datagram.data()),
                            static_cast<int>(datagram.size()), 0,
                            reinterpret_cast<const sockaddr*>(&target.address),
                            static_cast<int>(sizeof(target.address)));
#else
    const ssize_t sent = sendto(socket_, datagram.data(), datagram.size(), 0,
                                reinterpret_cast<const sockaddr*>(&target.address),
                                sizeof(target.address));
#endif
    if (sent < 0) {
        throw std::runtime_error(socket_error_message("sendto"));
    }
}

} // namespace fruityprime::net
