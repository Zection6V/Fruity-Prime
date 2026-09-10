#include "Mods/Network/net_slot_manager.hpp"

#include "Mods/Network/net_scoreboard.hpp"

namespace fruityprime::net {

void SlotManager::reset() noexcept {
    activated_.fill(false);
}

SlotSyncResult SlotManager::sync(gameplay::Session& session,
                                 const RosterPacket& roster,
                                 std::uint8_t local_slot,
                                 game::State& game_state) {
    SlotSyncResult result;
    std::array<bool, game::SlotCapacity> present{};
    if (local_slot < present.size()) {
        // The local slot is occupied from the client's point of view even
        // when a roster packet is delayed behind Welcome.
        present[local_slot] = true;
    }

    for (std::size_t i = 0; i < roster.count
        && i < RosterPacket::MaxSlots; ++i) {
        const std::uint8_t slot = roster.slots[i];
        if (slot >= present.size()) {
            continue;
        }
        present[slot] = true;
        const std::uint8_t hunter = roster.hunters[i];
        if (!session.has_player(slot)) {
            static_cast<void>(session.add_player(slot, hunter));
            activated_[slot] = true;
            NetScoreboard::ForgetSlot(game_state, slot);
            ++result.added;
            continue;
        }
        if (!activated_[slot]) {
            activated_[slot] = true;
            NetScoreboard::ForgetSlot(game_state, slot);
        }
        if (session.player_hunter(slot) != hunter) {
            session.set_player_hunter(slot, hunter);
            ++result.hunter_changes;
        }
    }

    std::array<std::uint8_t, game::SlotCapacity> departed{};
    std::size_t departed_count = 0;
    for (const auto& player : session.players()) {
        const std::uint8_t slot = player.slot_index;
        if (slot == local_slot || slot >= present.size() || present[slot]) {
            continue;
        }
        departed[departed_count++] = slot;
    }
    for (std::size_t i = 0; i < departed_count; ++i) {
        const std::uint8_t slot = departed[i];
        session.remove_player(slot);
        activated_[slot] = false;
        NetScoreboard::ForgetSlot(game_state, slot);
        ++result.removed;
    }

    result.active = session.players().size();
    return result;
}

} // namespace fruityprime::net
