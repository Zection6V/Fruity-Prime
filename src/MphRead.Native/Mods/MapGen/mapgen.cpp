#include "Mods/MapGen/mapgen.hpp"

#include "Formats/collision_format.hpp"
#include "Formats/entity_format.hpp"
#include "Formats/fixed.hpp"
#include "Formats/model_format.hpp"
#include "Formats/paths.hpp"
#include "Assets/game_assets.hpp"
#include "Entities/room_catalog.hpp"
#include "Entities/scene.hpp"
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
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace fruityprime::mapgen {
namespace {

[[nodiscard]] int round_to_even(double value) {
    if (!std::isfinite(value)) {
        throw std::runtime_error("map texture alpha is not finite");
    }
    const double lower = std::floor(value);
    const double fraction = value - lower;
    if (fraction < 0.5) {
        return static_cast<int>(lower);
    }
    if (fraction > 0.5) {
        return static_cast<int>(lower + 1.0);
    }
    const auto integer = static_cast<long long>(lower);
    return static_cast<int>((integer & 1LL) == 0 ? integer : integer + 1);
}

[[nodiscard]] std::uint8_t alpha_from_a5i3(std::uint8_t value) {
    const float alpha = static_cast<float>(value >> 3) / 31.0F * 255.0F;
    return static_cast<std::uint8_t>(round_to_even(alpha));
}

[[nodiscard]] std::uint8_t alpha_from_a3i5(std::uint8_t value) {
    const float alpha = static_cast<float>(value >> 5) / 7.0F * 255.0F;
    return static_cast<std::uint8_t>(round_to_even(alpha));
}

[[nodiscard]] std::vector<model::TexturePixel> texture_data_for_repack(
    const model::File& source, std::size_t texture_id) {
    const model::Texture& texture = source.textures().at(texture_id);
    const std::size_t pixel_count = static_cast<std::size_t>(texture.width)
        * texture.height;
    const std::size_t entries_per_byte = texture.format == 0 ? 4
        : texture.format == 1 ? 2 : 1;
    if (pixel_count % entries_per_byte != 0) {
        throw std::runtime_error("Pixel count " + std::to_string(pixel_count)
                                 + " is not divisible by "
                                 + std::to_string(entries_per_byte) + ".");
    }
    const std::size_t encoded_count = pixel_count / entries_per_byte;
    const std::size_t bytes_per_entry = texture.format == 5 ? 2 : 1;
    if (encoded_count > std::numeric_limits<std::size_t>::max()
            / bytes_per_entry) {
        throw std::runtime_error("model texture size overflows");
    }
    const std::size_t required = encoded_count * bytes_per_entry;
    const std::vector<std::uint8_t> bytes = source.read_texture_data(texture_id);
    if (bytes.size() < required) {
        throw std::runtime_error("model texture image is outside the file");
    }

    std::vector<model::TexturePixel> result;
    result.reserve(pixel_count);
    if (texture.format == 5) {
        for (std::size_t index = 0; index < pixel_count; ++index) {
            const std::uint16_t color = static_cast<std::uint16_t>(bytes[index * 2])
                | static_cast<std::uint16_t>(bytes[index * 2 + 1]) << 8;
            result.push_back({color, static_cast<std::uint8_t>(
                (color & 0x8000U) == 0 ? 0 : 255)});
        }
        return result;
    }

    for (std::size_t encoded = 0; encoded < encoded_count; ++encoded) {
        const std::uint8_t entry = bytes[encoded];
        for (std::size_t entry_index = 0;
             entry_index < entries_per_byte; ++entry_index) {
            const std::size_t shift = ((encoded * entries_per_byte
                                        + entry_index) % entries_per_byte)
                * (8 / entries_per_byte);
            std::uint32_t index = entry >> shift;
            std::uint8_t alpha = 255;
            if (texture.format == 0) {
                index &= 0x3U;
            } else if (texture.format == 1) {
                index &= 0xfU;
            } else if (texture.format == 4) {
                index &= 0x7U;
                alpha = alpha_from_a5i3(entry);
            } else if (texture.format == 6) {
                index &= 0x1fU;
                alpha = alpha_from_a3i5(entry);
            }
            if ((texture.format == 0 || texture.format == 1
                 || texture.format == 2)
                && texture.opaque == 0 && index == 0) {
                alpha = 0;
            }
            result.push_back({index, alpha});
        }
    }
    return result;
}

[[nodiscard]] packer::TextureInfo make_source_texture(
    const model::File& source, std::size_t texture_id) {
    const model::Texture& texture = source.textures().at(texture_id);
    const auto data = texture_data_for_repack(source, texture_id);
    packer::TextureInfo result;
    result.format = texture.format;
    result.opaque = texture.opaque != 0;
    result.width = texture.width;
    result.height = texture.height;

    if (texture.format == 5) {
        result.data.reserve(data.size() * 2);
        for (const model::TexturePixel& entry : data) {
            result.data.push_back(static_cast<std::uint8_t>(entry.data));
            result.data.push_back(static_cast<std::uint8_t>(entry.data >> 8));
        }
    } else if (texture.format == 4 || texture.format == 6) {
        result.data.reserve(data.size());
        const std::uint32_t alpha_bits = texture.format == 4 ? 31U : 7U;
        const unsigned alpha_shift = texture.format == 4 ? 3U : 5U;
        for (const model::TexturePixel& entry : data) {
            const float scaled = static_cast<float>(entry.alpha)
                * static_cast<float>(alpha_bits) / 255.0F;
            const auto alpha = static_cast<std::uint8_t>(round_to_even(scaled));
            result.data.push_back(static_cast<std::uint8_t>(entry.data)
                                  | static_cast<std::uint8_t>(alpha
                                                               << alpha_shift));
        }
    } else if (texture.format == 0) {
        result.data.reserve((data.size() + 3) / 4);
        for (std::size_t index = 0; index < data.size(); index += 4) {
            std::uint8_t value = 0;
            for (std::size_t entry = 0; entry < 4 && index + entry < data.size();
                 ++entry) {
                value |= static_cast<std::uint8_t>(data[index + entry].data
                                                   << (2 * entry));
            }
            result.data.push_back(value);
        }
    } else if (texture.format == 1) {
        result.data.reserve((data.size() + 1) / 2);
        for (std::size_t index = 0; index < data.size(); index += 2) {
            std::uint8_t value = 0;
            for (std::size_t entry = 0; entry < 2 && index + entry < data.size();
                 ++entry) {
                value |= static_cast<std::uint8_t>(data[index + entry].data
                                                   << (4 * entry));
            }
            result.data.push_back(value);
        }
    } else if (texture.format == 2) {
        result.data.reserve(data.size());
        for (const model::TexturePixel& entry : data) {
            result.data.push_back(static_cast<std::uint8_t>(entry.data));
        }
    }
    return result;
}

[[nodiscard]] std::filesystem::path configured_file_system() {
    std::filesystem::path root = formats::global_paths().file_system();
    return root.empty() ? std::filesystem::current_path() : root;
}

[[nodiscard]] packer::ModelInfo make_source_model_info(
    const MapDefinition& definition) {
    const scene::RoomCatalogEntry* entry = scene::find_room(
        definition.texture_source);
    if (entry == nullptr) {
        throw std::runtime_error("No room with this name is known.");
    }
    const assets::Store assets = assets::Store::from_directory(
        configured_file_system());
    const scene::Room room = scene::Room::load(assets, entry->definition);
    const model::File& source = room.model();

    packer::ModelInfo result;
    std::map<int, int> texture_map;
    std::map<int, int> palette_map;
    for (const Material& material : definition.materials) {
        if (material.source_material < 0
            || static_cast<std::size_t>(material.source_material)
                >= source.materials().size()) {
            throw std::runtime_error(definition.texture_source
                                     + " has no material "
                                     + std::to_string(material.source_material)
                                     + ".");
        }
        const auto& source_material = source.materials()[
            static_cast<std::size_t>(material.source_material)];
        if (source_material.texture_id < 0 || source_material.palette_id < 0) {
            throw std::runtime_error(
                "Material " + std::to_string(material.source_material)
                + " of " + definition.texture_source
                + " has no texture.");
        }
        const int source_texture = source_material.texture_id;
        const int source_palette = source_material.palette_id;
        auto texture = texture_map.find(source_texture);
        if (texture == texture_map.end()) {
            const int packed = static_cast<int>(result.textures.size());
            result.textures.push_back(make_source_texture(
                source, static_cast<std::size_t>(source_texture)));
            texture = texture_map.emplace(source_texture, packed).first;
        }
        auto palette = palette_map.find(source_palette);
        if (palette == palette_map.end()) {
            const int packed = static_cast<int>(result.palettes.size());
            result.palettes.push_back({source.decode_palette(
                static_cast<std::size_t>(source_palette))});
            palette = palette_map.emplace(source_palette, packed).first;
        }
        result.materials.push_back({material.name, texture->second,
                                    palette->second});
    }
    return result;
}

[[nodiscard]] packer::ModelInfo make_imported_model_info(
    const std::vector<detail::TexturePackEntry>& texture_pack) {
    packer::ModelInfo result;
    result.textures.reserve(texture_pack.size());
    result.palettes.reserve(texture_pack.size());
    result.materials.reserve(texture_pack.size());
    for (const detail::TexturePackEntry& entry : texture_pack) {
        result.textures.push_back({2, true, entry.width, entry.height,
                                   entry.pixels});
        result.palettes.push_back({entry.palette});
        const std::string name = entry.name.size() <= 30
            ? entry.name : entry.name.substr(entry.name.size() - 30);
        const int id = static_cast<int>(result.textures.size() - 1);
        result.materials.push_back({name, id, id});
    }
    return result;
}

} // namespace


GeneratedMap build(const MapDefinition& definition) {
    MapDefinition effective = definition;
    std::vector<BuiltFace> all_faces;
    std::vector<BuiltFace> solid_faces;
    std::vector<detail::TexturePackEntry> imported_texture_pack;
    packer::ModelInfo model_info;
    if (!definition.import_source.empty()) {
        const detail::ImportedMap imported = detail::import_q3(definition);
        imported_texture_pack = imported.texture_pack;
        if (imported.has_texture_pack) {
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
        model_info = imported.has_texture_pack
            ? make_imported_model_info(imported_texture_pack)
            : make_source_model_info(definition);
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
        model_info = make_source_model_info(definition);
    }
    GeneratedMap result;
    result.model = packer::build_model(effective, all_faces, model_info,
                                       result.stats);
    std::vector<map_collision::CollisionFace> collision_faces;
    collision_faces.reserve(solid_faces.size());
    for (const BuiltFace& face : solid_faces) {
        if (face.points.size() <= 10) {
            map_collision::CollisionFace collision_face;
            collision_face.points = face.points;
            collision_face.normal = face.normal;
            collision_face.flags = face.flags;
            collision_faces.push_back(std::move(collision_face));
            continue;
        }
        // MapPacker.BuildCollision keeps polygons up to ten points intact and
        // fans only the larger ones before passing them to the wc01 writer.
        for (std::size_t point = 1; point + 1 < face.points.size(); ++point) {
            map_collision::CollisionFace collision_face;
            collision_face.points = {face.points[0], face.points[point],
                                     face.points[point + 1]};
            collision_face.normal = face.normal;
            collision_face.flags = face.flags;
            collision_faces.push_back(std::move(collision_face));
        }
    }
    result.collision = map_collision::pack(collision_faces, result.stats);
    result.entities = packer::build_entities(effective, result.stats);
    std::vector<map_nodes::NavigationFace> navigation_faces;
    navigation_faces.reserve(solid_faces.size());
    for (const BuiltFace& face : solid_faces) {
        map_nodes::NavigationFace navigation_face;
        navigation_face.points = face.points;
        navigation_face.normal = face.normal;
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
                     const std::filesystem::path& archive_directory,
                     const std::filesystem::path& entity_directory,
                     const std::filesystem::path& node_directory) {
    packer::write_generated(definition, generated, archive_directory,
                            entity_directory, node_directory);
}

} // namespace fruityprime::mapgen
