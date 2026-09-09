#include "Mods/MapGen/mapgen.hpp"

#include "Formats/collision_format.hpp"
#include "Formats/entity_format.hpp"
#include "Formats/fixed.hpp"
#include "Formats/model_format.hpp"
#include "Mods/MapGen/map_builder.hpp"
#include "Mods/MapGen/map_collision_packer.hpp"
#include "Mods/MapGen/map_node_packer.hpp"
#include "Mods/MapGen/map_packer.hpp"
#include "Mods/MapGen/q3_convert.hpp"
#include "Mods/MapGen/q3_import.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <limits>
#include <map>
#include <queue>
#include <span>
#include <stdexcept>
#include <string_view>
#include <utility>

namespace fruityprime::mapgen {


GeneratedMap build(const MapDefinition& definition) {
    MapDefinition effective = definition;
    std::vector<BuiltFace> all_faces;
    std::vector<BuiltFace> solid_faces;
    std::vector<detail::TexturePackEntry> imported_texture_pack;
    if (!definition.import_source.empty()) {
        const detail::ImportedMap imported = detail::import_q3(definition);
        imported_texture_pack = imported.texture_pack;
        if (!imported_texture_pack.empty()) {
            // A baked pack is ordered independently of the BSP's shader
            // indices. The importer has already assigned each face to that
            // order, so the generated material table must use it verbatim.
            effective.materials = imported.materials;
        } else if (effective.materials.empty()) {
            effective.materials = imported.materials;
        } else if (effective.materials.size() < imported.materials.size()) {
            effective.materials.insert(effective.materials.end(),
                                       imported.materials.begin()
                                           + static_cast<std::ptrdiff_t>(effective.materials.size()),
                                       imported.materials.end());
        }
        const auto convert_face = [](const detail::ImportedFace& source) {
            BuiltFace result;
            result.points = source.points;
            result.normal = source.normal;
            result.material = source.material;
            result.shade = source.shade;
            result.damaging = source.damaging;
            result.flags = source.flags;
            result.point_count = source.point_count;
            result.has_texcoords = source.has_texcoords;
            result.texcoords = source.texcoords;
            return result;
        };
        all_faces.reserve(imported.faces.size());
        for (const detail::ImportedFace& face : imported.faces) {
            all_faces.push_back(convert_face(face));
        }
        solid_faces.reserve(imported.solid_faces.size());
        for (const detail::ImportedFace& face : imported.solid_faces) {
            solid_faces.push_back(convert_face(face));
        }
        effective.spawns.insert(effective.spawns.end(), imported.spawns.begin(),
                                imported.spawns.end());
        effective.jump_pads.insert(effective.jump_pads.end(),
                                   imported.jump_pads.begin(), imported.jump_pads.end());
        effective.items.insert(effective.items.end(), imported.items.begin(),
                               imported.items.end());
    } else {
        const BuiltMap built = builder::build(effective);
        all_faces = built.faces;
        solid_faces = built.solid;
    }
    GeneratedMap result;
    const auto* texture_pack = imported_texture_pack.empty()
        ? nullptr : &imported_texture_pack;
    result.model = packer::build_model(effective, all_faces, texture_pack,
                                       result.stats);
    std::vector<map_collision::CollisionFace> collision_faces;
    collision_faces.reserve(solid_faces.size());
    for (const BuiltFace& face : solid_faces) {
        map_collision::CollisionFace collision_face;
        collision_face.points = face.points;
        collision_face.normal = face.normal;
        collision_face.flags = face.flags;
        collision_face.point_count = face.point_count;
        collision_faces.push_back(std::move(collision_face));
    }
    result.collision = map_collision::pack(collision_faces, result.stats);
    result.entities = packer::build_entities(effective, result.stats);
    std::vector<map_nodes::NavigationFace> navigation_faces;
    navigation_faces.reserve(solid_faces.size());
    for (const BuiltFace& face : solid_faces) {
        map_nodes::NavigationFace navigation_face;
        navigation_face.points = face.points;
        navigation_face.normal = face.normal;
        navigation_face.point_count = face.point_count;
        navigation_faces.push_back(std::move(navigation_face));
    }
    result.nodes = map_nodes::pack(navigation_faces, result.stats);
    result.animation.assign(24, 0);
    return result;
}

Q3ConvertResult convert_q3(const Q3ConvertOptions& options) {
    return detail::convert_q3_recipe(options);
}

void write_generated(const MapDefinition& definition,
                     const GeneratedMap& generated,
                     const std::filesystem::path& output_directory) {
    packer::write_generated(definition, generated, output_directory);
}

} // namespace fruityprime::mapgen
