/*
 * Native translation of MphRead/Mods/MapGen/MapNodePacker.cs.
 *
 * This unit owns walkable-surface sampling, headroom rejection, neighbour
 * connectivity, BFS first-hop routes, and node-file serialization.
 */
#include "Mods/MapGen/map_node_packer.hpp"

#include "Formats/fixed.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <queue>
#include <stdexcept>
#include <utility>
#include <vector>

namespace fruityprime::mapgen::map_nodes {
namespace {

[[nodiscard]] float dot(Vec3 left, Vec3 right) noexcept {
    return left.x * right.x + left.y * right.y + left.z * right.z;
}

[[nodiscard]] std::int32_t fixed_raw(float value) {
    return formats::Fixed::to_int(value);
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

[[nodiscard]] std::uint32_t offset32(std::size_t offset) {
    if (offset > std::numeric_limits<std::uint32_t>::max()) {
        throw std::runtime_error("generated map is larger than 4 GiB");
    }
    return static_cast<std::uint32_t>(offset);
}

} // namespace

struct NavigationNode {
    Vec3 position;
    int cell = 0;
    std::vector<int> neighbours;
};

[[nodiscard]] bool face_contains_xz(const NavigationFace& face, float x, float z,
                                    float& y) {
    if (std::abs(face.normal.y) < 0.0001F) {
        return false;
    }
    float min_x = std::numeric_limits<float>::max();
    float max_x = std::numeric_limits<float>::lowest();
    float min_z = std::numeric_limits<float>::max();
    float max_z = std::numeric_limits<float>::lowest();
    for (std::size_t point_index = 0;
         point_index < face.points.size(); ++point_index) {
        const Vec3 point = face.points[point_index];
        min_x = std::min(min_x, point.x);
        max_x = std::max(max_x, point.x);
        min_z = std::min(min_z, point.z);
        max_z = std::max(max_z, point.z);
    }
    if (x < min_x - 0.01F || x > max_x + 0.01F
        || z < min_z - 0.01F || z > max_z + 0.01F) {
        return false;
    }
    y = (dot(face.normal, face.points[0]) - face.normal.x * x
         - face.normal.z * z) / face.normal.y;
    return std::isfinite(y);
}

[[nodiscard]] std::vector<std::uint8_t> pack(
    const std::vector<NavigationFace>& solid_faces, BuildStats& stats) {
    constexpr float spacing = 6.0F;
    constexpr float headroom = 1.9F;
    constexpr float reach = 1.6F;
    Vec3 min{std::numeric_limits<float>::max(),
             std::numeric_limits<float>::max(),
             std::numeric_limits<float>::max()};
    Vec3 max{std::numeric_limits<float>::lowest(),
             std::numeric_limits<float>::lowest(),
             std::numeric_limits<float>::lowest()};
    for (const NavigationFace& face : solid_faces) {
        for (std::size_t point_index = 0;
             point_index < face.points.size(); ++point_index) {
            const Vec3 point = face.points[point_index];
            min.x = std::min(min.x, point.x);
            min.y = std::min(min.y, point.y);
            min.z = std::min(min.z, point.z);
            max.x = std::max(max.x, point.x);
            max.y = std::max(max.y, point.y);
            max.z = std::max(max.z, point.z);
        }
    }
    const int columns = std::max(1, static_cast<int>(std::ceil(
        (max.x - min.x) / spacing)) + 1);
    const int rows = std::max(1, static_cast<int>(std::ceil(
        (max.z - min.z) / spacing)) + 1);
    std::vector<NavigationNode> nodes;
    std::vector<int> by_cell(static_cast<std::size_t>(columns * rows), -1);
    for (int row = 0; row < rows; ++row) {
        for (int column = 0; column < columns; ++column) {
            const float x = min.x + (static_cast<float>(column) + 0.5F) * spacing;
            const float z = min.z + (static_cast<float>(row) + 0.5F) * spacing;
            float selected_y = std::numeric_limits<float>::max();
            bool found = false;
            for (const NavigationFace& face : solid_faces) {
                if (face.normal.y < 0.7F) {
                    continue;
                }
                float y = 0.0F;
                if (face_contains_xz(face, x, z, y)) {
                    selected_y = std::min(selected_y, y);
                    found = true;
                }
            }
            if (!found) {
                continue;
            }
            bool blocked = false;
            for (const NavigationFace& face : solid_faces) {
                if (face.normal.y > -0.3F) {
                    continue;
                }
                float ceiling = 0.0F;
                if (face_contains_xz(face, x, z, ceiling)
                    && ceiling > selected_y + 0.2F
                    && ceiling < selected_y + headroom) {
                    blocked = true;
                    break;
                }
            }
            if (!blocked) {
                const int cell = row * columns + column;
                by_cell[static_cast<std::size_t>(cell)] =
                    static_cast<int>(nodes.size());
                nodes.push_back({{x, selected_y + 0.5F, z}, cell, {}});
            }
        }
    }
    for (std::size_t index = 0; index < nodes.size(); ++index) {
        const int cell = nodes[index].cell;
        const int row = cell / columns;
        const int column = cell % columns;
        for (int dr = -1; dr <= 1; ++dr) {
            for (int dc = -1; dc <= 1; ++dc) {
                if (dr == 0 && dc == 0) {
                    continue;
                }
                const int nr = row + dr;
                const int nc = column + dc;
                if (nr < 0 || nc < 0 || nr >= rows || nc >= columns) {
                    continue;
                }
                const int other = by_cell[static_cast<std::size_t>(nr * columns + nc)];
                if (other < 0 || std::find(nodes[index].neighbours.begin(),
                                           nodes[index].neighbours.end(), other)
                                   != nodes[index].neighbours.end()) {
                    continue;
                }
                const float rise = nodes[other].position.y - nodes[index].position.y;
                if (rise > 1.2F || rise < -8.0F) {
                    continue;
                }
                nodes[index].neighbours.push_back(other);
                nodes[other].neighbours.push_back(static_cast<int>(index));
            }
        }
    }

    if (nodes.size() > std::numeric_limits<std::uint16_t>::max()) {
        throw std::runtime_error("map has too many navigation nodes");
    }
    std::vector<std::vector<std::pair<std::uint16_t, std::uint16_t>>> routes(
        nodes.size());
    for (std::size_t source = 0; source < nodes.size(); ++source) {
        std::vector<int> distance(nodes.size(), -1);
        std::vector<std::uint16_t> hops(nodes.size(),
                                        static_cast<std::uint16_t>(source));
        std::queue<int> queue;
        distance[source] = 0;
        queue.push(static_cast<int>(source));
        while (!queue.empty()) {
            const int current = queue.front();
            queue.pop();
            for (const int neighbour : nodes[static_cast<std::size_t>(current)].neighbours) {
                if (distance[static_cast<std::size_t>(neighbour)] < 0) {
                    distance[static_cast<std::size_t>(neighbour)] =
                        distance[static_cast<std::size_t>(current)] + 1;
                    hops[static_cast<std::size_t>(neighbour)] =
                        static_cast<std::uint16_t>(current == static_cast<int>(source)
                                                       ? neighbour
                                                       : hops[static_cast<std::size_t>(current)]);
                    queue.push(neighbour);
                }
            }
        }
        // The native AI consumes a route for every destination. A BFS hop is
        // enough for this first grid generator; unreachable nodes point to
        // themselves, matching the managed writer's safe fallback.
        std::size_t start = 0;
        while (start < nodes.size()) {
            const std::uint16_t hop = distance[start] < 0
                ? static_cast<std::uint16_t>(start) : hops[start];
            std::size_t end = start + 1;
            while (end < nodes.size()) {
                const std::uint16_t next = distance[end] < 0
                    ? static_cast<std::uint16_t>(end) : hops[end];
                if (next != hop
                    || end - start >= std::numeric_limits<std::uint16_t>::max()) {
                    break;
                }
                ++end;
            }
            routes[source].emplace_back(static_cast<std::uint16_t>(end - start), hop);
            start = end;
        }
    }

    std::vector<std::uint8_t> result;
    const std::size_t node_offset = 32;
    const std::size_t values_offset = node_offset + nodes.size() * 36;
    std::vector<std::uint32_t> route_offsets(nodes.size());
    std::size_t cursor = values_offset;
    for (std::size_t i = 0; i < nodes.size(); ++i) {
        route_offsets[i] = offset32(cursor);
        cursor += routes[i].size() * 4;
    }
    append_u16(result, 6);
    append_u16(result, 1);
    append_u32(result, 14);
    append_u32(result, 16);
    append_u16(result, 0);
    append_u16(result, 0);
    append_u32(result, 24);
    append_u16(result, 1);
    append_u16(result, 0x5c);
    append_u32(result, static_cast<std::uint32_t>(node_offset));
    append_u16(result, static_cast<std::uint16_t>(nodes.size()));
    append_u16(result, 0x5c);
    for (std::size_t i = 0; i < nodes.size(); ++i) {
        const NavigationNode& node = nodes[i];
        append_u16(result, 0); // NodeType.Navigation
        append_u16(result, static_cast<std::uint16_t>(i));
        append_u16(result, 0);
        append_u16(result, 0);
        append_i32(result, fixed_raw(node.position.x));
        append_i32(result, fixed_raw(node.position.y));
        append_i32(result, fixed_raw(node.position.z));
        append_i32(result, fixed_raw(reach));
        append_u32(result, route_offsets[i]);
        append_u32(result, route_offsets[i]);
        append_u32(result, 0);
    }
    for (const auto& route : routes) {
        for (const auto& [run, hop] : route) {
            append_u16(result, run);
            append_u16(result, hop);
        }
    }
    stats.navigation_nodes = nodes.size();
    std::size_t edges = 0;
    for (const NavigationNode& node : nodes) {
        edges += node.neighbours.size();
    }
    stats.navigation_edges = edges / 2;
    return result;
}
} // namespace fruityprime::mapgen::map_nodes
