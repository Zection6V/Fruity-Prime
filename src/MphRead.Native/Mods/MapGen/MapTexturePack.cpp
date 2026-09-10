#include "map_texture_pack.hpp"

#include "map_bundle.hpp"

#include <fstream>
#include <limits>
#include <map>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

namespace fruityprime::mapgen::texture_pack_io {
namespace {

[[nodiscard]] std::vector<std::uint8_t> read_file(
    const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary | std::ios::ate);
    if (!input) {
        throw std::runtime_error("could not open texture pack "
                                 + path.string());
    }
    const std::streamoff length = input.tellg();
    if (length < 0 || static_cast<std::uintmax_t>(length)
        > std::numeric_limits<std::size_t>::max()) {
        throw std::runtime_error("texture pack is too large: " + path.string());
    }
    std::vector<std::uint8_t> result(static_cast<std::size_t>(length));
    input.seekg(0);
    if (!result.empty()) {
        input.read(reinterpret_cast<char*>(result.data()),
                   static_cast<std::streamsize>(result.size()));
        if (!input) {
            throw std::runtime_error("could not read texture pack "
                                     + path.string());
        }
    }
    return result;
}

[[nodiscard]] std::uint16_t read_u16(std::span<const std::uint8_t> bytes,
                                     std::size_t offset) {
    if (offset > bytes.size() || bytes.size() - offset < 2) {
        throw std::runtime_error("FPTX read exceeds the input");
    }
    return static_cast<std::uint16_t>(bytes[offset])
        | static_cast<std::uint16_t>(bytes[offset + 1] << 8);
}

[[nodiscard]] std::filesystem::path resolve(const MapDefinition& definition) {
    if (definition.import_textures.empty()) {
        return {};
    }
    const std::filesystem::path texture_path(definition.import_textures);
    std::vector<std::filesystem::path> candidates;
    if (texture_path.is_absolute()) {
        candidates.push_back(texture_path);
    } else {
        if (!definition.source_path.empty()) {
            candidates.push_back(definition.source_path.parent_path()
                                 / texture_path);
        }
        candidates.push_back(std::filesystem::current_path() / texture_path);
        candidates.push_back(std::filesystem::current_path() / "maps"
                             / texture_path);
    }
    for (const auto& candidate : candidates) {
        std::error_code error;
        if (std::filesystem::is_regular_file(candidate, error) && !error) {
            return std::filesystem::absolute(candidate);
        }
    }
    return {};
}

} // namespace

[[nodiscard]] std::vector<detail::TexturePackEntry> parse(
    std::span<const std::uint8_t> input, std::string_view label) {
    if (input.size() < 8
        || std::string(reinterpret_cast<const char*>(input.data()), 4) != "FPTX"
        || read_u16(input, 4) != 1) {
        throw std::runtime_error("texture pack " + std::string(label)
                                 + " is not FPTX version 1");
    }
    const std::size_t count = read_u16(input, 6);
    std::vector<detail::TexturePackEntry> result;
    result.reserve(count);
    std::map<std::uint16_t, bool> seen_sources;
    std::size_t cursor = 8;
    for (std::size_t index = 0; index < count; ++index) {
        if (cursor > input.size() || input.size() - cursor < 10) {
            throw std::runtime_error("FPTX entry header is truncated");
        }
        detail::TexturePackEntry entry;
        entry.source_index = read_u16(input, cursor);
        entry.width = read_u16(input, cursor + 2);
        entry.height = read_u16(input, cursor + 4);
        const std::size_t palette_count = read_u16(input, cursor + 6);
        const std::size_t name_length = read_u16(input, cursor + 8);
        cursor += 10;
        if (entry.width == 0 || entry.height == 0 || palette_count == 0
            || palette_count > 256
            || seen_sources.find(entry.source_index) != seen_sources.end()) {
            throw std::runtime_error(
                "FPTX entry dimensions, palette, or source index are invalid");
        }
        seen_sources.emplace(entry.source_index, true);
        if (name_length > input.size() - cursor) {
            throw std::runtime_error("FPTX entry name is truncated");
        }
        entry.name.assign(reinterpret_cast<const char*>(input.data() + cursor),
                          name_length);
        cursor += name_length;
        if (palette_count > (input.size() - cursor) / 2) {
            throw std::runtime_error("FPTX palette is truncated");
        }
        entry.palette.reserve(palette_count);
        for (std::size_t palette = 0; palette < palette_count; ++palette) {
            entry.palette.push_back(read_u16(input, cursor));
            cursor += 2;
        }
        const std::size_t pixel_count = static_cast<std::size_t>(entry.width)
            * entry.height;
        if (pixel_count > input.size() - cursor) {
            throw std::runtime_error("FPTX pixel data is truncated");
        }
        entry.pixels.assign(
            input.begin() + static_cast<std::ptrdiff_t>(cursor),
            input.begin() + static_cast<std::ptrdiff_t>(cursor + pixel_count));
        cursor += pixel_count;
        result.push_back(std::move(entry));
    }
    if (cursor != input.size()) {
        throw std::runtime_error("FPTX contains trailing data");
    }
    return result;
}

std::optional<std::vector<detail::TexturePackEntry>> load_optional(
    const MapDefinition& definition) {
    if (bundle::is_bundle(definition.source_path)) {
        if (definition.import_textures.empty()) {
            return std::nullopt;
        }
        const auto bytes = bundle::read_entry(
            definition.source_path, definition.import_textures);
        if (!bytes.has_value()) {
            return std::nullopt;
        }
        return parse(*bytes, definition.import_textures);
    }
    const std::filesystem::path path = resolve(definition);
    if (path.empty()) {
        return std::nullopt;
    }
    const std::vector<std::uint8_t> bytes = read_file(path);
    return parse(bytes, path.filename().string());
}

std::vector<detail::TexturePackEntry> load(const MapDefinition& definition) {
    const auto result = load_optional(definition);
    return result.has_value()
        ? *result : std::vector<detail::TexturePackEntry>{};
}

} // namespace fruityprime::mapgen::texture_pack_io
