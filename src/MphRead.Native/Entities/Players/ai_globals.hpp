#pragma once

// Native counterpart of the static half of PlayerAi: the shared visibility
// table, the queue of bots waiting on a line-of-sight check to a path node,
// and the entity-reference slots a bot fills in as it looks around.
//
// Three things here are worth stating outright, because all three were bugs
// in the managed code before they were fixed there:
//
//   * both tables are sized from the slot capacity, not from the four players
//     a DS match could hold.  They are indexed by slot, so a fifth player made
//     the first bot that looked at either of them read past the end -- which
//     is every bot, on the frame it first picks a target;
//   * visibility is symmetric and is refreshed one *pair* per frame, walking
//     every unordered pair in turn.  The walk is what keeps the cost flat as
//     players are added, and getting its wrap wrong silently freezes some
//     pairs at whatever they last were;
//   * the globals queue is a compacting array, not a free list: removing an
//     entry shuffles the rest down.  A bot that is removed while another is
//     mid-scan is therefore never skipped.

#include "Formats/Types.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <span>
#include <vector>

namespace fruityprime::players {

// PlayerAi.AiGlobals: one bot that is walking a path node list looking for the
// first node it has a clear line to.
struct AiGlobals {
    // -1 when the slot is unused.
    int player_slot = -1;
    // How many nodes are left to try before the bot gives up.
    int field4 = 0;
    std::size_t node_data_index = 0;
    // The node positions the bot is scanning, in order.
    std::vector<formats::Vector3> node_data;
};

// What UpdateGlobals needs to know about a player without owning one.
struct AiPlayerView {
    bool active = false;
    bool is_bot = false;
    int health = 0;
    // CameraInfo.Position, which is where a player actually sees from.
    formats::Vector3 camera_position{};
    formats::Vector3 position{};
    bool alt_form = false;
};

// The result of one UpdateGlobals step, so the caller can apply it to the bot
// it belongs to without this module knowing what a bot is.
struct AiGlobalsResult {
    bool ran = false;
    int player_slot = -1;
    // True when a clear line was found: the caller stores `node` as the bot's
    // target and clears its waiting flag.
    bool node_found = false;
    formats::Vector3 node{};
    // True when the bot was taken off the queue this step, whether because it
    // found a node or because it ran out of them.
    bool removed = false;
};

// PlayerAi.AiEntityRefs: seventy-eight slots a bot fills with whatever it has
// noticed -- path nodes, items, node defenses, a door.  The managed class
// names each one Field0..Field77 and answers IsPopulated with a switch over
// all of them; an array says the same thing and cannot fall out of step.
class AiEntityRefs final {
public:
    static constexpr std::size_t SlotCount = 78;

    // PlayerAi.AiEntityRefs.IsPopulated.  An index outside the table reads as
    // unpopulated rather than throwing, matching the managed switch's
    // discard arm.
    [[nodiscard]] bool is_populated(int index) const noexcept {
        if (index < 0 || index >= static_cast<int>(SlotCount)) {
            return false;
        }
        return populated_[static_cast<std::size_t>(index)];
    }

    void set_populated(int index, bool value) noexcept {
        if (index < 0 || index >= static_cast<int>(SlotCount)) {
            return;
        }
        populated_[static_cast<std::size_t>(index)] = value;
    }

    // What a populated slot refers to.  The managed class holds an entity
    // reference; here it is that entity's index in whichever of the
    // session's lists the slot's type names, or -1 for none.  A slot that
    // was never found reads back as -1, which is the null the managed code
    // takes the other branch on.
    [[nodiscard]] std::int32_t entity(int index) const noexcept {
        if (index < 0 || index >= static_cast<int>(SlotCount)) {
            return -1;
        }
        return entity_[static_cast<std::size_t>(index)];
    }

    void set_entity(int index, std::int32_t value) noexcept {
        if (index < 0 || index >= static_cast<int>(SlotCount)) {
            return;
        }
        entity_[static_cast<std::size_t>(index)] = value;
        populated_[static_cast<std::size_t>(index)] = value >= 0;
    }

    void clear() noexcept {
        populated_.fill(false);
        entity_.fill(-1);
    }

    [[nodiscard]] std::size_t populated_count() const noexcept {
        std::size_t count = 0;
        for (bool value : populated_) {
            count += value ? 1u : 0u;
        }
        return count;
    }

private:
    std::array<bool, SlotCount> populated_{};
    std::array<std::int32_t, SlotCount> entity_ = [] {
        std::array<std::int32_t, SlotCount> value{};
        value.fill(-1);
        return value;
    }();
};

// The static state PlayerAi keeps for the whole match.  It is an object rather
// than a set of file-scope variables so a test can hold two matches at once,
// but there is exactly one per session.
class AiGlobalState final {
public:
    // PlayerEntity.SlotCapacity
    static constexpr std::size_t SlotCapacity = 8;

    // A line-of-sight test between two points; true when something is in the
    // way.  Matches CollisionDetection.CheckBetweenPoints' sense.
    using BlockedQuery =
        std::function<bool(formats::Vector3, formats::Vector3)>;

    // PlayerAi.InitializeGlobals
    void initialize_globals() noexcept;

    // PlayerAi.UpdateVisibilityAndGlobals
    AiGlobalsResult update_visibility_and_globals(
        std::span<const AiPlayerView> players, int max_players,
        const BlockedQuery& blocked);

    // PlayerAi's _playerVisibility, which is symmetric by construction.
    [[nodiscard]] bool player_visible(std::size_t a,
                                      std::size_t b) const noexcept {
        if (a >= SlotCapacity || b >= SlotCapacity) {
            return false;
        }
        return visibility_[a][b];
    }

    // PlayerAi.Func214B810's tail: put a bot on the queue, or refresh the
    // entry it already has.
    void add_player_to_globals(int player_slot, int field4,
                               std::vector<formats::Vector3> node_data);
    // PlayerAi.RemovePlayerFromGlobals
    void remove_player_from_globals(int player_slot) noexcept;

    [[nodiscard]] int queued_count() const noexcept { return global_field2_; }
    [[nodiscard]] int queue_cursor() const noexcept { return global_field0_; }
    [[nodiscard]] const AiGlobals& queued(std::size_t index) const noexcept {
        return global_objs_[index];
    }

private:
    void update_visibility(std::span<const AiPlayerView> players,
                           int max_players, const BlockedQuery& blocked);
    AiGlobalsResult update_globals(std::span<const AiPlayerView> players,
                                   const BlockedQuery& blocked);

    int global_field0_ = 0;
    int global_field2_ = 0;
    std::array<AiGlobals, SlotCapacity> global_objs_{};
    std::array<std::array<bool, SlotCapacity>, SlotCapacity> visibility_{};
    std::uint8_t vis_index1_ = 0;
    std::uint8_t vis_index2_ = 0;
};

} // namespace fruityprime::players
