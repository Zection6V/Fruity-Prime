#pragma once

#include "GameState.hpp"
#include "Mods/Network/net_protocol.hpp"

#include <cstdint>
#include <string>

namespace fruityprime::net {

struct MatchSyncResult {
    bool accepted = false;
    bool new_match = false;
    bool clock_snapped = false;
    float drift_seconds = 0.0F;
};

// Native counterpart of MphRead/Mods/Network/NetMatchSync.cs.  The match
// flow owns the simulation phase; this class owns the client-side policy for
// adopting server rules and the server clock.
class MatchSync final {
public:
    void reset() noexcept;

    [[nodiscard]] MatchSyncResult apply(
        game::State& local, const MatchStatePacket& server,
        bool in_intermission) noexcept;

    [[nodiscard]] bool synced() const noexcept { return ever_synced_; }
    [[nodiscard]] float last_drift_seconds() const noexcept {
        return last_drift_seconds_;
    }
    [[nodiscard]] const std::string& last_room() const noexcept {
        return last_room_;
    }

private:
    std::string last_room_;
    float last_drift_seconds_ = 0.0F;
    bool ever_synced_ = false;
};

} // namespace fruityprime::net
