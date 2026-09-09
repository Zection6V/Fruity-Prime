#include "Mods/Network/net_match_end.hpp"

#include <algorithm>

namespace fruityprime::net {

void MatchEnd::reset() noexcept {
    last_report_frame_ = 0;
    stranded_since_ = 0;
    reported_ = false;
    acknowledged_ = false;
}

bool MatchEnd::in_intermission(bool network_active,
                               const game::State& local,
                               const MatchStatePacket* server) noexcept {
    return network_active
        && (local.match_state != game::MatchState::InProgress
            || (server != nullptr && server->ending()));
}

void MatchEnd::recover_if_stranded(
    std::uint32_t frame, game::State& local,
    const MatchStatePacket* server, bool server_ending,
    MatchEndResult& result) noexcept {
    const bool server_running = server != nullptr && !server_ending
        && (server->flags & MatchStatePacket::FlagInProgress) != 0;
    if (!server_running || local.match_state == game::MatchState::InProgress) {
        stranded_since_ = 0;
        return;
    }
    if (stranded_since_ == 0) {
        stranded_since_ = std::max(frame, 1U);
        return;
    }
    if (frame - stranded_since_ < StrandedFrames) {
        return;
    }

    stranded_since_ = 0;
    local.reset_match_progress();
    local.match_time = server->time_remaining;
    result.recovered = true;
}

MatchEndResult MatchEnd::sync(
    std::uint32_t frame, bool network_active, bool may_end,
    game::State& local, const MatchStatePacket* server) noexcept {
    MatchEndResult result;
    if (!network_active) {
        return result;
    }

    const bool server_ending = server != nullptr && server->ending();
    if (server_ending && local.match_state == game::MatchState::InProgress) {
        // GameState uses a zero timer to enter the ordinary results sequence.
        local.match_time = 0.0F;
    }
    recover_if_stranded(frame, local, server, server_ending, result);

    if (!may_end) {
        return result;
    }
    if (local.match_state == game::MatchState::InProgress) {
        reported_ = false;
        acknowledged_ = false;
        return result;
    }
    if (server_ending) {
        reported_ = true;
        acknowledged_ = true;
        return result;
    }
    if (acknowledged_) {
        return result;
    }
    if (reported_ && frame - last_report_frame_ < ReportIntervalFrames) {
        return result;
    }
    reported_ = true;
    last_report_frame_ = frame;
    result.should_report = true;
    return result;
}

} // namespace fruityprime::net
