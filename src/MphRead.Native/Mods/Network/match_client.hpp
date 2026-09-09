#pragma once

#include "Mods/Network/net_client.hpp"

#include <array>
#include <chrono>
#include <cstdint>
#include <string>

namespace fruityprime::net {

// Per-client clock schedule used by the headless network check.  Spectating
// is a wire state, not a disconnect: the client keeps sending intents so the
// relay does not time it out, but the gameplay buttons are suppressed while
// SpectatingState is asserted.  Keeping this separate from NetClient lets the
// recorded and non-recorded checks use exactly the same transition points.
class SpectateSchedule {
public:
    SpectateSchedule(double spectate_at_seconds = -1.0,
                     double rejoin_at_seconds = -1.0) noexcept;

    void update(std::uint32_t frame) noexcept;

    [[nodiscard]] bool spectating() const noexcept { return spectating_; }
    [[nodiscard]] int started_frame() const noexcept {
        return started_frame_;
    }
    [[nodiscard]] int rejoined_frame() const noexcept {
        return rejoined_frame_;
    }

    [[nodiscard]] IntentButtons buttons(IntentButtons base) const noexcept;

private:
    [[nodiscard]] static bool reached(double seconds,
                                      std::uint32_t frame) noexcept;

    double spectate_at_seconds_;
    double rejoin_at_seconds_;
    bool spectating_ = false;
    int started_frame_ = -1;
    int rejoined_frame_ = -1;
};

struct MatchClientOptions {
    std::string name = "FruityPrime";
    std::uint8_t hunter = 0;
    IntentButtons buttons = IntentButtons::None;
    Vec3 aim{0, 0, 1};
    Vec3 position;
    std::uint16_t ammo_ua = 0;
    std::uint16_t ammo_missiles = 0;
    NetworkConditions network;
    double spectate_at_seconds = -1.0;
    double rejoin_at_seconds = -1.0;
};

struct MatchClientStats {
    int local_slot = -1;
    std::size_t intents_sent = 0;
    std::size_t snapshots_received = 0;
    std::size_t rosters_received = 0;
    std::size_t match_states_received = 0;
    std::size_t chats_received = 0;
    std::uint32_t last_snapshot_frame = 0;
    std::uint8_t last_roster_count = 0;
    std::size_t spectating_frames = 0;
    int spectate_started_frame = -1;
    int rejoined_frame = -1;
    std::array<std::size_t, NetConfig::SlotCapacity> remote_spectating_frames{};
    MatchStatePacket last_match_state;
};

[[nodiscard]] MatchClientStats run_match_client(
    Endpoint server, const MatchClientOptions& options,
    std::chrono::milliseconds duration,
    std::chrono::milliseconds tick = std::chrono::milliseconds(16));

} // namespace fruityprime::net
