#include "Mods/Network/net_room_change.hpp"

#include "Entities/Players/PlayerEntity.hpp"
#include "GameState.hpp"
#include "Metadata/room_metadata.hpp"
#include "Mods/Network/net_damage.hpp"
#include "Mods/Network/net_log.hpp"
#include "Mods/Network/net_launch.hpp"
#include "Mods/Network/net_match_end.hpp"
#include "Mods/Network/net_player_bridge.hpp"
#include "Mods/Network/net_player_setup.hpp"
#include "Mods/Network/net_slot_manager.hpp"

#include <algorithm>
#include <iostream>
#include <string>

namespace fruityprime::net {
namespace {

struct RosterSlot {
    bool occupied = false;
    std::uint8_t hunter = 0;
};

[[nodiscard]] RosterSlot roster_slot(const RosterPacket& roster,
                                      std::size_t slot) noexcept {
    for (std::size_t index = 0; index < roster.count; ++index) {
        if (roster.slots[index] == slot) {
            return {true, roster.hunters[index]};
        }
    }
    return {};
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
        || context.current_room.empty()) {
        return;
    }
    const MatchStatePacket& state = context.server_match;
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
        context.log.event("unknown server map \""
                          + std::string(wanted) + "\"");
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
    context.log.event("loading " + std::string(wanted)
                      + " for match " + std::to_string(match));
    context.game_state.transition_room_id = meta->id;
    context.set_fade();
}

bool NetRoomChange::Settling(std::uint32_t net_frame) noexcept {
    return loaded_frame_ != 0 && net_frame - loaded_frame_ < SettleFrames;
}

int NetRoomChange::RoomPlayerCount() noexcept {
    return NetLaunch::active() ? NetLaunch::RoomPlayerCount : 0;
}

bool NetRoomChange::Rebuilding() noexcept {
    return NetLaunch::active();
}

players::PlayerEntity* NetRoomChange::RebuildPlayers(
    const RebuildContext& context) {
    const int local_slot = std::max(context.local_slot, 0);
    const int max_players = players::PlayerEntity::MaxPlayers();

    for (int slot = 0; slot < max_players; ++slot) {
        const RosterSlot roster = roster_slot(
            context.roster, static_cast<std::size_t>(slot));
        const std::uint8_t hunter = slot == local_slot
            ? context.local_hunter : roster.hunter;
        auto* player = players::PlayerEntity::Create(
            static_cast<metadata::Hunter>(hunter),
            slot == local_slot ? context.local_recolor : 0);
        if (player == nullptr) {
            continue;
        }

        // PlayerEntity.Create has already performed CreateHalfturret.  C#
        // CreateHalfturret then calls Scene.InitEntity before RebuildPlayers
        // changes the returned player's Active/Initial flags.
        context.init_halfturret(player->Halfturret());

        auto flags = static_cast<std::uint8_t>(player->LoadFlags());
        flags = static_cast<std::uint8_t>(
            flags | static_cast<std::uint8_t>(formats::LoadFlags::SlotActive));
        player->LoadFlags(static_cast<formats::LoadFlags>(flags));
        flags = static_cast<std::uint8_t>(
            flags | static_cast<std::uint8_t>(formats::LoadFlags::Active));
        player->LoadFlags(static_cast<formats::LoadFlags>(flags));
        flags = static_cast<std::uint8_t>(
            flags | static_cast<std::uint8_t>(formats::LoadFlags::Initial));
        player->LoadFlags(static_cast<formats::LoadFlags>(flags));
        // C# RebuildPlayers does not call PlayerEntity.ResetReferences here;
        // RoomEntity.StartTransition performs that reset before rebuilding.
        player->NodeRef(culling::NodeRef::none());
        player->Camera().info().node_ref = culling::NodeRef::none();
        player->IsBot(false);
        player->BotLevel(0);
        const bool occupied = slot == local_slot || roster.occupied;
        if (!occupied) {
            flags = static_cast<std::uint8_t>(
                flags & ~static_cast<std::uint8_t>(formats::LoadFlags::Active));
            player->LoadFlags(static_cast<formats::LoadFlags>(flags));
        }
    }

    players::PlayerEntity::PlayerCount(1);
    players::PlayerEntity::MainPlayerIndex(local_slot);
    context.slot_manager.reset();
    NetPlayerSetup::Reset();
    context.damage.reset_for_room_change();
    ResetScores(context.game_state, context.match_end);
    std::cout << "[net] player slots rebuilt for the new room, main player = slot "
              << local_slot << "\n";
    return players::PlayerEntity::Players()[static_cast<std::size_t>(local_slot)];
}

void NetRoomChange::AfterRebuild(
    const AfterRebuildContext& context) noexcept {
    loaded_frame_ = std::max(context.net_frame, 1U);
    context.player_bridge.note_room_changed();
    NetLaunch::disable_cheats_for_match(context.log);

    const auto table = players::PlayerEntity::Players();
    const int main_player = players::PlayerEntity::MainPlayerIndex();
    for (std::size_t slot = 0; slot < table.size(); ++slot) {
        players::PlayerEntity* player = table[slot];
        if (static_cast<int>(slot) == main_player) {
            continue;
        }
        const auto flags = static_cast<std::uint8_t>(player->LoadFlags());
        if ((flags & static_cast<std::uint8_t>(
                formats::LoadFlags::SlotActive)) == 0) {
            context.log.event("slot " + std::to_string(slot)
                              + " skipped on rebuild: flags="
                              + std::to_string(flags));
            continue;
        }
        context.insert_entity(*player);
        context.initialize(*player);
        context.init_entity(*player);
        context.init_halfturret(player->Halfturret());
        context.log.event("slot " + std::to_string(slot)
                          + " re-inserted into the new room");
    }
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
