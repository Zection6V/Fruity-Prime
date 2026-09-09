#include "Entities/match_flow.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <utility>

namespace fruityprime::match {
namespace {

[[nodiscard]] bool active(const net::PlayerState& player) noexcept {
    return (player.flags & net::PlayerState::FlagActive) != 0;
}

[[nodiscard]] bool ending(const net::MatchStatePacket& state) noexcept {
    return (state.flags & net::MatchStatePacket::FlagEnding) != 0;
}

[[nodiscard]] bool survival_mode(std::uint8_t mode) noexcept {
    return mode == 5 || mode == 6;
}

[[nodiscard]] bool defender_mode(std::uint8_t mode) noexcept {
    return mode == 12 || mode == 13;
}

[[nodiscard]] bool prime_hunter_mode(std::uint8_t mode) noexcept {
    return mode == 14;
}

[[nodiscard]] bool survival_over(
    std::span<const net::PlayerState> players, bool team_mode,
    std::uint16_t spare_lives) noexcept {
    std::size_t active_players = 0;
    std::size_t eligible_players = 0;
    if (!team_mode) {
        for (const auto& player : players) {
            if (!active(player)) {
                continue;
            }
            ++active_players;
            if (player.health > 0 || player.deaths <= spare_lives) {
                ++eligible_players;
            }
        }
        return active_players < 2 || eligible_players < 2;
    }

    std::array<std::uint32_t, 2> team_deaths{};
    for (const auto& player : players) {
        if (active(player)) {
            ++active_players;
            team_deaths[player.team & 1u] += player.deaths;
        }
    }
    std::array<bool, 2> teams_alive{};
    for (const auto& player : players) {
        if (!active(player)) {
            continue;
        }
        const std::size_t team = player.team & 1u;
        if (player.health > 0 || team_deaths[team] <= spare_lives) {
            ++eligible_players;
            teams_alive[team] = true;
        }
    }
    return active_players < 2 || eligible_players < 2
        || !teams_alive[0] || !teams_alive[1];
}

} // namespace

bool is_team_mode(std::uint8_t mode) noexcept {
    // GameMode values copied from the managed GameState.IsTeamMode table.
    return mode == 4 || mode == 6 || mode == 7 || mode == 9
        || mode == 11 || mode == 13;
}

ModeDefaults defaults_for_mode(std::uint8_t mode) noexcept {
    switch (mode) {
    case 2: // GameMode.SinglePlayer
        return {0.0F, 0, 0.0F};
    case 3: // GameMode.Battle
    case 4: // GameMode.BattleTeams
        return {7.0F * 60.0F, 7, 0.0F};
    case 5: // GameMode.Survival
    case 6: // GameMode.SurvivalTeams
        return {15.0F * 60.0F, 2, 0.0F};
    case 7: // GameMode.Capture
        return {15.0F * 60.0F, 5, 0.0F};
    case 8: // GameMode.Bounty
    case 9: // GameMode.BountyTeams
        return {15.0F * 60.0F, 3, 0.0F};
    case 10: // GameMode.Nodes
    case 11: // GameMode.NodesTeams
        return {15.0F * 60.0F, 70, 0.0F};
    case 12: // GameMode.Defender
    case 13: // GameMode.DefenderTeams
    case 14: // GameMode.PrimeHunter
        return {15.0F * 60.0F, 0, 1.5F * 60.0F};
    default:
        return {};
    }
}

bool uses_point_goal(std::uint8_t mode) noexcept {
    return mode == 3 || mode == 4 || mode == 7 || mode == 8
        || mode == 9 || mode == 10 || mode == 11;
}

namespace {

[[nodiscard]] bool point_goal_reached(
    std::span<const net::PlayerState> players, bool team_mode,
    std::uint16_t point_goal) noexcept {
    if (!team_mode) {
        return std::find_if(players.begin(), players.end(),
            [point_goal](const net::PlayerState& player) {
                return active(player) && player.points >= point_goal;
            }) != players.end();
    }

    std::array<int, 2> team_points{};
    for (const auto& player : players) {
        if (active(player)) {
            team_points[player.team & 1u] += player.points;
        }
    }
    return std::any_of(team_points.begin(), team_points.end(),
        [point_goal](int points) { return points >= point_goal; });
}

void set_ending(net::MatchStatePacket& state) noexcept {
    state.flags = static_cast<std::uint8_t>(
        state.flags & ~net::MatchStatePacket::FlagInProgress);
    state.flags |= net::MatchStatePacket::FlagEnding;
    state.time_remaining = 0.0F;
}

} // namespace

Flow::Flow(Rules rules) {
    reset(std::move(rules));
}

void Flow::reset(Rules rules) {
    rules_ = std::move(rules);
    if (rules_.time_goal_seconds < 0.0F) {
        rules_.time_goal_seconds = defaults_for_mode(rules_.mode)
            .time_goal_seconds;
    }
    phase_ = Phase::InProgress;
    end_report_pending_ = false;
    team_objective_time_.fill(0.0F);
    player_objective_time_.fill(0.0F);
    prime_hunter_ = -1;
    state_ = {};
    state_.mode = rules_.mode;
    state_.time_remaining = std::max(0.0F, rules_.time_limit_seconds);
    state_.time_elapsed = 0.0F;
    state_.flags = net::MatchStatePacket::FlagInProgress;
    state_.point_goal = rules_.point_goal;
    state_.room_key = rules_.room_key;
    state_.next_room_key = rules_.next_room_key;
}

void Flow::tick(float seconds,
                std::span<const net::PlayerState> players,
                bool score_authority,
                bool advance_local_clock) {
    if (phase_ != Phase::InProgress) {
        return;
    }
    state_.player_count = static_cast<std::uint8_t>(std::min<std::size_t>(
        players.size(), net::NetConfig::SlotCapacity));

    if (advance_local_clock && seconds > 0.0F) {
        state_.time_elapsed += seconds;
        if (rules_.time_limit_seconds > 0.0F) {
            state_.time_remaining = std::max(
                0.0F, state_.time_remaining - seconds);
            if (state_.time_remaining <= 0.0F) {
                set_ending(state_);
                phase_ = Phase::GameOver;
                return;
            }
        }
    }

    const bool effective_team_mode = rules_.team_mode
        || is_team_mode(rules_.mode);
    if (score_authority && survival_mode(rules_.mode)
        && survival_over(players, effective_team_mode, rules_.point_goal)) {
        set_ending(state_);
        phase_ = Phase::GameOver;
        end_report_pending_ = true;
        return;
    }

    if (score_authority && rules_.time_goal_seconds > 0.0F) {
        bool objective_reached = false;
        if (defender_mode(rules_.mode)) {
            objective_reached = std::any_of(
                team_objective_time_.begin(), team_objective_time_.end(),
                [goal = rules_.time_goal_seconds](float value) {
                    return value >= goal;
                });
        } else if (prime_hunter_mode(rules_.mode)
                   && prime_hunter_ >= 0
                   && static_cast<std::size_t>(prime_hunter_)
                       < player_objective_time_.size()) {
            const auto found = std::find_if(
                players.begin(), players.end(),
                [slot = static_cast<std::uint8_t>(prime_hunter_)](
                    const net::PlayerState& player) {
                    return active(player) && player.slot_index == slot;
                });
            objective_reached = found != players.end()
                && player_objective_time_[static_cast<std::size_t>(
                    prime_hunter_)] >= rules_.time_goal_seconds;
        }
        if (objective_reached) {
            set_ending(state_);
            phase_ = Phase::GameOver;
            end_report_pending_ = true;
            return;
        }
    }

    if (!score_authority || rules_.point_goal == 0
        || !uses_point_goal(rules_.mode)) {
        return;
    }
    if (!point_goal_reached(players,
                            effective_team_mode,
                            rules_.point_goal)) {
        return;
    }
    set_ending(state_);
    phase_ = Phase::GameOver;
    end_report_pending_ = true;
}

void Flow::set_team_objective_time(std::uint8_t team,
                                   float seconds) noexcept {
    if (team >= team_objective_time_.size() || !std::isfinite(seconds)) {
        return;
    }
    team_objective_time_[team] = std::max(0.0F, seconds);
}

void Flow::set_player_objective_time(std::uint8_t slot,
                                     float seconds) noexcept {
    if (slot >= player_objective_time_.size() || !std::isfinite(seconds)) {
        return;
    }
    player_objective_time_[slot] = std::max(0.0F, seconds);
}

void Flow::set_prime_hunter(std::int8_t slot) noexcept {
    prime_hunter_ = slot >= 0
        && static_cast<std::size_t>(slot) < player_objective_time_.size()
        ? slot : static_cast<std::int8_t>(-1);
}

void Flow::apply_server_state(const net::MatchStatePacket& state) {
    state_ = state;
    rules_.mode = state.mode;
    rules_.point_goal = state.point_goal;
    rules_.team_mode = is_team_mode(state.mode);
    end_report_pending_ = false;
    if (ending(state)) {
        phase_ = Phase::GameOver;
    } else if ((state.flags & net::MatchStatePacket::FlagInProgress) != 0) {
        phase_ = Phase::InProgress;
    } else {
        phase_ = Phase::Disconnected;
    }
}

bool Flow::consume_end_report() noexcept {
    if (!end_report_pending_) {
        return false;
    }
    end_report_pending_ = false;
    return true;
}

} // namespace fruityprime::match
