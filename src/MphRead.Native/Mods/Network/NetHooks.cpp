#include "Mods/Network/net_hooks.hpp"

#include "Entities/gameplay.hpp"
#include "Mods/Network/net_damage.hpp"
#include "Mods/Network/net_player_bridge.hpp"

#include <algorithm>
#include <cmath>

namespace fruityprime::net {
namespace {

[[nodiscard]] bool valid_slot(std::uint8_t slot) noexcept {
    return slot < NetConfig::SlotCapacity;
}

[[nodiscard]] bool finite_nonzero(Vec3 value) noexcept {
    const float length_squared = value.x * value.x + value.y * value.y
        + value.z * value.z;
    return std::isfinite(value.x) && std::isfinite(value.y)
        && std::isfinite(value.z) && length_squared > 0.0001F;
}

} // namespace

int NetHooks::local_slot(bool active, int assigned_slot,
                         bool playback) noexcept {
    if (playback) {
        return -1;
    }
    return active && assigned_slot >= 0 ? assigned_slot : 0;
}

bool NetHooks::keep_slot_alive(const NetHookContext& context) noexcept {
    return context.active;
}

bool NetHooks::try_apply_remote_input(
    gameplay::Session& session, NetPlayerBridge& bridge, std::uint8_t slot,
    const IntentState& intent, const NetHookContext& context) noexcept {
    if (!context.active || !valid_slot(slot)
        || slot == local_slot(true, context.local_slot, context.playback)) {
        return false;
    }

    // A roster packet may arrive before the scene materializes the slot. The
    // managed hook still claims the slot as remote so the keyboard path is
    // skipped; the caller will retry after roster reconciliation.
    if (!session.has_player(slot)) {
        return true;
    }
    static_cast<void>(bridge.apply_intent(session, slot, intent));
    return true;
}

bool NetHooks::apply_authority_snapshot(
    gameplay::Session& session, NetPlayerBridge& bridge,
    const SnapshotPacket& snapshot, DamageBridge& damage,
    const NetHookContext& context) noexcept {
    if (!context.active || context.authority) {
        return false;
    }
    return bridge.apply_snapshot(session, snapshot,
                                 static_cast<std::uint8_t>(
                                     std::max(0, context.local_slot)),
                                 context.settling, damage);
}

void NetHooks::after_simulation(gameplay::Session& session,
                                NetPlayerBridge& bridge,
                                const NetHookContext& context) noexcept {
    if (!context.active) {
        return;
    }
    bridge.restore_reported_positions(
        session, static_cast<std::uint8_t>(
            std::max(0, context.local_slot)));
}

Vec3 NetHooks::remote_shot_origin(
    Vec3 current, Vec3 player_position, const IntentState& intent,
    const NetHookContext& context, std::uint8_t player_slot) noexcept {
    if (!context.active || !context.authority || !valid_slot(player_slot)
        || player_slot == context.local_slot
        || !finite_nonzero(intent.position)) {
        return current;
    }
    return {current.x + intent.position.x - player_position.x,
            current.y + intent.position.y - player_position.y,
            current.z + intent.position.z - player_position.z};
}

Vec3 NetHooks::remote_shot_direction(
    Vec3 current, const IntentState& intent,
    const NetHookContext& context, std::uint8_t player_slot) noexcept {
    if (context.active && context.authority && valid_slot(player_slot)
        && player_slot != context.local_slot && finite_nonzero(intent.aim)) {
        const float length = std::sqrt(intent.aim.x * intent.aim.x
            + intent.aim.y * intent.aim.y + intent.aim.z * intent.aim.z);
        return {intent.aim.x / length, intent.aim.y / length,
                intent.aim.z / length};
    }
    return current;
}

bool NetHooks::force_spawn(
    bool force_everyone, const NetHookContext& context,
    std::uint8_t player_slot,
    const std::array<bool, NetConfig::SlotCapacity>& occupied) noexcept {
    if (force_everyone) {
        return true;
    }
    if (!context.active || !valid_slot(player_slot)
        || (!context.authority && !context.playback)) {
        return false;
    }
    return player_slot == context.local_slot || occupied[player_slot];
}

} // namespace fruityprime::net
