#include "Entities/entity_runtime_records.hpp"
#include "Formats/node_data.hpp"

#include "Utility/binary_reader.hpp"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <limits>
#include <stdexcept>
#include <string>
#include <utility>

namespace fruityprime::node {
namespace {

using ByteSpan = std::span<const std::uint8_t>;
using core::BinaryReader;

[[nodiscard]] std::vector<std::uint8_t> read_all(
    const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary | std::ios::ate);
    if (!input) {
        throw std::runtime_error("could not open node-data file "
                                 + path.string());
    }
    const std::streamoff length = input.tellg();
    if (length < 0 || static_cast<std::uintmax_t>(length)
        > std::numeric_limits<std::size_t>::max()) {
        throw std::runtime_error("node-data file is too large: "
                                 + path.string());
    }
    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(length));
    input.seekg(0);
    if (!bytes.empty()) {
        input.read(reinterpret_cast<char*>(bytes.data()),
                   static_cast<std::streamsize>(bytes.size()));
        if (!input) {
            throw std::runtime_error("could not read node-data file "
                                     + path.string());
        }
    }
    return bytes;
}

[[nodiscard]] formats::Fixed read_fixed(BinaryReader& reader) {
    return formats::Fixed{reader.read_i32_le()};
}

[[nodiscard]] formats::Vector3Fx read_vector3(BinaryReader& reader) {
    return {read_fixed(reader), read_fixed(reader), read_fixed(reader)};
}

template <typename Value, typename Decoder>
[[nodiscard]] std::vector<Value> read_array(ByteSpan bytes,
                                             std::uint32_t offset,
                                             std::size_t count,
                                             std::size_t element_size,
                                             Decoder&& decoder,
                                             const char* description) {
    if (offset == 0 || count == 0) {
        return {};
    }
    if (count > std::numeric_limits<std::size_t>::max() / element_size) {
        throw std::runtime_error(std::string("node-data ") + description
                                 + " size overflows");
    }
    const std::size_t start = static_cast<std::size_t>(offset);
    const std::size_t length = count * element_size;
    if (start > bytes.size() || length > bytes.size() - start) {
        throw std::runtime_error(std::string("node-data ") + description
                                 + " is outside the file");
    }

    std::vector<Value> output;
    output.reserve(count);
    for (std::size_t i = 0; i < count; ++i) {
        BinaryReader reader(bytes.subspan(start + i * element_size,
                                          element_size));
        output.push_back(decoder(reader));
    }
    return output;
}

[[nodiscard]] std::vector<std::uint16_t> read_values(
    ByteSpan bytes, std::uint32_t offset) {
    if (offset == 0) {
        return {};
    }
    const std::size_t start = static_cast<std::size_t>(offset);
    if (start > bytes.size()) {
        throw std::runtime_error("node-data value table is outside the file");
    }
    // The managed reader intentionally consumes complete u16 values and
    // ignores a possible trailing alignment byte.
    const std::size_t count = (bytes.size() - start) / sizeof(std::uint16_t);
    return read_array<std::uint16_t>(bytes, offset, count,
                                     sizeof(std::uint16_t),
                                     [](BinaryReader& reader) {
                                         return reader.read_u16_le();
                                     },
                                     "value table");
}

[[nodiscard]] NodeDataHeader read_header(ByteSpan bytes) {
    if (bytes.size() < NodeDataHeader::Size) {
        throw std::runtime_error("node-data file is smaller than its header");
    }
    BinaryReader reader(bytes.subspan(0, NodeDataHeader::Size));
    return {
        reader.read_u16_le(),
        reader.read_u16_le(),
        reader.read_u32_le(),
        reader.read_u32_le(),
        reader.read_u16_le(),
    };
}

[[nodiscard]] NodeDataStruct1 read_struct1(BinaryReader& reader) {
    return {reader.read_u32_le(), reader.read_u16_le(), reader.read_u16_le()};
}

[[nodiscard]] NodeDataStruct2 read_struct2(BinaryReader& reader) {
    return {reader.read_u32_le(), reader.read_u16_le(), reader.read_u16_le()};
}

[[nodiscard]] NodeDataStruct3 read_struct3(BinaryReader& reader) {
    return {
        reader.read_u16_le(),
        reader.read_u16_le(),
        reader.read_u16_le(),
        reader.read_u16_le(),
        read_vector3(reader),
        reader.read_i32_le(),
        reader.read_u32_le(),
        reader.read_u32_le(),
        reader.read_u32_le(),
    };
}

[[nodiscard]] FhNodeData read_fh_node(BinaryReader& reader) {
    return {
        reader.read_u16_le(),
        reader.read_u16_le(),
        read_vector3(reader),
        reader.read_u32_le(),
        reader.read_u32_le(),
    };
}

[[nodiscard]] formats::Matrix4 translation_matrix(formats::Vector3 position) {
    formats::Matrix4 matrix;
    matrix.m41 = position.x;
    matrix.m42 = position.y;
    matrix.m43 = position.z;
    return matrix;
}

[[nodiscard]] std::shared_ptr<const std::vector<std::uint16_t>> empty_values() {
    static const auto values = std::make_shared<const std::vector<std::uint16_t>>();
    return values;
}

[[nodiscard]] std::shared_ptr<const std::vector<std::uint16_t>> shared_values(
    std::vector<std::uint16_t> values) {
    return std::make_shared<const std::vector<std::uint16_t>>(std::move(values));
}

[[nodiscard]] int value_index(std::uint32_t offset, std::uint32_t minimum,
                              std::size_t value_count) {
    if (offset < minimum || ((offset - minimum) % 2) != 0) {
        throw std::runtime_error("node-data value offset is not aligned");
    }
    const std::uint32_t difference = offset - minimum;
    const std::uint64_t index = difference / 2U;
    if (index >= value_count) {
        throw std::runtime_error("node-data value offset is outside the table");
    }
    return static_cast<int>(index);
}

[[nodiscard]] NodeData read_first_hunt(ByteSpan bytes) {
    if (bytes.size() < 4) {
        throw std::runtime_error("First Hunt node-data file is too small");
    }
    BinaryReader initial(bytes.subspan(0, 4));
    const std::uint16_t version = initial.read_u16_le();
    if (version != 0) {
        throw std::runtime_error("unexpected First Hunt node-data version "
                                 + std::to_string(version));
    }
    const std::uint16_t count = initial.read_u16_le();
    const auto headers = read_array<FhNodeData>(
        bytes, 4, count, FhNodeData::Size,
        [](BinaryReader& reader) { return read_fh_node(reader); },
        "First Hunt node table");

    NodeDataLists data;
    std::vector<NodeData3> nodes;
    nodes.reserve(headers.size());
    for (const auto& header : headers) {
        nodes.emplace_back(header.position);
    }
    data.push_back({std::move(nodes)});
    return NodeData{NodeDataHeader{}, {0}, std::move(data)};
}

} // namespace

formats::Vector4 node_type_color(NodeType type) noexcept {
    switch (type) {
    case NodeType::Navigation: return {1.0F, 0.0F, 0.0F, 1.0F};
    case NodeType::Special: return {0.0F, 1.0F, 0.0F, 1.0F};
    case NodeType::Aerial: return {0.0F, 0.0F, 1.0F, 1.0F};
    case NodeType::Vantage: return {0.0F, 1.0F, 1.0F, 1.0F};
    case NodeType::AltForm: return {1.0F, 0.0F, 1.0F, 1.0F};
    case NodeType::Hazard: return {1.0F, 1.0F, 0.0F, 1.0F};
    }
    return {1.0F, 1.0F, 1.0F, 1.0F};
}

NodeData3::NodeData3(
    NodeDataStruct3 raw, int index1_value, int index2_value,
    std::shared_ptr<const std::vector<std::uint16_t>> values)
    : node_type(static_cast<NodeType>(raw.node_type)),
      id(raw.id),
      field4(raw.field4),
      count2(static_cast<int>(raw.count2)),
      position{raw.position.x.to_float(), raw.position.y.to_float(),
               raw.position.z.to_float()},
      max_distance(formats::Fixed::to_float(raw.max_distance)),
      index1(index1_value),
      index2(index2_value),
      transform(translation_matrix(position)),
      color(node_type_color(node_type)),
      values_(std::move(values)) {
    if (!values_) {
        values_ = empty_values();
    }
}

NodeData3::NodeData3(formats::Vector3Fx raw_position)
    : position{raw_position.x.to_float(), raw_position.y.to_float(),
                raw_position.z.to_float()},
      transform(translation_matrix(position)),
      color(node_type_color(NodeType::Navigation)),
      values_(empty_values()) {}

NodeData::NodeData(NodeDataHeader header,
                   std::vector<std::uint16_t> set_indices,
                   NodeDataLists data)
    : header_(header),
      set_indices_(std::move(set_indices)),
      data_(std::move(data)) {}

NodeData NodeData::from_bytes(std::span<const std::uint8_t> bytes,
                              bool first_hunt) {
    if (first_hunt) {
        return read_first_hunt(bytes);
    }

    if (bytes.size() < sizeof(std::uint16_t)) {
        throw std::runtime_error("node-data file is smaller than its version");
    }
    BinaryReader version_reader(bytes.subspan(0, sizeof(std::uint16_t)));
    if (version_reader.read_u16_le() == 0) {
        return read_first_hunt(bytes);
    }
    const NodeDataHeader header = read_header(bytes);
    if (header.version != 6) {
        throw std::runtime_error("unexpected node-data version "
                                 + std::to_string(header.version));
    }

    const auto set_indices = read_array<std::uint16_t>(
        bytes, header.index_offset, header.index_count,
        sizeof(std::uint16_t),
        [](BinaryReader& reader) { return reader.read_u16_le(); },
        "set-index table");
    const auto struct1s = read_array<NodeDataStruct1>(
        bytes, header.data_offset, header.data_count, NodeDataStruct1::Size,
        [](BinaryReader& reader) { return read_struct1(reader); },
        "top-level node table");

    struct RawSubList {
        std::vector<NodeDataStruct3> nodes;
    };
    std::vector<std::vector<RawSubList>> raw_data;
    raw_data.reserve(struct1s.size());
    std::uint32_t minimum_offset = std::numeric_limits<std::uint32_t>::max();
    for (const auto& struct1 : struct1s) {
        const auto struct2s = read_array<NodeDataStruct2>(
            bytes, struct1.offset2, struct1.count, NodeDataStruct2::Size,
            [](BinaryReader& reader) { return read_struct2(reader); },
            "second-level node table");
        std::vector<RawSubList> sub;
        sub.reserve(struct2s.size());
        for (const auto& struct2 : struct2s) {
            auto struct3s = read_array<NodeDataStruct3>(
                bytes, struct2.offset3, struct2.count, NodeDataStruct3::Size,
                [](BinaryReader& reader) { return read_struct3(reader); },
                "node record table");
            for (const auto& struct3 : struct3s) {
                minimum_offset = std::min(minimum_offset, struct3.offset1);
            }
            sub.push_back({std::move(struct3s)});
        }
        raw_data.push_back(std::move(sub));
    }

    if (minimum_offset == std::numeric_limits<std::uint32_t>::max()) {
        return NodeData{header, set_indices, {}};
    }
    const auto values = read_values(bytes, minimum_offset);
    auto shared = shared_values(values);
    NodeDataLists data;
    data.reserve(raw_data.size());
    for (auto& sub : raw_data) {
        std::vector<std::vector<NodeData3>> new_sub;
        new_sub.reserve(sub.size());
        for (auto& raw_nodes : sub) {
            std::vector<NodeData3> nodes;
            nodes.reserve(raw_nodes.nodes.size());
            for (const auto& raw : raw_nodes.nodes) {
                const int index1 = value_index(raw.offset1, minimum_offset,
                                               values.size());
                const int index2 = value_index(raw.offset2, minimum_offset,
                                               values.size());
                nodes.emplace_back(raw, index1, index2, shared);
            }
            new_sub.push_back(std::move(nodes));
        }
        data.push_back(std::move(new_sub));
    }
    return NodeData{header, set_indices, std::move(data)};
}

NodeData NodeData::from_bytes(std::vector<std::uint8_t> bytes, bool first_hunt) {
    return from_bytes(std::span<const std::uint8_t>(bytes), first_hunt);
}

NodeData NodeData::read_file(const std::filesystem::path& path, bool first_hunt) {
    return from_bytes(read_all(path), first_hunt);
}

const NodeData3* NodeData::closest_node(formats::Vector3 position,
                                        bool use_max_distance) const noexcept {
    if (data_.empty() || data_[0].empty() || data_[0][0].empty()) {
        return nullptr;
    }
    const NodeData3* result = nullptr;
    float minimum_distance = std::numeric_limits<float>::max();
    for (const auto& node : data_[0][0]) {
        const float distance = (position - node.position).length_squared();
        if (distance < minimum_distance) {
            minimum_distance = distance;
            result = &node;
        }
    }
    if (result != nullptr && use_max_distance) {
        const float max_distance_squared = result->max_distance
            * result->max_distance;
        if (minimum_distance > max_distance_squared) {
            return nullptr;
        }
    }
    return result;
}

} // namespace fruityprime::node
