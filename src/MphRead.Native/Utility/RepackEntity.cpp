#include "Utility/repack_entity.hpp"

#include "Read.hpp"

#include <algorithm>
#include <limits>
#include <stdexcept>
#include <utility>

namespace fruityprime::utility::entity_repack {
namespace {

constexpr std::size_t kDataHeaderSize = entity::DataHeader::Size;
constexpr std::size_t kMphEntrySize = entity::Entry::Size;
constexpr std::size_t kFhEntrySize = entity::FirstHuntEntry::Size;

void require_capacity(std::span<const std::uint8_t> bytes,
                      std::size_t offset, std::size_t size,
                      const char* description) {
    if (offset > bytes.size() || size > bytes.size() - offset) {
        throw std::runtime_error(std::string(description)
                                 + " is outside the entity payload");
    }
}

void patch_u16(std::vector<std::uint8_t>& bytes, std::size_t offset,
               std::uint16_t value) {
    if (offset > bytes.size() || sizeof(value) > bytes.size() - offset) {
        throw std::runtime_error("entity header patch is outside the payload");
    }
    bytes[offset] = static_cast<std::uint8_t>(value);
    bytes[offset + 1] = static_cast<std::uint8_t>(value >> 8);
}

void patch_i16(std::vector<std::uint8_t>& bytes, std::size_t offset,
               std::int16_t value) {
    patch_u16(bytes, offset, static_cast<std::uint16_t>(value));
}

void patch_i32(std::vector<std::uint8_t>& bytes, std::size_t offset,
               std::int32_t value) {
    if (offset > bytes.size() || sizeof(value) > bytes.size() - offset) {
        throw std::runtime_error("entity vector patch is outside the payload");
    }
    const auto raw = static_cast<std::uint32_t>(value);
    bytes[offset] = static_cast<std::uint8_t>(raw);
    bytes[offset + 1] = static_cast<std::uint8_t>(raw >> 8);
    bytes[offset + 2] = static_cast<std::uint8_t>(raw >> 16);
    bytes[offset + 3] = static_cast<std::uint8_t>(raw >> 24);
}

void patch_vector3(std::vector<std::uint8_t>& bytes, std::size_t offset,
                   formats::Vector3Fx value) {
    patch_i32(bytes, offset, value.x.value);
    patch_i32(bytes, offset + sizeof(std::int32_t), value.y.value);
    patch_i32(bytes, offset + 2 * sizeof(std::int32_t), value.z.value);
}

void patch_name(std::vector<std::uint8_t>& bytes, std::size_t offset,
                std::string_view name, std::size_t width) {
    if (name.size() > width) {
        throw std::invalid_argument("entity node name is longer than "
                                    + std::to_string(width) + " bytes");
    }
    require_capacity(std::span<const std::uint8_t>(bytes), offset, width,
                     "entity entry name");
    std::fill(bytes.begin() + static_cast<std::ptrdiff_t>(offset),
              bytes.begin() + static_cast<std::ptrdiff_t>(offset + width), 0);
    std::copy(name.begin(), name.end(),
              bytes.begin() + static_cast<std::ptrdiff_t>(offset));
}

[[nodiscard]] std::uint32_t offset32(std::size_t value) {
    if (value > std::numeric_limits<std::uint32_t>::max()) {
        throw std::length_error("entity file is larger than 4 GiB");
    }
    return static_cast<std::uint32_t>(value);
}

void align4(std::vector<std::uint8_t>& bytes) {
    while ((bytes.size() & 3U) != 0) {
        bytes.push_back(0);
    }
}

void write_common_header(std::vector<std::uint8_t>& payload,
                         const Record& record) {
    if (payload.size() < kDataHeaderSize) {
        throw std::invalid_argument("entity record payload is smaller than "
                                    "EntityDataHeader");
    }
    patch_u16(payload, 0, record.type);
    patch_i16(payload, 2, record.entity_id);
    patch_vector3(payload, 4, record.position);
    patch_vector3(payload, 16, record.up_vector);
    patch_vector3(payload, 28, record.facing_vector);
}

void write_u32_at(std::vector<std::uint8_t>& bytes, std::size_t offset,
                  std::uint32_t value) {
    if (offset > bytes.size() || sizeof(value) > bytes.size() - offset) {
        throw std::runtime_error("entity table patch is outside the output");
    }
    bytes[offset] = static_cast<std::uint8_t>(value);
    bytes[offset + 1] = static_cast<std::uint8_t>(value >> 8);
    bytes[offset + 2] = static_cast<std::uint8_t>(value >> 16);
    bytes[offset + 3] = static_cast<std::uint8_t>(value >> 24);
}

} // namespace

Document decode(const entity::File& file) {
    Document document;
    document.version = file.version();
    document.layer_lengths = file.header().lengths;
    if (file.is_first_hunt()) {
        document.records.reserve(file.first_hunt_records().size());
        for (const auto& source : file.first_hunt_records()) {
            document.records.push_back(Record{
                source.entry.node_name,
                0xffff,
                source.header.type,
                source.header.entity_id,
                source.header.position,
                source.header.up_vector,
                source.header.facing_vector,
                source.payload});
        }
    } else {
        document.records.reserve(file.records().size());
        for (const auto& source : file.records()) {
            document.records.push_back(Record{
                source.entry.node_name,
                source.entry.layer_mask,
                source.header.type,
                source.header.entity_id,
                source.header.position,
                source.header.up_vector,
                source.header.facing_vector,
                source.payload});
        }
    }
    return document;
}

std::vector<std::uint8_t> encode(const Document& document) {
    if (document.version != 1 && document.version != 2) {
        throw std::invalid_argument("entity document version must be 1 or 2");
    }
    if (document.records.size()
        > std::numeric_limits<std::uint16_t>::max()) {
        throw std::length_error("entity table contains too many records");
    }

    const std::size_t header_size = document.is_first_hunt()
        ? sizeof(std::uint32_t) : entity::Header::Size;
    const std::size_t entry_size = document.is_first_hunt()
        ? kFhEntrySize : kMphEntrySize;
    if (document.records.size() > (std::numeric_limits<std::size_t>::max()
                                   - header_size) / entry_size - 1) {
        throw std::length_error("entity entry table is too large");
    }
    const std::size_t table_size = header_size
        + entry_size * (document.records.size() + 1);
    std::vector<std::uint8_t> result(table_size, 0);
    write_u32_at(result, 0, document.version);

    std::array<std::uint16_t, 16> lengths{};
    if (!document.is_first_hunt()) {
        for (const Record& record : document.records) {
            for (std::size_t layer = 0; layer < lengths.size(); ++layer) {
                if ((record.layer_mask & (std::uint16_t{1} << layer)) != 0) {
                    if (lengths[layer] == std::numeric_limits<std::uint16_t>::max()) {
                        throw std::length_error("entity layer contains too many records");
                    }
                    ++lengths[layer];
                }
            }
        }
        for (std::size_t layer = 0; layer < lengths.size(); ++layer) {
            patch_u16(result, sizeof(std::uint32_t) + layer * sizeof(std::uint16_t),
                      lengths[layer]);
        }
    }

    struct WrittenRecord {
        std::size_t entry_offset = 0;
        std::uint32_t data_offset = 0;
        std::uint16_t length = 0;
    };
    std::vector<WrittenRecord> written;
    written.reserve(document.records.size());
    for (std::size_t index = 0; index < document.records.size(); ++index) {
        const Record& record = document.records[index];
        std::vector<std::uint8_t> payload = record.payload;
        write_common_header(payload, record);
        if (payload.size() > std::numeric_limits<std::uint16_t>::max()) {
            throw std::length_error("entity record payload is larger than 65535 bytes");
        }
        const std::size_t data_offset = result.size();
        written.push_back({header_size + entry_size * index,
                           offset32(data_offset),
                           static_cast<std::uint16_t>(payload.size())});
        result.insert(result.end(), payload.begin(), payload.end());
        if (index + 1 < document.records.size()) {
            align4(result);
        }
    }

    for (std::size_t index = 0; index < document.records.size(); ++index) {
        const Record& record = document.records[index];
        const WrittenRecord& output = written[index];
        patch_name(result, output.entry_offset, record.node_name, 16);
        if (document.is_first_hunt()) {
            write_u32_at(result, output.entry_offset + 16,
                         output.data_offset);
        } else {
            patch_u16(result, output.entry_offset + 16, record.layer_mask);
            patch_u16(result, output.entry_offset + 18, output.length);
            write_u32_at(result, output.entry_offset + 20,
                         output.data_offset);
        }
    }
    return result;
}

std::vector<std::uint8_t> repack(std::span<const std::uint8_t> bytes) {
    std::vector<std::uint8_t> input(bytes.begin(), bytes.end());
    return encode(decode(entity::File::from_bytes(std::move(input))));
}

std::vector<std::uint8_t> repack_file(const std::filesystem::path& path) {
    return repack(read::file(path));
}

ByteComparison compare(std::span<const std::uint8_t> left,
                        std::span<const std::uint8_t> right) noexcept {
    ByteComparison result;
    const std::size_t common = std::min(left.size(), right.size());
    for (std::size_t index = 0; index < common; ++index) {
        if (left[index] != right[index]) {
            if (result.equal) {
                result.first_difference = index;
            }
            result.equal = false;
            ++result.differing_bytes;
        }
    }
    if (left.size() != right.size()) {
        if (result.equal) {
            result.first_difference = common;
        }
        result.equal = false;
        result.differing_bytes += std::max(left.size(), right.size()) - common;
    }
    return result;
}

} // namespace fruityprime::utility::entity_repack
