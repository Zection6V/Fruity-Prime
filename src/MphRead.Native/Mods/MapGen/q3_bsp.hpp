#pragma once

#include "Mods/MapGen/mapgen.hpp"

#include <array>
#include <cstdint>
#include <map>
#include <span>
#include <string>
#include <vector>

namespace fruityprime::mapgen::detail {

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
    std::array<std::uint8_t, 4> color{255, 255, 255, 255};
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

using Q3Entity = std::map<std::string, std::string>;

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
};

// Decode exactly the nine lumps consumed by the managed Q3Bsp.Parse method.
[[nodiscard]] Q3Bsp parse_bsp(std::span<const std::uint8_t> bytes);

} // namespace fruityprime::mapgen::detail
