#include "Formats/collision_query.hpp"
#include "Entities/scene.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace fruityprime::collision {
namespace {

[[nodiscard]] Vec3 add(Vec3 a, Vec3 b) {
    return {a.x + b.x, a.y + b.y, a.z + b.z};
}

[[nodiscard]] Vec3 subtract(Vec3 a, Vec3 b) {
    return {a.x - b.x, a.y - b.y, a.z - b.z};
}

[[nodiscard]] Vec3 multiply(Vec3 value, float factor) {
    return {value.x * factor, value.y * factor, value.z * factor};
}

[[nodiscard]] float dot(Vec3 a, Vec3 b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

[[nodiscard]] Vec3 cross(Vec3 a, Vec3 b) {
    return {a.y * b.z - a.z * b.y,
            a.z * b.x - a.x * b.z,
            a.x * b.y - a.y * b.x};
}

[[nodiscard]] float length_squared(Vec3 value) {
    return dot(value, value);
}

[[nodiscard]] Vec3 normalized(Vec3 value) {
    const float length = std::sqrt(length_squared(value));
    if (length <= std::numeric_limits<float>::epsilon()) {
        return {};
    }
    return multiply(value, 1.0F / length);
}

[[nodiscard]] Vec3 to_float(formats::Vector3Fx value) {
    return {value.x.to_float(), value.y.to_float(), value.z.to_float()};
}

[[nodiscard]] bool point_on_face(const MphDataSet& data_set,
                                 const MphData& data, Vec3 point,
                                 Vec3 normal) {
    if (data.point_index_count == 0) {
        return false;
    }
    const std::size_t start = data.point_start_index;
    const std::size_t count = data.point_index_count;
    if (start > data_set.point_indices.size()
        || count > data_set.point_indices.size() - start) {
        return false;
    }
    constexpr float edge_margin = 0.03125F;
    for (std::size_t i = 0; i < count; ++i) {
        const auto point_index1 = data_set.point_indices[start + i];
        const auto point_index2 = data_set.point_indices[
            start + ((i + 1) % count)];
        if (point_index1 >= data_set.points.size()
            || point_index2 >= data_set.points.size()) {
            return false;
        }
        const Vec3 point1 = to_float(data_set.points[point_index1]);
        const Vec3 point2 = to_float(data_set.points[point_index2]);
        const Vec3 edge = normalized(subtract(point1, point2));
        if (length_squared(edge) <= std::numeric_limits<float>::epsilon()) {
            return false;
        }
        const Vec3 edge_normal = cross(edge, normal);
        if (dot(edge_normal, subtract(point, point2)) < -edge_margin) {
            return false;
        }
    }
    return true;
}

} // namespace

std::optional<SweepHit> sweep_sphere(const File& collision, Vec3 start,
                                     Vec3 end, float radius,
                                     std::uint16_t ignored_flags) {
    if (!collision.is_mph() || radius < 0.0F) {
        return std::nullopt;
    }
    const auto& data_set = collision.mph();
    const Vec3 movement = subtract(end, start);
    const float radius_value = radius;
    std::optional<SweepHit> best;
    for (const auto& data : data_set.data) {
        if ((data.flags & ignored_flags) != 0
            || data.plane_index >= data_set.planes.size()) {
            continue;
        }
        const auto& plane = data_set.planes[data.plane_index];
        const Vec3 normal = normalized({plane.x.to_float(), plane.y.to_float(),
                                        plane.z.to_float()});
        if (length_squared(normal) <= std::numeric_limits<float>::epsilon()) {
            continue;
        }
        const float plane_w = plane.w.to_float();
        const float distance_start = dot(start, normal) - plane_w;
        const float distance_end = dot(end, normal) - plane_w;
        if (distance_start <= radius_value
            || distance_end > radius_value
            || distance_start <= distance_end) {
            continue;
        }
        const float denominator = distance_start - distance_end;
        const float fraction = std::clamp(
            (distance_start - radius_value) / denominator, 0.0F, 1.0F);
        const Vec3 center = add(start, multiply(movement, fraction));
        const Vec3 contact = subtract(center, multiply(normal, radius_value));
        if (!point_on_face(data_set, data, contact, normal)) {
            continue;
        }
        if (best.has_value() && fraction >= best->fraction) {
            continue;
        }
        best = SweepHit{center, contact, normal, fraction, data.flags};
    }
    return best;
}

// Direct counterparts of the pure volume routines in CollisionDetection.cs.
// Preserve the source's asymmetric dispatch, partial result writes and IEEE
// division behavior, including the parallel-cylinder degeneracy.
namespace {
using formats::Vector3;
using formats::Vector4;
Vector3 as_vector(scene::VolumePoint p) { return {p.x, p.y, p.z}; }
Vector3 managed_normalized(Vector3 v) { return v / v.length(); }
}

bool check_sphere_overlap_volume(const scene::EntityVolume& other, Vector3 pos, float radius, Result& result)
{
    if (other.kind == scene::VolumeKind::Cylinder)
    {
        Vector3 between = pos - as_vector(other.cylinder_position);
        float dot = formats::dot(as_vector(other.cylinder_vector), between);
        if (dot >= -radius && dot <= other.cylinder_dot + radius)
        {
            between -= as_vector(other.cylinder_vector) * dot;
            float radii = radius + other.cylinder_radius;
            if (between.length_squared() <= radii * radii)
            {
                result.field0 = 2;
                result.entity_collision = nullptr;
                result.flags = 0;
                if (dot < 0)
                {
                    result.plane = Vector4(as_vector(other.cylinder_vector), 0);
                    result.field14 = -dot;
                }
                else if (dot <= other.cylinder_dot)
                {
                    float mag = between.length();
                    result.plane = Vector4(-between / mag, 0);
                    result.field14 = mag - radius;
                }
                else
                {
                    result.plane = Vector4(-as_vector(other.cylinder_vector), 0);
                    result.field14 = dot - other.cylinder_dot;
                }
                return true;
            }
        }
    }
    else if (other.kind == scene::VolumeKind::Sphere)
    {
        Vector3 between = as_vector(other.sphere_position) - pos;
        float radii = other.sphere_radius + radius;
        if (between.length_squared() <= radii * radii)
        {
            result.field0 = 2;
            result.entity_collision = nullptr;
            result.flags = 0;
            float mag = between.length();
            result.plane = Vector4(between / mag, 0);
            result.field14 = mag - radius;
            return true;
        }
    }
    else if (other.kind == scene::VolumeKind::Box)
    {
        Vector3 between = pos - as_vector(other.box_position);
        float dot1 = formats::dot(as_vector(other.box_vector1), between);
        if (dot1 >= -radius && dot1 <= other.box_dot1 + radius)
        {
            float dot2 = formats::dot(as_vector(other.box_vector2), between);
            if (dot2 >= -radius && dot2 <= other.box_dot2 + radius)
            {
                float dot3 = formats::dot(as_vector(other.box_vector3), between);
                if (dot3 >= -radius && dot3 <= other.box_dot3 + radius)
                {
                    return true;
                }
            }
        }
    }
    return false;
}

bool check_cylinder_overlap_volume(const scene::EntityVolume& other, Vector3 bottom, Vector3 top,
    float radius, Result& result)
{
    if (other.kind == scene::VolumeKind::Cylinder)
    {
        return check_cylinders_overlap(bottom, top, as_vector(other.cylinder_position), as_vector(other.cylinder_vector), other.cylinder_dot,
            radius + other.cylinder_radius, result);
    }
    if (other.kind == scene::VolumeKind::Sphere)
    {
        return check_cylinder_overlap_sphere(bottom, top, as_vector(other.sphere_position), radius + other.sphere_radius, result);
    }
    return false;
}

bool check_cylinders_overlap(Vector3 oneBottom, Vector3 oneTop, Vector3 twoBottom, Vector3 twoVector,
    float twoDot, float radii, Result& result)
{
    float v9 = 0;
    float v10 = 1;
    Vector3 a = oneBottom - twoBottom;
    Vector3 b = oneTop - twoBottom;
    float v11 = formats::dot(a, twoVector);
    float v12 = formats::dot(b, twoVector);
    if (v11 >= 0)
    {
        if (v11 > twoDot)
        {
            if (v12 > twoDot)
            {
                return false;
            }
            if (v12 <= v11)
            {
                v9 = (v11 - twoDot) / (v11 - v12);
            }
            else
            {
                v9 = (v11 - twoDot) / (v12 - v11);
            }
        }
    }
    else
    {
        if (v12 < 0)
        {
            return false;
        }
        if (v12 <= v11)
        {
            v9 = -v11 / (v11 - v12);
        }
        else
        {
            v9 = -v11 / (v12 - v11);
        }
    }
    if (v12 >= 0)
    {
        if (v12 > twoDot)
        {
            if (v12 <= v11)
            {
                v10 = 1 - (v12 - twoDot) / (v11 - v12);
            }
            else
            {
                v10 = 1 - (v12 - twoDot) / (v12 - v11);
            }
        }
    }
    else if (v12 <= v11)
    {
        v10 = 1 - (-v12 / (v11 - v12));
    }
    else
    {
        v10 = 1 - (-v12 / (v12 - v11));
    }
    Vector3 c = twoVector * (v12 - v11);
    Vector3 d = oneTop - oneBottom;
    c = d - c;
    Vector3 e = twoBottom + twoVector * v11;
    float v15 = formats::dot(c, c);
    Vector3 f = e - oneBottom;
    float v16 = formats::dot(c, f);
    float v17 = v16 / v15;
    if (v17 >= v9)
    {
        if (v17 > v10)
        {
            v17 = v10;
        }
    }
    else
    {
        v17 = v9;
    }
    Vector3 g = oneBottom + c * v17;
    Vector3 h = g - e;
    float v19 = formats::dot(h, h);
    if (v19 > radii * radii)
    {
        return false;
    }
    float v20 = std::sqrt(v19);
    float v21 = std::sqrt(v15);
    float v22 = v17 - (radii - v20) / v21;
    if (v22 >= v9)
    {
        if (v22 > v10)
        {
            v22 = v10;
        }
    }
    else
    {
        v22 = v9;
    }
    result.field0 = 0;
    result.entity_collision = nullptr;
    result.flags = 0;
    result.position = oneBottom + d * v22;
    result.distance = v22;
    if (d.x != 0 || d.y != 0 || d.z != 0)
    {
        d = managed_normalized(d);
    }
    else
    {
        d.x = 1;
    }
    result.plane.x = -d.x;
    result.plane.y = -d.y;
    result.plane.z = -d.z;
    return true;
}

static bool check_cylinder_overlap_volumeHelper(const scene::EntityVolume& other, Vector3 bottom, Vector3 vector,
    float dot, float radius, Result& result)
{
    if (other.kind == scene::VolumeKind::Cylinder)
    {
        Result discard{}; // the game doesn't pass the result on
        Vector3 top = bottom + vector * dot;
        return check_cylinders_overlap(bottom, top, as_vector(other.cylinder_position), as_vector(other.cylinder_vector),
            other.cylinder_dot, other.cylinder_radius + radius, discard);
    }
    if (other.kind == scene::VolumeKind::Sphere)
    {
        Vector3 between = as_vector(other.sphere_position) - bottom;
        float dot1 = formats::dot(between, vector);
        if (dot1 <= dot + other.sphere_radius && dot1 >= -other.sphere_radius)
        {
            between -= vector * dot1;
            float radii = other.sphere_radius + radius;
            float dot2 = formats::dot(between, between);
            if (dot2 <= radii * radii)
            {
                result.field0 = 2;
                result.entity_collision = nullptr;
                result.flags = 0;
                if (dot < 0)
                {
                    result.plane = Vector4(-as_vector(other.sphere_position), 0);
                    result.field14 = -dot1;
                }
                else if (dot1 <= dot)
                {
                    float mag = between.length();
                    result.plane = Vector4(between / mag, 0);
                    result.field14 = mag - radius;
                }
                else
                {
                    result.plane = Vector4(as_vector(other.sphere_position), 0);
                    result.field14 = dot1 - dot;
                }
                return true;
            }
        }
    }
    return false;
}

bool check_volumes_overlap(const scene::EntityVolume& one, const scene::EntityVolume& two, Result& result)
{
    if (two.kind == scene::VolumeKind::Box)
    {
        if (one.kind == scene::VolumeKind::Sphere)
        {
            // will return correctly for sphere-box, but won't update result
            return check_sphere_overlap_volume(two, as_vector(one.sphere_position), one.sphere_radius, result);
        }
        if (one.kind == scene::VolumeKind::Cylinder)
        {
            // will always return false for cylinder-box
            return check_cylinder_overlap_volumeHelper(two, as_vector(one.cylinder_position), as_vector(one.cylinder_vector),
                one.cylinder_dot, one.cylinder_radius, result);
        }
        if (one.kind == scene::VolumeKind::Box)
        {
            // will always return false for box-box
            return false;
        }
    }
    else if (two.kind == scene::VolumeKind::Cylinder)
    {
        // will return correctly for sphere-cylinder
        // will return correctly for cylinder-cylinder, but won't update result
        // will always return false for box-cylinder
        return check_cylinder_overlap_volumeHelper(one, as_vector(two.cylinder_position), as_vector(two.cylinder_vector),
                two.cylinder_dot, two.cylinder_radius, result);
    }
    else if (two.kind == scene::VolumeKind::Sphere)
    {
        // will return correctly for sphere-sphere
        // will return correctly for cylinder-sphere
        // will return correctly for box-sphere, but won't update result
        return check_sphere_overlap_volume(one, as_vector(two.sphere_position), two.sphere_radius, result);
    }
    return false;
}

bool check_cylinder_overlap_sphere(Vector3 cylBot, Vector3 cylTop, Vector3 spherePos,
    float radii, Result& result)
{
    Vector3 a = cylTop - cylBot;
    float v7 = a.length();
    Vector3 b = spherePos - cylBot;
    if (v7 <= 0)
    {
        if (b.length_squared() <= radii * radii)
        {
            result.field0 = 0;
            result.entity_collision = nullptr;
            result.flags = 0;
            result.distance = 0;
            result.position = cylBot;
            result.plane.x = 1;
            result.plane.y = 0;
            result.plane.z = 0;
            return true;
        }
    }
    else
    {
        a = a / v7;
        float v12 = formats::dot(a, b);
        if (v12 >= -radii && v12 <= v7 + radii)
        {
            Vector3 c = b - (a * v12);
            if (c.length_squared() <= radii * radii)
            {
                result.field0 = 0;
                result.entity_collision = nullptr;
                result.flags = 0;
                Vector3 pos = spherePos - c;
                float v15 = formats::dot(c, c);
                float v16 = std::sqrt(radii * radii - v15);
                Vector3 d = a * v16;
                pos -= d;
                result.position = pos;
                float dist = v12 / (v7 + 2 * radii);
                if (dist > 1)
                {
                    dist = 1;
                }
                else if (dist < 0)
                {
                    dist = 0;
                }
                result.distance = dist;
                Vector3 normal = managed_normalized((pos - spherePos));
                result.plane.x = normal.x;
                result.plane.y = normal.y;
                result.plane.z = normal.z;
                return true;
            }
        }
    }
    return false;
}

bool check_cylinder_intersect_plane(Vector3 cylBot, Vector3 cylTop, Vector4 plane, Result& result)
{
    float v10 = plane.x * (cylTop.x - cylBot.x);
    float v13 = plane.y * (cylTop.y - cylBot.y);
    float v14 = plane.z * (cylTop.z - cylBot.z);
    float sum = v10 + v13 + v14;
    if (sum == 0)
    {
        return false;
    }
    float dist = (plane.w - (plane.x * cylBot.x + plane.y * cylBot.y + plane.z * cylBot.z)) / sum;
    if (dist < 0 || dist > 1)
    {
        return false;
    }
    result.position = cylBot + dist * (cylTop - cylBot);
    result.distance = dist;
    result.entity_collision = nullptr;
    return true;
}

bool check_cylinder_between_points(Vector3 point1, Vector3 point2, Vector3 cylPos,
    float cylHeight, float radii, Result& result)
{
    Vector3 travel = point2 - point1;
    float length = travel.length();
    travel = travel / length;
    Vector3 vec1 = cylPos - point1;
    float dot = formats::dot(travel, vec1);
    if (dot < -radii || dot > length + radii)
    {
        return false;
    }
    Vector3 vec2 = vec1 - travel * dot;
    if (vec2.y > 0 || vec2.y < -cylHeight)
    {
        return false;
    }
    if (vec2.x * vec2.x + vec2.z * vec2.z <= radii * radii)
    {
        result.field0 = 0;
        result.entity_collision = nullptr;
        result.flags = 0;
        result.position = cylPos - vec2;
        result.distance = std::clamp(dot / length, 0.0F, 1.0F);
        result.plane = Vector4(-travel, 0);
        return true;
    }
    return false;
}

bool check_port_between_points(const MphPortal& portal, Vector3 point1,
                               Vector3 point2, bool otherSide) {
    const Vector3 normal{portal.plane.x.to_float(), portal.plane.y.to_float(),
                         portal.plane.z.to_float()};
    float dotPrev = formats::dot(point1, normal) - portal.plane.w.to_float();
    float dotCur = formats::dot(point2, normal) - portal.plane.w.to_float();
    if (otherSide) {
        dotPrev *= -1;
        dotCur *= -1;
    }
    if (dotPrev > 0 && dotCur <= 0) {
        const float div = dotPrev / (dotPrev - dotCur);
        const Vector3 vec = point1 + (point2 - point1) * div;
        // Portal(RawCollisionPortal) always installs all four edge planes.
        for (const auto& plane : portal.planes) {
            const Vector3 edgeNormal{plane.x.to_float(), plane.y.to_float(),
                                     plane.z.to_float()};
            if (formats::dot(vec, edgeNormal) - plane.w.to_float()
                < -4224.0F / 4096.0F) {
                return false;
            }
        }
        return true;
    }
    return false;
}

} // namespace fruityprime::collision
