#include "Mods/Network/net_player_setup.hpp"

#include "Entities/Players/PlayerEntity.hpp"

#include <iostream>

namespace fruityprime::net {
namespace {

detail::RuntimeBindings runtime{};

[[nodiscard]] std::size_t count_active() noexcept {
    const auto player_table = players::PlayerEntity::Players();
    const int max_players = players::PlayerEntity::MaxPlayers();
    std::size_t active = 0;
    for (int slot = 0; slot < max_players
             && static_cast<std::size_t>(slot) < player_table.size(); ++slot) {
        const players::PlayerEntity* player =
            player_table[static_cast<std::size_t>(slot)];
        if (player != nullptr
            && (static_cast<std::uint8_t>(player->LoadFlags())
                & static_cast<std::uint8_t>(formats::LoadFlags::Active)) != 0) {
            ++active;
        }
    }
    return active;
}

} // namespace

namespace detail {

void BindRuntime(RuntimeBindings bindings) noexcept {
    runtime = bindings;
}

} // namespace detail

void NetPlayerSetup::Reset() noexcept {
    applied_ = false;
}

void NetPlayerSetup::ApplyOnce() noexcept {
    if (applied_ || runtime.Active == nullptr || !runtime.Active()) {
        return;
    }
    if (runtime.LocalSlot == nullptr) {
        return;
    }
    const int local_slot = runtime.LocalSlot();
    if (local_slot < 0) {
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

    std::cout << "[net] player slots prepared -- local slot " << local_slot
              << ", " << count_active()
              << " active, AI disabled on remote slots\n";
}

} // namespace fruityprime::net
