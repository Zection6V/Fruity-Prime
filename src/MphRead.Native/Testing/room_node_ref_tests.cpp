// RoomEntity's node-reference lookups.
//
// The case worth pinning is the one the project's notes call out: a position
// no room part contains must report None rather than the caller's previous
// answer, and an entity with no part must be inaudible-safe (audible) but
// not drawn (invisible).  Getting that asymmetry backwards is what lets a
// player be shot from somewhere they cannot be seen.
#include "Entities/room_node_refs.hpp"
#include <cstdio>
int main() {
    using namespace fruityprime;
    using namespace fruityprime::scene;
    RoomPortals room;
    // One portal at x = 0, facing +x. Part 0 is on the +x side, part 1 on -x.
    Portal p;
    p.name = "door";
    p.plane = {1.0F, 0.0F, 0.0F, 0.0F};
    p.node_ref1 = culling::NodeRef{"room", 0, 0, 0};
    p.node_ref2 = culling::NodeRef{"room", 1, 0, 0};
    room.portals.push_back(p);
    room.portal_sides = {{{&room.portals[0], false}},
                         {{&room.portals[0], true}}};
    room.active_parts = {true, false};
    room.audible_parts = {true, false};

    const auto a = get_node_ref_by_position(room, {5.0F, 0.0F, 0.0F});
    const auto b = get_node_ref_by_position(room, {-5.0F, 0.0F, 0.0F});
    // A position no part contains must report None, not the last answer.
    RoomPortals empty;
    const auto none = get_node_ref_by_position(empty, {0.0F, 0.0F, 0.0F});

    bool crossed = false;
    const auto moved = update_node_ref(room, culling::NodeRef{"room", 0, 0, 0},
                                       {5.0F, 0, 0}, {-5.0F, 0, 0}, &crossed);
    std::printf("native room node refs: a=%d b=%d none=%d crossed=%d moved=%d "
                "audible(none)=%d visible(none)=%d visible0=%d visible1=%d\n",
        a.part_index, b.part_index, none.part_index, crossed, moved.part_index,
        is_node_ref_audible(room, culling::NodeRef::none()),
        is_node_ref_visible(room, culling::NodeRef::none()),
        is_node_ref_visible(room, culling::NodeRef{"room", 0, 0, 0}),
        is_node_ref_visible(room, culling::NodeRef{"room", 1, 0, 0}));
    return (a.part_index == 0 && b.part_index == 1 && none.part_index == -1
            && crossed && moved.part_index == 1
            && is_node_ref_audible(room, culling::NodeRef::none())
            && !is_node_ref_visible(room, culling::NodeRef::none())
            && is_node_ref_visible(room, culling::NodeRef{"room", 0, 0, 0})
            && !is_node_ref_visible(room, culling::NodeRef{"room", 1, 0, 0}))
        ? 0 : 1;
}
