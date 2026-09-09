#pragma once

#include "Mods/Network/net_protocol.hpp"

#include <array>
#include <cstdint>

namespace fruityprime::gameplay {
class Session;
}

namespace fruityprime::net {

class DamageBridge;
class NetPlayerBridge;

// The engine-facing state used by the two per-frame network hook points. It
// deliberately contains policy only; sockets and packet retention remain in
// NetSession/NetClient, while gameplay state remains in Session.
struct NetHookContext {
    bool active = false;
    bool authority = false;
    bool playback = false;
    bool settling = false;
    int local_slot = 0;
};

class NetHooks final {
public:
    NetHooks() = delete;

    // This matches the managed fallback: an unassigned live client uses slot
    // zero temporarily, while demo playback has no local player at all.
    [[nodiscard]] static int local_slot(bool active, int assigned_slot,
                                         bool playback) noexcept;

    [[nodiscard]] static bool keep_slot_alive(
        const NetHookContext& context) noexcept;

    // Apply a remote SlotIntent to the same Session that will simulate the
    // frame. Position, aim, edge history, ammo, and form flags are handled by
    // NetPlayerBridge; this boundary owns only the remote-slot decision.
    [[nodiscard]] static bool try_apply_remote_input(
        gameplay::Session& session, NetPlayerBridge& bridge,
        std::uint8_t slot, const IntentState& intent,
        const NetHookContext& context) noexcept;

    // Clients and playback import authority state. The authority itself must
    // never overwrite its locally simulated world from a snapshot.
    [[nodiscard]] static bool apply_authority_snapshot(
        gameplay::Session& session, NetPlayerBridge& bridge,
        const SnapshotPacket& snapshot, DamageBridge& damage,
        const NetHookContext& context) noexcept;

    // Run after the fixed-step simulation so a puppet's animation/input path
    // still runs, but its reported position is not simulated twice.
    static void after_simulation(gameplay::Session& session,
                                 NetPlayerBridge& bridge,
                                 const NetHookContext& context) noexcept;

    [[nodiscard]] static Vec3 remote_shot_origin(
        Vec3 current, Vec3 player_position, const IntentState& intent,
        const NetHookContext& context, std::uint8_t player_slot) noexcept;

    [[nodiscard]] static Vec3 remote_shot_direction(
        Vec3 current, const IntentState& intent,
        const NetHookContext& context, std::uint8_t player_slot) noexcept;

    [[nodiscard]] static bool force_spawn(
        bool force_everyone, const NetHookContext& context,
        std::uint8_t player_slot,
        const std::array<bool, NetConfig::SlotCapacity>& occupied) noexcept;
};

} // namespace fruityprime::net
