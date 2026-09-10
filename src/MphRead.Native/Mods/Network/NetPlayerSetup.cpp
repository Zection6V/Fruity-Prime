#include "Mods/Network/net_player_setup.hpp"

#include "Entities/Players/PlayerEntity.hpp"

namespace fruityprime::net {

void NetPlayerSetup::Reset() noexcept {
    applied_ = false;
}

void NetPlayerSetup::ApplyOnce(gameplay::Session& session,
                               int local_slot) noexcept {
    if (applied_ || local_slot < 0) {
        return;
    }
    applied_ = true;

    // NetPlayerSetup.cs points PlayerEntity.Main at the slot driven by this
    // process, then clears IsBot for every created player and BotLevel for
    // every remote player.
    if (local_slot < players::PlayerEntity::MaxPlayers()) {
        players::PlayerEntity::MainPlayerIndex(local_slot);
    }
    const auto player_table = players::PlayerEntity::Players();
    const int max_players = players::PlayerEntity::MaxPlayers();
    for (int slot = 0; slot < max_players
             && static_cast<std::size_t>(slot) < player_table.size(); ++slot) {
        players::PlayerEntity* player =
            player_table[static_cast<std::size_t>(slot)];
        if (player == nullptr) {
            continue;
        }
        player->IsBot(false);
        if (slot != local_slot) {
            player->BotLevel(0);
        }
    }
}

} // namespace fruityprime::net
