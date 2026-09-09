#include "Assets/nds_rom.hpp"

#include "Utility/binary_reader.hpp"
#include "Assets/compression.hpp"

#include <algorithm>
#include <fstream>
#include <functional>
#include <limits>
#include <stdexcept>

namespace fruityprime::nds {
namespace {

constexpr std::uint32_t DirectoryIdBase = 0xf000;

[[nodiscard]] std::vector<std::uint8_t> read_all(
    const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary | std::ios::ate);
    if (!input) {
        throw std::runtime_error("could not open ROM " + path.string());
    }
    const std::streamoff length = input.tellg();
    if (length < 0 || static_cast<std::uintmax_t>(length)
        > std::numeric_limits<std::size_t>::max()) {
        throw std::runtime_error("ROM is too large: " + path.string());
    }
    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(length));
    input.seekg(0);
    if (!bytes.empty()) {
        input.read(reinterpret_cast<char*>(bytes.data()),
                   static_cast<std::streamsize>(bytes.size()));
        if (!input) {
            throw std::runtime_error("could not read ROM " + path.string());
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

void validate_relative_component(std::string_view name) {
    if (name.empty() || name == "." || name == ".."
        || name.find('/') != std::string_view::npos
        || name.find('\\') != std::string_view::npos) {
        throw std::runtime_error("ROM contains an unsafe path component");
    }
}

} // namespace

Rom Rom::read_file(const std::filesystem::path& path) {
    return Rom(read_all(path));
}

Rom Rom::from_bytes(std::vector<std::uint8_t> bytes) {
    return Rom(std::move(bytes));
}

Rom::Rom(std::vector<std::uint8_t> bytes)
    : bytes_(std::move(bytes)) {
    parse_header();
}

void Rom::parse_header() {
    if (bytes_.size() < HeaderSize) {
        throw std::runtime_error("ROM is smaller than its header");
    }
    core::BinaryReader reader(bytes_);
    header_.title = reader.read_raw_string(12);
    header_.game_code = reader.read_raw_string(4);
    header_.maker_code = reader.read_raw_string(2);
    header_.unit_code = reader.read_u8();
    header_.seed = reader.read_u8();
    header_.capacity = reader.read_u8();
    reader.skip(7);
    reader.skip(1);
    header_.region = reader.read_u8();
    header_.version = reader.read_u8();
    header_.auto_start = reader.read_u8();
    header_.arm9_offset = reader.read_i32_le();
    header_.arm9_entry_address = reader.read_i32_le();
    header_.arm9_ram_address = reader.read_i32_le();
    header_.arm9_size = reader.read_i32_le();
    header_.arm7_offset = reader.read_i32_le();
    header_.arm7_entry_address = reader.read_i32_le();
    header_.arm7_ram_address = reader.read_i32_le();
    header_.arm7_size = reader.read_i32_le();
    header_.fnt_offset = reader.read_u32_le();
    header_.fnt_size = reader.read_u32_le();
    header_.fat_offset = reader.read_u32_le();
    header_.fat_size = reader.read_u32_le();
    header_.overlay9_offset = reader.read_i32_le();
    header_.overlay9_size = reader.read_i32_le();
    header_.overlay7_offset = reader.read_i32_le();
    header_.overlay7_size = reader.read_i32_le();
    header_.read_flags = reader.read_u32_le();
    header_.init_flags = reader.read_u32_le();
    header_.banner_offset = reader.read_i32_le();

    const auto in_file = [this](std::uint64_t offset, std::uint64_t size) {
        return offset <= bytes_.size() && size <= bytes_.size() - offset;
    };
    if (header_.fnt_size == 0 || header_.fat_size == 0
        || header_.fat_size % 8 != 0
        || !in_file(header_.fnt_offset, header_.fnt_size)
        || !in_file(header_.fat_offset, header_.fat_size)) {
        throw std::runtime_error("ROM file system offsets are invalid");
    }
}

const std::vector<FileEntry>& Rom::files() const {
    parse_files();
    return files_;
}

void Rom::parse_files() const {
    if (files_parsed_) {
        return;
    }
    core::BinaryReader fnt_reader(std::span<const std::uint8_t>(bytes_)
        .subspan(header_.fnt_offset, header_.fnt_size));
    if (fnt_reader.size() < 8) {
        throw std::runtime_error("ROM name table is too small");
    }
    const std::uint16_t directory_count = fnt_reader.peek_bytes(8)[6]
        | static_cast<std::uint16_t>(fnt_reader.peek_bytes(8)[7]) << 8;
    if (directory_count == 0
        || static_cast<std::uint64_t>(directory_count) * 8 > fnt_reader.size()) {
        throw std::runtime_error("ROM directory table is invalid");
    }

    struct Directory {
        std::uint32_t subtable_offset = 0;
        std::uint16_t first_file = 0;
        std::uint16_t directory_number = 0;
    };
    std::vector<Directory> directories;
    directories.reserve(directory_count);
    for (std::uint16_t i = 0; i < directory_count; ++i) {
        core::BinaryReader reader(fnt_reader.peek_bytes(fnt_reader.size()));
        reader.seek(static_cast<std::size_t>(i) * 8);
        directories.push_back(Directory{
            reader.read_u32_le(), reader.read_u16_le(), reader.read_u16_le()
        });
    }

    const std::uint32_t file_count = header_.fat_size / 8;
    const auto fat = std::span<const std::uint8_t>(bytes_)
        .subspan(header_.fat_offset, header_.fat_size);
    std::vector<bool> visited(directory_count, false);
    std::function<void(std::uint16_t, const std::filesystem::path&)> walk;
    walk = [&](std::uint16_t directory_index,
               const std::filesystem::path& parent) {
        if (directory_index >= directories.size()
            || visited[directory_index]) {
            throw std::runtime_error("ROM directory tree is cyclic or invalid");
        }
        visited[directory_index] = true;
        const Directory& directory = directories[directory_index];
        if (directory.subtable_offset >= fnt_reader.size()) {
            throw std::runtime_error("ROM directory subtable is outside FNT");
        }
        std::size_t offset = directory.subtable_offset;
        std::uint32_t file_index = directory.first_file;
        std::vector<std::pair<std::uint16_t, std::string>> subdirectories;
        for (;;) {
            if (offset >= fnt_reader.size()) {
                throw std::runtime_error("ROM directory subtable has no terminator");
            }
            const std::uint8_t type = fnt_reader.peek_bytes(fnt_reader.size())[offset];
            ++offset;
            if (type == 0) {
                break;
            }
            const bool is_directory = (type & 0x80) != 0;
            const std::size_t length = is_directory ? type - 0x80 : type;
            if (length == 0 || length > 127 || offset + length > fnt_reader.size()) {
                throw std::runtime_error("ROM directory name is invalid");
            }
            const auto name_bytes = fnt_reader.peek_bytes(fnt_reader.size())
                .subspan(offset, length);
            std::string name(reinterpret_cast<const char*>(name_bytes.data()), length);
            validate_relative_component(name);
            offset += length;
            if (is_directory) {
                if (offset + 2 > fnt_reader.size()) {
                    throw std::runtime_error("ROM subdirectory id is truncated");
                }
                const std::uint16_t id = static_cast<std::uint16_t>(
                    fnt_reader.peek_bytes(fnt_reader.size())[offset]
                    | static_cast<std::uint16_t>(fnt_reader.peek_bytes(
                        fnt_reader.size())[offset + 1]) << 8);
                offset += 2;
                if (id < DirectoryIdBase) {
                    throw std::runtime_error("ROM subdirectory id is invalid");
                }
                subdirectories.emplace_back(
                    static_cast<std::uint16_t>(id - DirectoryIdBase),
                    std::move(name));
            } else {
                if (file_index >= file_count ||
                    static_cast<std::uint64_t>(file_index) * 8 + 8 > fat.size()) {
                    throw std::runtime_error("ROM file id is outside FAT");
                }
                core::BinaryReader fat_reader(fat);
                fat_reader.seek(static_cast<std::size_t>(file_index) * 8);
                const std::uint32_t start = fat_reader.read_u32_le();
                const std::uint32_t end = fat_reader.read_u32_le();
                if (end < start || end > bytes_.size()) {
                    throw std::runtime_error("ROM file range is invalid");
                }
                const auto path = parent.empty()
                    ? std::filesystem::path(name)
                    : parent / name;
                files_.push_back(FileEntry{path.generic_string(), file_index,
                                            start, end - start});
                ++file_index;
            }
        }
        for (const auto& [child, name] : subdirectories) {
            walk(child, parent / name);
        }
    };

    walk(0, {});
    files_parsed_ = true;
}

std::vector<std::uint8_t> Rom::file(std::uint32_t file_id) const {
    parse_files();
    if (file_id >= header_.fat_size / 8) {
        throw std::out_of_range("ROM file id is out of range");
    }
    core::BinaryReader reader(std::span<const std::uint8_t>(bytes_)
        .subspan(header_.fat_offset, header_.fat_size));
    reader.seek(static_cast<std::size_t>(file_id) * 8);
    const std::uint32_t start = reader.read_u32_le();
    const std::uint32_t end = reader.read_u32_le();
    if (end < start || end > bytes_.size()) {
        throw std::runtime_error("ROM file range is invalid");
    }
    return std::vector<std::uint8_t>(bytes_.begin() + start,
                                     bytes_.begin() + end);
}

std::vector<std::uint8_t> Rom::file(std::string_view path) const {
    const auto found = std::find_if(files().begin(), files().end(),
        [path](const FileEntry& entry) { return entry.path == path; });
    if (found == files().end()) {
        throw std::out_of_range("ROM file path was not found");
    }
    return file(found->file_id);
}

std::vector<std::uint8_t> Rom::raw_range(std::uint32_t offset,
                                         std::uint32_t size) const {
    if (offset > bytes_.size() || size > bytes_.size() - offset) {
        throw std::out_of_range("ROM byte range is outside the image");
    }
    return std::vector<std::uint8_t>(bytes_.begin() + offset,
                                     bytes_.begin() + offset + size);
}

std::vector<std::uint8_t> Rom::arm9() const {
    if (header_.arm9_offset < 0 || header_.arm9_size < 0) {
        throw std::runtime_error("ROM ARM9 range is negative");
    }
    const auto offset = static_cast<std::uint64_t>(header_.arm9_offset);
    const auto size = static_cast<std::uint64_t>(header_.arm9_size);
    if (offset > bytes_.size() || size > bytes_.size() - offset) {
        throw std::runtime_error("ROM ARM9 range is outside the image");
    }
    return compression::lz_backward_decompress(
        std::span<const std::uint8_t>(bytes_).subspan(
            static_cast<std::size_t>(offset), static_cast<std::size_t>(size)));
}

std::vector<std::uint8_t> Rom::overlay9(std::uint32_t overlay_id) const {
    if (header_.overlay9_offset < 0 || header_.overlay9_size < 0
        || header_.overlay9_size % 32 != 0) {
        throw std::runtime_error("ROM ARM9 overlay table is invalid");
    }
    const auto offset = static_cast<std::uint64_t>(header_.overlay9_offset);
    const auto size = static_cast<std::uint64_t>(header_.overlay9_size);
    if (offset > bytes_.size() || size > bytes_.size() - offset) {
        throw std::runtime_error("ROM ARM9 overlay table is outside the image");
    }
    const auto table = std::span<const std::uint8_t>(bytes_).subspan(
        static_cast<std::size_t>(offset), static_cast<std::size_t>(size));
    for (std::size_t record = 0; record < table.size(); record += 32) {
        const std::uint32_t id = static_cast<std::uint32_t>(table[record])
            | static_cast<std::uint32_t>(table[record + 1]) << 8
            | static_cast<std::uint32_t>(table[record + 2]) << 16
            | static_cast<std::uint32_t>(table[record + 3]) << 24;
        if (id != overlay_id) {
            continue;
        }
        const std::uint32_t file_id =
            static_cast<std::uint32_t>(table[record + 24])
            | static_cast<std::uint32_t>(table[record + 25]) << 8
            | static_cast<std::uint32_t>(table[record + 26]) << 16
            | static_cast<std::uint32_t>(table[record + 27]) << 24;
        return compression::lz_backward_decompress(file(file_id));
    }
    throw std::out_of_range("ROM ARM9 overlay ID was not found");
}

std::size_t Rom::extract(const std::filesystem::path& destination) const {
    std::filesystem::create_directories(destination);
    for (const FileEntry& entry : files()) {
        const auto output = destination / std::filesystem::path(entry.path);
        std::filesystem::create_directories(output.parent_path());
        write_all(output, file(entry.file_id));
    }
    return files_.size();
}

} // namespace fruityprime::nds
