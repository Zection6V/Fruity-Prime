#pragma once

#include "Formats/fixed.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <variant>
#include <vector>

namespace fruityprime::collision {

struct MphHeader {
    static constexpr std::size_t Size = 84;

    std::array<std::uint8_t, 4> type{};
    std::uint32_t point_count = 0;
    std::uint32_t point_offset = 0;
    std::uint32_t plane_count = 0;
    std::uint32_t plane_offset = 0;
    std::uint32_t point_index_count = 0;
    std::uint32_t point_index_offset = 0;
    std::uint32_t data_count = 0;
    std::uint32_t data_offset = 0;
    std::uint32_t data_index_count = 0;
    std::uint32_t data_index_offset = 0;
    std::int32_t parts_x = 0;
    std::int32_t parts_y = 0;
    std::int32_t parts_z = 0;
    formats::Vector3Fx min_position;
    std::uint32_t entry_count = 0;
    std::uint32_t entry_offset = 0;
    std::uint32_t portal_count = 0;
    std::uint32_t portal_offset = 0;
};

struct MphData {
    static constexpr std::size_t Size = 16;

    std::int32_t counter = 0;
    std::uint16_t plane_index = 0;
    std::uint16_t flags = 0;
    std::uint16_t layer_mask = 0;
    std::uint16_t padding = 0;
    std::uint16_t point_index_count = 0;
    std::uint16_t point_start_index = 0;

    [[nodiscard]] int slipperiness() const noexcept { return (flags & 0x18) >> 3; }
    [[nodiscard]] int terrain() const noexcept { return (flags & 0x1E0) >> 5; }
    [[nodiscard]] bool ignore_players() const noexcept { return (flags & 0x2000) != 0; }
    [[nodiscard]] bool ignore_beams() const noexcept { return (flags & 0x4000) != 0; }
    [[nodiscard]] int axis() const noexcept { return layer_mask & 3; }
};

struct MphEntry {
    static constexpr std::size_t Size = 4;

    std::uint16_t data_count = 0;
    std::uint16_t data_start_index = 0;
};

struct MphPortal {
    static constexpr std::size_t Size = 224;

    std::string name;
    std::string node_name1;
    std::string node_name2;
    std::array<formats::Vector3Fx, 4> points{};
    std::array<formats::Vector4Fx, 4> planes{};
    formats::Vector4Fx plane;
    std::uint16_t flags = 0;
    std::uint16_t layer_mask = 0;
    std::uint16_t point_count = 0;
    std::uint8_t unused_00 = 0;
    std::uint8_t unused_01 = 0;
};

struct FhHeader {
    static constexpr std::size_t Size = 72;

    std::uint32_t point_count = 0;
    std::uint32_t point_offset = 0;
    std::uint32_t plane_count = 0;
    std::uint32_t plane_offset = 0;
    std::uint32_t vector_count = 0;
    std::uint32_t vector_offset = 0;
    std::uint16_t data_count = 0;
    std::uint16_t data_start_index = 0;
    std::uint32_t data_offset = 0;
    std::uint32_t data_index_count = 0;
    std::uint32_t data_index_offset = 0;
    std::uint32_t entry_count = 0;
    std::uint32_t entry_offset = 0;
    std::uint32_t tree_node_index_count = 0;
    std::uint32_t tree_node_index_offset = 0;
    std::uint32_t tree_node_count = 0;
    std::uint32_t tree_node_offset = 0;
    std::uint32_t portal_count = 0;
    std::uint32_t portal_offset = 0;
};

struct FhData {
    static constexpr std::size_t Size = 6;

    std::uint16_t plane_index = 0;
    std::uint16_t vector_count = 0;
    std::uint16_t vector_start_index = 0;
};

struct FhVector {
    static constexpr std::size_t Size = 6;

    std::uint16_t point1_index = 0;
    std::uint16_t point2_index = 0;
    std::uint16_t plane_index = 0;
};

struct FhEntry {
    static constexpr std::size_t Size = 28;

    formats::Vector3Fx min_bounds;
    formats::Vector3Fx max_bounds;
    std::uint16_t data_count = 0;
    std::uint16_t data_start_index = 0;
};

struct FhTreeNode {
    static constexpr std::size_t Size = 28;

    formats::Vector3Fx min_bounds;
    formats::Vector3Fx max_bounds;
    std::uint16_t left_index = 0;
    std::uint16_t right_index = 0;
};

struct FhPortal {
    static constexpr std::size_t Size = 96;

    std::string name;
    std::string node_name1;
    std::string node_name2;
    formats::Vector4Fx plane;
    std::uint16_t vector_count = 0;
    std::uint16_t vector_start_index = 0;
    std::uint8_t field_5c = 0;
    std::uint8_t field_5d = 0;
    std::uint16_t padding_5e = 0;
};

struct MphDataSet {
    MphHeader header;
    std::vector<formats::Vector3Fx> points;
    std::vector<formats::Vector4Fx> planes;
    std::vector<std::uint16_t> point_indices;
    std::vector<MphData> data;
    std::vector<std::uint16_t> data_indices;
    std::vector<MphEntry> entries;
    std::vector<MphPortal> portals;

    // ReadMphCollision's runtime view; the on-disk header is retained verbatim.
    [[nodiscard]] MphDataSet with_layer_mask(int room_layer_mask) const;
    [[nodiscard]] std::array<int, 3> part_index_from_entry(int index) const;
    [[nodiscard]] int entry_index_from_point(float x, float y, float z) const;
};

struct FhDataSet {
    FhHeader header;
    std::vector<formats::Vector3Fx> points;
    std::vector<formats::Vector4Fx> planes;
    std::vector<FhData> data;
    std::vector<FhVector> vectors;
    std::vector<std::uint16_t> data_indices;
    std::vector<FhEntry> entries;
    std::vector<std::int32_t> tree_node_indices;
    std::vector<FhTreeNode> tree_nodes;
    std::vector<FhPortal> portals;
};

class File {
public:
    [[nodiscard]] static File read_file(const std::filesystem::path& path);
    [[nodiscard]] static File from_bytes(std::vector<std::uint8_t> bytes);

    [[nodiscard]] bool is_mph() const noexcept;
    [[nodiscard]] bool is_first_hunt() const noexcept { return !is_mph(); }
    [[nodiscard]] const MphDataSet& mph() const;
    [[nodiscard]] const FhDataSet& first_hunt() const;

private:
    explicit File(std::vector<std::uint8_t> bytes);

    void parse();
    void parse_mph();
    void parse_first_hunt();

    std::vector<std::uint8_t> bytes_;
    std::variant<MphDataSet, FhDataSet> data_;
};

} // namespace fruityprime::collision
