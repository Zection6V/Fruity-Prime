#include "Mods/Network/player_entity_net_hud.hpp"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>

namespace fruityprime::net::hud {
namespace {

[[nodiscard]] bool uses_time_one(game::Mode mode) noexcept {
    return mode == game::Mode::Survival
        || mode == game::Mode::SurvivalTeams
        || mode == game::Mode::Defender
        || mode == game::Mode::DefenderTeams
        || mode == game::Mode::PrimeHunter;
}

[[nodiscard]] bool uses_deaths_two(game::Mode mode) noexcept {
    return mode == game::Mode::Survival
        || mode == game::Mode::SurvivalTeams
        || mode == game::Mode::Battle
        || mode == game::Mode::BattleTeams;
}

[[nodiscard]] std::string format_time(float seconds) {
    if (!std::isfinite(seconds)) {
        return "0:00";
    }
    const int total = std::max(0, static_cast<int>(std::floor(seconds)));
    const int minutes = total / 60;
    const int remainder = total % 60;
    std::ostringstream text;
    text << minutes << ':' << std::setfill('0') << std::setw(2) << remainder;
    return text.str();
}

} // namespace

ScoreColumns score_columns(bool networked) noexcept {
    if (networked) {
        return {145.0F, 193.0F, 236.0F};
    }
    return {160.0F, 215.0F, 236.0F};
}

PingLabel ping_label(int ping) {
    if (ping <= 0) {
        return {"--", PingTone::Unknown};
    }
    return {std::to_string(std::min(ping, 999)),
            ping < 80 ? PingTone::Good
                      : ping < 160 ? PingTone::Warning : PingTone::Bad};
}

std::array<float, 3> ping_rgb(PingTone tone) noexcept {
    switch (tone) {
    case PingTone::Good:
        return {110.0F / 255.0F, 231.0F / 255.0F, 135.0F / 255.0F};
    case PingTone::Warning:
        return {1.0F, 200.0F / 255.0F, 80.0F / 255.0F};
    case PingTone::Bad:
        return {1.0F, 110.0F / 255.0F, 110.0F / 255.0F};
    case PingTone::Unknown:
    default:
        return {120.0F / 255.0F, 120.0F / 255.0F, 140.0F / 255.0F};
    }
}

std::string score_header_one(game::Mode mode) {
    if (mode == game::Mode::Battle || mode == game::Mode::BattleTeams
        || mode == game::Mode::Nodes || mode == game::Mode::NodesTeams) {
        return "points";
    }
    if (mode == game::Mode::Capture || mode == game::Mode::Bounty
        || mode == game::Mode::BountyTeams) {
        return "octoliths";
    }
    return "time";
}

std::string score_header_two(game::Mode mode) {
    if (mode == game::Mode::Battle || mode == game::Mode::BattleTeams
        || mode == game::Mode::Survival
        || mode == game::Mode::SurvivalTeams) {
        return "deaths";
    }
    return "kills";
}

std::string score_value_one(const game::State& state, game::Mode mode,
                           std::size_t slot) {
    if (slot >= game::SlotCapacity) {
        return {};
    }
    if (uses_time_one(mode)) {
        return state.player_time[slot] < 0.0F
            ? "MAX" : format_time(state.player_time[slot]);
    }
    return std::to_string(state.points[slot]);
}

std::string score_value_two(const game::State& state, game::Mode mode,
                           std::size_t slot) {
    if (slot >= game::SlotCapacity) {
        return {};
    }
    return std::to_string(uses_deaths_two(mode)
                              ? state.deaths[slot] : state.kills[slot]);
}

} // namespace fruityprime::net::hud
