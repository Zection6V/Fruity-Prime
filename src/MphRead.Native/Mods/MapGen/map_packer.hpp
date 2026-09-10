#pragma once

#include "Mods/MapGen/mapgen.hpp"
#include "Mods/MapGen/q3_import.hpp"

namespace fruityprime::mapgen::packer {

// The managed MapPacker first makes the exact texture/palette/material lists
// it will write, then assembles geometry against those remapped IDs.  Keep
// that intermediate contract explicit instead of making the writer infer a
// palette from a texture (a texture may be used with more than one palette).
struct TextureInfo {
    std::uint8_t format = 0;
    bool opaque = false;
    std::uint16_t width = 0;
    std::uint16_t height = 0;
    std::vector<std::uint8_t> data;
};

struct PaletteInfo {
    std::vector<std::uint16_t> data;
};

struct MaterialInfo {
    std::string name;
    int texture_id = -1;
    int palette_id = -1;
};

struct ModelInfo {
    std::vector<TextureInfo> textures;
    std::vector<PaletteInfo> palettes;
    std::vector<MaterialInfo> materials;
};

[[nodiscard]] std::vector<std::uint8_t> build_model(
    const MapDefinition& definition, const std::vector<BuiltFace>& faces,
    const ModelInfo& model,
    BuildStats& stats);

[[nodiscard]] std::vector<std::uint8_t> build_entities(
    const MapDefinition& definition, BuildStats& stats);

void write_generated(const MapDefinition& definition,
                     const GeneratedMap& generated,
                     const std::filesystem::path& archive_directory,
                     const std::filesystem::path& entity_directory,
                     const std::filesystem::path& node_directory);

} // namespace fruityprime::mapgen::packer
