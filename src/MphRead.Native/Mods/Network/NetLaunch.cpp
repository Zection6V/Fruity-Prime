#include "Mods/Network/net_launch.hpp"

#include "Metadata/metadata.hpp"
#include "Entities/Players/PlayerEntity.hpp"
#include "Mods/Network/net_protocol.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <iostream>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <thread>

namespace fruityprime::net {
namespace {

struct LaunchState {
    std::unique_ptr<NetClient> client;
    MatchStatePacket server_match;
    RosterPacket roster;
    std::uint8_t local_hunter = 0;
    bool server_match_valid = false;
    bool roster_valid = false;
    std::string last_error;
};

LaunchState& state() {
    static LaunchState value;
    return value;
}

std::uint32_t launch_seed() noexcept {
    const auto ticks = std::chrono::steady_clock::now().time_since_epoch().count();
    const auto value = static_cast<std::uint64_t>(ticks);
    return static_cast<std::uint32_t>(value ^ (value >> 32));
}

std::string describe_join_failure(std::string_view address,
                                  std::uint16_t port) {
    const ServerStatusResult status = NetClient::query_status(
        address, port, std::chrono::milliseconds(1500));
    if (!status.online) {
        return "No answer from " + std::string(address) + ":"
            + std::to_string(port)
            + ". The server may be off, or UDP may be blocked between here and it.";
    }
    if (status.max_players > 0
        && status.match.player_count >= status.max_players) {
        return std::string(address) + ":" + std::to_string(port)
            + " is full (" + std::to_string(status.match.player_count) + "/"
            + std::to_string(status.max_players)
            + " players). Try again when somebody leaves.";
    }
    if (status.protocol > 0
        && status.protocol != NetConfig::ProtocolVersion) {
        return std::string(address) + ":" + std::to_string(port)
            + " is running protocol " + std::to_string(status.protocol)
            + " and this build speaks "
            + std::to_string(NetConfig::ProtocolVersion)
            + ". One of you needs updating.";
    }
    return std::string(address) + ":" + std::to_string(port)
        + " answered, but would not admit this client ("
        + std::to_string(status.match.player_count) + "/"
        + std::to_string(status.max_players)
        + " players). It may have filled up while joining.";
}

void retain_packet(const ReceivedPacket& packet, LaunchState& current) {
    if (packet.type() == PacketType::MatchState) {
        if (const auto match = MatchStatePacket::decode(packet.payload())) {
            current.server_match = *match;
            current.server_match_valid = !match->room_key.empty();
        }
    } else if (packet.type() == PacketType::Roster) {
        if (const auto roster = RosterPacket::decode(packet.payload())) {
            current.roster = *roster;
            current.roster_valid = true;
        }
    }
}

} // namespace

bool NetLaunch::join(const JoinOptions& options) {
    disconnect();
    // NetLaunch.Join raises the player construction limit before opening the
    // network session. Room changes reuse the same limit; they must not own
    // this unrelated launch-side effect.
    players::PlayerEntity::MaxPlayers(
        players::PlayerEntity::SlotCapacity);
    LaunchState& current = state();
    current.last_error.clear();
    if (options.address.empty()) {
        current.last_error = "A server address is required.";
        return false;
    }
    current.local_hunter = options.hunter;
    if (current.local_hunter
        == static_cast<std::uint8_t>(metadata::Hunter::Random)) {
        // Random is a launcher choice, not a model row.  Resolve it once at
        // the join boundary so every retry of Identify and the later player
        // construction use the same playable hunter.
        current.local_hunter = metadata::roll_hunter(launch_seed());
    }
    const int timeout_ms = std::max(1, options.timeout_ms);
    try {
        const Endpoint endpoint = NetClient::resolve_ipv4(
            options.address, options.port);
        current.client = std::make_unique<NetClient>(endpoint, options.network);

        // NetClient's handshake is deliberately bounded separately from the
        // post-Welcome MatchState wait.  This keeps a dead UDP endpoint from
        // consuming the whole launch loop while still allowing a healthy
        // server to lose and resend its first state packet.
        const auto handshake_timeout = std::chrono::milliseconds(
            std::min(timeout_ms, 1500));
        if (!current.client->connect(0xff, handshake_timeout)) {
            current.last_error = describe_join_failure(
                options.address, options.port);
            if (current.last_error.empty()) {
                current.last_error = std::string(current.client->last_error());
            }
            std::cout << "[net] " << current.last_error << '\n';
            disconnect();
            return false;
        }

        const auto started = std::chrono::steady_clock::now();
        const auto deadline = started + std::chrono::milliseconds(timeout_ms);
        auto next_identify = started;
        current.client->identify(current.local_hunter, options.player_name);
        while (std::chrono::steady_clock::now() < deadline) {
            for (const ReceivedPacket& packet : update()) {
                // A refusal after Welcome is not expected, but retaining the
                // packet here makes the native boundary safe against a relay
                // that changes its admission while the state is arriving.
                if (packet.type() == PacketType::Refused) {
                    current.last_error = "server refused the connection";
                    disconnect();
                    return false;
                }
            }
            if (current.server_match_valid && current.client->local_slot() >= 0) {
                std::cout << "[net] joining " << current.server_match.room_key
                          << " (" << game_mode_name(static_cast<GameMode>(
                              current.server_match.mode)) << "), "
                          << current.server_match.time_remaining
                          << " s remaining, slot " << current.client->local_slot()
                          << '\n';
                if (options.cheats != nullptr) {
                    const auto disabled = disable_cheats_for_match(*options.cheats);
                    if (!disabled.empty()) {
                        std::ostringstream names;
                        for (std::size_t index = 0; index < disabled.size(); ++index) {
                            if (index != 0) {
                                names << ", ";
                            }
                            names << disabled[index];
                        }
                        std::cout << "[net] cheats are off while connected ("
                                  << names.str() << ")\n";
                    }
                }
                return true;
            }
            const auto now = std::chrono::steady_clock::now();
            if (now >= next_identify) {
                current.client->identify(current.local_hunter, options.player_name);
                next_identify = now + std::chrono::milliseconds(500);
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
        }
        current.last_error = describe_join_failure(
            options.address, options.port);
        std::cout << "[net] " << current.last_error << '\n';
        disconnect();
        return false;
    } catch (const std::exception& error) {
        current.last_error = error.what();
        disconnect();
        return false;
    }
}

void NetLaunch::disconnect() noexcept {
    LaunchState& current = state();
    current.client.reset();
    current.server_match = {};
    current.roster = {};
    current.local_hunter = 0;
    current.server_match_valid = false;
    current.roster_valid = false;
}

std::vector<ReceivedPacket> NetLaunch::update() {
    LaunchState& current = state();
    if (current.client == nullptr) {
        return {};
    }
    std::vector<ReceivedPacket> packets = current.client->drain();
    for (const ReceivedPacket& packet : packets) {
        retain_packet(packet, current);
    }
    return packets;
}

bool NetLaunch::active() noexcept {
    return state().client != nullptr && state().client->connected();
}

int NetLaunch::local_slot() noexcept {
    return state().client == nullptr ? -1 : state().client->local_slot();
}

std::uint8_t NetLaunch::local_hunter() noexcept {
    return state().local_hunter;
}

std::string_view NetLaunch::last_join_error() noexcept {
    return state().last_error;
}

std::optional<LaunchRoom> NetLaunch::server_room() {
    const LaunchState& current = state();
    if (!current.server_match_valid || current.server_match.room_key.empty()) {
        return std::nullopt;
    }
    const auto mode = static_cast<GameMode>(current.server_match.mode);
    const bool known = mode >= GameMode::SinglePlayer
        && mode <= GameMode::Unknown15;
    return LaunchRoom{current.server_match.room_key,
                      known ? mode : GameMode::Battle};
}

std::array<PlayerSlotPlan, NetConfig::SlotCapacity>
NetLaunch::build_players(std::uint8_t local_hunter, int local_recolor,
                         int team_id, int requested_local_slot) {
    const LaunchState& current = state();
    // Once joined, the session's resolved value is authoritative.  This
    // protects callers that still carry the menu's Random sentinel into the
    // scene setup, and prevents the local scene from disagreeing with the
    // hunter already announced to the server.
    const std::uint8_t effective_local_hunter = current.client != nullptr
        ? current.local_hunter : local_hunter;
    const int resolved_local = requested_local_slot >= 0
        ? requested_local_slot : local_slot();
    std::array<PlayerSlotPlan, NetConfig::SlotCapacity> result{};
    std::array<bool, NetConfig::SlotCapacity> occupied{};
    if (current.roster_valid) {
        const std::size_t count = std::min<std::size_t>(
            current.roster.count, NetConfig::SlotCapacity);
        for (std::size_t index = 0; index < count; ++index) {
            const std::size_t slot = current.roster.slots[index];
            if (slot < NetConfig::SlotCapacity) {
                occupied[slot] = true;
            }
        }
    } else if (resolved_local >= 0
               && resolved_local < static_cast<int>(NetConfig::SlotCapacity)) {
        occupied[static_cast<std::size_t>(resolved_local)] = true;
    }

    for (std::size_t slot = 0; slot < result.size(); ++slot) {
        auto& plan = result[slot];
        plan.slot = static_cast<std::uint8_t>(slot);
        plan.hunter = 0;
        if (current.roster_valid) {
            const std::size_t count = std::min<std::size_t>(
                current.roster.count, NetConfig::SlotCapacity);
            for (std::size_t index = 0; index < count; ++index) {
                if (current.roster.slots[index] == slot) {
                    plan.hunter = current.roster.hunters[index];
                    break;
                }
            }
        }
        if (static_cast<int>(slot) == resolved_local) {
            plan.hunter = effective_local_hunter;
            plan.recolor = local_recolor;
            plan.local = true;
        }
        plan.team = team_id;
        plan.active = occupied[slot];
        plan.is_bot = false;
    }
    return result;
}

std::vector<std::string>
NetLaunch::disable_cheats_for_match(features::CheatSettings& cheats) {
    struct CheatEntry {
        const char* name;
        bool* value;
    };
    const std::array entries{
        CheatEntry{"FreeWeaponSelect", &cheats.free_weapon_select},
        CheatEntry{"UnlimitedJumps", &cheats.unlimited_jumps},
        CheatEntry{"NoRandomEncounters", &cheats.no_random_encounters},
        CheatEntry{"UnlockAllDoors", &cheats.unlock_all_doors},
        CheatEntry{"ContinueFromCurrentRoom", &cheats.continue_from_current_room},
        CheatEntry{"SkipPlanetIntros", &cheats.skip_planet_intros},
        CheatEntry{"StartWithAllUpgrades", &cheats.start_with_all_upgrades},
        CheatEntry{"StartWithAllOctoliths", &cheats.start_with_all_octoliths},
        CheatEntry{"WalkThroughWalls", &cheats.walk_through_walls},
        CheatEntry{"AlwaysFightGorea2", &cheats.always_fight_gorea2},
        CheatEntry{"QuadrupleDamage", &cheats.quadruple_damage}
    };
    std::vector<std::string> disabled;
    for (const CheatEntry& entry : entries) {
        if (*entry.value) {
            disabled.emplace_back(entry.name);
            *entry.value = false;
        }
    }
    return disabled;
}

} // namespace fruityprime::net
