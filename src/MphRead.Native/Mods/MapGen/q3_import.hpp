#pragma once

#include "Mods/MapGen/q3_bsp.hpp"

#include <array>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace fruityprime::mapgen::detail {

struct TexturePackEntry {
    std::uint16_t source_index = 0;
    std::string name;
    std::uint16_t width = 0;
    std::uint16_t height = 0;
    std::vector<std::uint16_t> palette;
    std::vector<std::uint8_t> pixels;
};

// The importer keeps the managed BuiltFace contract: a polygon owns a
// variable-length point and texture-coordinate array. The model and collision
// packers decide where the format requires a fan, rather than truncating the
// source polygon at an arbitrary native array size.
struct ImportedFace {
    std::vector<Vec3> points;
    Vec3 normal;
    int material = 0;
    float shade = 1.0F;
    bool damaging = false;
    std::uint16_t flags = 0;
    bool has_texcoords = false;
    std::vector<std::array<float, 2>> texcoords;
};

struct ImportedMap {
    std::vector<ImportedFace> faces;
    std::vector<ImportedFace> solid_faces;
    std::vector<Material> materials;
    std::vector<Spawn> spawns;
    std::vector<JumpPad> jump_pads;
    std::vector<Item> items;
    std::vector<TexturePackEntry> texture_pack;
    bool has_texture_pack = false;
};

struct ShaderUsage {
    std::string name;
    int triangles = 0;
};

struct TextureBakeResult {
    std::vector<TexturePackEntry> entries;
    std::vector<std::string> missing;
};

[[nodiscard]] ImportedMap import_q3(const MapDefinition& definition);

// MapReport.ListShaders uses the same face filtering as the managed report,
// but keeps BSP parsing private to the Q3 importer. The report module owns the
// presentation; this boundary only exposes the decoded usage counts.
[[nodiscard]] std::vector<ShaderUsage> list_shader_usage(
    const std::filesystem::path& source, std::string_view map_name);

// Q3Convert owns the command-level recipe generation. These narrow source
// helpers keep archive/BSP loading and first-run texture baking in the import
// module without moving the conversion policy back into it.
[[nodiscard]] std::vector<std::uint8_t> read_q3_level(
    const MapDefinition& definition, const std::filesystem::path& source);
[[nodiscard]] std::vector<std::string> list_q3_maps(
    const std::filesystem::path& source);
[[nodiscard]] std::vector<TexturePackEntry> bake_q3_texture_pack(
    const Q3Bsp& bsp, const std::filesystem::path& source,
    const MapDefinition& definition, int texture_size = 64);
[[nodiscard]] TextureBakeResult bake_q3_texture_pack_with_report(
    const Q3Bsp& bsp, const std::filesystem::path& source,
    const MapDefinition& definition, int texture_size = 64);

} // namespace fruityprime::mapgen::detail
