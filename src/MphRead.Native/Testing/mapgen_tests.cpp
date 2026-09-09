#include "Formats/collision_format.hpp"
#include "Formats/collision_query.hpp"
#include "Formats/entity_format.hpp"
#include "Mods/MapGen/mapgen.hpp"
#include "Mods/MapGen/map_report.hpp"
#include "Formats/model_format.hpp"

#include "../Mods/MapGen/map_bundle.hpp"

#include <cassert>
#if defined(_WIN32)
#include <process.h>
#else
#include <unistd.h>
#endif
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>

namespace {

std::filesystem::path arena_path() {
    std::filesystem::path current = std::filesystem::current_path();
    for (;;) {
        const auto candidate = current / "maps" / "arena" / "arena.json";
        std::error_code error;
        if (std::filesystem::is_regular_file(candidate, error) && !error) {
            return candidate;
        }
        const auto parent = current.parent_path();
        if (parent == current) {
            break;
        }
        current = parent;
    }
    throw std::runtime_error("could not find maps/arena/arena.json from test cwd");
}

std::filesystem::path repository_path(std::string_view relative) {
    std::filesystem::path current = std::filesystem::current_path();
    for (;;) {
        const auto candidate = current / std::filesystem::path(relative);
        std::error_code error;
        if (std::filesystem::is_regular_file(candidate, error) && !error) {
            return candidate;
        }
        const auto parent = current.parent_path();
        if (parent == current) {
            break;
        }
        current = parent;
    }
    throw std::runtime_error("could not find repository file: "
                             + std::string(relative));
}

void write_synthetic_texture_pack(const std::filesystem::path& path) {
    // Use a broad source-index range so the fixture covers the importer
    // lookup without depending on a particular Q3 shader ordering. Each
    // entry is deliberately the smallest valid palette-indexed texture.
    constexpr std::uint16_t count = 1024;
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    if (!output) {
        throw std::runtime_error("could not create synthetic FPTX fixture");
    }
    const auto write_u16 = [&](std::uint16_t value) {
        const char bytes[2] = {
            static_cast<char>(value & 0xff),
            static_cast<char>((value >> 8) & 0xff)
        };
        output.write(bytes, sizeof(bytes));
    };
    output.write("FPTX", 4);
    write_u16(1);
    write_u16(count);
    for (std::uint16_t index = 0; index < count; ++index) {
        write_u16(index);
        write_u16(1);
        write_u16(1);
        write_u16(1);
        write_u16(1);
        output.put('x');
        write_u16(0x7fff);
        output.put('\0');
    }
    if (!output) {
        throw std::runtime_error("could not write synthetic FPTX fixture");
    }
}

std::string process_id_string() {
#if defined(_WIN32)
    return std::to_string(static_cast<unsigned long long>(_getpid()));
#else
    return std::to_string(static_cast<unsigned long long>(getpid()));
#endif
}

} // namespace

int main() {
    try {
        const auto definition = fruityprime::mapgen::load_definition(arena_path());
        assert(definition.name == "TEST ARENA");
        assert(definition.materials.size() == 2);
        assert(definition.brushes.size() == 7);
        assert(definition.spawns.size() == 8);
        assert(definition.items.size() == 4);
        assert(definition.jump_pads.empty());
        assert(fruityprime::mapgen::item_type_from_name("uasmall") == 13);
        assert(fruityprime::mapgen::item_type_from_name("HealthMedium") == 0);
        assert(fruityprime::mapgen::item_type_from_name("EnergyTank") < 0);

        const auto generated = fruityprime::mapgen::build(definition);
        assert(generated.stats.model_faces == 42);
        assert(generated.stats.model_vertices == 168);
        assert(generated.stats.collision_faces == 42);
        assert(generated.stats.entities == 12);
        assert(generated.stats.navigation_nodes > 0);
        assert(generated.stats.navigation_edges > 0);
        assert(generated.animation.size() == 24);

        const auto model = fruityprime::model::File::from_bytes(generated.model);
        assert(model.header().scale_factor == 4);
        assert(model.materials().size() == 2);
        assert(model.meshes().size() == 2);
        std::size_t primitives = 0;
        for (const auto& mesh : model.meshes()) {
            const auto geometry = model.decode_geometry(mesh.display_list_id);
            primitives += geometry.size();
            assert(!geometry.empty());
            for (const auto& primitive : geometry) {
                assert(primitive.vertices.size() == 4);
            }
        }
        assert(primitives == 42);

        const auto collision = fruityprime::collision::File::from_bytes(
            generated.collision);
        assert(collision.is_mph());
        assert(collision.mph().points.size() == generated.stats.collision_points);
        assert(collision.mph().entries.size() > 1);
        const auto floor_hit = fruityprime::collision::sweep_sphere(
            collision, {10.0F, 2.0F, 0.0F}, {10.0F, -2.0F, 0.0F}, 0.1F);
        assert(floor_hit.has_value());
        assert(floor_hit->contact.y > -0.01F && floor_hit->contact.y < 0.01F);

        const auto entities = fruityprime::entity::File::from_bytes(
            generated.entities);
        assert(!entities.is_first_hunt());
        assert(entities.records().size() == generated.stats.entities);
        std::size_t player_spawns = 0;
        std::size_t item_spawns = 0;
        for (const auto& record : entities.records()) {
            if (record.header.type == 2) {
                ++player_spawns;
                assert(record.payload.size() == 43);
            } else if (record.header.type == 4) {
                ++item_spawns;
                assert(record.payload.size() == 72);
            }
        }
        assert(player_spawns == 8 && item_spawns == 4);

        assert(generated.nodes.size() > 32);
        assert(generated.nodes[0] == 6 && generated.nodes[1] == 0);

        // Keep one real PK3 in the native contract. This exercises the ZIP
        // deflate path, BSP lump bounds checks, Q3 coordinate conversion,
        // brush-side clipping/buried-face filtering, and entity translation;
        // the arena above remains the small deterministic writer fixture.
        const auto q3_recipe = repository_path("maps/dust2/dust2.json");
        const auto q3_definition = fruityprime::mapgen::load_definition(q3_recipe);
        assert(q3_definition.import_source == "df_dust2.pk3");
        assert(q3_definition.import_map_name == "df_dust2");
        assert(q3_definition.import_units_per_unit == 82.0F);
        const auto q3_generated = fruityprime::mapgen::build(q3_definition);
        assert(q3_generated.stats.model_faces > 1000);
        assert(q3_generated.stats.collision_faces > 1000);
        assert(q3_generated.stats.entities >= 10);
        assert(q3_generated.stats.navigation_nodes > 0);
        const auto q3_model = fruityprime::model::File::from_bytes(q3_generated.model);
        const auto q3_collision = fruityprime::collision::File::from_bytes(
            q3_generated.collision);
        const auto q3_entities = fruityprime::entity::File::from_bytes(
            q3_generated.entities);
        assert(q3_model.header().primitive_count > 1000);
        assert(q3_collision.is_mph());
        assert(q3_collision.mph().data.size() == q3_generated.stats.collision_faces);
        assert(q3_entities.records().size() == q3_generated.stats.entities);

        std::ostringstream shader_report;
        assert(fruityprime::mapgen::list_shaders(
                   shader_report, repository_path("maps/dust2/df_dust2.pk3"),
                   "df_dust2") == 0);
        assert(shader_report.str().find("21 shaders drawn")
            != std::string::npos);
        assert(shader_report.str().find("textures/dust2/-0CSSANDWALL")
            != std::string::npos);
#ifdef _WIN32
        assert(q3_model.header().texture_count > 0);
        assert(q3_model.header().palette_count == q3_model.header().texture_count);
        assert(!q3_model.decode_texture(0).empty());
#endif

        // Exercise the baked-texture branch without checking in a generated
        // .tex beside the real map. The source path stays absolute while the
        // pack is resolved relative to a temporary recipe, matching the
        // desktop and bundled-map lookup contract.
        const auto texture_test_dir = std::filesystem::temp_directory_path()
            / ("fruity-prime-native-mapgen-texture-test-" + process_id_string());
        std::error_code cleanup_error;
        std::filesystem::remove_all(texture_test_dir, cleanup_error);
        std::filesystem::create_directories(texture_test_dir);
        const auto texture_pack_path = texture_test_dir / "synthetic.tex";
        write_synthetic_texture_pack(texture_pack_path);
        auto packed_definition = q3_definition;
        packed_definition.source_path = texture_test_dir / "synthetic.json";
        packed_definition.import_source = std::filesystem::absolute(
            repository_path("maps/dust2/df_dust2.pk3")).string();
        packed_definition.import_textures = texture_pack_path.filename().string();
        const auto packed_generated = fruityprime::mapgen::build(packed_definition);
        const auto packed_model = fruityprime::model::File::from_bytes(
            packed_generated.model);
        assert(packed_model.header().texture_count == 1024);
        assert(packed_model.header().palette_count == 1024);
        assert(packed_model.textures().size() == 1024);
        assert(packed_model.palettes().size() == 1024);
        assert(packed_model.decode_texture(0).size() == 1);
        assert(packed_generated.stats.model_faces > 1000);
        std::filesystem::remove_all(texture_test_dir, cleanup_error);

        // Cook the real Q3 recipe into the distributable .fpmap form and
        // immediately consume it again. This catches the complete contract:
        // ZIP raw-deflate, trimmed BSP, embedded FPTX, rewritten recipe paths,
        // and the same mapgen output when no source PK3 remains beside it.
        const auto bundle_test_dir = std::filesystem::temp_directory_path()
            / ("fruity-prime-native-mapbundle-test-" + process_id_string());
        std::filesystem::remove_all(bundle_test_dir, cleanup_error);
        std::filesystem::create_directories(bundle_test_dir);
        const auto bundle_path = bundle_test_dir / "dust2.fpmap";
        const auto cooked_path = fruityprime::mapgen::bundle::cook(
            q3_definition, q3_recipe, bundle_path, false);
        assert(cooked_path == bundle_path);
        assert(fruityprime::mapgen::bundle::is_bundle(bundle_path));
        const auto bundled_recipe =
            fruityprime::mapgen::bundle::read_recipe(bundle_path);
        assert(bundled_recipe.has_value());
        assert(bundled_recipe->find("\"source\": \"maps/df_dust2.bsp\"")
            != std::string::npos);
        const auto bundled_level = fruityprime::mapgen::bundle::read_entry(
            bundle_path, "maps/df_dust2.bsp");
        const auto bundled_textures = fruityprime::mapgen::bundle::read_entry(
            bundle_path, "dust2.tex");
        assert(bundled_level.has_value() && bundled_level->size() > 8 + 17 * 8);
        assert(bundled_textures.has_value() && bundled_textures->size() > 8);
        const auto bundled_definition = fruityprime::mapgen::load_definition(
            bundle_path);
        assert(bundled_definition.import_source == "maps/df_dust2.bsp");
        assert(bundled_definition.import_map_name == "df_dust2");
        assert(bundled_definition.import_textures == "dust2.tex");
        const auto bundled_generated = fruityprime::mapgen::build(
            bundled_definition);
        assert(bundled_generated.stats.model_faces == q3_generated.stats.model_faces);
        assert(bundled_generated.stats.collision_faces
            == q3_generated.stats.collision_faces);
        assert(bundled_generated.stats.entities == q3_generated.stats.entities);
        std::filesystem::remove_all(bundle_test_dir, cleanup_error);
        std::cout << "native mapgen tests passed: model_faces="
                  << generated.stats.model_faces
                  << " collision_faces=" << generated.stats.collision_faces
                  << " entities=" << generated.stats.entities
                  << " nodes=" << generated.stats.navigation_nodes
                  << " q3_model_faces=" << q3_generated.stats.model_faces
                  << " q3_collision_faces=" << q3_generated.stats.collision_faces
                  << '\n';
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "native mapgen tests failed: " << error.what() << '\n';
        return 1;
    }
}
