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

} // namespace

std::vector<BuiltFace> make_faces(const MapDefinition& definition,
                                  bool solid_only) {
    const std::size_t material_count = std::max<std::size_t>(
        1, definition.materials.size());
    std::vector<BuiltFace> result;
    for (const Brush& brush : definition.brushes) {
        if (solid_only && !brush.solid) {
            continue;
        }
        if (brush.material < 0
            || static_cast<std::size_t>(brush.material) >= material_count) {
            throw std::runtime_error(
                "map brush material index is outside materials");
        }
        const float x0 = std::min(brush.min.x, brush.max.x);
        const float y0 = std::min(brush.min.y, brush.max.y);
        const float z0 = std::min(brush.min.z, brush.max.z);
        const float x1 = std::max(brush.min.x, brush.max.x);
        const float y1 = std::max(brush.min.y, brush.max.y);
        const float z1 = std::max(brush.min.z, brush.max.z);
        if (!(x1 > x0 && y1 > y0 && z1 > z0)) {
            throw std::runtime_error("map brush must have positive dimensions");
        }
        const std::array<float, 6> face_shades{
            1.0F, 0.55F, 0.82F, 0.82F, 0.74F, 0.74F};
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
            face.points = sides[i].first;
            face.normal = sides[i].second;
            face.material = brush.material;
            face.shade = std::clamp(brush.shade * face_shades[i], 0.0F, 1.0F);
            face.damaging = brush.damaging;
            face.flags = static_cast<std::uint16_t>(
                (brush.damaging ? 0x1 : 0) | terrain_flags(brush.terrain));
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
