#include "Mods/Network/net_status.hpp"

#include "Mods/Network/net_client.hpp"
#include "Mods/Network/net_protocol.hpp"
#include "Mods/Network/net_transport.hpp"
#include "Entities/room_catalog.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <chrono>
#include <cstdint>
#include <string>
#include <thread>
#include <utility>

namespace fruityprime::net {
namespace {

[[nodiscard]] bool is_blank(std::string_view value) noexcept {
    return std::all_of(value.begin(), value.end(), [](char character) {
        return std::isspace(static_cast<unsigned char>(character)) != 0;
    });
}

[[nodiscard]] GameMode decode_mode(std::uint8_t value) noexcept {
    switch (value) {
    case static_cast<std::uint8_t>(GameMode::None): return GameMode::None;
    case static_cast<std::uint8_t>(GameMode::SinglePlayer):
        return GameMode::SinglePlayer;
    case static_cast<std::uint8_t>(GameMode::Battle): return GameMode::Battle;
    case static_cast<std::uint8_t>(GameMode::BattleTeams):
        return GameMode::BattleTeams;
    case static_cast<std::uint8_t>(GameMode::Survival): return GameMode::Survival;
    case static_cast<std::uint8_t>(GameMode::SurvivalTeams):
        return GameMode::SurvivalTeams;
    case static_cast<std::uint8_t>(GameMode::Capture): return GameMode::Capture;
    case static_cast<std::uint8_t>(GameMode::Bounty): return GameMode::Bounty;
    case static_cast<std::uint8_t>(GameMode::BountyTeams):
        return GameMode::BountyTeams;
    case static_cast<std::uint8_t>(GameMode::Nodes): return GameMode::Nodes;
    case static_cast<std::uint8_t>(GameMode::NodesTeams):
        return GameMode::NodesTeams;
    case static_cast<std::uint8_t>(GameMode::Defender): return GameMode::Defender;
    case static_cast<std::uint8_t>(GameMode::DefenderTeams):
        return GameMode::DefenderTeams;
    case static_cast<std::uint8_t>(GameMode::PrimeHunter):
        return GameMode::PrimeHunter;
    case static_cast<std::uint8_t>(GameMode::Unknown15):
        return GameMode::Unknown15;
    default:
        // NetStatus.cs uses Battle when the packet carries a byte that is not
        // defined by GameMode.
        return GameMode::Battle;
    }
}

[[nodiscard]] std::string room_display_name(std::string_view room_key) {
    const scene::RoomCatalogEntry* entry = scene::find_room(room_key);
    if (entry != nullptr && !entry->in_game_name.empty()) {
        return entry->in_game_name;
    }
    return std::string(room_key);
}

[[nodiscard]] std::string player_description(
    const MatchStatePacket& match, std::uint8_t max_players) {
    if (max_players > 0) {
        return std::to_string(match.player_count) + "/"
            + std::to_string(max_players) + " players";
    }
    if (match.player_count == 0) {
        return "nobody playing yet";
    }
    if (match.player_count == 1) {
        return "1 player";
    }
    return std::to_string(match.player_count) + " players";
}

[[nodiscard]] ServerStatus describe(const ServerStatusPacket& packet,
                                    bool legacy, int latency_ms) {
    const MatchStatePacket& match = packet.match;
    const GameMode mode = decode_mode(match.mode);
    const std::string room = room_display_name(match.room_key);
    const std::string players = player_description(match, packet.max_players);
    std::string message = !room.empty()
        ? room + " · " + mode_name(mode) + " · " + players
        : players;
    if (!packet.server_name.empty()) {
        message = packet.server_name + " · " + message;
    }

    ServerStatus result;
    result.online = true;
    result.room_key = match.room_key;
    result.mode = mode;
    result.players = match.player_count;
    result.max_players = packet.max_players;
    result.time_remaining = match.time_remaining;
    result.server_name = packet.server_name;
    result.latency_ms = latency_ms;
    result.message = std::move(message);
    result.legacy = legacy;
    // A legacy Hello/Bye response has no status protocol field. This is 0 in
    // the managed struct's default value, even though a normal StatusReply
    // carries the current wire version.
    result.protocol = legacy ? 0 : packet.protocol;
    return result;
}

void send_bye(NetTransport& transport, const Endpoint& endpoint) noexcept {
    try {
        transport.send(endpoint, PacketType::Bye);
    } catch (...) {
        // The probe is ending; a server that did not accept Bye is already
        // handled by NetTransport's socket teardown.
    }
}

[[nodiscard]] ServerStatus join_probe(const Endpoint& endpoint,
                                      std::string_view address,
                                      std::chrono::milliseconds timeout) {
    try {
        NetTransport transport(0);
        const std::array<std::uint8_t, 2> hello{
            NetConfig::ProtocolVersion, 0xff
        };
        transport.send(endpoint, PacketType::Hello, hello);
        const auto deadline = std::chrono::steady_clock::now() + timeout;
        bool welcomed = false;
        while (std::chrono::steady_clock::now() < deadline) {
            for (const ReceivedPacket& packet : transport.drain()) {
                if (!(packet.sender == endpoint)) {
                    continue;
                }
                if (packet.type() == PacketType::Welcome) {
                    welcomed = true;
                    continue;
                }
                if (packet.type() != PacketType::MatchState) {
                    continue;
                }
                const auto match = MatchStatePacket::decode(packet.payload());
                if (!match) {
                    send_bye(transport, endpoint);
                    return ServerStatus::offline(
                        "Cannot reach " + std::string(address)
                        + ": server sent a malformed match state.");
                }
                send_bye(transport, endpoint);
                ServerStatusPacket status;
                status.match = *match;
                status.max_players = 0;
                status.protocol = 0;
                status.server_name.clear();
                // The probe temporarily occupies a slot. Do not expose it as
                // a real player in the browser's result.
                status.match.player_count = static_cast<std::uint8_t>(
                    std::max(0, static_cast<int>(status.match.player_count) - 1));
                return describe(status, true, -1);
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        if (welcomed) {
            send_bye(transport, endpoint);
            ServerStatus result;
            result.online = true;
            result.legacy = true;
            result.latency_ms = -1;
            result.message = "Online · the server did not say what is running.";
            return result;
        }
    } catch (...) {
        // UDP errors are intentionally presented as the same offline state as
        // an unanswered probe, matching the launcher-facing managed API.
    }
    return ServerStatus::offline(
        "No answer from " + std::string(address)
        + ". It may be off, or a firewall may be blocking UDP.");
}

} // namespace

ServerStatus ServerStatus::offline(std::string message) {
    ServerStatus result;
    result.latency_ms = -1;
    result.message = std::move(message);
    return result;
}

std::string mode_name(GameMode mode) {
    const std::string raw = fruityprime::game_mode_name(mode);
    std::string result;
    result.reserve(raw.size() + 4);
    for (std::size_t i = 0; i < raw.size(); ++i) {
        if (i > 0 && std::isupper(static_cast<unsigned char>(raw[i])) != 0) {
            result.push_back(' ');
        }
        result.push_back(raw[i]);
    }
    return result;
}

ServerStatus query(std::string_view address, std::uint16_t port,
                   bool allow_join_probe, std::chrono::milliseconds timeout) {
    if (is_blank(address)) {
        return ServerStatus::offline("No server address.");
    }

    Endpoint endpoint;
    try {
        endpoint = NetClient::resolve_ipv4(address, port);
    } catch (...) {
        return ServerStatus::offline("Cannot find " + std::string(address) + ".");
    }

    const ServerStatusResult status = NetClient::query_status(
        address, port, timeout);
    if (status.online) {
        ServerStatusPacket packet;
        packet.match = status.match;
        packet.max_players = status.max_players;
        packet.protocol = status.protocol;
        packet.server_name = status.server_name;
        return describe(packet, false, status.latency_ms);
    }
    if (!allow_join_probe) {
        return ServerStatus::offline(
            "No answer from " + std::string(address) + ":"
            + std::to_string(port) + ".");
    }
    return join_probe(endpoint, address, timeout);
}

} // namespace fruityprime::net
