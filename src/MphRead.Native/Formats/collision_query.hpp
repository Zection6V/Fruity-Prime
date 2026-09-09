#pragma once

#include "Formats/collision_format.hpp"
#include "Formats/Types.hpp"

#include <cstdint>
#include <optional>

namespace fruityprime::scene { struct EntityVolume; }

namespace fruityprime::collision {

// The managed routines intentionally write only selected fields. A miss, and
// some successful volume combinations, leave the caller's result untouched.
struct Result {
    std::uint8_t field0 = 0;
    std::uint16_t flags = 0;
    formats::Vector4 plane;
    float field14 = 0;
    formats::Vector3 position;
    float distance = 0;
    const void* entity_collision = nullptr;
    formats::Vector3 edge_point1;
    formats::Vector3 edge_point2;
    [[nodiscard]] int slipperiness() const noexcept { return (flags & 0x18) >> 3; }
    [[nodiscard]] formats::Terrain terrain() const noexcept {
        return static_cast<formats::Terrain>((flags & 0x1E0) >> 5);
    }
};

[[nodiscard]] bool check_sphere_overlap_volume(const scene::EntityVolume& other,
    formats::Vector3 pos, float radius, Result& result);
[[nodiscard]] bool check_cylinder_overlap_volume(const scene::EntityVolume& other,
    formats::Vector3 bottom, formats::Vector3 top, float radius, Result& result);
[[nodiscard]] bool check_cylinders_overlap(formats::Vector3 oneBottom,
    formats::Vector3 oneTop, formats::Vector3 twoBottom, formats::Vector3 twoVector,
    float twoDot, float radii, Result& result);
[[nodiscard]] bool check_volumes_overlap(const scene::EntityVolume& one,
    const scene::EntityVolume& two, Result& result);
[[nodiscard]] bool check_cylinder_overlap_sphere(formats::Vector3 cylBot,
    formats::Vector3 cylTop, formats::Vector3 spherePos, float radii, Result& result);
[[nodiscard]] bool check_cylinder_intersect_plane(formats::Vector3 cylBot,
    formats::Vector3 cylTop, formats::Vector4 plane, Result& result);
[[nodiscard]] bool check_cylinder_between_points(formats::Vector3 point1,
    formats::Vector3 point2, formats::Vector3 cylPos, float cylHeight,
    float radii, Result& result);
[[nodiscard]] bool check_port_between_points(const MphPortal& portal,
    formats::Vector3 point1, formats::Vector3 point2, bool otherSide);

struct Vec3 {
    float x = 0.0F;
    float y = 0.0F;
    float z = 0.0F;
};

struct SweepHit {
    Vec3 center;
    Vec3 contact;
    Vec3 normal;
    float fraction = 1.0F;
    std::uint16_t flags = 0;
};

// Brute-force MPH sphere sweep. It is intentionally independent of the
// renderer and spatial partition cache; the first native gameplay boundary
// favors a correct, easily-audited query before the partition fast path.
[[nodiscard]] std::optional<SweepHit> sweep_sphere(
    const File& collision, Vec3 start, Vec3 end, float radius,
    std::uint16_t ignored_flags = 0);

} // namespace fruityprime::collision
