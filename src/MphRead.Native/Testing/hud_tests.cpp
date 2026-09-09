#include "HUD/hud.hpp"

#include "Assets/game_assets.hpp"
#include "Mods/Render/pro_hud.hpp"

#include <algorithm>
#include <array>
#include <cassert>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {

using MphReadNative::Hud::ColorRgba;
using MphReadNative::Hud::decode_char_map;
using MphReadNative::Hud::parse_object;

void put_u16_le(std::vector<std::uint8_t>& bytes, std::size_t offset,
                std::uint16_t value) {
    bytes[offset] = static_cast<std::uint8_t>(value & 0xff);
    bytes[offset + 1] = static_cast<std::uint8_t>(value >> 8);
}

void put_i32_le(std::vector<std::uint8_t>& bytes, std::size_t offset,
                std::int32_t value) {
    const auto raw = static_cast<std::uint32_t>(value);
    bytes[offset] = static_cast<std::uint8_t>(raw & 0xff);
    bytes[offset + 1] = static_cast<std::uint8_t>((raw >> 8) & 0xff);
    bytes[offset + 2] = static_cast<std::uint8_t>((raw >> 16) & 0xff);
    bytes[offset + 3] = static_cast<std::uint8_t>((raw >> 24) & 0xff);
}

void put_palette(std::vector<std::uint8_t>& bytes, std::size_t offset) {
    for (std::size_t i = 0; i < 16; ++i) {
        put_u16_le(bytes, offset + i * 2,
                   i == 1 ? static_cast<std::uint16_t>(0x001f) : 0);
    }
}

std::vector<std::uint8_t> object_fixture() {
    constexpr std::size_t header_size = 24;
    constexpr std::size_t param_size = 16;
    constexpr std::size_t attr_size = 8;
    constexpr std::size_t char_size = 32;
    constexpr std::size_t palette_size = 32;
    std::vector<std::uint8_t> bytes(header_size + param_size + attr_size
                                    + char_size + palette_size, 0);
    put_u16_le(bytes, 0, 1); // FrameCount.
    put_u16_le(bytes, 2, 1); // ImageCount.
    put_u16_le(bytes, 4, 8);
    put_u16_le(bytes, 6, 8);
    put_i32_le(bytes, 8, static_cast<std::int32_t>(param_size));
    put_i32_le(bytes, 12, static_cast<std::int32_t>(attr_size));
    put_i32_le(bytes, 16, static_cast<std::int32_t>(char_size));
    put_i32_le(bytes, 20, static_cast<std::int32_t>(palette_size));
    const std::size_t attr_offset = header_size + param_size;
    put_u16_le(bytes, attr_offset, 0); // square/tiny normal 16-colour OAM.
    put_u16_le(bytes, attr_offset + 2, 0);
    put_u16_le(bytes, attr_offset + 4, 0);
    const std::size_t char_offset = attr_offset + attr_size;
    bytes[char_offset] = 0x10; // pixel 0 transparent, pixel 1 palette index 1.
    for (std::size_t i = 1; i < char_size; ++i) {
        bytes[char_offset + i] = 0x11;
    }
    put_palette(bytes, char_offset + char_size);
    return bytes;
}

std::vector<std::uint8_t> char_map_fixture() {
    constexpr std::size_t header_size = 12;
    constexpr std::size_t char_size = 32;
    constexpr std::size_t palette_size = 32;
    constexpr std::size_t screen_header_size = 8;
    std::vector<std::uint8_t> bytes(header_size + char_size + palette_size
                                    + screen_header_size + 2, 0);
    put_i32_le(bytes, 0, 0); // UiPartHeader.Magic.
    put_i32_le(bytes, 4, static_cast<std::int32_t>(char_size));
    put_i32_le(bytes, 8, static_cast<std::int32_t>(palette_size));
    const std::size_t char_offset = header_size;
    std::fill(bytes.begin() + static_cast<std::ptrdiff_t>(char_offset),
              bytes.begin() + static_cast<std::ptrdiff_t>(char_offset + char_size),
              static_cast<std::uint8_t>(0x11));
    put_palette(bytes, char_offset + char_size);
    const std::size_t screen_offset = char_offset + char_size + palette_size;
    put_u16_le(bytes, screen_offset, 1);
    put_u16_le(bytes, screen_offset + 2, 1);
    put_i32_le(bytes, screen_offset + 4, 2);
    put_u16_le(bytes, screen_offset + screen_header_size, 0);
    return bytes;
}

void test_object_fixture() {
    const auto object = parse_object(object_fixture(), "object-fixture");
    assert(object.frame_count == 1);
    assert(object.image_count == 1);
    assert(object.width == 8);
    assert(object.height == 8);
    assert(object.animation_params.size() == 1);
    assert(object.character_indices.size() == 64);
    assert(object.palette.size() == 16);
    const auto texture = object.render_frame();
    assert(texture.valid());
    assert(texture.pixels.front().alpha == 0);
    assert(texture.pixels[1].red == 255);
    assert(texture.pixels[1].alpha == 255);
}

void test_char_map_fixture() {
    const auto texture = decode_char_map(char_map_fixture(), 0, 0, 0, 0,
                                         {}, -1, "char-map-fixture");
    assert(texture.valid());
    assert(texture.width == 8);
    assert(texture.height == 8);
    assert(texture.palette.size() == 16);
    assert(texture.pixels.front().red == 255);
    assert(texture.pixels.front().alpha == 255);
}

void test_hud_layout_and_animation() {
    const auto& configured = MphReadNative::Hud::elements();
    assert(configured.hunter_objects.size() == 8);
    assert(configured.hunter_objects[0].reticle
           == "_archives/localSamus/hud_targetcircle.bin");
    assert(configured.hunter_objects[4].helmet
           == "_archives/localNox/bg_top.bin");
    assert(configured.rules[6].count == 8
           && configured.rules[6].message_ids[7] == 68);
    assert(configured.main_healthbars[0].length == 72
           && configured.main_healthbars[3].length == 152);

    std::vector<std::uint8_t> chars(8 * 8, 1);
    chars[0] = 0;
    std::vector<ColorRgba> palette(16);
    palette[1] = {255, 0, 0, 255};
    MphReadNative::Hud::ObjectInstance instance(8, 8);
    instance.set_data(chars, 0, palette, 0);
    assert(instance.rebuild_texture() && instance.texture[0].alpha == 0
           && instance.texture[1].red == 255);

    std::array<MphReadNative::Hud::UiAnimParams, 2> animation{{
        {0, 1, 0, 0, 0, 0, 0, 0},
        {1, 1, 0, 0, 0, 0, 0, 0}
    }};
    instance.set_animation_frames(animation);
    instance.set_animation(0, 1, 2, 1);
    instance.process_animation(1.0F / 15.0F);
    assert(instance.current_frame == 1);
}

void test_pro_hud_model() {
    MphReadNative::Hud::PlayerState player;
    fruityprime::gameplay::InventoryState inventory;
    fruityprime::game::State state;
    state.mode = fruityprime::game::Mode::Battle;
    state.single_player = false;
    state.point_goal = 7;
    state.points[0] = 3;
    state.player_teams[0] = 0;
    player.health = 98;
    player.health_max = 99;
    player.current_weapon = 1; // Missile, ten pool units per shot.
    inventory.ammo[1] = 100;

    const auto full = fruityprime::mods::render::pro_hud::build(
        player, inventory, state, 0, 99);
    assert(full.health_text == "98");
    assert(full.health_fraction == 1.0F);
    assert(full.health_tone
           == fruityprime::mods::render::pro_hud::Tone::Good);
    assert(full.ammo_visible && full.ammo_text == "10");
    assert(full.ammo_fraction == 1.0F);
    assert(full.ammo_tone
           == fruityprime::mods::render::pro_hud::Tone::Good);
    assert(full.score_message_id == 212);
    assert(full.score_label == "POINTS");
    assert(full.score_text == "3 / 7");
    assert(full.score_y == 12.0F);

    player.health = 59;
    auto warning = fruityprime::mods::render::pro_hud::build(
        player, inventory, state, 0, 99);
    assert(warning.health_tone
           == fruityprime::mods::render::pro_hud::Tone::Warning);
    player.health = 32;
    const auto danger = fruityprime::mods::render::pro_hud::build(
        player, inventory, state, 0, 99);
    assert(danger.health_tone
           == fruityprime::mods::render::pro_hud::Tone::Danger);

    inventory.ammo[1] = 15;
    const auto low_ammo = fruityprime::mods::render::pro_hud::build(
        player, inventory, state, 0, 99);
    assert(low_ammo.ammo_text == "1");
    assert(low_ammo.ammo_tone
           == fruityprime::mods::render::pro_hud::Tone::Danger);
    inventory.infinite_ammo = true;
    const auto unlimited = fruityprime::mods::render::pro_hud::build(
        player, inventory, state, 0, 99);
    assert(unlimited.ammo_text == "--"
           && unlimited.ammo_fraction == 1.0F
           && unlimited.ammo_tone
                  == fruityprime::mods::render::pro_hud::Tone::Good);
    inventory.infinite_ammo = false;

    player.alt_form = true;
    const auto alt = fruityprime::mods::render::pro_hud::build(
        player, inventory, state, 0, 99);
    assert(!alt.ammo_visible);
    player.alt_form = false;

    state.teams = true;
    state.player_teams[0] = 1;
    state.team_points[1] = 5;
    const auto team_score = fruityprime::mods::render::pro_hud::build(
        player, inventory, state, 0, 99);
    assert(team_score.score_text == "5 / 7");

    state.mode = fruityprime::game::Mode::SurvivalTeams;
    state.point_goal = 2;
    state.team_deaths[1] = 1;
    const auto survival = fruityprime::mods::render::pro_hud::build(
        player, inventory, state, 0, 99);
    assert(survival.score_message_id == 213
           && survival.score_label == "LIVES LEFT"
           && survival.score_text == "1");

    state.mode = fruityprime::game::Mode::Defender;
    state.player_time[0] = 65.0F;
    state.time_goal = 90.0F;
    const auto defender = fruityprime::mods::render::pro_hud::build(
        player, inventory, state, 0, 99);
    assert(defender.score_message_id == 217
           && defender.score_label == "RING TIME"
           && defender.score_text == "1:05/1:30");

    state.mode = fruityprime::game::Mode::SinglePlayer;
    state.single_player = true;
    inventory.health_max = 200;
    player.health = 100;
    const auto story = fruityprime::mods::render::pro_hud::build(
        player, inventory, state, 0, 99);
    assert(story.score_text == " " && story.score_message_id == 212);
    assert(story.health_fraction == 0.5F);

    const auto good = fruityprime::mods::render::pro_hud::tone_color(
        fruityprime::mods::render::pro_hud::Tone::Good);
    assert(good.red == 0.24F && good.green == 0.85F
           && good.blue == 0.32F);
}

void test_real_rom() {
    const char* rom_value = std::getenv("FRUITY_PRIME_TEST_NDS");
    if (rom_value == nullptr || rom_value[0] == '\0') {
        std::cout << "HUD ROM test skipped: set FRUITY_PRIME_TEST_NDS\n";
        return;
    }
    const auto assets = fruityprime::assets::Store::from_rom(rom_value);
    struct HunterPaths {
        const char* local;
        const char* hud;
    };
    constexpr std::array<HunterPaths, 7> paths{{
        {"Samus", "samus"}, {"Kanden", "kanden"}, {"Trace", "trace"},
        {"Sylux", "sylux"}, {"Nox", "nox"}, {"Spire", "spire"},
        {"Weavel", "weavel"}
    }};
    for (const auto& path : paths) {
        const std::string reticle_path = "_archives/local"
            + std::string(path.local) + "/hud_targetcircle.bin";
        const auto object = parse_object(assets.bytes(reticle_path), reticle_path);
        const auto reticle = object.render_frame();
        if (!reticle.valid()) {
            throw std::runtime_error("HUD reticle did not decode: "
                                     + reticle_path);
        }
        const std::string background_path = "hud/"
            + std::string(path.hud) + "/bg_bottom.bin";
        const auto background = decode_char_map(
            assets.bytes(background_path), 0, 0, 0, 0, {}, -1,
            background_path);
        if (!background.valid()) {
            throw std::runtime_error("HUD background did not decode: "
                                     + background_path);
        }
        const auto load_layer = [&assets, &path](const char* name) {
            const std::string layer_path = "_archives/local"
                + std::string(path.local) + "/" + name;
            const auto layer = decode_char_map(
                assets.bytes(layer_path), 0, 0, 0, 0, {}, -1, layer_path);
            if (!layer.valid()) {
                throw std::runtime_error("HUD layer did not decode: "
                                         + layer_path);
            }
            return std::pair<std::string, MphReadNative::Hud::HudTexture>{
                layer_path, layer};
        };
        const auto helmet = load_layer("bg_top.bin");
        const auto helmet_drop = load_layer("bg_top_drop.bin");
        const auto visor = load_layer("bg_top_ovl.bin");
        // These are the exact sub-rectangles used by PlayerHud.SetUpHud.
        const auto visor_slice = decode_char_map(
            assets.bytes(visor.first), 0, 0, 0, 32, {}, -1,
            visor.first + " combat slice");
        const std::string scan_path = "_archives/localSamus/bg_top_ovl.bin";
        const auto scan_slice = decode_char_map(
            assets.bytes(scan_path), 0, 96, 0, 32, {}, -1,
            scan_path + " scan slice");
        if (!visor_slice.valid() || !scan_slice.valid()) {
            throw std::runtime_error("HUD visor slices did not decode: "
                                     + visor.first);
        }
        const auto load_object = [&assets, &path](const char* name) {
            const std::string object_path = "_archives/local"
                + std::string(path.local) + "/" + name;
            const auto object = parse_object(
                assets.bytes(object_path), object_path);
            if (object.character_indices.empty()
                || object.palette.empty() || object.frame_count == 0) {
                throw std::runtime_error("HUD object is incomplete: "
                                         + object_path);
            }
            return std::pair<std::string, MphReadNative::Hud::HudObjectAsset>{
                object_path, object};
        };
        const auto health_bar = load_object("hud_energybar.bin");
        const auto health_bar_sub = load_object("hud_energybar2.bin");
        const auto ammo_bar = load_object("hud_ammobar.bin");
        const auto weapon_icon = load_object("hud_weaponicon.bin");
        const auto radial_ammo = load_object("rad_ammobar.bin");
        const auto validate_object_frames = [](const auto& named_object) {
            const auto& object = named_object.second;
            const std::size_t palette_count = std::max<std::size_t>(
                1, object.palette.size() / 16);
            for (std::size_t palette = 0; palette < palette_count; ++palette) {
                for (std::size_t image = 0; image < object.image_count;
                     ++image) {
                    if (!object.render_frame(image, palette).valid()) {
                        throw std::runtime_error(
                            "HUD object frame did not decode: "
                            + named_object.first);
                    }
                }
            }
        };
        validate_object_frames(health_bar);
        validate_object_frames(health_bar_sub);
        validate_object_frames(ammo_bar);
        validate_object_frames(weapon_icon);
        std::cout << path.local << " HUD: reticle " << reticle.width << 'x'
                  << reticle.height << ", background " << background.width
                  << 'x' << background.height << ", helmet " << helmet.second.width
                  << 'x' << helmet.second.height << ", helmet-drop "
                  << helmet_drop.second.width << 'x' << helmet_drop.second.height
                  << ", visor " << visor.second.width << 'x' << visor.second.height
                  << ", scan " << scan_slice.width << 'x' << scan_slice.height
                  << ", bars " << health_bar.second.width << 'x'
                  << health_bar.second.height << "/"
                  << health_bar_sub.second.width << 'x'
                  << health_bar_sub.second.height << ", ammo "
                  << ammo_bar.second.width << 'x' << ammo_bar.second.height
                  << ", weapon " << weapon_icon.second.width << 'x'
                  << weapon_icon.second.height << ", radial "
                  << radial_ammo.second.width << 'x'
                  << radial_ammo.second.height
                  << " frames " << health_bar.second.frame_count << '/'
                  << health_bar.second.image_count << ", weapon-frames "
                  << weapon_icon.second.frame_count << '/'
                  << weapon_icon.second.image_count
                  << '\n';
    }
    const auto ice = decode_char_map(
        assets.bytes("_archives/common/bg_ice.bin"), 16, 0, 32, 32, {}, -1,
        "_archives/common/bg_ice.bin");
    if (!ice.valid() || ice.width != 256 || ice.height != 256) {
        throw std::runtime_error("HUD ice layer did not decode at 256x256");
    }
}

} // namespace

int main() {
    try {
        test_object_fixture();
        test_char_map_fixture();
        test_hud_layout_and_animation();
        test_pro_hud_model();
        test_real_rom();
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "HUD test failed: " << error.what() << '\n';
        return 1;
    }
}
