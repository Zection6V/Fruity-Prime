#pragma once

#include "Mods/MapGen/mapgen.hpp"

#include <array>
#include <cstdint>
#include <filesystem>
#include <map>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace fruityprime::mapgen {

// These records are the native counterparts of the public records declared
// next to Q3Bsp in Q3Bsp.cs. Entity keys use the same ordinal,
// case-insensitive lookup contract as StringComparer.OrdinalIgnoreCase while
// retaining the spelling of the first key inserted in a dictionary.
struct Q3EntityKeyLess {
    [[nodiscard]] bool operator()(const std::string& left,
                                  const std::string& right) const noexcept;
};

using Q3Entity = std::map<std::string, std::string, Q3EntityKeyLess>;

struct Q3Texture {
    std::string name;
    std::int32_t flags = 0;
    std::int32_t contents = 0;
};

struct Q3Plane {
    Vec3 normal;
    float distance = 0.0F;
};

struct Q3Brush {
    std::int32_t first_side = 0;
    std::int32_t side_count = 0;
    std::int32_t texture = 0;
};

struct Q3BrushSide {
    std::int32_t plane = 0;
    std::int32_t texture = 0;
};

struct Q3Vertex {
    Vec3 position;
    float surface_s = 0.0F;
    float surface_t = 0.0F;
    Vec3 normal;
    std::array<std::uint8_t, 4> color{0, 0, 0, 0};
};

struct Q3Face {
    std::int32_t texture = 0;
    std::int32_t effect = 0;
    std::int32_t type = 0;
    std::int32_t vertex = 0;
    std::int32_t vertex_count = 0;
    std::int32_t mesh_vertex = 0;
    std::int32_t mesh_vertex_count = 0;
    Vec3 normal;
    std::array<std::int32_t, 2> size{0, 0};
};

struct Q3Model {
    Vec3 mins;
    Vec3 maxs;
    std::int32_t face = 0;
    std::int32_t face_count = 0;
    std::int32_t brush = 0;
    std::int32_t brush_count = 0;
};

struct Q3Bsp {
    std::vector<Q3Texture> textures;
    std::vector<Q3Plane> planes;
    std::vector<Q3Brush> brushes;
    std::vector<Q3BrushSide> brush_sides;
    std::vector<Q3Vertex> vertices;
    std::vector<std::int32_t> mesh_vertices;
    std::vector<Q3Face> faces;
    std::vector<Q3Model> models;
    std::vector<Q3Entity> entities;

    static constexpr std::int32_t ContentsSolid = 0x1;
    static constexpr std::int32_t ContentsPlayerClip = 0x10000;
    static constexpr std::int32_t SurfaceSky = 0x4;
    static constexpr std::int32_t SurfaceNoDraw = 0x80;
    static constexpr std::int32_t SurfaceHint = 0x100;
    static constexpr std::int32_t SurfaceSkip = 0x200;
    static constexpr std::array<int, 9> UsedLumps{0, 1, 2, 7, 8, 9, 10,
                                                    11, 13};

    [[nodiscard]] static Q3Bsp load(
        const std::filesystem::path& source,
        std::optional<std::string_view> map_name = std::nullopt);
    [[nodiscard]] static std::vector<std::uint8_t> read_level(
        const std::filesystem::path& source,
        std::optional<std::string_view> map_name = std::nullopt);
    [[nodiscard]] static std::vector<std::string> list_maps(
        const std::filesystem::path& source);
    [[nodiscard]] static std::vector<std::uint8_t> trim(
        std::span<const std::uint8_t> bsp);
};

namespace detail {

// Existing import and texture-bake code uses the detail namespace for helper
// functions. Keep aliases here so those helpers consume the same public
// Q3Bsp records instead of a second native-only model of the file.
using Q3Entity = ::fruityprime::mapgen::Q3Entity;
using Q3Texture = ::fruityprime::mapgen::Q3Texture;
using Q3Plane = ::fruityprime::mapgen::Q3Plane;
using Q3Brush = ::fruityprime::mapgen::Q3Brush;
using Q3BrushSide = ::fruityprime::mapgen::Q3BrushSide;
using Q3Vertex = ::fruityprime::mapgen::Q3Vertex;
using Q3Face = ::fruityprime::mapgen::Q3Face;
using Q3Model = ::fruityprime::mapgen::Q3Model;
using Q3Bsp = ::fruityprime::mapgen::Q3Bsp;

// Decode exactly the nine lumps consumed by the managed Q3Bsp.Parse method.
[[nodiscard]] Q3Bsp parse_bsp(std::span<const std::uint8_t> bytes);

} // namespace detail
} // namespace fruityprime::mapgen
