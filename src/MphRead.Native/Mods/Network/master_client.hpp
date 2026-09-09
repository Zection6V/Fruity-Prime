#pragma once

#include "Mods/Network/net_protocol.hpp"

#include <chrono>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace fruityprime::net {

struct MasterListing {
    std::string address;
    std::uint16_t port = 0;
    std::uint8_t players = 0;
    std::uint8_t max_players = 0;
    std::uint8_t mode = 0;
    std::uint8_t protocol = 0;
    std::string server_name;
    std::string room_key;
};

struct MasterListResult {
    bool answered = false;
    std::vector<MasterListing> servers;
    std::string error;
};

class MasterClient {
public:
    [[nodiscard]] static MasterListResult query(
        std::string_view host, std::uint16_t port,
        std::chrono::milliseconds timeout = std::chrono::milliseconds(1500));

    [[nodiscard]] static HostReplyPacket request_host(
        std::string_view host, std::uint16_t port,
        const HostRequestPacket& request,
        std::chrono::milliseconds timeout = std::chrono::milliseconds(3000));
};

} // namespace fruityprime::net
