#include "Formats/entity_layouts.hpp"
#include "Formats/entity_format.hpp"

#include "Utility/binary_reader.hpp"

#include <fstream>
#include <limits>
#include <stdexcept>
#include <utility>

namespace fruityprime::entity {
namespace {

[[nodiscard]] std::vector<std::uint8_t> read_all(
    const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary | std::ios::ate);
    if (!input) {
        throw std::runtime_error("could not open entity file " + path.string());
    }
    const std::streamoff length = input.tellg();
    if (length < 0 || static_cast<std::uintmax_t>(length)
        > std::numeric_limits<std::size_t>::max()) {
        throw std::runtime_error("entity file is too large: " + path.string());
    }
    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(length));
    input.seekg(0);
    if (!bytes.empty()) {
        input.read(reinterpret_cast<char*>(bytes.data()),
                   static_cast<std::streamsize>(bytes.size()));
        if (!input) {
            throw std::runtime_error("could not read entity file " + path.string());
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

DataHeader read_data_header(std::span<const std::uint8_t> bytes,
                            std::size_t offset) {
    if (offset > bytes.size() || DataHeader::Size > bytes.size() - offset) {
        throw std::runtime_error("entity data header is outside the file");
    }
    core::BinaryReader reader(bytes.subspan(offset, DataHeader::Size));
    DataHeader header;
    header.type = reader.read_u16_le();
    header.entity_id = reader.read_i16_le();
    header.position = read_vector3(reader);
    header.up_vector = read_vector3(reader);
    header.facing_vector = read_vector3(reader);
    return header;
}

[[nodiscard]] std::size_t first_hunt_record_size(std::uint16_t type) {
    // These are Marshal.SizeOf<T>() values from Formats/Entity.cs.  The
    // First Hunt index stores only a node name and data offset, so this
    // mapping is the length contract used by ReadFirstHuntEntity<T>().
    switch (type) {
    case 1:  // FhPlayerSpawnEntityData (type 101 after classification)
        return 43;
    case 3:  // FhDoorEntityData
        return 64;
    case 4:  // FhItemSpawnEntityData
        return 50;
    case 6:  // FhEnemySpawnEntityData
        return 268;
    case 9:  // FhTriggerVolumeEntityData
        return 272;
    case 10: // FhAreaVolumeEntityData
        return 260;
    case 11: // FhPlatformEntityData
        return 236;
    case 12: // FhJumpPadEntityData
        return 272;
    case 13: // PointModuleEntityData
        return 45;
    case 14: // FhMorphCameraEntityData
        return 104;
    default:
        throw std::runtime_error(
            "unsupported First Hunt entity type " + std::to_string(type));
    }
}

} // namespace

File File::read_file(const std::filesystem::path& path) {
    return File(read_all(path));
}

File File::from_bytes(std::vector<std::uint8_t> bytes) {
    return File(std::move(bytes));
}

File::File(std::vector<std::uint8_t> bytes)
    : bytes_(std::move(bytes)) {
    parse();
}

void File::parse() {
    if (bytes_.size() < sizeof(std::uint32_t)) {
        throw std::runtime_error("entity file is smaller than its version");
    }
    const std::span<const std::uint8_t> bytes(bytes_);
    core::BinaryReader reader(bytes.subspan(0, sizeof(std::uint32_t)));
    version_ = reader.read_u32_le();
    if (version_ == 1) {
        parse_first_hunt();
    } else if (version_ == 2) {
        parse_version_two();
    } else {
        throw std::runtime_error("unsupported entity file version "
                                 + std::to_string(version_));
    }
}

void File::parse_version_two() {
    if (bytes_.size() < Header::Size) {
        throw std::runtime_error("entity file is smaller than its header");
    }
    const std::span<const std::uint8_t> bytes(bytes_);
    core::BinaryReader reader(bytes.subspan(0, Header::Size));
    header_.version = reader.read_u32_le();
    for (std::uint16_t& length : header_.lengths) {
        length = reader.read_u16_le();
    }

    std::size_t entry_offset = Header::Size;
    for (;;) {
        if (entry_offset > bytes.size()
            || Entry::Size > bytes.size() - entry_offset) {
            throw std::runtime_error("entity entry is outside the file");
        }
        core::BinaryReader entry_reader(
            bytes.subspan(entry_offset, Entry::Size));
        Entry entry;
        entry.node_name = entry_reader.read_raw_string(16);
        entry.layer_mask = entry_reader.read_u16_le();
        entry.length = entry_reader.read_u16_le();
        entry.data_offset = entry_reader.read_u32_le();
        entry_offset += Entry::Size;
        if (entry.data_offset == 0) {
            break;
        }
        if (entry.length < DataHeader::Size) {
            throw std::runtime_error("entity payload is smaller than its header");
        }
        if (entry.data_offset > bytes.size()
            || entry.length > bytes.size() - entry.data_offset) {
            throw std::runtime_error("entity payload is outside the file");
        }
        Record record;
        record.entry = entry;
        record.header = read_data_header(bytes, entry.data_offset);
        const auto payload = bytes.subspan(entry.data_offset, entry.length);
        record.payload.assign(payload.begin(), payload.end());
        records_.push_back(std::move(record));
    }
}

void File::parse_first_hunt() {
    const std::span<const std::uint8_t> bytes(bytes_);
    std::size_t entry_offset = sizeof(std::uint32_t);
    for (;;) {
        if (entry_offset > bytes.size()
            || FirstHuntEntry::Size > bytes.size() - entry_offset) {
            throw std::runtime_error("First Hunt entity entry is outside the file");
        }
        core::BinaryReader entry_reader(
            bytes.subspan(entry_offset, FirstHuntEntry::Size));
        FirstHuntEntry entry;
        entry.node_name = entry_reader.read_raw_string(16);
        entry.data_offset = entry_reader.read_u32_le();
        entry_offset += FirstHuntEntry::Size;
        if (entry.data_offset == 0) {
            break;
        }
        const auto header = read_data_header(bytes, entry.data_offset);
        const auto record_size = first_hunt_record_size(header.type);
        if (entry.data_offset > bytes.size()
            || record_size > bytes.size() - entry.data_offset) {
            throw std::runtime_error(
                "First Hunt entity payload is outside the file");
        }
        FirstHuntRecord record;
        record.entry = entry;
        record.header = header;
        const auto payload = bytes.subspan(entry.data_offset, record_size);
        record.payload.assign(payload.begin(), payload.end());
        first_hunt_records_.push_back(std::move(record));
    }
}

} // namespace fruityprime::entity
