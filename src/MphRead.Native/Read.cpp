#include "Read.hpp"

#include <fstream>
#include <limits>
#include <stdexcept>
#include <utility>

namespace fruityprime::read {
namespace {

template <typename T>
[[nodiscard]] T scalar_at(std::span<const std::uint8_t> bytes,
                          std::size_t offset) {
    return read_struct<T>(bytes, offset);
}

[[nodiscard]] char ascii_decode(std::uint8_t value) noexcept {
    return value > 0x7fU ? '?' : static_cast<char>(value);
}

[[nodiscard]] std::size_t bounded_string_length(
    std::span<const std::uint8_t> bytes, std::size_t offset,
    std::size_t length) {
    if (offset > bytes.size()) {
        throw std::out_of_range("string read is outside the input");
    }
    const std::size_t available = bytes.size() - offset;
    if (length == std::numeric_limits<std::size_t>::max()) {
        return available;
    }
    if (length > available) {
        throw std::out_of_range("string read is outside the input");
    }
    return length;
}

} // namespace

std::vector<std::uint8_t> file(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary | std::ios::ate);
    if (!input) {
        throw std::runtime_error("could not open file: " + path.string());
    }
    const auto end = input.tellg();
    if (end < 0) {
        throw std::runtime_error("could not size file: " + path.string());
    }
    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(end));
    input.seekg(0, std::ios::beg);
    if (!bytes.empty()) {
        input.read(reinterpret_cast<char*>(bytes.data()),
                   static_cast<std::streamsize>(bytes.size()));
    }
    if (!input && !input.eof()) {
        throw std::runtime_error("could not read file: " + path.string());
    }
    return bytes;
}

std::string string(std::span<const std::uint8_t> bytes, std::size_t offset,
                   std::size_t length, bool preserve_bytes) {
    if (offset > bytes.size() || length > bytes.size() - offset) {
        throw std::out_of_range("string read is outside the input");
    }
    std::size_t actual_length = 0;
    while (actual_length < length && bytes[offset + actual_length] != 0) {
        ++actual_length;
    }
    std::string result;
    result.reserve(actual_length);
    for (std::size_t index = 0; index < actual_length; ++index) {
        const auto value = bytes[offset + index];
        result.push_back(!preserve_bytes && (value < 32 || value > 126)
                             ? '?'
                             : static_cast<char>(value));
    }
    return result;
}

bool write_file(const std::filesystem::path& path,
                std::span<const std::uint8_t> bytes) noexcept {
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    if (!output) {
        return false;
    }
    if (!bytes.empty()) {
        output.write(reinterpret_cast<const char*>(bytes.data()),
                     static_cast<std::streamsize>(bytes.size()));
    }
    return static_cast<bool>(output);
}

std::int32_t span_read_i32(std::span<const std::uint8_t> bytes,
                           std::size_t& offset) {
    const auto result = scalar_at<std::int32_t>(bytes, offset);
    offset += sizeof(result);
    return result;
}

std::uint32_t span_read_u32(std::span<const std::uint8_t> bytes,
                            std::size_t& offset) {
    const auto result = scalar_at<std::uint32_t>(bytes, offset);
    offset += sizeof(result);
    return result;
}

std::uint16_t span_read_u16(std::span<const std::uint8_t> bytes,
                            std::size_t& offset) {
    const auto result = scalar_at<std::uint16_t>(bytes, offset);
    offset += sizeof(result);
    return result;
}

std::int32_t span_read_i32_at(std::span<const std::uint8_t> bytes,
                              std::size_t offset) {
    return scalar_at<std::int32_t>(bytes, offset);
}

std::uint32_t span_read_u32_at(std::span<const std::uint8_t> bytes,
                               std::size_t offset) {
    return scalar_at<std::uint32_t>(bytes, offset);
}

std::uint16_t span_read_u16_at(std::span<const std::uint8_t> bytes,
                               std::size_t offset) {
    return scalar_at<std::uint16_t>(bytes, offset);
}

std::string read_string(std::span<const std::uint8_t> bytes,
                        std::size_t offset, std::size_t length) {
    const std::size_t bounded = bounded_string_length(bytes, offset, length);
    std::size_t actual_length = 0;
    while (actual_length < bounded && bytes[offset + actual_length] != 0) {
        ++actual_length;
    }
    std::string result;
    result.reserve(actual_length);
    for (std::size_t index = 0; index < actual_length; ++index) {
        result.push_back(ascii_decode(bytes[offset + index]));
    }
    return result;
}

std::string read_string_table(std::span<const std::uint8_t> bytes,
                              std::size_t offset, std::size_t length) {
    const std::size_t bounded = bounded_string_length(bytes, offset, length);
    std::size_t actual_length = 0;
    while (actual_length < bounded && bytes[offset + actual_length] != 0) {
        ++actual_length;
    }
    return std::string(
        reinterpret_cast<const char*>(bytes.data() + offset), actual_length);
}

std::vector<std::string> read_strings(std::span<const std::uint8_t> bytes,
                                      std::size_t offset,
                                      std::size_t count) {
    if (count == 0) {
        return {};
    }
    if (offset > bytes.size()) {
        throw std::out_of_range("string table is outside the input");
    }
    std::vector<std::string> result;
    result.reserve(count);
    while (result.size() < count) {
        if (offset > bytes.size()) {
            throw std::out_of_range("string table is outside the input");
        }
        const auto begin = offset;
        while (offset < bytes.size() && bytes[offset] != 0) {
            ++offset;
        }
        if (offset == bytes.size()) {
            throw std::out_of_range(
                "string table entry has no NUL terminator");
        }
        std::string value;
        value.reserve(offset - begin);
        for (std::size_t index = begin; index < offset; ++index) {
            value.push_back(ascii_decode(bytes[index]));
        }
        result.push_back(std::move(value));
        ++offset;
    }
    return result;
}

KanjiFontData read_kanji_font(std::span<const std::uint8_t> bytes) {
    if (bytes.size() < 8) {
        throw std::out_of_range("kanji font header is outside the input");
    }

    const auto count = span_read_i32_at(bytes, 0);
    const auto width = span_read_u16_at(bytes, 4);
    const auto height = span_read_u16_at(bytes, 6);
    if (count < 0) {
        throw std::invalid_argument("kanji font has a negative glyph count");
    }
    // ReadKanjiFont's table and output layout are deliberately fixed at 16
    // pixels wide.  The 11-row font is vertically inset by three rows in its
    // 16-row tile; a 16-row font uses the tile as-is.
    if (width != 16 || (height != 11 && height != 16)) {
        throw std::invalid_argument("unsupported kanji font dimensions");
    }

    const auto glyph_count = static_cast<std::size_t>(count);
    const auto rows_per_glyph = static_cast<std::size_t>(height);
    if (glyph_count > (std::numeric_limits<std::size_t>::max() - 8)
                          / (rows_per_glyph * 2)) {
        throw std::length_error("kanji font is too large");
    }
    const auto source_size = 8 + glyph_count * rows_per_glyph * 2;
    if (source_size > bytes.size()) {
        throw std::out_of_range("kanji font glyph data is outside the input");
    }
    if (glyph_count > std::numeric_limits<std::size_t>::max() / 128) {
        throw std::length_error("kanji font output is too large");
    }

    // The managed code writes an int[] with eight 4-bit pixels per word and
    // then serializes each word little-endian.  Building the words first
    // keeps that exact layout independent of the host compiler's integer
    // representation.
    std::vector<std::uint32_t> words(glyph_count * 32, 0);
    for (std::size_t glyph = 0; glyph < glyph_count; ++glyph) {
        const auto glyph_offset = 8 + glyph * rows_per_glyph * 2;
        for (std::size_t y = 0; y < rows_per_glyph; ++y) {
            const auto tile_row = height == 16 ? y : y + 3;
            for (std::size_t x = 0; x < 16; ++x) {
                const auto source = bytes[glyph_offset + y * 2 + x / 8];
                const auto mask = static_cast<std::uint8_t>(
                    1U << (7U - static_cast<unsigned>(x & 7)));
                if ((source & mask) == 0) {
                    continue;
                }
                const auto word_offset = (tile_row & 7) + 16 * (tile_row / 8)
                    + 8 * (x / 8);
                const auto shift = 4 * (x & 7);
                words[glyph * 32 + word_offset]
                    |= static_cast<std::uint32_t>(3U << shift);
            }
        }
    }

    KanjiFontData result;
    result.count = count;
    result.width = width;
    result.height = height;
    result.character_data.resize(words.size() * sizeof(std::uint32_t));
    for (std::size_t index = 0; index < words.size(); ++index) {
        const auto value = words[index];
        const auto output = index * sizeof(std::uint32_t);
        result.character_data[output] = static_cast<std::uint8_t>(value);
        result.character_data[output + 1]
            = static_cast<std::uint8_t>(value >> 8);
        result.character_data[output + 2]
            = static_cast<std::uint8_t>(value >> 16);
        result.character_data[output + 3]
            = static_cast<std::uint8_t>(value >> 24);
    }
    return result;
}

} // namespace fruityprime::read
