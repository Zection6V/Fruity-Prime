#pragma once

#include "Mods/Network/map_rotation.hpp"

#include <chrono>
#include <cstdint>
#include <string>
#include <string_view>

namespace fruityprime::net {

// The complete value shown by the launcher for one server-browser row.  The
// fields intentionally mirror NetStatus.ServerStatus rather than exposing
// the packet layout to UI code.
struct ServerStatus {
    bool online = false;
    std::string room_key;
    GameMode mode = GameMode::Battle;
    int players = 0;
    int max_players = 0;
    float time_remaining = 0.0F;
    std::string server_name;
    int latency_ms = -1;
    std::string message;
    bool legacy = false;
    int protocol = 0;

    [[nodiscard]] static ServerStatus offline(std::string message);
};

[[nodiscard]] ServerStatus query(
    std::string_view address, std::uint16_t port, bool allow_join_probe,
    std::chrono::milliseconds timeout = std::chrono::milliseconds(1200));

// The browser uses spaced title-case names while the packet and the metadata
// catalogue retain the enum spelling (for example, BattleTeams -> Battle
// Teams).
[[nodiscard]] std::string mode_name(GameMode mode);

} // namespace fruityprime::net
