#pragma once

// Native counterpart of src/MphRead/Entities/Players/PlayerPause.cs.
//
// The story-mode pause screen is a 3D map you fly around, not a menu: the
// camera orbits a selected room node, panning moves a free offset that decays
// back toward the room's centre, and pushing the stick far enough re-selects
// whichever room's "cent" node is now nearest the middle of the screen.  All
// of that is arithmetic, so it lives here rather than in the renderer, and the
// host is left with two lists of draw commands to execute.
//
// The three pieces that are easy to get wrong and are therefore reproduced
// exactly:
//
//   * the pan offset decays with the same exponential-decay curve the rest of
//     the engine uses, and is snapped to zero below 1/4096 -- without the snap
//     it never reaches the room centre and the map drifts forever;
//   * room re-selection projects each candidate through the *pause* view and
//     ortho matrices, and upstream converts the already-converted screen
//     coordinate a second time before its on-screen test.  That double
//     conversion is kept, because it is what decides which rooms are
//     selectable in the retail game;
//   * the quit prompt is a separate draw state that swallows the map's own
//     input, so a player answering "no" lands back on the map rather than on
//     nothing.

#include "GameState.hpp"
#include "Formats/Types.hpp"

#include <array>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace fruityprime::players {

// PlayerPause._drawPauseState
enum class PauseDrawState : std::uint8_t {
    Hidden = 0,
    Map = 1,
    QuitPrompt = 2,
};

// What ProcessPauseMenuInput decided the frame should do.  Quitting is not
// performed here: the caller owns the music fade and the scene fade.
enum class PauseMenuAction : std::uint8_t {
    None,
    OpenQuitPrompt,
    CancelQuit,
    ConfirmQuit,
};

// One room on the nav map, as the caller found it in the map model: a
// top-level node whose name is the room key, and the "cent" child that the
// camera actually looks at.
struct NavMapRoom {
    std::string name;
    // roomNode.Animation translation.
    formats::Vector3 room_position{};
    // The "cent" child's translation, or the room's own when it has none.
    formats::Vector3 center_position{};
    // CheckRoomVisited for this node.
    bool visited = false;
    // Which of the two areas the node came from, so the caller can find it
    // again in its own model list.
    int area = 0;
    // The node's index in that model.
    int node_index = -1;
};

// PlayerPause's read of the controls for one frame.
struct PauseMapInput {
    bool scroll_up = false;
    bool scroll_down = false;
    bool aim_left = false;
    bool aim_right = false;
    bool aim_up = false;
    bool aim_down = false;
    bool move_left = false;
    bool move_right = false;
    bool move_up = false;
    bool move_down = false;
    bool mouse_left_down = false;
    float mouse_delta_x = 0.0F;
    float mouse_delta_y = 0.0F;
    bool quit_pressed = false;
    bool yes_pressed = false;
    bool no_pressed = false;
    // GameState.PausePrevented: input is ignored entirely once quitting has
    // been confirmed, so a second confirmation cannot start a second fade.
    bool pause_prevented = false;
    // Features.NoMapCentering
    bool no_map_centering = false;
};

// A 2D HUD object the pause screen wants drawn, in the managed screen units
// (256x192) the layout is written in.  `image_index` is the frame within the
// object, or -1 to leave whatever it already shows.
struct PauseHudDraw {
    enum class Object : std::uint8_t {
        HunterPortrait,
        Octolith,
        LostOctolith,
        Teleporter,
        ArtifactDot,
        LegendSymbol,
        LegendOther,
        Quit,
    };

    Object object = Object::Octolith;
    // The instance within that object: the octolith slot, the hunter id, the
    // legend row.  Zero where the object has only one instance.
    int instance = 0;
    int image_index = -1;
    // Top-left position in 0..1 screen fractions, as the managed code writes
    // straight into HudObjectInstance.PositionX/Y.
    float position_x = 0.0F;
    float position_y = 0.0F;
};

// A line of text the pause screen wants drawn.  `max_length` is the scrolling
// reveal, and matches the managed maxLength argument.
struct PauseTextDraw {
    enum class Source : std::uint8_t {
        TopoInitializing,  // 'R' 997 in LocationNames
        TopoUnavailable,   // 'R' 998
        UnknownLocation,   // 'R' 999
        RoomName,          // 'R' area/2*100 + n
        LegendWeapon,      // 'B' message id in HudMessagesSP
        LegendLocked,      // "???" -- "(?)" in Spanish
        LegendOther,       // 'M' message id in HudMessagesSP
        QuitLabel,         // HUD message 119
        QuitPrompt,        // HUD message 122
    };

    Source source = Source::RoomName;
    int message_id = 0;
    float x = 0.0F;
    float y = 0.0F;
    // PlayerHud.Align
    int alignment = 0;
    int wrap_width = 0;
    int spacing_y = 0;
    // -1 draws the whole string; anything else is the scrolling reveal.
    int max_length = -1;
};

// PlayerPause's map legend row.  The six weapon rows are locked until the
// weapon is picked up; the rest are always readable.
struct MapLegendInfo {
    bool unlocked = false;
    // 0 is a weapon row, which reads "???" until the weapon is found; 1 is a
    // row that is always readable.
    int group = 0;
    int message_id = 0;
    int offset_x = 0;
    int offset_y = 0;
    int object_index = 0;
    // The last two rows use the "other" symbol sheet rather than the door one.
    bool other_object = false;
};

class PauseMapNav final {
public:
    static constexpr float MinZoom = 0.4375F;
    static constexpr float MaxZoom = 3.0F;
    static constexpr float ZoomStep = 0.0625F;
    // 2.8125 degrees is the DS's own turn granularity; the pause map turns at
    // half of it per frame.
    static constexpr float RotateStep = 2.8125F / 2.0F;
    static constexpr float PitchLimit = 71.41113F;
    static constexpr float PanSpeed = 4.6F / 2.0F;
    static constexpr float PanTimerMax = 16.0F * 30.0F;
    // The scrolling text reveals one character every 1/30 s and the timers run
    // out at 200 and 60 frames respectively.
    static constexpr float TextTimerMax = 200.0F / 30.0F;
    static constexpr float LoadingTimerMax = 60.0F / 30.0F;

    // PlayerPause._navDoorColors: the ten door palettes, already divided by
    // 255 the way the managed table is.
    [[nodiscard]] static const std::array<formats::Vector3, 10>&
        door_colors() noexcept;
    // PlayerPause._navMapNodeOffsets: the per-area nudge that lines the map
    // models up with each other.
    [[nodiscard]] static const std::array<formats::Vector3, 9>&
        map_node_offsets() noexcept;
    // PlayerPause._mapIconPositions / _mapDotOffsets
    [[nodiscard]] static const std::array<formats::Vector2, 8>&
        icon_positions() noexcept;
    [[nodiscard]] static const std::array<formats::Vector2, 3>&
        dot_offsets() noexcept;
    // The ten legend rows, in the order the column layout walks them.
    [[nodiscard]] static const std::array<MapLegendInfo, 10>&
        default_legend() noexcept;

    // PlayerPause.SetUpMenuPauseHud.  `rooms` is empty during a room
    // transition, which is what puts the screen in its loading state.
    void set_up_menu_pause_hud(formats::Vector3 facing_vector,
                               bool in_room_transition,
                               std::span<const NavMapRoom> rooms,
                               const std::string& room_name);
    // PlayerPause.SetUpMenuPauseMapNav
    void set_up_menu_pause_map_nav(formats::Vector3 facing_vector,
                                   std::span<const NavMapRoom> rooms,
                                   const std::string& room_name);
    // PlayerPause.SetNavMapDrawNode
    void set_nav_map_draw_node(const NavMapRoom& room);
    // PlayerPause.EndMenuPauseHud, minus the HUD layer restore the caller owns.
    void end_menu_pause_hud() noexcept;
    // PlayerPause.ResetPauseQuitDisplay
    void reset_pause_quit_display() noexcept;

    // PlayerPause.ProcessPauseMenu: ages the scrolling text and leaves the
    // loading state once the room transition is over.
    void process_pause_menu(float frame_time, bool in_room_transition,
                            formats::Vector3 facing_vector,
                            std::span<const NavMapRoom> rooms,
                            const std::string& room_name);
    // PlayerPause.ProcessPauseMenuInput
    PauseMenuAction process_pause_menu_input(
        const PauseMapInput& input, float frame_time,
        std::span<const NavMapRoom> rooms);

    // PlayerPause.GetPauseMapLookVectors: camera position and target.  Not
    // const, because it writes the pan target the same way the managed code
    // does -- the draw path relies on that side effect.
    [[nodiscard]] std::pair<formats::Vector3, formats::Vector3>
        pause_map_look_vectors() noexcept;
    // PlayerPause.GetPauseMapMatrices: view first, then the orthographic
    // projection.
    [[nodiscard]] std::pair<formats::Matrix4, formats::Matrix4>
        pause_map_matrices() noexcept;
    // PlayerPause.UpdateMapModelTransforms
    [[nodiscard]] static formats::Matrix4 map_model_transform(
        float model_scale, int area) noexcept;

    // PlayerPause.CheckRoomVisited: a node named ConNN is a connector and is
    // looked up by its index within the area; anything else is matched against
    // the room table by name.
    [[nodiscard]] static bool check_room_visited(
        const std::string& node_name, int area_id, const game::StorySave& save);

    // PlayerPause.GetPauseMapRenderItems: where the player marker goes on the
    // map, given the room offset the caller found in the room's collision.
    [[nodiscard]] formats::Vector3 player_marker_position(
        formats::Vector3 player_position, formats::Vector3 room_offset,
        const std::string& room_name) const noexcept;
    // The Fan Room height fudge, exposed because it is the surprising half of
    // the marker position.
    [[nodiscard]] static float marker_height_factor(
        const std::string& room_name) noexcept;
    // True when the map model, rather than a message, should be drawn.
    [[nodiscard]] bool draw_map_model(bool hud_overlay_down) const noexcept {
        return nav_map_model_enabled_ && draw_state_ == PauseDrawState::Map
            && !hud_overlay_down;
    }
    // Doors are only drawn for a selected room that is not a connector.
    [[nodiscard]] bool draw_room_doors() const noexcept;

    // PlayerPause.DrawPauseMenuBackground: the teleporter symbols that sit on
    // top of the map, for every visited room.  `symbol_positions` are the
    // symbol world positions the caller pulled from NavMapRoomSymbols, paired
    // with the sub-type that picks the image.
    struct TeleporterSymbol {
        formats::Vector3 position{};
        int sub_type = 0;
    };
    [[nodiscard]] std::vector<PauseHudDraw> draw_pause_menu_background(
        std::span<const TeleporterSymbol> symbols,
        bool hud_overlay_down) noexcept;

    // PlayerPause.DrawPauseMenuForeground.  Everything it needs from the save
    // is read through `save`; `available_weapons` unlocks the six weapon
    // legend rows, in Battlehammer, VoltDriver, ShockCoil, Imperialist,
    // Judicator, Magmaul order.
    struct ForegroundDraws {
        std::vector<PauseHudDraw> objects;
        std::vector<PauseTextDraw> text;
    };
    [[nodiscard]] ForegroundDraws draw_pause_menu_foreground(
        const game::StorySave& save, std::array<bool, 6> available_weapons,
        bool hud_overlay_down, const std::string& selected_room_display_name,
        int octolith_width, int octolith_height, int teleporter_width,
        int teleporter_height, int dot_width, int dot_height,
        int quit_width, int quit_height);

    [[nodiscard]] PauseDrawState draw_state() const noexcept {
        return draw_state_;
    }
    void set_draw_state(PauseDrawState state) noexcept { draw_state_ = state; }
    [[nodiscard]] bool nav_loading() const noexcept { return nav_loading_; }
    [[nodiscard]] bool nav_map_model_enabled() const noexcept {
        return nav_map_model_enabled_;
    }
    [[nodiscard]] float nav_text_timer() const noexcept {
        return nav_text_timer_;
    }
    [[nodiscard]] float zoom() const noexcept { return draw_zoom_; }
    [[nodiscard]] float rotation_x() const noexcept { return draw_rot_x_; }
    [[nodiscard]] float rotation_y() const noexcept { return draw_rot_y_; }
    [[nodiscard]] formats::Vector3 pan_offset() const noexcept {
        return pan_offset_;
    }
    [[nodiscard]] const std::string& selected_room() const noexcept {
        return selected_room_;
    }
    [[nodiscard]] formats::Vector3 current_room_position() const noexcept {
        return cur_room_node_pos_;
    }
    [[nodiscard]] formats::Vector3 current_center_position() const noexcept {
        return cur_center_node_pos_;
    }
    [[nodiscard]] int pause_frame_count() const noexcept {
        return pause_frame_count_;
    }
    [[nodiscard]] int scrolling_characters() const noexcept {
        return prev_scrolling_chars_;
    }
    // The area the caller's map models are indexed by; the pause screen always
    // works on a pair, `area_id & ~1` and the one after it.
    void set_area_id(int area_id) noexcept { area_id_ = area_id; }
    [[nodiscard]] int area_id() const noexcept { return area_id_; }

private:
    [[nodiscard]] int reveal_characters() const noexcept;

    PauseDrawState draw_state_ = PauseDrawState::Hidden;
    bool nav_loading_ = false;
    bool nav_map_model_enabled_ = false;
    bool has_draw_node_ = false;
    std::string selected_room_;
    float nav_text_timer_ = 0.0F;
    int prev_scrolling_chars_ = 0;
    int pause_frame_count_ = 0;
    int area_id_ = 0;
    float draw_zoom_ = 0.0F;
    float draw_rot_x_ = 0.0F;
    float draw_rot_y_ = 0.0F;
    float pan_timer_ = 0.0F;
    formats::Vector3 cur_room_node_pos_{};
    formats::Vector3 cur_center_node_pos_{};
    // Deliberately not updated when panning to another room: the player marker
    // stays anchored to the room the player is actually standing in.
    formats::Vector3 init_room_node_pos_{};
    formats::Vector3 target_pos_{};
    formats::Vector3 pan_offset_{};
    std::array<MapLegendInfo, 10> legend_ = default_legend();
};

} // namespace fruityprime::players
