#include "hud.hpp"

#include "../Metadata/metadata.hpp"

#include <algorithm>
#include <cmath>
#include <string>

namespace {

MphReadNative::Hud::ObjectPaths make_object_paths(std::string_view local_name) {
    const std::string local = "_archives/local" + std::string(local_name) + "/";
    const std::string scan = "_archives/localSamus/bg_top_ovl.bin";
    return {
        local + "bg_top.bin",
        local + "bg_top_drop.bin",
        local + "bg_top_ovl.bin",
        scan,
        local + "hud_energybar.bin",
        local + "hud_energybar2.bin",
        "_archives/spSamus/hud_etank.bin",
        local + "hud_weaponicon.bin",
        local + "hud_damage.bin",
        local + "cloaking.bin",
        local + "hud_primehunter.bin",
        local + "hud_ammobar.bin",
        local + "hud_targetcircle.bin",
        local + "hud_snipercircle.bin",
        local + "rad_wepsel.bin",
        local + "wepsel_icon.bin",
        local + "wepsel_box.bin",
        local + "rad_ammobar.bin"
    };
}

MphReadNative::Hud::Meter make_meter(bool horizontal, int length,
                                      int tank_spacing, int tank_offset_x,
                                      int tank_offset_y, int bar_offset_x,
                                      int bar_offset_y, int text_offset_x,
                                      int text_offset_y,
                                      MphReadNative::Hud::Align align,
                                      int message_id, int tank_count) {
    MphReadNative::Hud::Meter meter;
    meter.horizontal = horizontal;
    meter.length = length;
    meter.tank_spacing = tank_spacing;
    meter.tank_offset_x = tank_offset_x;
    meter.tank_offset_y = tank_offset_y;
    meter.bar_offset_x = bar_offset_x;
    meter.bar_offset_y = bar_offset_y;
    meter.text_offset_x = text_offset_x;
    meter.text_offset_y = text_offset_y;
    meter.align = align;
    meter.message_id = message_id;
    meter.tank_count = tank_count;
    return meter;
}

} // namespace

namespace MphReadNative::Hud {

ObjectInstance::ObjectInstance(int width_value, int height_value,
                               int max_width, int max_height)
    : width(width_value), height(height_value) {
    const int texture_width = max_width > 0 ? max_width : width_value;
    const int texture_height = max_height > 0 ? max_height : height_value;
    if (texture_width > 0 && texture_height > 0) {
        texture.resize(static_cast<std::size_t>(texture_width)
                       * static_cast<std::size_t>(texture_height));
    }
}

void ObjectInstance::set_animation_frames(
    std::span<const UiAnimParams> frames) {
    animation_frames.clear();
    for (const auto& frame : frames) {
        animation_frames.insert(animation_frames.end(), frame.delay,
                                static_cast<int>(frame.image_index));
    }
}

void ObjectInstance::set_character_data(
    std::span<const std::uint8_t> data, int frame) {
    character_data.assign(data.begin(), data.end());
    current_frame = frame;
    timer = 0.0F;
    if (!palette_data.empty() || has_color) {
        static_cast<void>(rebuild_texture());
    }
}

void ObjectInstance::set_palette_data(std::span<const ColorRgba> data,
                                      int palette) {
    palette_data.assign(data.begin(), data.end());
    palette_index = palette;
    has_color = false;
    if (!character_data.empty()) {
        static_cast<void>(rebuild_texture());
    }
}

void ObjectInstance::set_palette(int palette) {
    has_color = false;
    if (palette_index != palette) {
        palette_index = palette;
        if (!character_data.empty()) {
            static_cast<void>(rebuild_texture());
        }
    }
}

void ObjectInstance::set_data(std::span<const std::uint8_t> chars,
                              int char_frame,
                              std::span<const ColorRgba> palette,
                              int palette_value) {
    has_color = false;
    timer = 0.0F;
    character_data.assign(chars.begin(), chars.end());
    current_frame = char_frame;
    palette_data.assign(palette.begin(), palette.end());
    palette_index = palette_value;
    static_cast<void>(rebuild_texture());
}

void ObjectInstance::set_data(int char_frame, int palette) {
    has_color = false;
    timer = 0.0F;
    const bool changed = current_frame != char_frame
        || palette_index != palette;
    current_frame = char_frame;
    palette_index = palette;
    if (changed && !character_data.empty() && !palette_data.empty()) {
        static_cast<void>(rebuild_texture());
    }
}

void ObjectInstance::set_data(int char_frame, ColorRgba color_value) {
    timer = 0.0F;
    const bool changed = current_frame != char_frame
        || !has_color || color.red != color_value.red
        || color.green != color_value.green || color.blue != color_value.blue
        || color.alpha != color_value.alpha;
    current_frame = char_frame;
    palette_index = -1;
    color = color_value;
    has_color = true;
    if (changed && !character_data.empty()) {
        static_cast<void>(rebuild_texture());
    }
}

void ObjectInstance::set_index(int frame) {
    const bool changed = current_frame != frame;
    current_frame = frame;
    timer = 0.0F;
    if (changed && (!palette_data.empty() || has_color)) {
        static_cast<void>(rebuild_texture());
    }
}

void ObjectInstance::set_animation(int start, int target, int frames,
                                   bool repeat) {
    if (start == target) {
        current_frame = start;
        return;
    }
    start_frame = start;
    target_frame = target;
    timer = time = static_cast<float>(frames) / 30.0F;
    loop = repeat ? ObjectLoopType::Start : ObjectLoopType::None;
}

void ObjectInstance::set_animation(int start, int target, int frames,
                                   int after_animation,
                                   ObjectLoopType loop_type) {
    if (start == target) {
        current_frame = start;
        return;
    }
    start_frame = start;
    target_frame = target;
    timer = time = static_cast<float>(frames) / 30.0F;
    if (loop_type == ObjectLoopType::Offset) {
        after_animation_frame = after_animation;
    } else if (after_animation >= 0
               && static_cast<std::size_t>(after_animation)
                      < animation_frames.size()) {
        after_animation_frame = animation_frames[
            static_cast<std::size_t>(after_animation)];
    } else {
        after_animation_frame = target;
    }
    loop = loop_type;
}

void ObjectInstance::process_animation(float frame_seconds) {
    if (timer <= 0.0F) {
        return;
    }
    const int previous = current_frame;
    timer -= frame_seconds;
    if (timer <= 0.0F) {
        if (loop == ObjectLoopType::Start) {
            current_frame = start_frame;
            timer = time;
        } else if (loop == ObjectLoopType::Offset) {
            start_frame = after_animation_frame;
            timer = time;
        } else {
            current_frame = animation_frames.empty()
                ? target_frame : after_animation_frame;
        }
    } else if (loop == ObjectLoopType::Offset) {
        const float elapsed = time - timer;
        const int frame = start_frame + static_cast<int>(
            std::lround(elapsed * 30.0F));
        current_frame = frame >= 0
            && static_cast<std::size_t>(frame) < animation_frames.size()
            ? animation_frames[static_cast<std::size_t>(frame)] : 0;
    } else {
        const int frame = start_frame + static_cast<int>(std::lround(
            static_cast<float>(target_frame - start_frame)
                * (1.0F - timer / time)));
        if (animation_frames.empty()) {
            current_frame = frame;
        } else {
            current_frame = frame >= 0
                && static_cast<std::size_t>(frame) < animation_frames.size()
                ? animation_frames[static_cast<std::size_t>(frame)] : 0;
        }
    }
    if (current_frame != previous && (!palette_data.empty() || has_color)) {
        static_cast<void>(rebuild_texture());
    }
}

bool ObjectInstance::rebuild_texture() {
    if (width <= 0 || height <= 0 || character_data.empty()) {
        return false;
    }
    const std::size_t pixels = static_cast<std::size_t>(width)
        * static_cast<std::size_t>(height);
    const std::size_t offset = static_cast<std::size_t>(std::max(0, current_frame))
        * pixels;
    if (offset > character_data.size()
        || pixels > character_data.size() - offset) {
        return false;
    }
    if (!has_color) {
        if (palette_index < 0) {
            return false;
        }
        const std::size_t palette_offset = static_cast<std::size_t>(palette_index)
            * 16U;
        if (palette_offset > palette_data.size()
            || 16U > palette_data.size() - palette_offset) {
            return false;
        }
    }
    if (texture.size() < pixels) {
        texture.resize(pixels);
    }
    const std::size_t palette_offset = palette_index < 0 ? 0
        : static_cast<std::size_t>(palette_index) * 16U;
    // Same tile layout as HudObjectAsset::render_frame.
    std::fill(texture.begin(), texture.begin()
                  + static_cast<std::ptrdiff_t>(pixels), ColorRgba{});
    const std::size_t tiles_x = static_cast<std::size_t>(width) / 8;
    const std::size_t tiles_y = static_cast<std::size_t>(height) / 8;
    for (std::size_t tile_y = 0; tile_y < tiles_y; ++tile_y) {
        for (std::size_t tile_x = 0; tile_x < tiles_x; ++tile_x) {
            const std::size_t tile = (tile_y * tiles_x + tile_x) * 64;
            for (std::size_t y = 0; y < 8; ++y) {
                for (std::size_t x = 0; x < 8; ++x) {
                    const std::uint8_t palette_value =
                        character_data[offset + tile + y * 8 + x];
                    if (palette_value == 0) {
                        continue;
                    }
                    const std::size_t index = tile_y * tiles_x * 64
                        + tile_x * 8 + y * tiles_x * 8 + x;
                    texture[index] = has_color
                        ? color
                        : palette_data[palette_offset + palette_value];
                }
            }
        }
    }
    return true;
}

// The positional half of HudInfo.HudObjects, kept beside the paths so the
// table below reads as one record per hunter the way the managed one does.
struct HunterPositions {
    int health_main_pos_x;
    int health_main_pos_y;
    int health_sub_pos_x;
    int health_sub_pos_y;
    int health_offset_y;
    int health_offset_y_alt;
    int ammo_bar_pos_x;
    int ammo_bar_pos_y;
    int weapon_icon_pos_x;
    int weapon_icon_pos_y;
    int enemy_health_pos_x;
    int enemy_health_pos_y;
    int enemy_health_text_pos_x;
    int enemy_health_text_pos_y;
    int score_pos_x;
    int score_pos_y;
    Align score_align;
    int octolith_pos_x;
    int octolith_pos_y;
    int prime_pos_x;
    int prime_pos_y;
    int prime_text_pos_x;
    int prime_text_pos_y;
    Align prime_align;
    int node_bonus_pos_x;
    int node_bonus_pos_y;
    int enemy_bonus_pos_x;
    int enemy_bonus_pos_y;
    int node_icon_pos_x;
    int node_icon_pos_y;
    int node_text_pos_x;
    int node_text_pos_y;
    int dbl_dmg_pos_x;
    int dbl_dmg_pos_y;
    int dbl_dmg_text_pos_x;
    int dbl_dmg_text_pos_y;
    Align dbl_dmg_align;
    int cloak_pos_x;
    int cloak_pos_y;
    int cloak_text_pos_x;
    int cloak_text_pos_y;
    Align cloak_align;
};

void apply_positions(ObjectPaths& paths, const HunterPositions& value) {
    paths.health_main_pos_x = value.health_main_pos_x;
    paths.health_main_pos_y = value.health_main_pos_y;
    paths.health_sub_pos_x = value.health_sub_pos_x;
    paths.health_sub_pos_y = value.health_sub_pos_y;
    paths.health_offset_y = value.health_offset_y;
    paths.health_offset_y_alt = value.health_offset_y_alt;
    paths.ammo_bar_pos_x = value.ammo_bar_pos_x;
    paths.ammo_bar_pos_y = value.ammo_bar_pos_y;
    paths.weapon_icon_pos_x = value.weapon_icon_pos_x;
    paths.weapon_icon_pos_y = value.weapon_icon_pos_y;
    paths.enemy_health_pos_x = value.enemy_health_pos_x;
    paths.enemy_health_pos_y = value.enemy_health_pos_y;
    paths.enemy_health_text_pos_x = value.enemy_health_text_pos_x;
    paths.enemy_health_text_pos_y = value.enemy_health_text_pos_y;
    paths.score_pos_x = value.score_pos_x;
    paths.score_pos_y = value.score_pos_y;
    paths.score_align = value.score_align;
    paths.octolith_pos_x = value.octolith_pos_x;
    paths.octolith_pos_y = value.octolith_pos_y;
    paths.prime_pos_x = value.prime_pos_x;
    paths.prime_pos_y = value.prime_pos_y;
    paths.prime_text_pos_x = value.prime_text_pos_x;
    paths.prime_text_pos_y = value.prime_text_pos_y;
    paths.prime_align = value.prime_align;
    paths.node_bonus_pos_x = value.node_bonus_pos_x;
    paths.node_bonus_pos_y = value.node_bonus_pos_y;
    paths.enemy_bonus_pos_x = value.enemy_bonus_pos_x;
    paths.enemy_bonus_pos_y = value.enemy_bonus_pos_y;
    paths.node_icon_pos_x = value.node_icon_pos_x;
    paths.node_icon_pos_y = value.node_icon_pos_y;
    paths.node_text_pos_x = value.node_text_pos_x;
    paths.node_text_pos_y = value.node_text_pos_y;
    paths.dbl_dmg_pos_x = value.dbl_dmg_pos_x;
    paths.dbl_dmg_pos_y = value.dbl_dmg_pos_y;
    paths.dbl_dmg_text_pos_x = value.dbl_dmg_text_pos_x;
    paths.dbl_dmg_text_pos_y = value.dbl_dmg_text_pos_y;
    paths.dbl_dmg_align = value.dbl_dmg_align;
    paths.cloak_pos_x = value.cloak_pos_x;
    paths.cloak_pos_y = value.cloak_pos_y;
    paths.cloak_text_pos_x = value.cloak_text_pos_x;
    paths.cloak_text_pos_y = value.cloak_text_pos_y;
    paths.cloak_align = value.cloak_align;
}

const Elements& elements() noexcept {
    static const Elements value = [] {
        Elements output;
        output.ice_layer = "_archives/common/bg_ice.bin";
        output.boost = "_archives/common/hud_boost.bin";
        output.bombs = "_archives/common/hud_bombs.bin";
        output.stars = "_archives/commonMP/stars.bin";
        output.octolith = "_archives/commonMP/radar_octolithLARGE.bin";
        output.nodes_og = "hud/rad_NodesOG.bin";
        output.nodes_rb = "hud/rad_NodesRB.bin";
        output.system_load = "_archives/commonMP/hud_systemload.bin";
        output.message_box = "_archives/spSamus/hud_msgBox.bin";
        output.message_spacer = "_archives/spSamus/message_spacer.bin";
        output.map_scan = "_archives/spSamus/map_scan.bin";
        output.dialog_button = "_archives/spSamus/scan_ok.bin";
        output.dialog_arrow = "_archives/spSamus/scan_arrow.bin";
        output.dialog_crystal = "_archives/spSamus/message_crystalpickup.bin";
        output.dialog_pickup = "_archives/spSamus/message_pickups.bin";
        output.dialog_frame = "_archives/spSamus/message_pickupframe.bin";
        output.map_portal = "_archives/spSamus/map_portal.bin";
        output.map_octolith = "_archives/spSamus/map_crystalbig.bin";
        output.map_lost_octolith = "_archives/spSamus/map_crystalred.bin";
        output.map_legend_doors = "_archives/spSamus/map_legendDoors.bin";
        output.map_legend_other = "_archives/spSamus/map_legendOthers.bin";
        output.map_quit = "_archives/spSamus/map_quit.bin";
        output.hunters = {
            "_archives/common/enemy_samus.bin",
            "_archives/common/enemy_kanden.bin",
            "_archives/common/enemy_trace.bin",
            "_archives/common/enemy_sylux.bin",
            "_archives/common/enemy_noxus.bin",
            "_archives/common/enemy_spyre.bin",
            "_archives/common/enemy_weavel.bin",
            "_archives/common/enemy_weavel.bin"
        };
        for (std::size_t index = 0; index < output.map_dots.size(); ++index) {
            output.map_dots[index] = "_archives/spSamus/map_art_"
                + std::to_string(index + 1) + ".bin";
        }
        output.rules = {
            RulesInfo{4, {1, 2, 3, 4, 0, 0, 0, 0}, {}},
            RulesInfo{4, {11, 12, 13, 14, 0, 0, 0, 0}, {}},
            RulesInfo{7, {21, 22, 23, 24, 25, 26, 27, 0},
                      {0, 0, 0, 0, 12, 12, 12, 0}},
            RulesInfo{5, {31, 32, 33, 34, 35, 0, 0, 0}, {}},
            RulesInfo{6, {41, 42, 43, 44, 45, 46, 0, 0}, {}},
            RulesInfo{4, {51, 52, 53, 54, 0, 0, 0, 0}, {}},
            RulesInfo{8, {61, 62, 63, 64, 65, 66, 67, 68},
                      {0, 0, 0, 0, 0, 12, 12, 12}}
        };
        output.scan_icons = {
            "_archives/spSamus/scan_lore.bin",
            "_archives/spSamus/scan_lore_dim.bin",
            "_archives/spSamus/scan_enemy.bin",
            "_archives/spSamus/scan_enemy_dim.bin",
            "_archives/spSamus/scan_object.bin",
            "_archives/spSamus/scan_object_dim.bin",
            "_archives/spSamus/scan_equipment.bin",
            "_archives/spSamus/scan_equipment_dim.bin",
            "_archives/spSamus/scan_red.bin",
            "_archives/spSamus/scan_red_dim.bin"
        };
        const std::array<std::string_view, 8> local{
            "Samus", "Kanden", "Trace", "Sylux", "Nox", "Spire",
            "Weavel", "Weavel"
        };
        for (std::size_t index = 0; index < local.size(); ++index) {
            output.hunter_objects[index] = make_object_paths(local[index]);
        }
        // HudInfo.HunterObjects: where each helmet puts its own
        // readouts.  The paths above and these positions are one
        // record in the managed table.
        constexpr std::array<HunterPositions, 8> positions{{
            // Samus
            {93, -5, 93, 1, 32, -10, 236, 137, 214, 150,
             93, 164, 128, 168, 12, 30, Align::Left,
             228, 28, 232, 42, -16, -4, Align::Right,
             22, 56, 22, 80, 220, 41, 220, 45,
             64, 174, 16, -8, Align::Left,
             192, 174, -16, 2, Align::Right},
            // Kanden
            {13, 0, 20, 0, 128, 0, 238, 128, 230, 138,
             93, 164, 128, 168, 20, 4, Align::Left,
             212, 4, 222, 17, -16, -10, Align::Right,
             96, 4, 136, 4, 210, 12, 210, 14,
             22, 156, 6, 14, Align::Left,
             224, 176, -16, 3, Align::Right},
            // Trace
            {24, 135, 29, 135, 0, 0, 232, 135, 225, 148,
             93, 164, 128, 168, 128, 12, Align::Center,
             176, 12, 226, 56, -16, -10, Align::Right,
             60, 24, 24, 38, 202, 32, 202, 36,
             48, 172, 16, -7, Align::Left,
             208, 172, -16, 3, Align::Right},
            // Sylux
            {47, 165, 51, 165, 0, 0, 206, 165, 214, 131,
             93, 164, 128, 168, 56, 8, Align::Left,
             186, 4, 190, 16, 14, 17, Align::Right,
             212, 38, 212, 62, 180, 15, 180, 17,
             32, 164, 20, -3, Align::Left,
             186, 162, 16, 12, Align::Right},
            // Noxus
            {29, 0, 34, 0, 117, 0, 221, 117, 196, 138,
             93, 164, 128, 168, 36, 12, Align::Left,
             200, 8, 204, 16, 14, 17, Align::Right,
             40, 32, 190, 32, 200, 18, 200, 20,
             56, 173, 16, -8, Align::Left,
             200, 173, -16, 2, Align::Right},
            // Spire
            {12, 0, 21, 0, 128, 0, 233, 128, 227, 20,
             93, 164, 128, 168, 10, 16, Align::Left,
             208, 13, 210, 20, 14, 17, Align::Right,
             37, 35, 193, 35, 196, 16, 196, 20,
             68, 164, 16, -8, Align::Left,
             188, 164, -16, 2, Align::Right},
            // Weavel
            {22, 118, 30, 118, 0, 0, 229, 118, 206, 104,
             93, 164, 128, 168, 128, 18, Align::Center,
             216, 4, 214, 78, -16, -10, Align::Right,
             36, 4, 196, 4, 128, 9, 128, 11,
             88, 178, 13, -8, Align::Left,
             168, 178, 0, -18, Align::Center},
            // Guardian
            {93, -5, 93, 1, 32, -10, 236, 137, 214, 150,
             93, 164, 128, 168, 12, 30, Align::Left,
             228, 28, 232, 42, -16, -4, Align::Right,
             22, 56, 22, 80, 220, 41, 220, 45,
             64, 174, 16, -8, Align::Left,
             192, 174, -16, 2, Align::Right}
        }};
        for (std::size_t index = 0; index < positions.size();
             ++index) {
            apply_positions(output.hunter_objects[index],
                            positions[index]);
        }
        output.enemy_healthbar = make_meter(true, 0, 0, 0, 0, 15, 6, 30, 7,
                                            Align::Left, 0, 0);
        output.node_progress_bar = make_meter(true, 40, 8, 1, -8, 15, 6, 0,
                                              -7, Align::Center, 0, 5);
        output.main_healthbars = {
            make_meter(true, 72, 6, 1, 8, 0, -8, 30, -8, Align::Left, 6, 0),
            make_meter(false, 80, 8, 8, 3, 32, -35, 30, -7, Align::Right, 0, 5),
            make_meter(false, 64, 8, -8, 3, 8, 0, 0, 0, Align::Left, 0, 0),
            make_meter(false, 152, 8, 8, 1, -4, -66, 0, 0, Align::Right, 0, 5),
            make_meter(false, 80, 8, 8, 3, -3, 1, 30, -7, Align::Right, 0, 5),
            make_meter(false, 80, 8, 10, 3, 5, -82, 30, -7, Align::Center, 0, 5),
            make_meter(false, 64, 8, 8, 3, 10, -68, 0, 0, Align::Left, 0, 5),
            make_meter(true, 72, 6, 1, 8, 0, -8, 30, -8, Align::Left, 6, 0)
        };
        output.sub_healthbars = {
            make_meter(true, 72, 8, 1, -8, 15, 6, 30, 7, Align::Left, 0, 5),
            make_meter(false, 80, 8, 1, -8, 15, 6, 30, 7, Align::Left, 0, 5),
            make_meter(false, 64, 0, 0, 0, 0, 0, 0, 0, Align::Left, 0, 0),
            make_meter(false, 152, 8, 1, -8, 0, 0, 0, 0, Align::Left, 0, 5),
            make_meter(false, 80, 8, 1, -8, 15, 6, 30, 7, Align::Left, 0, 5),
            make_meter(false, 80, 8, 1, -8, 15, 6, 30, 7, Align::Left, 0, 5),
            make_meter(false, 64, 8, 1, -8, 0, 0, 0, 0, Align::Left, 0, 5),
            make_meter(true, 72, 8, 1, -8, 15, 6, 30, 7, Align::Left, 0, 5)
        };
        output.ammo_bars = {
            make_meter(false, 72, 8, 8, 1, -3, 0, -2, -1, Align::Right, 0, 5),
            make_meter(false, 80, 8, 8, 1, -22, -35, -2, -1, Align::Left, 0, 5),
            make_meter(false, 64, 0, 0, 0, -2, 0, 0, 0, Align::Right, 0, 0),
            make_meter(false, 152, 8, 8, 1, 6, -66, 0, 0, Align::Left, 0, 5),
            make_meter(false, 80, 8, 8, 1, 9, 1, -2, -1, Align::Left, 0, 5),
            make_meter(false, 80, 8, 8, 1, 3, -82, -2, -1, Align::Center, 0, 5),
            make_meter(false, 64, 8, 8, 1, -10, -68, 0, 0, Align::Center, 0, 5),
            make_meter(false, 72, 8, 8, 1, -3, 0, -2, -1, Align::Right, 0, 5)
        };
        return output;
    }();
    return value;
}

PlayerState player_state(const fruityprime::gameplay::Session& session,
                         std::uint8_t slot) {
    const auto& player = session.player(slot);
    const auto& inventory = session.inventory(slot);
    const auto& weapon = Metadata::weapon_info(player.current_weapon);
    const std::size_t ammo_type = std::min<std::size_t>(weapon.ammo_type,
        inventory.ammo.size() - 1);

    PlayerState result;
    result.health = player.health;
    result.health_max = inventory.health_max;
    result.points = player.points;
    result.kills = player.kills;
    result.deaths = player.deaths;
    result.current_weapon = player.current_weapon;
    result.weapon_name = std::string(weapon.display_name);
    result.ammo = inventory.ammo[ammo_type];
    result.ammo_max = inventory.ammo_max[ammo_type];
    result.alt_form = (player.flags & fruityprime::net::PlayerState::FlagAltForm) != 0;
    result.zoomed = (player.flags & fruityprime::net::PlayerState::FlagZoomed) != 0;
    result.spectating = (player.flags & fruityprime::net::PlayerState::FlagSpectating) != 0;
    result.frozen = (player.flags & fruityprime::net::PlayerState::FlagFrozen) != 0;
    return result;
}

} // namespace MphReadNative::Hud

#include "hud.hpp"

#include "Utility/binary_reader.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace MphReadNative::Hud {
namespace {

[[nodiscard]] std::string label(std::string_view source_name) {
    return source_name.empty() ? std::string{"<memory>"}
                               : std::string(source_name);
}

[[noreturn]] void invalid(std::string_view source_name,
                           std::string_view reason) {
    throw std::runtime_error("invalid HUD asset " + label(source_name)
                             + ": " + std::string(reason));
}

[[nodiscard]] std::size_t checked_size(std::int32_t value,
                                       std::string_view source_name,
                                       std::string_view field) {
    if (value < 0) {
        invalid(source_name, std::string(field) + " is negative");
    }
    return static_cast<std::size_t>(value);
}

[[nodiscard]] std::size_t checked_product(std::size_t left,
                                          std::size_t right,
                                          std::string_view source_name,
                                          std::string_view field) {
    if (right != 0
        && left > std::numeric_limits<std::size_t>::max() / right) {
        invalid(source_name, std::string(field) + " is too large");
    }
    return left * right;
}

struct RawObjectAttribute {
    std::uint16_t attr0 = 0;
    std::uint16_t attr1 = 0;
    std::uint16_t attr2 = 0;
    std::uint16_t padding = 0;
};

struct ScreenData {
    std::uint16_t value = 0;

    [[nodiscard]] int character_id() const noexcept {
        return value & 0x3ff;
    }
    [[nodiscard]] bool flip_horizontal() const noexcept {
        return (value & 0x400) != 0;
    }
    [[nodiscard]] bool flip_vertical() const noexcept {
        return (value & 0x800) != 0;
    }
    [[nodiscard]] int palette_id() const noexcept {
        return (value >> 12) & 0xf;
    }
};

constexpr std::array<std::array<std::pair<int, int>, 4>, 3> ObjectDimensions{{
    {{{1, 1}, {2, 2}, {4, 4}, {8, 8}}},
    {{{2, 1}, {4, 1}, {4, 2}, {8, 4}}},
    {{{1, 2}, {1, 4}, {2, 4}, {4, 8}}}
}};

} // namespace

ColorRgba ColorRgba::from_bgr555(std::uint16_t value,
                                 std::uint8_t alpha) noexcept {
    const auto expand = [](std::uint16_t channel) noexcept {
        return static_cast<std::uint8_t>(std::lround(
            static_cast<float>(channel & 0x1f) / 31.0F * 255.0F));
    };
    return {expand(value >> 0), expand(value >> 5), expand(value >> 10),
            alpha};
}

HudTexture HudObjectAsset::render_frame(std::size_t frame,
                                        std::size_t palette_index) const {
    HudTexture result;
    result.width = width;
    result.height = height;
    if (width <= 0 || height <= 0) {
        return result;
    }
    const std::size_t pixel_count = checked_product(
        static_cast<std::size_t>(width), static_cast<std::size_t>(height),
        "object", "dimensions");
    result.pixels.assign(pixel_count, ColorRgba{});
    const std::size_t frame_offset = checked_product(
        frame, pixel_count, "object", "frame offset");
    const std::size_t palette_offset = checked_product(
        palette_index, 16, "object", "palette offset");
    // The source is a run of 8x8 tiles, left to right and then down, each
    // tile's own pixels in reading order -- the DS's OAM layout, not a plain
    // image.  Copying it straight across only happens to be right for an
    // object exactly one tile across, which is why the bars and the font look
    // correct and anything larger came out in scrambled blocks.
    const std::size_t tiles_x = static_cast<std::size_t>(width) / 8;
    const std::size_t tiles_y = static_cast<std::size_t>(height) / 8;
    for (std::size_t tile_y = 0; tile_y < tiles_y; ++tile_y) {
        for (std::size_t tile_x = 0; tile_x < tiles_x; ++tile_x) {
            const std::size_t tile = (tile_y * tiles_x + tile_x) * 64;
            for (std::size_t y = 0; y < 8; ++y) {
                for (std::size_t x = 0; x < 8; ++x) {
                    const std::size_t source = frame_offset + tile + y * 8 + x;
                    if (source >= character_indices.size()) {
                        return result;
                    }
                    const std::uint8_t color_index =
                        character_indices[source];
                    if (color_index == 0) {
                        continue;
                    }
                    const std::size_t palette_entry =
                        palette_offset + color_index;
                    if (palette_entry >= palette.size()) {
                        continue;
                    }
                    result.pixels[tile_y * tiles_x * 64 + tile_x * 8
                                  + y * tiles_x * 8 + x] =
                        palette[palette_entry];
                }
            }
        }
    }
    return result;
}

HudObjectAsset parse_object(std::span<const std::uint8_t> bytes,
                            std::string_view source_name) {
    if (bytes.size() < 24) {
        invalid(source_name, "object header is truncated");
    }
    fruityprime::core::BinaryReader reader(bytes);
    HudObjectAsset result;
    result.frame_count = reader.read_u16_le();
    result.image_count = reader.read_u16_le();
    const std::uint16_t header_width = reader.read_u16_le();
    const std::uint16_t header_height = reader.read_u16_le();
    const std::size_t param_size = checked_size(
        reader.read_i32_le(), source_name, "param data size");
    const std::size_t attr_size = checked_size(
        reader.read_i32_le(), source_name, "attribute data size");
    const std::size_t char_size = checked_size(
        reader.read_i32_le(), source_name, "character data size");
    const std::size_t palette_size = checked_size(
        reader.read_i32_le(), source_name, "palette data size");
    if (param_size % 16 != 0 || attr_size % 8 != 0
        || palette_size % 2 != 0) {
        invalid(source_name, "HUD object section is not aligned");
    }

    result.animation_params.reserve(param_size / 16);
    for (std::size_t i = 0; i < param_size / 16; ++i) {
        UiAnimParams params;
        params.image_index = reader.read_u8();
        params.delay = reader.read_u8();
        params.field2 = reader.read_u16_le();
        params.field4 = reader.read_i32_le();
        params.param_pa = reader.read_u16_le();
        params.param_pb = reader.read_u16_le();
        params.param_pc = reader.read_u16_le();
        params.param_pd = reader.read_u16_le();
        result.animation_params.push_back(params);
    }

    std::vector<RawObjectAttribute> attributes;
    attributes.reserve(attr_size / 8);
    for (std::size_t i = 0; i < attr_size / 8; ++i) {
        RawObjectAttribute attribute;
        attribute.attr0 = reader.read_u16_le();
        attribute.attr1 = reader.read_u16_le();
        attribute.attr2 = reader.read_u16_le();
        attribute.padding = reader.read_u16_le();
        attributes.push_back(attribute);
    }
    if (attributes.empty()) {
        invalid(source_name, "HUD object has no OAM attributes");
    }

    const auto character_data = reader.read_bytes(char_size);
    result.character_indices.reserve(char_size * 2);
    for (const std::uint8_t value : character_data) {
        result.character_indices.push_back(value & 0xf);
        result.character_indices.push_back((value >> 4) & 0xf);
    }

    const auto palette_data = reader.read_bytes(palette_size);
    result.palette.reserve(palette_size / 2);
    for (std::size_t i = 0; i < palette_size; i += 2) {
        const std::uint16_t value = static_cast<std::uint16_t>(
            palette_data[i] | (static_cast<std::uint16_t>(
                palette_data[i + 1]) << 8));
        result.palette.push_back(ColorRgba::from_bgr555(value));
    }
    if (reader.remaining() != 0) {
        invalid(source_name, "trailing bytes follow the object palette");
    }

    const RawObjectAttribute& first = attributes.front();
    const std::size_t shape = (first.attr0 >> 14) & 3;
    const std::size_t size = (first.attr1 >> 14) & 3;
    if (shape >= ObjectDimensions.size() || size >= ObjectDimensions[shape].size()) {
        invalid(source_name, "object shape or size is unsupported");
    }
    const auto [tiles_x, tiles_y] = ObjectDimensions[shape][size];
    result.width = tiles_x * 8;
    result.height = tiles_y * 8;
    // Some tools write zero into these legacy header fields.  They are not
    // the authoritative dimensions (the first OAM record is), but retaining
    // them above keeps malformed data diagnosable without rejecting real ROMs.
    static_cast<void>(header_width);
    static_cast<void>(header_height);
    return result;
}

HudTexture decode_char_map(std::span<const std::uint8_t> bytes, int start_x,
                           int start_y, int tiles_x, int tiles_y,
                           std::span<const std::uint16_t> palette_override,
                           int palette_id, std::string_view source_name) {
    if (bytes.size() < 20) {
        invalid(source_name, "character-map header is truncated");
    }
    if (start_x < 0 || start_y < 0 || tiles_x < 0 || tiles_y < 0
        || palette_id < -1) {
        invalid(source_name, "character-map coordinates are negative");
    }
    fruityprime::core::BinaryReader reader(bytes);
    const std::int32_t magic = reader.read_i32_le();
    const std::size_t char_size = checked_size(
        reader.read_i32_le(), source_name, "character data size");
    const std::size_t palette_size = checked_size(
        reader.read_i32_le(), source_name, "palette data size");
    if (magic != 0) {
        invalid(source_name, "character-map magic is not zero");
    }
    if (palette_size % 2 != 0) {
        invalid(source_name, "character-map palette is not 16-bit aligned");
    }
    const auto character_data = reader.read_bytes(char_size);
    const auto palette_bytes = reader.read_bytes(palette_size);
    if (reader.remaining() < 8) {
        invalid(source_name, "screen-map header is truncated");
    }
    const int chars_x = reader.read_u16_le();
    const int chars_y = reader.read_u16_le();
    const std::size_t screen_size = checked_size(
        reader.read_i32_le(), source_name, "screen data size");
    if (screen_size % 2 != 0) {
        invalid(source_name, "screen data is not 16-bit aligned");
    }
    const auto screen_bytes = reader.read_bytes(screen_size);
    for (const std::uint8_t value : reader.read_bytes(reader.remaining())) {
        if (value != 0) {
            invalid(source_name, "non-zero bytes follow screen data");
        }
    }
    if (chars_x <= 0 || chars_y <= 0) {
        invalid(source_name, "character-map dimensions are empty");
    }

    std::vector<std::uint16_t> raw_palette;
    raw_palette.reserve(palette_size / 2);
    for (std::size_t i = 0; i < palette_size; i += 2) {
        raw_palette.push_back(static_cast<std::uint16_t>(
            palette_bytes[i] | (static_cast<std::uint16_t>(
                palette_bytes[i + 1]) << 8)));
    }
    if (!palette_override.empty()) {
        if (raw_palette.size() < 2 || palette_override.size() < 1) {
            invalid(source_name, "palette override has no base colors");
        }
        std::vector<std::uint16_t> replacement;
        replacement.push_back(raw_palette[0]);
        replacement.push_back(raw_palette[1]);
        for (std::size_t i = 1; i < palette_override.size(); ++i) {
            replacement.push_back(palette_override[i]);
        }
        raw_palette = std::move(replacement);
    }

    if (character_data.size() % 32 != 0) {
        invalid(source_name, "character data is not tile aligned");
    }
    const std::size_t character_count = character_data.size() / 32;
    const std::size_t palette_offset = palette_id < 0
        ? 0 : checked_product(static_cast<std::size_t>(palette_id), 16,
                              source_name, "palette offset");
    std::vector<std::array<std::uint8_t, 64>> character_indices(character_count);
    for (std::size_t character_id = 0; character_id < character_count;
         ++character_id) {
        auto& character = character_indices[character_id];
        std::size_t output = 0;
        for (int y = 0; y < 8; ++y) {
            for (int x = 0; x < 4; ++x) {
                const std::uint8_t packed = character_data[
                    character_id * 32 + static_cast<std::size_t>(y) * 4
                    + static_cast<std::size_t>(x)];
                for (const std::uint8_t index : {
                         static_cast<std::uint8_t>(packed & 0xf),
                         static_cast<std::uint8_t>((packed >> 4) & 0xf)}) {
                    character[output++] = index;
                }
            }
        }
    }

    if (screen_bytes.size() / 2
        > std::numeric_limits<std::size_t>::max() / 2) {
        invalid(source_name, "screen data is too large");
    }
    std::vector<ScreenData> screen_data;
    screen_data.reserve(screen_bytes.size() / 2);
    for (std::size_t i = 0; i < screen_bytes.size(); i += 2) {
        screen_data.push_back(ScreenData{
            static_cast<std::uint16_t>(screen_bytes[i]
                | (static_cast<std::uint16_t>(screen_bytes[i + 1]) << 8))});
    }

    if (palette_id >= 0) {
        screen_data.insert(screen_data.begin(), 32 * 10, ScreenData{});
        if (palette_id == 4) {
            const std::size_t end = std::min<std::size_t>(
                screen_data.size(), 17 * 32);
            for (std::size_t i = 10 * 32; i < end; ++i) {
                screen_data[i] = ScreenData{};
            }
        } else {
            const std::size_t end = std::min<std::size_t>(
                screen_data.size(), 13 * 32);
            for (std::size_t i = 10 * 32; i < end; ++i) {
                const int character_id = screen_data[i].character_id();
                if (character_id == 1 || character_id == 2
                    || character_id == 5) {
                    screen_data[i] = ScreenData{};
                }
            }
        }
        const auto set_screen = [&screen_data](std::size_t index,
                                                std::uint16_t value) {
            if (index < screen_data.size()) {
                screen_data[index] = ScreenData{value};
            }
        };
        for (std::size_t i = 16 * 32; i < 17 * 32; ++i) {
            set_screen(i, 13);
        }
        for (std::size_t i = 17 * 32; i < 21 * 32; ++i) {
            set_screen(i, 15);
        }
        for (std::size_t i = 21 * 32; i < 21 * 32 + 4; ++i) {
            set_screen(i, 21);
        }
        for (std::size_t i = 21 * 32 + 28; i < 22 * 32; ++i) {
            set_screen(i, 21);
        }
    }

    if (tiles_x == 0) {
        tiles_x = chars_x;
    }
    if (tiles_y == 0) {
        tiles_y = chars_y;
    }
    if (tiles_x <= 0 || tiles_y <= 0) {
        invalid(source_name, "requested character-map dimensions are empty");
    }
    const std::size_t width = checked_product(
        static_cast<std::size_t>(tiles_x), 8, source_name, "texture width");
    const std::size_t height = checked_product(
        static_cast<std::size_t>(tiles_y), 8, source_name, "texture height");
    const std::size_t pixel_count = checked_product(
        width, height, source_name, "texture dimensions");
    if (width > static_cast<std::size_t>(std::numeric_limits<int>::max())
        || height > static_cast<std::size_t>(std::numeric_limits<int>::max())) {
        invalid(source_name, "texture dimensions do not fit an integer");
    }
    HudTexture result;
    result.width = static_cast<int>(width);
    result.height = static_cast<int>(height);
    result.pixels.assign(pixel_count, ColorRgba{});
    result.palette = raw_palette;
    const int address_width = chars_x > 32 ? chars_x / 2 : chars_x;
    if (address_width <= 0) {
        invalid(source_name, "screen-map address width is empty");
    }
    for (int cy = 0; cy < tiles_y; ++cy) {
        const int icy = cy + start_y;
        for (int cx = 0; cx < tiles_x; ++cx) {
            const int icx = cx + start_x;
            const int idx = icy * address_width
                + ((icx / 32) == 1 ? 0x400 + (icx - 32) : icx);
            if (icy < 0 || icx < 0 || idx < 0
                || static_cast<std::size_t>(idx) >= screen_data.size()) {
                invalid(source_name, "requested tile is outside screen data");
            }
            const ScreenData screen = screen_data[static_cast<std::size_t>(idx)];
            const int character_id = screen.character_id();
            if (character_id < 0
                || static_cast<std::size_t>(character_id)
                    >= character_indices.size()) {
                invalid(source_name, "screen data references a missing character");
            }
            const auto& character = character_indices[static_cast<std::size_t>(
                character_id)];
            const std::size_t screen_palette_offset = palette_id >= 0
                ? palette_offset
                : checked_product(static_cast<std::size_t>(screen.palette_id()),
                                  16, source_name, "screen palette offset");
            for (int py = 0; py < 8; ++py) {
                const int iy = screen.flip_vertical() ? 7 - py : py;
                for (int px = 0; px < 8; ++px) {
                    const int ix = screen.flip_horizontal() ? 7 - px : px;
                    const std::size_t destination = static_cast<std::size_t>(
                        cy * 8 + py) * width
                        + static_cast<std::size_t>(cx * 8 + px);
                    const std::uint8_t source_index = character[
                        static_cast<std::size_t>(iy * 8 + ix)];
                    if (source_index == 0
                        || (palette_id >= 0 && source_index == 6)) {
                        result.pixels[destination] = ColorRgba{};
                    } else {
                        const std::size_t palette_index = screen_palette_offset
                            + source_index;
                        if (palette_index >= raw_palette.size()) {
                            invalid(source_name,
                                    "screen palette bank is missing a color");
                        }
                        result.pixels[destination] = ColorRgba::from_bgr555(
                            raw_palette[palette_index]);
                    }
                }
            }
        }
    }
    return result;
}

} // namespace MphReadNative::Hud
