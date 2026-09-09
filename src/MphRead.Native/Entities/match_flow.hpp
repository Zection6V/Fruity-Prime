#pragma once

#include "Mods/Network/net_protocol.hpp"

#include <cstdint>
#include <array>
#include <span>
#include <string>

namespace fruityprime::match {

// Keep the mode table in one place for both match scoring and gameplay rules.
[[nodiscard]] bool is_team_mode(std::uint8_t mode) noexcept;

struct ModeDefaults {
    float time_limit_seconds = 7.0F * 60.0F;
    std::uint16_t point_goal = 7;
    float time_goal_seconds = 0.0F;
};

[[nodiscard]] ModeDefaults defaults_for_mode(std::uint8_t mode) noexcept;
[[nodiscard]] bool uses_point_goal(std::uint8_t mode) noexcept;

enum class Phase : std::uint8_t {
    InProgress,
    GameOver,
    Ending,
    Disconnected
};

struct Rules {
    std::uint8_t mode = 3; // GameMode.Battle
    float time_limit_seconds = 7.0F * 60.0F;
    std::uint16_t point_goal = 7;
    // -1 selects the managed default for the mode.  Zero explicitly disables
    // an objective-time goal for a caller constructing custom rules.
    float time_goal_seconds = -1.0F;
    bool team_mode = false;
    std::string room_key;
    std::string next_room_key;
};

// The native equivalent of the managed GameState match-progress boundary.
// Simulation remains in gameplay::Session; this class owns only the clock,
// score-goal transition, and the server's MatchState/MatchEnd handshake.
class Flow {
public:
    explicit Flow(Rules rules = {});

    void reset(Rules rules);
    void tick(float seconds,
              std::span<const net::PlayerState> players,
              bool score_authority,
              bool advance_local_clock);
    void apply_server_state(const net::MatchStatePacket& state);

    // Objective entities feed their authoritative accumulated time through
    // these hooks.  Keeping this state outside PlayerState matches the wire
    // protocol: Defender scores team-held time, while Prime Hunter scores the
    // selected hunter's time.
    void set_team_objective_time(std::uint8_t team,
                                 float seconds) noexcept;
    void set_player_objective_time(std::uint8_t slot,
                                   float seconds) noexcept;
    void set_prime_hunter(std::int8_t slot) noexcept;

    [[nodiscard]] Phase phase() const noexcept { return phase_; }
    [[nodiscard]] bool in_progress() const noexcept {
        return phase_ == Phase::InProgress;
    }
    [[nodiscard]] const net::MatchStatePacket& state() const noexcept {
        return state_;
    }
    [[nodiscard]] bool consume_end_report() noexcept;

private:
    Rules rules_;
    Phase phase_ = Phase::InProgress;
    net::MatchStatePacket state_;
    bool end_report_pending_ = false;
    std::array<float, net::NetConfig::SlotCapacity> team_objective_time_{};
    std::array<float, net::NetConfig::SlotCapacity> player_objective_time_{};
    std::int8_t prime_hunter_ = -1;
};

} // namespace fruityprime::match
