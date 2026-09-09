/*
 * Native translation of MphRead/Utility/RepackCollision.cs.
 *
 * This file intentionally owns the editor projection and both collision
 * writers.  MapGen has a smaller writer for newly generated faces; this
 * utility must preserve the managed repacker's portals, flags, point order,
 * fixed-point truncation, and spatial index layout.
 */
#include "Utility/repack_collision.hpp"

#include "Utility/binary_reader.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <functional>
#include <limits>
#include <stdexcept>
#include <string_view>
#include <utility>

namespace fruityprime::utility::repack_collision {
namespace {

using formats::Vector3;
using formats::Vector4;

constexpr std::uint16_t flag_value(CollisionFlag flag) noexcept {
    return static_cast<std::uint16_t>(flag);
}

[[nodiscard]] bool same(Vector3 left, Vector3 right) noexcept {
    return left.x == right.x && left.y == right.y && left.z == right.z;
}

[[nodiscard]] bool same(Vector4 left, Vector4 right) noexcept {
    return left.x == right.x && left.y == right.y
        && left.z == right.z && left.w == right.w;
}

[[nodiscard]] Vector3 to_float(formats::Vector3Fx value) noexcept {
    return {value.x.to_float(), value.y.to_float(), value.z.to_float()};
}

[[nodiscard]] Vector4 to_float(formats::Vector4Fx value) noexcept {
    return {value.x.to_float(), value.y.to_float(), value.z.to_float(),
            value.w.to_float()};
}

[[nodiscard]] std::int32_t fixed_raw(float value) {
    const double scaled = static_cast<double>(value) * 4096.0;
    if (!std::isfinite(scaled)
        || scaled < static_cast<double>(std::numeric_limits<std::int32_t>::min())
        || scaled > static_cast<double>(std::numeric_limits<std::int32_t>::max())) {
        throw std::invalid_argument("collision value does not fit fixed point");
    }
    // C# Fixed.ToInt uses a checked-free float-to-int conversion, which
    // truncates toward zero.  Do not round here: raw cartridge values must
    // survive an editor round trip byte-for-byte.
    return static_cast<std::int32_t>(scaled);
}

[[nodiscard]] std::uint16_t checked_u16(std::size_t value,
                                        std::string_view what) {
    if (value > std::numeric_limits<std::uint16_t>::max()) {
        throw std::invalid_argument(std::string(what) + " exceeds 16-bit range");
    }
    return static_cast<std::uint16_t>(value);
}

[[nodiscard]] std::uint32_t checked_u32(std::size_t value,
                                        std::string_view what) {
    if (value > std::numeric_limits<std::uint32_t>::max()) {
        throw std::invalid_argument(std::string(what) + " exceeds 32-bit range");
    }
    return static_cast<std::uint32_t>(value);
}

void append_u8(std::vector<std::uint8_t>& bytes, std::uint8_t value) {
    bytes.push_back(value);
}

void append_u16(std::vector<std::uint8_t>& bytes, std::uint16_t value) {
    bytes.push_back(static_cast<std::uint8_t>(value));
    bytes.push_back(static_cast<std::uint8_t>(value >> 8));
}

void append_u32(std::vector<std::uint8_t>& bytes, std::uint32_t value) {
    bytes.push_back(static_cast<std::uint8_t>(value));
    bytes.push_back(static_cast<std::uint8_t>(value >> 8));
    bytes.push_back(static_cast<std::uint8_t>(value >> 16));
    bytes.push_back(static_cast<std::uint8_t>(value >> 24));
}

void append_i32(std::vector<std::uint8_t>& bytes, std::int32_t value) {
    append_u32(bytes, static_cast<std::uint32_t>(value));
}

void append_fixed(std::vector<std::uint8_t>& bytes, float value) {
    append_i32(bytes, fixed_raw(value));
}

void append_vector3(std::vector<std::uint8_t>& bytes, Vector3 value) {
    append_fixed(bytes, value.x);
    append_fixed(bytes, value.y);
    append_fixed(bytes, value.z);
}

void append_vector4(std::vector<std::uint8_t>& bytes, Vector4 value) {
    append_fixed(bytes, value.x);
    append_fixed(bytes, value.y);
    append_fixed(bytes, value.z);
    append_fixed(bytes, value.w);
}

void append_string(std::vector<std::uint8_t>& bytes, std::string_view value,
                   std::size_t width) {
    if (value.size() > width) {
        throw std::invalid_argument("collision portal name is too long");
    }
    for (const unsigned char character : value) {
        bytes.push_back(character);
    }
    bytes.insert(bytes.end(), width - value.size(), 0);
}

void align4(std::vector<std::uint8_t>& bytes) {
    while ((bytes.size() & 3U) != 0) {
        bytes.push_back(0);
    }
}

[[nodiscard]] std::uint32_t offset32(std::size_t value) {
    return checked_u32(value, "collision offset");
}

[[nodiscard]] std::size_t find_point(
    std::span<const Vector3> points, Vector3 value) noexcept {
    for (std::size_t i = 0; i < points.size(); ++i) {
        if (same(points[i], value)) {
            return i;
        }
    }
    return points.size();
}

[[nodiscard]] std::size_t find_plane(
    std::span<const Vector4> planes, Vector4 value) noexcept {
    for (std::size_t i = 0; i < planes.size(); ++i) {
        if (same(planes[i], value)) {
            return i;
        }
    }
    return planes.size();
}

[[nodiscard]] std::array<Vector3, 8> cube_points(
    Vector3 min_bounds, Vector3 max_bounds) noexcept {
    const Vector3 p1{min_bounds.x, min_bounds.y, min_bounds.z};
    const Vector3 p2{max_bounds.x, min_bounds.y, min_bounds.z};
    const Vector3 p3{min_bounds.x, min_bounds.y, max_bounds.z};
    const Vector3 p4{max_bounds.x, min_bounds.y, max_bounds.z};
    const Vector3 p5{min_bounds.x, max_bounds.y, min_bounds.z};
    const Vector3 p6{max_bounds.x, max_bounds.y, min_bounds.z};
    const Vector3 p7{min_bounds.x, max_bounds.y, max_bounds.z};
    const Vector3 p8{max_bounds.x, max_bounds.y, max_bounds.z};
    return {p1, p2, p3, p4, p5, p6, p7, p8};
}

struct MphDataPack {
    const CollisionDataEditor* editor = nullptr;
    std::uint16_t plane_index = 0;
    std::uint16_t point_count = 0;
    std::uint16_t point_start = 0;
};

struct MphPortalPack {
    const Portal* portal = nullptr;
    std::uint16_t point_count = 0;
};

struct FhVectorPack {
    std::uint16_t point1_index = 0;
    std::uint16_t point2_index = 0;
    std::uint16_t plane_index = 0;
};

struct FhDataPack {
    const CollisionDataEditor* editor = nullptr;
    std::uint16_t plane_index = 0;
    std::uint16_t vector_count = 0;
    std::uint16_t vector_start = 0;
};

struct FhPortalPack {
    const Portal* portal = nullptr;
    std::uint16_t plane_index = 0;
    std::uint16_t vector_count = 0;
    std::uint16_t vector_start = 0;
};

struct TreeNodePack {
    Vector3 min_bounds;
    Vector3 max_bounds;
    std::size_t left_index = 0;
    std::size_t right_index = 0;
};

} // namespace

bool CollisionDataEditor::has(CollisionFlag flag) const noexcept {
    return (flags & flag_value(flag)) != 0;
}

void CollisionDataEditor::set(CollisionFlag flag, bool value) noexcept {
    if (value) {
        flags = static_cast<std::uint16_t>(flags | flag_value(flag));
    } else {
        flags = static_cast<std::uint16_t>(flags & ~flag_value(flag));
    }
}

bool CollisionDataEditor::damaging() const noexcept {
    return has(CollisionFlag::Damaging);
}

void CollisionDataEditor::set_damaging(bool value) noexcept {
    set(CollisionFlag::Damaging, value);
}

bool CollisionDataEditor::reflect() const noexcept {
    return has(CollisionFlag::ReflectBeams);
}

void CollisionDataEditor::set_reflect(bool value) noexcept {
    set(CollisionFlag::ReflectBeams, value);
}

bool CollisionDataEditor::players() const noexcept {
    return !has(CollisionFlag::IgnorePlayers);
}

void CollisionDataEditor::set_players(bool value) noexcept {
    set(CollisionFlag::IgnorePlayers, !value);
}

bool CollisionDataEditor::beams() const noexcept {
    return !has(CollisionFlag::IgnoreBeams);
}

void CollisionDataEditor::set_beams(bool value) noexcept {
    set(CollisionFlag::IgnoreBeams, !value);
}

bool CollisionDataEditor::scan() const noexcept {
    return !has(CollisionFlag::IgnoreScan);
}

void CollisionDataEditor::set_scan(bool value) noexcept {
    set(CollisionFlag::IgnoreScan, !value);
}

int CollisionDataEditor::slipperiness() const noexcept {
    return static_cast<int>((flags & 0x0018U) >> 3);
}

void CollisionDataEditor::set_slipperiness(int value) {
    if (value < 0 || value > 3) {
        throw std::invalid_argument("invalid slipperiness value");
    }
    flags = static_cast<std::uint16_t>(
        (flags & 0xffe7U) | (static_cast<std::uint16_t>(value) << 3));
}

formats::Terrain CollisionDataEditor::terrain() const noexcept {
    return static_cast<formats::Terrain>((flags & 0x01e0U) >> 5);
}

void CollisionDataEditor::set_terrain(formats::Terrain value) {
    const auto raw = static_cast<std::uint16_t>(value);
    if (raw > static_cast<std::uint16_t>(formats::Terrain::All)) {
        throw std::invalid_argument("invalid terrain value");
    }
    flags = static_cast<std::uint16_t>(
        (flags & 0xfe1fU) | (raw << 5));
}

int primary_axis(Vector3 normal) noexcept {
    const float x = std::abs(normal.x);
    const float y = std::abs(normal.y);
    const float z = std::abs(normal.z);
    if (y > x && y >= z) {
        return 1;
    }
    if (z > x && z > y) {
        return 2;
    }
    return 0;
}

Vector4 plane_from_edge(Vector3 point1, Vector3 point2, Vector3 normal) {
    // The managed implementation computes the plane perpendicular to the
    // portal edge and the supplied face normal, with its normal facing away
    // from the portal/face interior.
    const Vector3 point2_to_point1 = point1 - point2;
    const Vector3 cross = point2_to_point1.normalized();
    const Vector3 edge_normal = formats::cross(cross, normal).normalized();
    return {edge_normal, formats::dot(edge_normal, point1)};
}

void validate(const CollisionDataEditor& data) {
    if (data.points.size() < 3) {
        throw std::invalid_argument(
            "collision face must have at least 3 vertices");
    }
    if (data.points.size() > 10) {
        throw std::invalid_argument(
            "collision face may not have more than 10 vertices");
    }
}

bool test_intersection(Vector3 point1, Vector3 point2, Vector3 v0,
                       Vector3 v1, Vector3 v2, float& distance) noexcept {
    distance = 0.0F;
    constexpr float epsilon = 0.00001F;
    const Vector3 between = point2 - point1;
    const Vector3 direction = between.normalized();
    const Vector3 edge1 = v1 - v0;
    const Vector3 edge2 = v2 - v0;
    const Vector3 q = formats::cross(direction, edge2);
    const float a = formats::dot(edge1, q);
    if (std::abs(a) <= epsilon) {
        return false;
    }
    const Vector3 s = (point1 - v0) / a;
    const Vector3 r = formats::cross(s, edge1);
    const float b0 = formats::dot(s, q);
    const float b1 = formats::dot(r, direction);
    const float b2 = 1.0F - b0 - b1;
    if (b0 < 0.0F || b1 < 0.0F || b2 < 0.0F) {
        return false;
    }
    distance = formats::dot(edge2, r);
    return distance >= 0.0F;
}

bool check_intersection(Vector3 point1, Vector3 point2,
                        std::span<const Vector3> face) noexcept {
    if (face.size() < 3) {
        return false;
    }
    const float length = (point2 - point1).length();
    for (std::size_t i = 0; i + 2 < face.size(); ++i) {
        float distance = 0.0F;
        if (test_intersection(point1, point2, face[0], face[i + 1],
                              face[i + 2], distance)
            && distance <= length) {
            return true;
        }
    }
    return false;
}

std::vector<std::uint16_t> data_in_region(
    Vector3 min_bounds, Vector3 max_bounds,
    std::span<const CollisionDataEditor> data) {
    const auto cube = cube_points(min_bounds, max_bounds);
    const std::array<std::array<std::size_t, 4>, 6> faces{{
        {{0, 4, 5, 1}},
        {{0, 2, 3, 1}},
        {{0, 4, 6, 2}},
        {{2, 6, 7, 3}},
        {{1, 5, 7, 3}},
        {{4, 6, 7, 5}}
    }};
    const std::array<std::array<std::size_t, 2>, 12> edges{{
        {{0, 4}}, {{2, 6}}, {{1, 5}}, {{3, 7}},
        {{0, 2}}, {{1, 3}}, {{4, 6}}, {{5, 7}},
        {{0, 1}}, {{4, 5}}, {{2, 3}}, {{6, 7}}
    }};

    std::vector<std::uint16_t> indices;
    for (std::size_t item_index = 0; item_index < data.size(); ++item_index) {
        const CollisionDataEditor& item = data[item_index];
        if (item.points.empty()) {
            continue;
        }
        Vector3 item_min = item.points.front();
        Vector3 item_max = item.points.front();
        for (const Vector3 point : item.points) {
            item_min.x = std::min(item_min.x, point.x);
            item_min.y = std::min(item_min.y, point.y);
            item_min.z = std::min(item_min.z, point.z);
            item_max.x = std::max(item_max.x, point.x);
            item_max.y = std::max(item_max.y, point.y);
            item_max.z = std::max(item_max.z, point.z);
        }
        if (item_min.x > max_bounds.x || item_max.x < min_bounds.x
            || item_min.y > max_bounds.y || item_max.y < min_bounds.y
            || item_min.z > max_bounds.z || item_max.z < min_bounds.z) {
            continue;
        }

        bool intersects = false;
        for (const Vector3 point : item.points) {
            if (point.x >= min_bounds.x && point.x < max_bounds.x
                && point.y >= min_bounds.y && point.y < max_bounds.y
                && point.z >= min_bounds.z && point.z < max_bounds.z) {
                intersects = true;
                break;
            }
        }
        auto item_edge = [&](Vector3 start, Vector3 end) {
            for (const auto& face : faces) {
                std::array<Vector3, 4> polygon{
                    cube[face[0]], cube[face[1]], cube[face[2]], cube[face[3]]};
                if (check_intersection(start, end,
                                       std::span<const Vector3>(polygon))) {
                    return true;
                }
            }
            return false;
        };
        if (!intersects) {
            for (std::size_t point_index = 0;
                 point_index + 1 < item.points.size() && !intersects;
                 ++point_index) {
                intersects = item_edge(item.points[point_index],
                                       item.points[point_index + 1]);
            }
            if (!intersects) {
                intersects = item_edge(item.points.back(), item.points.front());
            }
        }
        if (!intersects) {
            for (const auto& edge : edges) {
                std::array<Vector3, 10> polygon{};
                std::copy(item.points.begin(), item.points.end(), polygon.begin());
                if (check_intersection(cube[edge[0]], cube[edge[1]],
                                       std::span<const Vector3>(
                                           polygon.data(), item.points.size()))) {
                    intersects = true;
                    break;
                }
            }
        }
        if (intersects) {
            if (item_index > std::numeric_limits<std::uint16_t>::max()) {
                throw std::invalid_argument("collision data index exceeds 16-bit range");
            }
            indices.push_back(static_cast<std::uint16_t>(item_index));
        }
    }
    return indices;
}

std::vector<CollisionDataEditor> editors(const collision::MphDataSet& info) {
    std::vector<CollisionDataEditor> result;
    result.reserve(info.data.size());
    for (const collision::MphData& raw : info.data) {
        if (raw.plane_index >= info.planes.size()) {
            throw std::invalid_argument("MPH collision plane index is invalid");
        }
        CollisionDataEditor editor;
        const Vector4 plane = to_float(info.planes[raw.plane_index]);
        editor.layer_mask = static_cast<std::uint16_t>(
            (raw.layer_mask & 0xfffcU)
            | static_cast<std::uint16_t>(primary_axis(plane.xyz())));
        editor.flags = raw.flags;
        const std::size_t end = static_cast<std::size_t>(raw.point_start_index)
            + raw.point_index_count;
        if (end > info.point_indices.size()) {
            throw std::invalid_argument("MPH collision point index range is invalid");
        }
        editor.plane = plane;
        editor.points.reserve(raw.point_index_count);
        for (std::size_t i = 0; i < raw.point_index_count; ++i) {
            const std::uint16_t point_index =
                info.point_indices[raw.point_start_index + i];
            if (point_index >= info.points.size()) {
                throw std::invalid_argument("MPH collision point index is invalid");
            }
            editor.points.push_back(to_float(info.points[point_index]));
        }
        result.push_back(std::move(editor));
    }
    return result;
}

std::vector<CollisionDataEditor> editors(const collision::FhDataSet& info) {
    if (info.header.portal_count > info.data.size()) {
        throw std::invalid_argument("First Hunt portal count is invalid");
    }
    const std::size_t face_count =
        info.data.size() - static_cast<std::size_t>(info.header.portal_count);
    std::vector<CollisionDataEditor> result;
    result.reserve(face_count);
    for (std::size_t i = 0; i < face_count; ++i) {
        const collision::FhData& raw =
            info.data[i + static_cast<std::size_t>(info.header.portal_count)];
        if (raw.plane_index >= info.planes.size()) {
            throw std::invalid_argument("First Hunt collision plane index is invalid");
        }
        CollisionDataEditor editor;
        editor.plane = to_float(info.planes[raw.plane_index]);
        editor.layer_mask = static_cast<std::uint16_t>(
            4U | static_cast<std::uint16_t>(primary_axis(editor.plane.xyz())));
        const std::size_t end = static_cast<std::size_t>(raw.vector_start_index)
            + raw.vector_count;
        if (end > info.vectors.size()) {
            throw std::invalid_argument("First Hunt collision vector range is invalid");
        }
        editor.points.reserve(raw.vector_count);
        for (std::size_t j = 0; j < raw.vector_count; ++j) {
            const collision::FhVector& vector =
                info.vectors[raw.vector_start_index + j];
            if (vector.point2_index >= info.points.size()) {
                throw std::invalid_argument("First Hunt collision point index is invalid");
            }
            editor.points.push_back(to_float(info.points[vector.point2_index]));
        }
        result.push_back(std::move(editor));
    }
    return result;
}

std::vector<Portal> portals(const collision::MphDataSet& info) {
    std::vector<Portal> result;
    result.reserve(info.portals.size());
    for (const collision::MphPortal& raw : info.portals) {
        Portal portal;
        portal.name = raw.name;
        portal.node_name1 = raw.node_name1;
        portal.node_name2 = raw.node_name2;
        portal.layer_mask = raw.layer_mask;
        portal.flags = raw.flags;
        portal.unknown00 = raw.unused_00;
        portal.unknown01 = raw.unused_01;
        portal.points.reserve(raw.points.size());
        portal.planes.reserve(raw.planes.size());
        for (const formats::Vector3Fx point : raw.points) {
            portal.points.push_back(to_float(point));
        }
        for (const formats::Vector4Fx plane : raw.planes) {
            portal.planes.push_back(to_float(plane));
        }
        portal.plane = to_float(raw.plane);
        result.push_back(std::move(portal));
    }
    return result;
}

std::vector<Portal> portals(const collision::FhDataSet& info) {
    std::vector<Portal> result;
    result.reserve(info.portals.size());
    for (const collision::FhPortal& raw : info.portals) {
        Portal portal;
        portal.name = raw.name;
        portal.node_name1 = raw.node_name1;
        portal.node_name2 = raw.node_name2;
        portal.layer_mask = 4;
        portal.unknown00 = raw.field_5c;
        portal.unknown01 = raw.field_5d;
        portal.plane = to_float(raw.plane);
        const std::size_t end = static_cast<std::size_t>(raw.vector_start_index)
            + raw.vector_count;
        if (end > info.vectors.size()) {
            throw std::invalid_argument("First Hunt portal vector range is invalid");
        }
        portal.points.reserve(raw.vector_count);
        portal.planes.reserve(raw.vector_count);
        for (std::size_t i = 0; i < raw.vector_count; ++i) {
            const collision::FhVector& vector =
                info.vectors[raw.vector_start_index + i];
            if (vector.point2_index >= info.points.size()
                || vector.plane_index >= info.planes.size()) {
                throw std::invalid_argument("First Hunt portal index is invalid");
            }
            portal.points.push_back(to_float(info.points[vector.point2_index]));
            portal.planes.push_back(to_float(info.planes[vector.plane_index]));
        }
        result.push_back(std::move(portal));
    }
    return result;
}

std::vector<std::uint8_t> repack_mph(
    std::span<const CollisionDataEditor> data,
    std::span<const Portal> portal_values) {
    if (data.empty()) {
        throw std::invalid_argument("MPH collision needs at least one face");
    }

    std::vector<Vector3> points;
    std::vector<Vector4> planes;
    std::vector<std::uint16_t> point_indices;
    std::vector<MphDataPack> data_packs;
    points.reserve(data.size() * 3);
    planes.reserve(data.size());
    data_packs.reserve(data.size());

    float min_x = std::numeric_limits<float>::max();
    float min_y = std::numeric_limits<float>::max();
    float min_z = std::numeric_limits<float>::max();
    float max_x = std::numeric_limits<float>::lowest();
    float max_y = std::numeric_limits<float>::lowest();
    float max_z = std::numeric_limits<float>::lowest();

    for (const CollisionDataEditor& item : data) {
        validate(item);
        for (const Vector3 point : item.points) {
            if (find_point(std::span<const Vector3>(points), point)
                == points.size()) {
                points.push_back(point);
            }
            min_x = std::min(min_x, point.x);
            min_y = std::min(min_y, point.y);
            min_z = std::min(min_z, point.z);
            max_x = std::max(max_x, point.x);
            max_y = std::max(max_y, point.y);
            max_z = std::max(max_z, point.z);
        }
        std::size_t plane_index =
            find_plane(std::span<const Vector4>(planes), item.plane);
        if (plane_index == planes.size()) {
            planes.push_back(item.plane);
        }
        const std::size_t point_start = point_indices.size();
        for (const Vector3 point : item.points) {
            const std::size_t point_index =
                find_point(std::span<const Vector3>(points), point);
            point_indices.push_back(checked_u16(point_index, "MPH point index"));
        }
        point_indices.push_back(point_indices[point_start]);
        data_packs.push_back({
            &item,
            checked_u16(plane_index, "MPH plane index"),
            checked_u16(item.points.size(), "MPH point count"),
            checked_u16(point_start, "MPH point start")});
    }

    int parts_x = 1;
    int parts_y = 1;
    int parts_z = 1;
    while (min_x + static_cast<float>(parts_x) * 4.0F <= max_x) {
        ++parts_x;
    }
    while (min_y + static_cast<float>(parts_y) * 4.0F <= max_y) {
        ++parts_y;
    }
    while (min_z + static_cast<float>(parts_z) * 4.0F <= max_z) {
        ++parts_z;
    }

    std::vector<std::uint16_t> data_indices;
    std::vector<std::pair<std::uint16_t, std::uint16_t>> entries;
    const std::size_t cell_count = static_cast<std::size_t>(parts_x)
        * static_cast<std::size_t>(parts_y)
        * static_cast<std::size_t>(parts_z);
    entries.reserve(cell_count);
    for (int py = 0; py < parts_y; ++py) {
        for (int pz = 0; pz < parts_z; ++pz) {
            for (int px = 0; px < parts_x; ++px) {
                const Vector3 min_bounds{
                    min_x + static_cast<float>(px) * 4.0F,
                    min_y + static_cast<float>(py) * 4.0F,
                    min_z + static_cast<float>(pz) * 4.0F};
                const Vector3 max_bounds{
                    min_bounds.x + 4.0F, min_bounds.y + 4.0F,
                    min_bounds.z + 4.0F};
                const std::size_t start = data_indices.size();
                const auto indices = data_in_region(min_bounds, max_bounds, data);
                data_indices.insert(data_indices.end(), indices.begin(),
                                    indices.end());
                if (data_indices.size() > std::numeric_limits<std::uint16_t>::max()) {
                    throw std::invalid_argument("MPH data index table is too large");
                }
                entries.emplace_back(
                    checked_u16(data_indices.size() - start,
                                "MPH entry data count"),
                    checked_u16(start, "MPH entry data start"));
            }
        }
    }

    std::vector<std::uint8_t> result(collision::MphHeader::Size, 0);
    const std::uint32_t point_offset = offset32(result.size());
    for (const Vector3 point : points) {
        append_vector3(result, point);
    }
    const std::uint32_t plane_offset = offset32(result.size());
    for (const Vector4 plane : planes) {
        append_vector4(result, plane);
    }
    const std::uint32_t point_index_offset = offset32(result.size());
    for (const std::uint16_t index : point_indices) {
        append_u16(result, index);
    }
    align4(result);
    const std::uint32_t data_offset = offset32(result.size());
    for (const MphDataPack& pack : data_packs) {
        append_i32(result, 0);
        append_u16(result, pack.plane_index);
        append_u16(result, pack.editor->flags);
        append_u16(result, pack.editor->layer_mask);
        append_u16(result, 0);
        append_u16(result, pack.point_count);
        append_u16(result, pack.point_start);
    }
    const std::uint32_t data_index_offset = offset32(result.size());
    for (const std::uint16_t index : data_indices) {
        append_u16(result, index);
    }
    align4(result);
    const std::uint32_t entry_offset = offset32(result.size());
    for (const auto& [count, start] : entries) {
        append_u16(result, count);
        append_u16(result, start);
    }
    const std::uint32_t portal_offset = offset32(result.size());
    for (const Portal& portal : portal_values) {
        if (portal.points.size() != 4 || portal.planes.size() != 4) {
            throw std::invalid_argument(
                "MPH collision portals must have four points and planes");
        }
        append_string(result, portal.name, 40);
        append_string(result, portal.node_name1, 24);
        append_string(result, portal.node_name2, 24);
        for (const Vector3 point : portal.points) {
            append_vector3(result, point);
        }
        for (const Vector4 plane : portal.planes) {
            append_vector4(result, plane);
        }
        append_vector4(result, portal.plane);
        append_u16(result, portal.flags);
        append_u16(result, portal.layer_mask);
        append_u16(result, 4);
        append_u8(result, portal.unknown00);
        append_u8(result, portal.unknown01);
    }

    result[0] = 'w';
    result[1] = 'c';
    result[2] = '0';
    result[3] = '1';
    auto patch_u32 = [&result](std::size_t offset, std::uint32_t value) {
        result[offset] = static_cast<std::uint8_t>(value);
        result[offset + 1] = static_cast<std::uint8_t>(value >> 8);
        result[offset + 2] = static_cast<std::uint8_t>(value >> 16);
        result[offset + 3] = static_cast<std::uint8_t>(value >> 24);
    };
    patch_u32(4, checked_u32(points.size(), "MPH point count"));
    patch_u32(8, point_offset);
    patch_u32(12, checked_u32(planes.size(), "MPH plane count"));
    patch_u32(16, plane_offset);
    patch_u32(20, checked_u32(point_indices.size(), "MPH point index count"));
    patch_u32(24, point_index_offset);
    patch_u32(28, checked_u32(data_packs.size(), "MPH data count"));
    patch_u32(32, data_offset);
    patch_u32(36, checked_u32(data_indices.size(), "MPH data index count"));
    patch_u32(40, data_index_offset);
    patch_u32(44, static_cast<std::uint32_t>(parts_x));
    patch_u32(48, static_cast<std::uint32_t>(parts_y));
    patch_u32(52, static_cast<std::uint32_t>(parts_z));
    patch_u32(56, static_cast<std::uint32_t>(fixed_raw(min_x)));
    patch_u32(60, static_cast<std::uint32_t>(fixed_raw(min_y)));
    patch_u32(64, static_cast<std::uint32_t>(fixed_raw(min_z)));
    patch_u32(68, checked_u32(entries.size(), "MPH entry count"));
    patch_u32(72, entry_offset);
    patch_u32(76, checked_u32(portal_values.size(), "MPH portal count"));
    patch_u32(80, portal_offset);
    return result;
}

std::vector<std::uint8_t> repack_first_hunt(
    std::span<const CollisionDataEditor> data,
    std::span<const Portal> portal_values) {
    if (data.empty() && portal_values.empty()) {
        throw std::invalid_argument(
            "First Hunt collision needs faces or portals");
    }

    std::vector<Vector3> points;
    std::vector<Vector4> planes;
    std::vector<FhVectorPack> vectors;
    std::vector<FhPortalPack> portal_packs;
    std::vector<FhDataPack> data_packs;
    float min_x = std::numeric_limits<float>::max();
    float min_y = std::numeric_limits<float>::max();
    float min_z = std::numeric_limits<float>::max();
    float max_x = std::numeric_limits<float>::lowest();
    float max_y = std::numeric_limits<float>::lowest();
    float max_z = std::numeric_limits<float>::lowest();

    auto add_items = [&](std::span<const Vector3> vertices, Vector3 normal) {
        if (vertices.size() < 2) {
            throw std::invalid_argument(
                "First Hunt collision polygon needs at least two points");
        }
        for (std::size_t i = 0; i < vertices.size(); ++i) {
            const Vector3 point1 = vertices[i == 0 ? vertices.size() - 1 : i - 1];
            const Vector3 point2 = vertices[i];
            const Vector4 edge_plane = plane_from_edge(point1, point2, normal);
            std::size_t point1_index =
                find_point(std::span<const Vector3>(points), point1);
            std::size_t point2_index =
                find_point(std::span<const Vector3>(points), point2);
            std::size_t plane_index =
                find_plane(std::span<const Vector4>(planes), edge_plane);
            const bool point1_missing = point1_index == points.size();
            const bool point2_missing = point2_index == points.size();
            if (point1_missing) {
                point1_index = points.size();
                points.push_back(point1);
            }
            if (point2_missing) {
                point2_index = points.size();
                points.push_back(point2);
            }
            if (plane_index == planes.size()) {
                plane_index = planes.size();
                planes.push_back(edge_plane);
            }
            vectors.push_back({
                checked_u16(point1_index, "First Hunt point index"),
                checked_u16(point2_index, "First Hunt point index"),
                checked_u16(plane_index, "First Hunt plane index")});
            min_x = std::min(min_x, point2.x);
            min_y = std::min(min_y, point2.y);
            min_z = std::min(min_z, point2.z);
            max_x = std::max(max_x, point2.x);
            max_y = std::max(max_y, point2.y);
            max_z = std::max(max_z, point2.z);
        }
    };

    for (const Portal& portal : portal_values) {
        std::size_t plane_index =
            find_plane(std::span<const Vector4>(planes), portal.plane);
        if (plane_index == planes.size()) {
            plane_index = planes.size();
            planes.push_back(portal.plane);
        }
        const std::size_t vector_start = vectors.size();
        add_items(portal.points, portal.plane.xyz());
        portal_packs.push_back({
            &portal,
            checked_u16(plane_index, "First Hunt portal plane index"),
            checked_u16(portal.points.size(), "First Hunt portal vector count"),
            checked_u16(vector_start, "First Hunt portal vector start")});
    }
    for (const CollisionDataEditor& item : data) {
        validate(item);
        std::size_t plane_index =
            find_plane(std::span<const Vector4>(planes), item.plane);
        if (plane_index == planes.size()) {
            plane_index = planes.size();
            planes.push_back(item.plane);
        }
        const std::size_t vector_start = vectors.size();
        add_items(item.points, item.plane.xyz());
        data_packs.push_back({
            &item,
            checked_u16(plane_index, "First Hunt data plane index"),
            checked_u16(item.points.size(), "First Hunt data vector count"),
            checked_u16(vector_start, "First Hunt data vector start")});
    }

    std::vector<TreeNodePack> tree_nodes;
    const float root_min = formats::Fixed::to_float(0x50000000);
    const float root_max = formats::Fixed::to_float(
        static_cast<std::int32_t>(0xb0000000U));
    tree_nodes.push_back({
        {root_min, root_min, root_min},
        {root_max, root_max, root_max},
        0xcdcdU,
        0xcdcdU});
    tree_nodes.push_back({
        {min_x, min_y, min_z},
        {max_x, max_y, max_z},
        0,
        0});

    std::function<void(std::size_t)> make_nodes =
        [&](std::size_t parent_index) {
            const Vector3 min_bounds = tree_nodes[parent_index].min_bounds;
            const Vector3 max_bounds = tree_nodes[parent_index].max_bounds;
            const float size_x = max_bounds.x - min_bounds.x;
            const float size_y = max_bounds.y - min_bounds.y;
            const float size_z = max_bounds.z - min_bounds.z;
            if (size_x < 8.0F && size_y < 8.0F && size_z < 8.0F) {
                tree_nodes[parent_index].left_index = 0x8000U;
                tree_nodes[parent_index].right_index = 0x8000U;
                return;
            }

            Vector3 left_max = max_bounds;
            Vector3 right_min = min_bounds;
            if (size_x >= size_y && size_x >= size_z) {
                left_max.x = max_bounds.x - size_x / 2.0F;
                right_min.x = min_bounds.x + size_x / 2.0F;
            } else if (size_y > size_x && size_y >= size_z) {
                left_max.y = max_bounds.y - size_y / 2.0F;
                right_min.y = min_bounds.y + size_y / 2.0F;
            } else {
                left_max.z = max_bounds.z - size_z / 2.0F;
                right_min.z = min_bounds.z + size_z / 2.0F;
            }
            const std::size_t left_index = tree_nodes.size();
            tree_nodes.push_back({min_bounds, left_max, 0, 0});
            const std::size_t right_index = tree_nodes.size();
            tree_nodes.push_back({right_min, max_bounds, 0, 0});
            tree_nodes[parent_index].left_index = left_index;
            tree_nodes[parent_index].right_index = right_index;
            make_nodes(left_index);
            make_nodes(right_index);
        };
    make_nodes(1);

    std::vector<std::uint16_t> data_indices;
    std::vector<TreeNodePack> entries;
    for (TreeNodePack& node : tree_nodes) {
        if (node.right_index != 0x8000U) {
            continue;
        }
        node.left_index = entries.size();
        const std::size_t start = data_indices.size();
        const auto indices = data_in_region(node.min_bounds, node.max_bounds, data);
        data_indices.insert(data_indices.end(), indices.begin(), indices.end());
        if (data_indices.size() > std::numeric_limits<std::uint16_t>::max()) {
            throw std::invalid_argument(
                "First Hunt data index table is too large");
        }
        entries.push_back({
            node.min_bounds,
            node.max_bounds,
            data_indices.size() - start,
            start});
    }
    for (std::uint16_t& index : data_indices) {
        const std::size_t adjusted =
            static_cast<std::size_t>(index) + portal_values.size();
        index = checked_u16(adjusted, "First Hunt portal data index");
    }

    std::vector<std::uint8_t> result(collision::FhHeader::Size, 0);
    const std::uint32_t point_offset = offset32(result.size());
    for (const Vector3 point : points) {
        append_vector3(result, point);
    }
    const std::uint32_t plane_offset = offset32(result.size());
    for (const Vector4 plane : planes) {
        append_vector4(result, plane);
    }
    const std::uint32_t vector_offset = offset32(result.size());
    for (const FhVectorPack& vector : vectors) {
        append_u16(result, vector.point1_index);
        append_u16(result, vector.point2_index);
        append_u16(result, vector.plane_index);
    }
    align4(result);
    const std::uint32_t data_offset = offset32(result.size());
    for (const FhPortalPack& portal : portal_packs) {
        append_u16(result, portal.plane_index);
        append_u16(result, portal.vector_count);
        append_u16(result, portal.vector_start);
    }
    for (const FhDataPack& item : data_packs) {
        append_u16(result, item.plane_index);
        append_u16(result, item.vector_count);
        append_u16(result, item.vector_start);
    }
    align4(result);
    const std::uint32_t data_index_offset = offset32(result.size());
    for (const std::uint16_t index : data_indices) {
        append_u16(result, index);
    }
    align4(result);
    const std::uint32_t entry_offset = offset32(result.size());
    for (const TreeNodePack& entry : entries) {
        append_vector3(result, entry.min_bounds);
        append_vector3(result, entry.max_bounds);
        append_u16(result, checked_u16(entry.left_index,
                                      "First Hunt entry data count"));
        append_u16(result, checked_u16(entry.right_index,
                                      "First Hunt entry data start"));
    }
    const std::uint32_t tree_node_index_offset = offset32(result.size());
    append_u16(result, 1);
    append_u16(result, 0);
    const std::uint32_t tree_node_offset = offset32(result.size());
    for (const TreeNodePack& node : tree_nodes) {
        append_vector3(result, node.min_bounds);
        append_vector3(result, node.max_bounds);
        append_u16(result, checked_u16(node.left_index,
                                      "First Hunt tree node left index"));
        append_u16(result, checked_u16(node.right_index,
                                      "First Hunt tree node right index"));
    }
    const std::uint32_t portal_offset = offset32(result.size());
    for (const FhPortalPack& portal : portal_packs) {
        append_string(result, portal.portal->name, 40);
        append_string(result, portal.portal->node_name1, 16);
        append_string(result, portal.portal->node_name2, 16);
        append_vector4(result, portal.portal->plane);
        append_u16(result, portal.vector_count);
        append_u16(result, portal.vector_start);
        append_u8(result, portal.portal->unknown00);
        append_u8(result, portal.portal->unknown01);
        append_u16(result, 0);
    }

    auto patch_u16 = [&result](std::size_t offset, std::uint16_t value) {
        result[offset] = static_cast<std::uint8_t>(value);
        result[offset + 1] = static_cast<std::uint8_t>(value >> 8);
    };
    auto patch_u32 = [&result](std::size_t offset, std::uint32_t value) {
        result[offset] = static_cast<std::uint8_t>(value);
        result[offset + 1] = static_cast<std::uint8_t>(value >> 8);
        result[offset + 2] = static_cast<std::uint8_t>(value >> 16);
        result[offset + 3] = static_cast<std::uint8_t>(value >> 24);
    };
    patch_u32(0, checked_u32(points.size(), "First Hunt point count"));
    patch_u32(4, point_offset);
    patch_u32(8, checked_u32(planes.size(), "First Hunt plane count"));
    patch_u32(12, plane_offset);
    patch_u32(16, checked_u32(vectors.size(), "First Hunt vector count"));
    patch_u32(20, vector_offset);
    patch_u16(24, checked_u16(data.size() + portal_values.size(),
                              "First Hunt data count"));
    patch_u16(26, 0);
    patch_u32(28, data_offset);
    patch_u32(32, checked_u32(data_indices.size(),
                              "First Hunt data index count"));
    patch_u32(36, data_index_offset);
    patch_u32(40, checked_u32(entries.size(), "First Hunt entry count"));
    patch_u32(44, entry_offset);
    patch_u32(48, 1);
    patch_u32(52, tree_node_index_offset);
    patch_u32(56, checked_u32(tree_nodes.size(),
                              "First Hunt tree node count"));
    patch_u32(60, tree_node_offset);
    patch_u32(64, checked_u32(portal_values.size(),
                              "First Hunt portal count"));
    patch_u32(68, portal_offset);
    return result;
}

std::vector<std::uint8_t> repack(const collision::File& file) {
    if (file.is_mph()) {
        const auto data = editors(file.mph());
        const auto portal_values = portals(file.mph());
        return repack_mph(data, portal_values);
    }
    const auto data = editors(file.first_hunt());
    const auto portal_values = portals(file.first_hunt());
    return repack_first_hunt(data, portal_values);
}

std::vector<std::uint8_t> repack_file(const std::filesystem::path& path) {
    return repack(collision::File::read_file(path));
}

void print_summary(const collision::File& file, std::ostream& output) {
    if (file.is_mph()) {
        const auto& data = file.mph();
        output << "format=MPH type="
               << std::string(data.header.type.begin(), data.header.type.end())
               << " points=" << data.points.size()
               << " planes=" << data.planes.size()
               << " data=" << data.data.size()
               << " entries=" << data.entries.size()
               << " portals=" << data.portals.size() << '\n';
    } else {
        const auto& data = file.first_hunt();
        output << "format=FirstHunt points=" << data.points.size()
               << " planes=" << data.planes.size()
               << " vectors=" << data.vectors.size()
               << " data=" << data.data.size()
               << " entries=" << data.entries.size()
               << " tree_nodes=" << data.tree_nodes.size()
               << " portals=" << data.portals.size() << '\n';
    }
}

} // namespace fruityprime::utility::repack_collision
