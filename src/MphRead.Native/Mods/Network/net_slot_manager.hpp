#pragma once

#include "GameState.hpp"
#include "Entities/gameplay.hpp"
#include "Mods/Network/net_protocol.hpp"

#include <array>
#include <cstddef>
#include <cstdint>

namespace fruityprime::net {

struct SlotSyncResult {
    std::size_t added = 0;
    std::size_t removed = 0;
    std::size_t hunter_changes = 0;
    std::size_t active = 0;
};

// Owns the transition bookkeeping for a network roster. The managed
// NetSlotManager is stateful because the same room stays alive while peers
// enter and leave; keeping that state here avoids leaving score/input state
// attached to a reused slot.
class SlotManager {
public:
    void reset() noexcept;

    [[nodiscard]] SlotSyncResult sync(
        gameplay::Session& session, const RosterPacket& roster,
        std::uint8_t local_slot, game::State& game_state);

private:
    std::array<bool, game::SlotCapacity> activated_{};
};

} // namespace fruityprime::net
