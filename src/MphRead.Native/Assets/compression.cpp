#include "Assets/compression.hpp"

#include <algorithm>
#include <limits>
#include <stdexcept>
#include <string>

namespace fruityprime::compression {

std::size_t get_occurrence_length(
    const std::uint8_t* new_ptr, std::size_t new_length,
    const std::uint8_t* old_ptr, std::size_t old_length,
    std::size_t& displacement, std::size_t minimum_displacement) {
    displacement = 0;
    if (new_length == 0 || old_length <= minimum_displacement) {
        return 0;
    }

    std::size_t longest = 0;
    // This intentionally compares against the source span even when the
    // source overlaps the bytes being encoded.  The managed LZ10 writer uses
    // the same overlapping look-ahead, which is required for long runs such
    // as a repeated tile row.
    for (std::size_t index = 0;
         index < old_length - minimum_displacement; ++index) {
        std::size_t length = 0;
        while (length < new_length
               && old_ptr[index + length] == new_ptr[length]) {
            ++length;
        }
        if (length > longest) {
            longest = length;
            displacement = old_length - index;
            if (longest == new_length) {
                break;
            }
        }
    }
    return longest;
}

namespace {

[[nodiscard]] std::uint32_t read_reverse_u24_le(
    std::span<const std::uint8_t> bytes, std::size_t offset) {
    if (offset > bytes.size() || bytes.size() - offset < 3) {
        throw std::runtime_error("compressed stream header is truncated");
    }
    return static_cast<std::uint32_t>(bytes[offset])
        | static_cast<std::uint32_t>(bytes[offset + 1]) << 8
        | static_cast<std::uint32_t>(bytes[offset + 2]) << 16;
}

[[nodiscard]] std::uint32_t read_u24_le(std::span<const std::uint8_t> bytes,
                                        std::size_t offset) {
    if (offset > bytes.size() || bytes.size() - offset < 3) {
        throw std::runtime_error("reverse compressed stream header is truncated");
    }
    return static_cast<std::uint32_t>(bytes[offset])
        | static_cast<std::uint32_t>(bytes[offset + 1]) << 8
        | static_cast<std::uint32_t>(bytes[offset + 2]) << 16;
}

[[nodiscard]] std::uint32_t read_u32_le(std::span<const std::uint8_t> bytes,
                                        std::size_t offset) {
    if (offset > bytes.size() || bytes.size() - offset < 4) {
        throw std::runtime_error("reverse compressed stream trailer is truncated");
    }
    return static_cast<std::uint32_t>(bytes[offset])
        | static_cast<std::uint32_t>(bytes[offset + 1]) << 8
        | static_cast<std::uint32_t>(bytes[offset + 2]) << 16
        | static_cast<std::uint32_t>(bytes[offset + 3]) << 24;
}

void check_output_size(std::uint64_t size, const char* description) {
    if (size > std::numeric_limits<std::size_t>::max()) {
        throw std::runtime_error(std::string(description) + " is too large");
    }
}

} // namespace

std::vector<std::uint8_t> lz10_compress(
    std::span<const std::uint8_t> input) {
    if (input.empty()) {
        throw std::invalid_argument("cannot compress an empty LZ-0x10 stream");
    }
    if (input.size() > 0xFFFFFFU) {
        throw std::invalid_argument("input is too large for an LZ-0x10 stream");
    }

    std::vector<std::uint8_t> output;
    output.reserve(input.size() + input.size() / 8 + 4);
    output.push_back(0x10);
    output.push_back(static_cast<std::uint8_t>(input.size()));
    output.push_back(static_cast<std::uint8_t>(input.size() >> 8));
    output.push_back(static_cast<std::uint8_t>(input.size() >> 16));

    std::size_t read = 0;
    while (read < input.size()) {
        const std::size_t flag_offset = output.size();
        output.push_back(0);
        std::uint8_t flags = 0;
        for (int block = 0; block < 8 && read < input.size(); ++block) {
            const std::size_t old_length = std::min<std::size_t>(read, 0x1000);
            const std::size_t old_offset = read - old_length;
            const std::size_t new_length = std::min<std::size_t>(
                input.size() - read, 0x12);
            std::size_t displacement = 0;
            const std::size_t length = get_occurrence_length(
                input.data() + read, new_length, input.data() + old_offset,
                old_length, displacement);
            if (length < 3) {
                output.push_back(input[read++]);
                continue;
            }

            flags = static_cast<std::uint8_t>(flags | (1U << (7 - block)));
            output.push_back(static_cast<std::uint8_t>(
                ((length - 3) << 4) | ((displacement - 1) >> 8)));
            output.push_back(static_cast<std::uint8_t>(
                (displacement - 1) & 0xff));
            read += length;
        }
        output[flag_offset] = flags;
    }
    return output;
}

std::vector<std::uint8_t> lz10_decompress(
    std::span<const std::uint8_t> input) {
    if (input.size() < 4 || input[0] != 0x10) {
        throw std::runtime_error("input is not an LZ-0x10 stream");
    }

    std::uint64_t expected_size = read_u24_le(input, 1);
    std::size_t cursor = 4;
    if (expected_size == 0) {
        if (input.size() - cursor < 4) {
            throw std::runtime_error("extended LZ-0x10 size is truncated");
        }
        expected_size = static_cast<std::uint32_t>(input[cursor])
            | static_cast<std::uint32_t>(input[cursor + 1]) << 8
            | static_cast<std::uint32_t>(input[cursor + 2]) << 16
            | static_cast<std::uint32_t>(input[cursor + 3]) << 24;
        cursor += 4;
    }
    check_output_size(expected_size, "decoded LZ-0x10 stream");
    if (expected_size == 0) {
        throw std::runtime_error("LZ-0x10 stream has an empty output");
    }

    std::vector<std::uint8_t> output;
    output.reserve(static_cast<std::size_t>(expected_size));
    std::uint8_t flags = 0;
    std::uint8_t mask = 1;
    while (output.size() < expected_size) {
        if (mask == 1) {
            if (cursor >= input.size()) {
                throw std::runtime_error("LZ-0x10 flags are truncated");
            }
            flags = input[cursor++];
            mask = 0x80;
        } else {
            mask >>= 1;
        }

        if ((flags & mask) != 0) {
            if (input.size() - cursor < 2) {
                throw std::runtime_error("LZ-0x10 back reference is truncated");
            }
            const std::uint8_t first = input[cursor++];
            const std::uint8_t second = input[cursor++];
            const std::size_t length = static_cast<std::size_t>(first >> 4) + 3;
            const std::size_t displacement =
                (static_cast<std::size_t>(first & 0x0f) << 8)
                | second;
            const std::size_t distance = displacement + 1;
            if (distance > output.size()) {
                throw std::runtime_error(
                    "LZ-0x10 back reference exceeds decoded output (distance="
                    + std::to_string(distance) + ", output="
                    + std::to_string(output.size()) + ", cursor="
                    + std::to_string(cursor) + ")");
            }
            if (length > static_cast<std::size_t>(expected_size) - output.size()) {
                throw std::runtime_error(
                    "LZ-0x10 block exceeds declared output size");
            }
            for (std::size_t i = 0; i < length; ++i) {
                output.push_back(output[output.size() - distance]);
            }
        } else {
            if (cursor >= input.size()) {
                throw std::runtime_error("LZ-0x10 literal is truncated");
            }
            output.push_back(input[cursor++]);
        }
    }

    // ROM resources are commonly four-byte aligned. Permit only the padding
    // needed to reach that boundary; silently accepting arbitrary trailing
    // bytes would hide a bad resource length.
    if (cursor < input.size()) {
        const std::size_t aligned = cursor - (cursor % 4);
        if (input.size() > aligned + 4) {
            throw std::runtime_error("LZ-0x10 stream has unexpected trailing data");
        }
    }
    return output;
}

std::vector<std::uint8_t> lz_backward_decompress(
    std::span<const std::uint8_t> input) {
    if (input.size() < 4) {
        throw std::runtime_error("reverse compressed stream is too short");
    }

    const std::uint32_t extra_size = read_u32_le(input, input.size() - 4);
    if (extra_size == 0) {
        return std::vector<std::uint8_t>(input.begin(), input.end() - 4);
    }
    if (input.size() < 9) {
        throw std::runtime_error("reverse compressed stream header is truncated");
    }

    const std::size_t header_offset = input.size() - 5;
    const std::size_t header_size = input[header_offset];
    if (header_size < 8 || header_size > input.size() - 4) {
        throw std::runtime_error("reverse compressed stream header size is invalid");
    }
    const std::uint32_t declared_compressed_size = read_reverse_u24_le(
        input, input.size() - 8);
    std::size_t compressed_size = 0;
    if (declared_compressed_size >= input.size()) {
        // Match LZBackward.Decompress: the fallback length includes the
        // trailer area because the declared value is compared before the
        // header is subtracted.
        compressed_size = input.size() - header_size;
    } else {
        if (declared_compressed_size < header_size) {
            throw std::runtime_error(
                "reverse compressed stream length is smaller than its header");
        }
        compressed_size = declared_compressed_size - header_size;
    }
    if (compressed_size == 0 || compressed_size > input.size() - header_size) {
        throw std::runtime_error("reverse compressed stream length is invalid");
    }

    // The managed implementation computes this without subtracting the
    // four-byte extra-size trailer a second time.  The declared compressed
    // length already describes the span up to the reverse header.
    const std::size_t prefix_size = input.size() - header_size - compressed_size;
    const std::uint64_t decoded_size64 = static_cast<std::uint64_t>(compressed_size)
        + header_size + extra_size;
    check_output_size(decoded_size64, "decoded reverse stream");
    const std::size_t decoded_size = static_cast<std::size_t>(decoded_size64);

    std::vector<std::uint8_t> output;
    output.reserve(prefix_size + decoded_size);
    output.insert(output.end(), input.begin(), input.begin() + prefix_size);
    std::vector<std::uint8_t> decoded(decoded_size, 0);
    const auto compressed = input.subspan(prefix_size, compressed_size);

    std::size_t read_bytes = 0;
    std::size_t current_output = 0;
    std::uint8_t flags = 0;
    std::uint8_t mask = 1;
    while (current_output < decoded.size()) {
        if (mask == 1) {
            if (read_bytes >= compressed.size()) {
                throw std::runtime_error(
                    "reverse compressed stream flags are truncated");
            }
            flags = compressed[compressed.size() - 1 - read_bytes++];
            mask = 0x80;
        } else {
            mask >>= 1;
        }

        if ((flags & mask) != 0) {
            if (compressed.size() - read_bytes < 2) {
                throw std::runtime_error(
                    "reverse compressed stream back reference is truncated");
            }
            const std::uint8_t first =
                compressed[compressed.size() - 1 - read_bytes++];
            const std::uint8_t second =
                compressed[compressed.size() - 1 - read_bytes++];
            const std::size_t length = static_cast<std::size_t>(first >> 4) + 3;
            std::size_t displacement =
                (static_cast<std::size_t>(first & 0x0f) << 8) | second;
            displacement += 3;
            if (displacement > current_output) {
                if (current_output < 2) {
                    throw std::runtime_error(
                        "reverse compressed stream back reference is invalid");
                }
                // This is the compatibility fallback used by the managed
                // extractor for the format's special displacement value.
                displacement = 2;
            }
            const std::size_t writable = decoded.size() - current_output;
            const std::size_t actual_length = std::min(length, writable);
            std::size_t source_offset = current_output - displacement;
            for (std::size_t i = 0; i < actual_length; ++i) {
                const std::size_t source = decoded.size() - 1 - source_offset++;
                decoded[decoded.size() - 1 - current_output++] = decoded[source];
            }
        } else {
            if (read_bytes >= compressed.size()) {
                throw std::runtime_error(
                    "reverse compressed stream literal is truncated");
            }
            decoded[decoded.size() - 1 - current_output++] =
                compressed[compressed.size() - 1 - read_bytes++];
        }
    }

    output.insert(output.end(), decoded.begin(), decoded.end());
    return output;
}

std::vector<std::uint8_t> lz_backward_compress(
    std::span<const std::uint8_t> input) {
    if (input.size() > 0xFFFFFFU) {
        throw std::invalid_argument(
            "input is too large for an LZ-overlay stream");
    }
    if (input.empty()) {
        // CompressNormal in the managed implementation takes the address of
        // the first byte of its input buffer, so an empty input is invalid for
        // that path as well.
        throw std::invalid_argument(
            "cannot compress an empty LZ-overlay stream");
    }

    // LZBackward.Compress reverses the input before calling CompressNormal.
    const std::vector<std::uint8_t> reversed(input.rbegin(), input.rend());
    std::vector<std::uint8_t> compressed;
    compressed.reserve(reversed.size() + reversed.size() / 8 + 1);

    // This is CompressNormal translated directly. In particular, disp 1 and
    // disp 2 are deliberately emitted as literals because neither value can
    // be represented by the overlay displacement (disp - 3).
    std::array<std::uint8_t, 17> block{};
    std::size_t buffer_length = 1;
    int buffered_blocks = 0;
    std::size_t read = 0;
    while (read < reversed.size()) {
        if (buffered_blocks == 8) {
            compressed.insert(compressed.end(), block.begin(),
                              block.begin() + buffer_length);
            block[0] = 0;
            buffer_length = 1;
            buffered_blocks = 0;
        }

        const std::size_t old_length = std::min<std::size_t>(read, 0x1001);
        const std::size_t new_length = std::min<std::size_t>(
            reversed.size() - read, 0x12);
        std::size_t displacement = 0;
        std::size_t length = get_occurrence_length(
            reversed.data() + read, new_length,
            reversed.data() + read - old_length, old_length, displacement);
        if (displacement == 1 || displacement == 2) {
            length = 1;
        }

        if (length < 3) {
            block[buffer_length++] = reversed[read++];
        } else {
            read += length;
            block[0] = static_cast<std::uint8_t>(
                block[0] | (1U << (7 - buffered_blocks)));
            block[buffer_length++] = static_cast<std::uint8_t>(
                ((length - 3) << 4) | ((displacement - 3) >> 8));
            block[buffer_length++] = static_cast<std::uint8_t>(
                (displacement - 3) & 0xFF);
        }
        ++buffered_blocks;
    }
    if (buffered_blocks > 0) {
        compressed.insert(compressed.end(), block.begin(),
                          block.begin() + buffer_length);
    }

    const std::size_t compressed_length = compressed.size();
    std::size_t total_file_length = compressed_length + 8;
    if ((total_file_length & 3U) != 0) {
        total_file_length += 4 - (total_file_length & 3U);
    }

    std::vector<std::uint8_t> output;
    if (total_file_length < input.size()) {
        output.reserve(total_file_length);
        output.insert(output.end(), compressed.rbegin(), compressed.rend());
        while ((output.size() & 3U) != 0) {
            output.push_back(0xFF);
        }

        output.push_back(static_cast<std::uint8_t>(compressed_length));
        output.push_back(static_cast<std::uint8_t>(compressed_length >> 8));
        output.push_back(static_cast<std::uint8_t>(compressed_length >> 16));

        const std::size_t header_length =
            total_file_length - compressed.size();
        output.push_back(static_cast<std::uint8_t>(header_length));

        const std::size_t extra_size = input.size() - total_file_length;
        output.push_back(static_cast<std::uint8_t>(extra_size));
        output.push_back(static_cast<std::uint8_t>(extra_size >> 8));
        output.push_back(static_cast<std::uint8_t>(extra_size >> 16));
        output.push_back(static_cast<std::uint8_t>(extra_size >> 24));
    } else {
        // The managed fallback restores the original order and appends a zero
        // extra-size trailer, which makes the data an uncompressed overlay.
        output.assign(input.begin(), input.end());
        output.insert(output.end(), 4, 0);
    }
    return output;
}

} // namespace fruityprime::compression
