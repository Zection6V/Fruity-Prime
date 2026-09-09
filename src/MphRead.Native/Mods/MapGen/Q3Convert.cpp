#include "q3_convert.hpp"

#include "q3_import.hpp"

#include "q3_bsp.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <limits>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace fruityprime::mapgen::detail {
namespace {

[[nodiscard]] std::string lower(std::string_view value) {
    std::string result;
    result.reserve(value.size());
    for (const char character : value) {
        result.push_back(static_cast<char>(std::tolower(
            static_cast<unsigned char>(character))));
    }
    return result;
}

[[nodiscard]] std::string upper(std::string_view value) {
    std::string result;
    result.reserve(value.size());
    for (const char character : value) {
        result.push_back(static_cast<char>(std::toupper(
            static_cast<unsigned char>(character))));
    }
    return result;
}

[[nodiscard]] std::string json_quote(std::string_view value) {
    std::string result = "\"";
    for (const unsigned char character : value) {
        switch (character) {
        case '\\': result += "\\\\"; break;
        case '"': result += "\\\""; break;
        case '\n': result += "\\n"; break;
        case '\r': result += "\\r"; break;
        case '\t': result += "\\t"; break;
        default:
            if (character < 0x20) {
                result += "\\u00";
                constexpr char hex[] = "0123456789abcdef";
                result.push_back(hex[character >> 4]);
                result.push_back(hex[character & 0xf]);
            } else {
                result.push_back(static_cast<char>(character));
            }
            break;
        }
    }
    result.push_back('"');
    return result;
}

struct Q3Bounds {
    Vec3 min{std::numeric_limits<float>::max(),
             std::numeric_limits<float>::max(),
             std::numeric_limits<float>::max()};
    Vec3 max{std::numeric_limits<float>::lowest(),
             std::numeric_limits<float>::lowest(),
             std::numeric_limits<float>::lowest()};
    bool valid = false;
};

[[nodiscard]] Q3Bounds visible_bounds(const Q3Bsp& bsp, bool include_sky) {
    Q3Bounds result;
    for (const Q3Face& face : bsp.faces) {
        if (face.type != 1 && face.type != 2 && face.type != 3) {
            continue;
        }
        if (face.texture < 0
            || static_cast<std::size_t>(face.texture) >= bsp.textures.size()) {
            throw std::runtime_error("Q3 face texture index is outside the lump");
        }
        const Q3Texture& texture = bsp.textures[
            static_cast<std::size_t>(face.texture)];
        if ((texture.flags & (0x80 | 0x100 | 0x200)) != 0
            || (!include_sky && (texture.flags & 0x4) != 0)) {
            continue;
        }
        if (face.vertex < 0 || face.vertex_count < 0
            || static_cast<std::size_t>(face.vertex) > bsp.vertices.size()
            || static_cast<std::size_t>(face.vertex_count)
                > bsp.vertices.size() - static_cast<std::size_t>(face.vertex)) {
            throw std::runtime_error("Q3 face vertex range is invalid");
        }
        for (int i = 0; i < face.vertex_count; ++i) {
            const Vec3 point = bsp.vertices[
                static_cast<std::size_t>(face.vertex + i)].position;
            result.min.x = std::min(result.min.x, point.x);
            result.min.y = std::min(result.min.y, point.y);
            result.min.z = std::min(result.min.z, point.z);
            result.max.x = std::max(result.max.x, point.x);
            result.max.y = std::max(result.max.y, point.y);
            result.max.z = std::max(result.max.z, point.z);
            result.valid = true;
        }
    }
    return result;
}

[[nodiscard]] int q3_scale_factor(Q3Bounds bounds, float units_per_unit) {
    float reach = 0.0F;
    for (const float value : {bounds.min.x, bounds.min.y, bounds.min.z,
                              bounds.max.x, bounds.max.y, bounds.max.z}) {
        reach = std::max(reach, std::abs(value) / units_per_unit);
    }
    int factor = 0;
    while (8.0F * std::ldexp(1.0F, factor) < reach && factor < 10) {
        ++factor;
    }
    return factor;
}

void write_u16_le(std::ofstream& output, std::uint16_t value) {
    const char bytes[2] = {
        static_cast<char>(value & 0xff),
        static_cast<char>((value >> 8) & 0xff)
    };
    output.write(bytes, sizeof(bytes));
}

void write_texture_pack(const std::filesystem::path& path,
                        const std::vector<TexturePackEntry>& entries) {
    if (entries.size() > std::numeric_limits<std::uint16_t>::max()) {
        throw std::runtime_error("Q3 texture pack has too many entries");
    }
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    if (!output) {
        throw std::runtime_error("could not create texture pack "
                                 + path.string());
    }
    output.write("FPTX", 4);
    write_u16_le(output, 1);
    write_u16_le(output, static_cast<std::uint16_t>(entries.size()));
    for (const TexturePackEntry& entry : entries) {
        if (entry.name.size() > std::numeric_limits<std::uint16_t>::max()
            || entry.width == 0 || entry.height == 0
            || entry.pixels.size() != static_cast<std::size_t>(entry.width)
                * entry.height
            || entry.palette.empty()
            || entry.palette.size() > std::numeric_limits<std::uint16_t>::max()) {
            throw std::runtime_error("Q3 texture pack entry is invalid");
        }
        write_u16_le(output, entry.source_index);
        write_u16_le(output, entry.width);
        write_u16_le(output, entry.height);
        write_u16_le(output, static_cast<std::uint16_t>(entry.palette.size()));
        write_u16_le(output, static_cast<std::uint16_t>(entry.name.size()));
        output.write(entry.name.data(),
                     static_cast<std::streamsize>(entry.name.size()));
        for (const std::uint16_t color : entry.palette) {
            write_u16_le(output, color);
        }
        output.write(reinterpret_cast<const char*>(entry.pixels.data()),
                     static_cast<std::streamsize>(entry.pixels.size()));
    }
    if (!output) {
        throw std::runtime_error("could not write texture pack "
                                 + path.string());
    }
}

[[nodiscard]] Vec3 to_world(Vec3 value, float units_per_unit) {
    if (!(units_per_unit > 0.0F) || !std::isfinite(units_per_unit)) {
        throw std::runtime_error("Q3 unitsPerUnit must be positive and finite");
    }
    return {value.x / units_per_unit, value.z / units_per_unit,
            -value.y / units_per_unit};
}

[[nodiscard]] std::optional<Vec3> entity_origin(const Q3Entity& entity) {
    const auto found = entity.find("origin");
    if (found == entity.end()) {
        return std::nullopt;
    }
    std::istringstream stream(found->second);
    Vec3 value;
    if (!(stream >> value.x >> value.y >> value.z)) {
        return std::nullopt;
    }
    return value;
}

void write_q3_recipe(const std::filesystem::path& path,
                     const MapDefinition& definition) {
    std::ofstream output(path, std::ios::trunc);
    if (!output) {
        throw std::runtime_error("could not create map recipe " + path.string());
    }
    output << std::setprecision(6) << std::defaultfloat;
    output << "{\n"
           << "  \"name\": " << json_quote(definition.name) << ",\n"
           << "  \"inGameName\": " << json_quote(definition.in_game_name)
           << ",\n"
           << "  \"scaleFactor\": " << definition.scale_factor << ",\n"
           << "  \"killHeight\": " << definition.kill_height << ",\n"
           << "  \"farClip\": " << definition.far_clip << ",\n"
           << "  \"pointLimit\": " << definition.point_limit << ",\n"
           << "  \"import\": {\n"
           << "    \"source\": " << json_quote(definition.import_source)
           << ",\n"
           << "    \"mapName\": " << json_quote(definition.import_map_name)
           << ",\n"
           << "    \"unitsPerUnit\": " << definition.import_units_per_unit
           << ",\n"
           << "    \"textures\": " << json_quote(definition.import_textures)
           << ",\n"
           << "    \"keepSpawns\": "
           << (definition.import_keep_spawns ? "true" : "false") << ",\n"
           << "    \"keepSky\": "
           << (definition.import_keep_sky ? "true" : "false") << ",\n"
           << "    \"keepClip\": "
           << (definition.import_keep_clip ? "true" : "false") << "\n"
           << "  }";
    if (!definition.spawns.empty()) {
        output << ",\n  \"spawns\": [\n";
        for (std::size_t i = 0; i < definition.spawns.size(); ++i) {
            const Spawn& spawn = definition.spawns[i];
            output << "    { \"position\": [" << spawn.position.x << ", "
                    << spawn.position.y << ", " << spawn.position.z
                    << "], \"yaw\": " << spawn.yaw << " }"
                    << (i + 1 == definition.spawns.size() ? "\n" : ",\n");
        }
        output << "  ],\n  \"items\": []\n";
    } else {
        output << ",\n  \"items\": []\n";
    }
    output << "}\n";
    if (!output) {
        throw std::runtime_error("could not write map recipe " + path.string());
    }
}

} // namespace

Q3ConvertResult convert_q3_recipe(const Q3ConvertOptions& options) {
    if (options.source.empty()) {
        throw std::invalid_argument("Q3 conversion source is empty");
    }
    const std::filesystem::path source = std::filesystem::absolute(options.source);
    std::error_code error;
    if (!std::filesystem::is_regular_file(source, error) || error) {
        throw std::invalid_argument("Q3 conversion source does not exist: "
                                    + source.string());
    }
    if (options.texture_size <= 0 || options.texture_size > 1024) {
        throw std::invalid_argument("Q3 texture size must be between 1 and 1024");
    }
    MapDefinition probe;
    probe.import_source = source.string();
    probe.import_map_name = options.map_name;
    probe.import_keep_sky = true;
    probe.import_keep_clip = !options.drop_clip;
    const std::vector<std::uint8_t> level = read_q3_level(probe, source);
    const Q3Bsp bsp = parse_bsp(level);
    const std::vector<std::string> maps = list_q3_maps(source);
    const std::string map_name = options.map_name.empty()
        ? (maps.empty() ? std::string{} : maps.front()) : options.map_name;
    if (map_name.empty()) {
        throw std::runtime_error("Q3 source contains no map name");
    }
    const std::string room = upper(options.room_name.empty() ? map_name
                                                          : options.room_name);
    const Q3Bounds drawn = visible_bounds(bsp, false);
    if (!drawn.valid) {
        throw std::runtime_error(map_name + " has no drawn surfaces");
    }
    const float widest = std::max({drawn.max.x - drawn.min.x,
                                   drawn.max.y - drawn.min.y,
                                   drawn.max.z - drawn.min.z});
    const float units_per_unit = options.forced_units_per_unit > 0.0F
        ? options.forced_units_per_unit
        : std::round(std::max(35.0F, widest / 130.0F));
    if (!(units_per_unit > 0.0F) || !std::isfinite(units_per_unit)) {
        throw std::runtime_error("Q3 conversion produced an invalid unit scale");
    }
    const Q3Bounds reach = visible_bounds(bsp, true);
    const std::string prefix = [&] {
        MapDefinition named;
        named.name = room;
        return file_prefix(named);
    }();
    const std::filesystem::path directory = options.output_directory.empty()
        ? std::filesystem::current_path() / "maps" / prefix
        : options.output_directory;
    std::filesystem::create_directories(directory, error);
    if (error) {
        throw std::runtime_error("could not create Q3 output directory: "
                                 + error.message());
    }
    const std::filesystem::path definition_path = directory / (prefix + ".json");
    const std::filesystem::path level_path = directory / source.filename();
    const auto normalized_source = std::filesystem::absolute(source).lexically_normal();
    const auto normalized_level = std::filesystem::absolute(level_path).lexically_normal();
    if (normalized_source != normalized_level) {
        std::filesystem::copy_file(source, level_path,
                                   std::filesystem::copy_options::overwrite_existing,
                                   error);
        if (error) {
            throw std::runtime_error("could not copy Q3 source: "
                                     + error.message());
        }
    }

    MapDefinition definition;
    definition.name = room;
    definition.in_game_name = options.room_name.empty() ? map_name : options.room_name;
    definition.scale_factor = q3_scale_factor(reach, units_per_unit);
    definition.kill_height = std::round(drawn.min.z / units_per_unit) - 5.0F;
    definition.far_clip = std::round(std::min(400.0F,
                                              widest / units_per_unit * 1.2F));
    definition.import_source = level_path.filename().generic_string();
    definition.import_map_name = map_name;
    definition.import_units_per_unit = units_per_unit;
    definition.import_keep_sky = true;
    definition.import_keep_clip = !options.drop_clip;
    definition.import_keep_spawns = true;
    definition.import_textures = prefix + ".tex";
    definition.source_path = directory / (prefix + ".json");
    const auto baked = bake_q3_texture_pack(bsp, source, definition,
                                             options.texture_size);
    const std::filesystem::path texture_pack_path = directory
        / definition.import_textures;
    write_texture_pack(texture_pack_path, baked);

    std::vector<Vec3> starts;
    std::vector<Vec3> fallbacks;
    for (const Q3Entity& entity : bsp.entities) {
        const auto classname = entity.find("classname");
        const auto origin = entity_origin(entity);
        if (classname == entity.end() || !origin.has_value()) {
            continue;
        }
        const std::string type = lower(classname->second);
        if (type.rfind("info_player_deathmatch", 0) == 0
            || type == "info_player_start") {
            starts.push_back(*origin);
        } else if (type.rfind("target_", 0) == 0
                   || type == "info_player_intermission") {
            fallbacks.push_back(*origin);
        }
    }
    if (starts.size() < 4) {
        definition.import_keep_spawns = false;
        starts.insert(starts.end(), fallbacks.begin(), fallbacks.end());
        Vec3 centre{};
        for (const Vec3 point : starts) {
            centre.x += point.x;
            centre.y += point.y;
        }
        if (!starts.empty()) {
            centre.x /= static_cast<float>(starts.size());
            centre.y /= static_cast<float>(starts.size());
        }
        definition.spawns.reserve(starts.size());
        for (const Vec3 point : starts) {
            Spawn spawn;
            spawn.position = to_world(point, units_per_unit);
            spawn.position.y -= 24.0F / units_per_unit;
            spawn.yaw = std::atan2(centre.x - point.x, point.y - centre.y)
                * 180.0F / 3.14159265358979323846F;
            definition.spawns.push_back(spawn);
        }
    }
    write_q3_recipe(definition_path, definition);
    return {definition_path, level_path, texture_pack_path, room,
            units_per_unit, baked.size(),
            definition.import_keep_spawns ? starts.size() : definition.spawns.size()};
}

} // namespace fruityprime::mapgen::detail
