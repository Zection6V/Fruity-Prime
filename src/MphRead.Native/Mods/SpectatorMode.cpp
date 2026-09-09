#include "Mods/spectator_mode.hpp"

#include <algorithm>

namespace fruityprime::mods::spectator {

bool Controller::can_watch_player(
    const net::PlayerState& player, std::uint8_t local_slot) noexcept {
    return player.slot_index != local_slot
        && player.health > 0
        && (player.flags & net::PlayerState::FlagActive) != 0
        && (player.flags & net::PlayerState::FlagSpawned) != 0;
}

int Controller::find_next_active(const gameplay::Session& session,
                                 int from_slot,
                                 std::uint8_t local_slot) noexcept {
    constexpr std::size_t slot_count = net::NetConfig::SlotCapacity;
    const int normalized = ((from_slot % static_cast<int>(slot_count))
                            + static_cast<int>(slot_count))
        % static_cast<int>(slot_count);
    for (std::size_t offset = 1; offset <= slot_count; ++offset) {
        const auto slot = static_cast<std::uint8_t>(
            (static_cast<std::size_t>(normalized) + offset) % slot_count);
        const auto found = std::find_if(
            session.players().begin(), session.players().end(),
            [slot, local_slot](const net::PlayerState& player) {
                return player.slot_index == slot
                    && can_watch_player(player, local_slot);
            });
        if (found != session.players().end()) {
            return static_cast<int>(slot);
        }
    }
    return -1;
}

void Controller::switch_view(gameplay::Session& session,
                             std::uint8_t slot) noexcept {
    if (!session.has_player(slot)) {
        return;
    }
    view_slot_ = slot;
    // Native PlayerState is the camera source for the current renderer.  The
    // managed implementation additionally initializes the target HUD and
    // CameraInfo.NodeRef here; those objects do not exist in the native
    // frontend, so the equivalent is selecting the target slot atomically.
    free_camera_ = false;
}

bool Controller::start(gameplay::Session& session, std::uint8_t local_slot,
                       bool multiplayer, bool watch_someone,
                       input::State& input) {
    (void) input;
    if (is_spectating_ || !multiplayer) {
        return false;
    }
    const int next = find_next_active(session, local_slot, local_slot);
    if (watch_someone && next < 0) {
        // Demo playback calls Start(true) until a live player has arrived.
        return false;
    }

    is_spectating_ = true;
    show_scoreboard_ = false;
    view_slot_ = local_slot;
    if (session.has_player(local_slot)) {
        session.mutable_player(local_slot).flags |=
            net::PlayerState::FlagSpectating;
    }
    if (watch_someone) {
        switch_view(session, static_cast<std::uint8_t>(next));
    } else {
        // This is the overview first.  The native camera follows the local
        // position while free_camera_ is true, which is the same stable
        // starting point as the managed scene camera request.
        free_camera_ = true;
    }
    return true;
}

bool Controller::cycle_next(gameplay::Session& session,
                            std::uint8_t local_slot) {
    if (!is_spectating_) {
        return false;
    }
    const int next = find_next_active(session, view_slot_, local_slot);
    if (next < 0) {
        return false;
    }
    switch_view(session, static_cast<std::uint8_t>(next));
    return true;
}

bool Controller::toggle_view(gameplay::Session& session,
                             std::uint8_t local_slot) {
    if (!is_spectating_) {
        return false;
    }
    if (free_camera_) {
        return cycle_next(session, local_slot);
    }
    free_camera_ = true;
    return true;
}

bool Controller::toggle(gameplay::Session& session, std::uint8_t local_slot,
                        bool multiplayer, bool replay_active,
                        input::State& input) {
    if (replay_active) {
        return false;
    }
    if (!is_spectating_) {
        return start(session, local_slot, multiplayer, false, input);
    }
    return cycle_next(session, local_slot);
}

bool Controller::rejoin(gameplay::Session& session, std::uint8_t local_slot) {
    if (!is_spectating_ || !session.has_player(local_slot)) {
        return false;
    }
    session.rejoin_player(local_slot);
    is_spectating_ = false;
    free_camera_ = false;
    show_scoreboard_ = false;
    view_slot_ = local_slot;
    return true;
}

} // namespace fruityprime::mods::spectator
