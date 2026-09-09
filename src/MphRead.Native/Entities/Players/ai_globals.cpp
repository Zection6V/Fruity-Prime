// Native counterpart of the static half of src/MphRead/Entities/Players/PlayerAi.cs.
#include "ai_globals.hpp"

#include <algorithm>
#include <utility>

namespace fruityprime::players {

namespace {

[[nodiscard]] bool same_position(formats::Vector3 a,
                                 formats::Vector3 b) noexcept {
    return a.x == b.x && a.y == b.y && a.z == b.z;
}

[[nodiscard]] formats::Vector3 add_y(formats::Vector3 value,
                                     float amount) noexcept {
    return {value.x, value.y + amount, value.z};
}

} // namespace

void AiGlobalState::initialize_globals() noexcept {
    global_field0_ = 0;
    global_field2_ = 0;
    for (AiGlobals& globals : global_objs_) {
        globals.player_slot = -1;
        globals.field4 = 0;
        globals.node_data_index = 0;
        globals.node_data.clear();
    }
    for (auto& row : visibility_) {
        row.fill(false);
    }
    // The pair walk starts at (1, 0) rather than (0, 0), because a player is
    // trivially visible to themselves and that entry is never read.
    vis_index1_ = 1;
    vis_index2_ = 0;
}

void AiGlobalState::update_visibility(std::span<const AiPlayerView> players,
                                      int max_players,
                                      const BlockedQuery& blocked) {
    const std::size_t a = vis_index1_;
    const std::size_t b = vis_index2_;
    if (a >= SlotCapacity || b >= SlotCapacity) {
        return;
    }
    visibility_[a][b] = false;
    visibility_[b][a] = false;
    if (a < players.size() && b < players.size()) {
        const AiPlayerView& player1 = players[a];
        const AiPlayerView& player2 = players[b];
        // Only pairs involving a bot are worth testing: nothing else reads the
        // table.
        if (player1.health != 0 && player1.active && player2.health != 0
            && player2.active && (player1.is_bot || player2.is_bot)) {
            formats::Vector3 pos1 = player1.camera_position;
            formats::Vector3 pos2 = player2.camera_position;
            // Two players standing in exactly the same spot -- which happens
            // at a spawn -- would otherwise be tested along a zero-length
            // line, so their bodies are used instead.
            if (same_position(pos1, pos2)) {
                pos1 = player1.position;
                pos2 = player2.position;
            }
            if (blocked && !blocked(pos1, pos2)) {
                visibility_[a][b] = true;
                visibility_[b][a] = true;
            }
        }
    }
    // One unordered pair per frame, over however many slots this match has.
    const int slots = std::clamp(max_players, 2,
                                 static_cast<int>(SlotCapacity));
    if (++vis_index1_ >= static_cast<std::uint8_t>(slots)) {
        if (++vis_index2_ >= static_cast<std::uint8_t>(slots - 1)) {
            vis_index2_ = 0;
        }
        vis_index1_ = static_cast<std::uint8_t>(vis_index2_ + 1);
    }
}

AiGlobalsResult AiGlobalState::update_globals(
    std::span<const AiPlayerView> players, const BlockedQuery& blocked) {
    AiGlobalsResult result;
    if (global_field2_ == 0) {
        return result;
    }
    if (global_field0_ >= global_field2_) {
        global_field0_ = 0;
    }
    AiGlobals& global = global_objs_[static_cast<std::size_t>(global_field0_)];
    const int slot = global.player_slot;
    if (slot < 0 || static_cast<std::size_t>(slot) >= players.size()
        || global.node_data_index >= global.node_data.size()) {
        // Nothing usable is left on this entry; drop it rather than reading
        // past the end of its node list.
        remove_player_from_globals(slot);
        result.ran = true;
        result.player_slot = slot;
        result.removed = true;
        return result;
    }
    const AiPlayerView& player = players[static_cast<std::size_t>(slot)];
    const formats::Vector3 node = global.node_data[global.node_data_index];
    // A morphed player is lower to the ground, so the line is cast from lower
    // down; the node is measured half a unit up from the floor either way.
    const formats::Vector3 pos1 = add_y(player.position,
                                        player.alt_form ? 0.5F : 1.0F);
    const formats::Vector3 pos2 = add_y(node, 0.5F);
    result.ran = true;
    result.player_slot = slot;
    if (blocked && blocked(pos1, pos2)) {
        ++global.node_data_index;
        --global.field4;
        if (global.field4 == 0) {
            remove_player_from_globals(slot);
            result.removed = true;
        }
    } else {
        result.node_found = true;
        result.node = node;
        remove_player_from_globals(slot);
        result.removed = true;
    }
    ++global_field0_;
    return result;
}

AiGlobalsResult AiGlobalState::update_visibility_and_globals(
    std::span<const AiPlayerView> players, int max_players,
    const BlockedQuery& blocked) {
    update_visibility(players, max_players, blocked);
    return update_globals(players, blocked);
}

void AiGlobalState::add_player_to_globals(
    int player_slot, int field4, std::vector<formats::Vector3> node_data) {
    if (player_slot < 0) {
        return;
    }
    for (int i = 0; i < global_field2_; ++i) {
        AiGlobals& existing = global_objs_[static_cast<std::size_t>(i)];
        if (existing.player_slot == player_slot) {
            existing.field4 = field4;
            existing.node_data = std::move(node_data);
            // Index 1, not 0: the first node is where the bot already is.
            existing.node_data_index = 1;
            return;
        }
    }
    if (global_field2_ >= static_cast<int>(global_objs_.size())) {
        return;
    }
    AiGlobals& next = global_objs_[static_cast<std::size_t>(global_field2_)];
    next.player_slot = player_slot;
    next.field4 = field4;
    next.node_data = std::move(node_data);
    next.node_data_index = 1;
    ++global_field2_;
}

void AiGlobalState::remove_player_from_globals(int player_slot) noexcept {
    if (global_field2_ == 0) {
        return;
    }
    int i = 0;
    for (; i < global_field2_; ++i) {
        if (global_objs_[static_cast<std::size_t>(i)].player_slot
            == player_slot) {
            break;
        }
    }
    if (i == global_field2_) {
        return;
    }
    // Compacting rather than leaving a hole: the queue is walked by index, so
    // a hole would be visited as if it held a bot.
    for (; i < global_field2_ - 1; ++i) {
        AiGlobals& current = global_objs_[static_cast<std::size_t>(i)];
        AiGlobals& next = global_objs_[static_cast<std::size_t>(i + 1)];
        current.player_slot = next.player_slot;
        current.field4 = next.field4;
        current.node_data_index = next.node_data_index;
        current.node_data = next.node_data;
    }
    --global_field2_;
    AiGlobals& tail = global_objs_[static_cast<std::size_t>(global_field2_)];
    tail.player_slot = -1;
    tail.field4 = 0;
    tail.node_data_index = 0;
    tail.node_data.clear();
}

} // namespace fruityprime::players
