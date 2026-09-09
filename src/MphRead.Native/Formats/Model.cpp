#include "Formats/model_format.hpp"

#include "Utility/binary_reader.hpp"
#include "Read.hpp"

#include <algorithm>
#include <cmath>
#include <cctype>
#include <cstddef>
#include <fstream>
#include <iterator>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

namespace fruityprime::model {
namespace {

[[nodiscard]] std::vector<std::uint8_t> read_all(
    const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary | std::ios::ate);
    if (!input) {
        throw std::runtime_error("could not open model " + path.string());
    }
    const std::streamoff length = input.tellg();
    if (length < 0 || static_cast<std::uintmax_t>(length)
        > std::numeric_limits<std::size_t>::max()) {
        throw std::runtime_error("model is too large: " + path.string());
    }
    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(length));
    input.seekg(0);
    if (!bytes.empty()) {
        input.read(reinterpret_cast<char*>(bytes.data()),
                   static_cast<std::streamsize>(bytes.size()));
        if (!input) {
            throw std::runtime_error("could not read model " + path.string());
        }
    }
    return bytes;
}

formats::Fixed read_fixed(core::BinaryReader& reader) {
    return formats::Fixed{reader.read_i32_le()};
}

formats::Vector3Fx read_vector3(core::BinaryReader& reader) {
    return formats::Vector3Fx{read_fixed(reader), read_fixed(reader),
                              read_fixed(reader)};
}

Header read_header(std::span<const std::uint8_t> bytes) {
    if (bytes.size() < Header::Size) {
        throw std::runtime_error("model is smaller than its header");
    }
    core::BinaryReader reader(bytes.subspan(0, Header::Size));
    Header header;
    header.scale_factor = reader.read_u32_le();
    header.scale_base = read_fixed(reader);
    header.primitive_count = reader.read_u32_le();
    header.vertex_count = reader.read_u32_le();
    header.material_offset = reader.read_u32_le();
    header.display_list_offset = reader.read_u32_le();
    header.node_offset = reader.read_u32_le();
    header.node_weight_count = reader.read_u16_le();
    header.flags = reader.read_u8();
    header.padding_1f = reader.read_u8();
    header.node_weight_offset = reader.read_u32_le();
    header.mesh_offset = reader.read_u32_le();
    header.texture_count = reader.read_u16_le();
    header.padding_2a = reader.read_u16_le();
    header.texture_offset = reader.read_u32_le();
    header.palette_count = reader.read_u16_le();
    header.padding_32 = reader.read_u16_le();
    header.palette_offset = reader.read_u32_le();
    header.node_position_counts = reader.read_u32_le();
    header.node_position_scales = reader.read_u32_le();
    header.node_initial_position = reader.read_u32_le();
    header.node_position = reader.read_u32_le();
    header.material_count = reader.read_u16_le();
    header.node_count = reader.read_u16_le();
    header.texture_matrix_offset = reader.read_u32_le();
    header.node_animation_offset = reader.read_u32_le();
    header.texture_coordinate_animations = reader.read_u32_le();
    header.material_animations = reader.read_u32_le();
    header.texture_animations = reader.read_u32_le();
    header.mesh_count = reader.read_u16_le();
    header.texture_matrix_count = reader.read_u16_le();
    return header;
}

template <typename Value, typename Decoder>
void read_array(std::span<const std::uint8_t> bytes, std::uint32_t offset,
                std::size_t count, std::size_t size,
                std::vector<Value>& output, Decoder&& decoder) {
    if (offset == 0 || count == 0) {
        return;
    }
    const std::uint64_t end = static_cast<std::uint64_t>(offset)
        + static_cast<std::uint64_t>(count) * size;
    if (end > bytes.size()) {
        throw std::runtime_error(
            "model array is outside the file: offset="
            + std::to_string(offset) + " count=" + std::to_string(count)
            + " size=" + std::to_string(size)
            + " file=" + std::to_string(bytes.size()));
    }
    output.reserve(count);
    for (std::size_t i = 0; i < count; ++i) {
        core::BinaryReader reader(bytes.subspan(
            static_cast<std::size_t>(offset) + i * size, size));
        output.push_back(decoder(reader));
    }
}

std::pair<InstructionCode, std::size_t> decode_opcode(std::uint8_t opcode) {
    switch (opcode) {
    case 0x00: return {InstructionCode::Nop, 0};
    case 0x14: return {InstructionCode::MtxRestore, 1};
    case 0x20: return {InstructionCode::Color, 1};
    case 0x21: return {InstructionCode::Normal, 1};
    case 0x22: return {InstructionCode::Texcoord, 1};
    case 0x23: return {InstructionCode::Vtx16, 2};
    case 0x24: return {InstructionCode::Vtx10, 1};
    case 0x25: return {InstructionCode::VtxXy, 1};
    case 0x26: return {InstructionCode::VtxXz, 1};
    case 0x27: return {InstructionCode::VtxYz, 1};
    case 0x28: return {InstructionCode::VtxDiff, 1};
    case 0x30: return {InstructionCode::DifAmb, 1};
    case 0x40: return {InstructionCode::BeginVtxs, 1};
    case 0x41: return {InstructionCode::EndVtxs, 0};
    default:
        throw std::runtime_error("model display list contains an unknown instruction");
    }
}

std::vector<RenderInstruction> read_instructions(
    std::span<const std::uint8_t> bytes, const DisplayList& display_list) {
    if (display_list.size % sizeof(std::uint32_t) != 0) {
        throw std::runtime_error("model display-list size is not divisible by four");
    }
    if (display_list.offset > bytes.size()
        || display_list.size > bytes.size() - display_list.offset) {
        throw std::runtime_error("model display list is outside the file");
    }
    core::BinaryReader reader(bytes.subspan(display_list.offset,
                                            display_list.size));
    std::vector<RenderInstruction> result;
    while (reader.remaining() > 0) {
        const std::uint32_t packed = reader.read_u32_le();
        for (std::size_t i = 0; i < 4; ++i) {
            const auto [code, arity] = decode_opcode(
                static_cast<std::uint8_t>((packed >> (8 * i)) & 0xff));
            RenderInstruction instruction;
            instruction.code = code;
            instruction.arguments.reserve(arity);
            for (std::size_t argument = 0; argument < arity; ++argument) {
                instruction.arguments.push_back(reader.read_u32_le());
            }
            result.push_back(std::move(instruction));
        }
    }
    return result;
}

[[nodiscard]] std::size_t animation_distance(std::uint32_t begin,
                                              std::uint32_t end,
                                              std::size_t element_size,
                                              const char* description) {
    if (end < begin) {
        throw std::runtime_error(std::string(description)
                                 + " offsets are not ordered");
    }
    const std::uint32_t distance = end - begin;
    if (distance % element_size != 0) {
        throw std::runtime_error(std::string(description)
                                 + " table is not aligned");
    }
    return static_cast<std::size_t>(distance / element_size);
}

[[nodiscard]] std::string animation_name(
    const std::uint8_t* data, std::size_t size) {
    std::size_t length = 0;
    while (length < size && data[length] != 0) {
        ++length;
    }
    return std::string(reinterpret_cast<const char*>(data), length);
}

template <typename Record, typename NameField>
void add_animation(std::map<std::string, Record>& destination,
                   const Record& record, NameField name_field) {
    const std::string name = name_field(record);
    const auto [_, inserted] = destination.emplace(name, record);
    if (!inserted) {
        throw std::runtime_error("animation table contains duplicate name: "
                                 + name);
    }
}

[[nodiscard]] std::vector<float> read_fixed_lut(
    std::span<const std::uint8_t> bytes, std::uint32_t offset,
    std::size_t count) {
    const auto values = read::do_offsets<formats::Fixed>(bytes, offset, count);
    std::vector<float> result;
    result.reserve(values.size());
    for (const auto value : values) {
        result.push_back(value.to_float());
    }
    return result;
}

[[nodiscard]] std::vector<float> read_rotation_lut(
    std::span<const std::uint8_t> bytes, std::uint32_t offset,
    std::size_t count) {
    constexpr float FullTurn = 2.0F * 3.14159265358979323846F;
    const auto values = read::do_offsets<std::uint16_t>(bytes, offset, count);
    std::vector<float> result;
    result.reserve(values.size());
    for (const auto value : values) {
        result.push_back(static_cast<float>(value) / 65536.0F * FullTurn);
    }
    return result;
}

[[nodiscard]] formats::Matrix4 multiply_matrix4(
    const formats::Matrix4& first, const formats::Matrix4& second) noexcept {
    const float a[4][4] = {
        {first.m11, first.m12, first.m13, first.m14},
        {first.m21, first.m22, first.m23, first.m24},
        {first.m31, first.m32, first.m33, first.m34},
        {first.m41, first.m42, first.m43, first.m44}
    };
    const float b[4][4] = {
        {second.m11, second.m12, second.m13, second.m14},
        {second.m21, second.m22, second.m23, second.m24},
        {second.m31, second.m32, second.m33, second.m34},
        {second.m41, second.m42, second.m43, second.m44}
    };
    float output[4][4]{};
    for (std::size_t row = 0; row < 4; ++row) {
        for (std::size_t column = 0; column < 4; ++column) {
            for (std::size_t index = 0; index < 4; ++index) {
                output[row][column] += a[row][index] * b[index][column];
            }
        }
    }
    return formats::Matrix4{
        output[0][0], output[0][1], output[0][2], output[0][3],
        output[1][0], output[1][1], output[1][2], output[1][3],
        output[2][0], output[2][1], output[2][2], output[2][3],
        output[3][0], output[3][1], output[3][2], output[3][3]
    };
}

[[nodiscard]] formats::Matrix4 translation_matrix(float x, float y,
                                                   float z) noexcept {
    formats::Matrix4 result{};
    result.m41 = x;
    result.m42 = y;
    result.m43 = z;
    return result;
}

[[nodiscard]] formats::Matrix4 scale_matrix(float x, float y,
                                            float z) noexcept {
    formats::Matrix4 result{};
    result.m11 = x;
    result.m22 = y;
    result.m33 = z;
    return result;
}

[[nodiscard]] formats::Matrix4 rotation_z_matrix(float angle) noexcept {
    const float sine = std::sin(angle);
    const float cosine = std::cos(angle);
    return formats::Matrix4{
        cosine, sine, 0.0F, 0.0F,
        -sine, cosine, 0.0F, 0.0F,
        0.0F, 0.0F, 1.0F, 0.0F,
        0.0F, 0.0F, 0.0F, 1.0F
    };
}

[[nodiscard]] AnimationResults read_animations(
    std::span<const std::uint8_t> bytes, const std::vector<Node>& nodes,
    std::string_view model_name) {
    if (bytes.empty()) {
        return {};
    }
    const auto header = read::read_struct<raw::AnimationHeader>(bytes);
    const auto node_offsets = read::do_offsets<std::uint32_t>(
        bytes, header.node_group_offset, header.count);
    const auto material_offsets = read::do_offsets<std::uint32_t>(
        bytes, header.material_group_offset, header.count);
    const auto texcoord_offsets = read::do_offsets<std::uint32_t>(
        bytes, header.texcoord_group_offset, header.count);
    const auto texture_offsets = read::do_offsets<std::uint32_t>(
        bytes, header.texture_group_offset, header.count);

    AnimationResults result;
    result.node_group_offsets = node_offsets;
    result.material_group_offsets = material_offsets;
    result.texcoord_group_offsets = texcoord_offsets;
    result.texture_group_offsets = texture_offsets;

    result.node_groups.reserve(node_offsets.size());
    for (const auto offset : node_offsets) {
        NodeAnimationGroup group;
        if (offset != 0) {
            const auto raw_group = read::do_offset<raw::RawNodeAnimationGroup>(
                bytes, offset);
            group.frame_count = raw_group.frame_count;
            if (!nodes.empty()) {
                const auto animation_count = animation_distance(
                    raw_group.animation_offset, offset,
                    sizeof(raw::NodeAnimation), "node animation");
                const auto raw_animations = read::do_offsets<raw::NodeAnimation>(
                    bytes, raw_group.animation_offset, animation_count);
                for (std::size_t index = 0; index < raw_animations.size();
                     ++index) {
                    const std::string name = index < nodes.size()
                        ? nodes[index].name
                        : std::string{"__no_node_"}
                            + (index < 10 ? "0" : "")
                            + std::to_string(index);
                    const auto [_, inserted] = group.animations.emplace(
                        name, raw_animations[index]);
                    if (!inserted) {
                        throw std::runtime_error(
                            "node animation table contains duplicate name: "
                            + name);
                    }
                }
                const auto scale_count = animation_distance(
                    raw_group.scale_lut_offset, raw_group.rotate_lut_offset,
                    sizeof(formats::Fixed), "node scale LUT");
                const auto rotation_count = animation_distance(
                    raw_group.rotate_lut_offset,
                    raw_group.translate_lut_offset, sizeof(std::uint16_t),
                    "node rotation LUT");
                const auto translation_count = animation_distance(
                    raw_group.translate_lut_offset,
                    raw_group.animation_offset, sizeof(formats::Fixed),
                    "node translation LUT");
                group.scales = read_fixed_lut(bytes,
                                              raw_group.scale_lut_offset,
                                              scale_count);
                group.rotations = read_rotation_lut(
                    bytes, raw_group.rotate_lut_offset, rotation_count);
                group.translations = read_fixed_lut(
                    bytes, raw_group.translate_lut_offset, translation_count);
            }
        }
        result.node_groups.push_back(std::move(group));
    }

    result.material_groups.reserve(material_offsets.size());
    for (const auto offset : material_offsets) {
        MaterialAnimationGroup group;
        if (offset != 0) {
            const auto raw_group =
                read::do_offset<raw::RawMaterialAnimationGroup>(bytes, offset);
            group.frame_count = raw_group.frame_count;
            group.current_frame = raw_group.animation_frame;
            group.unused_frame = raw_group.unused_12;
            if (raw_group.animation_count != 0) {
                const auto raw_animations = read::do_offsets<raw::MaterialAnimation>(
                    bytes, raw_group.animation_offset,
                    raw_group.animation_count);
                for (const auto& animation : raw_animations) {
                    add_animation(group.animations, animation,
                                  [](const raw::MaterialAnimation& value) {
                                      return animation_name(value.name.data(),
                                                            value.name.size());
                                  });
                }
                const auto color_count = animation_distance(
                    raw_group.color_lut_offset, raw_group.animation_offset, 1,
                    "material color LUT");
                const auto colors = read::do_offsets<std::uint8_t>(
                    bytes, raw_group.color_lut_offset, color_count);
                group.colors.reserve(colors.size());
                for (const auto value : colors) {
                    group.colors.push_back(static_cast<float>(value));
                }
            }
        }
        result.material_groups.push_back(std::move(group));
    }

    result.texcoord_groups.reserve(texcoord_offsets.size());
    for (const auto offset : texcoord_offsets) {
        TexcoordAnimationGroup group;
        if (offset != 0) {
            const auto raw_group = read::do_offset<raw::RawTexcoordAnimationGroup>(
                bytes, offset);
            group.frame_count = raw_group.frame_count;
            group.current_frame = raw_group.animation_frame;
            group.unused_frame = raw_group.unused_1a;
            if (raw_group.animation_count != 0) {
                const auto raw_animations = read::do_offsets<raw::TexcoordAnimation>(
                    bytes, raw_group.animation_offset,
                    raw_group.animation_count);
                for (const auto& animation : raw_animations) {
                    add_animation(group.animations, animation,
                                  [](const raw::TexcoordAnimation& value) {
                                      return animation_name(value.name.data(),
                                                            value.name.size());
                                  });
                }
                const auto scale_count = animation_distance(
                    raw_group.scale_lut_offset, raw_group.rotate_lut_offset,
                    sizeof(formats::Fixed), "texcoord scale LUT");
                const auto rotation_count = animation_distance(
                    raw_group.rotate_lut_offset,
                    raw_group.translate_lut_offset, sizeof(std::uint16_t),
                    "texcoord rotation LUT");
                std::size_t translation_count = animation_distance(
                    raw_group.translate_lut_offset,
                    raw_group.animation_offset, sizeof(formats::Fixed),
                    "texcoord translation LUT");
                if (model_name == "GuardBot1" && offset == 3704) {
                    // Preserve the managed reader's one known data correction:
                    // this authored group has a short UV LUT span but the
                    // active model consumes 26 entries.
                    translation_count = 26;
                }
                group.scales = read_fixed_lut(
                    bytes, raw_group.scale_lut_offset, scale_count);
                group.rotations = read_rotation_lut(
                    bytes, raw_group.rotate_lut_offset, rotation_count);
                group.translations = read_fixed_lut(
                    bytes, raw_group.translate_lut_offset, translation_count);
            }
        }
        result.texcoord_groups.push_back(std::move(group));
    }

    result.texture_groups.reserve(texture_offsets.size());
    for (const auto offset : texture_offsets) {
        TextureAnimationGroup group;
        if (offset != 0) {
            const auto raw_group = read::do_offset<raw::RawTextureAnimationGroup>(
                bytes, offset);
            group.frame_count = raw_group.frame_count;
            group.current_frame = raw_group.animation_frame;
            group.unused_frame = raw_group.unused_1c;
            group.unused_a = raw_group.unused_0a;
            if (raw_group.animation_count != 0) {
                const auto raw_animations = read::do_offsets<raw::TextureAnimation>(
                    bytes, raw_group.animation_offset,
                    raw_group.animation_count);
                for (const auto& animation : raw_animations) {
                    add_animation(group.animations, animation,
                                  [](const raw::TextureAnimation& value) {
                                      return animation_name(value.name.data(),
                                                            value.name.size());
                                  });
                }
                group.frame_indices = read::do_offsets<std::uint16_t>(
                    bytes, raw_group.frame_index_offset,
                    raw_group.frame_index_count);
                group.texture_ids = read::do_offsets<std::uint16_t>(
                    bytes, raw_group.texture_id_offset,
                    raw_group.texture_id_count);
                group.palette_ids = read::do_offsets<std::uint16_t>(
                    bytes, raw_group.palette_id_offset,
                    raw_group.palette_id_count);
            }
        }
        result.texture_groups.push_back(std::move(group));
    }
    return result;
}

void append_animation_results(AnimationResults& destination,
                              AnimationResults source) {
    destination.node_group_offsets.insert(
        destination.node_group_offsets.end(),
        source.node_group_offsets.begin(), source.node_group_offsets.end());
    destination.material_group_offsets.insert(
        destination.material_group_offsets.end(),
        source.material_group_offsets.begin(),
        source.material_group_offsets.end());
    destination.texcoord_group_offsets.insert(
        destination.texcoord_group_offsets.end(),
        source.texcoord_group_offsets.begin(), source.texcoord_group_offsets.end());
    destination.texture_group_offsets.insert(
        destination.texture_group_offsets.end(),
        source.texture_group_offsets.begin(), source.texture_group_offsets.end());
    destination.node_groups.insert(destination.node_groups.end(),
                                   std::make_move_iterator(
                                       source.node_groups.begin()),
                                   std::make_move_iterator(
                                       source.node_groups.end()));
    destination.material_groups.insert(destination.material_groups.end(),
                                       std::make_move_iterator(
                                           source.material_groups.begin()),
                                       std::make_move_iterator(
                                           source.material_groups.end()));
    destination.texcoord_groups.insert(
        destination.texcoord_groups.end(),
        std::make_move_iterator(source.texcoord_groups.begin()),
        std::make_move_iterator(source.texcoord_groups.end()));
    destination.texture_groups.insert(
        destination.texture_groups.end(),
        std::make_move_iterator(source.texture_groups.begin()),
        std::make_move_iterator(source.texture_groups.end()));
}

} // namespace

float interpolate_animation(std::span<const float> values, int start,
                            int frame, int blend, int lut_length,
                            int frame_count, bool rotation) {
    const auto value_at = [&](std::int64_t index) {
        const auto absolute = static_cast<std::int64_t>(start) + index;
        if (absolute < 0 || static_cast<std::uint64_t>(absolute) >= values.size()) {
            throw std::out_of_range("animation LUT sample is outside the model");
        }
        return values[static_cast<std::size_t>(absolute)];
    };
    // Test these before division and range validation: a constant channel is
    // valid even when its unused blend/frame-count fields are zero.
    if (lut_length == 1) return value_at(0);
    if (blend == 1) return value_at(frame);
    const unsigned shift = static_cast<unsigned>(blend >> 1) & 31;
    const auto last = std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(frame_count) - 1);
    const int limit = std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(last >> shift) << shift);
    if (frame >= limit) return value_at(static_cast<std::int64_t>(lut_length) - frame_count + frame);
    if (blend == 0) throw std::invalid_argument("animation blend division by zero");
    const std::int64_t index = static_cast<std::int64_t>(frame) / blend;
    const int remainder = static_cast<int>(static_cast<std::int64_t>(frame) % blend);
    if (remainder == 0) return value_at(index);
    float first = value_at(index), second = value_at(index + 1);
    if (rotation) {
        constexpr float pi = 3.14159265358979323846F;
        if (first - second > pi) second += pi * 2.0F;
        else if (first - second < -pi) first += pi * 2.0F;
    }
    const float factor = 1.0F / static_cast<float>(blend) * static_cast<float>(remainder);
    return first + (second - first) * factor;
}

formats::Matrix4 animate_node(const NodeAnimationGroup& group,
                              const raw::NodeAnimation& animation,
                              int current_frame,
                              formats::Vector3 model_scale) {
    const float scale_x = interpolate_animation(
        group.scales, animation.scale_lut_index_x, current_frame,
        animation.scale_blend_x, animation.scale_lut_length_x,
        group.frame_count);
    const float scale_y = interpolate_animation(
        group.scales, animation.scale_lut_index_y, current_frame,
        animation.scale_blend_y, animation.scale_lut_length_y,
        group.frame_count);
    const float scale_z = interpolate_animation(
        group.scales, animation.scale_lut_index_z, current_frame,
        animation.scale_blend_z, animation.scale_lut_length_z,
        group.frame_count);
    const float rotate_x = interpolate_animation(
        group.rotations, animation.rotate_lut_index_x, current_frame,
        animation.rotate_blend_x, animation.rotate_lut_length_x,
        group.frame_count, true);
    const float rotate_y = interpolate_animation(
        group.rotations, animation.rotate_lut_index_y, current_frame,
        animation.rotate_blend_y, animation.rotate_lut_length_y,
        group.frame_count, true);
    const float rotate_z = interpolate_animation(
        group.rotations, animation.rotate_lut_index_z, current_frame,
        animation.rotate_blend_z, animation.rotate_lut_length_z,
        group.frame_count, true);
    const float translate_x = interpolate_animation(
        group.translations, animation.translate_lut_index_x, current_frame,
        animation.translate_blend_x, animation.translate_lut_length_x,
        group.frame_count);
    const float translate_y = interpolate_animation(
        group.translations, animation.translate_lut_index_y, current_frame,
        animation.translate_blend_y, animation.translate_lut_length_y,
        group.frame_count);
    const float translate_z = interpolate_animation(
        group.translations, animation.translate_lut_index_z, current_frame,
        animation.translate_blend_z, animation.translate_lut_length_z,
        group.frame_count);
    // AnimateNode uses S * Rx * Ry * Rz * T. ComputeNodeTransforms has
    // a different authored M32 expression and cannot be reused here.
    const float sx = std::sin(rotate_x), cx = std::cos(rotate_x);
    const float sy = std::sin(rotate_y), cy = std::cos(rotate_y);
    const formats::Matrix4 rx{1,0,0,0, 0,cx,sx,0, 0,-sx,cx,0, 0,0,0,1};
    const formats::Matrix4 ry{cy,0,-sy,0, 0,1,0,0, sy,0,cy,0, 0,0,0,1};
    auto matrix = multiply_matrix4(multiply_matrix4(rx, ry), rotation_z_matrix(rotate_z));
    matrix = multiply_matrix4(matrix, translation_matrix(
        translate_x / model_scale.x, translate_y / model_scale.y, translate_z / model_scale.z));
    return multiply_matrix4(scale_matrix(scale_x, scale_y, scale_z), matrix);
}

formats::Matrix4 animate_texcoords(const TexcoordAnimationGroup& group,
                                   const raw::TexcoordAnimation& animation,
                                   int current_frame) {
    const float scale_s = interpolate_animation(
        group.scales, animation.scale_lut_index_s, current_frame,
        animation.scale_blend_s, animation.scale_lut_length_s,
        group.frame_count);
    const float scale_t = interpolate_animation(
        group.scales, animation.scale_lut_index_t, current_frame,
        animation.scale_blend_t, animation.scale_lut_length_t,
        group.frame_count);
    const float rotate = interpolate_animation(
        group.rotations, animation.rotate_lut_index_z, current_frame,
        animation.rotate_blend_z, animation.rotate_lut_length_z,
        group.frame_count, true);
    const float translate_s = interpolate_animation(
        group.translations, animation.translate_lut_index_s, current_frame,
        animation.translate_blend_s, animation.translate_lut_length_s,
        group.frame_count);
    const float translate_t = interpolate_animation(
        group.translations, animation.translate_lut_index_t, current_frame,
        animation.translate_blend_t, animation.translate_lut_length_t,
        group.frame_count);

    formats::Matrix4 result = translation_matrix(translate_s, translate_t, 0.0F);
    if (rotate != 0.0F) {
        result = multiply_matrix4(translation_matrix(0.5F, 0.5F, 0.0F),
                                 result);
        result = multiply_matrix4(rotation_z_matrix(rotate), result);
        result = multiply_matrix4(
            translation_matrix(-0.5F, -0.5F, 0.0F), result);
    }
    return multiply_matrix4(scale_matrix(scale_s, scale_t, 1.0F), result);
}

TextureAnimationSelection select_texture_animation(
    const TextureAnimationGroup& group,
    const raw::TextureAnimation& animation, std::uint16_t current_frame) {
    const std::size_t start = animation.start_index;
    const std::size_t count = animation.count;
    if (start > group.frame_indices.size()
        || count > group.frame_indices.size() - start
        || start > group.texture_ids.size()
        || count > group.texture_ids.size() - start
        || start > group.palette_ids.size()
        || count > group.palette_ids.size() - start) {
        throw std::out_of_range("texture animation range is outside the group");
    }
    for (std::size_t index = start; index < start + count; ++index) {
        if (group.frame_indices[index] == current_frame) {
            return TextureAnimationSelection{
                true, group.texture_ids[index], group.palette_ids[index]
            };
        }
    }
    return {};
}

File File::read_file(const std::filesystem::path& path) {
    return File(read_all(path));
}

File File::from_bytes(std::vector<std::uint8_t> bytes) {
    return File(std::move(bytes));
}

File File::from_resources(std::vector<std::uint8_t> model_bytes,
                          std::vector<std::uint8_t> texture_bytes,
                          std::vector<std::uint8_t> palette_bytes) {
    return File(std::move(model_bytes), std::move(texture_bytes),
                std::move(palette_bytes));
}

File File::from_recolor_resources(
    std::vector<std::uint8_t> model_bytes,
    std::vector<std::uint8_t> table_bytes,
    std::vector<std::uint8_t> texture_bytes,
    std::vector<std::uint8_t> palette_bytes) {
    return File(std::move(model_bytes), std::move(texture_bytes),
                std::move(palette_bytes), std::move(table_bytes));
}

File::File(std::vector<std::uint8_t> bytes)
    : File(std::move(bytes), {}, {}) {}

File::File(std::vector<std::uint8_t> model_bytes,
           std::vector<std::uint8_t> texture_bytes,
           std::vector<std::uint8_t> palette_bytes,
           std::vector<std::uint8_t> table_bytes)
    : bytes_(std::move(model_bytes)),
      table_bytes_(std::move(table_bytes)),
      texture_bytes_(std::move(texture_bytes)),
      palette_bytes_(std::move(palette_bytes)) {
    if (table_bytes_.empty()) {
        table_bytes_ = bytes_;
    }
    if (texture_bytes_.empty()) {
        texture_bytes_ = bytes_;
    }
    if (palette_bytes_.empty()) {
        palette_bytes_ = texture_bytes_;
    }
    parse();
}

void File::load_animations(std::vector<std::uint8_t> bytes,
                           std::string model_name) {
    animation_bytes_ = std::move(bytes);
    animations_ = read_animations(animation_bytes_, nodes_, model_name);
}

void File::append_animations(std::vector<std::uint8_t> bytes,
                             std::string model_name) {
    AnimationResults additional = read_animations(bytes, nodes_, model_name);
    append_animation_results(animations_, std::move(additional));
    // Keep the accessor useful for diagnostics: it represents the most
    // recently supplied resource, while `animations()` contains the merged
    // model-local and shared groups.
    animation_bytes_ = std::move(bytes);
}

namespace {

[[nodiscard]] std::int32_t signed_10(std::uint32_t value) {
    value &= 0x3ff;
    if ((value & 0x200) != 0) {
        value |= 0xfffffc00;
    }
    return static_cast<std::int32_t>(value);
}

[[nodiscard]] std::int32_t signed_16(std::uint32_t value) {
    return static_cast<std::int16_t>(value & 0xffff);
}

} // namespace

int node_layer_mask(bool single_player, int room_layer, int player_count,
                    bool capture) noexcept {
    int mask = 0;
    if (single_player) {
        if (room_layer > 0) {
            mask = static_cast<int>(
                (static_cast<unsigned>(mask) & 0xC03Fu)
                | (((1u << room_layer) & 0xFFu) << 6));
        }
        return mask;
    }
    mask |= static_cast<int>(NodeLayer::MultiplayerU);
    mask |= static_cast<int>(player_count <= 2 ? NodeLayer::MultiplayerLod0
                                               : NodeLayer::MultiplayerLod1);
    if (capture) {
        mask |= static_cast<int>(NodeLayer::CaptureTheFlag);
    }
    return mask;
}

bool node_enabled_for_layer(std::string_view name, int layer_mask) {
    if (name.empty() || name.front() != '_') return true;
    std::uint32_t flags = 0;
    for (std::size_t offset = 0; offset + 4 <= name.size(); offset += 4) {
        const auto chunk = name.substr(offset, 4);
        if (chunk.starts_with("_s")) {
            auto number = chunk.substr(2);
            while (!number.empty() && std::isspace(static_cast<unsigned char>(number.front()))) number.remove_prefix(1);
            while (!number.empty() && std::isspace(static_cast<unsigned char>(number.back()))) number.remove_suffix(1);
            int sign = 1;
            if (!number.empty() && (number.front() == '+' || number.front() == '-')) {
                if (number.front() == '-') sign = -1;
                number.remove_prefix(1);
            }
            int id = 0;
            bool valid = !number.empty();
            for (char ch : number) {
                if (ch < '0' || ch > '9') { valid = false; break; }
                id = id * 10 + ch - '0';
            }
            if (valid) {
                const unsigned bit = static_cast<unsigned>(id * sign) & 31;
                flags = (flags & 0xC03Fu) | (((flags << 18 >> 24) | (1u << bit)) << 6);
            }
        } else if (chunk == "_ml0") flags |= static_cast<unsigned>(NodeLayer::MultiplayerLod0);
        else if (chunk == "_ml1") flags |= static_cast<unsigned>(NodeLayer::MultiplayerLod1);
        else if (chunk == "_mpu") flags |= static_cast<unsigned>(NodeLayer::MultiplayerU);
        else if (chunk == "_ctf") flags |= static_cast<unsigned>(NodeLayer::CaptureTheFlag);
    }
    return (flags & static_cast<std::uint32_t>(layer_mask)) != 0;
}

void File::set_name(std::string value) {
    name_ = std::move(value);
    texture_matrices_.clear();
    // Read.cs hard-codes this one matrix rather than decoding the table,
    // because everything outside the used 3x2 corner is uninitialised in the
    // cartridge's RAM image.
    if (name_ == "AlimbicCapsule") {
        formats::Matrix4 matrix{};
        matrix.m11 = 0.0F;
        matrix.m22 = 0.0F;
        matrix.m33 = 0.0F;
        matrix.m44 = 0.0F;
        matrix.m21 = formats::Fixed::to_float(static_cast<std::int64_t>(-2048));
        matrix.m31 = formats::Fixed::to_float(static_cast<std::int64_t>(410));
        matrix.m32 = formats::Fixed::to_float(static_cast<std::int64_t>(-3891));
        texture_matrices_.push_back(matrix);
    }
}

void File::filter_nodes(int layer_mask) {
    for (auto& node : nodes_) node.enabled = node_enabled_for_layer(node.name, layer_mask) ? 1 : 0;
}

int File::get_node_index_by_name(std::string_view name) const noexcept {
    for (std::size_t i = 0; i < nodes_.size(); ++i) {
        if (nodes_[i].name == name) return static_cast<int>(i);
    }
    return -1;
}

const Node* File::get_node_by_name(std::string_view name) const noexcept {
    const int index = get_node_index_by_name(name);
    return index < 0 ? nullptr : &nodes_[index];
}

const Material* File::get_material_by_name(std::string_view name) const noexcept {
    for (const auto& material : materials_) if (material.name == name) return &material;
    return nullptr;
}

namespace {
formats::ColorRgba rgba(std::uint32_t color, std::uint8_t alpha) {
    const auto channel = [](std::uint32_t value) {
        return static_cast<std::uint8_t>(std::round(static_cast<float>(value & 31) / 31.0F * 255.0F));
    };
    return {channel(color), channel(color >> 5), channel(color >> 10), alpha};
}
}

std::vector<formats::ColorRgba> File::get_pixels(int texture_id, int palette_id) const {
    const auto pixels = decode_texture(texture_id);
    const bool direct = textures_.at(texture_id).format == 5;
    const auto palette = direct ? std::vector<std::uint16_t>{} : decode_palette(palette_id);
    std::vector<formats::ColorRgba> result;
    result.reserve(pixels.size());
    for (const auto& pixel : pixels) {
        result.push_back(rgba(direct ? pixel.data : palette.at(pixel.data), pixel.alpha));
    }
    return result;
}

std::vector<formats::ColorRgba> File::get_palette_pixels(int palette_id) const {
    const auto palette = decode_palette(palette_id);
    std::vector<formats::ColorRgba> result;
    result.reserve(palette.size());
    for (const auto color : palette) result.push_back(rgba(color, 255));
    return result;
}

std::vector<GeometryPrimitive> File::decode_geometry(
    std::size_t display_list_id, int texture_width, int texture_height,
    bool texgen, bool is_room) const {
    if (display_list_id >= instructions_.size()) {
        throw std::out_of_range("model display-list index is outside the model");
    }
    float vertex_x = 0;
    float vertex_y = 0;
    float vertex_z = 0;
    float texture_s = texgen ? 0.5F : 0.0F;
    float texture_t = texgen ? 0.5F : 0.0F;
    float normal_x = 0;
    float normal_y = 0;
    float normal_z = 0;
    // OpenGL's current colour starts as white.  The managed renderer also
    // seeds it from the material before executing a display list, so an
    // omitted COLOR command must never turn an otherwise textured mesh black.
    std::uint32_t color = 0x7fff;
    std::uint32_t ambient_color = 0;
    std::uint32_t matrix_id = 0;
    bool has_color = false;
    bool diffuse_ambient = false;
    bool in_primitive = false;
    GeometryPrimitive current;
    std::vector<GeometryPrimitive> result;

    for (const RenderInstruction& instruction : instructions_[display_list_id]) {
        switch (instruction.code) {
        case InstructionCode::BeginVtxs:
            if (in_primitive || instruction.arguments.size() != 1) {
                throw std::runtime_error("model display list has an invalid BEGIN");
            }
            switch (instruction.arguments[0]) {
            case 0: current.type = GeometryPrimitiveType::Triangles; break;
            case 1: current.type = GeometryPrimitiveType::Quads; break;
            case 2: current.type = GeometryPrimitiveType::TriangleStrip; break;
            case 3: current.type = GeometryPrimitiveType::QuadStrip; break;
            default:
                throw std::runtime_error("model display list has an invalid primitive type");
            }
            current.vertices.clear();
            in_primitive = true;
            break;
        case InstructionCode::EndVtxs:
            if (!in_primitive) {
                throw std::runtime_error("model display list has an unmatched END");
            }
            result.push_back(std::move(current));
            current = GeometryPrimitive{};
            in_primitive = false;
            break;
        case InstructionCode::Color:
            color = instruction.arguments.at(0);
            has_color = true;
            diffuse_ambient = false;
            break;
        case InstructionCode::DifAmb:
            color = instruction.arguments.at(0);
            ambient_color = instruction.arguments.at(0);
            has_color = true;
            diffuse_ambient = true;
            break;
        case InstructionCode::Normal: {
            const std::uint32_t packed = instruction.arguments.at(0);
            normal_x = static_cast<float>(signed_10(packed)) / 512.0F;
            normal_y = static_cast<float>(signed_10(packed >> 10)) / 512.0F;
            normal_z = static_cast<float>(signed_10(packed >> 20)) / 512.0F;
            break;
        }
        case InstructionCode::Texcoord: {
            const std::uint32_t packed = instruction.arguments.at(0);
            // The managed display-list builder still accepts TEXCOORD in a
            // mesh whose material has no texture (the coordinates are simply
            // unused by the shader; Release builds also remove its debug
            // assertion).  Keep decoding that list instead of aborting the
            // whole room on optional models such as door locks.
            if (texture_width > 0 && texture_height > 0) {
                texture_s = static_cast<float>(signed_16(packed))
                    / 16.0F / static_cast<float>(texture_width);
                texture_t = static_cast<float>(signed_16(packed >> 16))
                    / 16.0F / static_cast<float>(texture_height);
            } else {
                texture_s = 0.0F;
                texture_t = 0.0F;
            }
            break;
        }
        case InstructionCode::Vtx16: {
            const std::uint32_t xy = instruction.arguments.at(0);
            vertex_x = static_cast<float>(signed_16(xy)) / 4096.0F;
            vertex_y = static_cast<float>(signed_16(xy >> 16)) / 4096.0F;
            vertex_z = static_cast<float>(signed_16(instruction.arguments.at(1)))
                / 4096.0F;
            if (!in_primitive) {
                throw std::runtime_error("model vertex is outside a primitive");
            }
            current.vertices.push_back(GeometryVertex{
                vertex_x, vertex_y, vertex_z, normal_x, normal_y, normal_z,
                texture_s, texture_t, color, ambient_color, matrix_id,
                has_color, diffuse_ambient
            });
            break;
        }
        case InstructionCode::Vtx10: {
            const std::uint32_t packed = instruction.arguments.at(0);
            vertex_x = static_cast<float>(signed_10(packed)) / 64.0F;
            vertex_y = static_cast<float>(signed_10(packed >> 10)) / 64.0F;
            vertex_z = static_cast<float>(signed_10(packed >> 20)) / 64.0F;
            if (!in_primitive) {
                throw std::runtime_error("model vertex is outside a primitive");
            }
            current.vertices.push_back(GeometryVertex{
                vertex_x, vertex_y, vertex_z, normal_x, normal_y, normal_z,
                texture_s, texture_t, color, ambient_color, matrix_id,
                has_color, diffuse_ambient
            });
            break;
        }
        case InstructionCode::VtxXy: {
            const std::uint32_t packed = instruction.arguments.at(0);
            vertex_x = static_cast<float>(signed_16(packed)) / 4096.0F;
            vertex_y = static_cast<float>(signed_16(packed >> 16)) / 4096.0F;
            if (!in_primitive) {
                throw std::runtime_error("model vertex is outside a primitive");
            }
            current.vertices.push_back(GeometryVertex{
                vertex_x, vertex_y, vertex_z, normal_x, normal_y, normal_z,
                texture_s, texture_t, color, ambient_color, matrix_id,
                has_color, diffuse_ambient
            });
            break;
        }
        case InstructionCode::VtxXz: {
            const std::uint32_t packed = instruction.arguments.at(0);
            vertex_x = static_cast<float>(signed_16(packed)) / 4096.0F;
            vertex_z = static_cast<float>(signed_16(packed >> 16)) / 4096.0F;
            if (!in_primitive) {
                throw std::runtime_error("model vertex is outside a primitive");
            }
            current.vertices.push_back(GeometryVertex{
                vertex_x, vertex_y, vertex_z, normal_x, normal_y, normal_z,
                texture_s, texture_t, color, ambient_color, matrix_id,
                has_color, diffuse_ambient
            });
            break;
        }
        case InstructionCode::VtxYz: {
            const std::uint32_t packed = instruction.arguments.at(0);
            vertex_y = static_cast<float>(signed_16(packed)) / 4096.0F;
            vertex_z = static_cast<float>(signed_16(packed >> 16)) / 4096.0F;
            if (!in_primitive) {
                throw std::runtime_error("model vertex is outside a primitive");
            }
            current.vertices.push_back(GeometryVertex{
                vertex_x, vertex_y, vertex_z, normal_x, normal_y, normal_z,
                texture_s, texture_t, color, ambient_color, matrix_id,
                has_color, diffuse_ambient
            });
            break;
        }
        case InstructionCode::VtxDiff: {
            // VTX_DIFF packs three 10-bit deltas, but they are 1/4096 units
            // like VTX_16 -- not the 1/64 of VTX_10.  Dividing by 64 makes
            // every difference-coded vertex 64 times too far from the one
            // before it, which is most of the cartridge's room geometry.
            const std::uint32_t packed = instruction.arguments.at(0);
            vertex_x += static_cast<float>(signed_10(packed)) / 4096.0F;
            vertex_y += static_cast<float>(signed_10(packed >> 10)) / 4096.0F;
            vertex_z += static_cast<float>(signed_10(packed >> 20)) / 4096.0F;
            if (!in_primitive) {
                throw std::runtime_error("model vertex is outside a primitive");
            }
            current.vertices.push_back(GeometryVertex{
                vertex_x, vertex_y, vertex_z, normal_x, normal_y, normal_z,
                texture_s, texture_t, color, ambient_color, matrix_id,
                has_color, diffuse_ambient
            });
            break;
        }
        case InstructionCode::MtxRestore:
            // The managed renderer ignores MTX_RESTORE in a room so the room
            // node transforms stay togglable; every room vertex uses stack
            // entry 0.
            if (!is_room) {
                matrix_id = instruction.arguments.at(0);
            }
            break;
        case InstructionCode::Nop:
            break;
        }
    }
    if (in_primitive) {
        throw std::runtime_error("model display list is missing END");
    }
    return result;
}

std::vector<TexturePixel> File::decode_texture(std::size_t texture_id) const {
    if (texture_id >= textures_.size()) {
        throw std::out_of_range("model texture index is outside the model");
    }
    const Texture& texture = textures_[texture_id];
    const std::size_t pixel_count = static_cast<std::size_t>(texture.width)
        * static_cast<std::size_t>(texture.height);
    std::size_t entries_per_byte = 1;
    if (texture.format == 0) {
        entries_per_byte = 4;
    } else if (texture.format == 1) {
        entries_per_byte = 2;
    }
    if (pixel_count % entries_per_byte != 0) {
        throw std::runtime_error("model texture pixel count is not divisible by its format");
    }
    const std::size_t encoded_count = pixel_count / entries_per_byte;
    const std::size_t bytes_per_pixel = texture.format == 5 ? 2 : 1;
    if (encoded_count > std::numeric_limits<std::size_t>::max() / bytes_per_pixel) {
        throw std::runtime_error("model texture size overflows");
    }
    const std::size_t required = encoded_count * bytes_per_pixel;
    if (texture.image_offset > texture_bytes_.size()
        || required > texture_bytes_.size() - texture.image_offset) {
        throw std::runtime_error("model texture image is outside the file");
    }
    const std::span<const std::uint8_t> bytes(texture_bytes_);
    core::BinaryReader reader(bytes.subspan(texture.image_offset, required));
    std::vector<TexturePixel> result;
    result.reserve(pixel_count);
    if (texture.format == 5) {
        for (std::size_t i = 0; i < pixel_count; ++i) {
            const std::uint16_t color = reader.read_u16_le();
            result.push_back(TexturePixel{
                color, static_cast<std::uint8_t>((color & 0x8000) != 0 ? 255 : 0)
            });
        }
        return result;
    }
    if (texture.format != 0 && texture.format != 1 && texture.format != 2
        && texture.format != 4 && texture.format != 6) {
        throw std::runtime_error("model texture format is unsupported");
    }
    for (std::size_t encoded = 0; encoded < encoded_count; ++encoded) {
        const std::uint8_t packed = reader.read_u8();
        for (std::size_t entry = 0; entry < entries_per_byte; ++entry) {
            const std::size_t shift = entry * (8 / entries_per_byte);
            std::uint32_t index = packed >> shift;
            std::uint8_t alpha = 255;
            if (texture.format == 0) {
                index &= 0x3;
            } else if (texture.format == 1) {
                index &= 0xf;
            } else if (texture.format == 4) {
                index &= 0x7;
                alpha = static_cast<std::uint8_t>(std::lround(
                    static_cast<float>(packed >> 3) / 31.0F * 255.0F));
            } else if (texture.format == 6) {
                index &= 0x1f;
                alpha = static_cast<std::uint8_t>(std::lround(
                    static_cast<float>(packed >> 5) / 7.0F * 255.0F));
            }
            if ((texture.format == 0 || texture.format == 1 || texture.format == 2)
                && texture.opaque == 0 && index == 0) {
                alpha = 0;
            }
            result.push_back(TexturePixel{index, alpha});
        }
    }
    return result;
}

std::vector<std::uint8_t> File::read_texture_data(
    std::size_t texture_id) const {
    if (texture_id >= textures_.size()) {
        throw std::out_of_range("model texture index is outside the model");
    }
    const Texture& texture = textures_[texture_id];
    if (texture.image_offset > texture_bytes_.size()
        || texture.image_size > texture_bytes_.size() - texture.image_offset) {
        throw std::runtime_error("model texture image is outside the file");
    }
    const auto begin = texture_bytes_.begin()
        + static_cast<std::ptrdiff_t>(texture.image_offset);
    return std::vector<std::uint8_t>(
        begin, begin + static_cast<std::ptrdiff_t>(texture.image_size));
}

std::vector<std::uint16_t> File::decode_palette(std::size_t palette_id) const {
    if (palette_id >= palettes_.size()) {
        throw std::out_of_range("model palette index is outside the model");
    }
    const Palette& palette = palettes_[palette_id];
    if (palette.size % 2 != 0) {
        throw std::runtime_error("model palette size is not divisible by two");
    }
    if (palette.offset > palette_bytes_.size()
        || palette.size > palette_bytes_.size() - palette.offset) {
        throw std::runtime_error("model palette is outside the file");
    }
    const std::span<const std::uint8_t> bytes(palette_bytes_);
    core::BinaryReader reader(bytes.subspan(palette.offset, palette.size));
    std::vector<std::uint16_t> result;
    result.reserve(palette.size / 2);
    for (std::size_t i = 0; i < palette.size / 2; ++i) {
        result.push_back(reader.read_u16_le());
    }
    return result;
}

std::vector<std::uint8_t> File::read_palette_data(
    std::size_t palette_id) const {
    if (palette_id >= palettes_.size()) {
        throw std::out_of_range("model palette index is outside the model");
    }
    const Palette& palette = palettes_[palette_id];
    if (palette.offset > palette_bytes_.size()
        || palette.size > palette_bytes_.size() - palette.offset) {
        throw std::runtime_error("model palette is outside the file");
    }
    const auto begin = palette_bytes_.begin()
        + static_cast<std::ptrdiff_t>(palette.offset);
    return std::vector<std::uint8_t>(
        begin, begin + static_cast<std::ptrdiff_t>(palette.size));
}

void File::parse() {
    const std::span<const std::uint8_t> bytes(bytes_);
    header_ = read_header(bytes);

    read_array(bytes, header_.mesh_offset, header_.mesh_count, Mesh::Size,
               meshes_, [](core::BinaryReader& value) {
                   return Mesh{value.read_u16_le(), value.read_u16_le()};
               });
    read_array(bytes, header_.material_offset, header_.material_count,
               Material::Size, materials_, [](core::BinaryReader& value) {
                   Material material;
                   material.name = value.read_raw_string(64);
                   material.lighting = value.read_u8();
                   material.init_lighting = material.lighting;
                   material.culling = value.read_u8();
                   material.alpha = value.read_u8();
                   material.wireframe = value.read_u8();
                   material.palette_id = value.read_i16_le();
                   material.texture_id = value.read_i16_le();
                   material.x_repeat = value.read_u8();
                   material.y_repeat = value.read_u8();
                   material.diffuse = ColorRgb{value.read_u8(), value.read_u8(),
                                              value.read_u8()};
                   material.ambient = ColorRgb{value.read_u8(), value.read_u8(),
                                               value.read_u8()};
                   material.specular = ColorRgb{value.read_u8(), value.read_u8(),
                                                value.read_u8()};
                   material.padding_53 = value.read_u8();
                   material.polygon_mode = value.read_u32_le();
                   material.render_mode = value.read_u8();
                   material.animation_flags = value.read_u8();
                   material.padding_5a = value.read_u16_le();
                   material.texcoord_transform_mode = value.read_u32_le();
                   material.texcoord_animation_id = value.read_u16_le();
                   material.padding_62 = value.read_u16_le();
                   material.matrix_id = value.read_u32_le();
                   material.scale_s = read_fixed(value);
                   material.scale_t = read_fixed(value);
                   material.rotate_z = value.read_u16_le();
                   material.padding_72 = value.read_u16_le();
                   material.translate_s = read_fixed(value);
                   material.translate_t = read_fixed(value);
                   material.material_animation_id = value.read_u16_le();
                   material.texture_animation_id = value.read_u16_le();
                   material.packed_repeat_mode = value.read_u8();
                   material.padding_81 = value.read_u8();
                   material.padding_82 = value.read_u16_le();
                   return material;
               });
    // Display lists are indexed by mesh in the source format, even though
    // the header does not carry a separate display-list count.
    read_array(bytes, header_.display_list_offset, header_.mesh_count,
               DisplayList::Size, display_lists_, [](core::BinaryReader& value) {
                   return DisplayList{
                       value.read_u32_le(), value.read_u32_le(),
                       read_vector3(value), read_vector3(value)
                   };
               });
    instructions_.reserve(display_lists_.size());
    for (const DisplayList& display_list : display_lists_) {
        instructions_.push_back(read_instructions(bytes, display_list));
    }
    const std::span<const std::uint8_t> table_bytes(table_bytes_);
    const Header table_header = read_header(table_bytes);
    read_array(table_bytes, table_header.texture_offset,
               table_header.texture_count,
               Texture::Size, textures_, [](core::BinaryReader& value) {
                   Texture texture;
                   texture.format = value.read_u8();
                   texture.padding_1 = value.read_u8();
                   texture.width = value.read_u16_le();
                   texture.height = value.read_u16_le();
                   texture.padding_6 = value.read_u16_le();
                   texture.image_offset = value.read_u32_le();
                   texture.image_size = value.read_u32_le();
                   texture.unused_offset = value.read_u32_le();
                   texture.unused_count = value.read_u32_le();
                   texture.vram_offset = value.read_u32_le();
                   texture.opaque = value.read_u32_le();
                   texture.skip_vram = value.read_u32_le();
                   texture.packed_size = value.read_u8();
                   texture.native_texture_format = value.read_u8();
                   texture.object_ref = value.read_u16_le();
                   return texture;
               });
    read_array(table_bytes, table_header.palette_offset,
               table_header.palette_count,
               Palette::Size, palettes_, [](core::BinaryReader& value) {
                   return Palette{value.read_u32_le(), value.read_u32_le(),
                                  value.read_u32_le(), value.read_u32_le()};
               });
    read_array(bytes, header_.node_offset, header_.node_count, Node::Size,
               nodes_, [](core::BinaryReader& value) {
                   Node node;
                   node.name = value.read_raw_string(64);
                   node.parent_id = value.read_i16_le();
                   node.child_id = value.read_i16_le();
                   node.next_id = value.read_i16_le();
                   value.skip(2);
                   node.enabled = value.read_u32_le();
                   node.mesh_count = value.read_u16_le();
                   node.mesh_id = value.read_u16_le();
                   node.scale = read_vector3(value);
                   node.angle_x = value.read_i16_le();
                   node.angle_y = value.read_i16_le();
                   node.angle_z = value.read_i16_le();
                   value.skip(2);
                   node.position = read_vector3(value);
                   node.bounding_radius = read_fixed(value);
                   node.min_bounds = read_vector3(value);
                   node.max_bounds = read_vector3(value);
                   node.billboard_mode = value.read_u32_le();
                   return node;
               });

    read_array(bytes, header_.node_weight_offset, header_.node_weight_count,
               sizeof(std::int32_t), node_weights_,
               [](core::BinaryReader& value) { return value.read_i32_le(); });
    read_array(bytes, header_.node_initial_position, header_.node_count,
               sizeof(formats::Vector3Fx), node_init_pos_,
               [](core::BinaryReader& value) { return read_vector3(value); });
    read_array(bytes, header_.node_position, header_.node_count,
               sizeof(formats::Vector3Fx), node_positions_,
               [](core::BinaryReader& value) { return read_vector3(value); });

    std::size_t position_count = header_.node_weight_count;
    if (header_.node_position_counts != 0
        && header_.node_weight_count == 0) {
        if (header_.node_position_counts < Header::Size) {
            throw std::runtime_error(
                "model node-position count table precedes its header");
        }
        position_count = (header_.node_position_counts - Header::Size)
            / sizeof(std::int32_t);
    }
    read_array(bytes, header_.node_position_counts, position_count,
               sizeof(std::int32_t), node_position_counts_,
               [](core::BinaryReader& value) { return value.read_i32_le(); });

    std::int64_t max_position_index = -1;
    if (header_.node_position_counts != 0) {
        if (node_weights_.size() < header_.node_weight_count
            || node_position_counts_.size() < header_.node_weight_count) {
            throw std::runtime_error(
                "model node position tables are shorter than their weights");
        }
        for (std::size_t index = 0;
             index < header_.node_weight_count; ++index) {
            const auto weight = node_weights_[index];
            const auto count = node_position_counts_[index];
            if (weight < 0 || count < 0) {
                throw std::runtime_error(
                    "model node position table contains a negative index");
            }
            if (count > 0) {
                const auto end = static_cast<std::int64_t>(weight)
                    + static_cast<std::int64_t>(count) - 1;
                max_position_index = std::max(max_position_index, end);
            }
        }
    }
    const std::size_t scale_count = max_position_index < 0
        ? 0
        : static_cast<std::size_t>(max_position_index + 1);
    read_array(bytes, header_.node_position_scales, scale_count,
               sizeof(formats::Fixed), node_position_scales_,
               [](core::BinaryReader& value) { return read_fixed(value); });
}

} // namespace fruityprime::model
