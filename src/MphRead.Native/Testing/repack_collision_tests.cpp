#include "Utility/repack_collision.hpp"

#include <array>
#include <cassert>
#include <sstream>
#include <stdexcept>
#include <vector>

namespace {

using fruityprime::formats::Terrain;
using fruityprime::formats::Vector3;
using fruityprime::formats::Vector4;
using fruityprime::utility::repack_collision::CollisionDataEditor;
using fruityprime::utility::repack_collision::CollisionFlag;
using fruityprime::utility::repack_collision::Portal;

CollisionDataEditor make_face(float y, std::uint16_t layer_mask) {
    CollisionDataEditor face;
    face.points = {{-1.0F, y, -1.0F}, {1.0F, y, -1.0F},
                   {1.0F, y, 1.0F}, {-1.0F, y, 1.0F}};
    face.plane = Vector4{0.0F, 1.0F, 0.0F, y};
    face.layer_mask = layer_mask;
    face.flags = static_cast<std::uint16_t>(CollisionFlag::Damaging);
    return face;
}

Portal make_mph_portal() {
    Portal portal;
    portal.name = "port_a_b";
    portal.node_name1 = "a";
    portal.node_name2 = "b";
    portal.layer_mask = 4;
    portal.flags = 1;
    portal.points = {{0.0F, 0.0F, -1.0F}, {0.0F, 1.0F, -1.0F},
                     {0.0F, 1.0F, 1.0F}, {0.0F, 0.0F, 1.0F}};
    portal.planes = {
        Vector4{0.0F, 0.0F, -1.0F, 1.0F},
        Vector4{0.0F, 1.0F, 0.0F, 0.0F},
        Vector4{0.0F, 0.0F, 1.0F, 1.0F},
        Vector4{0.0F, -1.0F, 0.0F, 0.0F}
    };
    portal.plane = Vector4{-1.0F, 0.0F, 0.0F, 0.0F};
    portal.unknown00 = 0xde;
    portal.unknown01 = 0xdf;
    return portal;
}

} // namespace

int main() {
    CollisionDataEditor face = make_face(0.0F, 1);
    assert(face.damaging());
    assert(face.players());
    assert(face.beams());
    assert(face.scan());
    face.set_players(false);
    face.set_beams(false);
    face.set_scan(false);
    face.set_reflect(true);
    face.set_slipperiness(3);
    face.set_terrain(Terrain::Ice);
    assert(!face.players());
    assert(!face.beams());
    assert(!face.scan());
    assert(face.reflect());
    assert(face.slipperiness() == 3);
    assert(face.terrain() == Terrain::Ice);
    bool rejected = false;
    try {
        face.set_slipperiness(4);
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    assert(rejected);

    const auto region = fruityprime::utility::repack_collision::data_in_region(
        {-2.0F, -1.0F, -2.0F}, {2.0F, 1.0F, 2.0F},
        std::span<const CollisionDataEditor>(&face, 1));
    assert(region.size() == 1 && region[0] == 0);
    assert(fruityprime::utility::repack_collision::primary_axis(
               {0.0F, 1.0F, 0.0F}) == 1);
    const Vector4 edge_plane =
        fruityprime::utility::repack_collision::plane_from_edge(
            {0.0F, 0.0F, 0.0F}, {1.0F, 0.0F, 0.0F}, {0.0F, 1.0F, 0.0F});
    assert(edge_plane.z < -0.99F);

    const Portal portal = make_mph_portal();
    const std::array<CollisionDataEditor, 1> faces{face};
    const std::array<Portal, 1> portals{portal};

    const auto mph_bytes = fruityprime::utility::repack_collision::repack_mph(
        faces, portals);
    const auto mph = fruityprime::collision::File::from_bytes(mph_bytes);
    assert(mph.is_mph());
    assert(mph.mph().data.size() == 1);
    assert(mph.mph().portals.size() == 1);
    assert(mph.mph().points.size() == 4);
    assert(mph.mph().point_indices.size() == 5);
    assert(mph.mph().data[0].flags == face.flags);
    const auto mph_round_trip =
        fruityprime::utility::repack_collision::repack(mph);
    assert(mph_round_trip == mph_bytes);

    const auto fh_bytes =
        fruityprime::utility::repack_collision::repack_first_hunt(faces, {});
    const auto fh = fruityprime::collision::File::from_bytes(fh_bytes);
    assert(fh.is_first_hunt());
    assert(fh.first_hunt().data.size() == 1);
    assert(fh.first_hunt().vectors.size() == 4);
    assert(fh.first_hunt().entries.size() == 1);
    const auto fh_round_trip =
        fruityprime::utility::repack_collision::repack(fh);
    assert(fh_round_trip == fh_bytes);

    std::ostringstream summary;
    fruityprime::utility::repack_collision::print_summary(mph, summary);
    assert(summary.str().find("format=MPH") != std::string::npos);
    assert(fruityprime::utility::repack_collision::check_intersection(
        {-2.0F, 0.0F, 0.0F}, {2.0F, 0.0F, 0.0F},
        std::span<const Vector3>(face.points)) == false);
    return 0;
}
