#include "map_packer.hpp"

#include "Formats/entity_format.hpp"
#include "Formats/model_format.hpp"
#include "raw_structs.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <fstream>
#include <limits>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

namespace fruityprime::mapgen::packer {
namespace {

[[nodiscard]] Vec3 subtract(Vec3 left, Vec3 right) noexcept {
    return {left.x - right.x, left.y - right.y, left.z - right.z};
}

[[nodiscard]] Vec3 multiply(Vec3 value, float factor) noexcept {
    return {value.x * factor, value.y * factor, value.z * factor};
}

[[nodiscard]] float length(Vec3 value) noexcept {
    return std::sqrt(value.x * value.x + value.y * value.y
                     + value.z * value.z);
}

[[nodiscard]] Vec3 normalized(Vec3 value) {
    const float size = length(value);
    if (!std::isfinite(size) || size <= std::numeric_limits<float>::epsilon()) {
        throw std::runtime_error("map vector must have a non-zero length");
    }
    return multiply(value, 1.0F / size);
}

[[nodiscard]] std::int32_t fixed_raw(float value) {
    // Fixed.ToInt is an explicit float-to-int conversion in the managed
    // writer.  It truncates; rounding here changes every negative fractional
    // coordinate and is not the C# serialization contract.
    return formats::Fixed::to_int(value);
}

[[nodiscard]] int round_to_even(float value) {
    if (!std::isfinite(value)) {
        return 0;
    }
    const double floor_value = std::floor(static_cast<double>(value));
    const double fraction = static_cast<double>(value) - floor_value;
    if (fraction < 0.5) {
        return static_cast<int>(floor_value);
    }
    if (fraction > 0.5) {
        return static_cast<int>(floor_value + 1.0);
    }
    const auto integer = static_cast<long long>(floor_value);
    return static_cast<int>((integer & 1LL) == 0 ? integer : integer + 1);
}

void append_u8(std::vector<std::uint8_t>& bytes, std::uint8_t value) {
    bytes.push_back(value);
}

void append_i8(std::vector<std::uint8_t>& bytes, std::int8_t value) {
    append_u8(bytes, static_cast<std::uint8_t>(value));
}

void append_u16(std::vector<std::uint8_t>& bytes, std::uint16_t value) {
    bytes.push_back(static_cast<std::uint8_t>(value));
    bytes.push_back(static_cast<std::uint8_t>(value >> 8));
}

void append_i16(std::vector<std::uint8_t>& bytes, std::int16_t value) {
    append_u16(bytes, static_cast<std::uint16_t>(value));
}

void append_u32(std::vector<std::uint8_t>& bytes, std::uint32_t value) {
    bytes.push_back(static_cast<std::uint8_t>(value));
    bytes.push_back(static_cast<std::uint8_t>(value >> 8));
    bytes.push_back(static_cast<std::uint8_t>(value >> 16));
    bytes.push_back(static_cast<std::uint8_t>(value >> 24));
}

void append_i32(std::vector<std::uint8_t>& bytes, std::int32_t value) {
    append_u32(bytes, static_cast<std::uint32_t>(value));
}

void append_name(std::vector<std::uint8_t>& bytes, std::string_view value,
                 std::size_t width) {
    const std::size_t count = std::min(width, value.size());
    bytes.insert(bytes.end(), value.begin(), value.begin() + count);
    bytes.insert(bytes.end(), width - count, 0);
}

void patch_u16(std::vector<std::uint8_t>& bytes, std::size_t offset,
               std::uint16_t value) {
    if (offset + 2 > bytes.size()) {
        throw std::runtime_error("map writer patch is outside the buffer");
    }
    bytes[offset] = static_cast<std::uint8_t>(value);
    bytes[offset + 1] = static_cast<std::uint8_t>(value >> 8);
}

void patch_u32(std::vector<std::uint8_t>& bytes, std::size_t offset,
               std::uint32_t value) {
    if (offset + 4 > bytes.size()) {
        throw std::runtime_error("map writer patch is outside the buffer");
    }
    bytes[offset] = static_cast<std::uint8_t>(value);
    bytes[offset + 1] = static_cast<std::uint8_t>(value >> 8);
    bytes[offset + 2] = static_cast<std::uint8_t>(value >> 16);
    bytes[offset + 3] = static_cast<std::uint8_t>(value >> 24);
}

[[nodiscard]] std::uint32_t offset32(std::size_t offset) {
    if (offset > std::numeric_limits<std::uint32_t>::max()) {
        throw std::runtime_error("generated map is larger than 4 GiB");
    }
    return static_cast<std::uint32_t>(offset);
}

void append_data_header(std::vector<std::uint8_t>& payload,
                        std::uint16_t type, std::int16_t id, Vec3 position,
                        Vec3 up, Vec3 facing) {
    append_u16(payload, type);
    append_i16(payload, id);
    for (const float value : {position.x, position.y, position.z,
                              up.x, up.y, up.z,
                              facing.x, facing.y, facing.z}) {
        append_i32(payload, fixed_raw(value));
    }
}

void append_box_volume(std::vector<std::uint8_t>& payload, Vec3 size) {
    append_u32(payload, 0); // VolumeType.Box
    append_i32(payload, fixed_raw(1.0F));
    append_i32(payload, 0);
    append_i32(payload, 0);
    append_i32(payload, 0);
    append_i32(payload, fixed_raw(1.0F));
    append_i32(payload, 0);
    append_i32(payload, 0);
    append_i32(payload, 0);
    append_i32(payload, fixed_raw(1.0F));
    append_i32(payload, fixed_raw(-size.x * 0.5F));
    append_i32(payload, 0);
    append_i32(payload, fixed_raw(-size.z * 0.5F));
    append_i32(payload, fixed_raw(size.x));
    append_i32(payload, fixed_raw(size.y));
    append_i32(payload, fixed_raw(size.z));
}

[[nodiscard]] std::pair<Vec3, float> jump_velocity(const JumpPad& pad) {
    if (pad.vector.has_value()) {
        if (!(pad.speed > 0.0F) || !std::isfinite(pad.speed)) {
            throw std::runtime_error("map jump pad vector requires positive speed");
        }
        return {normalized(*pad.vector), pad.speed};
    }
    if (!pad.target.has_value()) {
        throw std::runtime_error("map jump pad needs target or vector and speed");
    }
    const Vec3 delta = subtract(*pad.target, pad.position);
    const float horizontal = std::sqrt(delta.x * delta.x + delta.z * delta.z);
    constexpr float gravity = 77.0F / 4096.0F;
    const float rise = std::max(delta.y, 0.0F) + std::max(2.0F, horizontal * 0.22F);
    const float up = std::sqrt(2.0F * gravity * rise);
    const float fall = std::sqrt(2.0F * gravity
                                 * std::max(rise - delta.y, 0.01F));
    const float frames = (up + fall) / gravity;
    const Vec3 velocity{delta.x / frames, up, delta.z / frames};
    const float speed = length(velocity);
    if (!(speed > 0.0F) || !std::isfinite(speed)) {
        throw std::runtime_error("map jump pad target produces no velocity");
    }
    return {multiply(velocity, 1.0F / speed), speed};
}

void write_file(const std::filesystem::path& path,
                std::span<const std::uint8_t> bytes) {
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    if (!output) {
        throw std::runtime_error("could not create generated map file "
                                 + path.string());
    }
    if (!bytes.empty()) {
        output.write(reinterpret_cast<const char*>(bytes.data()),
                     static_cast<std::streamsize>(bytes.size()));
    }
    if (!output) {
        throw std::runtime_error("could not write generated map file "
                                 + path.string());
    }
}

[[nodiscard]] std::int16_t fixed_vertex(float value, float world_scale) {
    const std::int32_t raw = fixed_raw(value / world_scale);
    if (raw < std::numeric_limits<std::int16_t>::min()
        || raw > std::numeric_limits<std::int16_t>::max()) {
        throw std::runtime_error(
            "map vertex does not fit the 16-bit model coordinate; increase scaleFactor");
    }
    return static_cast<std::int16_t>(raw);
}


[[nodiscard]] std::uint32_t pack_color(float shade) {
    const auto channel = static_cast<std::uint32_t>(std::clamp(
        round_to_even(shade * 31.0F), 0, 31));
    return channel | (channel << 5) | (channel << 10);
}

[[nodiscard]] std::uint32_t pack_texcoord(
    const std::array<float, 2>& texcoord) {
    const auto component = [](float value) {
        if (!std::isfinite(value)) {
            throw std::runtime_error("map texture coordinate is not finite");
        }
        const auto rounded = std::clamp(static_cast<long long>(round_to_even(
            value * 16.0F)),
            static_cast<long long>(std::numeric_limits<std::int16_t>::min()),
            static_cast<long long>(std::numeric_limits<std::int16_t>::max()));
        return static_cast<std::uint16_t>(static_cast<std::int16_t>(rounded));
    };
    return static_cast<std::uint32_t>(component(texcoord[0]))
        | (static_cast<std::uint32_t>(component(texcoord[1])) << 16);
}

[[nodiscard]] std::uint32_t pack_normal(Vec3 normal) {
    const auto component = [](float value) {
        const auto rounded = std::clamp(round_to_even(value * 512.0F),
                                       -512, 511);
        return static_cast<std::uint32_t>(rounded) & 0x3ffU;
    };
    return component(normal.x) | (component(normal.y) << 10)
        | (component(normal.z) << 20);
}

struct Instruction {
    std::uint8_t code = 0;
    std::vector<std::uint32_t> arguments;
};

[[nodiscard]] std::vector<std::uint8_t> encode_instructions(
    const std::vector<Instruction>& instructions) {
    std::vector<std::uint8_t> result;
    for (std::size_t start = 0; start < instructions.size(); start += 4) {
        std::uint32_t packed = 0;
        for (std::size_t i = 0; i < 4; ++i) {
            const std::uint8_t code = start + i < instructions.size()
                ? instructions[start + i].code : 0;
            packed |= static_cast<std::uint32_t>(code) << (8 * i);
        }
        append_u32(result, packed);
        for (std::size_t i = 0; i < 4 && start + i < instructions.size(); ++i) {
            for (const std::uint32_t argument : instructions[start + i].arguments) {
                append_u32(result, argument);
            }
        }
    }
    return result;
}

struct RawDisplayList {
    std::vector<std::uint8_t> bytes;
    Vec3 min;
    Vec3 max;
    std::size_t faces = 0;
};
} // namespace

[[nodiscard]] std::vector<std::uint8_t> build_model(
    const MapDefinition& definition, const std::vector<BuiltFace>& faces,
    const ModelInfo& model,
    BuildStats& stats) {
    const float world_scale = std::ldexp(1.0F, definition.scale_factor);
    const std::size_t material_count = model.materials.size();
    if (material_count == 0) {
        throw std::runtime_error("a map needs at least one material");
    }
    std::vector<std::vector<const BuiltFace*>> by_material(material_count);
    for (const BuiltFace& face : faces) {
        // The managed Assemble method groups with Where(f.Material == id),
        // so an out-of-range face is not emitted rather than rejected here.
        if (face.material >= 0
            && static_cast<std::size_t>(face.material) < material_count) {
            by_material[static_cast<std::size_t>(face.material)].push_back(&face);
        }
    }

    std::uint64_t primitive_count = 0;
    std::uint64_t vertex_count = 0;
    std::vector<RawDisplayList> display_lists;
    for (const auto& material_faces : by_material) {
        if (material_faces.empty()) {
            continue;
        }
        RawDisplayList display;
        display.min = {std::numeric_limits<float>::max(),
                       std::numeric_limits<float>::max(),
                       std::numeric_limits<float>::max()};
        display.max = {std::numeric_limits<float>::lowest(),
                       std::numeric_limits<float>::lowest(),
                       std::numeric_limits<float>::lowest()};
        std::vector<const BuiltFace*> triangles;
        std::vector<const BuiltFace*> quads;
        std::size_t fan_count = 0;
        for (const BuiltFace* face : material_faces) {
            if (face->points.size() > 4) {
                fan_count += face->points.size() - 2;
            }
        }
        std::vector<BuiltFace> fans;
        fans.reserve(fan_count);
        triangles.reserve(material_faces.size());
        quads.reserve(material_faces.size());
        for (const BuiltFace* face : material_faces) {
            if (face->points.size() == 3) {
                triangles.push_back(face);
            } else if (face->points.size() == 4) {
                quads.push_back(face);
            } else if (face->points.size() > 4) {
                for (std::size_t point = 1;
                     point + 1 < face->points.size(); ++point) {
                    BuiltFace fan;
                    fan.points = {face->points[0], face->points[point],
                                  face->points[point + 1]};
                    fan.texcoords = {face->texcoords.at(0),
                                     face->texcoords.at(point),
                                     face->texcoords.at(point + 1)};
                    fan.normal = face->normal;
                    fan.material = face->material;
                    fan.shade = face->shade;
                    fan.damaging = face->damaging;
                    fan.flags = face->flags;
                    fan.has_texcoords = face->has_texcoords;
                    fans.push_back(std::move(fan));
                }
            }
            // Like C#'s Where/SelectMany sequence, polygons with fewer than
            // three points produce no model primitive.
        }
        // MapPacker.Assemble emits the three source sequences separately:
        // all original triangles, then all quads, then SelectMany(Fan).
        // Keep fan triangles out of the first sequence so an interleaved
        // input polygon list cannot change the managed display-list order.
        for (const BuiltFace& fan : fans) {
            triangles.push_back(&fan);
        }
        std::vector<Instruction> instructions;
        const auto emit = [&](const std::vector<const BuiltFace*>& group,
                              std::uint32_t primitive_type) {
            if (group.empty()) {
                return;
            }
            instructions.push_back({0x40, {primitive_type}});
            for (const BuiltFace* face : group) {
                instructions.push_back({0x20, {pack_color(face->shade)}});
                instructions.push_back({0x21, {pack_normal(face->normal)}});
                for (std::size_t point_index = 0;
                     point_index < face->points.size(); ++point_index) {
                    const Vec3 point = face->points[point_index];
                    const std::int16_t x = fixed_vertex(point.x, world_scale);
                    const std::int16_t y = fixed_vertex(point.y, world_scale);
                    const std::int16_t z = fixed_vertex(point.z, world_scale);
                    // C# emits TEXCOORD for every vertex, even when its
                    // material does not reference a texture.
                    instructions.push_back({0x22, {pack_texcoord(
                        face->texcoords[point_index])}});
                    instructions.push_back({0x23, {
                        static_cast<std::uint32_t>(static_cast<std::uint16_t>(x))
                            | (static_cast<std::uint32_t>(static_cast<std::uint16_t>(y))
                               << 16),
                        static_cast<std::uint16_t>(z)}});
                    const Vec3 world_point{
                        static_cast<float>(x) * world_scale / 4096.0F,
                        static_cast<float>(y) * world_scale / 4096.0F,
                        static_cast<float>(z) * world_scale / 4096.0F};
                    display.min.x = std::min(display.min.x, world_point.x);
                    display.min.y = std::min(display.min.y, world_point.y);
                    display.min.z = std::min(display.min.z, world_point.z);
                    display.max.x = std::max(display.max.x, world_point.x);
                    display.max.y = std::max(display.max.y, world_point.y);
                    display.max.z = std::max(display.max.z, world_point.z);
                    ++vertex_count;
                }
                ++primitive_count;
                ++display.faces;
            }
            instructions.push_back({0x41, {}}); // END
        };
        // This is the exact call order in MapPacker.EmitPrimitives.
        emit(triangles, 0);
        emit(quads, 1);
        display.bytes = encode_instructions(instructions);
        display_lists.push_back(std::move(display));
    }

    std::vector<std::uint8_t> result(::fruityprime::model::Header::Size, 0);
    // With no node matrix IDs and an ordinary room, PackModel writes one
    // node index for every node that owns meshes.  The generated hierarchy
    // below has only geo1 (node 1) in that set.
    if (!display_lists.empty()) {
        append_u32(result, 1);
    }

    std::uint32_t texture_offset = 0;
    std::vector<std::uint32_t> texture_image_offsets;
    std::vector<std::uint32_t> palette_data_offsets;
    if (model.textures.size() > std::numeric_limits<std::uint16_t>::max()
        || model.palettes.size() > std::numeric_limits<std::uint16_t>::max()) {
        throw std::runtime_error("map has too many generated textures");
    }
    texture_image_offsets.reserve(model.textures.size());
    for (const TextureInfo& texture : model.textures) {
        const std::size_t pixel_count = static_cast<std::size_t>(texture.width)
            * texture.height;
        const std::size_t entries_per_byte = texture.format == 0 ? 4
            : texture.format == 1 ? 2 : 1;
        const std::size_t bytes_per_entry = texture.format == 5 ? 2 : 1;
        if (texture.width == 0 || texture.height == 0
            || pixel_count % entries_per_byte != 0
            || (pixel_count / entries_per_byte) >
                std::numeric_limits<std::size_t>::max() / bytes_per_entry
            || texture.data.size() !=
                (pixel_count / entries_per_byte) * bytes_per_entry) {
            throw std::runtime_error(
                "map texture dimensions or data are invalid");
        }
        texture_image_offsets.push_back(offset32(result.size()));
        result.insert(result.end(), texture.data.begin(), texture.data.end());
    }
    texture_offset = model.textures.empty() ? 0 : offset32(result.size());
    for (std::size_t i = 0; i < model.textures.size(); ++i) {
        const TextureInfo& texture = model.textures[i];
        append_u8(result, texture.format);
        append_u8(result, 0);
        append_u16(result, texture.width);
        append_u16(result, texture.height);
        append_u16(result, 0);
        append_u32(result, texture_image_offsets[i]);
        append_u32(result, static_cast<std::uint32_t>(texture.data.size()));
        append_u32(result, 0);
        append_u32(result, 0);
        append_u32(result, 0);
        append_u32(result, texture.opaque ? 1U : 0U);
        append_u32(result, 0);
        append_u8(result, 0);
        append_u8(result, 0);
        append_u16(result, 0);
    }

    palette_data_offsets.reserve(model.palettes.size());
    for (const PaletteInfo& palette : model.palettes) {
        if (palette.data.size() > std::numeric_limits<std::uint16_t>::max()) {
            throw std::runtime_error("map palette has too many colors");
        }
        palette_data_offsets.push_back(offset32(result.size()));
        for (const std::uint16_t color : palette.data) {
            append_u16(result, color);
        }
    }
    const std::uint32_t palette_offset = model.palettes.empty()
        ? 0 : offset32(result.size());
    for (std::size_t i = 0; i < model.palettes.size(); ++i) {
        const PaletteInfo& palette = model.palettes[i];
        append_u32(result, palette_data_offsets[i]);
        append_u32(result, static_cast<std::uint32_t>(
            palette.data.size() * sizeof(std::uint16_t)));
        append_u32(result, 0);
        append_u32(result, 0);
    }

    std::vector<std::pair<std::uint32_t, std::uint32_t>> dlist_results;
    dlist_results.reserve(display_lists.size());
    for (const RawDisplayList& display : display_lists) {
        const std::uint32_t offset = offset32(result.size());
        result.insert(result.end(), display.bytes.begin(), display.bytes.end());
        dlist_results.emplace_back(offset, offset32(display.bytes.size()));
    }
    const std::uint32_t display_list_offset = display_lists.empty()
        ? 0 : offset32(result.size());
    for (std::size_t i = 0; i < display_lists.size(); ++i) {
        const RawDisplayList& display = display_lists[i];
        append_u32(result, dlist_results[i].first);
        append_u32(result, dlist_results[i].second);
        append_i32(result, fixed_raw(display.min.x));
        append_i32(result, fixed_raw(display.min.y));
        append_i32(result, fixed_raw(display.min.z));
        append_i32(result, fixed_raw(display.max.x));
        append_i32(result, fixed_raw(display.max.y));
        append_i32(result, fixed_raw(display.max.z));
    }

    const std::uint32_t material_offset = offset32(result.size());
    for (std::size_t i = 0; i < material_count; ++i) {
        const MaterialInfo& material = model.materials[i];
        if (material.texture_id < -1 || material.palette_id < -1
            || (material.texture_id >= 0
                && static_cast<std::size_t>(material.texture_id)
                    >= model.textures.size())
            || (material.palette_id >= 0
                && static_cast<std::size_t>(material.palette_id)
                    >= model.palettes.size())) {
            throw std::runtime_error("map material texture or palette index is outside the model");
        }
        const auto raw = raw_structs::make_material(
            material.name, material.texture_id, material.palette_id,
            formats::RepeatMode::Repeat, formats::RepeatMode::Repeat, false,
            {31, 31, 31}, {0, 0, 0});
        const auto encoded = raw_structs::encode(raw);
        result.insert(result.end(), encoded.begin(), encoded.end());
    }

    const std::uint32_t node_offset = offset32(result.size());
    Vec3 model_min{};
    Vec3 model_max{};
    if (!display_lists.empty()) {
        model_min = {std::numeric_limits<float>::max(),
                     std::numeric_limits<float>::max(),
                     std::numeric_limits<float>::max()};
        model_max = {std::numeric_limits<float>::lowest(),
                     std::numeric_limits<float>::lowest(),
                     std::numeric_limits<float>::lowest()};
        for (const RawDisplayList& display : display_lists) {
            model_min.x = std::min(model_min.x, display.min.x);
            model_min.y = std::min(model_min.y, display.min.y);
            model_min.z = std::min(model_min.z, display.min.z);
            model_max.x = std::max(model_max.x, display.max.x);
            model_max.y = std::max(model_max.y, display.max.y);
            model_max.z = std::max(model_max.z, display.max.z);
        }
    }
    const auto set_bounds = [&](raw::RawNode& node) {
        node.min_bounds = {
            formats::Fixed{fixed_raw(model_min.x)},
            formats::Fixed{fixed_raw(model_min.y)},
            formats::Fixed{fixed_raw(model_min.z)}};
        node.max_bounds = {
            formats::Fixed{fixed_raw(model_max.x)},
            formats::Fixed{fixed_raw(model_max.y)},
            formats::Fixed{fixed_raw(model_max.z)}};
    };
    auto root = raw_structs::make_node("rmMain", 0, 0, -1, 1);
    auto geometry = raw_structs::make_node(
        "geo1", static_cast<int>(display_lists.size()), 0, 0, -1);
    set_bounds(root);
    set_bounds(geometry);
    for (const auto& node : {root, geometry}) {
        const auto encoded = raw_structs::encode(node);
        result.insert(result.end(), encoded.begin(), encoded.end());
    }

    const std::uint32_t mesh_offset = offset32(result.size());
    std::size_t display_index = 0;
    for (std::size_t material = 0; material < by_material.size(); ++material) {
        if (by_material[material].empty()) {
            continue;
        }
        const auto mesh = raw_structs::make_mesh(
            static_cast<int>(material), static_cast<int>(display_index++));
        const auto encoded = raw_structs::encode(mesh);
        result.insert(result.end(), encoded.begin(), encoded.end());
    }

    if (display_lists.size() > std::numeric_limits<std::uint16_t>::max()
        || material_count > std::numeric_limits<std::uint16_t>::max()
        || faces.size() > std::numeric_limits<std::uint32_t>::max()) {
        throw std::runtime_error("map model has too many records");
    }
    patch_u32(result, 0, static_cast<std::uint32_t>(definition.scale_factor));
    patch_u32(result, 4, 4096);
    if (primitive_count > std::numeric_limits<std::uint32_t>::max()
        || vertex_count > std::numeric_limits<std::uint32_t>::max()) {
        throw std::runtime_error("map model has too many primitive vertices");
    }
    patch_u32(result, 8, static_cast<std::uint32_t>(primitive_count));
    patch_u32(result, 12, static_cast<std::uint32_t>(vertex_count));
    patch_u32(result, 16, material_offset);
    patch_u32(result, 20, display_list_offset);
    patch_u32(result, 24, node_offset);
    patch_u32(result, 36, mesh_offset);
    if (!model.textures.empty()) {
        patch_u16(result, 40, static_cast<std::uint16_t>(model.textures.size()));
        patch_u32(result, 44, texture_offset);
    }
    if (!model.palettes.empty()) {
        patch_u16(result, 48, static_cast<std::uint16_t>(model.palettes.size()));
        patch_u32(result, 52, palette_offset);
    }
    patch_u16(result, 72, static_cast<std::uint16_t>(material_count));
    patch_u16(result, 74, 2);
    patch_u16(result, 96, static_cast<std::uint16_t>(display_lists.size()));
    stats.model_faces = faces.size();
    stats.model_vertices = static_cast<std::size_t>(vertex_count);
    return result;
}

std::vector<std::uint8_t> build_entities(const MapDefinition& definition,
                                         BuildStats& stats) {
    struct Record {
        std::vector<std::uint8_t> payload;
        std::size_t entry_offset = 0;
    };
    std::vector<Record> records;
    std::int16_t id = 0;
    const Vec3 up{0.0F, 1.0F, 0.0F};
    for (const Spawn& spawn : definition.spawns) {
        const float radians = spawn.yaw * 3.14159265358979323846F / 180.0F;
        const Vec3 facing{std::sin(radians), 0.0F, std::cos(radians)};
        Record record;
        append_data_header(record.payload, 2, id++, spawn.position, up, facing);
        append_u8(record.payload, 0); // availability
        append_u8(record.payload, 1); // active
        append_i8(record.payload, -1); // team index
        records.push_back(std::move(record));
    }
    for (const JumpPad& pad : definition.jump_pads) {
        const auto [beam, speed] = jump_velocity(pad);
        Record record;
        append_data_header(record.payload, 9, id++, pad.position, up,
                           {0.0F, 0.0F, 1.0F});
        append_i32(record.payload, -1); // parent at offset 40
        append_i32(record.payload, 0); // alignment/padding before volume
        append_box_volume(record.payload, pad.size);
        append_i32(record.payload, fixed_raw(beam.x));
        append_i32(record.payload, fixed_raw(beam.y));
        append_i32(record.payload, fixed_raw(beam.z));
        append_i32(record.payload, fixed_raw(speed));
        append_u16(record.payload, pad.control_lock_time);
        append_u16(record.payload, pad.cooldown_time);
        append_u8(record.payload, 1); // active
        append_u8(record.payload, 0);
        append_u8(record.payload, 0);
        append_u8(record.payload, 0);
        append_u32(record.payload, pad.model_id);
        append_u32(record.payload, 0); // beam type
        append_u32(record.payload, 0x00000007); // player biped/alt/include bots
        if (record.payload.size() != 148) {
            throw std::logic_error("native jump pad writer emitted an invalid payload");
        }
        records.push_back(std::move(record));
    }
    for (const Item& item : definition.items) {
        const int item_type = item_type_from_name(item.type);
        if (item_type < 0) {
            throw std::runtime_error("unknown or story-only map item type: "
                                     + item.type);
        }
        Record record;
        append_data_header(record.payload, 4, id++, item.position, up,
                           {0.0F, 0.0F, 1.0F});
        append_i32(record.payload, -1);
        append_i32(record.payload, item_type);
        append_u8(record.payload, 1); // enabled
        append_u8(record.payload, item.has_base ? 1 : 0);
        append_u8(record.payload, 1); // always active
        append_u8(record.payload, 0);
        append_u16(record.payload, 0); // max spawn count
        append_u16(record.payload, item.spawn_interval);
        append_u16(record.payload, 0); // spawn delay
        append_i16(record.payload, -1); // notify entity
        append_u32(record.payload, 0); // collected message
        append_i32(record.payload, 0);
        append_i32(record.payload, 0);
        if (record.payload.size() != 72) {
            throw std::logic_error("native item writer emitted an invalid payload");
        }
        records.push_back(std::move(record));
    }
    if (records.empty()) {
        throw std::runtime_error("a map needs at least one entity");
    }
    if (records.size() > std::numeric_limits<std::uint16_t>::max()
        || records.size() > std::numeric_limits<std::int16_t>::max()) {
        throw std::runtime_error("map has too many entities");
    }

    std::vector<std::uint8_t> result(::fruityprime::entity::Header::Size, 0);
    patch_u32(result, 0, 2);
    patch_u16(result, 4, static_cast<std::uint16_t>(records.size()));
    for (Record& record : records) {
        record.entry_offset = result.size();
        append_name(result, "rmMain", 16);
        append_u16(result, 0xffff);
        append_u16(result, static_cast<std::uint16_t>(record.payload.size()));
        append_u32(result, 0); // payload offset patched below
    }
    result.insert(result.end(), 24, 0); // terminator
    for (Record& record : records) {
        const std::uint32_t payload_offset = offset32(result.size());
        patch_u32(result, record.entry_offset + 20, payload_offset);
        result.insert(result.end(), record.payload.begin(), record.payload.end());
    }
    stats.entities = records.size();
    return result;
}

void write_generated(const MapDefinition& definition,
                     const GeneratedMap& generated,
                     const std::filesystem::path& archive_directory,
                     const std::filesystem::path& entity_directory,
                     const std::filesystem::path& node_directory) {
    std::error_code error;
    std::filesystem::create_directories(archive_directory, error);
    if (error) {
        throw std::runtime_error("could not create map output directory "
                                 + archive_directory.string() + ": "
                                 + error.message());
    }
    error.clear();
    std::filesystem::create_directories(entity_directory, error);
    if (error) {
        throw std::runtime_error("could not create map output directory "
                                 + entity_directory.string() + ": "
                                 + error.message());
    }
    error.clear();
    std::filesystem::create_directories(node_directory, error);
    if (error) {
        throw std::runtime_error("could not create map output directory "
                                 + node_directory.string() + ": "
                                 + error.message());
    }
    const std::string prefix = file_prefix(definition);
    write_file(archive_directory / (prefix + "_Model.bin"), generated.model);
    write_file(archive_directory / (prefix + "_Anim.bin"), generated.animation);
    write_file(archive_directory / (prefix + "_Collision.bin"), generated.collision);
    write_file(entity_directory / (prefix + "_Ent.bin"), generated.entities);
    write_file(node_directory / (prefix + "_Node.bin"), generated.nodes);
}

} // namespace fruityprime::mapgen::packer
