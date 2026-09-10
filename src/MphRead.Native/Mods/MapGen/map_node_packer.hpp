#pragma once

#include "Mods/MapGen/mapgen.hpp"

#include <cstdint>
#include <vector>

namespace fruityprime::mapgen::map_nodes {

// Navigation generation needs geometry only.  Render materials and collision
// flags stay out of this input so the route writer remains independently
// testable and cannot accidentally change render output.
struct NavigationFace {
    std::vector<Vec3> points;
    Vec3 normal;
};

[[nodiscard]] std::vector<std::uint8_t> pack(
    const std::vector<NavigationFace>& solid_faces, BuildStats& stats);

} // namespace fruityprime::mapgen::map_nodes
