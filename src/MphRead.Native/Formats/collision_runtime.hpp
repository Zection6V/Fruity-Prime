#pragma once

// Native counterpart of Formats/Collision.cs's CollisionInstance and of the
// candidate/query entry points in Formats/CollisionDetection.cs.
//
// The decoded file (collision::File) stays the on-disk view; Info is the
// float view the queries walk, and Instance is what a room keeps per part --
// a named, optionally translated, optionally deactivated piece of collision.

#include "Formats/collision_format.hpp"
#include "Formats/collision_query.hpp"
#include "Formats/Types.hpp"

#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace fruityprime::collision {

// Collision.CollisionFlags: the per-face bits the queries mask against.
enum class Flags : std::uint16_t {
    None = 0x0,
    Damaging = 0x1,
    Bit01 = 0x2,
    Bit02 = 0x4,
    // bits 3-4: slipperiness
    // bits 5-8: terrain type
    ReflectBeams = 0x200,
    Bit10 = 0x400,
    Bit11 = 0x800,
    Bit12 = 0x1000,
    IgnorePlayers = 0x2000,
    IgnoreBeams = 0x4000,
    IgnoreScan = 0x8000
};

// CollisionDetection.TestFlags.  The values deliberately match the collision
// flags they mask, which is why the managed code can or them straight in.
enum class TestFlags : std::uint32_t {
    None = 0x0,
    Players = 0x2000,
    Beams = 0x4000,
    Scan = 0x8000
};

[[nodiscard]] constexpr TestFlags operator|(TestFlags a, TestFlags b) noexcept {
    return static_cast<TestFlags>(static_cast<std::uint32_t>(a)
                                  | static_cast<std::uint32_t>(b));
}

[[nodiscard]] constexpr bool has_flag(TestFlags value, TestFlags flag) noexcept {
    return (static_cast<std::uint32_t>(value)
            & static_cast<std::uint32_t>(flag)) != 0;
}

// Collision.CollisionInfo / MphCollisionInfo.  The managed class converts the
// fixed-point points and planes once at load; so does this.
class Info {
public:
    [[nodiscard]] static Info from_file(const File& file);

    [[nodiscard]] bool first_hunt() const noexcept { return first_hunt_; }
    [[nodiscard]] const std::vector<formats::Vector3>& points() const noexcept {
        return points_;
    }
    [[nodiscard]] const std::vector<formats::Vector4>& planes() const noexcept {
        return planes_;
    }
    // MPH only; empty for a First Hunt file.
    [[nodiscard]] const MphHeader& header() const noexcept { return header_; }
    [[nodiscard]] const std::vector<std::uint16_t>& point_indices()
        const noexcept {
        return point_indices_;
    }
    [[nodiscard]] const std::vector<MphData>& data() const noexcept {
        return data_;
    }
    [[nodiscard]] const std::vector<std::uint16_t>& data_indices()
        const noexcept {
        return data_indices_;
    }
    [[nodiscard]] const std::vector<MphEntry>& entries() const noexcept {
        return entries_;
    }
    [[nodiscard]] formats::Vector3 min_position() const noexcept {
        return min_position_;
    }

private:
    bool first_hunt_ = false;
    std::vector<formats::Vector3> points_;
    std::vector<formats::Vector4> planes_;
    MphHeader header_{};
    std::vector<std::uint16_t> point_indices_;
    std::vector<MphData> data_;
    std::vector<std::uint16_t> data_indices_;
    std::vector<MphEntry> entries_;
    formats::Vector3 min_position_{};
};

// Collision.CollisionInstance
struct Instance {
    std::string name;
    bool active = true;
    const Info* info = nullptr;
    bool is_entity = false;
    std::string connector_name;
    formats::Vector3 translation{};
};

// Collision.EntityCollision: the collision an entity carries with it.  The
// managed class keeps both inverses because the sphere query transforms the
// probe points into the entity's own space while the results come back in
// world space.
struct EntityCollision {
    formats::Matrix4 transform{};
    formats::Matrix4 inverse1{};
    formats::Matrix4 inverse2{};
    formats::Vector3 initial_center{};
    formats::Vector3 current_center{};
    float max_distance = 0.0F;
    const void* entity = nullptr;
    const Instance* collision = nullptr;
    // Kept for the viewer's collision display only.
    std::vector<formats::Vector3> draw_points;

    // Collision.CollisionInstance.IsForceField: a force field's collision is
    // the one an entity carries that beams pass through but players do not.
    [[nodiscard]] bool is_force_field() const noexcept {
        return collision != nullptr && collision->is_entity
            && !collision->connector_name.empty();
    }

    // The node references a room part keeps for audibility and visibility.
    std::int32_t node_ref1 = -1;
    std::int32_t node_ref2 = -1;
};

// CollisionDetection.CollisionCandidate
struct Candidate {
    const void* entity_collision = nullptr;
    const Instance* collision = nullptr;
    MphEntry entry{};
};

// CollisionDetection.GetCandidatesForLimits.  The managed routine serves both
// a limits query and a points query, so `point1` being absent selects the
// limits form -- reproduced here with optional pointers rather than
// nullable structs.
[[nodiscard]] std::vector<Candidate> get_candidates_for_limits(
    std::span<const Instance* const> room_collision,
    const formats::Vector3* point1, formats::Vector3 point2, float margin,
    const formats::Vector3* limit_min, formats::Vector3 limit_max);

// CollisionDetection.GetCandidatesForPoints
[[nodiscard]] std::vector<Candidate> get_candidates_for_points(
    std::span<const Instance* const> room_collision, formats::Vector3 point1,
    formats::Vector3 point2, float margin);

// CollisionDetection.CheckSphereBetweenPoints.  Returns how many results were
// written, up to `results.size()`.
[[nodiscard]] std::size_t check_sphere_between_points(
    std::span<const Candidate> candidates, formats::Vector3 point1,
    formats::Vector3 point2, float radius, bool include_offset,
    TestFlags flags, std::span<Result> results);

[[nodiscard]] std::size_t check_sphere_between_points(
    std::span<const Instance* const> room_collision, formats::Vector3 point1,
    formats::Vector3 point2, float radius, bool include_offset,
    TestFlags flags, std::span<Result> results);

// CollisionDetection.CheckInRadius
[[nodiscard]] std::size_t check_in_radius(
    std::span<const Instance* const> room_collision, formats::Vector3 point,
    float radius, bool get_simple_normal, TestFlags flags,
    std::span<Result> results);

} // namespace fruityprime::collision
