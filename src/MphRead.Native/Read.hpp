#pragma once

#include "Utility/binary_reader.hpp"

#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <limits>
#include <span>
#include <string>
#include <type_traits>
#include <vector>

namespace fruityprime::read {

using BinaryReader = core::BinaryReader;

[[nodiscard]] std::vector<std::uint8_t> file(
    const std::filesystem::path& path);
[[nodiscard]] std::string string(std::span<const std::uint8_t> bytes,
                                 std::size_t offset, std::size_t length,
                                 bool preserve_bytes = false);
[[nodiscard]] bool write_file(const std::filesystem::path& path,
                              std::span<const std::uint8_t> bytes) noexcept;

// The managed Read.SpanRead* helpers advance a caller-owned cursor after a
// checked, little-endian scalar read.  Keep both cursor and fixed-offset forms
// so the native format readers can use the same operation for packed records
// and tables.
[[nodiscard]] std::int32_t span_read_i32(
    std::span<const std::uint8_t> bytes, std::size_t& offset);
[[nodiscard]] std::uint32_t span_read_u32(
    std::span<const std::uint8_t> bytes, std::size_t& offset);
[[nodiscard]] std::uint16_t span_read_u16(
    std::span<const std::uint8_t> bytes, std::size_t& offset);
[[nodiscard]] std::int32_t span_read_i32_at(
    std::span<const std::uint8_t> bytes, std::size_t offset);
[[nodiscard]] std::uint32_t span_read_u32_at(
    std::span<const std::uint8_t> bytes, std::size_t offset);
[[nodiscard]] std::uint16_t span_read_u16_at(
    std::span<const std::uint8_t> bytes, std::size_t offset);

// Marshal.SizeOf/ReadStruct equivalents for the fixed little-endian records
// in Formats/RawFormats.cs.  The native build currently targets little-endian
// desktop CPUs; use the field-wise BinaryReader for a future big-endian host.
template <typename T>
[[nodiscard]] T read_struct(std::span<const std::uint8_t> bytes,
                            std::size_t offset = 0) {
    static_assert(std::is_trivially_copyable_v<T>,
                  "read_struct requires a fixed-layout record");
    static_assert(std::endian::native == std::endian::little,
                  "native fixed-layout reads require little-endian host");
    if (offset > bytes.size() || sizeof(T) > bytes.size() - offset) {
        throw std::out_of_range("fixed record is outside the input");
    }
    T result{};
    std::memcpy(&result, bytes.data() + offset, sizeof(T));
    return result;
}

template <typename T>
[[nodiscard]] std::vector<T> do_offsets(std::span<const std::uint8_t> bytes,
                                         std::size_t offset,
                                         std::size_t count) {
    static_assert(std::is_trivially_copyable_v<T>,
                  "do_offsets requires a fixed-layout record");
    if (offset == 0 || count == 0) {
        return {};
    }
    if (count > (std::numeric_limits<std::size_t>::max() - offset)
                    / sizeof(T)
        || offset + count * sizeof(T) > bytes.size()) {
        throw std::out_of_range("offset table is outside the input");
    }
    std::vector<T> result;
    result.reserve(count);
    for (std::size_t index = 0; index < count; ++index) {
        result.push_back(read_struct<T>(
            bytes, offset + index * sizeof(T)));
    }
    return result;
}

template <typename T>
[[nodiscard]] T do_offset(std::span<const std::uint8_t> bytes,
                          std::size_t offset) {
    if (offset == 0) {
        throw std::out_of_range("single offset record is null");
    }
    const auto result = do_offsets<T>(bytes, offset, 1);
    return result.front();
}

[[nodiscard]] inline std::vector<std::uint32_t> do_list_null_end(
    std::span<const std::uint8_t> bytes, std::size_t offset) {
    if (offset == 0) {
        return {};
    }
    if (offset > bytes.size()) {
        throw std::out_of_range("null-ended offset list is outside the input");
    }
    core::BinaryReader reader(bytes.subspan(offset));
    std::vector<std::uint32_t> result;
    while (reader.remaining() >= 4) {
        const std::uint32_t value = reader.read_u32_le();
        if (value == 0) {
            return result;
        }
        result.push_back(value);
    }
    throw std::out_of_range("null-ended offset list has no terminator");
}

// Read.cs exposes three string variants.  read_string follows ASCII decoder
// replacement for bytes above 0x7f, read_string_table keeps every byte as a
// one-byte character, and read_strings reads count consecutive NUL-terminated
// ASCII strings.  A max length means "until the end of the input", which is a
// safe native equivalent of the managed Int32.MaxValue default.
[[nodiscard]] std::string read_string(
    std::span<const std::uint8_t> bytes, std::size_t offset,
    std::size_t length = std::numeric_limits<std::size_t>::max());
[[nodiscard]] std::string read_string_table(
    std::span<const std::uint8_t> bytes, std::size_t offset,
    std::size_t length = std::numeric_limits<std::size_t>::max());
[[nodiscard]] std::vector<std::string> read_strings(
    std::span<const std::uint8_t> bytes, std::size_t offset,
    std::size_t count);

// Read.cs::ReadKanjiFont converts the cartridge's 1-bit, 16-pixel-wide
// Japanese glyph rows into the 4-bit character tiles consumed by the DS text
// renderer.  This is the byte-oriented part of that routine; path selection
// (ingame versus multiplayer) remains an asset-store concern.
struct KanjiFontData {
    std::int32_t count = 0;
    std::uint16_t width = 0;
    std::uint16_t height = 0;
    std::vector<std::uint8_t> character_data;
};

[[nodiscard]] KanjiFontData read_kanji_font(
    std::span<const std::uint8_t> bytes);

struct ArchiveExtractionResult {
    std::filesystem::path output_directory;
    std::size_t files_written = 0;
};

// Read.cs's ExtractArchive convenience path.  Archive::read_file handles both
// plain SNDFILE archives and the LZ-0x10 wrapper used by cartridge resources.
// If output_directory is empty, use the managed tool's sibling
// "../_archives/<stem>" location.
[[nodiscard]] ArchiveExtractionResult extract_archive(
    const std::filesystem::path& path,
    const std::filesystem::path& output_directory = {});

} // namespace fruityprime::read

namespace MphReadNative {
namespace Read = ::fruityprime::read;
using BinaryReader = ::fruityprime::read::BinaryReader;
}
