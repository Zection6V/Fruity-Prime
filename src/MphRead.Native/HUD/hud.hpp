#pragma once

#include "Entities/gameplay.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace MphReadNative::Hud {

struct ColorRgba {
    std::uint8_t red = 0;
    std::uint8_t green = 0;
    std::uint8_t blue = 0;
    std::uint8_t alpha = 0;

    [[nodiscard]] static ColorRgba from_bgr555(
        std::uint16_t value, std::uint8_t alpha = 255) noexcept;
};

// The fields match HudInfo.UiAnimParams.  Keeping the unused fields is
// intentional: animation tables are part of the on-cartridge contract even
// when a frontend only needs the first image today.
struct UiAnimParams {
    std::uint8_t image_index = 0;
    std::uint8_t delay = 0;
    std::uint16_t field2 = 0;
    std::int32_t field4 = 0;
    std::uint16_t param_pa = 0;
    std::uint16_t param_pb = 0;
    std::uint16_t param_pc = 0;
    std::uint16_t param_pd = 0;
};

enum class Align : std::uint8_t {
    Left = 0,
    Right = 1,
    Center = 2,
    PadCenter = 3
};

struct Meter {
    bool horizontal = false;
    int length = 0;
    int tank_spacing = 0;
    int tank_offset_x = 0;
    int tank_offset_y = 0;
    int bar_offset_x = 0;
    int bar_offset_y = 0;
    int text_offset_x = 0;
    int text_offset_y = 0;
    Align align = Align::Left;
    int message_id = 0;
    int tank_amount = 100;
    int tank_count = 0;
};

enum class ObjectLoopType : std::uint8_t {
    None = 0,
    Start = 1,
    Offset = 2
};

// Animation and texture state for one OAM-backed HUD object.  It mirrors the
// managed HudObjectInstance API, but leaves the final GPU binding to the
// renderer. Character data is one palette index per pixel after parsing.
class ObjectInstance {
public:
    ObjectInstance(int width, int height, int max_width = 0,
                   int max_height = 0);

    bool enabled = false;
    bool center = false;
    float position_x = 0.0F;
    float position_y = 0.0F;
    int width;
    int height;
    bool flip_horizontal = false;
    bool flip_vertical = false;
    bool use_mask = false;
    std::vector<std::uint8_t> character_data;
    int palette_index = -1;
    std::vector<ColorRgba> palette_data;
    std::vector<int> animation_frames;
    std::vector<ColorRgba> texture;
    ColorRgba color{};
    bool has_color = false;
    float alpha = 1.0F;
    int binding_id = -1;
    int current_frame = 0;
    int start_frame = 0;
    int target_frame = 0;
    float timer = -1.0F;
    float time = -1.0F;
    ObjectLoopType loop = ObjectLoopType::None;
    int after_animation_frame = -1;

    void set_animation_frames(std::span<const UiAnimParams> frames);
    void set_character_data(std::span<const std::uint8_t> data,
                            int frame = 0);
    void set_palette_data(std::span<const ColorRgba> data,
                          int palette = 0);
    void set_palette(int palette);
    void set_data(std::span<const std::uint8_t> chars, int char_frame,
                  std::span<const ColorRgba> palette, int palette_index);
    void set_data(int char_frame, int palette);
    void set_data(int char_frame, ColorRgba color);
    void set_index(int frame);
    void set_animation(int start, int target, int frames,
                       bool repeat = false);
    void set_animation(int start, int target, int frames, int after_animation,
                       ObjectLoopType loop_type = ObjectLoopType::None);
    void process_animation(float frame_seconds);
    [[nodiscard]] bool rebuild_texture();
};

struct RulesInfo {
    int count = 0;
    std::array<int, 8> message_ids{};
    std::array<int, 8> offsets{};
};

// HudInfo.HudObjects: the per-hunter HUD, which is the art it is drawn from
// and where every piece of it sits.  The helmets differ enough that almost
// nothing shares a position between two hunters.
struct ObjectPaths {
    std::string helmet;
    std::string helmet_drop;
    std::string visor;
    std::string scan_visor;
    std::string health_bar_a;
    std::string health_bar_b;
    std::string energy_tanks;
    std::string weapon_icon;
    std::string double_damage;
    std::string cloaking;
    std::string prime_hunter;
    std::string ammo_bar;
    std::string reticle;
    std::string sniper_reticle;
    std::string weapon_select;
    std::string select_icon;
    std::string select_box;
    std::string damage_bar;

    int health_main_pos_x = 0;
    int health_main_pos_y = 0;
    int health_sub_pos_x = 0;
    int health_sub_pos_y = 0;
    // Both bars hang this far below the position above, and slide by the
    // second amount while the player is a ball.
    int health_offset_y = 0;
    int health_offset_y_alt = 0;
    int ammo_bar_pos_x = 0;
    int ammo_bar_pos_y = 0;
    int weapon_icon_pos_x = 0;
    int weapon_icon_pos_y = 0;
    int enemy_health_pos_x = 0;
    int enemy_health_pos_y = 0;
    int enemy_health_text_pos_x = 0;
    int enemy_health_text_pos_y = 0;
    int score_pos_x = 0;
    int score_pos_y = 0;
    Align score_align = Align::Left;
    int octolith_pos_x = 0;
    int octolith_pos_y = 0;
    int prime_pos_x = 0;
    int prime_pos_y = 0;
    int prime_text_pos_x = 0;
    int prime_text_pos_y = 0;
    Align prime_align = Align::Left;
    int node_bonus_pos_x = 0;
    int node_bonus_pos_y = 0;
    int enemy_bonus_pos_x = 0;
    int enemy_bonus_pos_y = 0;
    int node_icon_pos_x = 0;
    int node_icon_pos_y = 0;
    int node_text_pos_x = 0;
    int node_text_pos_y = 0;
    int dbl_dmg_pos_x = 0;
    int dbl_dmg_pos_y = 0;
    int dbl_dmg_text_pos_x = 0;
    int dbl_dmg_text_pos_y = 0;
    Align dbl_dmg_align = Align::Left;
    int cloak_pos_x = 0;
    int cloak_pos_y = 0;
    int cloak_text_pos_x = 0;
    int cloak_text_pos_y = 0;
    Align cloak_align = Align::Left;
};

struct Elements {
    std::string ice_layer;
    std::string boost;
    std::string bombs;
    std::string stars;
    std::string octolith;
    std::string nodes_og;
    std::string nodes_rb;
    std::string system_load;
    std::string message_box;
    std::string message_spacer;
    std::string map_scan;
    std::string dialog_button;
    std::string dialog_arrow;
    std::string dialog_crystal;
    std::string dialog_pickup;
    std::string dialog_frame;
    std::string map_portal;
    std::string map_octolith;
    std::string map_lost_octolith;
    std::string map_legend_doors;
    std::string map_legend_other;
    std::string map_quit;
    std::array<std::string, 8> hunters;
    std::array<std::string, 8> map_dots;
    std::array<RulesInfo, 7> rules;
    std::array<std::string, 10> scan_icons;
    std::array<ObjectPaths, 8> hunter_objects;
    std::array<Meter, 8> main_healthbars;
    std::array<Meter, 8> sub_healthbars;
    std::array<Meter, 8> ammo_bars;
    Meter enemy_healthbar;
    Meter node_progress_bar;
};

[[nodiscard]] const Elements& elements() noexcept;

struct HudTexture {
    int width = 0;
    int height = 0;
    std::vector<ColorRgba> pixels;
    std::vector<std::uint16_t> palette;

    [[nodiscard]] bool valid() const noexcept {
        return width > 0 && height > 0
            && pixels.size() == static_cast<std::size_t>(width)
                * static_cast<std::size_t>(height);
    }
};

struct HudObjectAsset {
    std::uint16_t frame_count = 0;
    std::uint16_t image_count = 0;
    int width = 0;
    int height = 0;
    // One byte per pixel after expanding the source 4bpp character data.
    std::vector<std::uint8_t> character_indices;
    std::vector<ColorRgba> palette;
    std::vector<UiAnimParams> animation_params;

    [[nodiscard]] HudTexture render_frame(
        std::size_t frame = 0, std::size_t palette_index = 0) const;
};

// Decode a DS HUD object (the format used by hud_targetcircle.bin,
// hud_energybar.bin, and the other OAM-backed assets).
[[nodiscard]] HudObjectAsset parse_object(
    std::span<const std::uint8_t> bytes,
    std::string_view source_name = {});

// Decode a DS screen/character map (the format used by bg_bottom.bin and the
// visor/helmet layers).  The optional sub-rectangle and palette override are
// the same controls used by the managed HudInfo.CharMapToTexture path.
[[nodiscard]] HudTexture decode_char_map(
    std::span<const std::uint8_t> bytes, int start_x = 0, int start_y = 0,
    int tiles_x = 0, int tiles_y = 0,
    std::span<const std::uint16_t> palette_override = {},
    int palette_id = -1, std::string_view source_name = {});

// The managed HUD is designed in a 256x192 coordinate space.  Keeping this
// small layout contract independent from Win32/OpenGL lets the desktop,
// future Android renderer, and screenshot tests share the same geometry.
struct Rect {
    float left = 0.0F;
    float top = 0.0F;
    float right = 0.0F;
    float bottom = 0.0F;
};

struct Viewport {
    int width = 0;
    int height = 0;

    [[nodiscard]] float scale() const noexcept {
        if (width <= 0 || height <= 0) {
            return 0.0F;
        }
        return std::min(static_cast<float>(width) / 256.0F,
            static_cast<float>(height) / 192.0F);
    }

    [[nodiscard]] Rect design_area() const noexcept {
        const float factor = scale();
        const float left = (static_cast<float>(width) - 256.0F * factor) * 0.5F;
        const float top = (static_cast<float>(height) - 192.0F * factor) * 0.5F;
        return {left, top, left + 256.0F * factor, top + 192.0F * factor};
    }
};

struct PlayerState {
    std::uint16_t health = 0;
    std::uint16_t health_max = 0;
    std::int16_t points = 0;
    std::uint16_t kills = 0;
    std::uint16_t deaths = 0;
    std::uint8_t current_weapon = 0;
    std::string weapon_name;
    std::uint16_t ammo = 0;
    std::uint16_t ammo_max = 0;
    bool alt_form = false;
    bool zoomed = false;
    bool spectating = false;
    bool frozen = false;
};

// Converts simulation state into renderer-independent HUD data.  This keeps
// text, layout, Win32 drawing, and a future Android renderer out of the
// gameplay session.
[[nodiscard]] PlayerState player_state(
    const fruityprime::gameplay::Session& session, std::uint8_t slot);

} // namespace MphReadNative::Hud
