#pragma once

#include "Mods/Network/net_protocol.hpp"
#include "Mods/Network/player_entity_net_aim.hpp"

#include <array>
#include <cstddef>
#include <cstdint>

namespace fruityprime::gameplay {
class Session;
}

namespace fruityprime::net {

class DamageBridge;

// Native counterpart of MphRead/Mods/Network/NetPlayerBridge.cs.  It keeps
// network-only history out of gameplay::Session while exposing the same
// explicit boundaries: local intent capture, remote intent application,
// reported-position correction, and authority snapshot import.
class NetPlayerBridge final {
public:
    struct Metrics {
        std::uint64_t rejected_updates = 0;
        std::uint64_t snaps = 0;
        float worst_snap = 0.0F;
    };

    void reset() noexcept;
    void note_room_changed() noexcept;
    void forget_slot(std::uint8_t slot) noexcept;

    [[nodiscard]] IntentState capture_intent(
        const gameplay::Session& session, std::uint8_t slot,
        std::uint32_t frame, IntentButtons buttons, Vec3 aim,
        std::uint8_t weapon_select) noexcept;

    [[nodiscard]] bool apply_intent(gameplay::Session& session,
                                    std::uint8_t slot,
                                    const IntentState& intent,
                                    bool sync_reported_position = true) noexcept;

    [[nodiscard]] bool apply_snapshot(gameplay::Session& session,
                                      const SnapshotPacket& snapshot,
                                      std::uint8_t local_slot,
                                      bool settling,
                                      DamageBridge& damage) noexcept;

    // The native fixed-step session still evaluates the same input actions
    // for a puppet.  Reapply the owner's reported position after that step so
    // a remote player is not advanced twice by the authority.
    void restore_reported_positions(gameplay::Session& session,
                                    std::uint8_t local_slot,
                                    bool settling = false) noexcept;

    [[nodiscard]] PlayerEntityNetAim& aim() noexcept { return aim_; }
    [[nodiscard]] const PlayerEntityNetAim& aim() const noexcept {
        return aim_;
    }

    [[nodiscard]] const Metrics& metrics() const noexcept { return metrics_; }
    [[nodiscard]] bool state_valid(std::uint8_t slot) const noexcept {
        return slot < Slots && state_valid_[slot];
    }
    [[nodiscard]] bool intent_valid(std::uint8_t slot) const noexcept {
        return slot < Slots && intent_valid_[slot];
    }

private:
    static constexpr std::size_t Slots = NetConfig::SlotCapacity;
    static constexpr std::uint32_t StaleAfterSpawnFrames = 120;
    static constexpr float SnapDistance = 15.0F;
    static constexpr float MaxReportedSpeed = 5.0F;
    static constexpr float PositionLimit = 100000.0F;

    [[nodiscard]] static bool sane(Vec3 value) noexcept;
    [[nodiscard]] static bool has_button(IntentButtons buttons,
                                          IntentButtons button) noexcept;
    [[nodiscard]] IntentButtons missed_presses(
        std::uint8_t slot, const IntentState& intent) noexcept;
    [[nodiscard]] bool apply_reported_position(
        gameplay::Session& session, std::uint8_t slot,
        const IntentState& intent) noexcept;
    [[nodiscard]] bool stale_since_spawn(
        const gameplay::Session& session, std::uint8_t slot,
        const IntentState& intent) noexcept;
    void note_reported_velocity(gameplay::Session& session,
                                std::uint8_t slot, Vec3 reported,
                                std::uint32_t frame) noexcept;

    std::array<std::uint32_t, Slots> last_press_frame_{};
    std::array<bool, Slots> press_seen_{};
    std::array<std::uint32_t, Slots> spawn_intent_frame_{};
    std::array<bool, Slots> was_in_play_{};
    std::array<std::uint32_t, Slots> stale_frames_{};
    std::array<Vec3, Slots> last_report_position_{};
    std::array<std::uint32_t, Slots> last_report_frame_{};
    std::array<bool, Slots> report_seen_{};
    std::array<bool, Slots> authority_spawned_{};
    std::array<IntentState, Slots> latest_intent_{};
    std::array<bool, Slots> latest_intent_seen_{};
    std::array<bool, Slots> state_valid_{};
    std::array<bool, Slots> intent_valid_{};
    PlayerEntityNetAim aim_;
    IntentButtons local_previous_buttons_ = IntentButtons::None;
    std::array<std::uint32_t, IntentState::PressHistory> local_press_history_{};
    Metrics metrics_{};
};

} // namespace fruityprime::net
