#include "q3_bsp.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstring>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace fruityprime::mapgen::detail {
namespace {

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
    if (!std::isfinite(value)) {
        throw std::runtime_error("Q3 BSP contains a non-finite float");
    }
    return value;
}

struct Lump {
    std::size_t offset = 0;
    std::size_t length = 0;
};

[[nodiscard]] Lump checked_lump(std::span<const std::uint8_t> bytes,
                                std::int32_t offset, std::int32_t length,
                                std::string_view name) {
    if (offset < 0 || length < 0
        || static_cast<std::uint64_t>(offset) + length > bytes.size()) {
        throw std::runtime_error("Q3 BSP " + std::string(name)
                                 + " lump is outside the file");
    }
    return {static_cast<std::size_t>(offset), static_cast<std::size_t>(length)};
}

[[nodiscard]] std::string fixed_ascii(std::span<const std::uint8_t> bytes,
                                      std::size_t offset, std::size_t length) {
    const auto value = bytes.subspan(offset, length);
    const auto end = std::find(value.begin(), value.end(), std::uint8_t{0});
    return std::string(reinterpret_cast<const char*>(value.data()),
                       static_cast<std::size_t>(end - value.begin()));
}

template <typename Callback>
void read_records(std::span<const std::uint8_t> bytes, Lump lump,
                  std::size_t record_size, std::string_view name,
                  Callback&& callback) {
    static_cast<void>(bytes);
    if (record_size == 0 || lump.length % record_size != 0) {
        throw std::runtime_error("Q3 BSP " + std::string(name)
                                 + " lump has a partial record");
    }
    const std::size_t count = lump.length / record_size;
    if (count > 2'000'000) {
        throw std::runtime_error("Q3 BSP " + std::string(name)
                                 + " lump has too many records");
    }
    for (std::size_t index = 0; index < count; ++index) {
        callback(lump.offset + index * record_size);
    }
}

[[nodiscard]] std::string lower(std::string_view value) {
    std::string result;
    result.reserve(value.size());
    for (const char character : value) {
        result.push_back(static_cast<char>(std::tolower(
            static_cast<unsigned char>(character))));
    }
    return result;
}

[[nodiscard]] std::vector<Q3Entity> parse_entities(std::string_view text) {
    std::vector<Q3Entity> result;
    Q3Entity current;
    std::vector<std::string> tokens;
    bool in_string = false;
    std::string token;
    for (std::size_t i = 0; i < text.size(); ++i) {
        const char character = text[i];
        if (character == '"') {
            if (in_string) {
                tokens.push_back(std::move(token));
                token.clear();
            }
            in_string = !in_string;
        } else if (in_string) {
            if (character == '\\' && i + 1 < text.size()) {
                const char escaped = text[++i];
                token.push_back(escaped == 'n' ? '\n' : escaped);
            } else {
                token.push_back(character);
            }
        } else if (character == '{') {
            current.clear();
            tokens.clear();
        } else if (character == '}') {
            for (std::size_t j = 0; j + 1 < tokens.size(); j += 2) {
                current[lower(tokens[j])] = tokens[j + 1];
            }
            if (!current.empty()) {
                result.push_back(std::move(current));
            }
            current.clear();
            tokens.clear();
        }
    }
    if (in_string) {
        throw std::runtime_error("Q3 BSP entity lump has an unterminated string");
    }
    return result;
}

} // namespace

Q3Bsp parse_bsp(std::span<const std::uint8_t> bytes) {
    if (bytes.size() < 8 + 17 * 8
        || std::string(reinterpret_cast<const char*>(bytes.data()), 4) != "IBSP"
        || read_i32(bytes, 4) != 46) {
        throw std::runtime_error("not a Quake 3 level (expected IBSP version 46)");
    }
    std::array<Lump, 17> lumps{};
    for (std::size_t i = 0; i < lumps.size(); ++i) {
        lumps[i] = checked_lump(bytes, read_i32(bytes, 8 + i * 8),
                                 read_i32(bytes, 12 + i * 8),
                                 std::to_string(i));
    }
    Q3Bsp result;
    result.entities = parse_entities(std::string_view(
        reinterpret_cast<const char*>(bytes.data() + lumps[0].offset),
        lumps[0].length));
    read_records(bytes, lumps[1], 72, "textures", [&](std::size_t offset) {
        result.textures.push_back({fixed_ascii(bytes, offset, 64),
                                   read_i32(bytes, offset + 64),
                                   read_i32(bytes, offset + 68)});
    });
    read_records(bytes, lumps[2], 16, "planes", [&](std::size_t offset) {
        result.planes.push_back({{read_f32(bytes, offset),
                                  read_f32(bytes, offset + 4),
                                  read_f32(bytes, offset + 8)},
                                 read_f32(bytes, offset + 12)});
    });
    read_records(bytes, lumps[7], 40, "models", [&](std::size_t offset) {
        result.models.push_back({
            {read_f32(bytes, offset), read_f32(bytes, offset + 4),
             read_f32(bytes, offset + 8)},
            {read_f32(bytes, offset + 12), read_f32(bytes, offset + 16),
             read_f32(bytes, offset + 20)},
            read_i32(bytes, offset + 24), read_i32(bytes, offset + 28),
            read_i32(bytes, offset + 32), read_i32(bytes, offset + 36)});
    });
    read_records(bytes, lumps[8], 12, "brushes", [&](std::size_t offset) {
        result.brushes.push_back({read_i32(bytes, offset),
                                  read_i32(bytes, offset + 4),
                                  read_i32(bytes, offset + 8)});
    });
    read_records(bytes, lumps[9], 8, "brush sides", [&](std::size_t offset) {
        result.brush_sides.push_back({read_i32(bytes, offset),
                                      read_i32(bytes, offset + 4)});
    });
    read_records(bytes, lumps[10], 44, "vertices", [&](std::size_t offset) {
        Q3Vertex vertex;
        vertex.position = {read_f32(bytes, offset), read_f32(bytes, offset + 4),
                           read_f32(bytes, offset + 8)};
        vertex.surface_s = read_f32(bytes, offset + 12);
        vertex.surface_t = read_f32(bytes, offset + 16);
        vertex.normal = {read_f32(bytes, offset + 28),
                         read_f32(bytes, offset + 32),
                         read_f32(bytes, offset + 36)};
        for (std::size_t i = 0; i < 4; ++i) {
            vertex.color[i] = bytes[offset + 40 + i];
        }
        result.vertices.push_back(vertex);
    });
    read_records(bytes, lumps[11], 4, "mesh vertices", [&](std::size_t offset) {
        result.mesh_vertices.push_back(read_i32(bytes, offset));
    });
    read_records(bytes, lumps[13], 104, "faces", [&](std::size_t offset) {
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

} // namespace fruityprime::mapgen::detail
