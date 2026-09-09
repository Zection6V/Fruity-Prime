/*
 * Native translation of the animation writer in MphRead/Utility/RepackModel.cs.
 *
 * Model parsing and animation playback live in Formats/Model.cpp.  This
 * utility owns the inverse serialization of the four animation group types,
 * including the First Hunt padding bytes and the managed fixed-point/angle
 * conversions.
 */
#include "Utility/repack_model.hpp"

#include "Formats/raw_formats.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <string>
#include <utility>

namespace fruityprime::utility::repack_model {
namespace {

class Writer {
public:
    explicit Writer(std::size_t reserve = 0) {
        bytes_.resize(reserve, 0);
    }

    [[nodiscard]] std::size_t position() const noexcept {
        return bytes_.size();
    }

    void u8(std::uint8_t value) { bytes_.push_back(value); }

    void u16(std::uint16_t value) {
        u8(static_cast<std::uint8_t>(value));
        u8(static_cast<std::uint8_t>(value >> 8));
    }

    void u32(std::uint32_t value) {
        u8(static_cast<std::uint8_t>(value));
        u8(static_cast<std::uint8_t>(value >> 8));
        u8(static_cast<std::uint8_t>(value >> 16));
        u8(static_cast<std::uint8_t>(value >> 24));
    }

    void fixed(float value) {
        const double scaled = static_cast<double>(value) * 4096.0;
        if (!std::isfinite(scaled)
            || scaled < static_cast<double>(std::numeric_limits<std::int32_t>::min())
            || scaled > static_cast<double>(std::numeric_limits<std::int32_t>::max())) {
            throw std::invalid_argument("model animation fixed value is out of range");
        }
        u32(static_cast<std::uint32_t>(static_cast<std::int32_t>(scaled)));
    }

    void angle(float value) {
        constexpr double pi = 3.1415926535897932384626433832795;
        const double turns = static_cast<double>(value) / (pi * 2.0)
            * 65536.0;
        // Math.Round(double) uses the default midpoint-to-even mode.  The
        // explicit unsigned conversion below preserves the DS 16-bit wrap.
        const auto rounded = static_cast<std::int64_t>(std::nearbyint(turns));
        u16(static_cast<std::uint16_t>(static_cast<std::uint64_t>(rounded)));
    }

    void raw(std::span<const std::uint8_t> values) {
        bytes_.insert(bytes_.end(), values.begin(), values.end());
    }

    void zeroes(std::size_t count) {
        bytes_.insert(bytes_.end(), count, 0);
    }

    void pad_to_4(std::uint8_t value) {
        while ((position() & 3U) != 0) {
            u8(value);
        }
    }

    [[nodiscard]] std::vector<std::uint8_t> take() && {
        return std::move(bytes_);
    }

private:
    std::vector<std::uint8_t> bytes_;
};

[[nodiscard]] std::uint32_t offset32(std::size_t value) {
    if (value > std::numeric_limits<std::uint32_t>::max()) {
        throw std::length_error("model animation is larger than 4 GiB");
    }
    return static_cast<std::uint32_t>(value);
}

[[nodiscard]] std::uint16_t count16(std::size_t value,
                                    const char* description) {
    if (value > std::numeric_limits<std::uint16_t>::max()) {
        throw std::length_error(std::string(description)
                                + " contains too many values");
    }
    return static_cast<std::uint16_t>(value);
}

[[nodiscard]] std::uint32_t count32(std::size_t value,
                                    const char* description) {
    if (value > std::numeric_limits<std::uint32_t>::max()) {
        throw std::length_error(std::string(description)
                                + " contains too many values");
    }
    return static_cast<std::uint32_t>(value);
}

void patch_u16(std::vector<std::uint8_t>& bytes, std::size_t offset,
               std::uint16_t value) {
    bytes[offset] = static_cast<std::uint8_t>(value);
    bytes[offset + 1] = static_cast<std::uint8_t>(value >> 8);
}

void patch_u32(std::vector<std::uint8_t>& bytes, std::size_t offset,
               std::uint32_t value) {
    bytes[offset] = static_cast<std::uint8_t>(value);
    bytes[offset + 1] = static_cast<std::uint8_t>(value >> 8);
    bytes[offset + 2] = static_cast<std::uint8_t>(value >> 16);
    bytes[offset + 3] = static_cast<std::uint8_t>(value >> 24);
}

int require_same_group_count(const model::AnimationResults& animations) {
    const std::size_t count = animations.node_groups.size();
    if (animations.node_group_offsets.size() != count
        || animations.material_groups.size() != count
        || animations.material_group_offsets.size() != count
        || animations.texcoord_groups.size() != count
        || animations.texcoord_group_offsets.size() != count
        || animations.texture_groups.size() != count
        || animations.texture_group_offsets.size() != count) {
        throw std::invalid_argument(
            "model animation group and offset lists have different lengths");
    }
    if (count > std::numeric_limits<std::uint16_t>::max()) {
        throw std::length_error("model contains too many animation groups");
    }
    return static_cast<int>(count);
}

[[nodiscard]] std::uint32_t write_node_group(
    Writer& writer, const model::NodeAnimationGroup& group,
    bool first_hunt_padding) {
    const std::uint16_t pad_short = first_hunt_padding ? 0xccccU : 0;
    const std::uint32_t scale_offset = offset32(writer.position());
    for (const float value : group.scales) {
        writer.fixed(value);
    }
    const std::uint32_t rotate_offset = offset32(writer.position());
    for (const float value : group.rotations) {
        writer.angle(value);
    }
    while ((writer.position() & 3U) != 0) {
        writer.u16(pad_short);
    }
    const std::uint32_t translate_offset = offset32(writer.position());
    for (const float value : group.translations) {
        writer.fixed(value);
    }
    const std::uint32_t animation_offset = offset32(writer.position());
    for (const auto& [name, animation] : group.animations) {
        static_cast<void>(name);
        writer.u8(animation.scale_blend_x);
        writer.u8(animation.scale_blend_y);
        writer.u8(animation.scale_blend_z);
        writer.u8(animation.flags);
        writer.u16(animation.scale_lut_length_x);
        writer.u16(animation.scale_lut_length_y);
        writer.u16(animation.scale_lut_length_z);
        writer.u16(animation.scale_lut_index_x);
        writer.u16(animation.scale_lut_index_y);
        writer.u16(animation.scale_lut_index_z);
        writer.u8(animation.rotate_blend_x);
        writer.u8(animation.rotate_blend_y);
        writer.u8(animation.rotate_blend_z);
        writer.u8(animation.padding_13);
        writer.u16(animation.rotate_lut_length_x);
        writer.u16(animation.rotate_lut_length_y);
        writer.u16(animation.rotate_lut_length_z);
        writer.u16(animation.rotate_lut_index_x);
        writer.u16(animation.rotate_lut_index_y);
        writer.u16(animation.rotate_lut_index_z);
        writer.u8(animation.translate_blend_x);
        writer.u8(animation.translate_blend_y);
        writer.u8(animation.translate_blend_z);
        writer.u8(animation.padding_23);
        writer.u16(animation.translate_lut_length_x);
        writer.u16(animation.translate_lut_length_y);
        writer.u16(animation.translate_lut_length_z);
        writer.u16(animation.translate_lut_index_x);
        writer.u16(animation.translate_lut_index_y);
        writer.u16(animation.translate_lut_index_z);
    }
    const std::uint32_t group_offset = offset32(writer.position());
    writer.u32(group.frame_count);
    writer.u32(scale_offset);
    writer.u32(rotate_offset);
    writer.u32(translate_offset);
    writer.u32(animation_offset);
    return group_offset;
}

[[nodiscard]] std::uint32_t write_material_group(
    Writer& writer, const model::MaterialAnimationGroup& group,
    bool first_hunt_padding) {
    const std::uint8_t pad_byte = first_hunt_padding ? 0xccU : 0;
    const std::uint32_t color_offset = offset32(writer.position());
    for (const float value : group.colors) {
        writer.u8(static_cast<std::uint8_t>(value));
    }
    writer.pad_to_4(pad_byte);
    const std::uint32_t animation_offset = offset32(writer.position());
    for (const auto& [name, animation] : group.animations) {
        static_cast<void>(name);
        writer.raw(animation.name);
        writer.u32(animation.unused_40);
        writer.u8(animation.diffuse_blend_r);
        writer.u8(animation.diffuse_blend_g);
        writer.u8(animation.diffuse_blend_b);
        writer.u8(animation.unused_47);
        writer.u16(animation.diffuse_lut_length_r);
        writer.u16(animation.diffuse_lut_length_g);
        writer.u16(animation.diffuse_lut_length_b);
        writer.u16(animation.diffuse_lut_index_r);
        writer.u16(animation.diffuse_lut_index_g);
        writer.u16(animation.diffuse_lut_index_b);
        writer.u8(animation.ambient_blend_r);
        writer.u8(animation.ambient_blend_g);
        writer.u8(animation.ambient_blend_b);
        writer.u8(animation.unused_57);
        writer.u16(animation.ambient_lut_length_r);
        writer.u16(animation.ambient_lut_length_g);
        writer.u16(animation.ambient_lut_length_b);
        writer.u16(animation.ambient_lut_index_r);
        writer.u16(animation.ambient_lut_index_g);
        writer.u16(animation.ambient_lut_index_b);
        writer.u8(animation.specular_blend_r);
        writer.u8(animation.specular_blend_g);
        writer.u8(animation.specular_blend_b);
        writer.u8(animation.unused_67);
        writer.u16(animation.specular_lut_length_r);
        writer.u16(animation.specular_lut_length_g);
        writer.u16(animation.specular_lut_length_b);
        writer.u16(animation.specular_lut_index_r);
        writer.u16(animation.specular_lut_index_g);
        writer.u16(animation.specular_lut_index_b);
        writer.u32(animation.unused_74);
        writer.u32(animation.unused_78);
        writer.u32(animation.unused_7c);
        writer.u32(animation.unused_80);
        writer.u8(animation.alpha_blend);
        writer.u8(animation.unused_85);
        writer.u16(animation.alpha_lut_length);
        writer.u16(animation.alpha_lut_index);
        writer.u16(animation.material_id);
    }
    const std::uint32_t group_offset = offset32(writer.position());
    writer.u32(group.frame_count);
    writer.u32(color_offset);
    writer.u32(count32(group.animations.size(), "material animations"));
    writer.u32(animation_offset);
    writer.u16(group.current_frame);
    writer.u16(group.unused_frame);
    return group_offset;
}

[[nodiscard]] std::uint32_t write_texcoord_group(
    Writer& writer, const model::TexcoordAnimationGroup& group,
    bool first_hunt_padding) {
    const std::uint16_t pad_short = first_hunt_padding ? 0xccccU : 0;
    const std::uint32_t scale_offset = offset32(writer.position());
    for (const float value : group.scales) {
        writer.fixed(value);
    }
    const std::uint32_t rotate_offset = offset32(writer.position());
    for (const float value : group.rotations) {
        writer.angle(value);
    }
    while ((writer.position() & 3U) != 0) {
        writer.u16(pad_short);
    }
    const std::uint32_t translate_offset = offset32(writer.position());
    for (const float value : group.translations) {
        writer.fixed(value);
    }
    const std::uint32_t animation_offset = offset32(writer.position());
    for (const auto& [name, animation] : group.animations) {
        static_cast<void>(name);
        writer.raw(animation.name);
        writer.u8(animation.scale_blend_s);
        writer.u8(animation.scale_blend_t);
        writer.u16(animation.scale_lut_length_s);
        writer.u16(animation.scale_lut_length_t);
        writer.u16(animation.scale_lut_index_s);
        writer.u16(animation.scale_lut_index_t);
        writer.u8(animation.rotate_blend_z);
        writer.u8(animation.unused_2b);
        writer.u16(animation.rotate_lut_length_z);
        writer.u16(animation.rotate_lut_index_z);
        writer.u8(animation.translate_blend_s);
        writer.u8(animation.translate_blend_t);
        writer.u16(animation.translate_lut_length_s);
        writer.u16(animation.translate_lut_length_t);
        writer.u16(animation.translate_lut_index_s);
        writer.u16(animation.translate_lut_index_t);
        writer.u16(animation.padding_3a);
    }
    const std::uint32_t group_offset = offset32(writer.position());
    writer.u32(group.frame_count);
    writer.u32(scale_offset);
    writer.u32(rotate_offset);
    writer.u32(translate_offset);
    writer.u32(count32(group.animations.size(), "texcoord animations"));
    writer.u32(animation_offset);
    writer.u16(group.current_frame);
    writer.u16(group.unused_frame);
    return group_offset;
}

[[nodiscard]] std::uint32_t write_texture_group(
    Writer& writer, const model::TextureAnimationGroup& group,
    bool first_hunt_padding) {
    const std::uint16_t pad_short = first_hunt_padding ? 0xccccU : 0;
    const std::uint32_t frame_offset = offset32(writer.position());
    for (const std::uint16_t value : group.frame_indices) {
        writer.u16(value);
    }
    const std::uint32_t texture_offset = offset32(writer.position());
    for (const std::uint16_t value : group.texture_ids) {
        writer.u16(value);
    }
    const std::uint32_t palette_offset = offset32(writer.position());
    for (const std::uint16_t value : group.palette_ids) {
        writer.u16(value);
    }
    while ((writer.position() & 3U) != 0) {
        writer.u16(pad_short);
    }
    const std::uint32_t animation_offset = offset32(writer.position());
    for (const auto& [name, animation] : group.animations) {
        static_cast<void>(name);
        writer.raw(animation.name);
        writer.u16(animation.count);
        writer.u16(animation.start_index);
        writer.u16(animation.minimum_palette_id);
        writer.u16(animation.material_id);
        writer.u16(animation.minimum_texture_id);
        writer.u16(animation.field_2a);
    }
    const std::uint32_t group_offset = offset32(writer.position());
    writer.u16(count16(group.frame_count, "texture animation frames"));
    writer.u16(count16(group.frame_indices.size(), "texture frame indices"));
    writer.u16(count16(group.texture_ids.size(), "texture IDs"));
    writer.u16(count16(group.palette_ids.size(), "palette IDs"));
    writer.u16(count16(group.animations.size(), "texture animations"));
    writer.u16(group.unused_a);
    writer.u32(frame_offset);
    writer.u32(texture_offset);
    writer.u32(palette_offset);
    writer.u32(animation_offset);
    writer.u16(group.current_frame);
    writer.u16(group.unused_frame);
    return group_offset;
}

struct Int3 {
    std::int32_t x = 0;
    std::int32_t y = 0;
    std::int32_t z = 0;
};

struct IntBounds {
    Int3 min{
        std::numeric_limits<std::int32_t>::max(),
        std::numeric_limits<std::int32_t>::max(),
        std::numeric_limits<std::int32_t>::max()};
    Int3 max{
        std::numeric_limits<std::int32_t>::min(),
        std::numeric_limits<std::int32_t>::min(),
        std::numeric_limits<std::int32_t>::min()};
};

void write_name(Writer& writer, const std::string& name, std::size_t width) {
    if (name.size() > width) {
        throw std::invalid_argument("model name is longer than its fixed field");
    }
    for (const unsigned char value : name) {
        writer.u8(value);
    }
    writer.zeroes(width - name.size());
}

void write_vector3(Writer& writer, const formats::Vector3Fx& value) {
    writer.u32(static_cast<std::uint32_t>(value.x.value));
    writer.u32(static_cast<std::uint32_t>(value.y.value));
    writer.u32(static_cast<std::uint32_t>(value.z.value));
}

void write_texture_meta(Writer& writer, const model::Texture& texture,
                        std::uint32_t image_offset,
                        std::uint32_t image_size) {
    writer.u8(texture.format);
    writer.u8(0);
    writer.u16(texture.width);
    writer.u16(texture.height);
    writer.u16(0);
    writer.u32(image_offset);
    writer.u32(image_size);
    writer.u32(0); // UnusedOffset
    writer.u32(0); // UnusedCount
    writer.u32(0); // VramOffset
    writer.u32(texture.opaque);
    writer.u32(0); // SkipVram
    writer.u8(0); // PackedSize
    writer.u8(0); // NativeTextureFormat
    writer.u16(0); // ObjectRef
}

void write_palette_meta(Writer& writer, std::uint32_t offset,
                        std::uint32_t size) {
    writer.u32(offset);
    writer.u32(size);
    writer.u32(0); // VramOffset
    writer.u32(0); // ObjectRef
}

std::uint32_t texture_matrix_id(const model::Material& material,
                                std::uint32_t& count) {
    // This is the same special case as Repack.GetTextureMatrixId: a material
    // without texgen and with the identity transform uses matrix zero without
    // consuming an entry in the matrix table.
    if (material.scale_s.value == 4096
        && material.scale_t.value == 4096
        && material.rotate_z == 0
        && material.translate_s.value == 0
        && material.translate_t.value == 0
        && material.texcoord_transform_mode
            == static_cast<std::uint32_t>(formats::TexgenMode::None)) {
        return 0;
    }
    const std::uint32_t result = count;
    if (count == std::numeric_limits<std::uint16_t>::max()) {
        throw std::length_error("model contains too many texture matrices");
    }
    ++count;
    return result;
}

void write_material(Writer& writer, const model::Material& material,
                    std::uint32_t matrix_id) {
    write_name(writer, material.name, 64);
    writer.u8(material.lighting);
    writer.u8(material.culling);
    writer.u8(material.alpha);
    writer.u8(material.wireframe);
    writer.u16(static_cast<std::uint16_t>(material.palette_id));
    writer.u16(static_cast<std::uint16_t>(material.texture_id));
    writer.u8(material.x_repeat);
    writer.u8(material.y_repeat);
    writer.u8(material.diffuse.red);
    writer.u8(material.diffuse.green);
    writer.u8(material.diffuse.blue);
    writer.u8(material.ambient.red);
    writer.u8(material.ambient.green);
    writer.u8(material.ambient.blue);
    writer.u8(material.specular.red);
    writer.u8(material.specular.green);
    writer.u8(material.specular.blue);
    writer.u8(0);
    writer.u32(material.polygon_mode);
    writer.u8(material.render_mode);
    writer.u8(material.animation_flags);
    writer.u16(0);
    writer.u32(material.texcoord_transform_mode);
    writer.u16(material.texcoord_animation_id);
    writer.u16(0);
    writer.u32(matrix_id);
    writer.u32(static_cast<std::uint32_t>(material.scale_s.value));
    writer.u32(static_cast<std::uint32_t>(material.scale_t.value));
    writer.u16(material.rotate_z);
    writer.u16(0);
    writer.u32(static_cast<std::uint32_t>(material.translate_s.value));
    writer.u32(static_cast<std::uint32_t>(material.translate_t.value));
    writer.u16(material.material_animation_id);
    writer.u16(material.texture_animation_id);
    writer.u8(0);
    writer.u8(0);
    writer.u16(0);
}

void write_node(Writer& writer, const model::Node& node,
                const Int3& min_bounds, const Int3& max_bounds) {
    write_name(writer, node.name, 64);
    writer.u16(static_cast<std::uint16_t>(node.parent_id));
    writer.u16(static_cast<std::uint16_t>(node.child_id));
    writer.u16(static_cast<std::uint16_t>(node.next_id));
    writer.u16(0);
    writer.u32(node.enabled);
    writer.u16(node.mesh_count);
    writer.u16(node.mesh_id);
    write_vector3(writer, node.scale);
    writer.u16(static_cast<std::uint16_t>(node.angle_x));
    writer.u16(static_cast<std::uint16_t>(node.angle_y));
    writer.u16(static_cast<std::uint16_t>(node.angle_z));
    writer.u16(0);
    write_vector3(writer, node.position);
    writer.u32(static_cast<std::uint32_t>(node.bounding_radius.value));
    writer.u32(static_cast<std::uint32_t>(min_bounds.x));
    writer.u32(static_cast<std::uint32_t>(min_bounds.y));
    writer.u32(static_cast<std::uint32_t>(min_bounds.z));
    writer.u32(static_cast<std::uint32_t>(max_bounds.x));
    writer.u32(static_cast<std::uint32_t>(max_bounds.y));
    writer.u32(static_cast<std::uint32_t>(max_bounds.z));
    writer.u8(static_cast<std::uint8_t>(node.billboard_mode));
    writer.u8(0);
    writer.u16(0);
    writer.zeroes(12 * sizeof(std::uint32_t)); // transform MtxFx43
    writer.zeroes(12 * sizeof(std::uint32_t)); // Before/After and unused
}

std::size_t instruction_arity(model::InstructionCode code) {
    switch (code) {
    case model::InstructionCode::Nop:
    case model::InstructionCode::EndVtxs:
        return 0;
    case model::InstructionCode::MtxRestore:
    case model::InstructionCode::Color:
    case model::InstructionCode::Normal:
    case model::InstructionCode::Texcoord:
    case model::InstructionCode::Vtx10:
    case model::InstructionCode::VtxXy:
    case model::InstructionCode::VtxXz:
    case model::InstructionCode::VtxYz:
    case model::InstructionCode::VtxDiff:
    case model::InstructionCode::DifAmb:
    case model::InstructionCode::BeginVtxs:
        return 1;
    case model::InstructionCode::Vtx16:
        return 2;
    }
    throw std::invalid_argument("model display list contains an unknown instruction");
}

std::uint8_t instruction_opcode(model::InstructionCode code) {
    const auto value = static_cast<std::uint32_t>(code);
    if (value < 0x400U || ((value - 0x400U) & 3U) != 0
        || (value - 0x400U) / 4U > 0xffU) {
        throw std::invalid_argument("model display list contains an invalid opcode");
    }
    return static_cast<std::uint8_t>((value - 0x400U) / 4U);
}

std::size_t write_render_instructions(
    Writer& writer, const std::vector<model::RenderInstruction>& instructions) {
    if (instructions.size() % 4 != 0) {
        throw std::invalid_argument(
            "model display list instruction count is not divisible by four");
    }
    const std::size_t start = writer.position();
    for (std::size_t index = 0; index < instructions.size(); index += 4) {
        std::uint32_t packed = 0;
        for (std::size_t slot = 0; slot < 4; ++slot) {
            const auto& instruction = instructions[index + slot];
            if (instruction.arguments.size()
                != instruction_arity(instruction.code)) {
                throw std::invalid_argument(
                    "model display list instruction has the wrong argument count");
            }
            packed |= static_cast<std::uint32_t>(
                          instruction_opcode(instruction.code))
                << (slot * 8);
        }
        writer.u32(packed);
        for (std::size_t slot = 0; slot < 4; ++slot) {
            for (const std::uint32_t argument
                 : instructions[index + slot].arguments) {
                writer.u32(argument);
            }
        }
    }
    return writer.position() - start;
}

std::pair<std::uint32_t, std::uint32_t> display_list_counts(
    const std::vector<model::RenderInstruction>& instructions) {
    std::uint64_t primitive_count = 0;
    std::uint64_t vertex_count = 0;
    int primitive_type = -1;
    std::uint64_t current_vertex_count = 0;
    for (const auto& instruction : instructions) {
        switch (instruction.code) {
        case model::InstructionCode::BeginVtxs:
            if (primitive_type != -1 || instruction.arguments.size() != 1) {
                throw std::invalid_argument(
                    "model display list has an invalid BEGIN");
            }
            primitive_type = static_cast<int>(instruction.arguments[0]);
            if (primitive_type < 0 || primitive_type > 3) {
                throw std::invalid_argument(
                    "model display list has an invalid primitive type");
            }
            break;
        case model::InstructionCode::Vtx16:
        case model::InstructionCode::Vtx10:
        case model::InstructionCode::VtxXy:
        case model::InstructionCode::VtxXz:
        case model::InstructionCode::VtxYz:
        case model::InstructionCode::VtxDiff:
            if (primitive_type == -1) {
                throw std::invalid_argument(
                    "model display list vertex is outside a primitive");
            }
            ++vertex_count;
            ++current_vertex_count;
            break;
        case model::InstructionCode::EndVtxs:
            if (primitive_type == -1) {
                throw std::invalid_argument(
                    "model display list has an unmatched END");
            }
            if (primitive_type == 0) {
                if (current_vertex_count < 3 || current_vertex_count % 3 != 0) {
                    throw std::invalid_argument(
                        "model triangle list has an invalid vertex count");
                }
                primitive_count += current_vertex_count / 3;
            } else if (primitive_type == 1) {
                if (current_vertex_count < 4 || current_vertex_count % 4 != 0) {
                    throw std::invalid_argument(
                        "model quad list has an invalid vertex count");
                }
                primitive_count += current_vertex_count / 4;
            } else if (primitive_type == 2) {
                if (current_vertex_count < 3) {
                    throw std::invalid_argument(
                        "model triangle strip has an invalid vertex count");
                }
                primitive_count += 1 + current_vertex_count - 3;
            } else {
                if (current_vertex_count < 4 || current_vertex_count % 2 != 0) {
                    throw std::invalid_argument(
                        "model quad strip has an invalid vertex count");
                }
                primitive_count += 1 + (current_vertex_count - 4) / 2;
            }
            primitive_type = -1;
            current_vertex_count = 0;
            break;
        default:
            break;
        }
    }
    if (primitive_type != -1) {
        throw std::invalid_argument(
            "model display list is missing END");
    }
    if (primitive_count > std::numeric_limits<std::uint32_t>::max()
        || vertex_count > std::numeric_limits<std::uint32_t>::max()) {
        throw std::length_error("model display-list counts exceed 32 bits");
    }
    return {static_cast<std::uint32_t>(primitive_count),
            static_cast<std::uint32_t>(vertex_count)};
}

std::int32_t sign16(std::uint32_t value) {
    return static_cast<std::int16_t>(value & 0xffffU);
}

std::int32_t sign10(std::uint32_t value) {
    const std::int32_t result = static_cast<std::int32_t>(value & 0x3ffU);
    return (result & 0x200) != 0 ? result - 0x400 : result;
}

void update_bounds(IntBounds& bounds, const Int3& value) {
    bounds.min.x = std::min(bounds.min.x, value.x);
    bounds.min.y = std::min(bounds.min.y, value.y);
    bounds.min.z = std::min(bounds.min.z, value.z);
    bounds.max.x = std::max(bounds.max.x, value.x);
    bounds.max.y = std::max(bounds.max.y, value.y);
    bounds.max.z = std::max(bounds.max.z, value.z);
}

IntBounds calculate_bounds(
    const std::vector<model::RenderInstruction>& instructions) {
    IntBounds result;
    Int3 vertex{};
    for (const auto& instruction : instructions) {
        switch (instruction.code) {
        case model::InstructionCode::Vtx16:
            vertex.x = sign16(instruction.arguments.at(0));
            vertex.y = sign16(instruction.arguments.at(0) >> 16);
            vertex.z = sign16(instruction.arguments.at(1));
            update_bounds(result, vertex);
            break;
        case model::InstructionCode::Vtx10:
            vertex.x = sign10(instruction.arguments.at(0)) * 64;
            vertex.y = sign10(instruction.arguments.at(0) >> 10) * 64;
            vertex.z = sign10(instruction.arguments.at(0) >> 20) * 64;
            update_bounds(result, vertex);
            break;
        case model::InstructionCode::VtxXy:
            vertex.x = sign16(instruction.arguments.at(0));
            vertex.y = sign16(instruction.arguments.at(0) >> 16);
            update_bounds(result, vertex);
            break;
        case model::InstructionCode::VtxXz:
            vertex.x = sign16(instruction.arguments.at(0));
            vertex.z = sign16(instruction.arguments.at(0) >> 16);
            update_bounds(result, vertex);
            break;
        case model::InstructionCode::VtxYz:
            vertex.y = sign16(instruction.arguments.at(0));
            vertex.z = sign16(instruction.arguments.at(0) >> 16);
            update_bounds(result, vertex);
            break;
        case model::InstructionCode::VtxDiff:
            // Keep the managed utility's raw fixed-point delta semantics.
            vertex.x += sign10(instruction.arguments.at(0));
            vertex.y += sign10(instruction.arguments.at(0) >> 10);
            vertex.z += sign10(instruction.arguments.at(0) >> 20);
            update_bounds(result, vertex);
            break;
        default:
            break;
        }
    }
    return result;
}

void append_mesh_ids(const std::vector<model::Node>& nodes, std::size_t index,
                     bool root, std::vector<std::size_t>& result,
                     std::vector<bool>& active) {
    if (index >= nodes.size()) {
        throw std::out_of_range("model node reference is outside the model");
    }
    if (active[index]) {
        throw std::runtime_error("model node hierarchy contains a cycle");
    }
    active[index] = true;
    const auto& node = nodes[index];
    const std::size_t start = node.mesh_id / 2U;
    for (std::size_t offset = 0; offset < node.mesh_count; ++offset) {
        result.push_back(start + offset);
    }
    if (!root && node.next_id != -1) {
        append_mesh_ids(nodes, static_cast<std::size_t>(node.next_id), false,
                        result, active);
    }
    if (node.child_id != -1) {
        append_mesh_ids(nodes, static_cast<std::size_t>(node.child_id), false,
                        result, active);
    }
    active[index] = false;
}

std::int32_t scale_bound(std::int32_t value, std::int64_t scale) {
    const std::int64_t result = static_cast<std::int64_t>(value) * scale;
    if (result < std::numeric_limits<std::int32_t>::min()
        || result > std::numeric_limits<std::int32_t>::max()) {
        throw std::overflow_error("model bounds do not fit in 32 bits");
    }
    return static_cast<std::int32_t>(result);
}

Int3 scale_bounds(const Int3& value, std::int64_t scale) {
    return {scale_bound(value.x, scale), scale_bound(value.y, scale),
            scale_bound(value.z, scale)};
}

void write_display_list_meta(Writer& writer, std::uint32_t offset,
                             std::uint32_t size, const Int3& min_bounds,
                             const Int3& max_bounds) {
    writer.u32(offset);
    writer.u32(size);
    writer.u32(static_cast<std::uint32_t>(min_bounds.x));
    writer.u32(static_cast<std::uint32_t>(min_bounds.y));
    writer.u32(static_cast<std::uint32_t>(min_bounds.z));
    writer.u32(static_cast<std::uint32_t>(max_bounds.x));
    writer.u32(static_cast<std::uint32_t>(max_bounds.y));
    writer.u32(static_cast<std::uint32_t>(max_bounds.z));
}

void write_model_header(Writer& writer, const model::Header& source,
                        std::uint32_t primitive_count,
                        std::uint32_t vertex_count,
                        std::uint32_t materials_offset,
                        std::uint32_t display_lists_offset,
                        std::uint32_t nodes_offset,
                        std::uint16_t node_weight_count,
                        std::uint32_t node_weight_offset,
                        std::uint32_t meshes_offset,
                        std::uint16_t texture_count,
                        std::uint32_t textures_offset,
                        std::uint16_t palette_count,
                        std::uint32_t palette_offset,
                        std::uint32_t node_position_count_offset,
                        std::uint16_t material_count,
                        std::uint16_t node_count,
                        std::uint16_t mesh_count,
                        std::uint16_t texture_matrix_count) {
    writer.u32(source.scale_factor);
    writer.u32(static_cast<std::uint32_t>(source.scale_base.value));
    writer.u32(primitive_count);
    writer.u32(vertex_count);
    writer.u32(materials_offset);
    writer.u32(display_lists_offset);
    writer.u32(nodes_offset);
    writer.u16(node_weight_count);
    writer.u8(0);
    writer.u8(0);
    writer.u32(node_weight_offset);
    writer.u32(meshes_offset);
    writer.u16(texture_count);
    writer.u16(0);
    writer.u32(textures_offset);
    writer.u16(palette_count);
    writer.u16(0);
    writer.u32(palette_offset);
    writer.u32(node_position_count_offset);
    writer.u32(0); // NodePosScales
    writer.u32(0); // NodeInitialPosition
    writer.u32(0); // NodePosition
    writer.u16(material_count);
    writer.u16(node_count);
    writer.u32(0); // TextureMatrixOffset
    writer.u32(0); // NodeAnimationOffset
    writer.u32(0); // TexcoordAnimationOffset
    writer.u32(0); // MaterialAnimationOffset
    writer.u32(0); // TextureAnimationOffset
    writer.u16(mesh_count);
    writer.u16(texture_matrix_count);
}

} // namespace

std::vector<std::uint8_t> pack_animation(
    const model::AnimationResults& animations, bool first_hunt_padding) {
    const int count = require_same_group_count(animations);
    Writer writer(raw::Sizes::AnimationHeader);
    std::vector<std::uint32_t> node_offsets;
    std::vector<std::uint32_t> material_offsets;
    std::vector<std::uint32_t> texture_offsets;
    std::vector<std::uint32_t> texcoord_offsets;
    node_offsets.reserve(static_cast<std::size_t>(count));
    material_offsets.reserve(static_cast<std::size_t>(count));
    texture_offsets.reserve(static_cast<std::size_t>(count));
    texcoord_offsets.reserve(static_cast<std::size_t>(count));

    for (int i = 0; i < count; ++i) {
        node_offsets.push_back(animations.node_group_offsets[i] == 0
            ? 0
            : write_node_group(writer, animations.node_groups[i],
                               first_hunt_padding));
        material_offsets.push_back(animations.material_group_offsets[i] == 0
            ? 0
            : write_material_group(writer, animations.material_groups[i],
                                   first_hunt_padding));
        // PackAnim writes the texture group before the texcoord group, while
        // the offset table below stores texcoord before texture.
        texture_offsets.push_back(animations.texture_group_offsets[i] == 0
            ? 0
            : write_texture_group(writer, animations.texture_groups[i],
                                  first_hunt_padding));
        texcoord_offsets.push_back(animations.texcoord_group_offsets[i] == 0
            ? 0
            : write_texcoord_group(writer, animations.texcoord_groups[i],
                                   first_hunt_padding));
    }

    const std::size_t node_list = writer.position();
    for (const std::uint32_t offset : node_offsets) {
        writer.u32(offset);
    }
    const std::size_t unused_list = writer.position();
    for (int i = 0; i < count; ++i) {
        writer.u32(0);
    }
    const std::size_t material_list = writer.position();
    for (const std::uint32_t offset : material_offsets) {
        writer.u32(offset);
    }
    const std::size_t texcoord_list = writer.position();
    for (const std::uint32_t offset : texcoord_offsets) {
        writer.u32(offset);
    }
    const std::size_t texture_list = writer.position();
    for (const std::uint32_t offset : texture_offsets) {
        writer.u32(offset);
    }
    std::vector<std::uint8_t> result = std::move(writer).take();
    patch_u32(result, 0, offset32(node_list));
    patch_u32(result, 4, offset32(unused_list));
    patch_u32(result, 8, offset32(material_list));
    patch_u32(result, 12, offset32(texcoord_list));
    patch_u32(result, 16, offset32(texture_list));
    patch_u16(result, 20, static_cast<std::uint16_t>(count));
    patch_u16(result, 22, first_hunt_padding ? 0xccccU : 0);
    return result;
}

ModelPackResult pack_model(const model::File& source,
                           ModelPackOptions options) {
    const auto& header = source.header();
    const auto& meshes = source.meshes();
    const auto& materials = source.materials();
    const auto& displays = source.display_lists();
    const auto& instructions = source.instructions();
    const auto& nodes = source.nodes();
    const auto& weights = source.node_weights();
    const auto& position_counts = source.node_position_counts();
    const auto& textures = source.textures();
    const auto& palettes = source.palettes();

    if (displays.size() != instructions.size()
        || displays.size() != meshes.size()) {
        throw std::invalid_argument(
            "model display-list, instruction, and mesh tables disagree");
    }
    const auto material_count = count16(materials.size(), "model materials");
    const auto node_count = count16(nodes.size(), "model nodes");
    const auto mesh_count = count16(meshes.size(), "model meshes");
    const auto texture_count = count16(textures.size(), "model textures");
    const auto palette_count = count16(palettes.size(), "model palettes");
    const auto weight_count = count16(weights.size(), "model node weights");
    if (position_counts.size() > std::numeric_limits<std::uint32_t>::max()) {
        throw std::length_error("model node position counts exceed 32 bits");
    }

    std::uint64_t primitive_count = 0;
    std::uint64_t vertex_count = 0;
    for (const auto& list : instructions) {
        const auto [primitives, vertices] = display_list_counts(list);
        primitive_count += primitives;
        vertex_count += vertices;
    }
    if (primitive_count > std::numeric_limits<std::uint32_t>::max()
        || vertex_count > std::numeric_limits<std::uint32_t>::max()) {
        throw std::length_error("model counts exceed 32 bits");
    }

    Writer writer(raw::Sizes::Header);
    Writer external_texture;
    Writer& texture_writer = options.texture == TextureStorage::Inline
        ? writer : external_texture;

    std::size_t actual_offset = raw::Sizes::Header;
    std::uint32_t node_weight_offset = 0;
    if (weights.empty()) {
        // The managed writer emits an implicit matrix-id table even when the
        // decoded model had no explicit node weights. It is part of the file
        // layout, so reserve it before the position-count table.
        actual_offset += position_counts.empty()
            ? sizeof(std::uint32_t)
            : position_counts.size() * sizeof(std::uint32_t);
    } else {
        node_weight_offset = offset32(actual_offset);
        actual_offset += weights.size() * sizeof(std::uint32_t);
    }
    const std::uint32_t node_position_count_offset = position_counts.empty()
        ? 0 : offset32(actual_offset);

    if (weights.empty()) {
        if (options.is_room) {
            for (std::size_t index = 0; index < nodes.size(); ++index) {
                if (nodes[index].mesh_count > 0) {
                    writer.u32(static_cast<std::uint32_t>(index));
                }
            }
        } else {
            const std::size_t pad_count = position_counts.empty()
                ? 1 : position_counts.size();
            for (std::size_t index = 0; index < pad_count; ++index) {
                writer.u32(nodes.size() <= 1 ? 0
                    : static_cast<std::uint32_t>(index + 1));
            }
        }
    } else {
        for (const std::int32_t value : weights) {
            writer.u32(static_cast<std::uint32_t>(value));
        }
    }
    for (const std::int32_t value : position_counts) {
        writer.u32(static_cast<std::uint32_t>(value));
    }

    std::vector<std::uint32_t> texture_data_offsets;
    texture_data_offsets.reserve(textures.size());
    std::vector<std::uint32_t> texture_data_sizes;
    texture_data_sizes.reserve(textures.size());
    for (std::size_t index = 0; index < textures.size(); ++index) {
        const auto data = source.read_texture_data(index);
        texture_data_offsets.push_back(offset32(texture_writer.position()));
        texture_data_sizes.push_back(count32(data.size(), "texture data"));
        texture_writer.raw({data.data(), data.size()});
    }
    const std::uint32_t textures_offset = textures.empty()
        ? 0 : offset32(writer.position());
    for (std::size_t index = 0; index < textures.size(); ++index) {
        write_texture_meta(writer, textures[index], texture_data_offsets[index],
                           texture_data_sizes[index]);
    }

    std::vector<std::uint32_t> palette_data_offsets;
    palette_data_offsets.reserve(palettes.size());
    std::vector<std::uint32_t> palette_data_sizes;
    palette_data_sizes.reserve(palettes.size());
    for (std::size_t index = 0; index < palettes.size(); ++index) {
        const auto data = source.read_palette_data(index);
        if ((data.size() & 1U) != 0) {
            throw std::invalid_argument("model palette data is not 16-bit aligned");
        }
        palette_data_offsets.push_back(offset32(texture_writer.position()));
        palette_data_sizes.push_back(count32(data.size(), "palette data"));
        texture_writer.raw({data.data(), data.size()});
    }
    const std::uint32_t palette_offset = palettes.empty()
        ? 0 : offset32(writer.position());
    for (std::size_t index = 0; index < palettes.size(); ++index) {
        write_palette_meta(writer, palette_data_offsets[index],
                           palette_data_sizes[index]);
    }

    std::vector<std::uint32_t> display_data_offsets;
    std::vector<std::uint32_t> display_data_sizes;
    display_data_offsets.reserve(displays.size());
    display_data_sizes.reserve(displays.size());
    for (const auto& list : instructions) {
        display_data_offsets.push_back(offset32(writer.position()));
        display_data_sizes.push_back(count32(
            write_render_instructions(writer, list), "display-list data"));
    }

    std::vector<Int3> display_min;
    std::vector<Int3> display_max;
    std::vector<Int3> node_min;
    std::vector<Int3> node_max;
    display_min.reserve(displays.size());
    display_max.reserve(displays.size());
    node_min.reserve(nodes.size());
    node_max.reserve(nodes.size());

    const bool recompute_bounds = options.bounds != BoundsMode::None
        && weights.empty();
    if (!recompute_bounds) {
        for (const auto& display : displays) {
            display_min.push_back({display.min_bounds.x.value,
                                   display.min_bounds.y.value,
                                   display.min_bounds.z.value});
            display_max.push_back({display.max_bounds.x.value,
                                   display.max_bounds.y.value,
                                   display.max_bounds.z.value});
        }
        for (const auto& node : nodes) {
            node_min.push_back({node.min_bounds.x.value,
                                node.min_bounds.y.value,
                                node.min_bounds.z.value});
            node_max.push_back({node.max_bounds.x.value,
                                node.max_bounds.y.value,
                                node.max_bounds.z.value});
        }
    } else {
        std::vector<IntBounds> all_bounds;
        all_bounds.reserve(instructions.size());
        for (const auto& list : instructions) {
            all_bounds.push_back(calculate_bounds(list));
        }

        const float world_scale = source.world_scale();
        if (!std::isfinite(world_scale) || world_scale <= 0.0F) {
            throw std::invalid_argument("model scale is not positive");
        }
        const std::int64_t scale = static_cast<std::int64_t>(
            std::llround(world_scale));
        if (scale <= 0 || std::fabs(world_scale - static_cast<float>(scale))
                > 0.0001F) {
            throw std::invalid_argument(
                "computed model scale is not an integer");
        }
        const std::int64_t cap_min =
            static_cast<std::int64_t>(std::numeric_limits<std::int16_t>::min())
            * scale;
        const std::int64_t cap_max =
            static_cast<std::int64_t>(std::numeric_limits<std::int16_t>::max())
            * scale;

        for (const IntBounds& bounds : all_bounds) {
            if (options.bounds == BoundsMode::Capped) {
                const auto cap = [=](std::int32_t value) {
                    const auto scaled = static_cast<std::int64_t>(value) * scale;
                    return scale_bound(static_cast<std::int32_t>(
                        std::clamp(scaled, cap_min, cap_max)), 1);
                };
                display_min.push_back({cap(bounds.min.x), cap(bounds.min.y),
                                       cap(bounds.min.z)});
                display_max.push_back({cap(bounds.max.x), cap(bounds.max.y),
                                       cap(bounds.max.z)});
            } else {
                display_min.push_back(bounds.min);
                display_max.push_back(bounds.max);
            }
        }

        std::vector<bool> active(nodes.size(), false);
        for (std::size_t node_index = 0; node_index < nodes.size();
             ++node_index) {
            std::vector<std::size_t> ids;
            if (nodes[node_index].mesh_count == 0) {
                append_mesh_ids(nodes, node_index, true, ids, active);
            } else {
                const std::size_t start = nodes[node_index].mesh_id / 2U;
                for (std::size_t offset = 0;
                     offset < nodes[node_index].mesh_count; ++offset) {
                    ids.push_back(start + offset);
                }
            }
            if (ids.empty()) {
                node_min.push_back({0, 0, 0});
                node_max.push_back({0, 0, 0});
                continue;
            }
            Int3 min{
                std::numeric_limits<std::int32_t>::max(),
                std::numeric_limits<std::int32_t>::max(),
                std::numeric_limits<std::int32_t>::max()};
            Int3 max{
                std::numeric_limits<std::int32_t>::min(),
                std::numeric_limits<std::int32_t>::min(),
                std::numeric_limits<std::int32_t>::min()};
            for (const std::size_t mesh_index : ids) {
                if (mesh_index >= meshes.size()) {
                    throw std::out_of_range(
                        "model node mesh reference is outside the model");
                }
                const std::size_t display_index =
                    meshes[mesh_index].display_list_id;
                if (display_index >= display_min.size()) {
                    throw std::out_of_range(
                        "model mesh display-list reference is outside the model");
                }
                min.x = std::min(min.x, display_min[display_index].x);
                min.y = std::min(min.y, display_min[display_index].y);
                min.z = std::min(min.z, display_min[display_index].z);
                max.x = std::max(max.x, display_max[display_index].x);
                max.y = std::max(max.y, display_max[display_index].y);
                max.z = std::max(max.z, display_max[display_index].z);
            }
            node_min.push_back(scale_bounds(min, scale));
            node_max.push_back(scale_bounds(max, scale));
        }
    }

    const std::uint32_t display_lists_offset = offset32(writer.position());
    for (std::size_t index = 0; index < displays.size(); ++index) {
        write_display_list_meta(writer, display_data_offsets[index],
                                display_data_sizes[index], display_min[index],
                                display_max[index]);
    }

    const std::uint32_t materials_offset = offset32(writer.position());
    std::uint32_t matrix_count = 0;
    for (const auto& material : materials) {
        write_material(writer, material, texture_matrix_id(material, matrix_count));
    }

    const std::uint32_t nodes_offset = offset32(writer.position());
    for (std::size_t index = 0; index < nodes.size(); ++index) {
        write_node(writer, nodes[index], node_min[index], node_max[index]);
    }

    const std::uint32_t meshes_offset = offset32(writer.position());
    for (const auto& mesh : meshes) {
        writer.u16(mesh.material_id);
        writer.u16(mesh.display_list_id);
    }

    Writer header_writer;
    write_model_header(header_writer, header,
                       static_cast<std::uint32_t>(primitive_count),
                       static_cast<std::uint32_t>(vertex_count),
                       materials_offset, display_lists_offset, nodes_offset,
                       weight_count, node_weight_offset, meshes_offset,
                       texture_count, textures_offset, palette_count,
                       palette_offset, node_position_count_offset,
                       material_count, node_count, mesh_count,
                       static_cast<std::uint16_t>(matrix_count));
    std::vector<std::uint8_t> model_bytes = std::move(writer).take();
    std::vector<std::uint8_t> header_bytes = std::move(header_writer).take();
    if (header_bytes.size() != raw::Sizes::Header
        || model_bytes.size() < header_bytes.size()) {
        throw std::runtime_error("model header writer produced an invalid size");
    }
    std::copy(header_bytes.begin(), header_bytes.end(), model_bytes.begin());
    return {std::move(model_bytes), std::move(external_texture).take()};
}

ModelPackResult repack_model(std::span<const std::uint8_t> bytes,
                             ModelPackOptions options) {
    return pack_model(model::File::from_bytes(
                          std::vector<std::uint8_t>(bytes.begin(), bytes.end())),
                      options);
}

} // namespace fruityprime::utility::repack_model
