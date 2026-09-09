#include "Formats/collision_runtime.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <set>

namespace fruityprime::collision {
namespace {

using formats::Vector3;
using formats::Vector4;

[[nodiscard]] float dot3(Vector3 left, Vector3 right) noexcept {
    return left.x * right.x + left.y * right.y + left.z * right.z;
}

[[nodiscard]] Vector3 xyz(const Vector4& value) noexcept {
    return {value.x, value.y, value.z};
}

// Vector4.AddW: the managed helper the queries use to slide a plane along a
// part's translation.
[[nodiscard]] Vector4 move_plane(const Vector4& plane,
                                 Vector3 translation) noexcept {
    if (translation.x == 0.0F && translation.y == 0.0F
        && translation.z == 0.0F) {
        return plane;
    }
    return {plane.x, plane.y, plane.z, plane.w + dot3(xyz(plane), translation)};
}

[[nodiscard]] std::uint16_t flag_mask(TestFlags flags) noexcept {
    std::uint16_t mask = 0;
    if (has_flag(flags, TestFlags::Players)) {
        mask |= static_cast<std::uint16_t>(Flags::IgnorePlayers);
    }
    if (has_flag(flags, TestFlags::Beams)) {
        mask |= static_cast<std::uint16_t>(Flags::IgnoreBeams);
    }
    return mask;
}

// The managed code reads pointIndices[index + 1] and relies on the format
// repeating the first index after the last one.  Keep that read in one place
// so it can be bounds-checked rather than trusted.
[[nodiscard]] Vector3 edge_point(const Info& info, std::size_t index) {
    if (index >= info.point_indices().size()) {
        return {};
    }
    const std::uint16_t point = info.point_indices()[index];
    return point < info.points().size() ? info.points()[point] : Vector3{};
}

// The shared inner test: how far outside edge `p_index` of this face the
// probe point lies, in the face's own plane.
[[nodiscard]] float edge_dot_difference(const Info& info, const MphData& data,
                                        const Vector4& plane, Vector3 probe,
                                        std::size_t p_index) {
    const std::size_t index = data.point_start_index + p_index;
    const Vector3 point1 = edge_point(info, index);
    const Vector3 point2 = edge_point(info, index + 1);
    const Vector3 edge_dir = (point1 - point2).normalized();
    const Vector3 cross_value = formats::cross(edge_dir, xyz(plane));
    return dot3(probe, cross_value) - dot3(cross_value, point2);
}

} // namespace

Info Info::from_file(const File& file) {
    Info info;
    info.first_hunt_ = file.is_first_hunt();
    if (info.first_hunt_) {
        const auto& set = file.first_hunt();
        info.points_.reserve(set.points.size());
        for (const auto& point : set.points) {
            info.points_.push_back(point.to_float_vector());
        }
        info.planes_.reserve(set.planes.size());
        for (const auto& plane : set.planes) {
            info.planes_.push_back(plane.to_float_vector());
        }
        return info;
    }
    const auto& set = file.mph();
    info.header_ = set.header;
    info.points_.reserve(set.points.size());
    for (const auto& point : set.points) {
        info.points_.push_back(point.to_float_vector());
    }
    info.planes_.reserve(set.planes.size());
    for (const auto& plane : set.planes) {
        info.planes_.push_back(plane.to_float_vector());
    }
    info.point_indices_ = set.point_indices;
    info.data_ = set.data;
    info.data_indices_ = set.data_indices;
    info.entries_ = set.entries;
    info.min_position_ = set.header.min_position.to_float_vector();
    return info;
}

std::vector<Candidate> get_candidates_for_limits(
    std::span<const Instance* const> room_collision, const Vector3* point1,
    Vector3 point2, float margin, const Vector3* limit_min,
    Vector3 limit_max) {
    Vector3 low;
    Vector3 high;
    if (limit_min == nullptr) {
        // The managed routine is used both ways; without limits it derives
        // them from the two points and the margin.
        if (point1 == nullptr) {
            return {};
        }
        low = {std::min(point1->x, point2.x) - margin,
               std::min(point1->y, point2.y) - margin,
               std::min(point1->z, point2.z) - margin};
        high = {std::max(point1->x, point2.x) + margin,
                std::max(point1->y, point2.y) + margin,
                std::max(point1->z, point2.z) + margin};
    } else {
        low = *limit_min;
        high = limit_max;
    }

    std::vector<Candidate> result;
    for (const Instance* instance : room_collision) {
        if (instance == nullptr || instance->info == nullptr
            || instance->info->first_hunt() || !instance->active) {
            continue;
        }
        const Info& info = *instance->info;
        constexpr float PartSize = 4.0F;
        const int parts_x = info.header().parts_x;
        const int parts_y = info.header().parts_y;
        const int parts_z = info.header().parts_z;
        if (parts_x <= 0 || parts_y <= 0 || parts_z <= 0) {
            continue;
        }
        const Vector3 min_pos = info.min_position() + instance->translation;
        int min_x = static_cast<int>((low.x - min_pos.x) / PartSize);
        int max_x = static_cast<int>((high.x - min_pos.x) / PartSize);
        int min_y = static_cast<int>((low.y - min_pos.y) / PartSize);
        int max_y = static_cast<int>((high.y - min_pos.y) / PartSize);
        int min_z = static_cast<int>((low.z - min_pos.z) / PartSize);
        int max_z = static_cast<int>((high.z - min_pos.z) / PartSize);
        if (max_x < 0 || min_x > parts_x || max_y < 0 || min_y > parts_y
            || max_z < 0 || min_z > parts_z) {
            continue;
        }
        min_x = std::max(min_x, 0);
        min_y = std::max(min_y, 0);
        min_z = std::max(min_z, 0);
        max_x = std::min(max_x, parts_x - 1);
        max_y = std::min(max_y, parts_y - 1);
        max_z = std::min(max_z, parts_z - 1);
        for (int y = min_y; y <= max_y; ++y) {
            for (int z = min_z; z <= max_z; ++z) {
                for (int x = min_x; x <= max_x; ++x) {
                    const std::size_t entry_index =
                        static_cast<std::size_t>(y) * parts_x * parts_z
                        + static_cast<std::size_t>(z) * parts_x
                        + static_cast<std::size_t>(x);
                    if (entry_index >= info.entries().size()) {
                        continue;
                    }
                    const MphEntry& entry = info.entries()[entry_index];
                    if (entry.data_count == 0) {
                        continue;
                    }
                    result.push_back(Candidate{nullptr, instance, entry});
                }
            }
        }
    }
    return result;
}

std::vector<Candidate> get_candidates_for_points(
    std::span<const Instance* const> room_collision, Vector3 point1,
    Vector3 point2, float margin) {
    return get_candidates_for_limits(room_collision, &point1, point2, margin,
                                     nullptr, Vector3{});
}

std::size_t check_sphere_between_points(std::span<const Candidate> candidates,
                                        Vector3 point1, Vector3 point2,
                                        float radius, bool include_offset,
                                        TestFlags flags,
                                        std::span<Result> results) {
    const std::uint16_t mask = flag_mask(flags);
    // The managed routine keeps a per-call set so a face shared by several
    // grid cells is only reported once.
    std::set<std::int32_t> seen;
    std::size_t count = 0;
    for (const Candidate& candidate : candidates) {
        if (count == results.size()) {
            break;
        }
        if (candidate.collision == nullptr
            || candidate.collision->info == nullptr) {
            continue;
        }
        const Info& info = *candidate.collision->info;
        const Vector3 translation = candidate.collision->translation;
        const Vector3 trans1 = point1 - translation;
        const Vector3 trans2 = point2 - translation;
        for (std::size_t j = 0; j < candidate.entry.data_count; ++j) {
            if (count == results.size()) {
                break;
            }
            const std::size_t data_index = candidate.entry.data_start_index + j;
            if (data_index >= info.data_indices().size()) {
                continue;
            }
            const std::uint16_t data_id = info.data_indices()[data_index];
            if (data_id >= info.data().size()) {
                continue;
            }
            const MphData& data = info.data()[data_id];
            if ((data.flags & mask) != 0) {
                continue;
            }
            if (candidate.entity_collision == nullptr
                && !seen.insert(static_cast<std::int32_t>(data_id)).second) {
                continue;
            }
            if (data.plane_index >= info.planes().size()) {
                continue;
            }
            const Vector4 plane = info.planes()[data.plane_index];
            const float dot1 = dot3(trans1, xyz(plane)) - plane.w;
            if (dot1 <= 0.0F) {
                // The plane is behind the starting point.
                continue;
            }
            const float dot2 = dot3(trans2, xyz(plane)) - plane.w;
            if (dot2 > radius) {
                // The plane is more than a radius ahead of the end point.
                continue;
            }
            float pct = 1.0F;
            if (std::fabs(dot1 - dot2) >= 1.0F / 4096.0F) {
                pct = std::clamp(dot1 / (dot1 - dot2), 0.0F, 1.0F);
            }
            const Vector3 vec = trans1 + (trans2 - trans1) * pct;

            bool full_collision = true;
            for (std::size_t p1 = 0; p1 < data.point_index_count; ++p1) {
                const float diff =
                    edge_dot_difference(info, data, plane, vec, p1);
                if (diff >= -0.03125F) {
                    continue;
                }
                full_collision = false;
                // The managed comment records the known bug here: the first
                // edge that is outside by the margin ends the walk, so a face
                // the probe is far outside of can still be reported.  It is
                // reproduced rather than fixed, because callers compensate.
                if (include_offset && diff >= -radius) {
                    const std::size_t index = data.point_start_index + p1;
                    Result& result = results[count];
                    result = Result{};
                    result.field0 = 1;
                    result.entity_collision = candidate.entity_collision;
                    result.flags = data.flags;
                    result.field14 = dot2;
                    result.distance = pct;
                    result.plane = move_plane(plane, translation);
                    result.position = vec + translation;
                    result.edge_point1 = edge_point(info, index) + translation;
                    result.edge_point2 =
                        edge_point(info, index + 1) + translation;
                    ++count;
                }
                break;
            }
            if (full_collision) {
                Result& result = results[count];
                result = Result{};
                result.field0 = 0;
                result.entity_collision = candidate.entity_collision;
                result.flags = data.flags;
                result.field14 = dot2;
                result.distance = pct;
                result.plane = move_plane(plane, translation);
                result.position = vec + translation;
                ++count;
            }
        }
    }
    return count;
}

std::size_t check_sphere_between_points(
    std::span<const Instance* const> room_collision, Vector3 point1,
    Vector3 point2, float radius, bool include_offset, TestFlags flags,
    std::span<Result> results) {
    const auto candidates = get_candidates_for_limits(
        room_collision, &point1, point2, radius, nullptr, Vector3{});
    return check_sphere_between_points(candidates, point1, point2, radius,
                                       include_offset, flags, results);
}

std::size_t check_in_radius(std::span<const Instance* const> room_collision,
                            Vector3 point, float radius,
                            bool get_simple_normal, TestFlags flags,
                            std::span<Result> results) {
    const std::uint16_t mask = flag_mask(flags);
    const Vector3 limit_min{point.x - radius, point.y - radius,
                            point.z - radius};
    const Vector3 limit_max{point.x + radius, point.y + radius,
                            point.z + radius};
    const auto candidates = get_candidates_for_limits(
        room_collision, nullptr, Vector3{}, 0.0F, &limit_min, limit_max);

    std::set<std::int32_t> seen;
    std::size_t count = 0;
    for (const Candidate& candidate : candidates) {
        if (count == results.size()) {
            break;
        }
        if (candidate.collision == nullptr
            || candidate.collision->info == nullptr) {
            continue;
        }
        const Info& info = *candidate.collision->info;
        const Vector3 translation = candidate.collision->translation;
        const Vector3 trans_point = point - translation;
        for (std::size_t j = 0; j < candidate.entry.data_count; ++j) {
            if (count == results.size()) {
                break;
            }
            const std::size_t data_index = candidate.entry.data_start_index + j;
            if (data_index >= info.data_indices().size()) {
                continue;
            }
            const std::uint16_t data_id = info.data_indices()[data_index];
            if (data_id >= info.data().size()) {
                continue;
            }
            const MphData& data = info.data()[data_id];
            if ((data.flags & mask) != 0) {
                continue;
            }
            if (candidate.entity_collision == nullptr
                && !seen.insert(static_cast<std::int32_t>(data_id)).second) {
                continue;
            }
            if (data.plane_index >= info.planes().size()) {
                continue;
            }
            const Vector4 plane = info.planes()[data.plane_index];
            const float dot = dot3(trans_point, xyz(plane)) - plane.w;
            float result_dot = dot;
            if (dot <= 0.0F || dot > radius) {
                continue;
            }

            bool no_negative = true;
            bool found_blocker = false;
            std::size_t p1 = 0;
            for (; p1 < data.point_index_count; ++p1) {
                if (edge_dot_difference(info, data, plane, trans_point, p1)
                    < -0.03125F) {
                    no_negative = false;
                    break;
                }
            }
            if (no_negative) {
                results[count] = Result{};
                results[count].field0 = 0;
                results[count].plane = move_plane(plane, translation);
                found_blocker = true;
            }
            if (!found_blocker) {
                // Pick up at the edge the walk above stopped on, exactly as
                // the managed routine does.
                const float diff =
                    edge_dot_difference(info, data, plane, trans_point, p1);
                if (diff < 0.0F && diff >= -radius) {
                    for (std::size_t p2 = 0; p2 < data.point_index_count;
                         ++p2) {
                        const std::size_t index = data.point_start_index + p2;
                        const Vector3 point_a = edge_point(info, index);
                        const Vector3 point_b = edge_point(info, index + 1);
                        const Vector3 edge = point_b - point_a;
                        const float edge_len = dot3(edge, edge);
                        if (edge_len == 0.0F) {
                            continue;
                        }
                        const float div =
                            dot3(edge, trans_point - point_a) / edge_len;
                        if (div < 0.0F || div >= 1.0F) {
                            const Vector3 vec1 = trans_point - point_a;
                            const float mag1 = vec1.length();
                            if (mag1 > radius) {
                                continue;
                            }
                            found_blocker = true;
                            result_dot = mag1;
                            results[count] = Result{};
                            results[count].field0 = 2;
                            results[count].plane =
                                get_simple_normal
                                    ? move_plane(plane, translation)
                                    : move_plane({vec1.x / mag1, vec1.y / mag1,
                                                  vec1.z / mag1, plane.w},
                                                 translation);
                            break;
                        }
                        const Vector3 vec2 =
                            trans_point - (point_a + edge * div);
                        const float mag2 = vec2.length();
                        if (mag2 <= radius) {
                            found_blocker = true;
                            result_dot = mag2;
                            results[count] = Result{};
                            results[count].field0 = 1;
                            results[count].plane =
                                get_simple_normal
                                    ? move_plane(plane, translation)
                                    : move_plane({vec2.x / mag2, vec2.y / mag2,
                                                  vec2.z / mag2, plane.w},
                                                 translation);
                            break;
                        }
                    }
                }
            }
            if (found_blocker) {
                results[count].flags = data.flags;
                results[count].entity_collision = nullptr;
                results[count].field14 = result_dot;
                ++count;
            }
        }
    }
    return count;
}

} // namespace fruityprime::collision
