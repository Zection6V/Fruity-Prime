#pragma once

#include "Mods/Network/net_protocol.hpp"

#include <cstdint>
#include <string>

namespace fruityprime::net {

enum class RoomChangeDecision : std::uint8_t {
    None,
    AdoptCurrent,
    WaitForPreviousRequest,
    Load,
};

struct RoomChangeResult {
    RoomChangeDecision decision = RoomChangeDecision::None;
    std::string room_key;
    std::uint16_t match_id = 0;
};

// Native counterpart of MphRead/Mods/Network/NetRoomChange.cs.  The actual
// asset/scene load stays in the frontend, but the packet-loss-safe request
// suppression and post-load settling window live here instead of in the game
// loop.
class RoomChange final {
public:
    static constexpr std::uint32_t SettleFrames = 60;
    static constexpr std::uint32_t RequestRetryFrames = 300;

    void reset() noexcept;

    [[nodiscard]] RoomChangeResult observe(
        std::string_view current_room, const MatchStatePacket& server,
        std::uint32_t frame) noexcept;

    void mark_loaded(std::uint16_t match_id, std::uint32_t frame) noexcept;

    [[nodiscard]] bool settling(std::uint32_t frame) const noexcept;
    [[nodiscard]] std::uint16_t loaded_match() const noexcept {
        return loaded_match_;
    }
    [[nodiscard]] std::uint32_t loaded_frame() const noexcept {
        return loaded_frame_;
    }
    [[nodiscard]] const std::string& requested_room() const noexcept {
        return requested_room_;
    }

private:
    std::string requested_room_;
    std::uint32_t requested_frame_ = 0;
    std::uint16_t loaded_match_ = 0;
    std::uint32_t loaded_frame_ = 0;
};

} // namespace fruityprime::net
