#pragma once

#include "Mods/Network/demo.hpp"
#include "Mods/Network/net_protocol.hpp"

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace fruityprime::demo {

// File/timeline ownership for Mods/Network/DemoPlayback.cs.  Gameplay still
// applies each returned packet because that is deliberately the same handler
// used by the live network path; this class only opens, validates, and
// releases records on their recorded simulation frame.
class Playback final {
public:
    Playback() = default;
    Playback(const Playback&) = delete;
    Playback& operator=(const Playback&) = delete;

    [[nodiscard]] bool open(const std::filesystem::path& path,
                            net::MatchStatePacket& first_match,
                            std::string& error);
    void stop() noexcept;

    [[nodiscard]] bool active() const noexcept {
        return active_;
    }
    [[nodiscard]] bool at_end() const noexcept {
        return active_ && cursor_ >= records_.size();
    }
    [[nodiscard]] bool protocol_mismatch() const noexcept {
        return protocol_mismatch_;
    }

    // Move the records scheduled for this frame out of the timeline.  The
    // caller owns applying them and may decode them in any order-preserving
    // way required by the gameplay layer.
    [[nodiscard]] std::vector<Record> take_until(std::uint32_t frame);

private:
    std::vector<Record> records_;
    std::size_t cursor_ = 0;
    bool active_ = false;
    bool protocol_mismatch_ = false;
};

} // namespace fruityprime::demo
