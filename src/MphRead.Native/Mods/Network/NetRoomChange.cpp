#include "Mods/Network/net_room_change.hpp"

#include <algorithm>

namespace fruityprime::net {

void RoomChange::reset() noexcept {
    requested_room_.clear();
    requested_frame_ = 0;
    loaded_match_ = 0;
    loaded_frame_ = 0;
}

RoomChangeResult RoomChange::observe(
    std::string_view current_room, const MatchStatePacket& server,
    std::uint32_t frame) noexcept {
    RoomChangeResult result;
    result.room_key = server.room_key;
    result.match_id = server.match_id;
    if (server.room_key.empty()) {
        return result;
    }

    // A joiner that is already on the server's room adopts the first match
    // number without reloading.  A repeated room name with another match id
    // is still a new round and must be loaded again.
    if (current_room == server.room_key
        && (loaded_match_ == 0 || loaded_match_ == server.match_id)) {
        loaded_match_ = server.match_id;
        requested_room_.clear();
        result.decision = RoomChangeDecision::AdoptCurrent;
        return result;
    }

    if (requested_room_ == server.room_key
        && loaded_match_ == server.match_id
        && frame - requested_frame_ < RequestRetryFrames) {
        result.decision = RoomChangeDecision::WaitForPreviousRequest;
        return result;
    }

    requested_room_ = server.room_key;
    requested_frame_ = frame;
    loaded_match_ = server.match_id;
    result.decision = RoomChangeDecision::Load;
    return result;
}

void RoomChange::mark_loaded(std::uint16_t match_id,
                             std::uint32_t frame) noexcept {
    loaded_match_ = match_id;
    loaded_frame_ = std::max(frame, 1U);
    requested_room_.clear();
    requested_frame_ = 0;
}

bool RoomChange::settling(std::uint32_t frame) const noexcept {
    return loaded_frame_ != 0 && frame - loaded_frame_ < SettleFrames;
}

} // namespace fruityprime::net
