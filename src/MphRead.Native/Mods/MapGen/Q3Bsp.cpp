#include "q3_bsp.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <limits>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <zlib.h>

namespace fruityprime::mapgen {
namespace {

[[nodiscard]] unsigned char fold_ascii(unsigned char value) noexcept {
    if (value >= static_cast<unsigned char>('A')
        && value <= static_cast<unsigned char>('Z')) {
        return static_cast<unsigned char>(value + ('a' - 'A'));
    }
    return value;
}

[[nodiscard]] std::uint32_t read_u32(std::span<const std::uint8_t> bytes,
                                     std::size_t offset) {
    if (offset > bytes.size() || bytes.size() - offset < 4) {
        throw std::runtime_error("Q3 BSP read exceeds the input");
    }
    return static_cast<std::uint32_t>(bytes[offset])
        | static_cast<std::uint32_t>(bytes[offset + 1]) << 8
        | static_cast<std::uint32_t>(bytes[offset + 2]) << 16
        | static_cast<std::uint32_t>(bytes[offset + 3]) << 24;
}

[[nodiscard]] std::uint16_t read_u16(std::span<const std::uint8_t> bytes,
                                     std::size_t offset) {
    if (offset > bytes.size() || bytes.size() - offset < 2) {
        throw std::runtime_error("Q3 ZIP read exceeds the input");
    }
    return static_cast<std::uint16_t>(bytes[offset])
        | static_cast<std::uint16_t>(bytes[offset + 1]) << 8;
}

[[nodiscard]] std::int32_t read_i32(std::span<const std::uint8_t> bytes,
                                    std::size_t offset) {
    return static_cast<std::int32_t>(read_u32(bytes, offset));
}

[[nodiscard]] float read_f32(std::span<const std::uint8_t> bytes,
                             std::size_t offset) {
    const std::uint32_t raw = read_u32(bytes, offset);
    float value = 0.0F;
    static_assert(sizeof(value) == sizeof(raw));
    std::memcpy(&value, &raw, sizeof(value));
    return value;
}

[[nodiscard]] std::size_t checked_range(std::span<const std::uint8_t> bytes,
                                        std::int32_t offset,
                                        std::int32_t length) {
    if (offset < 0 || length < 0
        || static_cast<std::uint64_t>(offset)
               + static_cast<std::uint64_t>(length)
            > bytes.size()) {
        throw std::runtime_error("Q3 BSP lump is outside the input");
    }
    return static_cast<std::size_t>(offset);
}

[[nodiscard]] std::string ascii_string(std::span<const std::uint8_t> bytes,
                                        std::size_t offset,
                                        std::size_t length) {
    std::string result;
    result.reserve(length);
    for (const std::uint8_t value : bytes.subspan(offset, length)) {
        result.push_back(value <= 0x7f
                             ? static_cast<char>(value)
                             : '?');
    }
    return result;
}

[[nodiscard]] std::string fixed_ascii(std::span<const std::uint8_t> bytes,
                                      std::size_t offset, std::size_t length) {
    std::string result = ascii_string(bytes, offset, length);
    while (!result.empty() && result.back() == '\0') {
        result.pop_back();
    }
    return result;
}

[[nodiscard]] std::string lower_ascii(std::string_view value) {
    std::string result;
    result.reserve(value.size());
    for (const unsigned char character : value) {
        result.push_back(static_cast<char>(fold_ascii(character)));
    }
    return result;
}

[[nodiscard]] bool ends_with_bsp(std::string_view value) {
    constexpr std::string_view extension = ".bsp";
    return value.size() >= extension.size()
        && lower_ascii(value.substr(value.size() - extension.size()))
            == extension;
}

[[nodiscard]] std::string entry_stem(std::string_view name) {
    return std::filesystem::path(std::string(name)).stem().string();
}

[[nodiscard]] std::vector<std::uint8_t> read_file(
    const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary | std::ios::ate);
    if (!input) {
        throw std::runtime_error("could not open Q3 source " + path.string());
    }
    const std::streamoff length = input.tellg();
    if (length < 0 || static_cast<std::uintmax_t>(length)
        > std::numeric_limits<std::size_t>::max()) {
        throw std::runtime_error("Q3 source is too large: " + path.string());
    }
    std::vector<std::uint8_t> result(static_cast<std::size_t>(length));
    input.seekg(0);
    if (!result.empty()) {
        input.read(reinterpret_cast<char*>(result.data()),
                   static_cast<std::streamsize>(result.size()));
        if (!input) {
            throw std::runtime_error("could not read Q3 source "
                                     + path.string());
        }
    }
    return result;
}

struct ZipEntry {
    std::string name;
    std::uint16_t method = 0;
    std::uint32_t compressed_size = 0;
    std::uint32_t uncompressed_size = 0;
    std::uint32_t crc = 0;
    std::uint32_t local_offset = 0;
};

[[nodiscard]] std::vector<std::uint8_t> inflate_raw(
    std::span<const std::uint8_t> compressed, std::size_t output_size) {
    if (output_size > std::numeric_limits<uInt>::max()) {
        throw std::runtime_error("Q3 ZIP entry is too large for zlib");
    }
    z_stream stream{};
    stream.next_in = const_cast<Bytef*>(
        reinterpret_cast<const Bytef*>(compressed.data()));
    stream.avail_in = static_cast<uInt>(compressed.size());
    if (inflateInit2(&stream, -MAX_WBITS) != Z_OK) {
        throw std::runtime_error("could not initialize ZIP deflate decoder");
    }
    std::vector<std::uint8_t> output(std::max<std::size_t>(output_size, 1));
    stream.next_out = reinterpret_cast<Bytef*>(output.data());
    stream.avail_out = static_cast<uInt>(output.size());
    const int status = inflate(&stream, Z_FINISH);
    const int end_status = inflateEnd(&stream);
    if (status != Z_STREAM_END || end_status != Z_OK
        || stream.total_out != output_size
        || stream.total_in != compressed.size()) {
        throw std::runtime_error("invalid or truncated ZIP deflate stream");
    }
    output.resize(output_size);
    return output;
}

[[nodiscard]] std::vector<ZipEntry> read_zip_entries(
    std::span<const std::uint8_t> bytes) {
    constexpr std::uint32_t end_signature = 0x06054b50U;
    constexpr std::uint32_t central_signature = 0x02014b50U;
    const std::size_t search_start = bytes.size() > 65'557
        ? bytes.size() - 65'557 : 0;
    std::optional<std::size_t> end_offset;
    if (bytes.size() >= 4) {
        for (std::size_t offset = bytes.size() - 4;;) {
            if (read_u32(bytes, offset) == end_signature) {
                end_offset = offset;
                break;
            }
            if (offset == search_start || offset == 0) {
                break;
            }
            --offset;
        }
    }
    if (!end_offset.has_value()) {
        throw std::runtime_error("Q3 source is not a ZIP/PK3 archive");
    }
    const std::size_t end = *end_offset;
    const std::uint16_t disk = read_u16(bytes, end + 4);
    const std::uint16_t central_disk = read_u16(bytes, end + 6);
    const std::uint16_t disk_count = read_u16(bytes, end + 8);
    const std::uint16_t total_count = read_u16(bytes, end + 10);
    const std::uint32_t central_size = read_u32(bytes, end + 12);
    const std::uint32_t central_offset = read_u32(bytes, end + 16);
    if (disk != 0 || central_disk != 0 || disk_count != total_count
        || static_cast<std::uint64_t>(central_offset) + central_size
            > bytes.size()) {
        throw std::runtime_error("multi-disk or malformed Q3 ZIP archive");
    }
    std::vector<ZipEntry> entries;
    entries.reserve(total_count);
    std::size_t cursor = central_offset;
    for (std::size_t index = 0; index < total_count; ++index) {
        if (cursor > bytes.size() || bytes.size() - cursor < 46
            || read_u32(bytes, cursor) != central_signature) {
            throw std::runtime_error("Q3 ZIP central directory is malformed");
        }
        const std::uint16_t method = read_u16(bytes, cursor + 10);
        const std::uint32_t crc = read_u32(bytes, cursor + 16);
        const std::uint32_t compressed = read_u32(bytes, cursor + 20);
        const std::uint32_t uncompressed = read_u32(bytes, cursor + 24);
        const std::uint16_t name_length = read_u16(bytes, cursor + 28);
        const std::uint16_t extra_length = read_u16(bytes, cursor + 30);
        const std::uint16_t comment_length = read_u16(bytes, cursor + 32);
        const std::uint32_t local_offset = read_u32(bytes, cursor + 42);
        const std::size_t record_size = 46ULL + name_length + extra_length
            + comment_length;
        if (record_size > bytes.size() - cursor) {
            throw std::runtime_error("Q3 ZIP central directory entry is "
                                     "truncated");
        }
        ZipEntry entry;
        entry.name.assign(reinterpret_cast<const char*>(
                              bytes.data() + cursor + 46),
                          name_length);
        entry.method = method;
        entry.compressed_size = compressed;
        entry.uncompressed_size = uncompressed;
        entry.crc = crc;
        entry.local_offset = local_offset;
        entries.push_back(std::move(entry));
        cursor += record_size;
    }
    return entries;
}

[[nodiscard]] std::vector<std::uint8_t> zip_file(
    std::span<const std::uint8_t> archive, const ZipEntry& entry) {
    constexpr std::uint32_t local_signature = 0x04034b50U;
    const std::size_t local = entry.local_offset;
    if (local > archive.size() || archive.size() - local < 30
        || read_u32(archive, local) != local_signature) {
        throw std::runtime_error("Q3 ZIP local entry is malformed");
    }
    const std::uint16_t name_length = read_u16(archive, local + 26);
    const std::uint16_t extra_length = read_u16(archive, local + 28);
    const std::size_t data_offset = local + 30ULL + name_length + extra_length;
    if (data_offset > archive.size()
        || entry.compressed_size > archive.size() - data_offset) {
        throw std::runtime_error("Q3 ZIP entry data is outside the archive");
    }
    const auto compressed = archive.subspan(data_offset, entry.compressed_size);
    std::vector<std::uint8_t> output;
    if (entry.method == 0) {
        if (entry.compressed_size != entry.uncompressed_size) {
            throw std::runtime_error("Q3 ZIP stored entry has mismatched "
                                     "sizes");
        }
        output.assign(compressed.begin(), compressed.end());
    } else if (entry.method == 8) {
        output = inflate_raw(compressed, entry.uncompressed_size);
    } else {
        throw std::runtime_error("Q3 ZIP uses an unsupported compression "
                                 "method");
    }
    if (output.size() > std::numeric_limits<uInt>::max()
        || crc32(0, reinterpret_cast<const Bytef*>(output.data()),
                 static_cast<uInt>(output.size())) != entry.crc) {
        throw std::runtime_error("Q3 ZIP entry CRC does not match");
    }
    return output;
}

[[nodiscard]] std::vector<std::uint8_t> read_level_bytes(
    const std::filesystem::path& source,
    std::optional<std::string_view> map_name) {
    std::error_code error;
    if (!std::filesystem::is_regular_file(source, error) || error) {
        throw std::runtime_error("No such file: " + source.string());
    }
    if (lower_ascii(source.extension().string()) == ".bsp") {
        return read_file(source);
    }
    const auto archive_bytes = read_file(source);
    const std::span<const std::uint8_t> archive(archive_bytes);
    const auto entries = read_zip_entries(archive);
    std::vector<const ZipEntry*> maps;
    for (const ZipEntry& entry : entries) {
        if (ends_with_bsp(entry.name)) {
            maps.push_back(&entry);
        }
    }
    if (maps.empty()) {
        throw std::runtime_error(source.filename().string()
                                 + " contains no .bsp.");
    }
    const ZipEntry* selected = maps.front();
    if (map_name.has_value()) {
        selected = nullptr;
        const std::string wanted = lower_ascii(*map_name);
        for (const ZipEntry* entry : maps) {
            if (lower_ascii(entry_stem(entry->name)) == wanted) {
                selected = entry;
                break;
            }
        }
        if (selected == nullptr) {
            std::vector<std::string> names;
            names.reserve(maps.size());
            for (const ZipEntry* entry : maps) {
                names.push_back(entry_stem(entry->name));
            }
            std::sort(names.begin(), names.end());
            std::string available;
            for (const std::string& name : names) {
                if (!available.empty()) {
                    available += ", ";
                }
                available += name;
            }
            throw std::runtime_error(source.filename().string() + " has no "
                                     "map " + std::string(*map_name)
                                     + ". It has: " + available);
        }
    }
    return zip_file(archive, *selected);
}

} // namespace

bool Q3EntityKeyLess::operator()(const std::string& left,
                                  const std::string& right) const noexcept {
    const std::size_t common = std::min(left.size(), right.size());
    for (std::size_t index = 0; index < common; ++index) {
        const unsigned char left_value = fold_ascii(
            static_cast<unsigned char>(left[index]));
        const unsigned char right_value = fold_ascii(
            static_cast<unsigned char>(right[index]));
        if (left_value != right_value) {
            return left_value < right_value;
        }
    }
    return left.size() < right.size();
}

namespace detail {
namespace {

template <typename Callback>
void read_records(std::span<const std::uint8_t> bytes, std::int32_t offset,
                  std::int32_t length, std::size_t record_size,
                  Callback&& callback) {
    if (length <= 0) {
        return;
    }
    if (offset < 0) {
        throw std::runtime_error("Q3 BSP lump is outside the input");
    }
    const std::size_t count = static_cast<std::size_t>(length) / record_size;
    for (std::size_t index = 0; index < count; ++index) {
        const std::uint64_t record_offset =
            static_cast<std::uint64_t>(offset)
            + static_cast<std::uint64_t>(index) * record_size;
        if (record_offset > bytes.size()
            || record_size > bytes.size() - record_offset) {
            throw std::runtime_error("Q3 BSP lump is outside the input");
        }
        callback(static_cast<std::size_t>(record_offset));
    }
}

[[nodiscard]] std::vector<Q3Entity> parse_entities(std::string_view text) {
    std::vector<Q3Entity> result;
    std::optional<Q3Entity> current;
    std::string token;
    std::vector<std::string> tokens;
    bool in_string = false;
    for (const char character : text) {
        if (character == '"') {
            if (in_string) {
                tokens.push_back(token);
                token.clear();
            }
            in_string = !in_string;
            continue;
        }
        if (in_string) {
            token.push_back(character);
            continue;
        }
        if (character == '{') {
            current.emplace();
            tokens.clear();
        } else if (character == '}') {
            if (current.has_value()) {
                for (std::size_t index = 0; index + 1 < tokens.size();
                     index += 2) {
                    (*current)[tokens[index]] = tokens[index + 1];
                }
                result.push_back(std::move(*current));
                current.reset();
            }
            tokens.clear();
        }
    }
    return result;
}

} // namespace

Q3Bsp parse_bsp(std::span<const std::uint8_t> bytes) {
    if (bytes.size() < 4) {
        throw std::runtime_error("Q3 BSP read exceeds the input");
    }
    const std::string magic = ascii_string(bytes, 0, 4);
    const std::int32_t version = read_i32(bytes, 4);
    if (magic != "IBSP" || version != 46) {
        throw std::runtime_error("Not a Quake 3 level (magic " + magic
                                 + ", version " + std::to_string(version)
                                 + ").");
    }
    std::array<std::pair<std::int32_t, std::int32_t>, 17> lumps{};
    for (std::size_t index = 0; index < lumps.size(); ++index) {
        lumps[index] = {read_i32(bytes, 8 + index * 8),
                        read_i32(bytes, 12 + index * 8)};
    }

    Q3Bsp result;
    const auto [entity_offset, entity_length] = lumps[0];
    const std::size_t entity_begin = checked_range(
        bytes, entity_offset, entity_length);
    result.entities = parse_entities(ascii_string(
        bytes, entity_begin, static_cast<std::size_t>(entity_length)));

    const auto [texture_offset, texture_length] = lumps[1];
    read_records(bytes, texture_offset, texture_length, 72,
                 [&](std::size_t offset) {
        result.textures.push_back({fixed_ascii(bytes, offset, 64),
                                   read_i32(bytes, offset + 64),
                                   read_i32(bytes, offset + 68)});
    });

    const auto [plane_offset, plane_length] = lumps[2];
    read_records(bytes, plane_offset, plane_length, 16,
                 [&](std::size_t offset) {
        result.planes.push_back({{read_f32(bytes, offset),
                                  read_f32(bytes, offset + 4),
                                  read_f32(bytes, offset + 8)},
                                 read_f32(bytes, offset + 12)});
    });

    const auto [model_offset, model_length] = lumps[7];
    read_records(bytes, model_offset, model_length, 40,
                 [&](std::size_t offset) {
        result.models.push_back({
            {read_f32(bytes, offset), read_f32(bytes, offset + 4),
             read_f32(bytes, offset + 8)},
            {read_f32(bytes, offset + 12), read_f32(bytes, offset + 16),
             read_f32(bytes, offset + 20)},
            read_i32(bytes, offset + 24), read_i32(bytes, offset + 28),
            read_i32(bytes, offset + 32), read_i32(bytes, offset + 36)});
    });

    const auto [brush_offset, brush_length] = lumps[8];
    read_records(bytes, brush_offset, brush_length, 12,
                 [&](std::size_t offset) {
        result.brushes.push_back({read_i32(bytes, offset),
                                  read_i32(bytes, offset + 4),
                                  read_i32(bytes, offset + 8)});
    });

    const auto [side_offset, side_length] = lumps[9];
    read_records(bytes, side_offset, side_length, 8,
                 [&](std::size_t offset) {
        result.brush_sides.push_back({read_i32(bytes, offset),
                                      read_i32(bytes, offset + 4)});
    });

    const auto [vertex_offset, vertex_length] = lumps[10];
    read_records(bytes, vertex_offset, vertex_length, 44,
                 [&](std::size_t offset) {
        Q3Vertex vertex;
        vertex.position = {read_f32(bytes, offset),
                           read_f32(bytes, offset + 4),
                           read_f32(bytes, offset + 8)};
        vertex.surface_s = read_f32(bytes, offset + 12);
        vertex.surface_t = read_f32(bytes, offset + 16);
        vertex.normal = {read_f32(bytes, offset + 28),
                         read_f32(bytes, offset + 32),
                         read_f32(bytes, offset + 36)};
        for (std::size_t index = 0; index < vertex.color.size(); ++index) {
            vertex.color[index] = bytes[offset + 40 + index];
        }
        result.vertices.push_back(vertex);
    });

    const auto [mesh_offset, mesh_length] = lumps[11];
    read_records(bytes, mesh_offset, mesh_length, 4,
                 [&](std::size_t offset) {
        result.mesh_vertices.push_back(read_i32(bytes, offset));
    });

    const auto [face_offset, face_length] = lumps[13];
    read_records(bytes, face_offset, face_length, 104,
                 [&](std::size_t offset) {
        Q3Face face;
        face.texture = read_i32(bytes, offset);
        face.effect = read_i32(bytes, offset + 4);
        face.type = read_i32(bytes, offset + 8);
        face.vertex = read_i32(bytes, offset + 12);
        face.vertex_count = read_i32(bytes, offset + 16);
        face.mesh_vertex = read_i32(bytes, offset + 20);
        face.mesh_vertex_count = read_i32(bytes, offset + 24);
        face.normal = {read_f32(bytes, offset + 84),
                       read_f32(bytes, offset + 88),
                       read_f32(bytes, offset + 92)};
        face.size = {read_i32(bytes, offset + 96),
                     read_i32(bytes, offset + 100)};
        result.faces.push_back(face);
    });
    return result;
}

} // namespace detail

Q3Bsp Q3Bsp::load(const std::filesystem::path& source,
                   std::optional<std::string_view> map_name) {
    return detail::parse_bsp(read_level(source, map_name));
}

std::vector<std::uint8_t> Q3Bsp::read_level(
    const std::filesystem::path& source,
    std::optional<std::string_view> map_name) {
    return read_level_bytes(source, map_name);
}

std::vector<std::string> Q3Bsp::list_maps(
    const std::filesystem::path& source) {
    if (lower_ascii(source.extension().string()) == ".bsp") {
        return {source.stem().string()};
    }
    const auto archive_bytes = read_file(source);
    const std::span<const std::uint8_t> archive(archive_bytes);
    const auto entries = read_zip_entries(archive);
    std::vector<std::string> result;
    for (const ZipEntry& entry : entries) {
        if (ends_with_bsp(entry.name)) {
            result.push_back(entry_stem(entry.name));
        }
    }
    std::sort(result.begin(), result.end());
    return result;
}

std::vector<std::uint8_t> Q3Bsp::trim(
    std::span<const std::uint8_t> bsp) {
    constexpr std::size_t header_size = 8 + 17 * 8;
    if (bsp.size() < header_size) {
        throw std::runtime_error(
            "Not a Quake 3 level: too short to hold a header.");
    }
    std::vector<std::uint8_t> output;
    output.insert(output.end(), bsp.begin(), bsp.begin() + header_size);
    const auto patch_i32 = [&](std::size_t offset, std::int32_t value) {
        const std::uint32_t raw = static_cast<std::uint32_t>(value);
        output[offset] = static_cast<std::uint8_t>(raw);
        output[offset + 1] = static_cast<std::uint8_t>(raw >> 8);
        output[offset + 2] = static_cast<std::uint8_t>(raw >> 16);
        output[offset + 3] = static_cast<std::uint8_t>(raw >> 24);
    };
    for (int lump = 0; lump < 17; ++lump) {
        const std::int32_t offset = read_i32(
            bsp, 8 + static_cast<std::size_t>(lump) * 8);
        const std::int32_t length = read_i32(
            bsp, 12 + static_cast<std::size_t>(lump) * 8);
        const bool used = std::find(UsedLumps.begin(), UsedLumps.end(), lump)
            != UsedLumps.end();
        const std::int64_t end = static_cast<std::int64_t>(offset)
            + static_cast<std::int64_t>(length);
        if (!used || offset < 0 || length <= 0
            || end > static_cast<std::int64_t>(bsp.size())) {
            patch_i32(8 + static_cast<std::size_t>(lump) * 8,
                      static_cast<std::int32_t>(output.size()));
            patch_i32(12 + static_cast<std::size_t>(lump) * 8, 0);
            continue;
        }
        patch_i32(8 + static_cast<std::size_t>(lump) * 8,
                  static_cast<std::int32_t>(output.size()));
        patch_i32(12 + static_cast<std::size_t>(lump) * 8, length);
        output.insert(output.end(),
                      bsp.begin() + static_cast<std::size_t>(offset),
                      bsp.begin() + static_cast<std::size_t>(end));
        while (output.size() % 4 != 0) {
            output.push_back(0);
        }
    }
    return output;
}

} // namespace fruityprime::mapgen
