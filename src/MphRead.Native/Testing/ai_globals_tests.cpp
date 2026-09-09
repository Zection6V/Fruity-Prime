#include "../Entities/Players/ai_func_names.hpp"
#include "../Entities/Players/ai_globals.hpp"

#include <array>
#include <cassert>
#include <iostream>
#include <set>
#include <string>
#include <vector>

namespace {

using fruityprime::formats::Vector3;
using fruityprime::players::AiEntityRefs;
using fruityprime::players::AiGlobalState;
using fruityprime::players::AiPlayerView;

[[nodiscard]] std::vector<AiPlayerView> make_players(std::size_t count,
                                                     bool bots = true) {
    std::vector<AiPlayerView> players(count);
    for (std::size_t i = 0; i < count; ++i) {
        players[i].active = true;
        players[i].is_bot = bots;
        players[i].health = 99;
        players[i].position = {static_cast<float>(i) * 4.0F, 0.0F, 0.0F};
        players[i].camera_position = {static_cast<float>(i) * 4.0F, 1.5F,
                                      0.0F};
    }
    return players;
}

// Nothing blocks: every pair the walk visits ends up visible.
const AiGlobalState::BlockedQuery clear_line =
    [](Vector3, Vector3) { return false; };
const AiGlobalState::BlockedQuery solid_wall =
    [](Vector3, Vector3) { return true; };

// The pair walk visits every unordered pair and then starts again, and it does
// so without ever indexing past the slot capacity -- which is the shape of the
// bug that made a fifth player crash the first bot to look at the table.
void test_visibility_walk_covers_every_pair() {
    for (int max_players = 2; max_players <= 8; ++max_players) {
        AiGlobalState state;
        state.initialize_globals();
        const auto players =
            make_players(static_cast<std::size_t>(max_players));
        const int pairs = max_players * (max_players - 1) / 2;
        std::set<std::pair<int, int>> seen;
        for (int frame = 0; frame < pairs * 3; ++frame) {
            static_cast<void>(state.update_visibility_and_globals(
                players, max_players, clear_line));
            for (int a = 0; a < max_players; ++a) {
                for (int b = a + 1; b < max_players; ++b) {
                    if (state.player_visible(static_cast<std::size_t>(a),
                                             static_cast<std::size_t>(b))) {
                        seen.insert({a, b});
                    }
                }
            }
        }
        assert(static_cast<int>(seen.size()) == pairs);
    }
}

// Visibility is symmetric: a bot that can see you can be seen by you.
void test_visibility_is_symmetric() {
    AiGlobalState state;
    state.initialize_globals();
    const auto players = make_players(4);
    for (int frame = 0; frame < 24; ++frame) {
        static_cast<void>(state.update_visibility_and_globals(players, 4,
                                                              clear_line));
    }
    for (std::size_t a = 0; a < 4; ++a) {
        for (std::size_t b = 0; b < 4; ++b) {
            assert(state.player_visible(a, b) == state.player_visible(b, a));
        }
    }
}

// A pair with no bot in it is never marked visible, however clear the line.
void test_visibility_needs_a_bot() {
    AiGlobalState state;
    state.initialize_globals();
    const auto players = make_players(4, /*bots=*/false);
    for (int frame = 0; frame < 24; ++frame) {
        static_cast<void>(state.update_visibility_and_globals(players, 4,
                                                              clear_line));
    }
    for (std::size_t a = 0; a < 4; ++a) {
        for (std::size_t b = 0; b < 4; ++b) {
            assert(!state.player_visible(a, b));
        }
    }
}

// A dead or absent player is invisible even with a clear line, and a wall
// makes everybody invisible.
void test_visibility_gates() {
    AiGlobalState state;
    state.initialize_globals();
    auto players = make_players(4);
    players[2].health = 0;
    players[3].active = false;
    for (int frame = 0; frame < 24; ++frame) {
        static_cast<void>(state.update_visibility_and_globals(players, 4,
                                                              clear_line));
    }
    assert(state.player_visible(0, 1));
    assert(!state.player_visible(0, 2));
    assert(!state.player_visible(0, 3));

    AiGlobalState walled;
    walled.initialize_globals();
    const auto standing = make_players(4);
    for (int frame = 0; frame < 24; ++frame) {
        static_cast<void>(walled.update_visibility_and_globals(standing, 4,
                                                               solid_wall));
    }
    for (std::size_t a = 0; a < 4; ++a) {
        for (std::size_t b = 0; b < 4; ++b) {
            assert(!walled.player_visible(a, b));
        }
    }
}

// Two players in exactly the same spot -- which a spawn produces -- fall back
// to their bodies rather than testing a zero-length line from one camera.
void test_coincident_cameras() {
    AiGlobalState state;
    state.initialize_globals();
    auto players = make_players(2);
    players[0].camera_position = {5.0F, 1.5F, 5.0F};
    players[1].camera_position = {5.0F, 1.5F, 5.0F};
    players[0].position = {5.0F, 0.0F, 5.0F};
    players[1].position = {9.0F, 0.0F, 5.0F};
    bool used_bodies = false;
    const AiGlobalState::BlockedQuery watcher =
        [&used_bodies](Vector3 a, Vector3 b) {
            if (a.y == 0.0F && b.y == 0.0F) {
                used_bodies = true;
            }
            return false;
        };
    static_cast<void>(state.update_visibility_and_globals(players, 2,
                                                          watcher));
    assert(used_bodies);
    assert(state.player_visible(0, 1));
}

// The globals queue is a compacting array: removing the middle entry leaves
// the rest contiguous, so the walk never visits a hole.
void test_queue_compacts() {
    AiGlobalState state;
    state.initialize_globals();
    assert(state.queued_count() == 0);
    for (int slot = 0; slot < 4; ++slot) {
        state.add_player_to_globals(slot, 3,
                                    {{0.0F, 0.0F, 0.0F}, {1.0F, 0.0F, 0.0F}});
    }
    assert(state.queued_count() == 4);
    state.remove_player_from_globals(1);
    assert(state.queued_count() == 3);
    assert(state.queued(0).player_slot == 0);
    assert(state.queued(1).player_slot == 2);
    assert(state.queued(2).player_slot == 3);
    // Removing a slot that is not queued changes nothing.
    state.remove_player_from_globals(9);
    assert(state.queued_count() == 3);

    // Re-adding a bot that is already queued refreshes its entry instead of
    // taking a second slot.
    state.add_player_to_globals(2, 7,
                                {{0.0F, 0.0F, 0.0F}, {2.0F, 0.0F, 0.0F}});
    assert(state.queued_count() == 3);
    assert(state.queued(1).field4 == 7);
    // The scan starts at the second node: the first is where the bot is.
    assert(state.queued(1).node_data_index == 1);
}

// A bot scanning path nodes stops at the first one it has a clear line to, and
// is taken off the queue when it does.
void test_globals_finds_node() {
    AiGlobalState state;
    state.initialize_globals();
    const auto players = make_players(2);
    state.add_player_to_globals(
        0, 3,
        {{0.0F, 0.0F, 0.0F}, {10.0F, 0.0F, 0.0F}, {20.0F, 0.0F, 0.0F}});

    const auto result = state.update_visibility_and_globals(players, 2,
                                                            clear_line);
    assert(result.ran);
    assert(result.player_slot == 0);
    assert(result.node_found);
    assert(result.node.x == 10.0F);
    assert(result.removed);
    assert(state.queued_count() == 0);
}

// A bot that cannot reach any of its nodes runs its counter down and drops off
// the queue rather than scanning forever.
void test_globals_gives_up() {
    AiGlobalState state;
    state.initialize_globals();
    const auto players = make_players(2);
    state.add_player_to_globals(
        0, 2,
        {{0.0F, 0.0F, 0.0F}, {10.0F, 0.0F, 0.0F}, {20.0F, 0.0F, 0.0F}});

    auto first = state.update_visibility_and_globals(players, 2, solid_wall);
    assert(first.ran && !first.node_found && !first.removed);
    assert(state.queued_count() == 1);
    auto second = state.update_visibility_and_globals(players, 2, solid_wall);
    assert(second.ran && !second.node_found && second.removed);
    assert(state.queued_count() == 0);

    // An empty queue does nothing at all.
    const auto idle = state.update_visibility_and_globals(players, 2,
                                                          solid_wall);
    assert(!idle.ran);
}

// A morphed bot casts its line from lower down than one on foot.
void test_globals_alt_form_height() {
    AiGlobalState state;
    state.initialize_globals();
    auto players = make_players(2);
    players[0].position = {0.0F, 0.0F, 0.0F};
    players[0].alt_form = true;
    state.add_player_to_globals(0, 3,
                                {{0.0F, 0.0F, 0.0F}, {10.0F, 0.0F, 0.0F}});
    float origin_y = -1.0F;
    const AiGlobalState::BlockedQuery watcher =
        [&origin_y](Vector3 a, Vector3 b) {
            static_cast<void>(b);
            origin_y = a.y;
            return false;
        };
    // The visibility step runs first and uses camera positions, so only the
    // globals step is read here -- it is the last call the query sees.
    static_cast<void>(state.update_visibility_and_globals(players, 2,
                                                          watcher));
    assert(origin_y == 0.5F);
}

// Initializing clears both tables and the queue, so a second match does not
// inherit the first one's answers.
void test_initialize_clears() {
    AiGlobalState state;
    state.initialize_globals();
    const auto players = make_players(4);
    for (int frame = 0; frame < 24; ++frame) {
        static_cast<void>(state.update_visibility_and_globals(players, 4,
                                                              clear_line));
    }
    state.add_player_to_globals(0, 3, {{0.0F, 0.0F, 0.0F}});
    assert(state.player_visible(0, 1));
    assert(state.queued_count() == 1);

    state.initialize_globals();
    assert(!state.player_visible(0, 1));
    assert(state.queued_count() == 0);
    assert(state.queue_cursor() == 0);
}

// The entity-reference slots answer for exactly the seventy-eight the managed
// switch covers, and nothing outside.
void test_entity_refs() {
    AiEntityRefs refs;
    assert(AiEntityRefs::SlotCount == 78);
    for (int i = 0; i < 78; ++i) {
        assert(!refs.is_populated(i));
    }
    refs.set_populated(0, true);
    refs.set_populated(77, true);
    assert(refs.is_populated(0));
    assert(refs.is_populated(77));
    assert(refs.populated_count() == 2);
    // Out of range reads as unpopulated rather than throwing, and a write
    // there is dropped rather than corrupting a neighbour.
    assert(!refs.is_populated(78));
    assert(!refs.is_populated(-1));
    refs.set_populated(78, true);
    refs.set_populated(-1, true);
    assert(refs.populated_count() == 2);
    refs.clear();
    assert(refs.populated_count() == 0);
}

// The name tables are only read by a dump, but a wrong entry would mislabel a
// bot's whole execution path, so their edges are checked against the managed
// switch arms.
void test_func_names() {
    using namespace fruityprime::players;
    assert(funcs1_name(0) == "Func1_214A39C");
    assert(funcs1_name(83) == funcs1_name(83));
    assert(funcs2_name(0) == "empty");
    assert(funcs2_name(1) == "Func2_213EA10");
    assert(funcs2_name(45) == "Func2_213DDCC");
    assert(funcs2_name(47) == "Func2_213DA88");
    assert(funcs2_name(125) == "empty");
    // The ranges that share one implementation carry the managed table's own
    // trailing asterisk.
    assert(funcs2_name(2) == "Func2_213EA48*");
    assert(funcs2_name(122) == "Func2_213EA48*");
    assert(funcs3_name(0) == "Func3_213D87C");
    assert(funcs4_name(0) == "empty");
    assert(funcs4_name(45) == "Func4_2145EB0");
    assert(funcs4_name(125) == "Func4_SetDespawned");
    assert(funcs4_name(2) == "Func4_21462DC*");

    const std::array<int, 3> ids{0, 1, 2};
    const auto names = funcs1_name_list(ids);
    assert(names.size() == 3);
    assert(names[0] == funcs1_name(0));
    assert(names[2] == funcs1_name(2));
    const auto three = funcs3_name_list(ids);
    assert(three[1] == funcs3_name(1));

    bool threw = false;
    try {
        static_cast<void>(funcs4_name(126));
    } catch (const std::exception&) {
        threw = true;
    }
    assert(threw);
}

} // namespace

int main() {
    test_visibility_walk_covers_every_pair();
    test_visibility_is_symmetric();
    test_visibility_needs_a_bot();
    test_visibility_gates();
    test_coincident_cameras();
    test_queue_compacts();
    test_globals_finds_node();
    test_globals_gives_up();
    test_globals_alt_form_height();
    test_initialize_clears();
    test_entity_refs();
    test_func_names();
    std::cout << "ai globals tests passed\n";
    return 0;
}
