#pragma once

#include "Entities/gameplay.hpp"
#include "Mods/Launcher/Portable/launch_plan.hpp"
#include "Entities/match_flow.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace fruityprime::launcher {

// Settings owned by the front end but consumed by the common match-start
// boundary.  Keeping these out of Win32/Avalonia controls makes the same
// preparation usable by the native dialog, the text launcher, and Android.
struct MatchStartOptions {
    // An absent value selects the mode table, so Survival/Capture/Nodes do
    // not accidentally inherit Battle's 420-second/7-point defaults.
    std::optional<float> time_limit_seconds;
    std::optional<std::uint16_t> point_goal;
    bool team_play = false;
    bool friendly_fire = false;
    bool objective_authority = true;
    std::uint8_t local_slot = 0;
};

struct PlayerSlot {
    std::uint8_t slot = 0;
    metadata::Hunter hunter = metadata::Hunter::Samus;
    std::uint8_t team = 0;
    int bot_level = 0;
    bool bot = false;
};

// The result of turning a LaunchPlan into engine state.  Loading the room and
// creating the render window remain platform concerns; everything that must
// agree between launchers is decided here once.
struct PreparedMatch {
    bool ok = false;
    std::string error;
    LaunchKind kind = LaunchKind::None;
    std::string room_key;
    std::uint8_t mode = 3;
    match::Rules rules;
    gameplay::Config gameplay;
    std::vector<PlayerSlot> players;
};

class MatchStart final {
public:
    // For online/host plans, an authoritative server room/mode may be passed
    // after the handshake. Empty/absent values keep the launcher's choice.
    [[nodiscard]] static PreparedMatch prepare(
        const LaunchPlan& plan,
        MatchStartOptions options = {},
        std::string_view authoritative_room = {},
        std::optional<std::uint8_t> authoritative_mode = std::nullopt);

    // Populate a newly constructed gameplay session from the prepared roster.
    // Bot slots are returned separately because the renderer drives those with
    // its local AI while network clients receive only the local player here.
    [[nodiscard]] static bool populate(
        gameplay::Session& session,
        const PreparedMatch& prepared,
        std::vector<std::uint8_t>* bot_slots = nullptr,
        std::string* error = nullptr);
};

} // namespace fruityprime::launcher
