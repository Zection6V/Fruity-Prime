#include "../Entities/Players/pause_map.hpp"

#include <cassert>
#include <cmath>
#include <iostream>
#include <string>
#include <vector>

namespace {

using fruityprime::formats::Vector3;
using fruityprime::players::NavMapRoom;
using fruityprime::players::PauseDrawState;
using fruityprime::players::PauseMapInput;
using fruityprime::players::PauseMapNav;
using fruityprime::players::PauseMenuAction;

constexpr float FrameTime = 1.0F / 30.0F;

[[nodiscard]] bool near_equal(float a, float b, float epsilon = 1e-4F) {
    return std::fabs(a - b) <= epsilon;
}

[[nodiscard]] std::vector<NavMapRoom> two_rooms() {
    std::vector<NavMapRoom> rooms;
    NavMapRoom left;
    left.name = "UNIT1_RM1";
    left.room_position = {0.0F, 0.0F, 0.0F};
    left.center_position = {0.0F, 0.0F, 0.0F};
    left.visited = true;
    rooms.push_back(left);
    NavMapRoom right;
    right.name = "UNIT1_RM2";
    right.room_position = {20.0F, 0.0F, 0.0F};
    right.center_position = {20.0F, 0.0F, 0.0F};
    right.visited = true;
    rooms.push_back(right);
    return rooms;
}

// The static tables are the ones a wrong value shows up in as a wrong colour
// or a symbol in the wrong corner, so they are checked outright.
void test_tables() {
    assert(PauseMapNav::door_colors().size() == 10);
    // Palette 4 is the red locked door, and palette 9 the grey one the game
    // falls back to for a sub-type above seven.
    assert(near_equal(PauseMapNav::door_colors()[4].x, 1.0F));
    assert(near_equal(PauseMapNav::door_colors()[4].y, 0.0F));
    assert(near_equal(PauseMapNav::door_colors()[9].x, 165.0F / 255.0F));

    assert(PauseMapNav::map_node_offsets().size() == 9);
    // Only areas 1, 3 and 5 are nudged; the rest sit where their model does.
    assert(near_equal(PauseMapNav::map_node_offsets()[3].y, 73.0F));
    assert(near_equal(PauseMapNav::map_node_offsets()[0].y, 0.0F));

    assert(PauseMapNav::icon_positions().size() == 8);
    // The first four octoliths run down the left edge, the last four the right.
    assert(near_equal(PauseMapNav::icon_positions()[0].x, 15.0F));
    assert(near_equal(PauseMapNav::icon_positions()[7].x, 241.0F));

    const auto& legend = PauseMapNav::default_legend();
    assert(legend.size() == 10);
    // Six weapon rows start locked; the remaining four never are.
    int locked = 0;
    for (const auto& row : legend) {
        if (row.group == 0) {
            ++locked;
            assert(!row.unlocked);
        } else {
            assert(row.unlocked);
        }
    }
    assert(locked == 6);
    // The last two rows come from the other symbol sheet and are nudged.
    assert(legend[8].other_object && legend[9].other_object);
    assert(legend[8].offset_x == -3 && legend[8].offset_y == -4);
}

// Pausing during a room transition shows the "initializing" message, and the
// map is only built once the load is over.
void test_loading_state() {
    PauseMapNav nav;
    const auto rooms = two_rooms();
    nav.set_up_menu_pause_hud({0.0F, 0.0F, -1.0F}, true, rooms, "UNIT1_RM1");
    assert(nav.nav_loading());
    assert(nav.draw_state() == PauseDrawState::Map);
    assert(!nav.nav_map_model_enabled());

    // Still loading: the timer runs out but the transition has not.
    for (int i = 0; i < 120; ++i) {
        nav.process_pause_menu(FrameTime, true, {0.0F, 0.0F, -1.0F}, rooms,
                               "UNIT1_RM1");
    }
    assert(nav.nav_loading());
    assert(!nav.nav_map_model_enabled());

    // One frame after the transition ends the map is built.
    nav.process_pause_menu(FrameTime, false, {0.0F, 0.0F, -1.0F}, rooms,
                           "UNIT1_RM1");
    assert(!nav.nav_loading());
    assert(nav.nav_map_model_enabled());
    assert(nav.selected_room() == "UNIT1_RM1");
    assert(near_equal(nav.zoom(), 0.75F));
    assert(near_equal(nav.rotation_y(), 28.125F));
}

// A room the map model does not contain leaves the screen on the
// "topographical view unavailable" message rather than an empty map.
void test_unknown_room() {
    PauseMapNav nav;
    const auto rooms = two_rooms();
    nav.set_up_menu_pause_hud({0.0F, 0.0F, -1.0F}, false, rooms, "UNIT9_RM9");
    assert(!nav.nav_loading());
    assert(!nav.nav_map_model_enabled());
    assert(nav.selected_room().empty());
}

// The camera sits 90 units from what it is looking at, whatever the zoom, and
// the zoom only changes how much of the map fits on screen.
void test_matrices() {
    PauseMapNav nav;
    const auto rooms = two_rooms();
    nav.set_up_menu_pause_hud({0.0F, 0.0F, -1.0F}, false, rooms, "UNIT1_RM1");
    const auto [camera_pos, camera_target] = nav.pause_map_look_vectors();
    // The target is lifted so the camera looks at chest height, not the floor.
    assert(near_equal(camera_target.y, 3.75F));
    const float dx = camera_pos.x - camera_target.x;
    const float dy = camera_pos.y - camera_target.y;
    const float dz = camera_pos.z - camera_target.z;
    assert(near_equal(std::sqrt(dx * dx + dy * dy + dz * dz), 90.0F, 1e-3F));

    const auto [view, ortho] = nav.pause_map_matrices();
    // CreateOrthographic(256 * zoom, 192 * zoom, ...) at the default 0.75.
    assert(near_equal(ortho.m11, 2.0F / (256.0F * 0.75F)));
    assert(near_equal(ortho.m22, 2.0F / (192.0F * 0.75F)));
    // Facing -Z with no yaw puts the camera's right axis on +X, which is what
    // makes panning right move the map in +X.
    assert(near_equal(view.m11, 1.0F, 1e-3F));
    assert(near_equal(view.m21, 0.0F, 1e-3F));
    assert(near_equal(view.m31, 0.0F, 1e-3F));
}

// Zoom clamps at both ends, yaw wraps rather than clamping, and pitch clamps.
void test_zoom_and_rotation() {
    PauseMapNav nav;
    const auto rooms = two_rooms();
    nav.set_up_menu_pause_hud({0.0F, 0.0F, -1.0F}, false, rooms, "UNIT1_RM1");

    PauseMapInput input;
    input.scroll_up = true;
    for (int i = 0; i < 200; ++i) {
        static_cast<void>(nav.process_pause_menu_input(input, FrameTime,
                                                       rooms));
    }
    assert(near_equal(nav.zoom(), PauseMapNav::MinZoom));

    input.scroll_up = false;
    input.scroll_down = true;
    for (int i = 0; i < 200; ++i) {
        static_cast<void>(nav.process_pause_menu_input(input, FrameTime,
                                                       rooms));
    }
    assert(near_equal(nav.zoom(), PauseMapNav::MaxZoom));

    input.scroll_down = false;
    input.aim_left = true;
    // 360 degrees at 1.40625 per frame is 256 frames; one more wraps.
    for (int i = 0; i < 300; ++i) {
        static_cast<void>(nav.process_pause_menu_input(input, FrameTime,
                                                       rooms));
    }
    assert(nav.rotation_x() > -360.0F && nav.rotation_x() < 360.0F);

    input.aim_left = false;
    input.aim_up = true;
    for (int i = 0; i < 300; ++i) {
        static_cast<void>(nav.process_pause_menu_input(input, FrameTime,
                                                       rooms));
    }
    assert(near_equal(nav.rotation_y(), -PauseMapNav::PitchLimit));
}

// Panning moves a free offset that decays back to exactly zero once the stick
// is let go -- exactly, because the snap below 1/4096 is what stops the map
// drifting for the rest of the pause.
void test_pan_and_decay() {
    PauseMapNav nav;
    const auto rooms = two_rooms();
    nav.set_up_menu_pause_hud({0.0F, 0.0F, -1.0F}, false, rooms, "UNIT1_RM1");

    PauseMapInput input;
    input.move_right = true;
    static_cast<void>(nav.process_pause_menu_input(input, FrameTime, rooms));
    // The camera's right axis is +X here, so one frame of panning is one
    // PanSpeed step along X and nothing along the others.
    assert(near_equal(nav.pan_offset().x, PauseMapNav::PanSpeed, 1e-3F));
    assert(near_equal(nav.pan_offset().y, 0.0F));

    input.move_right = false;
    bool settled = false;
    for (int i = 0; i < 600 && !settled; ++i) {
        static_cast<void>(nav.process_pause_menu_input(input, FrameTime,
                                                       rooms));
        settled = nav.pan_offset().x == 0.0F;
    }
    assert(settled);
    assert(nav.pan_offset().y == 0.0F && nav.pan_offset().z == 0.0F);

    // With centering disabled the offset is left where the player put it.
    static_cast<void>(nav.process_pause_menu_input(
        [] { PauseMapInput held; held.move_right = true; return held; }(),
        FrameTime, rooms));
    const float held_x = nav.pan_offset().x;
    assert(held_x > 0.0F);
    PauseMapInput no_centering;
    no_centering.no_map_centering = true;
    for (int i = 0; i < 100; ++i) {
        static_cast<void>(nav.process_pause_menu_input(no_centering, FrameTime,
                                                       rooms));
    }
    assert(near_equal(nav.pan_offset().x, held_x));
}

// Panning far enough re-selects the room now nearest the middle of the screen,
// and rebases the pan offset so the camera does not jump when it does.
void test_room_reselection() {
    PauseMapNav nav;
    const auto rooms = two_rooms();
    nav.set_up_menu_pause_hud({0.0F, 0.0F, -1.0F}, false, rooms, "UNIT1_RM1");
    assert(nav.selected_room() == "UNIT1_RM1");

    PauseMapInput input;
    input.move_right = true;
    bool switched = false;
    for (int i = 0; i < 40 && !switched; ++i) {
        static_cast<void>(nav.process_pause_menu_input(input, FrameTime,
                                                       rooms));
        switched = nav.selected_room() == "UNIT1_RM2";
    }
    assert(switched);
    // The centre the camera looks at moved to the new room; the pan offset
    // absorbed the difference, so the target it produces is unchanged.
    assert(near_equal(nav.current_center_position().x, 20.0F));
    const auto [camera_pos, camera_target] = nav.pause_map_look_vectors();
    static_cast<void>(camera_pos);
    assert(camera_target.x < 20.0F);
    // Re-selection restarts the location-name scroll.
    assert(nav.nav_text_timer() == 0.0F);
}

// An unvisited room is not selectable, however close to the centre it is.
void test_unvisited_not_selectable() {
    PauseMapNav nav;
    auto rooms = two_rooms();
    rooms[1].visited = false;
    nav.set_up_menu_pause_hud({0.0F, 0.0F, -1.0F}, false, rooms, "UNIT1_RM1");
    PauseMapInput input;
    input.move_right = true;
    for (int i = 0; i < 40; ++i) {
        static_cast<void>(nav.process_pause_menu_input(input, FrameTime,
                                                       rooms));
    }
    assert(nav.selected_room() == "UNIT1_RM1");
}

// The quit prompt takes the map's input while it is up, and answering "no"
// puts the map back rather than leaving the screen on nothing.
void test_quit_flow() {
    PauseMapNav nav;
    const auto rooms = two_rooms();
    nav.set_up_menu_pause_hud({0.0F, 0.0F, -1.0F}, false, rooms, "UNIT1_RM1");

    PauseMapInput quit;
    quit.quit_pressed = true;
    assert(nav.process_pause_menu_input(quit, FrameTime, rooms)
           == PauseMenuAction::OpenQuitPrompt);
    assert(nav.draw_state() == PauseDrawState::QuitPrompt);

    // The map does not move while the prompt is up.
    PauseMapInput pan;
    pan.move_right = true;
    static_cast<void>(nav.process_pause_menu_input(pan, FrameTime, rooms));
    assert(nav.pan_offset().x == 0.0F);

    PauseMapInput no;
    no.no_pressed = true;
    assert(nav.process_pause_menu_input(no, FrameTime, rooms)
           == PauseMenuAction::CancelQuit);
    assert(nav.draw_state() == PauseDrawState::Map);

    static_cast<void>(nav.process_pause_menu_input(quit, FrameTime, rooms));
    PauseMapInput yes;
    yes.yes_pressed = true;
    assert(nav.process_pause_menu_input(yes, FrameTime, rooms)
           == PauseMenuAction::ConfirmQuit);
    assert(nav.draw_state() == PauseDrawState::Hidden);

    // Once quitting is under way nothing else is accepted, so a second
    // confirmation cannot start a second fade.
    PauseMapInput prevented;
    prevented.pause_prevented = true;
    prevented.quit_pressed = true;
    assert(nav.process_pause_menu_input(prevented, FrameTime, rooms)
           == PauseMenuAction::None);
}

// The Fan Rooms' map geometry is stretched, so the player marker is stretched
// with it; everywhere else the marker sits where the player does.
void test_marker_position() {
    assert(near_equal(PauseMapNav::marker_height_factor("UNIT2_C2"), 2.05F));
    assert(near_equal(PauseMapNav::marker_height_factor("UNIT2_C3"), 1.305F));
    assert(near_equal(PauseMapNav::marker_height_factor("UNIT1_RM1"), 1.0F));

    PauseMapNav nav;
    std::vector<NavMapRoom> rooms;
    NavMapRoom room;
    room.name = "UNIT2_C2";
    room.room_position = {100.0F, 5.0F, -30.0F};
    room.center_position = {100.0F, 5.0F, -30.0F};
    room.visited = true;
    rooms.push_back(room);
    nav.set_up_menu_pause_hud({0.0F, 0.0F, -1.0F}, false, rooms, "UNIT2_C2");

    const Vector3 marker = nav.player_marker_position(
        {2.0F, 4.0F, 1.0F}, {1.0F, 0.0F, -1.0F}, "UNIT2_C2");
    assert(near_equal(marker.x, 2.0F + 100.0F - 1.0F));
    assert(near_equal(marker.y, 4.0F * 2.05F + 5.0F - 0.0F + 1.0F));
    assert(near_equal(marker.z, 1.0F + -30.0F - -1.0F));
}

// The map model transform is the model's own scale plus the per-area nudge.
void test_map_model_transform() {
    const auto identity_area = PauseMapNav::map_model_transform(2.0F, 0);
    assert(near_equal(identity_area.m11, 2.0F));
    assert(near_equal(identity_area.m41, 0.0F));
    const auto nudged = PauseMapNav::map_model_transform(2.0F, 3);
    assert(near_equal(nudged.m42, 73.0F));
    // An area outside the table leaves the transform unshifted rather than
    // reading past the end of it.
    const auto out_of_range = PauseMapNav::map_model_transform(2.0F, 99);
    assert(near_equal(out_of_range.m41, 0.0F));
    assert(near_equal(out_of_range.m42, 0.0F));
}

// Doors are only drawn for a selected room, and never for a connector.
void test_door_drawing_rule() {
    PauseMapNav nav;
    std::vector<NavMapRoom> rooms;
    NavMapRoom connector;
    connector.name = "Con01";
    connector.visited = true;
    rooms.push_back(connector);
    assert(!nav.draw_room_doors());
    nav.set_up_menu_pause_hud({0.0F, 0.0F, -1.0F}, false, rooms, "Con01");
    assert(nav.nav_map_model_enabled());
    assert(!nav.draw_room_doors());

    const auto normal = two_rooms();
    nav.set_up_menu_pause_hud({0.0F, 0.0F, -1.0F}, false, normal, "UNIT1_RM1");
    assert(nav.draw_room_doors());

    // Closing the screen forgets the selection, so reopening does not draw
    // doors for a room the player has since left.
    nav.end_menu_pause_hud();
    assert(!nav.draw_room_doors());
    assert(!nav.nav_map_model_enabled());
}

// The foreground lays out one icon per octolith slot, and the quit button is
// drawn on the map but replaced by the prompt once quitting is asked about.
void test_foreground_layout() {
    PauseMapNav nav;
    const auto rooms = two_rooms();
    nav.set_up_menu_pause_hud({0.0F, 0.0F, -1.0F}, false, rooms, "UNIT1_RM1");

    fruityprime::game::StorySave save;
    // Every area unlocked, no octoliths held and no hunter holding one, which
    // is the state that draws a teleporter symbol in every slot.
    save.areas = 0xffffu;
    save.current_octoliths = 0;
    save.lost_octoliths = 0xffffffffU;
    save.artifacts = 0;

    const auto draws = nav.draw_pause_menu_foreground(
        save, {false, false, false, false, false, false}, false, "Celestial",
        16, 16, 16, 16, 8, 8, 32, 16);
    int teleporters = 0;
    int quit_buttons = 0;
    for (const auto& draw : draws.objects) {
        if (draw.object
            == fruityprime::players::PauseHudDraw::Object::Teleporter) {
            ++teleporters;
        } else if (draw.object
                   == fruityprime::players::PauseHudDraw::Object::Quit) {
            ++quit_buttons;
        }
    }
    assert(teleporters == 8);
    assert(quit_buttons == 1);

    // One artifact found lights one dot; all three switch the portal symbol.
    save.artifacts = 0x7u;
    const auto with_artifacts = nav.draw_pause_menu_foreground(
        save, {false, false, false, false, false, false}, false, "Celestial",
        16, 16, 16, 16, 8, 8, 32, 16);
    int dots = 0;
    int open_portals = 0;
    for (const auto& draw : with_artifacts.objects) {
        if (draw.object
            == fruityprime::players::PauseHudDraw::Object::ArtifactDot) {
            ++dots;
        } else if (draw.object
                       == fruityprime::players::PauseHudDraw::Object::Teleporter
                   && draw.image_index == 1) {
            ++open_portals;
        }
    }
    assert(dots == 3);
    assert(open_portals == 1);

    // A hunter holding an octolith draws their portrait and the lost-octolith
    // icon in place of the player's own.
    // Nibble 0 names hunter 3; every other nibble stays 15, which is what
    // "nobody took this one" looks like.
    save.lost_octoliths = 0xfffffff3U;
    const auto with_lost = nav.draw_pause_menu_foreground(
        save, {false, false, false, false, false, false}, false, "Celestial",
        16, 16, 16, 16, 8, 8, 32, 16);
    int portraits = 0;
    int lost = 0;
    for (const auto& draw : with_lost.objects) {
        if (draw.object
            == fruityprime::players::PauseHudDraw::Object::HunterPortrait) {
            ++portraits;
            assert(draw.instance == 3);
        } else if (draw.object
                   == fruityprime::players::PauseHudDraw::Object::LostOctolith) {
            ++lost;
        }
    }
    assert(portraits == 1 && lost == 1);

    // The legend only appears while the overlay button is held.
    const auto with_legend = nav.draw_pause_menu_foreground(
        save, {true, false, true, false, false, false}, true, "Celestial",
        16, 16, 16, 16, 8, 8, 32, 16);
    int legend_rows = 0;
    for (const auto& draw : with_legend.objects) {
        if (draw.object
                == fruityprime::players::PauseHudDraw::Object::LegendSymbol
            || draw.object
                == fruityprime::players::PauseHudDraw::Object::LegendOther) {
            ++legend_rows;
        }
    }
    assert(legend_rows == 10);
    int locked_rows = 0;
    int named_weapons = 0;
    for (const auto& text : with_legend.text) {
        if (text.source
            == fruityprime::players::PauseTextDraw::Source::LegendLocked) {
            ++locked_rows;
        } else if (text.source
                   == fruityprime::players::PauseTextDraw::Source::LegendWeapon) {
            ++named_weapons;
        }
    }
    // Two of the six weapons are held, so four rows still read "???".
    assert(named_weapons == 2 && locked_rows == 4);

    // The prompt replaces the quit button rather than sitting beside it.
    nav.set_draw_state(PauseDrawState::QuitPrompt);
    const auto prompting = nav.draw_pause_menu_foreground(
        save, {false, false, false, false, false, false}, false, "Celestial",
        16, 16, 16, 16, 8, 8, 32, 16);
    bool has_prompt = false;
    for (const auto& text : prompting.text) {
        if (text.source
            == fruityprime::players::PauseTextDraw::Source::QuitPrompt) {
            has_prompt = true;
        }
        assert(text.source
               != fruityprime::players::PauseTextDraw::Source::QuitLabel);
    }
    assert(has_prompt);
}

// The teleporter symbols drawn under the map are skipped entirely when the map
// itself is not being drawn.
void test_background_symbols() {
    PauseMapNav nav;
    const auto rooms = two_rooms();
    nav.set_up_menu_pause_hud({0.0F, 0.0F, -1.0F}, false, rooms, "UNIT1_RM1");

    std::vector<PauseMapNav::TeleporterSymbol> symbols;
    symbols.push_back({{0.0F, 0.0F, 0.0F}, 0});
    // Far outside the 192-unit-wide ortho view, so it projects off screen.
    symbols.push_back({{5000.0F, 0.0F, 0.0F}, 1});

    const auto visible = nav.draw_pause_menu_background(symbols, false);
    assert(visible.size() == 1);
    assert(visible[0].image_index == 0);

    // Holding the overlay button hides them, and so does the quit prompt.
    assert(nav.draw_pause_menu_background(symbols, true).empty());
    nav.set_draw_state(PauseDrawState::QuitPrompt);
    assert(nav.draw_pause_menu_background(symbols, false).empty());
}

// A connector node is looked up by its index, not against the room table.
void test_check_room_visited() {
    fruityprime::game::StorySave save;
    save.visited_rooms.fill(0);
    save.visited_connectors.fill(0);
    assert(!PauseMapNav::check_room_visited("Con01", 0, save));
    save.set_visited_connector(0, 0);
    assert(PauseMapNav::check_room_visited("Con01", 0, save));
    // The same connector in another area is a different bit.
    assert(!PauseMapNav::check_room_visited("Con01", 2, save));
    // "Con" with no number is not a connector and matches no room either.
    assert(!PauseMapNav::check_room_visited("Con", 0, save));
    assert(!PauseMapNav::check_room_visited("NotARoom", 0, save));
}

} // namespace

int main() {
    test_tables();
    test_loading_state();
    test_unknown_room();
    test_matrices();
    test_zoom_and_rotation();
    test_pan_and_decay();
    test_room_reselection();
    test_unvisited_not_selectable();
    test_quit_flow();
    test_marker_position();
    test_map_model_transform();
    test_door_drawing_rule();
    test_foreground_layout();
    test_background_symbols();
    test_check_room_visited();
    std::cout << "pause map tests passed\n";
    return 0;
}
