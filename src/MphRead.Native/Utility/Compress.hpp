#pragma once

#include "Assets/compression.hpp"

#include <cstdint>
#include <filesystem>
#include <iosfwd>
#include <span>
#include <vector>

namespace fruityprime {

class LZUtil final {
public:
    LZUtil() = delete;

    [[nodiscard]] static int GetOccurrenceLength(
        const std::uint8_t* new_ptr, int new_length,
        const std::uint8_t* old_ptr, int old_length, int& displacement,
        int minimum_displacement = 1);
};

class LZ10 final {
public:
    LZ10() = delete;

    static constexpr std::uint8_t MagicByte = 0x10;

    [[nodiscard]] static std::int64_t Decompress(
        const std::filesystem::path& input,
        const std::filesystem::path& output);
    [[nodiscard]] static std::int64_t Decompress(
        std::istream& input, std::int64_t input_length,
        std::ostream& output);
    [[nodiscard]] static std::vector<std::uint8_t> Decompress(
        std::span<const std::uint8_t> input);

    [[nodiscard]] static int Compress(const std::filesystem::path& input,
                                      const std::filesystem::path& output);
    [[nodiscard]] static int Compress(std::istream& input,
                                      std::int64_t input_length,
                                      std::ostream& output);
    [[nodiscard]] static std::vector<std::uint8_t> Compress(
        std::span<const std::uint8_t> input);
};

class LZBackward final {
public:
    LZBackward() = delete;

    [[nodiscard]] static std::int64_t Decompress(
        const std::filesystem::path& input,
        const std::filesystem::path& output);
    [[nodiscard]] static std::int64_t Decompress(
        std::istream& input, std::int64_t input_length,
        std::ostream& output);
    [[nodiscard]] static std::vector<std::uint8_t> Decompress(
        std::span<const std::uint8_t> input);

    [[nodiscard]] static int Compress(std::istream& input,
                                      std::int64_t input_length,
                                      std::ostream& output);
    [[nodiscard]] static std::vector<std::uint8_t> Compress(
        std::span<const std::uint8_t> input);

    [[nodiscard]] static std::uint32_t ToNDSu32(
        std::span<const std::uint8_t> buffer, std::size_t offset);
};

} // namespace fruityprime

namespace MphReadNative {
using LZUtil = ::fruityprime::LZUtil;
using LZ10 = ::fruityprime::LZ10;
using LZBackward = ::fruityprime::LZBackward;
}
