#pragma once

#include "Formats/fixed.hpp"
#include "Formats/Types.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <span>
#include <vector>

namespace fruityprime::node {

// The node-data files are the navigation, aerial, vantage, and hazard tables
// used by multiplayer rooms.  These are kept as explicit records instead of
// packing C++ structs, because the file format is little-endian and the
// native build must behave identically on every host.
struct NodeDataHeader {
    static constexpr std::size_t Size = 14;

    std::uint16_t version = 0;
    std::uint16_t data_count = 0;
    std::uint32_t index_offset = 0;
    std::uint32_t data_offset = 0;
    std::uint16_t index_count = 0;
};

struct NodeDataStruct1 {
    static constexpr std::size_t Size = 8;

    std::uint32_t offset2 = 0;
    std::uint16_t count = 0;
    std::uint16_t padding6 = 0;
};

struct NodeDataStruct2 {
    static constexpr std::size_t Size = 8;

    std::uint32_t offset3 = 0;
    std::uint16_t count = 0;
    std::uint16_t padding6 = 0;
};

struct NodeDataStruct3 {
    static constexpr std::size_t Size = 36;

    std::uint16_t node_type = 0;
    std::uint16_t id = 0;
    std::uint16_t field4 = 0;
    std::uint16_t count2 = 0;
    formats::Vector3Fx position;
    std::int32_t max_distance = 0;
    std::uint32_t offset1 = 0;
    std::uint32_t offset2 = 0;
    std::uint32_t offset3 = 0;
};

struct FhNodeData {
    static constexpr std::size_t Size = 24;

    std::uint16_t field0 = 0;
    std::uint16_t field2 = 0;
    formats::Vector3Fx position;
    std::uint32_t offset1 = 0;
    std::uint32_t offset2 = 0;
};

enum class NodeType : std::uint16_t {
    Navigation = 0,
    Special = 1,
    Aerial = 2,
    Vantage = 3,
    AltForm = 4,
    Hazard = 5,
};

[[nodiscard]] formats::Vector4 node_type_color(NodeType type) noexcept;

class NodeData3 {
public:
    NodeData3() = default;

    NodeData3(NodeDataStruct3 raw, int index1, int index2,
              std::shared_ptr<const std::vector<std::uint16_t>> values);
    explicit NodeData3(formats::Vector3Fx position);

    NodeType node_type = NodeType::Navigation;
    std::uint16_t id = 0;
    std::uint16_t field4 = 0;
    int count2 = 0;
    formats::Vector3 position;
    float max_distance = 0.0F;
    // Offset1 has no defined count.  Keep the complete value table and these
    // two starting indices, as the managed implementation does.
    int index1 = 0;
    int index2 = 0;
    formats::Matrix4 transform;
    formats::Vector4 color;

    [[nodiscard]] const std::vector<std::uint16_t>& values() const noexcept {
        return *values_;
    }

private:
    std::shared_ptr<const std::vector<std::uint16_t>> values_
        = std::make_shared<const std::vector<std::uint16_t>>();
};

using NodeDataLists =
    std::vector<std::vector<std::vector<NodeData3>>>;

class NodeData {
public:
    NodeData(NodeDataHeader header, std::vector<std::uint16_t> set_indices,
             NodeDataLists data);

    [[nodiscard]] static NodeData from_bytes(
        std::span<const std::uint8_t> bytes, bool first_hunt = false);
    [[nodiscard]] static NodeData from_bytes(
        std::vector<std::uint8_t> bytes, bool first_hunt = false);
    [[nodiscard]] static NodeData read_file(const std::filesystem::path& path,
                                            bool first_hunt = false);

    [[nodiscard]] const NodeDataHeader& header() const noexcept {
        return header_;
    }
    [[nodiscard]] const std::vector<std::uint16_t>& set_indices()
        const noexcept {
        return set_indices_;
    }
    [[nodiscard]] const NodeDataLists& data() const noexcept { return data_; }
    [[nodiscard]] bool simple() const noexcept {
        return data_.size() == 1 && data_[0].size() == 1;
    }
    [[nodiscard]] const std::array<bool, 16>& set_selector() const noexcept {
        return set_selector_;
    }
    [[nodiscard]] std::array<bool, 16>& set_selector() noexcept {
        return set_selector_;
    }

    // Searches the first node list, matching ReadNodeData.FindClosestNode.
    // A null result means the file has no usable first list or the optional
    // maximum-distance constraint rejected the closest node.
    [[nodiscard]] const NodeData3* closest_node(
        formats::Vector3 position, bool use_max_distance = false) const
        noexcept;

private:
    NodeDataHeader header_;
    std::vector<std::uint16_t> set_indices_;
    NodeDataLists data_;
    std::array<bool, 16> set_selector_{};
};

} // namespace fruityprime::node

namespace MphReadNative {
namespace Node = ::fruityprime::node;
}
