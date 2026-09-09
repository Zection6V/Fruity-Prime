#pragma once

#include "Mods/Network/map_rotation.hpp"
#include "Mods/Network/net_master.hpp"
#include "Mods/Network/net_protocol.hpp"
#include "Mods/Network/net_transport.hpp"

#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace fruityprime {

struct ServerOptions {
    std::uint16_t port = net::NetConfig::DefaultPort;
    int max_players = 4;
    MapRotation rotation;
    std::string server_name;
    bool friendly_fire = false;
    bool advertise = true;
    std::string master_host = std::string(net::NetMasterConfig::DefaultHost);
    std::uint16_t master_port = net::NetMasterConfig::DefaultPort;
};

class MasterReporter {
public:
    MasterReporter(std::string host, std::uint16_t port);
    ~MasterReporter() noexcept;

    MasterReporter(const MasterReporter&) = delete;
    MasterReporter& operator=(const MasterReporter&) = delete;

    void beat(double now, std::string_view server_name, std::uint16_t game_port,
              std::uint8_t players, std::uint8_t max_players, std::uint8_t mode,
              std::string_view room_key);
    void farewell(std::uint16_t game_port) noexcept;

private:
    bool resolve();
    void complain(std::string_view message);

    std::string host_;
    std::uint16_t port_;
    double last_beat_ = -1e30;
    double last_resolve_ = -1e30;
    bool complained_ = false;
    bool have_endpoint_ = false;
    std::array<std::uint8_t, net::MasterHeartbeatPacket::Size> scratch_{};

    sockaddr_in endpoint_{};
#ifdef _WIN32
    SOCKET socket_ = INVALID_SOCKET;
#else
    int socket_ = -1;
#endif
};

class DedicatedServer {
public:
    explicit DedicatedServer(ServerOptions options);
    DedicatedServer(const DedicatedServer&) = delete;
    DedicatedServer& operator=(const DedicatedServer&) = delete;
    ~DedicatedServer() = default;

    // Runs until stop() is called, a signal handler requests shutdown, or the
    // optional duration expires. A zero duration means no deadline.
    void run(std::chrono::milliseconds duration = {});
    void stop() noexcept { running_.store(false); }

    [[nodiscard]] bool listening() const noexcept { return listening_.load(); }
    [[nodiscard]] std::uint16_t bound_port() const noexcept;
    [[nodiscard]] std::size_t peer_count() const noexcept { return peer_count_.load(); }
    [[nodiscard]] bool ever_occupied() const noexcept { return ever_occupied_.load(); }

private:
    struct Peer {
        net::Endpoint endpoint;
        int slot = -1;
        double last_seen = 0;
        std::uint32_t last_intent_frame = 0;
        std::string name;
        std::uint8_t hunter = 0;
        int ping = 0;
        double ping_sent_at = 0;
        std::uint8_t ping_id = 0;
        bool ping_pending = false;
        double chat_credit = 3;
        double chat_credit_at = 0;
        int chat_dropped = 0;
    };

    static constexpr double EndSequenceSeconds = 9.0;
    static constexpr double ChatRatePerSecond = 0.5;
    static constexpr double ChatBurst = 3.0;

    [[nodiscard]] int find_peer(const net::Endpoint& endpoint) const noexcept;
    [[nodiscard]] int find_peer_by_slot(int slot) const noexcept;
    [[nodiscard]] bool slot_free(int slot) const noexcept;
    [[nodiscard]] int next_free_slot() const noexcept;
    [[nodiscard]] net::MatchStatePacket build_state(double now) const;

    void handle(const net::ReceivedPacket& packet, double now);
    void handle_hello(const net::ReceivedPacket& packet, double now);
    void handle_identify(const net::ReceivedPacket& packet, double now);
    void handle_intent(const net::ReceivedPacket& packet, double now);
    void handle_snapshot(const net::ReceivedPacket& packet, double now);
    void handle_chat(const net::ReceivedPacket& packet, double now);
    void handle_pong(const net::ReceivedPacket& packet, double now);
    void handle_match_end(const net::ReceivedPacket& packet, double now);

    void send_refusal(const net::Endpoint& target, std::uint8_t reason);
    void send_status(const net::Endpoint& target, double now);
    void notify_authority(const Peer& peer);
    void broadcast_match_state(double now);
    void broadcast_roster();
    void ping_peers(double now);
    void announce(std::string_view text);
    void remove_peer(std::size_t index, std::string_view reason);
    void drop_timed_out(double now);
    void end_match(double now, std::string_view reason);
    void advance_map(double now);
    void shutdown();

    static void log(std::string_view message);

    ServerOptions options_;
    std::unique_ptr<net::NetTransport> transport_;
    std::unique_ptr<MasterReporter> reporter_;
    std::vector<Peer> peers_;
    std::vector<std::uint8_t> last_snapshot_;
    std::atomic<bool> running_{false};
    std::atomic<bool> listening_{false};
    std::atomic<std::size_t> peer_count_{0};
    std::atomic<bool> ever_occupied_{false};
    std::atomic<std::uint16_t> bound_port_{0};
    int authority_slot_ = -1;
    double match_started_ = 0;
    double match_ended_at_ = -1;
    std::uint16_t match_id_ = 1;
};

} // namespace fruityprime
