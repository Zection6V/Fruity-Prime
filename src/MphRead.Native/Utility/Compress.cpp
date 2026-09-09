#include "Utility/Compress.hpp"

#include <fstream>
#include <limits>
#include <stdexcept>

namespace fruityprime {
namespace {

[[nodiscard]] std::vector<std::uint8_t> read_exact(
    std::istream& input, std::int64_t input_length) {
    if (input_length < 0
        || static_cast<std::uint64_t>(input_length)
               > std::numeric_limits<std::size_t>::max()) {
        throw std::invalid_argument("input length is invalid");
    }
    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(input_length));
    if (!bytes.empty()) {
        input.read(reinterpret_cast<char*>(bytes.data()),
                   static_cast<std::streamsize>(bytes.size()));
        if (input.gcount() != static_cast<std::streamsize>(bytes.size())) {
            throw std::runtime_error("Stream too short");
        }
    }
    return bytes;
}

void write_all(std::ostream& output, std::span<const std::uint8_t> bytes) {
    if (!bytes.empty()) {
        output.write(reinterpret_cast<const char*>(bytes.data()),
                     static_cast<std::streamsize>(bytes.size()));
    }
    if (!output) {
        throw std::runtime_error("Unable to write output stream");
    }
}

[[nodiscard]] std::int64_t file_length(std::ifstream& input) {
    input.seekg(0, std::ios::end);
    const std::streampos end = input.tellg();
    if (end < 0) {
        throw std::runtime_error("Unable to determine input length");
    }
    input.seekg(0, std::ios::beg);
    return static_cast<std::int64_t>(end);
}

} // namespace

int LZUtil::GetOccurrenceLength(
    const std::uint8_t* new_ptr, int new_length,
    const std::uint8_t* old_ptr, int old_length, int& displacement,
    int minimum_displacement) {
    if (new_length < 0 || old_length < 0 || minimum_displacement < 0) {
        throw std::invalid_argument("LZ occurrence length is invalid");
    }
    std::size_t native_displacement = 0;
    const std::size_t length = compression::get_occurrence_length(
        new_ptr, static_cast<std::size_t>(new_length), old_ptr,
        static_cast<std::size_t>(old_length), native_displacement,
        static_cast<std::size_t>(minimum_displacement));
    displacement = static_cast<int>(native_displacement);
    return static_cast<int>(length);
}

std::vector<std::uint8_t> LZ10::Decompress(
    std::span<const std::uint8_t> input) {
    return compression::lz10_decompress(input);
}

std::int64_t LZ10::Decompress(std::istream& input,
                              std::int64_t input_length,
                              std::ostream& output) {
    const auto decoded = Decompress(read_exact(input, input_length));
    write_all(output, decoded);
    return static_cast<std::int64_t>(decoded.size());
}

std::int64_t LZ10::Decompress(const std::filesystem::path& input_path,
                              const std::filesystem::path& output_path) {
    std::ifstream input(input_path, std::ios::binary);
    if (!input) {
        throw std::runtime_error("Unable to open input file");
    }
    const std::int64_t length = file_length(input);
    std::ofstream output(output_path, std::ios::binary | std::ios::trunc);
    if (!output) {
        throw std::runtime_error("Unable to create output file");
    }
    return Decompress(input, length, output);
}

std::vector<std::uint8_t> LZ10::Compress(
    std::span<const std::uint8_t> input) {
    return compression::lz10_compress(input);
}

int LZ10::Compress(std::istream& input, std::int64_t input_length,
                   std::ostream& output) {
    const auto encoded = Compress(read_exact(input, input_length));
    write_all(output, encoded);
    return static_cast<int>(encoded.size());
}

int LZ10::Compress(const std::filesystem::path& input_path,
                   const std::filesystem::path& output_path) {
    std::ifstream input(input_path, std::ios::binary);
    if (!input) {
        throw std::runtime_error("Unable to open input file");
    }
    const std::int64_t length = file_length(input);
    std::ofstream output(output_path, std::ios::binary | std::ios::trunc);
    if (!output) {
        throw std::runtime_error("Unable to create output file");
    }
    return Compress(input, length, output);
}

std::vector<std::uint8_t> LZBackward::Decompress(
    std::span<const std::uint8_t> input) {
    return compression::lz_backward_decompress(input);
}

std::int64_t LZBackward::Decompress(std::istream& input,
                                    std::int64_t input_length,
                                    std::ostream& output) {
    const auto decoded = Decompress(read_exact(input, input_length));
    write_all(output, decoded);
    return static_cast<std::int64_t>(decoded.size());
}

std::int64_t LZBackward::Decompress(
    const std::filesystem::path& input_path,
    const std::filesystem::path& output_path) {
    std::ifstream input(input_path, std::ios::binary);
    if (!input) {
        throw std::runtime_error("Unable to open input file");
    }
    const std::int64_t length = file_length(input);
    std::ofstream output(output_path, std::ios::binary | std::ios::trunc);
    if (!output) {
        throw std::runtime_error("Unable to create output file");
    }
    return Decompress(input, length, output);
}

std::vector<std::uint8_t> LZBackward::Compress(
    std::span<const std::uint8_t> input) {
    return compression::lz_backward_compress(input);
}

int LZBackward::Compress(std::istream& input, std::int64_t input_length,
                         std::ostream& output) {
    const auto encoded = Compress(read_exact(input, input_length));
    write_all(output, encoded);
    return static_cast<int>(encoded.size());
}

std::uint32_t LZBackward::ToNDSu32(
    std::span<const std::uint8_t> buffer, std::size_t offset) {
    if (offset > buffer.size() || buffer.size() - offset < 4) {
        throw std::out_of_range("ToNDSu32 requires four bytes");
    }
    return static_cast<std::uint32_t>(buffer[offset])
        | static_cast<std::uint32_t>(buffer[offset + 1]) << 8
        | static_cast<std::uint32_t>(buffer[offset + 2]) << 16
        | static_cast<std::uint32_t>(buffer[offset + 3]) << 24;
}

} // namespace fruityprime
