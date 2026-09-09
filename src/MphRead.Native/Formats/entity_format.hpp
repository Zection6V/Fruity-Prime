#pragma once

#include "Formats/fixed.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace fruityprime::entity {

struct Header {
    static constexpr std::size_t Size = 36;

    std::uint32_t version = 0;
    std::array<std::uint16_t, 16> lengths{};
};

struct Entry {
    static constexpr std::size_t Size = 24;

    std::string node_name;
    std::uint16_t layer_mask = 0;
    std::uint16_t length = 0;
    std::uint32_t data_offset = 0;
};

struct FirstHuntEntry {
    static constexpr std::size_t Size = 20;

    std::string node_name;
    std::uint32_t data_offset = 0;
};

struct DataHeader {
    static constexpr std::size_t Size = 40;

    std::uint16_t type = 0;
    std::int16_t entity_id = 0;
    formats::Vector3Fx position;
    formats::Vector3Fx up_vector;
    formats::Vector3Fx facing_vector;
};

struct Record {
    Entry entry;
    DataHeader header;
    std::vector<std::uint8_t> payload;
};

struct FirstHuntRecord {
    FirstHuntEntry entry;
    DataHeader header;
    // First Hunt's index has no length field.  Read.cs selects the fixed
    // record size from the type and reads that many bytes from DataOffset;
    // retain the same complete record here instead of dropping everything
    // after EntityDataHeader.
    std::vector<std::uint8_t> payload;
};

class File {
public:
    [[nodiscard]] static File read_file(const std::filesystem::path& path);
    [[nodiscard]] static File from_bytes(std::vector<std::uint8_t> bytes);

    [[nodiscard]] bool is_first_hunt() const noexcept { return version_ == 1; }
    [[nodiscard]] std::uint32_t version() const noexcept { return version_; }
    [[nodiscard]] const Header& header() const noexcept { return header_; }
    [[nodiscard]] const std::vector<Record>& records() const noexcept {
        return records_;
    }
    [[nodiscard]] const std::vector<FirstHuntRecord>& first_hunt_records()
        const noexcept {
        return first_hunt_records_;
    }

private:
    explicit File(std::vector<std::uint8_t> bytes);

    void parse();
    void parse_version_two();
    void parse_first_hunt();

    std::vector<std::uint8_t> bytes_;
    std::uint32_t version_ = 0;
    Header header_;
    std::vector<Record> records_;
    std::vector<FirstHuntRecord> first_hunt_records_;
};

} // namespace fruityprime::entity
