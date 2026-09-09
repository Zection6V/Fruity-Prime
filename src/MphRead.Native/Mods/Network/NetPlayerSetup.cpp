#include "Mods/Network/net_player_setup.hpp"

namespace fruityprime::net {

PlayerSetupResult apply_player_setup(gameplay::Session& session,
                                     int local_slot) noexcept {
    PlayerSetupResult result;
    if (local_slot < 0 || local_slot >= static_cast<int>(game::SlotCapacity)) {
        return result;
    }
    result.applied = true;
    result.local_slot = static_cast<std::uint8_t>(local_slot);
    for (const auto& player : session.players()) {
        session.prepare_network_player(player.slot_index);
        ++result.active_players;
        if (player.slot_index != result.local_slot) {
            ++result.remote_players;
        }
    }
    return result;
}

void NetPlayerSetup::Reset() noexcept {
    applied_ = false;
}

PlayerSetupResult NetPlayerSetup::ApplyOnce(
    gameplay::Session& session, int local_slot, bool network_active) noexcept {
    if (applied_ || !network_active) {
        return {};
    }
    PlayerSetupResult result = apply_player_setup(session, local_slot);
    if (result.applied) {
        applied_ = true;
    }
    return result;
}

} // namespace fruityprime::net
