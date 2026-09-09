#include "Mods/Network/map_audit.hpp"

#include "Assets/game_assets.hpp"
#include "Entities/gameplay.hpp"
#include "Entities/match_flow.hpp"
#include "Mods/Network/map_rotation.hpp"
#include "Mods/mod_entry.hpp"
#include "Mods/Network/net_protocol.hpp"
#include "Entities/room_catalog.hpp"

#include "Entities/Players/PlayerAi.hpp"
#include "../MapGen/custom_rooms.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>

namespace fruityprime::map_audit {
namespace {
} // namespace

int run(int argc, char** argv) {
    const std::string room_name = mods::value_after(argc, argv, "-maptest");
    const std::string rom_path = mods::value_after(argc, argv, "-rom");
    if (room_name.empty() || rom_path.empty()) {
        throw std::invalid_argument("-maptest needs ROOM and -rom FILE");
    }

    // ModEntry generates missing custom-room binaries before every normal
    // launch. Do the same for -maptest so a recipe is sufficient and the
    // command does not depend on a previous manual -mapgen invocation.
    static_cast<void>(mapgen::custom_rooms::generate_missing());
    const auto* catalog_entry = scene::find_room(room_name);
    scene::RoomDefinition definition = catalog_entry != nullptr
        ? catalog_entry->definition
        : scene::RoomDefinition{
            room_name,
            "archives/unit1_C0.arc",
            "unit1_c0_model.bin",
            "levels/textures/unit1_c0_tex.bin",
            "unit1_c0_collision.bin",
            "levels/entities/Unit1_C0_Ent.bin", {}, {}, {}
        };
    const auto assets = assets::Store::from_rom(rom_path);
    const auto room = scene::Room::load(assets, definition);

    const int requested_players = std::clamp(
        mods::integer_after(argc, argv, "-players", 8), 1,
        static_cast<int>(net::NetConfig::SlotCapacity));
    int mode = mods::integer_after(argc, argv, "-mode", 3);
    mode = std::clamp(mode, 0, 255);
    float seconds = 10.0F;
    const std::string seconds_text = mods::value_after(
        argc, argv, "-seconds");
    if (!seconds_text.empty()) {
        try {
            seconds = std::stof(seconds_text);
        } catch (...) {
            throw std::invalid_argument("-seconds must be a number");
        }
        if (!std::isfinite(seconds) || seconds < 0.0F) {
            throw std::invalid_argument("-seconds must be finite and >= 0");
        }
    }

    gameplay::Config gameplay_config;
    gameplay_config.mode = static_cast<std::uint8_t>(mode);
    gameplay_config.point_goal = fruityprime::match::defaults_for_mode(
        static_cast<std::uint8_t>(mode)).point_goal;
    gameplay_config.team_mode = fruityprime::match::is_team_mode(
        static_cast<std::uint8_t>(mode));
    gameplay_config.survival_mode = mode == 5 || mode == 6;
    gameplay_config.survival_lives = fruityprime::match::defaults_for_mode(
        static_cast<std::uint8_t>(mode)).point_goal;
    gameplay::Session session(room, gameplay_config);
    for (int i = 0; i < requested_players; ++i) {
        static_cast<void>(session.add_player(
            static_cast<std::uint8_t>(i), static_cast<std::uint8_t>(i % 7)));
    }

    const bool drive_bots = mods::has_flag(argc, argv, "-bots");
    const int bot_level = std::clamp(
        mods::integer_after(argc, argv, "-bot-level", 1), 0, 2);
    const std::uint64_t ticks = static_cast<std::uint64_t>(std::ceil(
        seconds / gameplay_config.tick_seconds));
    for (std::uint64_t tick = 0; tick < ticks; ++tick) {
        const auto local_position = session.player(0).position;
        for (int i = 0; i < requested_players; ++i) {
            gameplay::Input input;
            if (i == 0) {
                const auto& player = session.player(static_cast<std::uint8_t>(i));
                input.buttons = net::IntentButtons::MoveUp;
                input.aim = {
                    local_position.x - player.position.x,
                    local_position.y - player.position.y,
                    local_position.z - player.position.z
                };
            } else if (drive_bots) {
                input = players::BotAi::decide(
                    session, static_cast<std::uint8_t>(i), bot_level).input;
            }
            session.set_input(static_cast<std::uint8_t>(i), input);
        }
        session.tick();
    }

    std::size_t primitives = 0;
    std::size_t vertices = 0;
    std::size_t textured_materials = 0;
    if (mods::has_flag(argc, argv, "-renderprobe")) {
        for (const auto& mesh : room.model().meshes()) {
            int texture_width = 0;
            int texture_height = 0;
            bool texgen = false;
            if (mesh.material_id < room.model().materials().size()) {
                const auto& material = room.model().materials()[mesh.material_id];
                texgen = material.texcoord_transform_mode == 2;
                if (material.texture_id >= 0
                    && static_cast<std::size_t>(material.texture_id)
                        < room.model().textures().size()) {
                    const auto& texture = room.model().textures()[material.texture_id];
                    texture_width = texture.width;
                    texture_height = texture.height;
                    if (!room.model().decode_texture(material.texture_id).empty()) {
                        ++textured_materials;
                    }
                    if (material.palette_id >= 0
                        && static_cast<std::size_t>(material.palette_id)
                            < room.model().palettes().size()
                        && room.model().decode_palette(material.palette_id).empty()) {
                        throw std::runtime_error(
                            "-maptest render probe decoded an empty palette");
                    }
                }
            }
            for (const auto& primitive : room.model().decode_geometry(
                     mesh.display_list_id, texture_width, texture_height, texgen)) {
                ++primitives;
                vertices += primitive.vertices.size();
            }
        }
    }

    std::size_t alive = 0;
    for (const auto& player : session.players()) {
        if (player.health > 0
            && (player.flags & net::PlayerState::FlagActive) != 0) {
            ++alive;
        }
    }
    std::cout << "maptest room=\"" << room.definition().name << '"'
              << " players=" << session.players().size()
              << " alive=" << alive
              << " ticks=" << session.tick_count()
              << " projectiles=" << session.projectiles().size()
              << " items=" << session.items().size()
              << " entities=" << room.entities().size()
              << " collision=" << (room.collision().is_mph() ? "MPH" : "FirstHunt");
    if (mods::has_flag(argc, argv, "-renderprobe")) {
        std::cout << " primitives=" << primitives
                  << " vertices=" << vertices
                  << " textured_materials=" << textured_materials;
    }
    std::cout << '\n';
    return 0;
}

} // namespace fruityprime::map_audit
