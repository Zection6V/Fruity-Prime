#pragma once

#include "Features.hpp"
#include "Mods/Network/map_rotation.hpp"
#include "Mods/Network/net_client.hpp"

#include <array>
#include <chrono>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace fruityprime::net {

// The portable part of the managed NetLaunch input.  Keeping this as a value
// object makes the command-line and launcher paths use the same join rules.
struct JoinOptions {
    std::string address;
    std::uint16_t port = NetConfig::DefaultPort;
    std::string player_name = "FruityPrime";
    std::uint8_t hunter = 0;
    int timeout_ms = 8000;
    NetworkConditions network;
    features::CheatSettings* cheats = nullptr;
};

struct LaunchRoom {
    std::string room_key;
    GameMode mode = GameMode::Battle;
};

// Scene.AddPlayer's native equivalent.  The renderer can turn these plans
// into actual entities without having to know how the roster packet is laid
// out.  A network match always has the full slot capacity available, while
// only occupied slots are active at load time.
struct PlayerSlotPlan {
    std::uint8_t slot = 0;
    std::uint8_t hunter = 0;
    int recolor = 0;
    int team = -1;
    bool active = false;
    bool local = false;
    bool is_bot = false;
};

class NetLaunch final {
public:
    NetLaunch() = delete;

    [[nodiscard]] static bool join(const JoinOptions& options);
    static void disconnect() noexcept;

    // Pump packets received after Join.  MatchState and Roster are retained
    // for ServerRoom/BuildPlayers; all packets are returned to the caller so
    // gameplay and recording can consume the same stream.
    [[nodiscard]] static std::vector<ReceivedPacket> update();

    [[nodiscard]] static bool active() noexcept;
    [[nodiscard]] static int local_slot() noexcept;
    // The playable hunter actually sent in Identify.  A launcher may pass
    // Random as the menu choice; after Join this returns the one resolved
    // value that both the roster announcement and local player use.
    [[nodiscard]] static std::uint8_t local_hunter() noexcept;
    [[nodiscard]] static std::string_view last_join_error() noexcept;
    [[nodiscard]] static std::optional<LaunchRoom> server_room();

    // The room loader uses a stable two-player layout for every networked
    // room, independent of how many slots the server has admitted.
    static constexpr int RoomPlayerCount = 2;

    [[nodiscard]] static std::array<PlayerSlotPlan, NetConfig::SlotCapacity>
    build_players(std::uint8_t local_hunter, int local_recolor,
                  int team_id = -1, int local_slot = -1);

    // C# discovers all public boolean cheat properties by reflection.  The
    // native settings type is explicit, so the equivalent list is kept in
    // one implementation table and every field is cleared before a match.
    [[nodiscard]] static std::vector<std::string>
    disable_cheats_for_match(features::CheatSettings& cheats);
};

} // namespace fruityprime::net
