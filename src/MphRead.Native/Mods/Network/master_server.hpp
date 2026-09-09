#pragma once

#include "Mods/Network/dedicated_server.hpp"
#include "Mods/Network/net_master.hpp"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <memory>
#include <map>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

namespace fruityprime {

struct MasterOptions {
    std::uint16_t port = net::NetMasterConfig::DefaultPort;
    int host_port_first = 0;
    int host_port_last = -1;
    std::string public_address;
};

class MasterServer {
public:
    explicit MasterServer(MasterOptions options = {});
    MasterServer(const MasterServer&) = delete;
    MasterServer& operator=(const MasterServer&) = delete;
    ~MasterServer();

    void set_host_ports(int first, int last) noexcept;
    [[nodiscard]] bool can_host() const noexcept;
    bool set_public_address(std::string_view host);

    void run(std::chrono::milliseconds duration = {});
    void stop() noexcept { running_.store(false); }
    [[nodiscard]] bool listening() const noexcept { return listening_.load(); }
    [[nodiscard]] std::uint16_t bound_port() const noexcept;

private:
    struct Entry {
        net::Endpoint key;
        std::uint32_t address = 0;
        std::uint16_t port = 0;
        std::uint8_t players = 0;
        std::uint8_t max_players = 0;
        std::uint8_t mode = 0;
        std::uint8_t protocol = net::NetConfig::ProtocolVersion;
        std::string server_name;
        std::string room_key;
        double last_seen = 0;
    };

    struct Hosted {
        std::unique_ptr<DedicatedServer> server;
        std::thread thread;
        int port = 0;
        std::string name;
        net::Endpoint asker;
        double started_at = 0;
        double last_occupied = 0;
    };

    void handle(const net::ReceivedPacket& packet, double now);
    void handle_heartbeat(const net::ReceivedPacket& packet, double now);
    void handle_farewell(const net::ReceivedPacket& packet);
    void handle_host_request(const net::ReceivedPacket& packet, double now);
    void send_list(const net::Endpoint& target);
    [[nodiscard]] net::HostReplyPacket start_hosted(
        const net::HostRequestPacket& request, const net::Endpoint& asker,
        double now);
    [[nodiscard]] int free_host_port(double now);
    void reap_hosted(double now);
    void stop_hosted(std::size_t index, std::string_view reason, double now);
    void unlist(int port);
    void expire(double now);
    void shutdown();

    static bool is_local_address(const sockaddr_in& address) noexcept;
    static std::uint32_t address_number(const sockaddr_in& address) noexcept;
    static void log(std::string_view message);

    MasterOptions options_;
    std::unique_ptr<net::NetTransport> transport_;
    std::vector<Entry> entries_;
    std::vector<Hosted> hosted_;
    std::map<int, double> cooling_;
    std::atomic<bool> running_{false};
    std::atomic<bool> listening_{false};
    std::atomic<std::uint16_t> bound_port_{0};
    std::uint32_t public_address_ = 0;
    std::string public_name_;
};

} // namespace fruityprime
