// CollisionVolume.TestPoint and GetCenter.
//
// A box is tested against its own three authored axes, not an axis-aligned
// bound: the rotated case below is inside one way and outside the other, so
// an axis-aligned shortcut passes the easy tests and fails this one.
#include "Formats/formats_layouts.hpp"
#include <cstdio>
int main() {
    using namespace fruityprime::formats;
    CollisionVolume sphere;
    sphere.Type = VolumeType::Sphere;
    sphere.SpherePosition = {0, 0, 0};
    sphere.SphereRadius = 2.0F;
    // A rotated box: axes at 45 degrees, so an axis-aligned test would differ.
    CollisionVolume box;
    box.Type = VolumeType::Box;
    box.BoxPosition = {0, 0, 0};
    box.BoxVector1 = {0.70710678F, 0.70710678F, 0};
    box.BoxVector2 = {-0.70710678F, 0.70710678F, 0};
    box.BoxVector3 = {0, 0, 1};
    box.BoxDot1 = 2.0F; box.BoxDot2 = 2.0F; box.BoxDot3 = 2.0F;
    CollisionVolume cyl;
    cyl.Type = VolumeType::Cylinder;
    cyl.CylinderPosition = {0, 0, 0};
    cyl.CylinderVector = {0, 1, 0};
    cyl.CylinderDot = 4.0F;
    cyl.CylinderRadius = 1.0F;
    const bool s1 = sphere.TestPoint({1, 1, 1});      // dist 1.73 < 2
    const bool s2 = !sphere.TestPoint({2, 2, 0});     // dist 2.83 > 2
    // Along +X the rotated box's first axis gives 0.707 (inside 0..2) but the
    // second gives -0.707, so the point is outside despite being near.
    const bool b1 = !box.TestPoint({1, 0, 1});
    const bool b2 = box.TestPoint({0, 1, 1});         // both axes 0.707
    const bool c1 = cyl.TestPoint({0.5F, 2, 0});      // inside
    const bool c2 = !cyl.TestPoint({0.5F, 5, 0});     // above the top
    const bool c3 = !cyl.TestPoint({2, 2, 0});        // outside the radius
    const auto center = box.GetCenter();
    std::printf("native volume tests: s=%d%d b=%d%d c=%d%d%d center=(%.2f,%.2f,%.2f)\n",
        s1, s2, b1, b2, c1, c2, c3, center.x, center.y, center.z);
    return (s1 && s2 && b1 && b2 && c1 && c2 && c3) ? 0 : 1;
}
