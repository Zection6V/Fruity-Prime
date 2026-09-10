#pragma once

#include "Mods/Network/net_protocol.hpp"

#ifdef _WIN32
#include <winsock2.h>
#else
#include <netinet/in.h>
#endif

#include <atomic>
#include <chrono>
#include <cstdint>
#include <deque>
#include <mutex>
#include <random>
#include <span>
#include <string>
#include <thread>
#include <vector>

namespace fruityprime::net {

struct Endpoint {
    sockaddr_in address{};

    [[nodiscard]] bool operator==(const Endpoint& other) const noexcept;
    [[nodiscard]] std::string to_string() const;
};

struct ReceivedPacket {
    Endpoint sender;
    std::vector<std::uint8_t> data;

    [[nodiscard]] PacketType type() const noexcept;
    [[nodiscard]] std::span<const std::uint8_t> payload() const noexcept;
};

// Value snapshot used by the transport. The managed NetLag setting itself is
// process-global; this value remains available for isolated native callers
// that explicitly inject a condition set.
struct NetworkConditions {
    int round_trip_ms = 0;
    int jitter_ms = 0;
    double loss_percent = 0.0;

    [[nodiscard]] bool active() const noexcept {
        return round_trip_ms > 0 || loss_percent > 0.0;
    }

    // Parse the managed -netlag/-netloss option shapes. Empty values mean
    // the real line; malformed values throw instead of silently disabling a
    // requested reproduction.
    [[nodiscard]] static NetworkConditions from_options(
        std::string_view netlag, std::string_view netloss);
    [[nodiscard]] std::string describe() const;
};

class NetTransport {
public:
    explicit NetTransport(std::uint16_t port);
    NetTransport(std::uint16_t port, NetworkConditions conditions);
    NetTransport(const NetTransport&) = delete;
    NetTransport& operator=(const NetTransport&) = delete;
    ~NetTransport();

    [[nodiscard]] std::uint16_t local_port() const noexcept { return local_port_; }
    [[nodiscard]] std::uint64_t packets_dropped() const noexcept {
        return packets_dropped_.load();
    }
    [[nodiscard]] bool running() const noexcept { return running_.load(); }

    [[nodiscard]] std::vector<ReceivedPacket> drain();

    void send(const Endpoint& target, PacketType type,
              std::span<const std::uint8_t> payload = {});

private:
    using Clock = std::chrono::steady_clock;

    struct PendingIncoming {
        Clock::time_point due_at;
        ReceivedPacket packet;
    };

    struct PendingOutgoing {
        Clock::time_point due_at;
        Endpoint target;
        std::vector<std::uint8_t> datagram;
    };

    void receive_loop();
    void lag_loop();
    void promote_held_arrivals();
    void enqueue_received(ReceivedPacket packet);
    void send_now(const Endpoint& target, std::span<const std::uint8_t> datagram);
    void close_socket() noexcept;
    [[nodiscard]] bool should_drop();
    [[nodiscard]] Clock::duration hold_duration();

#ifdef _WIN32
    SOCKET socket_ = INVALID_SOCKET;
#else
    int socket_ = -1;
#endif
    std::uint16_t local_port_ = 0;
    bool use_global_lag_ = false;
    NetworkConditions conditions_;
    std::atomic<bool> running_{false};
    std::thread worker_;
    std::thread lag_worker_;
    std::mutex inbox_mutex_;
    std::deque<ReceivedPacket> inbox_;
    std::atomic<std::uint64_t> packets_dropped_{0};
    std::mutex held_mutex_;
    std::deque<PendingIncoming> held_in_;
    std::deque<PendingOutgoing> held_out_;
    std::mutex random_mutex_;
    std::mt19937 random_{std::random_device{}()};
};

} // namespace fruityprime::net
