#include "Entities/scene.hpp"
#include "Entities/room_catalog.hpp"
#include "Scene.hpp"

#include "../Mods/MapGen/custom_rooms.hpp"

#include <cassert>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <set>
#include <stdexcept>
#include <string>

namespace {

std::string environment_value(const char* name) {
    const char* value = std::getenv(name);
    return value == nullptr ? std::string{} : std::string(value);
}

} // namespace

int main() {
    const std::string rom_path = environment_value("FRUITY_PRIME_TEST_NDS");
    if (rom_path.empty()) {
        std::cout << "real scene test skipped: set FRUITY_PRIME_TEST_NDS\n";
        return 0;
    }

    try {
        // The standard ROM scene regression owns the cartridge catalog. Keep
        // repository-only custom maps out of this test; their external-file
        // load is covered by the dedicated maptest command.
        fruityprime::mapgen::custom_rooms::set_map_directory(
            std::filesystem::temp_directory_path()
                / "fruityprime-native-scene-no-custom-maps");
        const auto assets = fruityprime::assets::Store::from_rom(rom_path);
        const fruityprime::scene::RoomDefinition definition{
            "UNIT1_C0",
            "archives/unit1_C0.arc",
            "unit1_c0_model.bin",
            "levels/textures/unit1_c0_tex.bin",
            "unit1_c0_collision.bin",
            "levels/entities/Unit1_C0_Ent.bin", {}, {}, {}
        };
        const auto room = fruityprime::scene::Room::load(assets, definition);
        assert(room.model().header().mesh_count > 0);
        assert(room.model().meshes().size() == room.model().header().mesh_count);
        assert(room.entities().size() > 0);
        assert(room.has_entities());
        assert(room.collision().is_mph());
        std::size_t primitive_count = 0;
        std::size_t vertex_count = 0;
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
                }
            }
            const auto batches = room.model().decode_geometry(
                mesh.display_list_id, texture_width, texture_height, texgen);
            primitive_count += batches.size();
            for (const auto& batch : batches) {
                vertex_count += batch.vertices.size();
            }
        }
        if (primitive_count == 0 || vertex_count == 0) {
            throw std::runtime_error("real room model decoded no geometry");
        }
        std::set<std::pair<int, int>> texture_pairs;
        for (const auto& material : room.model().materials()) {
            if (material.texture_id < 0
                || static_cast<std::size_t>(material.texture_id)
                    >= room.model().textures().size()) {
                continue;
            }
            const auto pair = std::make_pair(material.texture_id,
                                             material.palette_id);
            if (!texture_pairs.insert(pair).second) {
                continue;
            }
            const auto pixels = room.model().decode_texture(material.texture_id);
            if (pixels.empty()) {
                throw std::runtime_error("real room texture decoded no pixels");
            }
            if (material.palette_id >= 0
                && static_cast<std::size_t>(material.palette_id)
                    < room.model().palettes().size()) {
                const auto palette = room.model().decode_palette(material.palette_id);
                if (palette.empty()) {
                    throw std::runtime_error("real room palette decoded no colors");
                }
            }
        }
        std::size_t catalog_rooms = 0;
        for (const auto& catalog_entry :
             fruityprime::scene::multiplayer_rooms()) {
            const auto catalog_room = fruityprime::scene::Room::load(
                assets, catalog_entry.definition);
            if (catalog_room.model().meshes().empty()
                || catalog_room.entities().empty()
                || !catalog_room.collision().is_mph()) {
                throw std::runtime_error(
                    "catalog room did not load all native resources: "
                    + catalog_entry.name);
            }
            ++catalog_rooms;
        }
        bool cross_room_teleporter_tested = false;
        for (const auto& source_entry : fruityprime::scene::story_rooms()) {
            if (cross_room_teleporter_tested) {
                break;
            }
            fruityprime::gameplay::Config transition_config;
            transition_config.mode = static_cast<std::uint8_t>(
                fruityprime::game::Mode::Story);
            transition_config.gravity = 0.0F;
            transition_config.walk_speed = 0.0F;
            transition_config.air_acceleration = 0.0F;
            transition_config.world_padding = 0.0F;
            fruityprime::scene_runtime::Scene transition_scene(assets);
            if (!transition_scene.load_room(source_entry.name,
                                             transition_config)
                || transition_scene.session() == nullptr) {
                continue;
            }
            for (const auto& entity : transition_scene.room()->entities()) {
                if (entity.kind != fruityprime::scene::EntityKind::Teleporter) {
                    continue;
                }
                const auto* teleporter = std::get_if<
                    fruityprime::scene::TeleporterData>(&entity.typed_data);
                if (teleporter == nullptr || !teleporter->active
                    || teleporter->entity_filename.empty()) {
                    continue;
                }
                const auto target_index = teleporter->target_index;
                auto* transition_session = transition_scene.session();
                static_cast<void>(transition_session->add_player(0, 0));
                auto setup = transition_session->snapshot(0);
                setup.players[0].position = {
                    entity.position.x.to_float() + 8.0F,
                    entity.position.y.to_float(),
                    entity.position.z.to_float()};
                transition_session->apply_snapshot(setup);
                transition_session->set_input(0,
                    fruityprime::gameplay::Input{});
                transition_scene.tick();
                setup = transition_session->snapshot(0);
                setup.players[0].position = {
                    entity.position.x.to_float(),
                    entity.position.y.to_float(),
                    entity.position.z.to_float()};
                transition_session->apply_snapshot(setup);
                transition_scene.tick();
                if (transition_scene.room() == nullptr
                    || transition_scene.room()->definition().name
                        == source_entry.definition.name) {
                    continue;
                }
                const auto target = std::find_if(
                    transition_scene.room()->entities().begin(),
                    transition_scene.room()->entities().end(),
                    [target_index](const auto& candidate) {
                        if (candidate.kind
                            != fruityprime::scene::EntityKind::Teleporter) {
                            return false;
                        }
                        const auto* data = std::get_if<
                            fruityprime::scene::TeleporterData>(
                                &candidate.typed_data);
                        return data != nullptr
                            && data->load_index == target_index;
                    });
                if (target == transition_scene.room()->entities().end()
                    || transition_scene.session() == nullptr) {
                    throw std::runtime_error(
                        "cross-room teleporter did not resolve its load entity");
                }
                const auto arrived = transition_scene.session()->player(0);
                if (std::fabs(arrived.position.x
                                  - target->position.x.to_float()) > 0.001F
                    || std::fabs(arrived.position.y
                                  - target->position.y.to_float() - 0.5F)
                        > 0.001F
                    || std::fabs(arrived.position.z
                                  - target->position.z.to_float()) > 0.001F) {
                    throw std::runtime_error(
                        "cross-room teleporter did not place the player at the load entity");
                }
                cross_room_teleporter_tested = true;
                break;
            }
        }
        if (!cross_room_teleporter_tested) {
            throw std::runtime_error(
                "real story room catalog has no cross-room teleporter test");
        }
        std::cout << "real room: " << room.definition().name
                  << " meshes=" << room.model().meshes().size()
                  << " entities=" << room.entities().size()
                  << " primitives=" << primitive_count
                  << " vertices=" << vertex_count
                  << " texture_pairs=" << texture_pairs.size()
                  << " catalog_rooms=" << catalog_rooms << '\n';
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "scene test failed: " << error.what() << '\n';
        return 1;
    }
}
