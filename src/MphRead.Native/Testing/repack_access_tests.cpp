#include "Mods/MapGen/repack_access.hpp"

#include <array>
#include <cassert>

int main() {
    fruityprime::mapgen::MapDefinition definition;
    definition.name = "REPACK TEST";
    definition.spawns.push_back({{0.0F, 1.0F, 0.0F}, 0.0F});
    definition.items.push_back({{1.0F, 1.0F, 1.0F}, "UASmall", true, 300});

    fruityprime::mapgen::BuildStats entity_stats;
    const auto entities = fruityprime::mapgen::repack::pack_entities(
        definition, entity_stats);
    assert(!entities.empty());
    assert(entity_stats.entities >= 2);

    fruityprime::mapgen::BuiltFace face;
    face.points = {{{-1.0F, 0.0F, -1.0F}, {1.0F, 0.0F, -1.0F},
                    {1.0F, 0.0F, 1.0F}, {-1.0F, 0.0F, 1.0F}}};
    face.normal = {0.0F, 1.0F, 0.0F};
    face.point_count = 4;
    const std::array<fruityprime::mapgen::BuiltFace, 1> faces{face};
    fruityprime::mapgen::BuildStats collision_stats;
    const auto collision = fruityprime::mapgen::repack::pack_mph_collision(
        faces, collision_stats);
    assert(!collision.empty());
    assert(collision_stats.collision_faces == 1);
    assert(collision_stats.collision_points == 4);
    return 0;
}
