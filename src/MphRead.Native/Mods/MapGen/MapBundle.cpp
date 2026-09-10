#include "map_bundle.hpp"

#include "q3_bsp.hpp"
#include "q3_import.hpp"

#include <zlib.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace fruityprime::mapgen::bundle {
namespace {

constexpr std::uint32_t ZipEndSignature = 0x06054b50U;
constexpr std::uint32_t ZipCentralSignature = 0x02014b50U;
constexpr std::uint32_t ZipLocalSignature = 0x04034b50U;

[[nodiscard]] std::string lower(std::string_view value) {
    std::string result;
    result.reserve(value.size());
    for (const char character : value) {
        result.push_back(static_cast<char>(std::tolower(
            static_cast<unsigned char>(character == '\\' ? '/' : character))));
    }
    return result;
}

[[nodiscard]] bool ends_with(std::string_view value,
                             std::string_view suffix) {
    if (value.size() < suffix.size()) {
        return false;
    }
    return lower(value.substr(value.size() - suffix.size())) == lower(suffix);
}

[[nodiscard]] std::uint16_t read_u16(std::span<const std::uint8_t> bytes,
                                     std::size_t offset) {
    if (offset > bytes.size() || bytes.size() - offset < 2) {
        throw std::runtime_error("map bundle ZIP read exceeds the input");
    }
    return static_cast<std::uint16_t>(bytes[offset])
        | static_cast<std::uint16_t>(bytes[offset + 1]) << 8;
}

[[nodiscard]] std::uint32_t read_u32(std::span<const std::uint8_t> bytes,
                                     std::size_t offset) {
    if (offset > bytes.size() || bytes.size() - offset < 4) {
        throw std::runtime_error("map bundle ZIP read exceeds the input");
    }
    return static_cast<std::uint32_t>(bytes[offset])
        | static_cast<std::uint32_t>(bytes[offset + 1]) << 8
        | static_cast<std::uint32_t>(bytes[offset + 2]) << 16
        | static_cast<std::uint32_t>(bytes[offset + 3]) << 24;
}

[[nodiscard]] std::vector<std::uint8_t> read_file(
    const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary | std::ios::ate);
    if (!input) {
        throw std::runtime_error("could not open map bundle " + path.string());
    }
    const std::streamoff length = input.tellg();
    if (length < 0 || static_cast<std::uintmax_t>(length)
        > std::numeric_limits<std::size_t>::max()) {
        throw std::runtime_error("map bundle is too large: " + path.string());
    }
    std::vector<std::uint8_t> result(static_cast<std::size_t>(length));
    input.seekg(0);
    if (!result.empty()) {
        input.read(reinterpret_cast<char*>(result.data()),
                   static_cast<std::streamsize>(result.size()));
        if (!input) {
            throw std::runtime_error("could not read map bundle " + path.string());
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

[[nodiscard]] std::vector<ZipEntry> read_entries(
    std::span<const std::uint8_t> bytes) {
    const std::size_t search_start = bytes.size() > 65'557
        ? bytes.size() - 65'557 : 0;
    std::optional<std::size_t> end_offset;
    if (bytes.size() >= 4) {
        for (std::size_t offset = bytes.size() - 4;;) {
            if (read_u32(bytes, offset) == ZipEndSignature) {
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
        throw std::runtime_error("map bundle is not a ZIP archive");
    }
    const std::size_t end = *end_offset;
    const std::uint16_t disk = read_u16(bytes, end + 4);
    const std::uint16_t central_disk = read_u16(bytes, end + 6);
    const std::uint16_t disk_count = read_u16(bytes, end + 8);
    const std::uint16_t total_count = read_u16(bytes, end + 10);
    const std::uint32_t central_size = read_u32(bytes, end + 12);
    const std::uint32_t central_offset = read_u32(bytes, end + 16);
    if (disk != 0 || central_disk != 0 || disk_count != total_count
        || static_cast<std::uint64_t>(central_offset) + central_size > bytes.size()) {
        throw std::runtime_error("map bundle ZIP central directory is malformed");
    }
    std::vector<ZipEntry> result;
    result.reserve(total_count);
    std::size_t cursor = central_offset;
    for (std::size_t index = 0; index < total_count; ++index) {
        if (cursor > bytes.size() || bytes.size() - cursor < 46
            || read_u32(bytes, cursor) != ZipCentralSignature) {
            throw std::runtime_error("map bundle ZIP entry is malformed");
        }
        const std::uint16_t name_length = read_u16(bytes, cursor + 28);
        const std::uint16_t extra_length = read_u16(bytes, cursor + 30);
        const std::uint16_t comment_length = read_u16(bytes, cursor + 32);
        const std::size_t record_size = 46ULL + name_length + extra_length
            + comment_length;
        if (record_size > bytes.size() - cursor) {
            throw std::runtime_error("map bundle ZIP entry is truncated");
        }
        ZipEntry entry;
        entry.name.assign(reinterpret_cast<const char*>(bytes.data() + cursor + 46),
                          name_length);
        entry.method = read_u16(bytes, cursor + 10);
        entry.crc = read_u32(bytes, cursor + 16);
        entry.compressed_size = read_u32(bytes, cursor + 20);
        entry.uncompressed_size = read_u32(bytes, cursor + 24);
        entry.local_offset = read_u32(bytes, cursor + 42);
        result.push_back(std::move(entry));
        cursor += record_size;
    }
    return result;
}

[[nodiscard]] std::vector<std::uint8_t> inflate_raw(
    std::span<const std::uint8_t> compressed, std::size_t output_size) {
    if (output_size > std::numeric_limits<uInt>::max()) {
        throw std::runtime_error("map bundle entry is too large for zlib");
    }
    z_stream stream{};
    stream.next_in = const_cast<Bytef*>(
        reinterpret_cast<const Bytef*>(compressed.data()));
    stream.avail_in = static_cast<uInt>(compressed.size());
    if (inflateInit2(&stream, -MAX_WBITS) != Z_OK) {
        throw std::runtime_error("could not initialize map bundle decompressor");
    }
    std::vector<std::uint8_t> output(std::max<std::size_t>(output_size, 1));
    stream.next_out = reinterpret_cast<Bytef*>(output.data());
    stream.avail_out = static_cast<uInt>(output.size());
    const int status = inflate(&stream, Z_FINISH);
    const int end_status = inflateEnd(&stream);
    if (status != Z_STREAM_END || end_status != Z_OK
        || stream.total_out != output_size || stream.total_in != compressed.size()) {
        throw std::runtime_error("invalid or truncated map bundle entry");
    }
    output.resize(output_size);
    return output;
}

[[nodiscard]] std::vector<std::uint8_t> entry_bytes(
    std::span<const std::uint8_t> archive, const ZipEntry& entry) {
    const std::size_t local = entry.local_offset;
    if (local > archive.size() || archive.size() - local < 30
        || read_u32(archive, local) != ZipLocalSignature) {
        throw std::runtime_error("map bundle ZIP local entry is malformed");
    }
    const std::uint16_t name_length = read_u16(archive, local + 26);
    const std::uint16_t extra_length = read_u16(archive, local + 28);
    const std::size_t data_offset = local + 30ULL + name_length + extra_length;
    if (data_offset > archive.size()
        || entry.compressed_size > archive.size() - data_offset) {
        throw std::runtime_error("map bundle ZIP data is outside the archive");
    }
    const auto compressed = archive.subspan(data_offset, entry.compressed_size);
    std::vector<std::uint8_t> output;
    if (entry.method == 0) {
        if (entry.compressed_size != entry.uncompressed_size) {
            throw std::runtime_error("map bundle stored entry has mismatched sizes");
        }
        output.assign(compressed.begin(), compressed.end());
    } else if (entry.method == 8) {
        output = inflate_raw(compressed, entry.uncompressed_size);
    } else {
        throw std::runtime_error("map bundle uses an unsupported compression method");
    }
    if (crc32(0, reinterpret_cast<const Bytef*>(output.data()),
              static_cast<uInt>(output.size())) != entry.crc) {
        throw std::runtime_error("map bundle entry CRC does not match");
    }
    return output;
}

[[nodiscard]] const ZipEntry* find_entry(
    const std::vector<ZipEntry>& entries, std::string_view name) {
    if (name.empty()) {
        return nullptr;
    }
    const std::string wanted = lower(name);
    const std::string suffix = "/" + wanted;
    for (const ZipEntry& entry : entries) {
        const std::string candidate = lower(entry.name);
        if (candidate == wanted || (candidate.size() > suffix.size()
                                    && candidate.ends_with(suffix))) {
            return &entry;
        }
    }
    return nullptr;
}

[[nodiscard]] std::vector<std::uint8_t> trim_bsp(
    std::span<const std::uint8_t> bsp) {
    return Q3Bsp::trim(bsp);
}

[[nodiscard]] std::vector<std::uint8_t> texture_pack_bytes(
    const std::vector<detail::TexturePackEntry>& entries) {
    if (entries.size() > std::numeric_limits<std::uint16_t>::max()) {
        throw std::runtime_error("map bundle texture pack has too many entries");
    }
    std::vector<std::uint8_t> result;
    const auto append_u16 = [&](std::uint16_t value) {
        result.push_back(static_cast<std::uint8_t>(value));
        result.push_back(static_cast<std::uint8_t>(value >> 8));
    };
    result.insert(result.end(), {'F', 'P', 'T', 'X'});
    append_u16(1);
    append_u16(static_cast<std::uint16_t>(entries.size()));
    for (const auto& entry : entries) {
        if (entry.name.size() > std::numeric_limits<std::uint16_t>::max()
            || entry.width == 0 || entry.height == 0
            || entry.pixels.size() != static_cast<std::size_t>(entry.width)
                * entry.height
            || entry.palette.empty()
            || entry.palette.size() > std::numeric_limits<std::uint16_t>::max()) {
            throw std::runtime_error("map bundle texture pack entry is invalid");
        }
        append_u16(entry.source_index);
        append_u16(entry.width);
        append_u16(entry.height);
        append_u16(static_cast<std::uint16_t>(entry.palette.size()));
        append_u16(static_cast<std::uint16_t>(entry.name.size()));
        result.insert(result.end(), entry.name.begin(), entry.name.end());
        for (const std::uint16_t color : entry.palette) {
            append_u16(color);
        }
        result.insert(result.end(), entry.pixels.begin(), entry.pixels.end());
    }
    return result;
}

[[nodiscard]] std::vector<std::uint8_t> compress_entry(
    std::span<const std::uint8_t> bytes) {
    if (bytes.size() > std::numeric_limits<uInt>::max()) {
        throw std::runtime_error("map bundle entry is too large for zlib");
    }
    const uLong bound = compressBound(static_cast<uLong>(bytes.size()));
    if (bound > std::numeric_limits<uInt>::max()) {
        throw std::runtime_error("compressed map bundle entry is too large for zlib");
    }
    std::vector<std::uint8_t> result(static_cast<std::size_t>(bound));
    z_stream stream{};
    if (deflateInit2(&stream, Z_BEST_COMPRESSION, Z_DEFLATED, -MAX_WBITS,
                     8, Z_DEFAULT_STRATEGY) != Z_OK) {
        throw std::runtime_error("could not initialize map bundle compressor");
    }
    stream.next_in = const_cast<Bytef*>(
        reinterpret_cast<const Bytef*>(bytes.data()));
    stream.avail_in = static_cast<uInt>(bytes.size());
    stream.next_out = reinterpret_cast<Bytef*>(result.data());
    stream.avail_out = static_cast<uInt>(result.size());
    const int status = deflate(&stream, Z_FINISH);
    const int end_status = deflateEnd(&stream);
    if (status != Z_STREAM_END || end_status != Z_OK
        || stream.total_in != bytes.size()) {
        throw std::runtime_error("could not compress map bundle entry");
    }
    result.resize(static_cast<std::size_t>(stream.total_out));
    return result;
}

struct OutputEntry {
    std::string name;
    std::vector<std::uint8_t> data;
    std::vector<std::uint8_t> compressed;
    std::uint32_t crc = 0;
    std::uint32_t local_offset = 0;
};

void append_u16(std::vector<std::uint8_t>& output, std::uint16_t value) {
    output.push_back(static_cast<std::uint8_t>(value));
    output.push_back(static_cast<std::uint8_t>(value >> 8));
}

void append_u32(std::vector<std::uint8_t>& output, std::uint32_t value) {
    output.push_back(static_cast<std::uint8_t>(value));
    output.push_back(static_cast<std::uint8_t>(value >> 8));
    output.push_back(static_cast<std::uint8_t>(value >> 16));
    output.push_back(static_cast<std::uint8_t>(value >> 24));
}

[[nodiscard]] std::vector<std::uint8_t> make_zip(
    std::vector<OutputEntry> entries) {
    if (entries.size() > std::numeric_limits<std::uint16_t>::max()) {
        throw std::runtime_error("map bundle has too many files");
    }
    std::vector<std::uint8_t> output;
    for (OutputEntry& entry : entries) {
        if (entry.name.size() > std::numeric_limits<std::uint16_t>::max()
            || entry.data.size() > std::numeric_limits<std::uint32_t>::max()
            || entry.compressed.size() > std::numeric_limits<std::uint32_t>::max()
            || output.size() > std::numeric_limits<std::uint32_t>::max()) {
            throw std::runtime_error("map bundle entry exceeds ZIP limits");
        }
        entry.local_offset = static_cast<std::uint32_t>(output.size());
        append_u32(output, ZipLocalSignature);
        append_u16(output, 20);
        append_u16(output, 0);
        append_u16(output, 8);
        append_u16(output, 0);
        append_u16(output, 0);
        append_u32(output, entry.crc);
        append_u32(output, static_cast<std::uint32_t>(entry.compressed.size()));
        append_u32(output, static_cast<std::uint32_t>(entry.data.size()));
        append_u16(output, static_cast<std::uint16_t>(entry.name.size()));
        append_u16(output, 0);
        output.insert(output.end(), entry.name.begin(), entry.name.end());
        output.insert(output.end(), entry.compressed.begin(), entry.compressed.end());
    }
    const std::uint32_t central_offset = static_cast<std::uint32_t>(output.size());
    for (const OutputEntry& entry : entries) {
        append_u32(output, ZipCentralSignature);
        append_u16(output, 20);
        append_u16(output, 20);
        append_u16(output, 0);
        append_u16(output, 8);
        append_u16(output, 0);
        append_u16(output, 0);
        append_u32(output, entry.crc);
        append_u32(output, static_cast<std::uint32_t>(entry.compressed.size()));
        append_u32(output, static_cast<std::uint32_t>(entry.data.size()));
        append_u16(output, static_cast<std::uint16_t>(entry.name.size()));
        append_u16(output, 0);
        append_u16(output, 0);
        append_u16(output, 0);
        append_u16(output, 0);
        append_u32(output, 0);
        append_u32(output, entry.local_offset);
        output.insert(output.end(), entry.name.begin(), entry.name.end());
    }
    const std::uint32_t central_size =
        static_cast<std::uint32_t>(output.size()) - central_offset;
    append_u32(output, ZipEndSignature);
    append_u16(output, 0);
    append_u16(output, 0);
    append_u16(output, static_cast<std::uint16_t>(entries.size()));
    append_u16(output, static_cast<std::uint16_t>(entries.size()));
    append_u32(output, central_size);
    append_u32(output, central_offset);
    append_u16(output, 0);
    return output;
}

[[nodiscard]] std::filesystem::path resolve_candidate(
    const std::filesystem::path& recipe_path, std::string_view value) {
    const std::filesystem::path requested(value);
    std::vector<std::filesystem::path> candidates;
    if (requested.is_absolute()) {
        candidates.push_back(requested);
    } else {
        candidates.push_back(recipe_path.parent_path() / requested);
        candidates.push_back(std::filesystem::current_path() / requested);
        candidates.push_back(std::filesystem::current_path() / "maps" / requested);
    }
    for (const auto& candidate : candidates) {
        std::error_code error;
        if (std::filesystem::is_regular_file(candidate, error) && !error) {
            return std::filesystem::absolute(candidate);
        }
    }
    return {};
}

[[nodiscard]] std::string map_name_for(
    const MapDefinition& definition, const std::filesystem::path& source) {
    if (!definition.import_map_name.empty()) {
        return definition.import_map_name;
    }
    const auto maps = detail::list_q3_maps(source);
    if (maps.empty()) {
        throw std::runtime_error(source.filename().string()
                                 + " contains no .bsp level");
    }
    return maps.front();
}

} // namespace

bool is_bundle(const std::filesystem::path& path) noexcept {
    return lower(path.extension().string()) == Extension;
}

std::optional<std::vector<std::uint8_t>> read_entry(
    const std::filesystem::path& bundle_path, std::string_view name) {
    if (name.empty()) {
        return std::nullopt;
    }
    const auto bytes = read_file(bundle_path);
    const std::span<const std::uint8_t> archive(bytes);
    const auto entries = read_entries(archive);
    const ZipEntry* entry = find_entry(entries, name);
    if (entry == nullptr) {
        return std::nullopt;
    }
    return entry_bytes(archive, *entry);
}

std::optional<std::string> read_recipe(
    const std::filesystem::path& bundle_path) {
    const auto bytes = read_file(bundle_path);
    const std::span<const std::uint8_t> archive(bytes);
    const auto entries = read_entries(archive);
    for (const ZipEntry& entry : entries) {
        if (!ends_with(entry.name, ".json")) {
            continue;
        }
        const auto value = entry_bytes(archive, entry);
        return std::string(reinterpret_cast<const char*>(value.data()),
                           value.size());
    }
    return std::nullopt;
}

std::filesystem::path cook(const MapDefinition& definition,
                           const std::filesystem::path& recipe_path,
                           const std::filesystem::path& output_path,
                           bool verbose) {
    if (definition.import_source.empty()) {
        throw std::runtime_error(definition.name
                                 + " builds from its own description; there is no level to bundle");
    }
    const auto source = resolve_candidate(recipe_path, definition.import_source);
    if (source.empty()) {
        throw std::runtime_error(definition.name + ": its source level "
                                 + definition.import_source + " is not here");
    }
    if (is_bundle(source)) {
        throw std::runtime_error(definition.name
                                 + " is already a map bundle");
    }

    MapDefinition probe = definition;
    probe.import_source = source.string();
    const std::string map_name = map_name_for(probe, source);
    probe.import_map_name = map_name;
    const std::vector<std::uint8_t> level = detail::read_q3_level(probe, source);
    const std::vector<std::uint8_t> trimmed = trim_bsp(level);

    std::string texture_name;
    std::vector<std::uint8_t> texture_data;
    if (!definition.import_textures.empty()) {
        const auto texture_path = resolve_candidate(
            recipe_path, definition.import_textures);
        if (!texture_path.empty()) {
            texture_name = texture_path.filename().string();
            texture_data = read_file(texture_path);
        } else {
            const detail::Q3Bsp bsp = detail::parse_bsp(level);
            const auto baked = detail::bake_q3_texture_pack(
                bsp, source, probe, 64);
            if (baked.empty()) {
                throw std::runtime_error(definition.name + ": its textures ("
                                         + definition.import_textures
                                         + ") are not here and could not be baked");
            }
            texture_name = std::filesystem::path(
                definition.import_textures).filename().string();
            texture_data = texture_pack_bytes(baked);
        }
    }

    MapDefinition inside = load_definition(recipe_path);
    inside.import_source = "maps/" + map_name + ".bsp";
    inside.import_map_name = map_name;
    inside.import_textures = texture_name;
    const std::string recipe_name = recipe_path.filename().string();
    const std::filesystem::path destination = output_path.empty()
        ? std::filesystem::current_path() / "maps"
            / (recipe_path.stem().string() + std::string(Extension))
        : output_path;
    std::error_code error;
    std::filesystem::create_directories(destination.parent_path(), error);
    if (error) {
        throw std::runtime_error("could not create map bundle directory: "
                                 + error.message());
    }

    std::vector<OutputEntry> entries;
    const auto add = [&](std::string name, std::vector<std::uint8_t> data) {
        if (name.size() > std::numeric_limits<std::uint16_t>::max()) {
            throw std::runtime_error("map bundle entry name is too long");
        }
        OutputEntry entry;
        entry.name = std::move(name);
        entry.crc = crc32(0, reinterpret_cast<const Bytef*>(data.data()),
                          static_cast<uInt>(data.size()));
        entry.compressed = compress_entry(data);
        entry.data = std::move(data);
        entries.push_back(std::move(entry));
    };
    const std::string recipe_text = serialize_definition(inside);
    add(recipe_name, std::vector<std::uint8_t>(recipe_text.begin(), recipe_text.end()));
    add("maps/" + map_name + ".bsp", trimmed);
    if (!texture_data.empty()) {
        add(texture_name, std::move(texture_data));
    }
    const std::vector<std::uint8_t> archive = make_zip(std::move(entries));
    const std::filesystem::path temporary = destination.string() + ".tmp";
    {
        std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
        if (!output) {
            throw std::runtime_error("could not create map bundle "
                                     + temporary.string());
        }
        output.write(reinterpret_cast<const char*>(archive.data()),
                     static_cast<std::streamsize>(archive.size()));
        if (!output) {
            throw std::runtime_error("could not write map bundle "
                                     + temporary.string());
        }
    }
    std::filesystem::remove(destination, error);
    error.clear();
    std::filesystem::rename(temporary, destination, error);
    if (error) {
        std::filesystem::remove(temporary);
        throw std::runtime_error("could not install map bundle: "
                                 + error.message());
    }
    if (verbose) {
        std::error_code size_error;
        const auto cooked_size = std::filesystem::file_size(destination, size_error);
        const auto level_size = std::filesystem::file_size(source, size_error);
        std::cout << "[mapbundle] " << definition.name << " -> "
                  << destination.string() << " ("
                  << (cooked_size / 1024) << " KiB, from "
                  << (level_size / 1024) << " KiB)\n";
    }
    return destination;
}

} // namespace fruityprime::mapgen::bundle
