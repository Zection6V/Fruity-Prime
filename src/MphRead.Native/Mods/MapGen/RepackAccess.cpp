#include "Mods/MapGen/repack_access.hpp"

#include "map_collision_packer.hpp"
#include "map_packer.hpp"

#include <vector>

namespace fruityprime::mapgen::repack {

std::vector<std::uint8_t> pack_entities(const MapDefinition& definition,
                                        BuildStats& stats) {
    return packer::build_entities(definition, stats);
}

std::vector<std::uint8_t> pack_mph_collision(
    std::span<const BuiltFace> faces, BuildStats& stats) {
    std::vector<map_collision::CollisionFace> collision_faces;
    collision_faces.reserve(faces.size());
    for (const auto& face : faces) {
        map_collision::CollisionFace collision_face;
        collision_face.points = face.points;
        collision_face.normal = face.normal;
        collision_face.flags = face.flags;
        collision_faces.push_back(collision_face);
    }
    return map_collision::pack(collision_faces, stats);
}

} // namespace fruityprime::mapgen::repack
