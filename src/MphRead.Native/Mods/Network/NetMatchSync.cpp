#include "Mods/Network/net_match_sync.hpp"

#include <cmath>

namespace fruityprime::net {

void MatchSync::reset() noexcept {
    last_room_.clear();
    last_drift_seconds_ = 0.0F;
    ever_synced_ = false;
}

MatchSyncResult MatchSync::apply(game::State& local,
                                 const MatchStatePacket& server,
                                 bool in_intermission) noexcept {
    MatchSyncResult result;
    if (server.room_key.empty()) {
        return result;
    }

    result.accepted = true;
    if (server.point_goal > 0) {
        local.point_goal = server.point_goal;
    }
    local.friendly_fire = server.friendly_fire();

    // Results are driven by the same local counter as the managed sequence.
    // Replacing it with the server's remaining time here would rewind the
    // winner camera and keep the intermission from completing.
    if (in_intermission || server.ending()) {
        last_room_ = server.room_key;
        return result;
    }

    // A zero/zero state is the server's "no clock" representation.  Do not
    // turn an unlimited match into a frozen local timer.
    if (server.time_remaining <= 0.0F && server.time_elapsed <= 0.0F) {
        return result;
    }

    result.new_match = server.room_key != last_room_;
    last_room_ = server.room_key;
    last_drift_seconds_ = local.match_time - server.time_remaining;
    result.drift_seconds = last_drift_seconds_;

    // Small differences are packet age.  Snapping only on a new match, the
    // first authoritative value, or a clear desynchronisation keeps the HUD
    // timer from visibly stuttering.
    if (result.new_match || !ever_synced_
        || std::abs(last_drift_seconds_) > 1.5F) {
        local.match_time = server.time_remaining;
        result.clock_snapped = true;
        ever_synced_ = true;
    }
    return result;
}

} // namespace fruityprime::net
