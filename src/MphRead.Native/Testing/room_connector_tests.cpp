#include "../Entities/room_connectors.hpp"
#include "Entities/room_node_refs.hpp"

#include <cassert>
#include <cmath>
#include <iostream>
#include <string>
#include <vector>

namespace {

using fruityprime::formats::Vector3;
using fruityprime::scene::Connector;
using fruityprime::scene::RoomConnectors;
using fruityprime::scene::RoomNode;

[[nodiscard]] bool near_equal(float a, float b, float epsilon = 1e-4F) {
    return std::fabs(a - b) <= epsilon;
}

// The size table is read by index, so its length and its odd entries are what
// a wrong value would show up in as a corridor in the wrong place.
void test_size_table() {
    const auto& sizes = fruityprime::scene::connector_sizes();
    assert(sizes.size() == 27);
    assert(near_equal(sizes[0].x, 10.0F));
    // 0xA60F in 1/4096 units.
    assert(near_equal(sizes[8].x, 42511.0F / 4096.0F));
    // The sloped connector's two halves are not mirror images: one rises and
    // the other falls, by different amounts.
    assert(sizes[20].y > 0.0F);
    assert(sizes[21].y < 0.0F);
    assert(!near_equal(sizes[20].y, -sizes[21].y));
    // Entry 24 is the only 20-unit one.
    assert(near_equal(sizes[24].z, 20.0F));
}

// The offset flips when the door faces broadly +X or +Z, and the two axes are
// tested separately -- a diagonal door flips once, not twice.
void test_size_sign() {
    const Vector3 facing_neg_x{-1.0F, 0.0F, 0.0F};
    const Vector3 facing_pos_x{1.0F, 0.0F, 0.0F};
    assert(near_equal(
        fruityprime::scene::connector_size(0, facing_neg_x).x, 10.0F));
    assert(near_equal(
        fruityprime::scene::connector_size(0, facing_pos_x).x, -10.0F));

    // Just under and just over the 45-degree threshold.
    const float threshold = fruityprime::scene::ConnectorFacingThreshold;
    const Vector3 just_under{threshold - 0.001F, 0.0F, threshold - 0.001F};
    const Vector3 just_over{threshold + 0.001F, 0.0F, threshold + 0.001F};
    assert(fruityprime::scene::connector_size(0, just_under).x > 0.0F);
    assert(fruityprime::scene::connector_size(0, just_over).x < 0.0F);

    // A door facing +Z flips an X-axis connector too, because the test is an
    // or over both axes rather than a choice between them.
    const Vector3 facing_pos_z{0.0F, 0.0F, 1.0F};
    assert(fruityprime::scene::connector_size(0, facing_pos_z).x < 0.0F);

    // An id outside the table is a zero offset rather than a read past it.
    const Vector3 out_of_range =
        fruityprime::scene::connector_size(27, facing_neg_x);
    assert(out_of_range.x == 0.0F && out_of_range.y == 0.0F
           && out_of_range.z == 0.0F);
    const Vector3 negative =
        fruityprime::scene::connector_size(-1, facing_neg_x);
    assert(negative.x == 0.0F);
}

// The collision sits half a size past the door and the loader door a whole
// size past, which is what puts the corridor between the two.
void test_placement() {
    const Vector3 door{5.0F, 1.0F, -2.0F};
    const Vector3 size{10.0F, 0.0F, 0.0F};
    const Vector3 collision =
        fruityprime::scene::connector_collision_translation(door, size);
    assert(near_equal(collision.x, 10.0F));
    assert(near_equal(collision.y, 1.0F));
    const Vector3 loader =
        fruityprime::scene::connector_loader_door_position(door, size);
    assert(near_equal(loader.x, 15.0F));
    assert(near_equal(loader.z, -2.0F));
}

// The connector's own room node is the first one named rm-something; a
// connector with none falls back to rmMain rather than to nothing.
void test_node_name() {
    const std::vector<std::string> with_node{"root", "rmCon01", "rmOther"};
    assert(fruityprime::scene::connector_node_name(with_node) == "rmCon01");
    const std::vector<std::string> without{"root", "geo1"};
    assert(fruityprime::scene::connector_node_name(without) == "rmMain");
    const std::vector<std::string> empty;
    assert(fruityprime::scene::connector_node_name(empty) == "rmMain");
    // "rm" alone counts; the managed test is a prefix, not a length.
    const std::vector<std::string> bare{"rm"};
    assert(fruityprime::scene::connector_node_name(bare) == "rm");
}

// Exactly one connector is live at a time: the corridors overlap in space, so
// a second live one puts two sets of walls in the same place.
void test_exclusive_activation() {
    RoomConnectors connectors;
    const std::vector<std::string> nodes{"rmMain"};
    const std::size_t first = connectors.add_connector(
        0, /*door_entity_id=*/10, {0.0F, 0.0F, 0.0F}, {-1.0F, 0.0F, 0.0F},
        "UNIT1_RM1", nodes, /*in_room_transition=*/false);
    const std::size_t second = connectors.add_connector(
        2, /*door_entity_id=*/11, {0.0F, 0.0F, 0.0F}, {0.0F, 0.0F, -1.0F},
        "UNIT1_RM2", nodes, /*in_room_transition=*/false);
    assert(first == 0 && second == 1);
    assert(connectors.connectors().size() == 2);
    // Nothing is live until a door is walked into.
    assert(connectors.active_connector() == nullptr);
    for (const Connector& connector : connectors.connectors()) {
        assert(!connector.model_active);
        assert(connector.loader_door_inactive);
    }

    assert(connectors.activate_connector(10));
    assert(connectors.connectors()[0].model_active);
    assert(connectors.connectors()[0].collision_active);
    assert(!connectors.connectors()[0].loader_door_inactive);
    assert(!connectors.connectors()[1].model_active);
    assert(connectors.connectors()[1].loader_door_inactive);
    assert(connectors.active_connector() == &connectors.connectors()[0]);

    // Activating the other one puts the first away in the same pass.
    assert(connectors.activate_connector(11));
    assert(!connectors.connectors()[0].model_active);
    assert(connectors.connectors()[0].loader_door_inactive);
    assert(connectors.connectors()[1].model_active);
    assert(connectors.active_connector() == &connectors.connectors()[1]);

    // A door with no connector leaves everything off rather than half on.
    assert(!connectors.activate_connector(99));
    assert(connectors.active_connector() == nullptr);
    for (const Connector& connector : connectors.connectors()) {
        assert(!connector.model_active);
        assert(connector.loader_door_inactive);
    }
}

// A connector added during a room transition belongs to the room being loaded
// rather than the one being left, and is flagged so the two can be told apart.
void test_transition_flag() {
    RoomConnectors connectors;
    const std::vector<std::string> nodes{"rmMain"};
    connectors.add_connector(0, 10, {}, {-1.0F, 0.0F, 0.0F}, "A", nodes,
                             /*in_room_transition=*/false);
    connectors.add_connector(0, 11, {}, {-1.0F, 0.0F, 0.0F}, "B", nodes,
                             /*in_room_transition=*/true);
    assert(connectors.connectors()[0].node_anim_ignore_root);
    assert(!connectors.connectors()[1].node_anim_ignore_root);

    // An id outside the table is refused rather than added with a zero size.
    const std::size_t rejected = connectors.add_connector(
        99, 12, {}, {-1.0F, 0.0F, 0.0F}, "C", nodes, false);
    assert(rejected == static_cast<std::size_t>(-1));
    assert(connectors.connectors().size() == 2);

    connectors.set_loader_door_entity_id(0, 500);
    assert(connectors.connectors()[0].loader_door_entity_id == 500);
    // An index past the end is dropped rather than corrupting a neighbour.
    connectors.set_loader_door_entity_id(9, 600);
    assert(connectors.connectors()[1].loader_door_entity_id == -1);
}

// Cancelling a transition is a flag the loading thread reads; it does not
// switch the connector being walked into off.
void test_cancel_transition() {
    RoomConnectors connectors;
    const std::vector<std::string> nodes{"rmMain"};
    connectors.add_connector(0, 10, {}, {-1.0F, 0.0F, 0.0F}, "A", nodes,
                             false);
    assert(connectors.activate_connector(10));
    assert(!connectors.transition_cancelled());
    connectors.cancel_transition();
    assert(connectors.transition_cancelled());
    assert(connectors.active_connector() != nullptr);
    connectors.reset_transition();
    assert(!connectors.transition_cancelled());
}

// A named node resolves to the room's own model, and a node that is not part
// of a room part is skipped rather than returned with a -1 in it.
void test_node_ref_by_name() {
    const std::vector<RoomNode> nodes{
        RoomNode{"rmMain", 0, 4},
        RoomNode{"rmSide", 1, 7},
        // A node with no room part: the managed code asserts on this, and the
        // lookup here skips it so the caller falls back to rmMain.
        RoomNode{"broken", -1, 9},
        RoomNode{"childless", 2, -1}
    };
    const auto main = fruityprime::scene::get_node_ref_by_name(
        nodes, "UNIT1_RM1", "rmMain");
    assert(main.room_name == "UNIT1_RM1");
    assert(main.part_index == 0);
    assert(main.node_index == 4);
    // Always the room's own model, never a connector's.
    assert(main.model_index == 0);

    const auto side = fruityprime::scene::get_node_ref_by_name(
        nodes, "UNIT1_RM1", "rmSide");
    assert(side.part_index == 1 && side.node_index == 7);

    assert(fruityprime::scene::get_node_ref_by_name(nodes, "UNIT1_RM1",
                                                    "broken")
           == fruityprime::culling::NodeRef::none());
    assert(fruityprime::scene::get_node_ref_by_name(nodes, "UNIT1_RM1",
                                                    "childless")
           == fruityprime::culling::NodeRef::none());
    assert(fruityprime::scene::get_node_ref_by_name(nodes, "UNIT1_RM1",
                                                    "nothing")
           == fruityprime::culling::NodeRef::none());
    // The match is exact, not a prefix.
    assert(fruityprime::scene::get_node_ref_by_name(nodes, "UNIT1_RM1", "rm")
           == fruityprime::culling::NodeRef::none());
}

} // namespace

int main() {
    test_size_table();
    test_size_sign();
    test_placement();
    test_node_name();
    test_exclusive_activation();
    test_transition_flag();
    test_cancel_transition();
    test_node_ref_by_name();
    std::cout << "room connector tests passed\n";
    return 0;
}
