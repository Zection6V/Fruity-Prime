#pragma once

#include "Formats/collision_format.hpp"
#include "Formats/Types.hpp"

#include <cstdint>
#include <filesystem>
#include <ostream>
#include <span>
#include <string>
#include <vector>

namespace fruityprime::utility::repack_collision {

// The managed RepackFilter is deliberately kept local to this utility.  It
// controls which layer of a room is selected before a First Hunt collision is
// written; it is not a wire or runtime enum.
enum class RepackFilter : std::uint8_t {
    All,
    Multiplayer,
    SinglePlayer
};

enum class CollisionFlag : std::uint16_t {
    None = 0x0000,
    Damaging = 0x0001,
    Bit01 = 0x0002,
    Bit02 = 0x0004,
    ReflectBeams = 0x0200,
    Bit10 = 0x0400,
    Bit11 = 0x0800,
    Bit12 = 0x1000,
    IgnorePlayers = 0x2000,
    IgnoreBeams = 0x4000,
    IgnoreScan = 0x8000
};

struct CollisionDataEditor {
    std::vector<formats::Vector3> points;
    formats::Vector4 plane;
    std::uint16_t layer_mask = 0;
    std::uint16_t flags = 0;

    [[nodiscard]] bool has(CollisionFlag flag) const noexcept;
    void set(CollisionFlag flag, bool value) noexcept;

    [[nodiscard]] bool damaging() const noexcept;
    void set_damaging(bool value) noexcept;
    [[nodiscard]] bool reflect() const noexcept;
    void set_reflect(bool value) noexcept;
    [[nodiscard]] bool players() const noexcept;
    void set_players(bool value) noexcept;
    [[nodiscard]] bool beams() const noexcept;
    void set_beams(bool value) noexcept;
    [[nodiscard]] bool scan() const noexcept;
    void set_scan(bool value) noexcept;

    [[nodiscard]] int slipperiness() const noexcept;
    void set_slipperiness(int value);
    [[nodiscard]] formats::Terrain terrain() const noexcept;
    void set_terrain(formats::Terrain value);
};

// A format-neutral portal view.  MPH stores four portal points and four edge
// planes; First Hunt derives its edge planes from the point order while
// retaining the portal plane.  Keeping both views here lets each writer apply
// exactly the same conversions as RepackCollision.cs.
struct Portal {
    bool active = true;
    std::string name;
    std::string node_name1;
    std::string node_name2;
    std::uint16_t layer_mask = 0;
    std::uint16_t flags = 0;
    std::uint8_t unknown00 = 0;
    std::uint8_t unknown01 = 0;
    std::vector<formats::Vector3> points;
    std::vector<formats::Vector4> planes;
    formats::Vector4 plane;
};

[[nodiscard]] std::vector<CollisionDataEditor> editors(
    const collision::MphDataSet& info);
[[nodiscard]] std::vector<CollisionDataEditor> editors(
    const collision::FhDataSet& info);
[[nodiscard]] std::vector<Portal> portals(const collision::MphDataSet& info);
[[nodiscard]] std::vector<Portal> portals(const collision::FhDataSet& info);

[[nodiscard]] int primary_axis(formats::Vector3 normal) noexcept;
[[nodiscard]] formats::Vector4 plane_from_edge(
    formats::Vector3 point1, formats::Vector3 point2,
    formats::Vector3 normal);

void validate(const CollisionDataEditor& data);

[[nodiscard]] bool test_intersection(
    formats::Vector3 point1, formats::Vector3 point2,
    formats::Vector3 v0, formats::Vector3 v1, formats::Vector3 v2,
    float& distance) noexcept;
[[nodiscard]] bool check_intersection(
    formats::Vector3 point1, formats::Vector3 point2,
    std::span<const formats::Vector3> face) noexcept;
[[nodiscard]] std::vector<std::uint16_t> data_in_region(
    formats::Vector3 min_bounds, formats::Vector3 max_bounds,
    std::span<const CollisionDataEditor> data);

[[nodiscard]] std::vector<std::uint8_t> repack_mph(
    std::span<const CollisionDataEditor> data,
    std::span<const Portal> portals);
[[nodiscard]] std::vector<std::uint8_t> repack_first_hunt(
    std::span<const CollisionDataEditor> data,
    std::span<const Portal> portals);
[[nodiscard]] std::vector<std::uint8_t> repack(
    const collision::File& file);
[[nodiscard]] std::vector<std::uint8_t> repack_file(
    const std::filesystem::path& path);

void print_summary(const collision::File& file, std::ostream& output);

} // namespace fruityprime::utility::repack_collision
