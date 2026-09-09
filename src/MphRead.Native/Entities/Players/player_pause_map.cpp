// Native counterpart of src/MphRead/Entities/Players/PlayerPause.cs.
#include "pause_map.hpp"

#include "Metadata/metadata_lookup.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>

namespace fruityprime::players {

namespace {

constexpr float Pi = 3.14159265358979323846F;

[[nodiscard]] constexpr float to_radians(float degrees) noexcept {
    return degrees * Pi / 180.0F;
}

[[nodiscard]] constexpr float to_degrees(float radians) noexcept {
    return radians * 180.0F / Pi;
}

[[nodiscard]] formats::Vector3 add(formats::Vector3 a,
                                   formats::Vector3 b) noexcept {
    return {a.x + b.x, a.y + b.y, a.z + b.z};
}

[[nodiscard]] formats::Vector3 subtract(formats::Vector3 a,
                                        formats::Vector3 b) noexcept {
    return {a.x - b.x, a.y - b.y, a.z - b.z};
}

[[nodiscard]] formats::Vector3 scale(formats::Vector3 a, float f) noexcept {
    return {a.x * f, a.y * f, a.z * f};
}

[[nodiscard]] bool is_zero(formats::Vector3 a) noexcept {
    return a.x == 0.0F && a.y == 0.0F && a.z == 0.0F;
}

// EntityBase.ExponentialDecay: a per-second decay expressed as a per-frame
// factor, so the map settles at the same rate whatever the frame rate is.
[[nodiscard]] float exponential_decay(float step, float value,
                                      float frame_time) noexcept {
    const float decay = std::pow(step, 30.0F);
    return value * std::pow(decay, frame_time);
}

[[nodiscard]] bool equals_ignore_case(const std::string& a,
                                      const std::string& b) noexcept {
    if (a.size() != b.size()) {
        return false;
    }
    for (std::size_t i = 0; i < a.size(); ++i) {
        const auto left = static_cast<unsigned char>(a[i]);
        const auto right = static_cast<unsigned char>(b[i]);
        if (std::tolower(left) != std::tolower(right)) {
            return false;
        }
    }
    return true;
}

[[nodiscard]] bool starts_with(const std::string& value,
                               const char* prefix) noexcept {
    const std::string needle(prefix);
    return value.size() >= needle.size()
        && value.compare(0, needle.size(), needle) == 0;
}

// OpenTK's Matrix4.LookAt, in the same row-vector convention the rest of the
// port uses.
[[nodiscard]] formats::Matrix4 look_at(formats::Vector3 eye,
                                       formats::Vector3 target,
                                       formats::Vector3 up) noexcept {
    formats::Vector3 z = subtract(eye, target);
    float length = std::sqrt(z.x * z.x + z.y * z.y + z.z * z.z);
    if (length > 0.0F) {
        z = scale(z, 1.0F / length);
    }
    formats::Vector3 x{up.y * z.z - up.z * z.y, up.z * z.x - up.x * z.z,
                       up.x * z.y - up.y * z.x};
    length = std::sqrt(x.x * x.x + x.y * x.y + x.z * x.z);
    if (length > 0.0F) {
        x = scale(x, 1.0F / length);
    }
    formats::Vector3 y{z.y * x.z - z.z * x.y, z.z * x.x - z.x * x.z,
                       z.x * x.y - z.y * x.x};
    length = std::sqrt(y.x * y.x + y.y * y.y + y.z * y.z);
    if (length > 0.0F) {
        y = scale(y, 1.0F / length);
    }
    formats::Matrix4 result{};
    result.m11 = x.x; result.m12 = y.x; result.m13 = z.x; result.m14 = 0.0F;
    result.m21 = x.y; result.m22 = y.y; result.m23 = z.y; result.m24 = 0.0F;
    result.m31 = x.z; result.m32 = y.z; result.m33 = z.z; result.m34 = 0.0F;
    result.m41 = -(x.x * eye.x + x.y * eye.y + x.z * eye.z);
    result.m42 = -(y.x * eye.x + y.y * eye.y + y.z * eye.z);
    result.m43 = -(z.x * eye.x + z.y * eye.y + z.z * eye.z);
    result.m44 = 1.0F;
    return result;
}

// OpenTK's Matrix4.CreateOrthographic, centred on the origin.
[[nodiscard]] formats::Matrix4 create_orthographic(float width, float height,
                                                   float depth_near,
                                                   float depth_far) noexcept {
    formats::Matrix4 result{};
    result.m11 = 2.0F / width;
    result.m22 = 2.0F / height;
    result.m33 = 2.0F / (depth_near - depth_far);
    result.m43 = (depth_near + depth_far) / (depth_near - depth_far);
    result.m44 = 1.0F;
    return result;
}

// The first column of a row-vector view matrix is the camera's right axis, and
// the second its up axis -- which is what panning moves along.
[[nodiscard]] formats::Vector3 column0(const formats::Matrix4& m) noexcept {
    return {m.m11, m.m21, m.m31};
}

[[nodiscard]] formats::Vector3 column1(const formats::Matrix4& m) noexcept {
    return {m.m12, m.m22, m.m32};
}

} // namespace

const std::array<formats::Vector3, 10>& PauseMapNav::door_colors() noexcept {
    static const std::array<formats::Vector3, 10> colors{{
        {230.0F / 255.0F, 230.0F / 255.0F, 230.0F / 255.0F},
        {255.0F / 255.0F, 255.0F / 255.0F, 0.0F / 255.0F},
        {247.0F / 255.0F, 148.0F / 255.0F, 82.0F / 255.0F},
        {0.0F / 255.0F, 255.0F / 255.0F, 0.0F / 255.0F},
        {255.0F / 255.0F, 0.0F / 255.0F, 0.0F / 255.0F},
        {165.0F / 255.0F, 74.0F / 255.0F, 255.0F / 255.0F},
        {255.0F / 255.0F, 132.0F / 255.0F, 0.0F / 255.0F},
        {0.0F / 255.0F, 132.0F / 255.0F, 255.0F / 255.0F},
        {230.0F / 255.0F, 230.0F / 255.0F, 230.0F / 255.0F},
        {165.0F / 255.0F, 165.0F / 255.0F, 165.0F / 255.0F}
    }};
    return colors;
}

const std::array<formats::Vector3, 9>&
PauseMapNav::map_node_offsets() noexcept {
    static const std::array<formats::Vector3, 9> offsets{{
        {0.0F, 0.0F, 0.0F},
        {-80.87378F, 22.282959F, 205.73096F},
        {0.0F, 0.0F, 0.0F},
        {0.0F, 73.0F, 0.0F},
        {0.0F, 0.0F, 0.0F},
        {-136.7998F, -0.21191406F, 4.36499F},
        {0.0F, 0.0F, 0.0F},
        {0.0F, 0.0F, 0.0F},
        {0.0F, 0.0F, 0.0F}
    }};
    return offsets;
}

const std::array<formats::Vector2, 8>&
PauseMapNav::icon_positions() noexcept {
    static const std::array<formats::Vector2, 8> positions{{
        {15.0F, 102.0F}, {15.0F, 134.0F}, {15.0F, 38.0F}, {15.0F, 70.0F},
        {241.0F, 38.0F}, {241.0F, 70.0F}, {241.0F, 102.0F}, {241.0F, 134.0F}
    }};
    return positions;
}

const std::array<formats::Vector2, 3>& PauseMapNav::dot_offsets() noexcept {
    static const std::array<formats::Vector2, 3> offsets{{
        {-10.0F, -7.0F}, {10.0F, -7.0F}, {0.0F, 12.0F}
    }};
    return offsets;
}

const std::array<MapLegendInfo, 10>& PauseMapNav::default_legend() noexcept {
    // Group 0 is a weapon whose name is hidden until it is found; group 1 is
    // always readable.  The object index is the frame within the symbol sheet,
    // and is not in the same order as the rows.
    static const std::array<MapLegendInfo, 10> legend{{
        {false, 0, 4, 0, 0, 4, false},   // BATTLEHAMMER
        {false, 0, 2, 0, 0, 5, false},   // VOLT DRIVER
        {false, 0, 8, 0, 0, 1, false},   // SHOCK COIL
        {false, 0, 5, 0, 0, 0, false},   // IMPERIALIST
        {false, 0, 6, 0, 0, 3, false},   // JUDICATOR
        {false, 0, 7, 0, 0, 2, false},   // MAGMAUL
        {true, 1, 1, 0, 0, 7, false},    // ANY BEAM
        {true, 1, 3, 0, 0, 8, false},    // MISSILE
        {true, 1, 2, -3, -4, 0, true},   // PORTAL
        {true, 1, 4, -3, -4, 1, true}    // BOSS PORTAL
    }};
    return legend;
}

void PauseMapNav::set_nav_map_draw_node(const NavMapRoom& room) {
    has_draw_node_ = true;
    selected_room_ = room.name;
    cur_room_node_pos_ = room.room_position;
    cur_center_node_pos_ = room.center_position;
}

void PauseMapNav::set_up_menu_pause_map_nav(formats::Vector3 facing_vector,
                                            std::span<const NavMapRoom> rooms,
                                            const std::string& room_name) {
    has_draw_node_ = false;
    selected_room_.clear();
    nav_map_model_enabled_ = false;
    draw_zoom_ = 0.75F;
    draw_rot_x_ = to_degrees(std::atan2(-facing_vector.x, -facing_vector.z));
    draw_rot_y_ = 28.125F;
    pan_timer_ = 0.0F;
    cur_room_node_pos_ = {};
    cur_center_node_pos_ = {};
    init_room_node_pos_ = {};
    target_pos_ = {};
    pan_offset_ = {};
    pause_frame_count_ = 0;
    if (room_name.empty()) {
        return;
    }
    // The managed code walks the two map models for this area pair and takes
    // the node whose name is the room the player is standing in.  Both models
    // are visited even after a match, which is why the last one wins.
    for (const NavMapRoom& room : rooms) {
        if (!equals_ignore_case(room.name, room_name)) {
            continue;
        }
        set_nav_map_draw_node(room);
        nav_map_model_enabled_ = true;
        init_room_node_pos_ = cur_room_node_pos_;
    }
    if (has_draw_node_) {
        target_pos_ = cur_room_node_pos_;
    }
}

void PauseMapNav::set_up_menu_pause_hud(formats::Vector3 facing_vector,
                                        bool in_room_transition,
                                        std::span<const NavMapRoom> rooms,
                                        const std::string& room_name) {
    nav_text_timer_ = 0.0F;
    prev_scrolling_chars_ = 0;
    draw_state_ = PauseDrawState::Map;
    if (in_room_transition) {
        // The map cannot be built while the next room is still loading, so the
        // screen shows its "initializing" message until the load finishes.
        nav_loading_ = true;
        return;
    }
    set_up_menu_pause_map_nav(facing_vector, rooms, room_name);
    nav_loading_ = false;
}

void PauseMapNav::end_menu_pause_hud() noexcept {
    has_draw_node_ = false;
    selected_room_.clear();
    nav_map_model_enabled_ = false;
}

void PauseMapNav::reset_pause_quit_display() noexcept {
    nav_text_timer_ = 0.0F;
    prev_scrolling_chars_ = 0;
}

int PauseMapNav::reveal_characters() const noexcept {
    return static_cast<int>(nav_text_timer_ / (1.0F / 30.0F));
}

void PauseMapNav::process_pause_menu(float frame_time,
                                     bool in_room_transition,
                                     formats::Vector3 facing_vector,
                                     std::span<const NavMapRoom> rooms,
                                     const std::string& room_name) {
    if (nav_loading_) {
        if (nav_text_timer_ < LoadingTimerMax) {
            nav_text_timer_ += frame_time;
        }
        if (nav_text_timer_ >= LoadingTimerMax && !in_room_transition) {
            set_up_menu_pause_map_nav(facing_vector, rooms, room_name);
            nav_text_timer_ = 0.0F;
            prev_scrolling_chars_ = 0;
            nav_loading_ = false;
        }
    } else if (nav_text_timer_ < TextTimerMax) {
        // The same timer scrolls the topo-unavailable message, the location
        // name and the quit prompt.
        nav_text_timer_ += frame_time;
    }
    ++pause_frame_count_;
}

std::pair<formats::Vector3, formats::Vector3>
PauseMapNav::pause_map_look_vectors() noexcept {
    target_pos_ = add(cur_center_node_pos_, pan_offset_);
    formats::Vector3 camera_target = target_pos_;
    camera_target.y += 3.75F;
    const float sin_x = std::sin(to_radians(draw_rot_x_));
    const float cos_x = std::cos(to_radians(draw_rot_x_));
    const float sin_y = std::sin(to_radians(draw_rot_y_));
    const float cos_y = std::cos(to_radians(draw_rot_y_));
    const formats::Vector3 camera_pos = add(
        camera_target,
        {sin_x * cos_y * 90.0F, sin_y * 90.0F, cos_x * cos_y * 90.0F});
    return {camera_pos, camera_target};
}

std::pair<formats::Matrix4, formats::Matrix4>
PauseMapNav::pause_map_matrices() noexcept {
    const formats::Matrix4 ortho = create_orthographic(
        256.0F * draw_zoom_, 192.0F / 256.0F * 256.0F * draw_zoom_,
        -400.0F, 400.0F);
    const auto [camera_pos, camera_target] = pause_map_look_vectors();
    const formats::Matrix4 view = look_at(camera_pos, camera_target,
                                          {0.0F, 1.0F, 0.0F});
    return {view, ortho};
}

formats::Matrix4 PauseMapNav::map_model_transform(float model_scale,
                                                  int area) noexcept {
    formats::Matrix4 transform{};
    transform.m11 = model_scale;
    transform.m22 = model_scale;
    transform.m33 = model_scale;
    transform.m44 = 1.0F;
    if (area >= 0
        && area < static_cast<int>(map_node_offsets().size())) {
        const formats::Vector3 offset =
            map_node_offsets()[static_cast<std::size_t>(area)];
        transform.m41 += offset.x;
        transform.m42 += offset.y;
        transform.m43 += offset.z;
    }
    return transform;
}

bool PauseMapNav::check_room_visited(const std::string& node_name,
                                     int area_id, const game::StorySave& save) {
    // A connector node is named ConNN and is tracked per area rather than by
    // room id, so it is looked up first -- a connector name would otherwise
    // never match the room table and always read as unvisited.
    if (starts_with(node_name, "Con") && node_name.size() >= 5) {
        const std::string digits = node_name.substr(3, 2);
        char* end = nullptr;
        const long id = std::strtol(digits.c_str(), &end, 10);
        if (end != nullptr && *end == '\0' && id >= 1) {
            return save.visited_connector(static_cast<std::int32_t>(id - 1),
                                          area_id);
        }
        return false;
    }
    // Rooms 27 through 92 are the story rooms; the multiplayer arenas below
    // and above that range never appear on a map.
    for (int i = 27; i <= 92; ++i) {
        const metadata::RoomMetadata* meta = metadata::room_by_id(i);
        if (meta == nullptr) {
            continue;
        }
        if (equals_ignore_case(node_name, std::string(meta->name))) {
            return save.visited_room(i);
        }
    }
    return false;
}

PauseMenuAction PauseMapNav::process_pause_menu_input(
    const PauseMapInput& input, float frame_time,
    std::span<const NavMapRoom> rooms) {
    if (input.pause_prevented) {
        return PauseMenuAction::None;
    }
    if (draw_state_ == PauseDrawState::Map && input.quit_pressed) {
        draw_state_ = PauseDrawState::QuitPrompt;
        reset_pause_quit_display();
        return PauseMenuAction::OpenQuitPrompt;
    }
    if (draw_state_ == PauseDrawState::QuitPrompt) {
        if (input.yes_pressed) {
            draw_state_ = PauseDrawState::Hidden;
            reset_pause_quit_display();
            return PauseMenuAction::ConfirmQuit;
        }
        if (input.no_pressed) {
            draw_state_ = PauseDrawState::Map;
            reset_pause_quit_display();
            return PauseMenuAction::CancelQuit;
        }
    }
    if (draw_state_ != PauseDrawState::Map) {
        return PauseMenuAction::None;
    }

    if (input.scroll_up) {
        draw_zoom_ -= ZoomStep;
    } else if (input.scroll_down) {
        draw_zoom_ += ZoomStep;
    }
    draw_zoom_ = std::clamp(draw_zoom_, MinZoom, MaxZoom);

    if (input.aim_left) {
        draw_rot_x_ += RotateStep;
    } else if (input.aim_right) {
        draw_rot_x_ -= RotateStep;
    } else if (input.mouse_left_down && input.mouse_delta_x != 0.0F) {
        draw_rot_x_ += -input.mouse_delta_x / 8.0F * 2.8125F;
    }
    // Wrapped rather than clamped, because yaw is a full turn.
    if (draw_rot_x_ >= 360.0F) {
        draw_rot_x_ -= 360.0F;
    } else if (draw_rot_x_ <= -360.0F) {
        draw_rot_x_ += 360.0F;
    }

    if (input.aim_up) {
        draw_rot_y_ -= RotateStep;
    } else if (input.aim_down) {
        draw_rot_y_ += RotateStep;
    } else if (input.mouse_left_down && input.mouse_delta_y != 0.0F) {
        draw_rot_y_ += input.mouse_delta_y / 8.0F * 2.8125F;
    }
    draw_rot_y_ = std::clamp(draw_rot_y_, -PitchLimit, PitchLimit);

    const auto [view, ortho] = pause_map_matrices();
    float pan_dir_x = 0.0F;
    if (input.move_left) {
        pan_dir_x = -1.0F;
    } else if (input.move_right) {
        pan_dir_x = 1.0F;
    }
    if (pan_dir_x != 0.0F) {
        pan_offset_ = add(pan_offset_,
                          scale(column0(view), PanSpeed * pan_dir_x));
    }
    float pan_dir_y = 0.0F;
    if (input.move_up) {
        pan_dir_y = 1.0F;
    } else if (input.move_down) {
        pan_dir_y = -1.0F;
    }
    if (pan_dir_y != 0.0F) {
        pan_offset_ = add(pan_offset_,
                          scale(column1(view), PanSpeed * pan_dir_y));
    }

    if (pan_dir_x != 0.0F || pan_dir_y != 0.0F) {
        pan_timer_ = 0.0F;
        float min_dist = 512.0F * 512.0F;
        const NavMapRoom* new_room = nullptr;
        for (const NavMapRoom& room : rooms) {
            if (!room.visited) {
                continue;
            }
            formats::Vector2 projected{};
            if (formats::MatrixOps::project_position(room.center_position, view,
                                                  ortho, projected) <= 0.0F) {
                continue;
            }
            // Upstream converts the already-converted coordinate a second
            // time before the on-screen test, which biases the test toward the
            // lower right.  Reproduced, because it is what decides which rooms
            // the retail game lets you select.
            const formats::Vector2 screen{(projected.x + 1.0F) / 2.0F,
                                          (1.0F - projected.y) / 2.0F};
            const float dist_x = projected.x * 2.0F - 1.0F;
            const float dist_y = 1.0F - projected.y * 2.0F;
            const float dist = dist_x * dist_x + dist_y * dist_y;
            if (screen.x > 0.0F && screen.x < 1.0F && screen.y > 0.0F
                && screen.y < 1.0F && dist < min_dist) {
                new_room = &room;
                min_dist = dist;
            }
        }
        if (new_room != nullptr
            && (!has_draw_node_ || new_room->name != selected_room_)) {
            set_nav_map_draw_node(*new_room);
            // The pan offset is rebased onto the new room so the camera does
            // not jump: it now measures the distance from the new centre.
            pan_offset_ = subtract(target_pos_, cur_center_node_pos_);
            nav_text_timer_ = 0.0F;
            prev_scrolling_chars_ = 0;
        }
    } else if (!is_zero(pan_offset_) && !input.no_map_centering) {
        if (pan_timer_ < PanTimerMax) {
            pan_timer_ += frame_time;
            pan_timer_ = std::min(pan_timer_, PanTimerMax);
        }
        const float frames = pan_timer_ * 30.0F;
        // Up to a tenth is shed per frame after sixteen in-game frames of
        // holding still, so a small nudge settles quickly and a long pan
        // eases back.
        const float factor = 1.0F - 0.1F * (frames / 16.0F);
        float x = exponential_decay(factor, pan_offset_.x, frame_time);
        float y = exponential_decay(factor, pan_offset_.y, frame_time);
        float z = exponential_decay(factor, pan_offset_.z, frame_time);
        // Below the cartridge's own 1/4096 the offset is treated as gone;
        // without the snap it approaches zero forever and never arrives.
        if (std::fabs(x) < 1.0F / 4096.0F) {
            x = 0.0F;
        }
        if (std::fabs(y) < 1.0F / 4096.0F) {
            y = 0.0F;
        }
        if (std::fabs(z) < 1.0F / 4096.0F) {
            z = 0.0F;
        }
        pan_offset_ = {x, y, z};
    }
    return PauseMenuAction::None;
}

float PauseMapNav::marker_height_factor(const std::string& room_name) noexcept {
    // bugfix: the Fan Rooms' map geometry is stretched vertically so the room
    // lines up with its connector, but the player marker is not, which puts
    // the marker halfway up the room when standing at its upper door.  The
    // same factor is applied to the marker here.
    if (room_name == "UNIT2_C2") {  // Fan Room Alpha
        return 2.05F;
    }
    if (room_name == "UNIT2_C3") {  // Fan Room Beta
        return 1.305F;
    }
    return 1.0F;
}

formats::Vector3 PauseMapNav::player_marker_position(
    formats::Vector3 player_position, formats::Vector3 room_offset,
    const std::string& room_name) const noexcept {
    const float height_factor = marker_height_factor(room_name);
    return {
        player_position.x + init_room_node_pos_.x - room_offset.x,
        player_position.y * height_factor + init_room_node_pos_.y
            - room_offset.y + 1.0F,
        player_position.z + init_room_node_pos_.z - room_offset.z
    };
}

bool PauseMapNav::draw_room_doors() const noexcept {
    return has_draw_node_ && !starts_with(selected_room_, "Con");
}

std::vector<PauseHudDraw> PauseMapNav::draw_pause_menu_background(
    std::span<const TeleporterSymbol> symbols,
    bool hud_overlay_down) noexcept {
    std::vector<PauseHudDraw> draws;
    if (!nav_map_model_enabled_ || draw_state_ != PauseDrawState::Map
        || hud_overlay_down) {
        return draws;
    }
    const auto [view, ortho] = pause_map_matrices();
    for (const TeleporterSymbol& symbol : symbols) {
        formats::Vector2 projected{};
        if (formats::MatrixOps::project_position(symbol.position, view, ortho,
                                              projected) <= 0.0F) {
            continue;
        }
        const formats::Vector2 screen{(projected.x + 1.0F) / 2.0F,
                                      (1.0F - projected.y) / 2.0F};
        if (screen.x <= 0.0F || screen.x >= 1.0F || screen.y <= 0.0F
            || screen.y >= 1.0F) {
            continue;
        }
        PauseHudDraw draw;
        draw.object = PauseHudDraw::Object::LegendOther;
        draw.image_index = symbol.sub_type;
        // Half of the symbol's eight pixels, in screen fractions.
        draw.position_x = projected.x - 1.0F / (256.0F / 8.0F);
        draw.position_y = projected.y - 1.0F / (192.0F / 8.0F);
        draws.push_back(draw);
    }
    return draws;
}

PauseMapNav::ForegroundDraws PauseMapNav::draw_pause_menu_foreground(
    const game::StorySave& save, std::array<bool, 6> available_weapons,
    bool hud_overlay_down, const std::string& selected_room_display_name,
    int octolith_width, int octolith_height, int teleporter_width,
    int teleporter_height, int dot_width, int dot_height, int quit_width,
    int quit_height) {
    ForegroundDraws result;
    const int characters = reveal_characters();

    if (nav_loading_) {
        PauseTextDraw text;
        text.source = PauseTextDraw::Source::TopoInitializing;
        text.message_id = 997;
        text.x = 128.0F;
        text.y = 96.0F;
        text.alignment = 3;  // Align.PadCenter
        text.wrap_width = 150;
        text.spacing_y = 9;
        text.max_length = characters;
        result.text.push_back(text);
    } else if (draw_state_ == PauseDrawState::Map) {
        int artifact_index = 0;
        for (int i = 0; i < 8; ++i) {
            const formats::Vector2 pos =
                icon_positions()[static_cast<std::size_t>(i)];
            // Areas are unlocked in pairs, so both slots of a pair read the
            // even one's bit.
            if ((save.areas & (1u << (i / 2 * 2))) != 0) {
                const bool has_octolith =
                    (save.current_octoliths & (1u << i)) != 0;
                const std::uint32_t lost_hunter =
                    (save.lost_octoliths >> (i * 4)) & 15u;
                if (has_octolith || lost_hunter < 8) {
                    int offset_x = 0;
                    auto object = PauseHudDraw::Object::Octolith;
                    int instance = i;
                    if (!has_octolith) {
                        // A hunter who took the octolith is shown holding it,
                        // which is why the portrait is drawn first and the
                        // octolith is nudged clear of their face.
                        PauseHudDraw portrait;
                        portrait.object = PauseHudDraw::Object::HunterPortrait;
                        portrait.instance = static_cast<int>(lost_hunter);
                        portrait.position_x = (pos.x - 16.0F) / 256.0F;
                        portrait.position_y = (pos.y - 16.0F) / 192.0F;
                        result.objects.push_back(portrait);
                        object = PauseHudDraw::Object::LostOctolith;
                        instance = 0;
                        offset_x = 6;
                    }
                    PauseHudDraw octolith;
                    octolith.object = object;
                    octolith.instance = instance;
                    octolith.position_x =
                        (pos.x + static_cast<float>(offset_x)
                         - static_cast<float>(octolith_width) / 2.0F) / 256.0F;
                    octolith.position_y =
                        (pos.y - static_cast<float>(octolith_height) / 2.0F)
                        / 192.0F;
                    result.objects.push_back(octolith);
                } else {
                    PauseHudDraw teleporter;
                    teleporter.object = PauseHudDraw::Object::Teleporter;
                    teleporter.position_x =
                        (pos.x - static_cast<float>(teleporter_width) / 2.0F)
                        / 256.0F;
                    teleporter.position_y =
                        (pos.y - static_cast<float>(teleporter_height) / 2.0F)
                        / 192.0F;
                    // All three of the area's artifacts switches the portal
                    // symbol to its open frame.
                    const std::uint32_t mask = 7u
                        << static_cast<unsigned>(artifact_index);
                    teleporter.image_index =
                        ((save.artifacts & mask)
                         >> static_cast<unsigned>(artifact_index)) == 7u
                        ? 1 : 0;
                    result.objects.push_back(teleporter);
                    for (int j = 0; j < 3; ++j) {
                        if ((save.artifacts
                             & (1u << static_cast<unsigned>(
                                    artifact_index + j))) == 0) {
                            continue;
                        }
                        const formats::Vector2 offset =
                            dot_offsets()[static_cast<std::size_t>(j)];
                        PauseHudDraw dot;
                        dot.object = PauseHudDraw::Object::ArtifactDot;
                        dot.instance = i;
                        dot.position_x =
                            (pos.x + offset.x
                             - static_cast<float>(dot_width) / 2.0F) / 256.0F;
                        dot.position_y =
                            (pos.y + offset.y
                             - static_cast<float>(dot_height) / 2.0F) / 192.0F;
                        result.objects.push_back(dot);
                    }
                }
            }
            artifact_index += 3;
        }

        if (!nav_map_model_enabled_) {
            PauseTextDraw text;
            text.source = PauseTextDraw::Source::TopoUnavailable;
            text.message_id = 998;
            text.x = 128.0F;
            text.y = 96.0F;
            text.alignment = 3;
            text.wrap_width = 150;
            text.spacing_y = 9;
            text.max_length = characters;
            result.text.push_back(text);
        } else {
            PauseTextDraw name;
            if (selected_room_display_name.empty()) {
                name.source = PauseTextDraw::Source::UnknownLocation;
                name.message_id = 999;
            } else {
                name.source = PauseTextDraw::Source::RoomName;
                name.message_id = 0;
            }
            name.x = 128.0F;
            // One line is centred on 173; every extra line lifts it by four.
            name.y = 173.0F;
            name.alignment = 3;
            name.wrap_width = 175;
            name.spacing_y = 9;
            name.max_length = characters;
            result.text.push_back(name);

            if (hud_overlay_down) {
                legend_[0].unlocked = available_weapons[0];
                legend_[1].unlocked = available_weapons[1];
                legend_[2].unlocked = available_weapons[2];
                legend_[3].unlocked = available_weapons[3];
                legend_[4].unlocked = available_weapons[4];
                legend_[5].unlocked = available_weapons[5];
                int column = 0;
                float pos_y = 65.0F;
                for (std::size_t i = 0; i < legend_.size(); ++i) {
                    const MapLegendInfo& info = legend_[i];
                    const float text_x = column == 0 ? 114.0F : 143.0F;
                    const float object_x = column == 0 ? 116.0F : 132.0F;
                    PauseHudDraw symbol;
                    symbol.object = info.other_object
                        ? PauseHudDraw::Object::LegendOther
                        : PauseHudDraw::Object::LegendSymbol;
                    symbol.instance = static_cast<int>(i);
                    symbol.image_index = info.object_index;
                    symbol.position_x =
                        (object_x + static_cast<float>(info.offset_x)) / 256.0F;
                    symbol.position_y =
                        (pos_y + static_cast<float>(info.offset_y)) / 192.0F;
                    result.objects.push_back(symbol);

                    PauseTextDraw row;
                    if (info.group == 0) {
                        row.source = info.unlocked
                            ? PauseTextDraw::Source::LegendWeapon
                            : PauseTextDraw::Source::LegendLocked;
                    } else {
                        row.source = PauseTextDraw::Source::LegendOther;
                    }
                    row.message_id = info.message_id;
                    row.x = text_x;
                    row.y = pos_y + 1.0F;
                    // Right-aligned in the left column, left-aligned in the
                    // right one, so the two columns meet in the middle.
                    row.alignment = column == 0 ? 2 : 0;
                    row.wrap_width = 80;
                    row.spacing_y = 8;
                    result.text.push_back(row);

                    pos_y += 8.0F + 3.0F;
                    if (pos_y >= 129.0F) {
                        pos_y = 65.0F;
                        ++column;
                    }
                }
            }
        }
    }

    // PlayerPause.DrawPauseQuitInterface
    if (draw_state_ == PauseDrawState::Map) {
        constexpr float QuitX = 26.0F;
        PauseHudDraw quit;
        quit.object = PauseHudDraw::Object::Quit;
        quit.position_x =
            (QuitX - static_cast<float>(quit_width) / 2.0F) / 256.0F;
        quit.position_y =
            (173.0F - static_cast<float>(quit_height) / 2.0F) / 192.0F;
        result.objects.push_back(quit);
        PauseTextDraw label;
        label.source = PauseTextDraw::Source::QuitLabel;
        label.message_id = 119;
        label.x = QuitX;
        label.y = 181.0F;
        label.alignment = 1;  // Align.Center
        result.text.push_back(label);
    } else if (draw_state_ == PauseDrawState::QuitPrompt) {
        PauseTextDraw prompt;
        prompt.source = PauseTextDraw::Source::QuitPrompt;
        prompt.message_id = 122;
        prompt.x = 128.0F;
        prompt.y = 90.0F;
        prompt.alignment = 3;
        prompt.max_length = characters;
        result.text.push_back(prompt);
    }
    return result;
}

} // namespace fruityprime::players
