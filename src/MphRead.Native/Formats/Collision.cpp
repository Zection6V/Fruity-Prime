#include "Formats/collision_layouts.hpp"
#include "Formats/collision_format.hpp"

#include "Utility/binary_reader.hpp"

#include <fstream>
#include <limits>
#include <stdexcept>
#include <utility>

namespace fruityprime::collision {
namespace {

[[nodiscard]] std::vector<std::uint8_t> read_all(
    const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary | std::ios::ate);
    if (!input) {
        throw std::runtime_error("could not open collision file " + path.string());
    }
    const std::streamoff length = input.tellg();
    if (length < 0 || static_cast<std::uintmax_t>(length)
        > std::numeric_limits<std::size_t>::max()) {
        throw std::runtime_error("collision file is too large: " + path.string());
    }
    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(length));
    input.seekg(0);
    if (!bytes.empty()) {
        input.read(reinterpret_cast<char*>(bytes.data()),
                   static_cast<std::streamsize>(bytes.size()));
        if (!input) {
            throw std::runtime_error("could not read collision file " + path.string());
        }
    }
    return bytes;
}

formats::Fixed read_fixed(core::BinaryReader& reader) {
    return formats::Fixed{reader.read_i32_le()};
}

formats::Vector3Fx read_vector3(core::BinaryReader& reader) {
    return formats::Vector3Fx{read_fixed(reader), read_fixed(reader),
                              read_fixed(reader)};
}

formats::Vector4Fx read_vector4(core::BinaryReader& reader) {
    return formats::Vector4Fx{read_fixed(reader), read_fixed(reader),
                              read_fixed(reader), read_fixed(reader)};
}

template <typename Value, typename Decoder>
void read_array(std::span<const std::uint8_t> bytes, std::uint32_t offset,
                std::size_t count, std::size_t size,
                std::vector<Value>& output, Decoder&& decoder) {
    if (offset == 0 || count == 0) {
        return;
    }
    if (count > std::numeric_limits<std::size_t>::max() / size) {
        throw std::runtime_error("collision array size overflows");
    }
    const std::size_t length = count * size;
    if (offset > bytes.size() || length > bytes.size() - offset) {
        throw std::runtime_error("collision array is outside the file");
    }
    output.reserve(count);
    for (std::size_t i = 0; i < count; ++i) {
        core::BinaryReader reader(bytes.subspan(
            static_cast<std::size_t>(offset) + i * size, size));
        output.push_back(decoder(reader));
    }
}

MphHeader read_mph_header(std::span<const std::uint8_t> bytes) {
    core::BinaryReader reader(bytes.subspan(0, MphHeader::Size));
    MphHeader header;
    for (std::uint8_t& value : header.type) {
        value = reader.read_u8();
    }
    header.point_count = reader.read_u32_le();
    header.point_offset = reader.read_u32_le();
    header.plane_count = reader.read_u32_le();
    header.plane_offset = reader.read_u32_le();
    header.point_index_count = reader.read_u32_le();
    header.point_index_offset = reader.read_u32_le();
    header.data_count = reader.read_u32_le();
    header.data_offset = reader.read_u32_le();
    header.data_index_count = reader.read_u32_le();
    header.data_index_offset = reader.read_u32_le();
    header.parts_x = reader.read_i32_le();
    header.parts_y = reader.read_i32_le();
    header.parts_z = reader.read_i32_le();
    header.min_position = read_vector3(reader);
    header.entry_count = reader.read_u32_le();
    header.entry_offset = reader.read_u32_le();
    header.portal_count = reader.read_u32_le();
    header.portal_offset = reader.read_u32_le();
    return header;
}

FhHeader read_fh_header(std::span<const std::uint8_t> bytes) {
    core::BinaryReader reader(bytes.subspan(0, FhHeader::Size));
    FhHeader header;
    header.point_count = reader.read_u32_le();
    header.point_offset = reader.read_u32_le();
    header.plane_count = reader.read_u32_le();
    header.plane_offset = reader.read_u32_le();
    header.vector_count = reader.read_u32_le();
    header.vector_offset = reader.read_u32_le();
    header.data_count = reader.read_u16_le();
    header.data_start_index = reader.read_u16_le();
    header.data_offset = reader.read_u32_le();
    header.data_index_count = reader.read_u32_le();
    header.data_index_offset = reader.read_u32_le();
    header.entry_count = reader.read_u32_le();
    header.entry_offset = reader.read_u32_le();
    header.tree_node_index_count = reader.read_u32_le();
    header.tree_node_index_offset = reader.read_u32_le();
    header.tree_node_count = reader.read_u32_le();
    header.tree_node_offset = reader.read_u32_le();
    header.portal_count = reader.read_u32_le();
    header.portal_offset = reader.read_u32_le();
    return header;
}

MphPortal read_mph_portal(core::BinaryReader& reader) {
    MphPortal portal;
    portal.name = reader.read_raw_string(40);
    portal.node_name1 = reader.read_raw_string(24);
    portal.node_name2 = reader.read_raw_string(24);
    for (auto& point : portal.points) {
        point = read_vector3(reader);
    }
    for (auto& plane : portal.planes) {
        plane = read_vector4(reader);
    }
    portal.plane = read_vector4(reader);
    portal.flags = reader.read_u16_le();
    portal.layer_mask = reader.read_u16_le();
    portal.point_count = reader.read_u16_le();
    portal.unused_00 = reader.read_u8();
    portal.unused_01 = reader.read_u8();
    return portal;
}

FhPortal read_fh_portal(core::BinaryReader& reader) {
    FhPortal portal;
    portal.name = reader.read_raw_string(40);
    portal.node_name1 = reader.read_raw_string(16);
    portal.node_name2 = reader.read_raw_string(16);
    portal.plane = read_vector4(reader);
    portal.vector_count = reader.read_u16_le();
    portal.vector_start_index = reader.read_u16_le();
    portal.field_5c = reader.read_u8();
    portal.field_5d = reader.read_u8();
    portal.padding_5e = reader.read_u16_le();
    return portal;
}

} // namespace

File File::read_file(const std::filesystem::path& path) {
    return File(read_all(path));
}

File File::from_bytes(std::vector<std::uint8_t> bytes) {
    return File(std::move(bytes));
}

File::File(std::vector<std::uint8_t> bytes)
    : bytes_(std::move(bytes)), data_(MphDataSet{}) {
    parse();
}

bool File::is_mph() const noexcept {
    return std::holds_alternative<MphDataSet>(data_);
}

const MphDataSet& File::mph() const {
    return std::get<MphDataSet>(data_);
}

const FhDataSet& File::first_hunt() const {
    return std::get<FhDataSet>(data_);
}

void File::parse() {
    if (bytes_.size() < 4) {
        throw std::runtime_error("collision file is smaller than its type");
    }
    if (bytes_[0] == 'w' && bytes_[1] == 'c'
        && bytes_[2] == '0' && bytes_[3] == '1') {
        parse_mph();
    } else {
        parse_first_hunt();
    }
}

void File::parse_mph() {
    if (bytes_.size() < MphHeader::Size) {
        throw std::runtime_error("MPH collision file is smaller than its header");
    }
    const std::span<const std::uint8_t> bytes(bytes_);
    MphDataSet result;
    result.header = read_mph_header(bytes);
    read_array(bytes, result.header.point_offset, result.header.point_count,
               sizeof(std::int32_t) * 3, result.points,
               [](core::BinaryReader& reader) { return read_vector3(reader); });
    read_array(bytes, result.header.plane_offset, result.header.plane_count,
               sizeof(std::int32_t) * 4, result.planes,
               [](core::BinaryReader& reader) { return read_vector4(reader); });
    read_array(bytes, result.header.point_index_offset,
               result.header.point_index_count, sizeof(std::uint16_t),
               result.point_indices,
               [](core::BinaryReader& reader) { return reader.read_u16_le(); });
    read_array(bytes, result.header.data_offset, result.header.data_count,
               MphData::Size, result.data, [](core::BinaryReader& reader) {
                   return MphData{reader.read_i32_le(), reader.read_u16_le(),
                                  reader.read_u16_le(), reader.read_u16_le(),
                                  reader.read_u16_le(), reader.read_u16_le(),
                                  reader.read_u16_le()};
               });
    read_array(bytes, result.header.data_index_offset,
               result.header.data_index_count, sizeof(std::uint16_t),
               result.data_indices,
               [](core::BinaryReader& reader) { return reader.read_u16_le(); });
    read_array(bytes, result.header.entry_offset, result.header.entry_count,
               MphEntry::Size, result.entries, [](core::BinaryReader& reader) {
                   return MphEntry{reader.read_u16_le(), reader.read_u16_le()};
               });
    read_array(bytes, result.header.portal_offset, result.header.portal_count,
               MphPortal::Size, result.portals,
               [](core::BinaryReader& reader) { return read_mph_portal(reader); });
    data_ = std::move(result);
}

void File::parse_first_hunt() {
    if (bytes_.size() < FhHeader::Size) {
        throw std::runtime_error("First Hunt collision file is smaller than its header");
    }
    const std::span<const std::uint8_t> bytes(bytes_);
    FhDataSet result;
    result.header = read_fh_header(bytes);
    read_array(bytes, result.header.point_offset, result.header.point_count,
               sizeof(std::int32_t) * 3, result.points,
               [](core::BinaryReader& reader) { return read_vector3(reader); });
    read_array(bytes, result.header.plane_offset, result.header.plane_count,
               sizeof(std::int32_t) * 4, result.planes,
               [](core::BinaryReader& reader) { return read_vector4(reader); });
    read_array(bytes, result.header.vector_offset, result.header.vector_count,
               FhVector::Size, result.vectors,
               [](core::BinaryReader& reader) {
                   return FhVector{reader.read_u16_le(), reader.read_u16_le(),
                                   reader.read_u16_le()};
               });
    read_array(bytes, result.header.data_offset, result.header.data_count,
               FhData::Size, result.data, [](core::BinaryReader& reader) {
                   return FhData{reader.read_u16_le(), reader.read_u16_le(),
                                 reader.read_u16_le()};
               });
    read_array(bytes, result.header.data_index_offset,
               result.header.data_index_count, sizeof(std::uint16_t),
               result.data_indices,
               [](core::BinaryReader& reader) { return reader.read_u16_le(); });
    read_array(bytes, result.header.entry_offset, result.header.entry_count,
               FhEntry::Size, result.entries, [](core::BinaryReader& reader) {
                   return FhEntry{read_vector3(reader), read_vector3(reader),
                                  reader.read_u16_le(), reader.read_u16_le()};
               });
    read_array(bytes, result.header.tree_node_index_offset,
               result.header.tree_node_index_count, sizeof(std::int32_t),
               result.tree_node_indices,
               [](core::BinaryReader& reader) { return reader.read_i32_le(); });
    read_array(bytes, result.header.tree_node_offset,
               result.header.tree_node_count, FhTreeNode::Size,
               result.tree_nodes, [](core::BinaryReader& reader) {
                   return FhTreeNode{read_vector3(reader), read_vector3(reader),
                                     reader.read_u16_le(), reader.read_u16_le()};
               });
    read_array(bytes, result.header.portal_offset, result.header.portal_count,
               FhPortal::Size, result.portals,
               [](core::BinaryReader& reader) { return read_fh_portal(reader); });
    data_ = std::move(result);
}

MphDataSet MphDataSet::with_layer_mask(int room_layer_mask) const {
    if (room_layer_mask == -1) {
        return *this;
    }
    MphDataSet result = *this;
    result.data.clear();
    result.data_indices.clear();
    result.entries.clear();
    result.portals.clear();
    for (const auto& portal : portals) {
        if ((portal.layer_mask & 4) != 0
            || (portal.layer_mask & room_layer_mask) != 0) {
            result.portals.push_back(portal);
        }
    }
    std::vector<int> index_map(data.size(), -1);
    for (const auto& entry : entries) {
        if (entry.data_count == 0) {
            result.entries.push_back(entry);
            continue;
        }
        MphEntry next{0, static_cast<std::uint16_t>(result.data_indices.size())};
        for (std::size_t i = 0; i < entry.data_count; ++i) {
            const auto old_index = data_indices.at(entry.data_start_index + i);
            int& mapped = index_map.at(old_index);
            if (mapped == -1) {
                const auto& item = data.at(old_index);
                if ((item.layer_mask & 4) == 0
                    && (item.layer_mask & room_layer_mask) == 0) {
                    continue;
                }
                mapped = static_cast<std::uint16_t>(result.data.size());
                result.data.push_back(item);
            }
            result.data_indices.push_back(static_cast<std::uint16_t>(mapped));
            ++next.data_count;
        }
        result.entries.push_back(next);
    }
    return result;
}

std::array<int, 3> MphDataSet::part_index_from_entry(int index) const {
    const int x = header.parts_x;
    const int xz = x * header.parts_z;
    if (x == 0 || xz == 0) {
        throw std::runtime_error("collision partition division by zero");
    }
    const int y_inc = index / xz;
    const int z_inc = (index - y_inc * xz) / x;
    return {index - y_inc * xz - z_inc * x, y_inc, z_inc};
}

int MphDataSet::entry_index_from_point(float x, float y, float z) const {
    // C# truncates toward zero and only checks the upper bounds here.
    const int xi = static_cast<int>((x - header.min_position.x.to_float()) / 4.0F);
    const int yi = static_cast<int>((y - header.min_position.y.to_float()) / 4.0F);
    const int zi = static_cast<int>((z - header.min_position.z.to_float()) / 4.0F);
    if (xi >= header.parts_x || yi >= header.parts_y || zi >= header.parts_z) {
        return -1;
    }
    return yi * header.parts_x * header.parts_z + zi * header.parts_x + xi;
}

} // namespace fruityprime::collision
