/*
 * Native translation of MphRead/Mods/MapGen/MapCollisionPacker.cs.
 *
 * This unit owns the collision face deduplication, spatial-cell indexing,
 * fixed-point conversion, and wc01 binary emission for generated rooms.
 */
#include "Mods/MapGen/map_collision_packer.hpp"

#include "Formats/collision_format.hpp"
#include "Formats/fixed.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <map>
#include <stdexcept>
#include <vector>

namespace fruityprime::mapgen::map_collision {
namespace {

[[nodiscard]] std::int32_t fixed_raw(float value) {
    return formats::Fixed::to_int(value);
}

[[nodiscard]] float dot(Vec3 left, Vec3 right) noexcept {
    return left.x * right.x + left.y * right.y + left.z * right.z;
}

void append_u16(std::vector<std::uint8_t>& bytes, std::uint16_t value) {
    bytes.push_back(static_cast<std::uint8_t>(value));
    bytes.push_back(static_cast<std::uint8_t>(value >> 8));
}

void append_i32(std::vector<std::uint8_t>& bytes, std::int32_t value) {
    const auto unsigned_value = static_cast<std::uint32_t>(value);
    bytes.push_back(static_cast<std::uint8_t>(unsigned_value));
    bytes.push_back(static_cast<std::uint8_t>(unsigned_value >> 8));
    bytes.push_back(static_cast<std::uint8_t>(unsigned_value >> 16));
    bytes.push_back(static_cast<std::uint8_t>(unsigned_value >> 24));
}

void append_u32(std::vector<std::uint8_t>& bytes, std::uint32_t value) {
    bytes.push_back(static_cast<std::uint8_t>(value));
    bytes.push_back(static_cast<std::uint8_t>(value >> 8));
    bytes.push_back(static_cast<std::uint8_t>(value >> 16));
    bytes.push_back(static_cast<std::uint8_t>(value >> 24));
}

void patch_u32(std::vector<std::uint8_t>& bytes, std::size_t offset,
               std::uint32_t value) {
    if (offset + 4 > bytes.size()) {
        throw std::runtime_error("map writer patch is outside the buffer");
    }
    bytes[offset] = static_cast<std::uint8_t>(value);
    bytes[offset + 1] = static_cast<std::uint8_t>(value >> 8);
    bytes[offset + 2] = static_cast<std::uint8_t>(value >> 16);
    bytes[offset + 3] = static_cast<std::uint8_t>(value >> 24);
}

[[nodiscard]] std::uint32_t offset32(std::size_t offset) {
    if (offset > std::numeric_limits<std::uint32_t>::max()) {
        throw std::runtime_error("generated map is larger than 4 GiB");
    }
    return static_cast<std::uint32_t>(offset);
}

void align4(std::vector<std::uint8_t>& bytes) {
    while ((bytes.size() & 3U) != 0) {
        bytes.push_back(0);
    }
}

} // namespace

struct CollisionRecord {
    std::uint16_t plane_index = 0;
    std::uint16_t flags = 0;
    std::uint16_t layer_mask = 0;
    std::uint16_t point_count = 0;
    std::uint16_t point_start = 0;
    Vec3 min;
    Vec3 max;
};

[[nodiscard]] std::vector<std::uint8_t> pack(
    const std::vector<CollisionFace>& faces, BuildStats& stats) {
    if (faces.empty()) {
        throw std::runtime_error("a map needs at least one solid face");
    }
    using PointKey = std::array<std::int32_t, 3>;
    using PlaneKey = std::array<std::int32_t, 4>;
    std::map<PointKey, std::uint16_t> point_ids;
    std::map<PlaneKey, std::uint16_t> plane_ids;
    std::vector<formats::Vector3Fx> points;
    std::vector<formats::Vector4Fx> planes;
    std::vector<std::uint16_t> point_indices;
    std::vector<CollisionRecord> records;
    Vec3 min{std::numeric_limits<float>::max(),
             std::numeric_limits<float>::max(),
             std::numeric_limits<float>::max()};
    Vec3 max{std::numeric_limits<float>::lowest(),
             std::numeric_limits<float>::lowest(),
             std::numeric_limits<float>::lowest()};

    for (const CollisionFace& face : faces) {
        if (face.points.size() < 3 || face.points.size() > 10) {
            throw std::runtime_error(
                "map collision face must contain between three and ten points");
        }
        const std::int32_t nx = fixed_raw(face.normal.x);
        const std::int32_t ny = fixed_raw(face.normal.y);
        const std::int32_t nz = fixed_raw(face.normal.z);
        const std::int32_t w = fixed_raw(dot(face.normal, face.points[0]));
        const PlaneKey plane_key{nx, ny, nz, w};
        std::uint16_t plane_index = 0;
        const auto plane_found = plane_ids.find(plane_key);
        if (plane_found == plane_ids.end()) {
            if (planes.size() > std::numeric_limits<std::uint16_t>::max()) {
                throw std::runtime_error("map has too many collision planes");
            }
            plane_index = static_cast<std::uint16_t>(planes.size());
            plane_ids.emplace(plane_key, plane_index);
            planes.push_back(formats::Vector4Fx{
                formats::Fixed{nx}, formats::Fixed{ny},
                formats::Fixed{nz}, formats::Fixed{w}});
        } else {
            plane_index = plane_found->second;
        }
        if (point_indices.size()
            > static_cast<std::size_t>(std::numeric_limits<std::uint16_t>::max())
                - face.points.size()) {
            throw std::runtime_error("map has too many collision point indices");
        }
        const std::uint16_t point_start = static_cast<std::uint16_t>(
            point_indices.size());
        for (std::size_t point_slot = 0;
             point_slot < face.points.size(); ++point_slot) {
            const Vec3 point = face.points[point_slot];
            const PointKey key{fixed_raw(point.x), fixed_raw(point.y),
                               fixed_raw(point.z)};
            std::uint16_t point_index = 0;
            const auto found = point_ids.find(key);
            if (found == point_ids.end()) {
                if (points.size() > std::numeric_limits<std::uint16_t>::max()) {
                    throw std::runtime_error("map has more than 65535 collision points");
                }
                point_index = static_cast<std::uint16_t>(points.size());
                point_ids.emplace(key, point_index);
                points.push_back(formats::Vector3Fx{
                    formats::Fixed{key[0]}, formats::Fixed{key[1]},
                    formats::Fixed{key[2]}});
            } else {
                point_index = found->second;
            }
            point_indices.push_back(point_index);
            min.x = std::min(min.x, point.x);
            min.y = std::min(min.y, point.y);
            min.z = std::min(min.z, point.z);
            max.x = std::max(max.x, point.x);
            max.y = std::max(max.y, point.y);
            max.z = std::max(max.z, point.z);
        }
        if (point_indices.size() >= std::numeric_limits<std::uint16_t>::max()) {
            throw std::runtime_error("map has too many collision point indices");
        }
        // The managed packer repeats the first point after the polygon. The
        // runtime uses that sentinel when walking the edge list, so preserve
        // it for imported triangles as well as hand-authored quads.
        point_indices.push_back(point_indices[point_start]);
        Vec3 face_min{std::numeric_limits<float>::max(),
                      std::numeric_limits<float>::max(),
                      std::numeric_limits<float>::max()};
        Vec3 face_max{std::numeric_limits<float>::lowest(),
                      std::numeric_limits<float>::lowest(),
                      std::numeric_limits<float>::lowest()};
        for (std::size_t point_index = 0;
             point_index < face.points.size(); ++point_index) {
            const Vec3 point = face.points[point_index];
            face_min.x = std::min(face_min.x, point.x);
            face_min.y = std::min(face_min.y, point.y);
            face_min.z = std::min(face_min.z, point.z);
            face_max.x = std::max(face_max.x, point.x);
            face_max.y = std::max(face_max.y, point.y);
            face_max.z = std::max(face_max.z, point.z);
        }
        const float abs_x = std::abs(face.normal.x);
        const float abs_y = std::abs(face.normal.y);
        const float abs_z = std::abs(face.normal.z);
        const std::uint16_t primary_axis = abs_y > abs_x && abs_y >= abs_z
            ? 1 : (abs_z > abs_x && abs_z > abs_y ? 2 : 0);
        records.push_back({plane_index, face.flags,
                           static_cast<std::uint16_t>(4 | primary_axis),
                           static_cast<std::uint16_t>(face.points.size()),
                           point_start, face_min, face_max});
    }

    const auto parts = [](float extent) {
        const double value = std::floor(static_cast<double>(extent) / 4.0) + 1.0;
        if (value < 1.0 || value > static_cast<double>(std::numeric_limits<int>::max())) {
            throw std::runtime_error("map collision grid dimensions are invalid");
        }
        return static_cast<int>(value);
    };
    const int parts_x = parts(max.x - min.x);
    const int parts_y = parts(max.y - min.y);
    const int parts_z = parts(max.z - min.z);
    const std::uint64_t cell_count = static_cast<std::uint64_t>(parts_x)
        * static_cast<std::uint64_t>(parts_y)
        * static_cast<std::uint64_t>(parts_z);
    if (cell_count > 1'000'000) {
        throw std::runtime_error("map collision grid has too many cells");
    }
    std::vector<std::vector<std::uint16_t>> cells(
        static_cast<std::size_t>(cell_count));
    const auto cell_index = [](float value, float origin, int count) {
        const int index = static_cast<int>(std::floor(
            (static_cast<double>(value) - origin) / 4.0));
        return std::clamp(index, 0, count - 1);
    };
    for (std::size_t face_index = 0; face_index < records.size(); ++face_index) {
        const CollisionRecord& record = records[face_index];
        const int x0 = cell_index(record.min.x, min.x, parts_x);
        const int x1 = cell_index(record.max.x, min.x, parts_x);
        const int y0 = cell_index(record.min.y, min.y, parts_y);
        const int y1 = cell_index(record.max.y, min.y, parts_y);
        const int z0 = cell_index(record.min.z, min.z, parts_z);
        const int z1 = cell_index(record.max.z, min.z, parts_z);
        for (int y = y0; y <= y1; ++y) {
            for (int z = z0; z <= z1; ++z) {
                for (int x = x0; x <= x1; ++x) {
                    const std::size_t index = static_cast<std::size_t>(y)
                        * static_cast<std::size_t>(parts_x)
                        * static_cast<std::size_t>(parts_z)
                        + static_cast<std::size_t>(z)
                            * static_cast<std::size_t>(parts_x)
                        + static_cast<std::size_t>(x);
                    if (cells[index].size()
                        >= std::numeric_limits<std::uint16_t>::max()) {
                        throw std::runtime_error("map collision cell has too many faces");
                    }
                    cells[index].push_back(static_cast<std::uint16_t>(face_index));
                }
            }
        }
    }

    std::vector<std::uint16_t> data_indices;
    std::vector<std::pair<std::uint16_t, std::uint16_t>> entries;
    entries.reserve(cells.size());
    for (const auto& cell : cells) {
        if (data_indices.size() > std::numeric_limits<std::uint16_t>::max()) {
            throw std::runtime_error("map collision index table is too large");
        }
        const auto start = static_cast<std::uint16_t>(data_indices.size());
        if (cell.size() > std::numeric_limits<std::uint16_t>::max()
            || data_indices.size() > std::numeric_limits<std::uint16_t>::max()
                - cell.size()) {
            throw std::runtime_error("map collision index table is too large");
        }
        data_indices.insert(data_indices.end(), cell.begin(), cell.end());
        entries.emplace_back(static_cast<std::uint16_t>(cell.size()), start);
    }

    std::vector<std::uint8_t> result(
        ::fruityprime::collision::MphHeader::Size, 0);
    const std::uint32_t point_offset = offset32(result.size());
    for (const auto point : points) {
        append_i32(result, point.x.value);
        append_i32(result, point.y.value);
        append_i32(result, point.z.value);
    }
    const std::uint32_t plane_offset = offset32(result.size());
    for (const auto plane : planes) {
        append_i32(result, plane.x.value);
        append_i32(result, plane.y.value);
        append_i32(result, plane.z.value);
        append_i32(result, plane.w.value);
    }
    const std::uint32_t point_index_offset = offset32(result.size());
    for (const std::uint16_t index : point_indices) {
        append_u16(result, index);
    }
    align4(result);
    const std::uint32_t data_offset = offset32(result.size());
    for (const CollisionRecord& record : records) {
        append_u32(result, 0);
        append_u16(result, record.plane_index);
        append_u16(result, record.flags);
        append_u16(result, record.layer_mask);
        append_u16(result, 0);
        append_u16(result, record.point_count);
        append_u16(result, record.point_start);
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
    result[0] = 'w';
    result[1] = 'c';
    result[2] = '0';
    result[3] = '1';
    patch_u32(result, 4, static_cast<std::uint32_t>(points.size()));
    patch_u32(result, 8, point_offset);
    patch_u32(result, 12, static_cast<std::uint32_t>(planes.size()));
    patch_u32(result, 16, plane_offset);
    patch_u32(result, 20, static_cast<std::uint32_t>(point_indices.size()));
    patch_u32(result, 24, point_index_offset);
    patch_u32(result, 28, static_cast<std::uint32_t>(records.size()));
    patch_u32(result, 32, data_offset);
    patch_u32(result, 36, static_cast<std::uint32_t>(data_indices.size()));
    patch_u32(result, 40, data_index_offset);
    patch_u32(result, 44, static_cast<std::uint32_t>(parts_x));
    patch_u32(result, 48, static_cast<std::uint32_t>(parts_y));
    patch_u32(result, 52, static_cast<std::uint32_t>(parts_z));
    patch_u32(result, 56, static_cast<std::uint32_t>(fixed_raw(min.x)));
    patch_u32(result, 60, static_cast<std::uint32_t>(fixed_raw(min.y)));
    patch_u32(result, 64, static_cast<std::uint32_t>(fixed_raw(min.z)));
    patch_u32(result, 68, static_cast<std::uint32_t>(entries.size()));
    patch_u32(result, 72, entry_offset);
    patch_u32(result, 76, 0);
    patch_u32(result, 80, portal_offset);
    stats.collision_faces = records.size();
    stats.collision_points = points.size();
    return result;
}
} // namespace fruityprime::mapgen::map_collision
