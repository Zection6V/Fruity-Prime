// Formats/CollisionDetection.cs's candidate walk and its two sphere queries,
// exercised against every cartridge room's real collision mesh.  A synthetic
// mesh cannot catch a wrong partition-grid index or a mis-signed plane test;
// a real room can, because a downward sweep from the middle of the mesh has
// to find its floor.
#include "Formats/collision_runtime.hpp"
#include "Assets/game_assets.hpp"
#include "Entities/room_catalog.hpp"
#include "Entities/scene.hpp"

#include <array>
#include <cstdlib>
#include <algorithm>
#include <iostream>

namespace fp = fruityprime;

int main(int argc, char** argv) {
    const char* rom = argc > 1 ? argv[1] : std::getenv("FRUITY_PRIME_TEST_NDS");
    if (rom == nullptr || rom[0] == '\0') {
        std::cout << "native collision runtime test skipped: set "
                     "FRUITY_PRIME_TEST_NDS\n";
        return 0;
    }
    const auto assets = fp::assets::Store::from_path(rom);
    std::size_t rooms = 0, with_candidates = 0, radius_hits = 0, sweep_hits = 0;
    for (const auto& entry : fp::scene::multiplayer_rooms()) {
        const auto room = fp::scene::Room::load(assets, entry.definition);
        if (room.collision().is_first_hunt()) { continue; }
        const auto info = fp::collision::Info::from_file(room.collision());
        fp::collision::Instance instance{entry.name, true, &info, false, {}, {}};
        const fp::collision::Instance* list[] = {&instance};
        // Probe the middle of the collision mesh's own extent.
        fp::formats::Vector3 lo{}, hi{};
        bool any = false;
        for (const auto& p : room.collision().mph().points) {
            const fp::formats::Vector3 v = p.to_float_vector();
            if (!any) { lo = hi = v; any = true; continue; }
            lo = {std::min(lo.x, v.x), std::min(lo.y, v.y), std::min(lo.z, v.z)};
            hi = {std::max(hi.x, v.x), std::max(hi.y, v.y), std::max(hi.z, v.z)};
        }
        const fp::formats::Vector3 mid{(lo.x + hi.x) / 2, (lo.y + hi.y) / 2,
                                       (lo.z + hi.z) / 2};
        ++rooms;
        const auto cands = fp::collision::get_candidates_for_points(
            list, mid, {mid.x, lo.y, mid.z}, 0.5F);
        if (!cands.empty()) { ++with_candidates; }
        std::array<fp::collision::Result, 16> results{};
        const auto n = fp::collision::check_sphere_between_points(
            list, mid, {mid.x, lo.y - 1.0F, mid.z}, 0.5F, true,
            fp::collision::TestFlags::Players, results);
        if (n > 0) { ++sweep_hits; }
        const auto r = fp::collision::check_in_radius(
            list, {mid.x, lo.y + 0.5F, mid.z}, 2.0F, false,
            fp::collision::TestFlags::Players, results);
        if (r > 0) { ++radius_hits; }
    }
    std::cout << "native collision runtime: rooms=" << rooms
              << " with_candidates=" << with_candidates
              << " downward_sweep_hits=" << sweep_hits
              << " radius_hits=" << radius_hits << "\n";
    if (rooms == 0) {
        std::cerr << "no MPH room collision was loaded\n";
        return 1;
    }
    if (with_candidates != rooms) {
        std::cerr << "the partition grid returned no candidate for some room\n";
        return 1;
    }
    if (sweep_hits != rooms) {
        std::cerr << "a downward sweep from the middle of a room found no "
                     "floor\n";
        return 1;
    }
    return 0;
}
