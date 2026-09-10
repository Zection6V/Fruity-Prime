#include "Mods/MapGen/map_report.hpp"

#include "Assets/game_assets.hpp"
#include "Formats/model_format.hpp"
#include "Entities/room_catalog.hpp"
#include "Entities/scene.hpp"
#include "Metadata/Rooms.hpp"
#include "Mods/MapGen/q3_import.hpp"

#include <iomanip>
#include <iostream>
#include <ostream>
#include <stdexcept>
#include <string>

namespace fruityprime::mapgen {
namespace {

[[nodiscard]] std::string texture_format_name(std::uint8_t value) {
    switch (static_cast<formats::TextureFormat>(value)) {
    case formats::TextureFormat::Palette2Bit: return "Palette2Bit";
    case formats::TextureFormat::Palette4Bit: return "Palette4Bit";
    case formats::TextureFormat::Palette8Bit: return "Palette8Bit";
    case formats::TextureFormat::PaletteA5I3: return "PaletteA5I3";
    case formats::TextureFormat::DirectRgb: return "DirectRgb";
    case formats::TextureFormat::PaletteA3I5: return "PaletteA3I5";
    }
    return "Unknown";
}

[[nodiscard]] std::string render_mode_name(std::uint8_t value) {
    switch (static_cast<formats::RenderMode>(value)) {
    case formats::RenderMode::Normal: return "Normal";
    case formats::RenderMode::Decal: return "Decal";
    case formats::RenderMode::Translucent: return "Translucent";
    case formats::RenderMode::Unknown3: return "Unknown3";
    case formats::RenderMode::Unknown4: return "Unknown4";
    }
    return "Unknown";
}

[[nodiscard]] scene::RoomDefinition definition_for_room(
    std::string_view room_name) {
    if (fruityprime::metadata::get_room_by_name(room_name).first == nullptr) {
        throw std::invalid_argument("No room with this name is known.");
    }
    const auto* entry = scene::find_room(room_name);
    if (entry != nullptr) {
        return entry->definition;
    }
    throw std::invalid_argument("No room with this name is known.");
}

[[nodiscard]] std::vector<std::uint8_t> archive_entry(
    const assets::Store& assets, std::string_view archive_path,
    std::string_view entry_name) {
    const auto archive = assets.archive(archive_path);
    for (std::size_t index = 0; index < archive.entries().size(); ++index) {
        if (archive.entries()[index].filename == entry_name) {
            return archive.file(index);
        }
    }
    throw std::out_of_range("room archive entry was not found: "
                           + std::string(entry_name));
}

[[nodiscard]] model::File model_for_room(
    const assets::Store& assets, const scene::RoomDefinition& definition) {
    if (!definition.external_root.empty()) {
        const auto external = assets::Store::from_directory(
            definition.external_root);
        const auto texture = definition.texture_path.empty()
            ? std::vector<std::uint8_t>{}
            : external.bytes(definition.texture_path);
        return model::File::from_resources(
            external.bytes(definition.model_entry), texture);
    }
    const auto texture = definition.texture_path.empty()
        ? std::vector<std::uint8_t>{}
        : assets.bytes(definition.texture_path);
    return model::File::from_resources(
        archive_entry(assets, definition.model_archive,
                      definition.model_entry), texture);
}

} // namespace

int list_shaders(std::ostream& output, const std::filesystem::path& source,
                 std::string_view map_name) {
    try {
        const auto counts = detail::list_shader_usage(source, map_name);
        output << (map_name.empty() ? source.string() : std::string(map_name))
               << ": " << counts.size() << " shaders drawn\n";
        for (const auto& count : counts) {
            output << "  " << std::setw(6) << count.triangles
                   << " triangles  " << count.name << '\n';
        }
        return 0;
    } catch (const std::exception& error) {
        output << error.what() << '\n';
        return 1;
    }
}

int list_materials(std::ostream& output, const assets::Store& assets,
                   std::string_view room_name) {
    try {
        const model::File model = model_for_room(
            assets, definition_for_room(room_name));
        output << room_name << ": " << model.materials().size()
               << " materials, " << model.textures().size() << " textures\n";
        for (std::size_t index = 0; index < model.materials().size(); ++index) {
            const model::Material& material = model.materials()[index];
            std::string size = "no texture";
            std::string format;
            if (material.texture_id >= 0
                && static_cast<std::size_t>(material.texture_id)
                    < model.textures().size()) {
                const model::Texture& texture = model.textures()[
                    static_cast<std::size_t>(material.texture_id)];
                size = std::to_string(texture.width) + "x"
                    + std::to_string(texture.height);
                format = texture_format_name(texture.format);
            }
            output << "  " << std::setw(3) << index << "  "
                   << std::left << std::setw(32) << material.name
                   << std::right << " tex " << std::setw(3)
                   << material.texture_id << " pal " << std::setw(3)
                   << material.palette_id << "  " << std::left
                   << std::setw(9) << size << " " << format;
            if (material.render_mode != static_cast<std::uint8_t>(
                    formats::RenderMode::Normal)) {
                output << ' ' << render_mode_name(material.render_mode);
            }
            output << '\n';
        }
        return 0;
    } catch (const std::exception& error) {
        output << "Could not load " << room_name << ": "
               << error.what() << '\n';
        return 1;
    }
}

} // namespace fruityprime::mapgen
