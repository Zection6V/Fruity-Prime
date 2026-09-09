#pragma once

#include "Formats/entity_format.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace fruityprime::utility::entity_repack {

// Editable representation of one entry in Formats/Entity.cs.  The payload is
// intentionally retained in addition to the decoded data header: entity
// payloads contain game-specific bytes that are not understood by every
// editor, and a repack must not erase those bytes just because the native
// reader has not decoded them yet.
struct Record {
    std::string node_name;
    std::uint16_t layer_mask = 0xffff;
    std::uint16_t type = 0;
    std::int16_t entity_id = -1;
    formats::Vector3Fx position;
    formats::Vector3Fx up_vector;
    formats::Vector3Fx facing_vector;
    std::vector<std::uint8_t> payload;
};

struct Document {
    std::uint32_t version = 2;
    std::array<std::uint16_t, 16> layer_lengths{};
    std::vector<Record> records;

    [[nodiscard]] bool is_first_hunt() const noexcept { return version == 1; }
};

struct ByteComparison {
    bool equal = true;
    std::size_t differing_bytes = 0;
    std::size_t first_difference = 0;
};

// Converts the parsed cartridge representation into an editable document.
// Both MPH (version 2) and First Hunt (version 1) entry tables are supported.
[[nodiscard]] Document decode(const entity::File& file);

// Writes the same little-endian, 4-byte-aligned entity table layout as
// RepackEntity.cs.  Common data-header fields are written from Record, while
// all type-specific payload bytes remain intact.
[[nodiscard]] std::vector<std::uint8_t> encode(const Document& document);

// Convenience paths used by command-line tools and tests.
[[nodiscard]] std::vector<std::uint8_t> repack(
    std::span<const std::uint8_t> bytes);
[[nodiscard]] std::vector<std::uint8_t> repack_file(
    const std::filesystem::path& path);

[[nodiscard]] ByteComparison compare(
    std::span<const std::uint8_t> left,
    std::span<const std::uint8_t> right) noexcept;

} // namespace fruityprime::utility::entity_repack
