#pragma once

#include "Mods/Network/net_transport.hpp"

#include <chrono>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace fruityprime::net {

struct ServerStatusResult {
    bool online = false;
    MatchStatePacket match;
    std::uint8_t max_players = 0;
    std::uint8_t protocol = 0;
    std::string server_name;
    int latency_ms = -1;
    std::string error;
};

class NetClient {
public:
    explicit NetClient(Endpoint server, NetworkConditions conditions = {});
    NetClient(const NetClient&) = delete;
    NetClient& operator=(const NetClient&) = delete;
    ~NetClient();

    [[nodiscard]] static Endpoint resolve_ipv4(std::string_view host,
                                               std::uint16_t port);

    bool connect(std::uint8_t requested_slot = 0xff,
                 std::chrono::milliseconds timeout = std::chrono::milliseconds(1500));
    void disconnect() noexcept;

    void identify(std::uint8_t hunter, std::string_view name);
    void send(PacketType type, std::span<const std::uint8_t> payload = {});

    [[nodiscard]] std::vector<ReceivedPacket> drain();
    [[nodiscard]] bool connected() const noexcept { return connected_; }
    [[nodiscard]] int local_slot() const noexcept { return local_slot_; }
    [[nodiscard]] std::uint16_t local_port() const noexcept;
    [[nodiscard]] std::string_view last_error() const noexcept {
        return last_error_;
    }

    [[nodiscard]] static ServerStatusResult query_status(
        std::string_view host, std::uint16_t port,
        std::chrono::milliseconds timeout = std::chrono::milliseconds(1200));

private:
    Endpoint server_;
    NetworkConditions conditions_;
    std::unique_ptr<NetTransport> transport_;
    bool connected_ = false;
    int local_slot_ = -1;
    std::string last_error_;
};

} // namespace fruityprime::net
