#include "map_builder.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <stdexcept>
#include <string>
#include <string_view>

namespace fruityprime::mapgen::builder {
namespace {

[[nodiscard]] std::string lower(std::string_view value) {
    std::string result;
    result.reserve(value.size());
    for (const char character : value) {
        result.push_back(static_cast<char>(std::tolower(
            static_cast<unsigned char>(character))));
    }
    return result;
}

[[nodiscard]] std::uint16_t terrain_flags(std::string_view name) {
    const std::string value = lower(name);
    static constexpr std::array<std::string_view, 13> names{
        "metal", "orangeholo", "greenholo", "blueholo", "ice", "snow",
        "sand", "rock", "lava", "acid", "gorea", "unknown11", "all"};
    const auto found = std::find(names.begin(), names.end(), value);
    const std::size_t terrain = found == names.end()
        ? 0 : static_cast<std::size_t>(found - names.begin());
    return static_cast<std::uint16_t>(terrain << 5);
}

[[nodiscard]] std::array<float, 2> project_texture(
    Vec3 point, Vec3 normal, Vec3 origin, float texture_scale) noexcept {
    const float ax = std::abs(normal.x);
    const float ay = std::abs(normal.y);
    const float az = std::abs(normal.z);
    if (ay > ax && ay >= az) {
        return {(point.x - origin.x) * texture_scale,
                (point.z - origin.z) * texture_scale};
    }
    if (ax >= az) {
        return {(point.z - origin.z) * texture_scale,
                (origin.y - point.y) * texture_scale};
    }
    return {(point.x - origin.x) * texture_scale,
            (origin.y - point.y) * texture_scale};
}

} // namespace

std::vector<BuiltFace> make_faces(const MapDefinition& definition,
                                  bool solid_only) {
    std::vector<BuiltFace> result;
    for (const Brush& brush : definition.brushes) {
        if (solid_only && !brush.solid) {
            continue;
        }
        const float x0 = std::min(brush.min.x, brush.max.x);
        const float y0 = std::min(brush.min.y, brush.max.y);
        const float z0 = std::min(brush.min.z, brush.max.z);
        const float x1 = std::max(brush.min.x, brush.max.x);
        const float y1 = std::max(brush.min.y, brush.max.y);
        const float z1 = std::max(brush.min.z, brush.max.z);
        const std::array<float, 6> face_shades{
            1.0F, 0.55F, 0.82F, 0.82F, 0.74F, 0.74F};
        const float texture_scale = brush.material >= 0
                && static_cast<std::size_t>(brush.material)
                    < definition.materials.size()
            ? definition.materials[static_cast<std::size_t>(brush.material)]
                  .tex_scale
            : 16.0F;
        const Vec3 origin{x0, y0, z0};
        const std::array<std::pair<std::array<Vec3, 4>, Vec3>, 6> sides{{
            {{{{x0, y1, z1}, {x1, y1, z1}, {x1, y1, z0},
                {x0, y1, z0}}}, {0.0F, 1.0F, 0.0F}},
            {{{{x0, y0, z0}, {x1, y0, z0}, {x1, y0, z1},
                {x0, y0, z1}}}, {0.0F, -1.0F, 0.0F}},
            {{{{x1, y0, z1}, {x1, y0, z0}, {x1, y1, z0},
                {x1, y1, z1}}}, {1.0F, 0.0F, 0.0F}},
            {{{{x0, y0, z0}, {x0, y0, z1}, {x0, y1, z1},
                {x0, y1, z0}}}, {-1.0F, 0.0F, 0.0F}},
            {{{{x0, y0, z1}, {x1, y0, z1}, {x1, y1, z1},
                {x0, y1, z1}}}, {0.0F, 0.0F, 1.0F}},
            {{{{x1, y0, z0}, {x0, y0, z0}, {x0, y1, z0},
                {x1, y1, z0}}}, {0.0F, 0.0F, -1.0F}}
        }};
        for (std::size_t i = 0; i < sides.size(); ++i) {
            BuiltFace face;
            face.points.assign(sides[i].first.begin(), sides[i].first.end());
            face.normal = sides[i].second;
            face.material = brush.material;
            face.shade = brush.shade * face_shades[i];
            face.damaging = brush.damaging;
            face.flags = static_cast<std::uint16_t>(
                (brush.damaging ? 0x1 : 0) | terrain_flags(brush.terrain));
            face.has_texcoords = true;
            face.texcoords.resize(face.points.size());
            for (std::size_t point = 0; point < face.points.size(); ++point) {
                face.texcoords[point] = project_texture(
                    face.points[point], face.normal, origin, texture_scale);
            }
            result.push_back(std::move(face));
        }
    }
    return result;
}

BuiltMap build(const MapDefinition& definition) {
    BuiltMap result(definition);
    result.faces = make_faces(result.definition, false);
    result.solid = make_faces(result.definition, true);
    return result;
}

} // namespace fruityprime::mapgen::builder
