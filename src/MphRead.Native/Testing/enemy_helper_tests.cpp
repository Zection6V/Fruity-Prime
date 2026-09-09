// EnemyInstanceEntity's shared turning helpers.
//
// The rotation is applied in OpenTK's row-vector order; the transpose of it
// turns every enemy the wrong way while still looking like it is turning, so
// the direction is pinned here.  The seek stops one step short and snaps,
// which is what keeps an enemy from oscillating around a heading it cannot
// land on exactly.
#include "Entities/enemy_helpers.hpp"
#include <cmath>
#include <cstdio>
int main() {
    using namespace fruityprime;
    using namespace fruityprime::entities;
    // 90 degrees about +Y takes +Z to +X (right-handed, managed order).
    const formats::Vector3 r = rotate_vector({0, 0, 1}, {0, 1, 0}, 90.0F);
    // From +Z to -Z is 180 degrees: five 30-degree turns get within one step,
    // and the sixth call snaps rather than overshooting.
    formats::Vector3 cur{0, 0, 1};
    std::uint16_t steps = 10;
    int turns = 0;
    while (!seek_target_vector({0, 0, -1}, cur, {0, 1, 0}, steps, 30.0F)) {
        if (++turns > 20) { std::printf("did not converge\n"); return 1; }
    }
    // Running out of steps must stop, not snap.
    formats::Vector3 cur2{0, 0, 1};
    std::uint16_t few = 2;
    const bool done2 = seek_target_vector({0, 0, -1}, cur2, {0, 1, 0}, few, 30.0F);
    // A parallel up must be replaced, not produce NaNs.
    const formats::Vector3 up = fix_parallel_vectors({0, 1, 0}, {0, 1, 0});
    EnemyHitPlayers hits;
    hits.set(2, true);
    const bool hit_kept = hits[2] && !hits[0];
    hits.clear();
    std::printf("native enemy helpers: rot=(%.3f,%.3f,%.3f) turns=%d steps_left=%d partial=%d "
                "up=(%.2f,%.2f,%.2f) finite=%d hits=%d cleared=%d\n",
        r.x, r.y, r.z, turns, steps, !done2, up.x, up.y, up.z,
        std::isfinite(up.x) && std::isfinite(up.y) && std::isfinite(up.z),
        hit_kept, !hits[2]);
    return (std::fabs(r.x - 1.0F) < 1e-5F && std::fabs(r.z) < 1e-5F
            && turns == 5 && steps == 5 && !done2
            && std::isfinite(up.x) && std::isfinite(up.y)
            && hit_kept && !hits[2]) ? 0 : 1;
}
