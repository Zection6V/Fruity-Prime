// Snapshot import/export for the native gameplay session.
//
// The network and replay frontends consume the same compact player state;
// keeping this boundary separate mirrors the managed GameState snapshot path.
#include "Entities/gameplay.hpp"
#include "Entities/Players/PlayerEntity.hpp"

#include <algorithm>

namespace fruityprime::gameplay {

void Session::apply_snapshot(const net::SnapshotPacket& snapshot) {
    for (const auto& state : snapshot.players) {
        if (!has_player(state.slot_index)) {
            if (players_.size() >= net::NetConfig::SlotCapacity) {
                continue;
            }
            static_cast<void>(add_player(state.slot_index));
        }
        const std::size_t index = player_index(state.slot_index);
        const bool was_alive = players_[index].health != 0;
        const std::uint8_t previous_weapon = players_[index].current_weapon;
        players_[index] = state;
        players::PlayerEntity::SessionWeaponChanged(
            *this, state.slot_index, previous_weapon, state.current_weapon);
        const auto runtime = std::find_if(
            inputs_.begin(), inputs_.end(),
            [slot = state.slot_index](const RuntimeInput& value) {
                return value.slot == slot;
            });
        if (runtime != inputs_.end()) {
            if (state.health == 0) {
                if (config_.survival_mode
                    && state.deaths > config_.survival_lives) {
                    runtime->eliminated = true;
                    runtime->respawn_ticks = 0;
                } else if (was_alive) {
                    runtime->eliminated = false;
                    runtime->respawn_ticks = config_.respawn_ticks;
                }
            } else if (state.health != 0) {
                runtime->eliminated = false;
                runtime->respawn_ticks = 0;
            }
        }
    }
}

net::SnapshotPacket Session::snapshot(std::uint32_t frame,
                                      std::uint32_t rng1,
                                      std::uint32_t rng2) const {
    net::SnapshotPacket snapshot;
    snapshot.header.frame = frame;
    snapshot.header.rng1 = rng1;
    snapshot.header.rng2 = rng2;
    snapshot.players = players_;
    return snapshot;
}

} // namespace fruityprime::gameplay
