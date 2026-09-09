#include "Utility/archive.hpp"

#include "Utility/binary_reader.hpp"
#include "Assets/compression.hpp"
#include "Read.hpp"

#include <algorithm>
#include <fstream>
#include <limits>
#include <stdexcept>

namespace fruityprime::archive {
namespace {

[[nodiscard]] std::uint32_t read_be32(std::span<const std::uint8_t> bytes,
                                      std::size_t offset) {
    return static_cast<std::uint32_t>(bytes[offset]) << 24
        | static_cast<std::uint32_t>(bytes[offset + 1]) << 16
        | static_cast<std::uint32_t>(bytes[offset + 2]) << 8
        | static_cast<std::uint32_t>(bytes[offset + 3]);
}

void write_be32(std::span<std::uint8_t> bytes, std::size_t offset,
                std::uint32_t value) {
    bytes[offset] = static_cast<std::uint8_t>(value >> 24);
    bytes[offset + 1] = static_cast<std::uint8_t>(value >> 16);
    bytes[offset + 2] = static_cast<std::uint8_t>(value >> 8);
    bytes[offset + 3] = static_cast<std::uint8_t>(value);
}

[[nodiscard]] std::uint32_t aligned_size(std::uint32_t value) {
    if (value <= 32) {
        return value;
    }
    const std::uint32_t remainder = value % 32;
    return remainder == 0 ? value : value + (32 - remainder);
}

[[nodiscard]] std::string fixed_name(std::span<const std::uint8_t> bytes) {
    std::size_t length = 0;
    while (length < bytes.size() && bytes[length] != 0) {
        ++length;
    }
    return std::string(reinterpret_cast<const char*>(bytes.data()), length);
}

void validate_filename(std::string_view filename) {
    if (filename.empty() || filename == "." || filename == ".."
        || filename.find('/') != std::string_view::npos
        || filename.find('\\') != std::string_view::npos) {
        throw std::runtime_error("archive contains an unsafe filename");
    }
}

[[nodiscard]] std::vector<std::uint8_t> read_all(
    const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary | std::ios::ate);
    if (!input) {
        throw std::runtime_error("could not open " + path.string());
    }
    const std::streamoff length = input.tellg();
    if (length < 0 || static_cast<std::uintmax_t>(length)
        > std::numeric_limits<std::size_t>::max()) {
        throw std::runtime_error("file is too large: " + path.string());
    }
    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(length));
    input.seekg(0);
    if (!bytes.empty()) {
        input.read(reinterpret_cast<char*>(bytes.data()),
                   static_cast<std::streamsize>(bytes.size()));
        if (!input) {
            throw std::runtime_error("could not read " + path.string());
        }
    }
    return bytes;
}

void write_all(const std::filesystem::path& path,
               std::span<const std::uint8_t> bytes) {
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    if (!output) {
        throw std::runtime_error("could not create " + path.string());
    }
    if (!bytes.empty()) {
        output.write(reinterpret_cast<const char*>(bytes.data()),
                     static_cast<std::streamsize>(bytes.size()));
    }
    if (!output) {
        throw std::runtime_error("could not write " + path.string());
    }
}

} // namespace

Archive Archive::parse(std::span<const std::uint8_t> bytes) {
    if (!bytes.empty() && bytes[0] == 0x10) {
        const auto decoded = compression::lz10_decompress(bytes);
        return parse(decoded);
    }
    if (bytes.size() < HeaderSize) {
        throw std::runtime_error("could not read archive header");
    }
    const std::string magic(reinterpret_cast<const char*>(bytes.data()), 7);
    const std::uint32_t count = read_be32(bytes, 8);
    const std::uint32_t total_size = read_be32(bytes, 12);
    if (magic != Magic || bytes[7] != 0 || total_size != bytes.size()) {
        throw std::runtime_error("could not read archive");
    }
    const std::uint64_t table_end = static_cast<std::uint64_t>(HeaderSize)
        + static_cast<std::uint64_t>(EntrySize) * count;
    if (count == 0 || table_end > bytes.size()) {
        throw std::runtime_error("archive file table is invalid");
    }

    Archive result;
    result.bytes_.assign(bytes.begin(), bytes.end());
    result.entries_.reserve(count);
    std::uint64_t pointer = table_end;
    for (std::uint32_t i = 0; i < count; ++i) {
        const std::size_t offset = HeaderSize
            + static_cast<std::size_t>(i) * EntrySize;
        Entry entry;
        entry.filename = fixed_name(bytes.subspan(offset, 32));
        entry.offset = read_be32(bytes, offset + 32);
        entry.padded_size = read_be32(bytes, offset + 36);
        entry.target_size = read_be32(bytes, offset + 40);
        validate_filename(entry.filename);
        if (entry.padded_size == 0 || entry.target_size == 0
            || entry.padded_size < entry.target_size
            || entry.padded_size != aligned_size(entry.target_size)
            || entry.offset < HeaderSize
            || static_cast<std::uint64_t>(entry.offset) != pointer
            || static_cast<std::uint64_t>(entry.offset) + entry.padded_size
                > bytes.size()) {
            throw std::runtime_error("archive file entry is invalid");
        }
        pointer += entry.padded_size;
        result.entries_.push_back(std::move(entry));
    }
    if (pointer != bytes.size()) {
        throw std::runtime_error("archive does not end at its final file");
    }
    return result;
}

Archive Archive::read_file(const std::filesystem::path& path) {
    const auto bytes = read_all(path);
    return parse(bytes);
}

std::vector<std::uint8_t> Archive::file(std::size_t index) const {
    if (index >= entries_.size()) {
        throw std::out_of_range("archive file index is out of range");
    }
    const Entry& entry = entries_[index];
    return std::vector<std::uint8_t>(
        bytes_.begin() + entry.offset,
        bytes_.begin() + entry.offset + entry.target_size);
}

std::size_t Archive::extract(const std::filesystem::path& destination) const {
    std::filesystem::create_directories(destination);
    for (std::size_t i = 0; i < entries_.size(); ++i) {
        const Entry& entry = entries_[i];
        const auto output = destination / entry.filename;
        write_all(output, file(i));
    }
    return entries_.size();
}

void Archive::create(const std::filesystem::path& destination,
                     const std::vector<std::filesystem::path>& files) {
    if (files.empty()) {
        throw std::runtime_error("could not write an empty archive");
    }
    const std::uint64_t table_end = static_cast<std::uint64_t>(HeaderSize)
        + static_cast<std::uint64_t>(EntrySize) * files.size();
    if (table_end > std::numeric_limits<std::uint32_t>::max()) {
        throw std::runtime_error("archive is too large");
    }

    std::vector<std::vector<std::uint8_t>> contents;
    contents.reserve(files.size());
    std::vector<Entry> entries;
    entries.reserve(files.size());
    std::uint64_t pointer = table_end;
    for (const auto& path : files) {
        const std::string filename = path.filename().string();
        if (filename.empty() || filename.size() > 32) {
            throw std::runtime_error("archive filename must be 1-32 bytes");
        }
        validate_filename(filename);
        contents.push_back(read_all(path));
        const std::uint32_t target_size = static_cast<std::uint32_t>(
            contents.back().size());
        if (contents.back().size() > std::numeric_limits<std::uint32_t>::max()
            || target_size == 0) {
            throw std::runtime_error("archive files must be non-empty");
        }
        const std::uint32_t padded_size = aligned_size(target_size);
        if (pointer + padded_size > std::numeric_limits<std::uint32_t>::max()) {
            throw std::runtime_error("archive is too large");
        }
        entries.push_back(Entry{filename, static_cast<std::uint32_t>(pointer),
                                padded_size, target_size});
        pointer += padded_size;
    }

    std::vector<std::uint8_t> output(static_cast<std::size_t>(pointer), 0);
    std::copy(Magic.begin(), Magic.end(), output.begin());
    write_be32(output, 8, static_cast<std::uint32_t>(files.size()));
    write_be32(output, 12, static_cast<std::uint32_t>(pointer));
    for (std::size_t i = 0; i < entries.size(); ++i) {
        const std::size_t offset = HeaderSize + i * EntrySize;
        const Entry& entry = entries[i];
        std::copy(entry.filename.begin(), entry.filename.end(),
                  output.begin() + static_cast<std::ptrdiff_t>(offset));
        write_be32(output, offset + 32, entry.offset);
        write_be32(output, offset + 36, entry.padded_size);
        write_be32(output, offset + 40, entry.target_size);
        std::copy(contents[i].begin(), contents[i].end(),
                  output.begin() + entry.offset);
    }
    write_all(destination, output);
}

} // namespace fruityprime::archive

namespace fruityprime::read {

ArchiveExtractionResult extract_archive(
    const std::filesystem::path& path,
    const std::filesystem::path& output_directory) {
    const auto output = output_directory.empty()
        ? std::filesystem::absolute(path.parent_path() / ".." / "_archives"
                                    / path.stem())
              .lexically_normal()
        : output_directory;
    std::filesystem::create_directories(output);
    const auto archive = fruityprime::archive::Archive::read_file(path);
    return ArchiveExtractionResult{output, archive.extract(output)};
}

} // namespace fruityprime::read
