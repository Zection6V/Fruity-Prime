#include "Mods/MapGen/map_report.hpp"

#include "Assets/game_assets.hpp"
#include "Formats/model_format.hpp"
#include "Entities/room_catalog.hpp"
#include "Entities/scene.hpp"
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
    const auto* entry = scene::find_room(room_name);
    if (entry != nullptr) {
        return entry->definition;
    }
    // Match the existing native room probes: an unknown label still reaches
    // the asset loader and reports its real failure instead of silently
    // selecting an unrelated room definition.
    return scene::RoomDefinition{
        std::string(room_name),
        "archives/unit1_C0.arc",
        "unit1_c0_model.bin",
        "levels/textures/unit1_c0_tex.bin",
        "unit1_c0_collision.bin",
        "levels/entities/Unit1_C0_Ent.bin", {}, {}, {}
    };
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
        const scene::Room room = scene::Room::load(
            assets, definition_for_room(room_name));
        const model::File& model = room.model();
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
