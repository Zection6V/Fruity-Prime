#pragma once

#include "Mods/MapGen/mapgen.hpp"

#include <cstdint>
#include <vector>

namespace fruityprime::mapgen::map_collision {

// The collision packer only needs the geometric and run-time flag portion of
// a generated face.  Keeping this smaller than the render face prevents the
// collision writer from depending on texture coordinates or material data.
struct CollisionFace {
    std::vector<Vec3> points;
    Vec3 normal;
    std::uint16_t flags = 0;
};

[[nodiscard]] std::vector<std::uint8_t> pack(
    const std::vector<CollisionFace>& faces, BuildStats& stats);

} // namespace fruityprime::mapgen::map_collision
