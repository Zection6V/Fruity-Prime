#pragma once

#include "Mods/Network/net_protocol.hpp"

#include <array>
#include <cstdint>

namespace fruityprime::gameplay {
class Session;
}

namespace fruityprime::net {

// Native counterpart of MphRead/Mods/Network/NetDamage.cs.  The gameplay
// session owns the authoritative PlayerState fields; this object owns the
// per-client history needed to decide whether a wire damage event is new and
// to reproduce its presentation without counting it twice.
class DamageBridge final {
public:
    static constexpr std::uint8_t NoSlot = 0xff;
    static constexpr std::uint8_t NoBeam = 0xff;

    struct Metrics {
        std::array<std::uint32_t, NetConfig::SlotCapacity> resolved{};
        std::array<std::uint32_t, NetConfig::SlotCapacity> replayed{};
    };

    void reset() noexcept;
    void reset_for_room_change() noexcept;
    void forget_slot(std::uint8_t slot) noexcept;

    [[nodiscard]] bool suppress(bool network_active,
                                 bool authority) const noexcept {
        return network_active && !authority;
    }

    // Keep this hook separate from PlayerState mutation.  Projectile and
    // enemy code may use it when a future frontend has a damage source that is
    // not represented by the current compact state.
    void note_resolved(std::uint8_t victim_slot, std::uint8_t attacker_slot,
                       std::uint8_t beam, std::uint8_t flags,
                       Vec3 impulse) noexcept;
    void write(std::uint8_t slot, PlayerState& state) const noexcept;

    // Apply only the feedback side of a newly observed authoritative hit.
    // The caller subsequently applies the authoritative PlayerState, so this
    // never owns scoring or the final health value.
    void replay(gameplay::Session& session,
                const PlayerState& state) noexcept;

    [[nodiscard]] const Metrics& metrics() const noexcept { return metrics_; }

private:
    static constexpr std::uint8_t MaxCatchUp = 32;
    static constexpr float MaxImpulse = 1.5F;

    [[nodiscard]] static Vec3 clamp_impulse(Vec3 impulse) noexcept;

    std::array<std::uint8_t, NetConfig::SlotCapacity> sequence_{};
    std::array<std::uint8_t, NetConfig::SlotCapacity> attacker_{};
    std::array<std::uint8_t, NetConfig::SlotCapacity> beam_{};
    std::array<std::uint8_t, NetConfig::SlotCapacity> flags_{};
    std::array<Vec3, NetConfig::SlotCapacity> direction_{};
    std::array<std::uint8_t, NetConfig::SlotCapacity> last_seen_{};
    std::array<bool, NetConfig::SlotCapacity> ever_seen_{};
    bool replaying_ = false;
    std::uint8_t replay_beam_ = NoBeam;
    Metrics metrics_{};
};

} // namespace fruityprime::net
