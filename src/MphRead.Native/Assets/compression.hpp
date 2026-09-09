#pragma once

#include <cstdint>
#include <span>
#include <vector>

namespace fruityprime::compression {

// LZUtil.GetOccurrenceLength. old_ptr must refer to the bytes immediately
// preceding new_ptr when overlapping matches are desired, just like the
// pointer-based managed API.
[[nodiscard]] std::size_t get_occurrence_length(
    const std::uint8_t* new_ptr, std::size_t new_length,
    const std::uint8_t* old_ptr, std::size_t old_length,
    std::size_t& displacement, std::size_t minimum_displacement = 1);

// Nintendo's LZ-0x10 stream used by compressed archives and several ROM
// resources. Both functions return an owned, fully decoded byte sequence and
// reject truncated or out-of-range back references.
[[nodiscard]] std::vector<std::uint8_t> lz10_compress(
    std::span<const std::uint8_t> input);

[[nodiscard]] std::vector<std::uint8_t> lz10_decompress(
    std::span<const std::uint8_t> input);

// Reverse LZ used by the ARM9/overlay extraction path. The four-byte trailer
// is not part of the decoded output, matching the managed extractor.
[[nodiscard]] std::vector<std::uint8_t> lz_backward_decompress(
    std::span<const std::uint8_t> input);

// LZBackward.Compress. The managed writer reverses the input, emits the
// forward LZ stream, reverses that stream again, and stores the overlay
// header/trailer at the end.
[[nodiscard]] std::vector<std::uint8_t> lz_backward_compress(
    std::span<const std::uint8_t> input);

} // namespace fruityprime::compression
