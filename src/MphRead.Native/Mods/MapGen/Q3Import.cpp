#include "q3_import.hpp"

#include <zlib.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <limits>
#include <map>
#include <optional>
#include <span>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>

#include "image_decode.hpp"
#include "map_bundle.hpp"
#include "map_texture_bake.hpp"
#include "map_texture_pack.hpp"
#include "q3_bsp.hpp"

namespace fruityprime::mapgen::detail {
namespace {

constexpr std::uint32_t ZipEndSignature = 0x06054b50U;
constexpr std::uint32_t ZipCentralSignature = 0x02014b50U;
constexpr std::uint32_t ZipLocalSignature = 0x04034b50U;

[[nodiscard]] std::string lower(std::string_view value) {
    std::string result;
    result.reserve(value.size());
    for (const char character : value) {
        result.push_back(static_cast<char>(std::tolower(
            static_cast<unsigned char>(character))));
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
        throw std::runtime_error("Q3/ZIP read exceeds the input");
    }
    return static_cast<std::uint16_t>(bytes[offset])
        | static_cast<std::uint16_t>(bytes[offset + 1] << 8);
}

[[nodiscard]] std::uint32_t read_u32(std::span<const std::uint8_t> bytes,
                                     std::size_t offset) {
    if (offset > bytes.size() || bytes.size() - offset < 4) {
        throw std::runtime_error("Q3/ZIP read exceeds the input");
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
            throw std::runtime_error("could not read Q3 source " + path.string());
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
        || stream.total_out != output_size || stream.total_in != compressed.size()) {
        throw std::runtime_error("invalid or truncated ZIP deflate stream");
    }
    output.resize(output_size);
    return output;
}

[[nodiscard]] std::vector<ZipEntry> read_zip_entries(
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
        || static_cast<std::uint64_t>(central_offset) + central_size > bytes.size()) {
        throw std::runtime_error("multi-disk or malformed Q3 ZIP archive");
    }
    std::vector<ZipEntry> entries;
    std::size_t cursor = central_offset;
    for (std::size_t index = 0; index < total_count; ++index) {
        if (cursor > bytes.size() || bytes.size() - cursor < 46
            || read_u32(bytes, cursor) != ZipCentralSignature) {
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
            throw std::runtime_error("Q3 ZIP central directory entry is truncated");
        }
        ZipEntry entry;
        entry.name.assign(reinterpret_cast<const char*>(bytes.data() + cursor + 46),
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
    const std::size_t local = entry.local_offset;
    if (local > archive.size() || archive.size() - local < 30
        || read_u32(archive, local) != ZipLocalSignature) {
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
            throw std::runtime_error("Q3 ZIP stored entry has mismatched sizes");
        }
        output.assign(compressed.begin(), compressed.end());
    } else if (entry.method == 8) {
        output = inflate_raw(compressed, entry.uncompressed_size);
    } else {
        throw std::runtime_error("Q3 ZIP uses an unsupported compression method");
    }
    if (crc32(0, reinterpret_cast<const Bytef*>(output.data()),
              static_cast<uInt>(output.size())) != entry.crc) {
        throw std::runtime_error("Q3 ZIP entry CRC does not match");
    }
    return output;
}

[[nodiscard]] std::filesystem::path resolve_source(
    const MapDefinition& definition) {
    if (definition.import_source.empty()) {
        throw std::runtime_error("map import source is empty");
    }
    if (bundle::is_bundle(definition.source_path)) {
        std::error_code error;
        if (std::filesystem::is_regular_file(definition.source_path, error)
            && !error) {
            return std::filesystem::absolute(definition.source_path);
        }
    }
    const std::filesystem::path source(definition.import_source);
    std::vector<std::filesystem::path> candidates;
    if (source.is_absolute()) {
        candidates.push_back(source);
    } else {
        if (!definition.source_path.empty()) {
            candidates.push_back(definition.source_path.parent_path() / source);
        }
        candidates.push_back(std::filesystem::current_path() / source);
        candidates.push_back(std::filesystem::current_path() / "maps" / source);
    }
    for (const auto& candidate : candidates) {
        std::error_code error;
        if (std::filesystem::is_regular_file(candidate, error)) {
            return std::filesystem::absolute(candidate);
        }
    }
    throw std::runtime_error("could not resolve Q3 import source: "
                             + definition.import_source);
}

[[nodiscard]] std::string archive_name(std::string_view value) {
    std::string result;
    result.reserve(value.size());
    for (const char character : value) {
        result.push_back(static_cast<char>(std::tolower(
            static_cast<unsigned char>(character == '\\' ? '/' : character))));
    }
    return result;
}

[[nodiscard]] const ZipEntry* find_image(
    const std::vector<ZipEntry>& entries, std::string_view shader) {
    static constexpr std::array<std::string_view, 8> suffixes{{
        "", "_1", "_2", "_ft", "_bk", "_lf", "_rt", "_up"
    }};
    static constexpr std::array<std::string_view, 4> extensions{{
        ".tga", ".jpg", ".jpeg", ".png"
    }};
    for (const std::string_view suffix : suffixes) {
        for (const std::string_view extension : extensions) {
            const std::string wanted = archive_name(
                std::string(shader) + std::string(suffix)
                    + std::string(extension));
            for (const ZipEntry& entry : entries) {
                if (archive_name(entry.name) == wanted) {
                    return &entry;
                }
            }
        }
    }
    return nullptr;
}

[[nodiscard]] TextureBakeResult bake_texture_pack_with_report(
    const Q3Bsp& bsp, const std::filesystem::path& source,
    const MapDefinition& definition, int texture_size = 64) {
    std::vector<std::uint8_t> archive_bytes;
    std::vector<ZipEntry> entries;
    if (!ends_with(source.extension().string(), ".bsp")) {
        archive_bytes = read_file(source);
        const std::span<const std::uint8_t> archive(archive_bytes);
        entries = read_zip_entries(archive);
    }
    const auto load_image = [&](std::string_view shader)
        -> std::optional<image::RgbImage> {
        const ZipEntry* image_entry = find_image(entries, shader);
        if (image_entry == nullptr) {
            return std::nullopt;
        }
#ifndef _WIN32
        // The portable native build has no image dependency. Keep its Q3
        // geometry/collision path usable when a PK3 contains JPG/PNG art;
        // explicit FPTX packs still work on every platform.
        if (!ends_with(image_entry->name, ".tga")) {
            return std::nullopt;
        }
#endif
        const std::span<const std::uint8_t> archive(archive_bytes);
        const auto raw = zip_file(archive, *image_entry);
        return image::decode(raw, image_entry->name);
    };
    const texture_bake::BakeResult baked =
        texture_bake::bake_q3_textures_with_report(
            bsp, definition, texture_size, load_image);
    return {std::move(baked.entries), std::move(baked.missing)};
}

[[nodiscard]] std::vector<TexturePackEntry> bake_texture_pack(
    const Q3Bsp& bsp, const std::filesystem::path& source,
    const MapDefinition& definition, int texture_size = 64) {
    return bake_texture_pack_with_report(
        bsp, source, definition, texture_size).entries;
}

[[nodiscard]] Vec3 add(Vec3 left, Vec3 right) noexcept {
    return {left.x + right.x, left.y + right.y, left.z + right.z};
}

[[nodiscard]] Vec3 subtract(Vec3 left, Vec3 right) noexcept {
    return {left.x - right.x, left.y - right.y, left.z - right.z};
}

[[nodiscard]] Vec3 multiply(Vec3 value, float factor) noexcept {
    return {value.x * factor, value.y * factor, value.z * factor};
}

[[nodiscard]] float dot(Vec3 left, Vec3 right) noexcept {
    return left.x * right.x + left.y * right.y + left.z * right.z;
}

[[nodiscard]] Vec3 cross(Vec3 left, Vec3 right) noexcept {
    return {left.y * right.z - left.z * right.y,
            left.z * right.x - left.x * right.z,
            left.x * right.y - left.y * right.x};
}

[[nodiscard]] float length(Vec3 value) noexcept {
    return std::sqrt(dot(value, value));
}

[[nodiscard]] Vec3 normalize_or(Vec3 value, Vec3 fallback) {
    const float size = length(value);
    if (!std::isfinite(size) || size <= 0.0001F) {
        return fallback;
    }
    return multiply(value, 1.0F / size);
}

[[nodiscard]] Vec3 to_world(Vec3 value, float units_per_unit) {
    if (!(units_per_unit > 0.0F) || !std::isfinite(units_per_unit)) {
        throw std::runtime_error("Q3 unitsPerUnit must be positive and finite");
    }
    return {value.x / units_per_unit, value.z / units_per_unit,
            -value.y / units_per_unit};
}

[[nodiscard]] Vec3 to_direction(Vec3 value) {
    return normalize_or({value.x, value.z, -value.y}, {0.0F, 1.0F, 0.0F});
}

[[nodiscard]] std::optional<Vec3> parse_vector(std::string_view text) {
    std::istringstream stream{std::string(text)};
    Vec3 value;
    if (!(stream >> value.x >> value.y >> value.z)) {
        return std::nullopt;
    }
    return value;
}

[[nodiscard]] std::optional<float> parse_float(std::string_view text) {
    std::istringstream stream{std::string(text)};
    float value = 0.0F;
    if (!(stream >> value) || !std::isfinite(value)) {
        return std::nullopt;
    }
    return value;
}

[[nodiscard]] int match_material(const MapDefinition& definition,
                                 std::string_view shader) {
    const std::string shader_lower = lower(shader);
    int result = definition.import_default_material;
    std::size_t best_length = 0;
    for (const auto& [prefix, material] : definition.import_shader_materials) {
        const std::string prefix_lower = lower(prefix);
        if (shader_lower.rfind(prefix_lower, 0) == 0
            && prefix_lower.size() >= best_length) {
            result = material;
            best_length = prefix_lower.size();
        }
    }
    return result;
}

[[nodiscard]] float shade_from_vertices(const std::vector<Q3Vertex>& vertices,
                                        std::span<const std::int32_t> indices,
                                        std::size_t start, std::size_t count) {
    if (count == 0) {
        return 1.0F;
    }
    float total = 0.0F;
    for (std::size_t i = 0; i < count; ++i) {
        const auto& color = vertices[static_cast<std::size_t>(indices[start + i])].color;
        total += (static_cast<float>(color[0]) + color[1] + color[2])
            / (3.0F * 255.0F);
    }
    return std::clamp(0.62F + total / static_cast<float>(count) * 0.75F,
                      0.55F, 1.0F);
}

void wind(ImportedFace& face) {
    if (face.points.size() < 3) {
        return;
    }
    const Vec3 wound = cross(subtract(face.points[1], face.points[0]),
                             subtract(face.points[2], face.points[0]));
    if (dot(wound, face.normal) < 0.0F) {
        std::swap(face.points[1], face.points[2]);
        if (face.texcoords.size() >= 3) {
            std::swap(face.texcoords[1], face.texcoords[2]);
        }
    }
}

[[nodiscard]] ImportedFace make_triangle(const Q3Bsp& bsp, const Q3Face& source,
                                         const std::array<std::int32_t, 3>& indices,
                                         float units_per_unit, int material,
                                         const TexturePackEntry* texture) {
    ImportedFace face;
    face.points.resize(3);
    face.texcoords.resize(3);
    Vec3 normal{};
    for (std::size_t i = 0; i < 3; ++i) {
        const std::int32_t index = indices[i];
        if (index < 0 || static_cast<std::size_t>(index) >= bsp.vertices.size()) {
            throw std::runtime_error("Q3 face vertex index is outside the vertex lump");
        }
        const Q3Vertex& vertex = bsp.vertices[static_cast<std::size_t>(index)];
        face.points[i] = to_world(vertex.position, units_per_unit);
        if (texture != nullptr) {
            face.has_texcoords = true;
            face.texcoords[i] = {vertex.surface_s * texture->width,
                                 vertex.surface_t * texture->height};
        }
        normal = add(normal, to_direction(vertex.normal));
    }
    face.normal = normalize_or(normal, to_direction(source.normal));
    face.material = material;
    face.shade = shade_from_vertices(
        bsp.vertices, std::span<const std::int32_t>(indices), 0, indices.size());
    wind(face);
    return face;
}

[[nodiscard]] ImportedFace make_patch_triangle(
    const std::array<Vec3, 3>& points,
    const std::array<std::array<float, 2>, 3>& texcoords,
    Vec3 normal, int material, float shade, bool has_texcoords) {
    ImportedFace face;
    face.points.assign(points.begin(), points.end());
    face.texcoords.assign(texcoords.begin(), texcoords.end());
    face.has_texcoords = has_texcoords;
    face.normal = normalize_or(normal, {0.0F, 1.0F, 0.0F});
    face.material = material;
    face.shade = std::clamp(shade, 0.0F, 1.0F);
    wind(face);
    return face;
}

[[nodiscard]] std::vector<ImportedFace> tessellate_patch(
    const Q3Bsp& bsp, const Q3Face& source, float units_per_unit, int material,
    int requested_level, const TexturePackEntry* texture) {
    const int width = source.size[0];
    const int height = source.size[1];
    if (width < 3 || height < 3 || width % 2 == 0 || height % 2 == 0) {
        return {};
    }
    const int level = std::clamp(requested_level, 1, 8);
    std::vector<ImportedFace> result;
    const auto weights = [](float t) {
        const float inverse = 1.0F - t;
        return std::array<float, 3>{inverse * inverse, 2.0F * t * inverse,
                                    t * t};
    };
    for (int py = 0; py + 2 < height; py += 2) {
        for (int px = 0; px + 2 < width; px += 2) {
            struct Sample {
                Vec3 point;
                Vec3 normal;
                float shade = 1.0F;
                std::array<float, 2> texcoord{};
            };
            std::vector<Sample> samples(static_cast<std::size_t>(level + 1)
                                         * static_cast<std::size_t>(level + 1));
            const auto at = [&](int row, int column) -> const Q3Vertex& {
                const std::int64_t index = static_cast<std::int64_t>(source.vertex)
                    + static_cast<std::int64_t>(py + row) * width + px + column;
                if (index < 0 || static_cast<std::size_t>(index) >= bsp.vertices.size()) {
                    throw std::runtime_error("Q3 patch control point is outside vertices");
                }
                return bsp.vertices[static_cast<std::size_t>(index)];
            };
            for (int a = 0; a <= level; ++a) {
                const auto v = weights(static_cast<float>(a) / level);
                for (int b = 0; b <= level; ++b) {
                    const auto u = weights(static_cast<float>(b) / level);
                    Sample sample{};
                    for (int row = 0; row < 3; ++row) {
                        for (int column = 0; column < 3; ++column) {
                            const float weight = v[row] * u[column];
                            const Q3Vertex& vertex = at(row, column);
                            sample.point = add(sample.point,
                                               multiply(to_world(vertex.position,
                                                                 units_per_unit),
                                                        weight));
                            sample.normal = add(sample.normal,
                                                multiply(to_direction(vertex.normal),
                                                         weight));
                            if (texture != nullptr) {
                                sample.texcoord[0] += vertex.surface_s
                                    * texture->width * weight;
                                sample.texcoord[1] += vertex.surface_t
                                    * texture->height * weight;
                            }
                            sample.shade += ((static_cast<float>(vertex.color[0])
                                              + vertex.color[1] + vertex.color[2])
                                             / (3.0F * 255.0F) - 1.0F) * weight;
                        }
                    }
                    sample.normal = normalize_or(sample.normal,
                                                 to_direction(source.normal));
                    sample.shade = std::clamp(0.62F + sample.shade * 0.75F,
                                              0.55F, 1.0F);
                    samples[static_cast<std::size_t>(a * (level + 1) + b)] = sample;
                }
            }
            const auto sample_at = [&](int a, int b) -> const Sample& {
                return samples[static_cast<std::size_t>(a * (level + 1) + b)];
            };
            for (int a = 0; a < level; ++a) {
                for (int b = 0; b < level; ++b) {
                    const Sample& p00 = sample_at(a, b);
                    const Sample& p01 = sample_at(a, b + 1);
                    const Sample& p11 = sample_at(a + 1, b + 1);
                    const Sample& p10 = sample_at(a + 1, b);
                    result.push_back(make_patch_triangle(
                        {p00.point, p01.point, p11.point},
                        {p00.texcoord, p01.texcoord, p11.texcoord},
                        add(add(p00.normal, p01.normal), p11.normal), material,
                        (p00.shade + p01.shade + p11.shade) / 3.0F,
                        texture != nullptr));
                    result.push_back(make_patch_triangle(
                        {p00.point, p11.point, p10.point},
                        {p00.texcoord, p11.texcoord, p10.texcoord},
                        add(add(p00.normal, p11.normal), p10.normal), material,
                        (p00.shade + p11.shade + p10.shade) / 3.0F,
                        texture != nullptr));
                }
            }
        }
    }
    return result;
}

[[nodiscard]] std::vector<Vec3> make_sheet(Vec3 normal, float distance) {
    const Vec3 axis = std::abs(normal.z) < 0.9F
        ? Vec3{0.0F, 0.0F, 1.0F} : Vec3{1.0F, 0.0F, 0.0F};
    const Vec3 right = normalize_or(cross(axis, normal), {1.0F, 0.0F, 0.0F});
    const Vec3 up = normalize_or(cross(normal, right), {0.0F, 1.0F, 0.0F});
    const Vec3 centre = multiply(normal, distance);
    constexpr float extent = 65'536.0F;
    return {subtract(subtract(centre, multiply(right, extent)), multiply(up, extent)),
            add(subtract(centre, multiply(up, extent)), multiply(right, extent)),
            add(add(centre, multiply(right, extent)), multiply(up, extent)),
            add(subtract(centre, multiply(right, extent)), multiply(up, extent))};
}

[[nodiscard]] std::vector<Vec3> clip_polygon(const std::vector<Vec3>& points,
                                              Vec3 normal, float distance) {
    constexpr float epsilon = 0.01F;
    std::vector<Vec3> result;
    for (std::size_t i = 0; i < points.size(); ++i) {
        const Vec3 current = points[i];
        const Vec3 next = points[(i + 1) % points.size()];
        const float current_distance = dot(normal, current) - distance;
        const float next_distance = dot(normal, next) - distance;
        if (current_distance <= epsilon) {
            result.push_back(current);
        }
        if ((current_distance > epsilon) != (next_distance > epsilon)
            && std::abs(current_distance - next_distance) > 1e-6F) {
            const float fraction = current_distance
                / (current_distance - next_distance);
            result.push_back(add(current, multiply(subtract(next, current), fraction)));
        }
    }
    return result;
}

[[nodiscard]] std::vector<Vec3> weld(const std::vector<Vec3>& points) {
    std::vector<Vec3> result;
    for (const Vec3 point : points) {
        bool duplicate = false;
        for (const Vec3 previous : result) {
            const Vec3 difference = subtract(point, previous);
            if (dot(difference, difference) < 0.0004F) {
                duplicate = true;
                break;
            }
        }
        if (!duplicate) {
            result.push_back(point);
        }
    }
    return result;
}

struct BrushPolygon {
    std::vector<Vec3> points;
    Vec3 normal;
};

[[nodiscard]] std::vector<BrushPolygon> brush_polygons(
    const Q3Bsp& bsp, const Q3Brush& brush) {
    if (brush.first_side < 0 || brush.side_count < 0
        || static_cast<std::size_t>(brush.first_side) > bsp.brush_sides.size()
        || static_cast<std::size_t>(brush.side_count)
            > bsp.brush_sides.size() - static_cast<std::size_t>(brush.first_side)) {
        throw std::runtime_error("Q3 brush side range is outside the lump");
    }
    std::vector<BrushPolygon> result;
    for (int side_index = 0; side_index < brush.side_count; ++side_index) {
        const Q3BrushSide& side = bsp.brush_sides[
            static_cast<std::size_t>(brush.first_side + side_index)];
        if (side.plane < 0 || static_cast<std::size_t>(side.plane) >= bsp.planes.size()) {
            throw std::runtime_error("Q3 brush plane index is outside the lump");
        }
        const Q3Plane& plane = bsp.planes[static_cast<std::size_t>(side.plane)];
        std::vector<Vec3> points = make_sheet(plane.normal, plane.distance);
        for (int other_index = 0;
             other_index < brush.side_count && points.size() >= 3; ++other_index) {
            if (other_index == side_index) {
                continue;
            }
            const Q3BrushSide& other_side = bsp.brush_sides[
                static_cast<std::size_t>(brush.first_side + other_index)];
            if (other_side.plane < 0
                || static_cast<std::size_t>(other_side.plane) >= bsp.planes.size()) {
                throw std::runtime_error("Q3 brush plane index is outside the lump");
            }
            const Q3Plane& other = bsp.planes[static_cast<std::size_t>(other_side.plane)];
            points = clip_polygon(points, other.normal, other.distance);
        }
        points = weld(points);
        if (points.size() >= 3) {
            result.push_back({std::move(points), plane.normal});
        }
    }
    return result;
}

[[nodiscard]] bool inside(Vec3 point, Vec3 min, Vec3 max) {
    return point.x >= min.x && point.x <= max.x
        && point.y >= min.y && point.y <= max.y
        && point.z >= min.z && point.z <= max.z;
}

[[nodiscard]] int brush_cell(float value) {
    return static_cast<int>(std::floor(value / 512.0F));
}

struct BrushVolume {
    std::vector<std::array<float, 4>> planes;
    Vec3 min;
    Vec3 max;
};

[[nodiscard]] bool covered(Vec3 point, std::size_t owner,
                           const std::vector<BrushVolume>& brushes,
                           const std::map<std::array<int, 3>, std::vector<std::size_t>>& lookup) {
    const auto found = lookup.find({brush_cell(point.x), brush_cell(point.y),
                                    brush_cell(point.z)});
    if (found == lookup.end()) {
        return false;
    }
    for (const std::size_t index : found->second) {
        if (index == owner) {
            continue;
        }
        const BrushVolume& volume = brushes[index];
        if (point.x < volume.min.x - 1.0F || point.x > volume.max.x + 1.0F
            || point.y < volume.min.y - 1.0F || point.y > volume.max.y + 1.0F
            || point.z < volume.min.z - 1.0F || point.z > volume.max.z + 1.0F) {
            continue;
        }
        bool is_inside = true;
        for (const auto plane : volume.planes) {
            if (plane[0] * point.x + plane[1] * point.y + plane[2] * point.z
                - plane[3] > -0.1F) {
                is_inside = false;
                break;
            }
        }
        if (is_inside) {
            return true;
        }
    }
    return false;
}

[[nodiscard]] bool buried(const std::vector<Vec3>& points, Vec3 normal,
                          std::size_t owner, const std::vector<BrushVolume>& brushes,
                          const std::map<std::array<int, 3>, std::vector<std::size_t>>& lookup) {
    Vec3 centre{};
    for (const Vec3 point : points) {
        centre = add(centre, point);
    }
    centre = multiply(centre, 1.0F / static_cast<float>(points.size()));
    const Vec3 offset = normalize_or(normal, {0.0F, 1.0F, 0.0F});
    for (std::size_t i = 0; i < points.size(); ++i) {
        const Vec3 edge = multiply(add(points[i], points[(i + 1) % points.size()]),
                                   0.5F);
        const Vec3 corner_sample = add(
            add(points[i], multiply(subtract(centre, points[i]), 0.15F)), offset);
        const Vec3 edge_sample = add(
            add(edge, multiply(subtract(centre, edge), 0.15F)), offset);
        if (!covered(corner_sample, owner, brushes, lookup)
            || !covered(edge_sample, owner, brushes, lookup)) {
            return false;
        }
    }
    return covered(add(centre, offset), owner, brushes, lookup);
}

[[nodiscard]] std::optional<float> entity_angle(const Q3Entity& entity) {
    const auto found = entity.find("angle");
    return found == entity.end() ? std::nullopt : parse_float(found->second);
}

[[nodiscard]] std::optional<Vec3> entity_origin(const Q3Entity& entity) {
    const auto found = entity.find("origin");
    return found == entity.end() ? std::nullopt : parse_vector(found->second);
}

[[nodiscard]] std::string item_name(std::string_view classname) {
    const std::string value = lower(classname);
    static constexpr std::array<std::pair<std::string_view, std::string_view>, 21> map{{
        {"weapon_railgun", "Imperialist"},
        {"weapon_rocketlauncher", "Magmaul"},
        {"weapon_lightning", "ShockCoil"},
        {"weapon_plasmagun", "VoltDriver"},
        {"weapon_shotgun", "Battlehammer"},
        {"weapon_grenadelauncher", "Judicator"},
        {"weapon_bfg", "OmegaCannon"},
        {"item_quad", "DoubleDamage"},
        {"item_invis", "Cloak"},
        {"item_health", "HealthMedium"},
        {"item_health_small", "HealthSmall"},
        {"item_health_large", "HealthBig"},
        {"item_health_mega", "HealthBig"},
        {"item_armor_shard", "UASmall"},
        {"item_armor_combat", "UABig"},
        {"item_armor_body", "UABig"},
        {"ammo_rockets", "MissileBig"},
        {"ammo_slugs", "UABig"},
        {"ammo_cells", "UASmall"},
        {"ammo_shells", "UASmall"},
        {"ammo_bullets", "UASmall"}
    }};
    for (const auto& [source, target] : map) {
        if (value == source) {
            return std::string(target);
        }
    }
    if (value == "ammo_grenades") {
        return "MissileSmall";
    }
    if (value == "ammo_lightning") {
        return "UASmall";
    }
    return {};
}

void append_entities(const Q3Bsp& bsp, const MapDefinition& definition,
                     ImportedMap& result, float units_per_unit) {
    std::map<std::string, Vec3> targets;
    for (const Q3Entity& entity : bsp.entities) {
        const auto name = entity.find("targetname");
        const auto origin = entity_origin(entity);
        if (name != entity.end() && origin.has_value()) {
            targets[lower(name->second)] = to_world(*origin, units_per_unit);
        }
    }
    for (const Q3Entity& entity : bsp.entities) {
        const auto classname = entity.find("classname");
        if (classname == entity.end()) {
            continue;
        }
        const std::string type = lower(classname->second);
        const auto origin = entity_origin(entity);
        if ((type == "info_player_deathmatch" || type == "info_player_start")
            && definition.import_keep_spawns && origin.has_value()) {
            const float angle = entity_angle(entity).value_or(0.0F);
            Vec3 position = to_world(*origin, units_per_unit);
            position.y -= 24.0F / units_per_unit;
            result.spawns.push_back({position, 90.0F + angle});
            continue;
        }
        if (type == "trigger_push") {
            const auto model = entity.find("model");
            const auto target = entity.find("target");
            if (model == entity.end() || target == entity.end()
                || model->second.empty() || model->second.front() != '*') {
                continue;
            }
            const auto destination = targets.find(lower(target->second));
            if (destination == targets.end()) {
                continue;
            }
            int model_index = -1;
            try {
                model_index = std::stoi(model->second.substr(1));
            } catch (...) {
                continue;
            }
            if (model_index < 0 || static_cast<std::size_t>(model_index) >= bsp.models.size()) {
                continue;
            }
            const Q3Model& volume = bsp.models[static_cast<std::size_t>(model_index)];
            const Vec3 min = to_world(volume.mins, units_per_unit);
            const Vec3 max = to_world(volume.maxs, units_per_unit);
            const Vec3 low{std::min(min.x, max.x), std::min(min.y, max.y),
                           std::min(min.z, max.z)};
            const Vec3 high{std::max(min.x, max.x), std::max(min.y, max.y),
                            std::max(min.z, max.z)};
            result.jump_pads.push_back({
                {(low.x + high.x) * 0.5F, low.y, (low.z + high.z) * 0.5F},
                destination->second, std::nullopt, 0.0F,
                {std::max(high.x - low.x, 0.8F), std::max(high.y - low.y, 0.8F),
                 std::max(high.z - low.z, 0.8F)}});
            continue;
        }
        if (origin.has_value()) {
            const std::string item = item_name(classname->second);
            if (!item.empty()) {
                result.items.push_back({to_world(*origin, units_per_unit), item});
            }
        }
    }
}

[[nodiscard]] ImportedMap convert(const Q3Bsp& bsp, const MapDefinition& definition,
                                  const std::filesystem::path& source) {
    const float units_per_unit = definition.import_units_per_unit;
    ImportedMap result;
    std::optional<std::vector<TexturePackEntry>> texture_pack =
        texture_pack_io::load_optional(definition);
    if (!texture_pack.has_value() && !definition.import_textures.empty()) {
        // Match the managed converter's convenient first-run behavior: a
        // recipe can name the derived pack before it exists, in which case
        // bake the level's own image files directly from the PK3. The pack is
        // kept in memory for this build; an explicit FPTX remains the fast
        // and portable path for later launches and Android bundles.
        // Q3Import.BakeTextures returns null when no image was baked. Keep
        // that distinct from an already-resolved empty FPTX, which must not
        // be converted into a source/borrowed-texture fallback.
        auto baked = bake_texture_pack(bsp, source, definition);
        if (!baked.empty()) {
            texture_pack = std::move(baked);
        }
    }
    std::map<std::uint16_t, std::size_t> texture_materials;
    if (texture_pack.has_value()) {
        if (texture_pack->empty()) {
            throw std::runtime_error(
                definition.import_textures.empty()
                    ? definition.name
                        + " names no texture pack and maps no shaders onto a shipped "
                          "room's materials, so it has no materials at all."
                    : definition.name + " has no textures: "
                        + definition.import_textures
                        + " is not beside its recipe, not in its bundle, and could not be baked from the level.");
        }
        result.has_texture_pack = true;
        result.texture_pack = *texture_pack;
        result.materials.reserve(texture_pack->size());
        for (std::size_t index = 0; index < texture_pack->size(); ++index) {
            const TexturePackEntry& texture = (*texture_pack)[index];
            Material material;
            material.name = texture.name.empty()
                ? "q3_" + std::to_string(index) : texture.name;
            material.tex_scale = definition.import_tex_scale;
            result.materials.push_back(std::move(material));
            texture_materials.emplace(texture.source_index, index);
        }
    } else {
        std::size_t max_material = definition.materials.size();
        for (const auto& [_, value] : definition.import_shader_materials) {
            if (value >= 0) {
                max_material = std::max(max_material,
                                        static_cast<std::size_t>(value + 1));
            }
        }
        max_material = std::max<std::size_t>(1, max_material);
        if (definition.materials.empty()) {
            result.materials.reserve(max_material);
            for (std::size_t i = 0; i < max_material; ++i) {
                Material material;
                material.name = i == 0 ? "q3" : "q3_" + std::to_string(i);
                material.tex_scale = definition.import_tex_scale;
                result.materials.push_back(std::move(material));
            }
        } else {
            result.materials = definition.materials;
            while (result.materials.size() < max_material) {
                Material material;
                material.name = "q3_" + std::to_string(result.materials.size());
                material.tex_scale = definition.import_tex_scale;
                result.materials.push_back(std::move(material));
            }
        }
    }

    Vec3 drawn_min{std::numeric_limits<float>::max(),
                   std::numeric_limits<float>::max(),
                   std::numeric_limits<float>::max()};
    Vec3 drawn_max{std::numeric_limits<float>::lowest(),
                   std::numeric_limits<float>::lowest(),
                   std::numeric_limits<float>::lowest()};
    for (const Q3Face& source : bsp.faces) {
        if (source.texture < 0 || static_cast<std::size_t>(source.texture)
                >= bsp.textures.size()) {
            throw std::runtime_error("Q3 face texture index is outside the lump");
        }
        const Q3Texture& texture = bsp.textures[static_cast<std::size_t>(source.texture)];
        if (source.type != 1 && source.type != 2 && source.type != 3) {
            continue;
        }
        if ((texture.flags & (0x80 | 0x100 | 0x200)) != 0
            || ((texture.flags & 0x4) != 0 && !definition.import_keep_sky)) {
            continue;
        }
        int material = -1;
        const TexturePackEntry* packed_texture = nullptr;
        if (texture_pack.has_value()) {
            const auto packed = texture_materials.find(
                static_cast<std::uint16_t>(source.texture));
            if (packed == texture_materials.end()) {
                // A shader with no baked image of its own -- a light or an
                // effect defined by the Q3 shader scripts -- must not borrow
                // somebody else's texture.
                continue;
            }
            material = static_cast<int>(packed->second);
            packed_texture = &(*texture_pack)[packed->second];
        } else {
            material = match_material(definition, texture.name);
        }
        if (material < 0
            || static_cast<std::size_t>(material) >= result.materials.size()) {
            throw std::runtime_error("Q3 shader material index is outside materials");
        }
        std::vector<ImportedFace> generated;
        if (source.type == 2) {
            generated = tessellate_patch(bsp, source, units_per_unit, material,
                                         definition.import_patch_level,
                                         packed_texture);
        } else {
            if (source.mesh_vertex < 0 || source.mesh_vertex_count < 0
                || static_cast<std::size_t>(source.mesh_vertex)
                    > bsp.mesh_vertices.size()
                || static_cast<std::size_t>(source.mesh_vertex_count)
                    > bsp.mesh_vertices.size() - static_cast<std::size_t>(source.mesh_vertex)
                || source.mesh_vertex_count % 3 != 0) {
                throw std::runtime_error("Q3 face mesh-vertex range is invalid");
            }
            for (int index = 0; index + 2 < source.mesh_vertex_count; index += 3) {
                const std::size_t start = static_cast<std::size_t>(source.mesh_vertex + index);
                generated.push_back(make_triangle(
                    bsp, source,
                    {source.vertex + bsp.mesh_vertices[start],
                     source.vertex + bsp.mesh_vertices[start + 1],
                     source.vertex + bsp.mesh_vertices[start + 2]},
                    units_per_unit, material, packed_texture));
            }
        }
        const bool sky = (texture.flags & 0x4) != 0;
        for (ImportedFace& face : generated) {
            if (sky) {
                face.shade = 1.0F;
            }
            result.faces.push_back(face);
            if (!sky) {
                for (std::size_t i = 0; i < face.points.size(); ++i) {
                    drawn_min.x = std::min(drawn_min.x, face.points[i].x);
                    drawn_min.y = std::min(drawn_min.y, face.points[i].y);
                    drawn_min.z = std::min(drawn_min.z, face.points[i].z);
                    drawn_max.x = std::max(drawn_max.x, face.points[i].x);
                    drawn_max.y = std::max(drawn_max.y, face.points[i].y);
                    drawn_max.z = std::max(drawn_max.z, face.points[i].z);
                }
            }
        }
    }
    if (result.faces.empty()) {
        throw std::runtime_error("Q3 level has no drawable polygon surfaces");
    }
    if (drawn_min.x == std::numeric_limits<float>::max()) {
        drawn_min = to_world(bsp.models.empty() ? Vec3{} : bsp.models[0].mins,
                             units_per_unit);
        drawn_max = to_world(bsp.models.empty() ? Vec3{} : bsp.models[0].maxs,
                             units_per_unit);
    }
    const Vec3 margin{6.0F, 6.0F, 6.0F};
    const Vec3 keep_min = subtract(drawn_min, margin);
    const Vec3 keep_max = add(drawn_max, margin);

    const int first_brush = bsp.models.empty() ? 0 : bsp.models[0].brush;
    const int brush_count = bsp.models.empty()
        ? static_cast<int>(bsp.brushes.size()) : bsp.models[0].brush_count;
    if (first_brush < 0 || brush_count < 0
        || static_cast<std::size_t>(first_brush) > bsp.brushes.size()
        || static_cast<std::size_t>(brush_count)
            > bsp.brushes.size() - static_cast<std::size_t>(first_brush)) {
        throw std::runtime_error("Q3 world brush range is invalid");
    }

    struct Candidate {
        std::vector<BrushPolygon> polygons;
        BrushVolume volume;
    };
    std::vector<Candidate> candidates;
    for (int brush_index = first_brush; brush_index < first_brush + brush_count; ++brush_index) {
        const Q3Brush& brush = bsp.brushes[static_cast<std::size_t>(brush_index)];
        if (brush.texture < 0 || static_cast<std::size_t>(brush.texture) >= bsp.textures.size()) {
            throw std::runtime_error("Q3 brush texture index is outside the lump");
        }
        const Q3Texture& texture = bsp.textures[static_cast<std::size_t>(brush.texture)];
        const bool solid = (texture.contents & 0x1) != 0;
        const bool clip = (texture.contents & 0x10000) != 0;
        if ((!solid && clip && !definition.import_keep_clip) || (!solid && !clip)) {
            continue;
        }
        bool all_sky = brush.side_count > 0;
        for (int side = 0; side < brush.side_count; ++side) {
            const auto& brush_side = bsp.brush_sides[
                static_cast<std::size_t>(brush.first_side + side)];
            if (brush_side.texture < 0
                || static_cast<std::size_t>(brush_side.texture) >= bsp.textures.size()
                || (bsp.textures[static_cast<std::size_t>(brush_side.texture)].flags & 0x4) == 0) {
                all_sky = false;
                break;
            }
        }
        if (all_sky) {
            continue;
        }
        Candidate candidate;
        candidate.polygons = brush_polygons(bsp, brush);
        if (candidate.polygons.empty()) {
            continue;
        }
        candidate.volume.min = {std::numeric_limits<float>::max(),
                                std::numeric_limits<float>::max(),
                                std::numeric_limits<float>::max()};
        candidate.volume.max = {std::numeric_limits<float>::lowest(),
                                std::numeric_limits<float>::lowest(),
                                std::numeric_limits<float>::lowest()};
        for (const BrushPolygon& polygon : candidate.polygons) {
            for (const Vec3 point : polygon.points) {
                // The burial test uses the BSP planes, so keep its bounds in
                // Quake space as well. Converting only the polygon emitted
                // below would make a plane in Quake coordinates test a point
                // in MPH coordinates and leave every internal seam alive.
                candidate.volume.min.x = std::min(candidate.volume.min.x, point.x);
                candidate.volume.min.y = std::min(candidate.volume.min.y, point.y);
                candidate.volume.min.z = std::min(candidate.volume.min.z, point.z);
                candidate.volume.max.x = std::max(candidate.volume.max.x, point.x);
                candidate.volume.max.y = std::max(candidate.volume.max.y, point.y);
                candidate.volume.max.z = std::max(candidate.volume.max.z, point.z);
            }
        }
        bool in_level = false;
        for (const BrushPolygon& polygon : candidate.polygons) {
            for (const Vec3 point : polygon.points) {
                if (inside(to_world(point, units_per_unit), keep_min, keep_max)) {
                    in_level = true;
                    break;
                }
            }
            if (in_level) {
                break;
            }
        }
        if (in_level) {
            candidate.volume.planes.reserve(static_cast<std::size_t>(brush.side_count));
            for (int side = 0; side < brush.side_count; ++side) {
                const auto& brush_side = bsp.brush_sides[
                    static_cast<std::size_t>(brush.first_side + side)];
                const Q3Plane& plane = bsp.planes[static_cast<std::size_t>(brush_side.plane)];
                candidate.volume.planes.push_back({plane.normal.x, plane.normal.y,
                                                   plane.normal.z, plane.distance});
            }
            candidates.push_back(std::move(candidate));
        }
    }

    std::vector<BrushVolume> volumes;
    volumes.reserve(candidates.size());
    for (const Candidate& candidate : candidates) {
        volumes.push_back(candidate.volume);
    }
    std::map<std::array<int, 3>, std::vector<std::size_t>> lookup;
    for (std::size_t index = 0; index < volumes.size(); ++index) {
        const BrushVolume& volume = volumes[index];
        for (int x = brush_cell(volume.min.x); x <= brush_cell(volume.max.x); ++x) {
            for (int y = brush_cell(volume.min.y); y <= brush_cell(volume.max.y); ++y) {
                for (int z = brush_cell(volume.min.z); z <= brush_cell(volume.max.z); ++z) {
                    lookup[{x, y, z}].push_back(index);
                }
            }
        }
    }
    for (std::size_t index = 0; index < candidates.size(); ++index) {
        for (const BrushPolygon& polygon : candidates[index].polygons) {
            const Vec3 world_normal = to_direction(polygon.normal);
            const std::vector<Vec3> world_points = [&] {
                std::vector<Vec3> result_points;
                result_points.reserve(polygon.points.size());
                for (const Vec3 point : polygon.points) {
                    result_points.push_back(to_world(point, units_per_unit));
                }
                return result_points;
            }();
            if (buried(polygon.points, polygon.normal, index, volumes, lookup)) {
                continue;
            }
            // Keep the whole brush polygon here. The managed MapPacker decides
            // when a model or collision format needs a fan.
            if (world_points.size() >= 3) {
                ImportedFace face;
                face.points = world_points;
                face.texcoords.resize(face.points.size());
                face.normal = world_normal;
                face.material = 0;
                face.shade = 1.0F;
                result.solid_faces.push_back(face);
            }
        }
    }
    append_entities(bsp, definition, result, units_per_unit);
    return result;
}

} // namespace

ImportedMap import_q3(const MapDefinition& definition) {
    const std::filesystem::path source = resolve_source(definition);
    const std::optional<std::string_view> map_name =
        definition.import_map_name.empty()
        ? std::nullopt
        : std::optional<std::string_view>(definition.import_map_name);
    const std::vector<std::uint8_t> level =
        Q3Bsp::read_level(source, map_name);
    const Q3Bsp bsp = parse_bsp(level);
    return convert(bsp, definition, source);
}

std::vector<ShaderUsage> list_shader_usage(
    const std::filesystem::path& source, std::string_view map_name) {
    MapDefinition probe;
    probe.import_source = source.string();
    probe.import_map_name = std::string(map_name);
    const std::filesystem::path resolved = resolve_source(probe);
    const std::optional<std::string_view> selected_map = map_name.empty()
        ? std::nullopt : std::optional<std::string_view>(map_name);
    const std::vector<std::uint8_t> level =
        Q3Bsp::read_level(resolved, selected_map);
    const Q3Bsp bsp = parse_bsp(level);

    std::vector<ShaderUsage> result;
    std::map<std::string, std::size_t> positions;
    for (const Q3Face& face : bsp.faces) {
        if (face.type != 1 && face.type != 3) {
            continue;
        }
        if (face.texture < 0
            || static_cast<std::size_t>(face.texture) >= bsp.textures.size()) {
            throw std::runtime_error("Q3 face texture index is outside the lump");
        }
        const Q3Texture& texture = bsp.textures[
            static_cast<std::size_t>(face.texture)];
        if ((texture.flags & (0x80 | 0x4 | 0x100 | 0x200)) != 0) {
            continue;
        }
        if (face.mesh_vertex_count < 0) {
            throw std::runtime_error("Q3 face mesh-vertex count is negative");
        }
        const auto [iterator, inserted] = positions.emplace(
            texture.name, result.size());
        if (inserted) {
            result.push_back({texture.name, 0});
        }
        result[iterator->second].triangles += face.mesh_vertex_count / 3;
    }
    std::stable_sort(result.begin(), result.end(),
                     [](const ShaderUsage& left, const ShaderUsage& right) {
                         return left.triangles > right.triangles;
                     });
    return result;
}

std::vector<std::uint8_t> read_q3_level(
    const MapDefinition& definition, const std::filesystem::path& source) {
    const std::optional<std::string_view> map_name =
        definition.import_map_name.empty()
        ? std::nullopt
        : std::optional<std::string_view>(definition.import_map_name);
    return Q3Bsp::read_level(source, map_name);
}

std::vector<std::string> list_q3_maps(const std::filesystem::path& source) {
    return Q3Bsp::list_maps(source);
}

std::vector<TexturePackEntry> bake_q3_texture_pack(
    const Q3Bsp& bsp, const std::filesystem::path& source,
    const MapDefinition& definition, int texture_size) {
    return bake_texture_pack(bsp, source, definition, texture_size);
}

TextureBakeResult bake_q3_texture_pack_with_report(
    const Q3Bsp& bsp, const std::filesystem::path& source,
    const MapDefinition& definition, int texture_size) {
    return bake_texture_pack_with_report(
        bsp, source, definition, texture_size);
}

} // namespace fruityprime::mapgen::detail
