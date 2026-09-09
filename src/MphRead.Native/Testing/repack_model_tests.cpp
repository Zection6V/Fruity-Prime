#include "Formats/raw_formats.hpp"
#include "Read.hpp"
#include "Utility/repack_model.hpp"

#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <span>
#include <string>
#include <vector>

namespace {

template <std::size_t N>
void set_name(std::array<std::uint8_t, N>& destination,
              const char* value) {
    const std::size_t length = std::strlen(value);
    assert(length < N);
    std::memcpy(destination.data(), value, length);
}

void put_u16(std::vector<std::uint8_t>& bytes, std::size_t offset,
             std::uint16_t value) {
    bytes[offset] = static_cast<std::uint8_t>(value);
    bytes[offset + 1] = static_cast<std::uint8_t>(value >> 8);
}

void put_u32(std::vector<std::uint8_t>& bytes, std::size_t offset,
             std::uint32_t value) {
    bytes[offset] = static_cast<std::uint8_t>(value);
    bytes[offset + 1] = static_cast<std::uint8_t>(value >> 8);
    bytes[offset + 2] = static_cast<std::uint8_t>(value >> 16);
    bytes[offset + 3] = static_cast<std::uint8_t>(value >> 24);
}

void put_i32(std::vector<std::uint8_t>& bytes, std::size_t offset,
             std::int32_t value) {
    put_u32(bytes, offset, static_cast<std::uint32_t>(value));
}

void append_u8(std::vector<std::uint8_t>& bytes, std::uint8_t value) {
    bytes.push_back(value);
}

void append_u16(std::vector<std::uint8_t>& bytes, std::uint16_t value) {
    append_u8(bytes, static_cast<std::uint8_t>(value));
    append_u8(bytes, static_cast<std::uint8_t>(value >> 8));
}

void append_u32(std::vector<std::uint8_t>& bytes, std::uint32_t value) {
    append_u8(bytes, static_cast<std::uint8_t>(value));
    append_u8(bytes, static_cast<std::uint8_t>(value >> 8));
    append_u8(bytes, static_cast<std::uint8_t>(value >> 16));
    append_u8(bytes, static_cast<std::uint8_t>(value >> 24));
}

void append_i32(std::vector<std::uint8_t>& bytes, std::int32_t value) {
    append_u32(bytes, static_cast<std::uint32_t>(value));
}

std::vector<std::uint8_t> synthetic_model() {
    std::vector<std::uint8_t> bytes(100, 0);

    // The no-weight model form still emits one implicit matrix ID before the
    // texture stream. This is the normal small-model path in RepackModel.cs.
    append_u32(bytes, 0);
    const std::uint32_t image_offset = static_cast<std::uint32_t>(bytes.size());
    append_u8(bytes, 0);

    const std::uint32_t texture_offset = static_cast<std::uint32_t>(bytes.size());
    bytes.resize(bytes.size() + 40, 0);
    put_u16(bytes, texture_offset + 2, 2);
    put_u16(bytes, texture_offset + 4, 2);
    put_u32(bytes, texture_offset + 8, image_offset);
    put_u32(bytes, texture_offset + 12, 1);
    put_u32(bytes, texture_offset + 28, 1);

    const std::uint32_t palette_data_offset =
        static_cast<std::uint32_t>(bytes.size());
    append_u16(bytes, 0x001f);
    const std::uint32_t palette_offset = static_cast<std::uint32_t>(bytes.size());
    bytes.resize(bytes.size() + 16, 0);
    put_u32(bytes, palette_offset, palette_data_offset);
    put_u32(bytes, palette_offset + 4, 2);

    const std::uint32_t display_data_offset =
        static_cast<std::uint32_t>(bytes.size());
    // BEGIN(TRIANGLES), three VTX_16 records, END, and three NOPs. The
    // hardware command stream groups four opcodes per command word.
    append_u32(bytes, 0x23232340);
    append_u32(bytes, 0);
    append_u32(bytes, 0);
    append_u32(bytes, 0);
    append_u32(bytes, 4096);
    append_u32(bytes, 0);
    append_u32(bytes, 4096U << 16);
    append_u32(bytes, 0);
    append_u32(bytes, 0x00000041);
    append_u32(bytes, 0);
    const std::uint32_t display_data_size =
        static_cast<std::uint32_t>(bytes.size()) - display_data_offset;

    const std::uint32_t display_offset = static_cast<std::uint32_t>(bytes.size());
    bytes.resize(bytes.size() + 32, 0);
    put_u32(bytes, display_offset, display_data_offset);
    put_u32(bytes, display_offset + 4, display_data_size);
    put_u32(bytes, display_offset + 20, 4096);
    put_u32(bytes, display_offset + 24, 4096);

    const std::uint32_t material_offset = static_cast<std::uint32_t>(bytes.size());
    bytes.resize(bytes.size() + 132, 0);
    bytes[material_offset + 64] = 1; // lighting
    bytes[material_offset + 65] = 2; // back-face culling
    put_u16(bytes, material_offset + 68, 0); // palette ID
    put_u16(bytes, material_offset + 70, 0); // texture ID
    put_i32(bytes, material_offset + 104, 4096);
    put_i32(bytes, material_offset + 108, 4096);

    const std::uint32_t node_offset = static_cast<std::uint32_t>(bytes.size());
    bytes.resize(bytes.size() + 240, 0);
    put_u16(bytes, node_offset + 64, 0xffff);
    put_u16(bytes, node_offset + 66, 0xffff);
    put_u16(bytes, node_offset + 68, 0xffff);
    put_u32(bytes, node_offset + 72, 1);
    put_u16(bytes, node_offset + 76, 1);
    put_u16(bytes, node_offset + 78, 0);
    put_i32(bytes, node_offset + 80, 4096);
    put_i32(bytes, node_offset + 84, 4096);
    put_i32(bytes, node_offset + 88, 4096);
    put_i32(bytes, node_offset + 112, 4096);
    put_i32(bytes, node_offset + 128, 4096);
    put_i32(bytes, node_offset + 132, 4096);
    put_i32(bytes, node_offset + 136, 0);

    const std::uint32_t mesh_offset = static_cast<std::uint32_t>(bytes.size());
    append_u16(bytes, 0);
    append_u16(bytes, 0);

    put_u32(bytes, 0, 0); // scale factor
    put_i32(bytes, 4, 4096); // scale base
    put_u32(bytes, 8, 1); // primitive count
    put_u32(bytes, 12, 3); // vertex count
    put_u32(bytes, 16, material_offset);
    put_u32(bytes, 20, display_offset);
    put_u32(bytes, 24, node_offset);
    put_u16(bytes, 28, 0); // no explicit node weights
    put_u32(bytes, 32, 0);
    put_u32(bytes, 36, mesh_offset);
    put_u16(bytes, 40, 1);
    put_u32(bytes, 44, texture_offset);
    put_u16(bytes, 48, 1);
    put_u32(bytes, 52, palette_offset);
    put_u16(bytes, 72, 1);
    put_u16(bytes, 74, 1);
    put_u16(bytes, 96, 1);
    put_u16(bytes, 98, 0);
    return bytes;
}

void test_model_repack() {
    using fruityprime::model::File;
    using fruityprime::utility::repack_model::BoundsMode;
    using fruityprime::utility::repack_model::ModelPackOptions;
    using fruityprime::utility::repack_model::TextureStorage;
    using fruityprime::utility::repack_model::pack_model;

    const auto source_bytes = synthetic_model();
    const auto source = File::from_bytes(source_bytes);
    const auto inline_result = pack_model(source);
    assert(inline_result.model == source_bytes);
    assert(inline_result.texture.empty());

    const auto separate_result = pack_model(
        source, ModelPackOptions{TextureStorage::Separate, false,
                                 BoundsMode::None});
    assert(!separate_result.texture.empty());
    const auto reparsed = File::from_resources(
        separate_result.model, separate_result.texture);
    assert(reparsed.textures().size() == 1);
    assert(reparsed.palettes().size() == 1);
    assert(reparsed.read_texture_data(0) == std::vector<std::uint8_t>{0});
    const std::vector<std::uint8_t> expected_palette{0x1f, 0x00};
    assert(reparsed.read_palette_data(0) == expected_palette);

    const auto uncapped = pack_model(
        source, ModelPackOptions{TextureStorage::Inline, false,
                                 BoundsMode::Uncapped});
    const auto uncapped_file = File::from_bytes(uncapped.model);
    assert(uncapped_file.display_lists()[0].max_bounds.x.value == 4096);
    assert(uncapped_file.nodes()[0].max_bounds.x.value == 4096);
}

fruityprime::model::AnimationResults make_animation_results() {
    fruityprime::model::AnimationResults result;
    result.node_group_offsets = {1, 0};
    result.material_group_offsets = {1, 0};
    result.texcoord_group_offsets = {1, 0};
    result.texture_group_offsets = {1, 0};
    result.node_groups.resize(2);
    result.material_groups.resize(2);
    result.texcoord_groups.resize(2);
    result.texture_groups.resize(2);

    auto& node = result.node_groups[0];
    node.frame_count = 3;
    node.scales = {1.0F, -1.0F};
    node.rotations = {0.0F, 1.57079632679489661923F,
                      3.14159265358979323846F};
    node.translations = {0.25F, -0.5F};
    fruityprime::raw::NodeAnimation node_animation;
    node_animation.scale_blend_x = 1;
    node_animation.scale_lut_length_x = 2;
    node_animation.scale_lut_index_x = 4;
    node_animation.rotate_blend_z = 3;
    node_animation.translate_lut_length_y = 5;
    node.animations.emplace("node", node_animation);

    auto& material = result.material_groups[0];
    material.frame_count = 4;
    material.current_frame = 2;
    material.unused_frame = 0x1234;
    material.colors = {1.0F, 2.0F, 3.0F};
    fruityprime::raw::MaterialAnimation material_animation;
    set_name(material_animation.name, "material");
    material_animation.diffuse_blend_r = 7;
    material_animation.diffuse_lut_length_r = 8;
    material_animation.alpha_blend = 9;
    material_animation.material_id = 10;
    material.animations.emplace("material", material_animation);

    auto& texcoord = result.texcoord_groups[0];
    texcoord.frame_count = 5;
    texcoord.current_frame = 3;
    texcoord.unused_frame = 0x2345;
    texcoord.scales = {1.0F};
    texcoord.rotations = {0.5F};
    texcoord.translations = {-0.25F, 0.75F};
    fruityprime::raw::TexcoordAnimation texcoord_animation;
    set_name(texcoord_animation.name, "uv");
    texcoord_animation.scale_blend_s = 2;
    texcoord_animation.rotate_lut_index_z = 6;
    texcoord_animation.translate_lut_length_t = 7;
    texcoord.animations.emplace("uv", texcoord_animation);

    auto& texture = result.texture_groups[0];
    texture.frame_count = 6;
    texture.current_frame = 4;
    texture.unused_frame = 0x3456;
    texture.unused_a = 0x4567;
    texture.frame_indices = {11};
    texture.texture_ids = {12};
    texture.palette_ids = {13};
    fruityprime::raw::TextureAnimation texture_animation;
    set_name(texture_animation.name, "texture");
    texture_animation.count = 1;
    texture_animation.start_index = 2;
    texture_animation.material_id = 3;
    texture_animation.minimum_texture_id = 4;
    texture.animations.emplace("texture", texture_animation);

    return result;
}

} // namespace

int main() {
    using fruityprime::model::AnimationResults;
    using fruityprime::raw::AnimationHeader;
    using fruityprime::raw::RawMaterialAnimationGroup;
    using fruityprime::raw::RawNodeAnimationGroup;
    using fruityprime::raw::RawTexcoordAnimationGroup;
    using fruityprime::raw::RawTextureAnimationGroup;
    using fruityprime::read::do_offset;
    using fruityprime::read::do_offsets;
    using fruityprime::read::read_struct;
    using fruityprime::utility::repack_model::pack_animation;

    const AnimationResults source = make_animation_results();
    const auto bytes = pack_animation(source);
    const auto header = read_struct<AnimationHeader>(bytes);
    assert(header.count == 2);
    assert(header.padding_16 == 0);
    assert(header.node_group_offset != 0);
    assert(header.unused_group_offset != 0);
    assert(header.material_group_offset != 0);
    assert(header.texcoord_group_offset != 0);
    assert(header.texture_group_offset != 0);

    const auto node_offsets = do_offsets<std::uint32_t>(
        bytes, header.node_group_offset, header.count);
    const auto material_offsets = do_offsets<std::uint32_t>(
        bytes, header.material_group_offset, header.count);
    const auto texcoord_offsets = do_offsets<std::uint32_t>(
        bytes, header.texcoord_group_offset, header.count);
    const auto texture_offsets = do_offsets<std::uint32_t>(
        bytes, header.texture_group_offset, header.count);
    const std::vector<std::uint32_t> expected_node_offsets{96, 0};
    assert(node_offsets == expected_node_offsets);
    assert(material_offsets[1] == 0);
    assert(texcoord_offsets[1] == 0);
    assert(texture_offsets[1] == 0);

    const auto node_group = do_offset<RawNodeAnimationGroup>(bytes,
                                                              node_offsets[0]);
    assert(node_group.frame_count == 3);
    assert(node_group.scale_lut_offset == 24);
    assert(node_group.rotate_lut_offset == 32);
    assert(node_group.translate_lut_offset == 40);
    assert(node_group.animation_offset == 48);
    assert(read_struct<std::int32_t>(bytes, node_group.scale_lut_offset)
           == 4096);
    assert(read_struct<std::int32_t>(bytes, node_group.scale_lut_offset + 4)
           == -4096);
    assert(read_struct<std::uint16_t>(bytes, node_group.rotate_lut_offset + 2)
           == 0x4000);

    const auto material_group = do_offset<RawMaterialAnimationGroup>(
        bytes, material_offsets[0]);
    assert(material_group.frame_count == 4);
    assert(material_group.animation_frame == 2);
    assert(material_group.unused_12 == 0x1234);
    assert(bytes[material_group.color_lut_offset] == 1);
    assert(bytes[material_group.color_lut_offset + 3] == 0);

    const auto texcoord_group = do_offset<RawTexcoordAnimationGroup>(
        bytes, texcoord_offsets[0]);
    assert(texcoord_group.frame_count == 5);
    assert(texcoord_group.animation_frame == 3);

    const auto texture_group = do_offset<RawTextureAnimationGroup>(
        bytes, texture_offsets[0]);
    assert(texture_group.frame_count == 6);
    assert(texture_group.frame_index_count == 1);
    assert(texture_group.texture_id_count == 1);
    assert(texture_group.palette_id_count == 1);
    assert(read_struct<std::uint16_t>(bytes, texture_group.frame_index_offset)
           == 11);
    assert(read_struct<std::uint16_t>(bytes, texture_group.texture_id_offset)
           == 12);
    assert(read_struct<std::uint16_t>(bytes, texture_group.palette_id_offset)
           == 13);

    const auto first_hunt = pack_animation(source, true);
    const auto first_hunt_header = read_struct<AnimationHeader>(first_hunt);
    assert(first_hunt_header.padding_16 == 0xcccc);
    const auto first_hunt_node = do_offset<RawNodeAnimationGroup>(
        first_hunt, do_offsets<std::uint32_t>(
                        first_hunt, first_hunt_header.node_group_offset, 2)[0]);
    assert(first_hunt[first_hunt_node.translate_lut_offset - 1] == 0xcc);
    const auto first_hunt_material = do_offset<RawMaterialAnimationGroup>(
        first_hunt, do_offsets<std::uint32_t>(
                         first_hunt, first_hunt_header.material_group_offset,
                         2)[0]);
    assert(first_hunt[first_hunt_material.color_lut_offset + 3] == 0xcc);

    test_model_repack();

    return 0;
}
