#include "raw_structs.hpp"

#include <algorithm>
#include <limits>
#include <stdexcept>
#include <string>
#include <type_traits>

namespace fruityprime::mapgen::raw_structs {
namespace {

template <typename Value>
void append_le(std::vector<std::uint8_t>& bytes, Value value) {
    using Unsigned = std::make_unsigned_t<Value>;
    const Unsigned raw_value = static_cast<Unsigned>(value);
    for (std::size_t shift = 0; shift < sizeof(Value); ++shift) {
        bytes.push_back(static_cast<std::uint8_t>(
            raw_value >> (shift * 8)));
    }
}

void append_fixed(std::vector<std::uint8_t>& bytes, formats::Fixed value) {
    append_le(bytes, value.value);
}

void append_vector(std::vector<std::uint8_t>& bytes,
                   formats::Vector3Fx value) {
    append_fixed(bytes, value.x);
    append_fixed(bytes, value.y);
    append_fixed(bytes, value.z);
}

void append_matrix(std::vector<std::uint8_t>& bytes,
                   formats::Matrix43Fx value) {
    append_vector(bytes, value.one);
    append_vector(bytes, value.two);
    append_vector(bytes, value.three);
    append_vector(bytes, value.four);
}

template <typename Value>
[[nodiscard]] Value checked_unsigned(int value, const char* field) {
    if (value < 0
        || static_cast<unsigned long long>(value)
               > std::numeric_limits<Value>::max()) {
        throw std::runtime_error(std::string("RawStructs ") + field
                                 + " is outside its fixed-width range");
    }
    return static_cast<Value>(value);
}

[[nodiscard]] std::int16_t checked_signed(int value, const char* field) {
    if (value < std::numeric_limits<std::int16_t>::min()
        || value > std::numeric_limits<std::int16_t>::max()) {
        throw std::runtime_error(std::string("RawStructs ") + field
                                 + " is outside its fixed-width range");
    }
    return static_cast<std::int16_t>(value);
}

} // namespace

raw::RawNode make_node(std::string_view name, int mesh_count, int first_mesh_id,
                       int parent, int child, int next) {
    raw::RawNode result;
    result.name.fill(0);
    const std::size_t count = std::min(name.size(), result.name.size() - 1);
    std::copy_n(name.begin(), count, result.name.begin());
    result.parent_id = checked_signed(parent, "parent");
    result.child_id = checked_signed(child, "child");
    result.next_id = checked_signed(next, "next");
    result.enabled = 1;
    result.mesh_count = checked_unsigned<std::uint16_t>(
        mesh_count, "mesh count");
    const auto first = checked_unsigned<std::uint32_t>(
        first_mesh_id, "first mesh id");
    if (first > std::numeric_limits<std::uint16_t>::max() / 2U) {
        throw std::runtime_error(
            "RawStructs first mesh id does not fit the model byte offset");
    }
    result.mesh_id = static_cast<std::uint16_t>(first * 2U);
    result.scale = {formats::Fixed{4096}, formats::Fixed{4096},
                    formats::Fixed{4096}};
    result.billboard_mode = formats::BillboardMode::None;
    // All other fields intentionally remain zero, just as the managed
    // MemoryStream writer leaves runtime transform tables for the loader.
    return result;
}

raw::RawMaterial make_material(std::string_view name, int texture_id,
                               int palette_id, formats::RepeatMode x_repeat,
                               formats::RepeatMode y_repeat, bool lighting,
                               formats::ColorRgb diffuse,
                               formats::ColorRgb ambient) {
    raw::RawMaterial result;
    result.name.fill(0);
    const std::size_t count = std::min(name.size(), result.name.size() - 1);
    std::copy_n(name.begin(), count, result.name.begin());
    result.lighting = lighting ? 1 : 0;
    result.culling = formats::CullingMode::Back;
    result.alpha = 31;
    result.palette_id = checked_signed(palette_id, "palette id");
    result.texture_id = checked_signed(texture_id, "texture id");
    result.x_repeat = x_repeat;
    result.y_repeat = y_repeat;
    result.diffuse = diffuse;
    result.ambient = ambient;
    result.specular = {};
    result.polygon_mode = formats::PolygonMode::Modulate;
    result.render_mode = formats::RenderMode::Normal;
    result.texcoord_transform_mode = formats::TexgenMode::None;
    result.scale_s = formats::Fixed{4096};
    result.scale_t = formats::Fixed{4096};
    return result;
}

raw::RawMesh make_mesh(int material_id, int display_list_id) {
    return raw::RawMesh{
        checked_unsigned<std::uint16_t>(material_id, "material id"),
        checked_unsigned<std::uint16_t>(display_list_id, "display list id")};
}

std::vector<std::uint8_t> encode(const raw::RawNode& value) {
    std::vector<std::uint8_t> bytes;
    bytes.reserve(sizeof(raw::RawNode));
    bytes.insert(bytes.end(), value.name.begin(), value.name.end());
    append_le(bytes, value.parent_id);
    append_le(bytes, value.child_id);
    append_le(bytes, value.next_id);
    append_le(bytes, value.padding_46);
    append_le(bytes, value.enabled);
    append_le(bytes, value.mesh_count);
    append_le(bytes, value.mesh_id);
    append_vector(bytes, value.scale);
    append_le(bytes, value.angle_x);
    append_le(bytes, value.angle_y);
    append_le(bytes, value.angle_z);
    append_le(bytes, value.padding_62);
    append_vector(bytes, value.position);
    append_fixed(bytes, value.bounding_radius);
    append_vector(bytes, value.min_bounds);
    append_vector(bytes, value.max_bounds);
    bytes.push_back(static_cast<std::uint8_t>(value.billboard_mode));
    bytes.push_back(value.padding_8d);
    append_le(bytes, value.padding_8e);
    append_matrix(bytes, value.transform);
    append_le(bytes, value.before_transform);
    append_le(bytes, value.after_transform);
    append_le(bytes, value.unused_c8);
    append_le(bytes, value.unused_cc);
    append_le(bytes, value.unused_d0);
    append_le(bytes, value.unused_d4);
    append_le(bytes, value.unused_d8);
    append_le(bytes, value.unused_dc);
    append_le(bytes, value.unused_e0);
    append_le(bytes, value.unused_e4);
    append_le(bytes, value.unused_e8);
    append_le(bytes, value.unused_ec);
    if (bytes.size() != sizeof(raw::RawNode)) {
        throw std::logic_error("RawStructs node encoder emitted an invalid size");
    }
    return bytes;
}

std::vector<std::uint8_t> encode(const raw::RawMaterial& value) {
    std::vector<std::uint8_t> bytes;
    bytes.reserve(sizeof(raw::RawMaterial));
    bytes.insert(bytes.end(), value.name.begin(), value.name.end());
    bytes.push_back(value.lighting);
    bytes.push_back(static_cast<std::uint8_t>(value.culling));
    bytes.push_back(value.alpha);
    bytes.push_back(value.wireframe);
    append_le(bytes, value.palette_id);
    append_le(bytes, value.texture_id);
    bytes.push_back(static_cast<std::uint8_t>(value.x_repeat));
    bytes.push_back(static_cast<std::uint8_t>(value.y_repeat));
    bytes.push_back(value.diffuse.red);
    bytes.push_back(value.diffuse.green);
    bytes.push_back(value.diffuse.blue);
    bytes.push_back(value.ambient.red);
    bytes.push_back(value.ambient.green);
    bytes.push_back(value.ambient.blue);
    bytes.push_back(value.specular.red);
    bytes.push_back(value.specular.green);
    bytes.push_back(value.specular.blue);
    bytes.push_back(value.padding_53);
    append_le(bytes, static_cast<std::uint32_t>(value.polygon_mode));
    bytes.push_back(static_cast<std::uint8_t>(value.render_mode));
    bytes.push_back(value.animation_flags);
    append_le(bytes, value.padding_5a);
    append_le(bytes, static_cast<std::uint32_t>(
        value.texcoord_transform_mode));
    append_le(bytes, value.texcoord_animation_id);
    append_le(bytes, value.padding_62);
    append_le(bytes, value.matrix_id);
    append_fixed(bytes, value.scale_s);
    append_fixed(bytes, value.scale_t);
    append_le(bytes, value.rotate_z);
    append_le(bytes, value.padding_72);
    append_fixed(bytes, value.translate_s);
    append_fixed(bytes, value.translate_t);
    append_le(bytes, value.material_animation_id);
    append_le(bytes, value.texture_animation_id);
    bytes.push_back(value.packed_repeat_mode);
    bytes.push_back(value.padding_81);
    append_le(bytes, value.padding_82);
    if (bytes.size() != sizeof(raw::RawMaterial)) {
        throw std::logic_error(
            "RawStructs material encoder emitted an invalid size");
    }
    return bytes;
}

std::vector<std::uint8_t> encode(const raw::RawMesh& value) {
    std::vector<std::uint8_t> bytes;
    bytes.reserve(sizeof(raw::RawMesh));
    append_le(bytes, value.material_id);
    append_le(bytes, value.dlist_id);
    return bytes;
}

} // namespace fruityprime::mapgen::raw_structs
