#include "Mods/Network/net_room_change.hpp"

#include "Entities/Players/PlayerEntity.hpp"
#include "Entities/gameplay.hpp"
#include "Features.hpp"
#include "GameState.hpp"
#include "Metadata/room_metadata.hpp"
#include "Mods/Network/net_damage.hpp"
#include "Mods/Network/net_log.hpp"
#include "Mods/Network/net_match_end.hpp"
#include "Mods/Network/net_player_bridge.hpp"
#include "Mods/Network/net_player_setup.hpp"
#include "Mods/Network/net_slot_manager.hpp"

#include <algorithm>
#include <array>
#include <iostream>
#include <string>

namespace fruityprime::net {
namespace {

struct RosterSlot {
    bool occupied = false;
    std::uint8_t hunter = 0;
};

[[nodiscard]] RosterSlot roster_slot(const RosterPacket* roster,
                                      std::size_t slot) noexcept {
    if (roster == nullptr) {
        return {};
    }
    const std::size_t count = std::min<std::size_t>(
        roster->count, RosterPacket::MaxSlots);
    for (std::size_t index = 0; index < count; ++index) {
        if (roster->slots[index] == slot) {
            return {true, roster->hunters[index]};
        }
    }
    return {};
}

void disable_network_cheats(NetLog* log) noexcept {
    struct CheatEntry {
        const char* name;
        bool* value;
    };
    const std::array entries{
        CheatEntry{"FreeWeaponSelect", &features::Cheats::FreeWeaponSelect},
        CheatEntry{"UnlimitedJumps", &features::Cheats::UnlimitedJumps},
        CheatEntry{"NoRandomEncounters",
                   &features::Cheats::NoRandomEncounters},
        CheatEntry{"UnlockAllDoors", &features::Cheats::UnlockAllDoors},
        CheatEntry{"ContinueFromCurrentRoom",
                   &features::Cheats::ContinueFromCurrentRoom},
        CheatEntry{"SkipPlanetIntros", &features::Cheats::SkipPlanetIntros},
        CheatEntry{"StartWithAllUpgrades",
                   &features::Cheats::StartWithAllUpgrades},
        CheatEntry{"StartWithAllOctoliths",
                   &features::Cheats::StartWithAllOctoliths},
        CheatEntry{"WalkThroughWalls", &features::Cheats::WalkThroughWalls},
        CheatEntry{"AlwaysFightGorea2",
                   &features::Cheats::AlwaysFightGorea2},
        CheatEntry{"QuadrupleDamage", &features::Cheats::QuadrupleDamage}
    };
    std::string disabled;
    for (const CheatEntry& entry : entries) {
        if (!*entry.value) {
            continue;
        }
        *entry.value = false;
        if (!disabled.empty()) {
            disabled += ", ";
        }
        disabled += entry.name;
    }
    if (disabled.empty()) {
        return;
    }
    std::cout << "[net] cheats are off while connected (" << disabled
              << ")\n";
    if (log != nullptr) {
        log->event("cheats disabled for this session: " + disabled);
    }
}

} // namespace

void NetRoomChange::Reset() noexcept {
    requested_.clear();
    requested_frame_ = 0;
    loaded_match_ = 0;
    loaded_frame_ = 0;
}

void NetRoomChange::Sync(const SyncContext& context) noexcept {
    if (!context.active || context.in_room_transition
        || context.server_match == nullptr) {
        return;
    }
    const MatchStatePacket& state = *context.server_match;
    const std::string_view wanted = state.room_key;
    if (wanted.empty()) {
        return;
    }

    const std::uint16_t match = state.match_id;
    const std::string_view current = context.current_room;
    // A joiner arrives already on the server's map and must not immediately
    // reload it. The first match number is adopted rather than acted on.
    if (current == wanted
        && (loaded_match_ == 0 || loaded_match_ == match)) {
        loaded_match_ = match;
        requested_.clear();
        return;
    }
    // The fade runs before the load begins. Keep our own request marker so a
    // periodic MatchState cannot restart that fade every frame.
    if (requested_ == wanted && loaded_match_ == match
        && context.net_frame - requested_frame_ < RequestRetryFrames) {
        return;
    }

    const metadata::RoomMetadata* meta =
        metadata::find_room_metadata(wanted);
    if (meta == nullptr) {
        std::cout << "[net] server switched to \"" << wanted
                  << "\", which this build does not know\n";
        if (context.log != nullptr) {
            context.log->event("unknown server map \""
                               + std::string(wanted) + "\"");
        }
        requested_ = wanted;
        requested_frame_ = context.net_frame;
        return;
    }

    requested_ = wanted;
    requested_frame_ = context.net_frame;
    loaded_match_ = match;
    std::cout << (current == wanted
                      ? "[net] server started a new match on "
                      : "[net] server rotated to ")
              << wanted << "; loading it\n";
    if (context.log != nullptr) {
        context.log->event("loading " + std::string(wanted)
                           + " for match " + std::to_string(match));
    }
    if (context.game_state != nullptr) {
        context.game_state->transition_room_id = meta->id;
        // Scene.SetFade starts the managed room-transition state. The native
        // host has no separate Scene object, so preserve that state here to
        // make the next Sync call observe the same guard.
        context.game_state->transition_state = game::TransitionState::Start;
    }
    if (context.set_fade != nullptr) {
        context.set_fade();
    }
}

bool NetRoomChange::Settling(std::uint32_t net_frame) noexcept {
    return loaded_frame_ != 0 && net_frame - loaded_frame_ < SettleFrames;
}

int NetRoomChange::RoomPlayerCount(bool active) noexcept {
    return active ? NetworkRoomPlayerCount : 0;
}

bool NetRoomChange::Rebuilding(bool active) noexcept {
    return active;
}

players::PlayerEntity* NetRoomChange::RebuildPlayers(
    const RebuildContext& context) {
    if (context.game_state == nullptr || context.session == nullptr) {
        return nullptr;
    }

    const int local_slot = std::clamp(
        std::max(context.local_slot, 0), 0,
        static_cast<int>(players::PlayerEntity::SlotCapacity - 1));
    // NetLaunch.Join sets this before the managed BuildPlayers call. Keep the
    // same capacity when a native room is rebuilt after a rotation.
    players::PlayerEntity::MaxPlayers(
        players::PlayerEntity::SlotCapacity);

    std::array<players::PlayerEntity*, players::PlayerEntity::SlotCapacity>
        created{};
    for (int slot = 0;
         slot < players::PlayerEntity::MaxPlayers()
         && slot < players::PlayerEntity::SlotCapacity; ++slot) {
        const RosterSlot roster = roster_slot(
            context.roster, static_cast<std::size_t>(slot));
        const std::uint8_t hunter = slot == local_slot
            ? context.local_hunter : roster.hunter;
        auto* player = players::PlayerEntity::Create(
            static_cast<metadata::Hunter>(hunter),
            slot == local_slot ? context.local_recolor : 0);
        created[static_cast<std::size_t>(slot)] = player;
        if (player == nullptr) {
            continue;
        }

        player->ResetReferences();
        auto flags = static_cast<std::uint8_t>(player->LoadFlags());
        flags = static_cast<std::uint8_t>(
            flags | static_cast<std::uint8_t>(formats::LoadFlags::SlotActive)
                | static_cast<std::uint8_t>(formats::LoadFlags::Active)
                | static_cast<std::uint8_t>(formats::LoadFlags::Initial));
        const bool occupied = slot == local_slot || roster.occupied;
        if (!occupied) {
            flags = static_cast<std::uint8_t>(
                flags & ~static_cast<std::uint8_t>(formats::LoadFlags::Active));
        }
        player->LoadFlags(static_cast<formats::LoadFlags>(flags));
        player->IsBot(false);
        player->BotLevel(0);
        if (slot == local_slot) {
            player->Recolor(context.local_recolor);
        }
    }

    // The native Session owns the live simulation records. Build the pooled
    // PlayerEntity objects first (as C# does), then materialize only the
    // occupied slots without changing the slot order of that pool.
    for (int slot = 0; slot < players::PlayerEntity::SlotCapacity; ++slot) {
        const RosterSlot roster = roster_slot(
            context.roster, static_cast<std::size_t>(slot));
        if (slot != local_slot && !roster.occupied) {
            continue;
        }
        if (!context.session->has_player(static_cast<std::uint8_t>(slot))) {
            const std::uint8_t hunter = slot == local_slot
                ? context.local_hunter : roster.hunter;
            static_cast<void>(context.session->add_player(
                static_cast<std::uint8_t>(slot), hunter));
        }
    }

    // Session::add_player notifies the PlayerEntity boundary and restores the
    // Spawned bit. Reapply the C# BuildPlayers flags after that notification.
    for (int slot = 0; slot < players::PlayerEntity::SlotCapacity; ++slot) {
        auto* player = created[static_cast<std::size_t>(slot)];
        if (player == nullptr) {
            continue;
        }
        const RosterSlot roster = roster_slot(
            context.roster, static_cast<std::size_t>(slot));
        auto flags = static_cast<std::uint8_t>(player->LoadFlags());
        flags = static_cast<std::uint8_t>(
            flags | static_cast<std::uint8_t>(formats::LoadFlags::SlotActive)
                | static_cast<std::uint8_t>(formats::LoadFlags::Initial));
        const bool occupied = slot == local_slot || roster.occupied;
        if (occupied) {
            flags = static_cast<std::uint8_t>(
                flags | static_cast<std::uint8_t>(formats::LoadFlags::Active));
        } else {
            flags = static_cast<std::uint8_t>(
                flags & ~static_cast<std::uint8_t>(formats::LoadFlags::Active));
        }
        player->LoadFlags(static_cast<formats::LoadFlags>(flags));
        player->IsBot(false);
        player->BotLevel(0);
        if (slot == local_slot) {
            player->Recolor(context.local_recolor);
        }
    }

    players::PlayerEntity::PlayerCount(1);
    players::PlayerEntity::MainPlayerIndex(local_slot);
    if (context.slot_manager != nullptr) {
        context.slot_manager->reset();
    }
    NetPlayerSetup::Reset();
    if (context.damage != nullptr) {
        context.damage->reset_for_room_change();
    }
    if (context.match_end != nullptr) {
        ResetScores(*context.game_state, *context.match_end);
    } else {
        context.game_state->reset_match_progress();
    }
    std::cout << "[net] player slots rebuilt for the new room, main player = slot "
              << local_slot << "\n";
    return players::PlayerEntity::Players()[static_cast<std::size_t>(local_slot)];
}

void NetRoomChange::AfterRebuild(
    const AfterRebuildContext& context) noexcept {
    loaded_frame_ = std::max(context.net_frame, 1U);
    if (context.player_bridge != nullptr) {
        context.player_bridge->note_room_changed();
    }
    disable_network_cheats(context.log);

    const auto table = players::PlayerEntity::Players();
    const int main_player = players::PlayerEntity::MainPlayerIndex();
    for (std::size_t slot = 0; slot < table.size(); ++slot) {
        players::PlayerEntity* player = table[slot];
        if (player == nullptr || static_cast<int>(slot) == main_player) {
            continue;
        }
        const auto flags = static_cast<std::uint8_t>(player->LoadFlags());
        if ((flags & static_cast<std::uint8_t>(
                formats::LoadFlags::SlotActive)) == 0) {
            if (context.log != nullptr) {
                context.log->event(
                    "slot " + std::to_string(slot)
                    + " skipped on rebuild: flags="
                    + std::to_string(flags));
            }
            continue;
        }
        // Session::add_player has already inserted the native simulation
        // record. This creates the equivalent child entity used by
        // Scene.InitEntity(player.Halfturret) in the managed path.
        player->CreateHalfturret();
        if (context.log != nullptr) {
            context.log->event("slot " + std::to_string(slot)
                               + " re-inserted into the new room");
        }
    }
}

const std::string& NetRoomChange::Requested() noexcept {
    return requested_;
}

std::uint16_t NetRoomChange::LoadedMatch() noexcept {
    return loaded_match_;
}

std::uint32_t NetRoomChange::LoadedFrame() noexcept {
    return loaded_frame_;
}

void NetRoomChange::ResetScores(game::State& state,
                                MatchEnd& match_end) noexcept {
    for (std::size_t slot = 0; slot < players::PlayerEntity::SlotCapacity;
         ++slot) {
        state.points[slot] = 0;
        state.team_points[slot] = 0;
        state.kills[slot] = 0;
        state.team_kills[slot] = 0;
        state.deaths[slot] = 0;
        state.team_deaths[slot] = 0;
        state.standings[slot] = 0;
        state.team_standings[slot] = 0;
        state.damage_count[slot] = 0;
        state.kill_streak[slot] = 0;
    }
    state.reset_match_progress();
    match_end.reset();
}

} // namespace fruityprime::net
