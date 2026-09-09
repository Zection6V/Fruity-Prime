#include "Formats/model_format.hpp"
#include "Formats/model_instance.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {

void require(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

void put_u16(std::vector<std::uint8_t>& bytes, std::size_t offset,
             std::uint16_t value) {
    bytes.at(offset) = static_cast<std::uint8_t>(value);
    bytes.at(offset + 1) = static_cast<std::uint8_t>(value >> 8);
}

void put_i16(std::vector<std::uint8_t>& bytes, std::size_t offset,
             std::int16_t value) {
    put_u16(bytes, offset, static_cast<std::uint16_t>(value));
}

void put_u32(std::vector<std::uint8_t>& bytes, std::size_t offset,
             std::uint32_t value) {
    bytes.at(offset) = static_cast<std::uint8_t>(value);
    bytes.at(offset + 1) = static_cast<std::uint8_t>(value >> 8);
    bytes.at(offset + 2) = static_cast<std::uint8_t>(value >> 16);
    bytes.at(offset + 3) = static_cast<std::uint8_t>(value >> 24);
}

void put_i32(std::vector<std::uint8_t>& bytes, std::size_t offset,
             std::int32_t value) {
    put_u32(bytes, offset, static_cast<std::uint32_t>(value));
}

void put_vector3(std::vector<std::uint8_t>& bytes, std::size_t offset,
                 std::int32_t x, std::int32_t y, std::int32_t z) {
    put_i32(bytes, offset, x);
    put_i32(bytes, offset + 4, y);
    put_i32(bytes, offset + 8, z);
}

void put_name(std::vector<std::uint8_t>& bytes, std::size_t offset,
              std::size_t width, const char* value) {
    for (std::size_t i = 0; value[i] != 0 && i < width; ++i) {
        bytes.at(offset + i) = static_cast<std::uint8_t>(value[i]);
    }
}

constexpr std::size_t NodeNameOffset = 192;

std::vector<std::uint8_t> make_model() {
    constexpr std::size_t mesh_offset = 100;
    constexpr std::size_t dlist_offset = 104;
    constexpr std::size_t texture_offset = 136;
    constexpr std::size_t palette_offset = 176;
    constexpr std::size_t node_offset = NodeNameOffset;
    constexpr std::size_t material_offset =
        node_offset + fruityprime::model::Node::Size;
    constexpr std::size_t image_data_offset = 600;
    constexpr std::size_t palette_data_offset = 608;
    constexpr std::size_t node_table_end = 656;
    std::vector<std::uint8_t> bytes(node_table_end, 0);

    put_u32(bytes, 0, 2);
    put_i32(bytes, 4, 0x1000);
    put_u32(bytes, 8, 3);
    put_u32(bytes, 12, 4);
    put_u32(bytes, 16, material_offset);
    put_u32(bytes, 20, dlist_offset);
    put_u32(bytes, 24, node_offset);
    put_u16(bytes, 28, 1);
    put_u32(bytes, 32, 620);
    put_u32(bytes, 36, mesh_offset);
    put_u16(bytes, 40, 1);
    put_u32(bytes, 44, texture_offset);
    put_u16(bytes, 48, 1);
    put_u32(bytes, 52, palette_offset);
    put_u32(bytes, 56, 624);
    put_u32(bytes, 60, 628);
    put_u32(bytes, 64, 632);
    put_u32(bytes, 68, 644);
    put_u16(bytes, 72, 1);
    put_u16(bytes, 74, 1);
    put_u16(bytes, 96, 1);

    put_u16(bytes, mesh_offset, 7);
    put_u16(bytes, mesh_offset + 2, 0);

    put_u32(bytes, dlist_offset, 400);
    put_u32(bytes, dlist_offset + 4, 20);
    put_vector3(bytes, dlist_offset + 8, -0x1000, 0, 0x1000);
    put_vector3(bytes, dlist_offset + 20, 0x2000, 0x3000, 0x4000);
    put_u32(bytes, 400, 0x41232040);
    put_u32(bytes, 404, 0);
    put_u32(bytes, 408, 0x11223344);
    put_u32(bytes, 412, 0x20001000);
    put_u32(bytes, 416, 0x3000);

    bytes[texture_offset] = 5;
    put_u16(bytes, texture_offset + 2, 2);
    put_u16(bytes, texture_offset + 4, 2);
    put_u32(bytes, texture_offset + 8, image_data_offset);
    put_u32(bytes, texture_offset + 12, 8);
    bytes[texture_offset + 32] = 3;
    bytes[texture_offset + 33] = 4;
    put_u16(bytes, texture_offset + 34, 9);

    put_u32(bytes, palette_offset, palette_data_offset);
    put_u32(bytes, palette_offset + 4, 8);
    put_u32(bytes, palette_offset + 8, 512);
    put_u32(bytes, palette_offset + 12, 11);
    put_u16(bytes, image_data_offset, 0x8001);
    put_u16(bytes, image_data_offset + 2, 0);
    put_u16(bytes, image_data_offset + 4, 0x83e0);
    put_u16(bytes, image_data_offset + 6, 0xffff);
    put_u16(bytes, palette_data_offset, 0x8001);
    put_u16(bytes, palette_data_offset + 2, 0x83e0);
    put_u16(bytes, palette_data_offset + 4, 0xfc00);
    put_u16(bytes, palette_data_offset + 6, 0xffff);

    const std::string name = "root";
    for (std::size_t i = 0; i < name.size(); ++i) {
        bytes[node_offset + i] = static_cast<std::uint8_t>(name[i]);
    }
    put_i16(bytes, node_offset + 64, -1);
    put_i16(bytes, node_offset + 66, -1);
    put_i16(bytes, node_offset + 68, -1);
    put_u32(bytes, node_offset + 72, 1);
    put_u16(bytes, node_offset + 76, 1);
    put_u16(bytes, node_offset + 78, 0);
    put_vector3(bytes, node_offset + 80, 0x1000, 0x1000, 0x1000);
    put_i16(bytes, node_offset + 92, 10);
    put_i16(bytes, node_offset + 94, 20);
    put_i16(bytes, node_offset + 96, 30);
    put_vector3(bytes, node_offset + 100, 0x1000, 0x2000, 0x3000);
    put_i32(bytes, node_offset + 112, 0x4000);
    put_vector3(bytes, node_offset + 116, -0x1000, -0x2000, -0x3000);
    put_vector3(bytes, node_offset + 128, 0x1000, 0x2000, 0x3000);
    put_u32(bytes, node_offset + 140, 2);
    put_i32(bytes, 620, 0); // node weight -> root
    put_i32(bytes, 624, 1); // one streamed position
    put_i32(bytes, 628, 0x1000);
    put_vector3(bytes, 632, 0x1000, 0x2000, 0x3000);
    put_vector3(bytes, 644, 0x4000, 0x5000, 0x6000);
    put_name(bytes, material_offset, 64, "mat");
    bytes[material_offset + 64] = 1;
    bytes[material_offset + 65] = 2;
    bytes[material_offset + 66] = 3;
    bytes[material_offset + 67] = 4;
    put_i16(bytes, material_offset + 68, 5);
    put_i16(bytes, material_offset + 70, 6);
    bytes[material_offset + 74] = 10;
    bytes[material_offset + 75] = 20;
    bytes[material_offset + 76] = 30;
    bytes[material_offset + 77] = 40;
    bytes[material_offset + 78] = 50;
    bytes[material_offset + 79] = 60;
    bytes[material_offset + 80] = 70;
    bytes[material_offset + 81] = 80;
    bytes[material_offset + 82] = 90;
    put_u32(bytes, material_offset + 84, 2);
    put_u32(bytes, material_offset + 92, 3);
    put_u32(bytes, material_offset + 100, 4);
    put_i32(bytes, material_offset + 104, 0x1000);
    put_i32(bytes, material_offset + 108, 0x2000);
    put_i32(bytes, material_offset + 116, 0x3000);
    put_i32(bytes, material_offset + 120, 0x4000);
    return bytes;
}

std::vector<std::uint8_t> make_animations() {
    // Keep one record of every animation kind in a compact synthetic file.
    // The layout follows Read.LoadAnimation: LUTs and records precede their
    // group header, and the top-level header points at four offset tables.
    constexpr std::size_t node_table = 24;
    constexpr std::size_t material_table = 28;
    constexpr std::size_t texcoord_table = 32;
    constexpr std::size_t texture_table = 36;
    constexpr std::size_t node_scales = 40;
    constexpr std::size_t node_rotations = 48;
    constexpr std::size_t node_translations = 52;
    constexpr std::size_t node_animation = 60;
    constexpr std::size_t node_group = 108;
    constexpr std::size_t material_group = 128;
    constexpr std::size_t material_colors = 148;
    constexpr std::size_t material_animation = 152;
    constexpr std::size_t texcoord_group = 320;
    constexpr std::size_t texcoord_scales = 348;
    constexpr std::size_t texcoord_rotations = 356;
    constexpr std::size_t texcoord_translations = 360;
    constexpr std::size_t texcoord_animation = 368;
    constexpr std::size_t texture_group = 500;
    constexpr std::size_t texture_frame_indices = 532;
    constexpr std::size_t texture_ids = 536;
    constexpr std::size_t palette_ids = 540;
    constexpr std::size_t texture_animation = 544;
    std::vector<std::uint8_t> bytes(700, 0);

    put_u32(bytes, 0, node_table);
    put_u32(bytes, 8, material_table);
    put_u32(bytes, 12, texcoord_table);
    put_u32(bytes, 16, texture_table);
    put_u32(bytes, node_table, static_cast<std::uint32_t>(node_group));
    put_u32(bytes, material_table,
            static_cast<std::uint32_t>(material_group));
    put_u32(bytes, texcoord_table,
            static_cast<std::uint32_t>(texcoord_group));
    put_u32(bytes, texture_table, static_cast<std::uint32_t>(texture_group));
    put_u16(bytes, 20, 1);

    put_i32(bytes, node_scales, 0x1000);
    put_i32(bytes, node_scales + 4, 0x2000);
    put_u16(bytes, node_rotations, 0);
    put_u16(bytes, node_rotations + 2, 0x4000);
    put_i32(bytes, node_translations, 0x3000);
    put_i32(bytes, node_translations + 4, -0x1000);
    bytes[node_animation] = 1;
    bytes[node_animation + 1] = 2;
    bytes[node_animation + 2] = 3;
    put_u16(bytes, node_animation + 4, 1);
    put_u16(bytes, node_animation + 6, 1);
    put_u16(bytes, node_animation + 8, 1);
    put_u16(bytes, node_animation + 10, 0);
    put_u16(bytes, node_animation + 12, 1);
    put_u16(bytes, node_animation + 14, 0);
    bytes[node_animation + 16] = 4;
    bytes[node_animation + 17] = 5;
    bytes[node_animation + 18] = 6;
    put_u16(bytes, node_animation + 20, 1);
    put_u16(bytes, node_animation + 22, 1);
    put_u16(bytes, node_animation + 24, 1);
    put_u16(bytes, node_animation + 26, 0);
    put_u16(bytes, node_animation + 28, 1);
    put_u16(bytes, node_animation + 30, 0);
    bytes[node_animation + 32] = 7;
    bytes[node_animation + 33] = 8;
    bytes[node_animation + 34] = 9;
    put_u16(bytes, node_animation + 36, 1);
    put_u16(bytes, node_animation + 38, 1);
    put_u16(bytes, node_animation + 40, 1);
    put_u16(bytes, node_animation + 42, 0);
    put_u16(bytes, node_animation + 44, 1);
    put_u16(bytes, node_animation + 46, 0);
    put_u32(bytes, node_group, 3);
    put_u32(bytes, node_group + 4, node_scales);
    put_u32(bytes, node_group + 8, node_rotations);
    put_u32(bytes, node_group + 12, node_translations);
    put_u32(bytes, node_group + 16, node_animation);

    put_u32(bytes, material_group, 2);
    put_u32(bytes, material_group + 4, material_colors);
    put_u32(bytes, material_group + 8, 1);
    put_u32(bytes, material_group + 12, material_animation);
    put_u16(bytes, material_group + 16, 1);
    put_u16(bytes, material_group + 18, 0x55);
    bytes[material_colors] = 11;
    bytes[material_colors + 1] = 22;
    bytes[material_colors + 2] = 33;
    bytes[material_colors + 3] = 44;
    put_name(bytes, material_animation, 64, "mat");
    bytes[material_animation + 68] = 1;
    bytes[material_animation + 69] = 1;
    bytes[material_animation + 70] = 1;
    put_u16(bytes, material_animation + 72, 1);
    put_u16(bytes, material_animation + 74, 1);
    put_u16(bytes, material_animation + 76, 1);
    put_u16(bytes, material_animation + 78, 0);
    put_u16(bytes, material_animation + 80, 0);
    put_u16(bytes, material_animation + 82, 0);
    bytes[material_animation + 84] = 1;
    bytes[material_animation + 85] = 1;
    bytes[material_animation + 86] = 1;
    put_u16(bytes, material_animation + 88, 1);
    put_u16(bytes, material_animation + 90, 1);
    put_u16(bytes, material_animation + 92, 1);
    put_u16(bytes, material_animation + 94, 0);
    put_u16(bytes, material_animation + 96, 0);
    put_u16(bytes, material_animation + 98, 0);
    bytes[material_animation + 100] = 1;
    bytes[material_animation + 101] = 1;
    bytes[material_animation + 102] = 1;
    put_u16(bytes, material_animation + 104, 1);
    put_u16(bytes, material_animation + 106, 1);
    put_u16(bytes, material_animation + 108, 1);
    put_u16(bytes, material_animation + 110, 0);
    put_u16(bytes, material_animation + 112, 0);
    put_u16(bytes, material_animation + 114, 0);
    bytes[material_animation + 132] = 1;
    put_u16(bytes, material_animation + 134, 1);
    put_u16(bytes, material_animation + 136, 0);
    put_u16(bytes, material_animation + 138, 4);

    put_u32(bytes, texcoord_group, 5);
    put_u32(bytes, texcoord_group + 4, texcoord_scales);
    put_u32(bytes, texcoord_group + 8, texcoord_rotations);
    put_u32(bytes, texcoord_group + 12, texcoord_translations);
    put_u32(bytes, texcoord_group + 16, 1);
    put_u32(bytes, texcoord_group + 20, texcoord_animation);
    put_u16(bytes, texcoord_group + 24, 2);
    put_u16(bytes, texcoord_group + 26, 0x66);
    put_i32(bytes, texcoord_scales, 0x1800);
    put_i32(bytes, texcoord_scales + 4, 0x2000);
    put_u16(bytes, texcoord_rotations, 0x8000);
    put_u16(bytes, texcoord_rotations + 2, 0);
    put_i32(bytes, texcoord_translations, 0x4000);
    put_i32(bytes, texcoord_translations + 4, -0x2000);
    put_name(bytes, texcoord_animation, 32, "mat");
    bytes[texcoord_animation + 32] = 1;
    bytes[texcoord_animation + 33] = 2;
    put_u16(bytes, texcoord_animation + 34, 1);
    put_u16(bytes, texcoord_animation + 36, 1);
    put_u16(bytes, texcoord_animation + 38, 0);
    put_u16(bytes, texcoord_animation + 40, 1);
    bytes[texcoord_animation + 42] = 3;
    put_u16(bytes, texcoord_animation + 44, 1);
    put_u16(bytes, texcoord_animation + 46, 0);
    bytes[texcoord_animation + 48] = 4;
    bytes[texcoord_animation + 49] = 5;
    put_u16(bytes, texcoord_animation + 50, 1);
    put_u16(bytes, texcoord_animation + 52, 1);
    put_u16(bytes, texcoord_animation + 54, 0);
    put_u16(bytes, texcoord_animation + 56, 1);
    put_u16(bytes, texcoord_animation + 58, 0);

    put_u16(bytes, texture_group, 2);
    put_u16(bytes, texture_group + 2, 2);
    put_u16(bytes, texture_group + 4, 2);
    put_u16(bytes, texture_group + 6, 2);
    put_u16(bytes, texture_group + 8, 1);
    put_u16(bytes, texture_group + 10, 0x77);
    put_u32(bytes, texture_group + 12, texture_frame_indices);
    put_u32(bytes, texture_group + 16, texture_ids);
    put_u32(bytes, texture_group + 20, palette_ids);
    put_u32(bytes, texture_group + 24, texture_animation);
    put_u16(bytes, texture_group + 28, 1);
    put_u16(bytes, texture_group + 30, 0x88);
    put_u16(bytes, texture_frame_indices, 0);
    put_u16(bytes, texture_frame_indices + 2, 1);
    put_u16(bytes, texture_ids, 9);
    put_u16(bytes, texture_ids + 2, 10);
    put_u16(bytes, palette_ids, 19);
    put_u16(bytes, palette_ids + 2, 20);
    put_name(bytes, texture_animation, 32, "mat");
    put_u16(bytes, texture_animation + 32, 2);
    put_u16(bytes, texture_animation + 34, 0);
    put_u16(bytes, texture_animation + 36, 3);
    put_u16(bytes, texture_animation + 38, 4);
    put_u16(bytes, texture_animation + 40, 5);
    put_u16(bytes, texture_animation + 42, 6);
    return bytes;
}

} // namespace

int main() {
    try {
        const fruityprime::model::File model =
            fruityprime::model::File::from_bytes(make_model());
        require(model.header().scale_factor == 2, "model scale factor mismatch");
        require(model.world_scale() > 3.99F && model.world_scale() < 4.01F,
                "model world scale mismatch");
        require(model.header().mesh_count == 1, "model mesh count mismatch");
        require(model.meshes().size() == 1, "model mesh array mismatch");
        require(model.meshes()[0].material_id == 7,
                "model mesh material mismatch");
        require(model.materials().size() == 1
                    && model.materials()[0].name == "mat"
                    && model.materials()[0].diffuse.green == 20,
                "model material mismatch");
        require(model.display_lists().size() == 1,
                "model display-list array mismatch");
        require(model.display_lists()[0].size == 20,
                "model display-list size mismatch");
        require(model.instructions().size() == 1
                    && model.instructions()[0].size() == 4,
                "model instruction list mismatch");
        require(model.instructions()[0][0].code
                    == fruityprime::model::InstructionCode::BeginVtxs
                    && model.instructions()[0][0].arguments[0] == 0,
                "model begin instruction mismatch");
        require(model.instructions()[0][1].code
                    == fruityprime::model::InstructionCode::Color
                    && model.instructions()[0][1].arguments[0] == 0x11223344,
                "model color instruction mismatch");
        require(model.instructions()[0][2].code
                    == fruityprime::model::InstructionCode::Vtx16
                    && model.instructions()[0][3].code
                        == fruityprime::model::InstructionCode::EndVtxs,
                "model vertex instruction mismatch");
        const auto geometry = model.decode_geometry(0);
        require(geometry.size() == 1
                    && geometry[0].type
                        == fruityprime::model::GeometryPrimitiveType::Triangles
                    && geometry[0].vertices.size() == 1,
                "model geometry batch mismatch");
        require(geometry[0].vertices[0].x > 0.99F
                    && geometry[0].vertices[0].y > 1.99F
                    && geometry[0].vertices[0].z > 2.99F,
                "model geometry vertex mismatch");
        require(model.textures().size() == 1 && model.textures()[0].width == 2,
                "model texture mismatch");
        require(model.palettes().size() == 1 && model.palettes()[0].size == 8,
                "model palette mismatch");
        const auto texture = model.decode_texture(0);
        require(texture.size() == 4 && texture[0].data == 0x8001
                    && texture[0].alpha == 255 && texture[1].alpha == 0,
                "model texture decode mismatch");
        const auto palette = model.decode_palette(0);
        require(palette.size() == 4 && palette[2] == 0xfc00,
                "model palette decode mismatch");

        std::vector<std::uint8_t> model_bytes = make_model();
        std::vector<std::uint8_t> table_bytes = make_model();
        std::vector<std::uint8_t> texture_bytes(608, 0);
        std::vector<std::uint8_t> palette_bytes(616, 0);
        std::copy(model_bytes.begin() + 600, model_bytes.begin() + 608,
                  texture_bytes.begin() + 600);
        std::copy(model_bytes.begin() + 608, model_bytes.begin() + 616,
                  palette_bytes.begin() + 608);
        std::fill(model_bytes.begin() + 600, model_bytes.begin() + 616, 0);
        const auto external_resources =
            fruityprime::model::File::from_recolor_resources(
                std::move(model_bytes), std::move(table_bytes),
                std::move(texture_bytes), std::move(palette_bytes));
        require(external_resources.decode_texture(0)[0].data == 0x8001,
                "external texture resource decode mismatch");
        require(external_resources.decode_palette(0)[2] == 0xfc00,
                "external palette resource decode mismatch");
        require(model.nodes().size() == 1 && model.nodes()[0].name == "root",
                "model node name mismatch");
        require(model.nodes()[0].position.x.value == 0x1000,
                "model node position mismatch");
        require(model.nodes()[0].billboard_mode == 2,
                "model node billboard mismatch");
        require(model.node_weights().size() == 1
                    && model.node_weights()[0] == 0
                    && model.node_position_counts().size() == 1
                    && model.node_position_counts()[0] == 1
                    && model.node_position_scales().size() == 1
                    && model.node_position_scales()[0].value == 0x1000
                    && model.node_init_pos().size() == 1
                    && model.node_init_pos()[0].y.value == 0x2000
                    && model.node_positions().size() == 1
                    && model.node_positions()[0].z.value == 0x6000,
                "model node position tables mismatch");

        auto animated_model = fruityprime::model::File::from_bytes(make_model());
        animated_model.load_animations(make_animations(), "synthetic");
        animated_model.append_animations(make_animations(), "synthetic");
        const auto& animation = animated_model.animations();
        require(animation.any()
                    && animation.node_group_offsets.size() == 2
                    && animation.material_group_offsets.size() == 2
                    && animation.texcoord_group_offsets.size() == 2
                    && animation.texture_group_offsets.size() == 2
                    && animation.node_groups.size() == 2
                    && animation.material_groups.size() == 2
                    && animation.texcoord_groups.size() == 2
                    && animation.texture_groups.size() == 2,
                "model animation offset tables mismatch");
        require(animation.node_groups.size() == 2
                    && animation.node_groups[0].frame_count == 3
                    && animation.node_groups[0].animations.count("root") == 1
                    && animation.node_groups[0].scales.size() == 2
                    && animation.node_groups[0].translations[1] < -0.99F
                    && animation.node_groups[0].rotations.size() == 2
                    && std::fabs(animation.node_groups[0].rotations[1]
                                 - 1.57079632679F)
                        < 0.0001F,
                "node animation decode mismatch");
        require(animation.material_groups.size() == 2
                    && animation.material_groups[0].current_frame == 1
                    && animation.material_groups[0].unused_frame == 0x55
                    && animation.material_groups[0].colors.size() == 4
                    && animation.material_groups[0].colors[2] == 33.0F
                    && animation.material_groups[0].animations.count("mat") == 1,
                "material animation decode mismatch");
        require(animation.texcoord_groups.size() == 2
                    && animation.texcoord_groups[0].frame_count == 5
                    && animation.texcoord_groups[0].current_frame == 2
                    && animation.texcoord_groups[0].rotations.size() == 2
                    && std::fabs(animation.texcoord_groups[0].rotations[0]
                                 - 3.14159265359F)
                        < 0.0001F
                    && animation.texcoord_groups[0].animations.count("mat") == 1,
                "texcoord animation decode mismatch");
        require(animation.texture_groups.size() == 2
                    && animation.texture_groups[0].frame_count == 2
                    && animation.texture_groups[0].unused_frame == 0x88
                    && animation.texture_groups[0].frame_indices[1] == 1
                    && animation.texture_groups[0].texture_ids[0] == 9
                    && animation.texture_groups[0].palette_ids[1] == 20
                    && animation.texture_groups[0].animations.count("mat") == 1,
                "texture animation decode mismatch");

        const std::vector<float> interpolation_values{0.0F, 10.0F,
                                                       20.0F, 30.0F};
        require(fruityprime::model::interpolate_animation(
                    interpolation_values, 0, 1, 2, 2, 4)
                    == 5.0F,
                "animation interpolation mismatch");
        const std::vector<float> rotation_values{3.0F, -3.0F};
        const float halfway_rotation = fruityprime::model::interpolate_animation(
            rotation_values, 0, 1, 2, 2, 4, true);
        require(std::fabs(halfway_rotation - 3.14159265359F) < 0.0001F,
                "animation rotation interpolation mismatch");

        const auto& node_group = animation.node_groups[0];
        const auto node_matrix = fruityprime::model::animate_node(
            node_group, node_group.animations.at("root"), 0,
            {1.0F, 1.0F, 1.0F});
        require(std::fabs(node_matrix.m41 - 3.0F) < 0.0001F
                    && std::fabs(node_matrix.m42 + 1.0F) < 0.0001F
                    && std::isfinite(node_matrix.m11),
                "animated node matrix mismatch");
        const auto texcoord_matrix = fruityprime::model::animate_texcoords(
            animation.texcoord_groups[0],
            animation.texcoord_groups[0].animations.at("mat"), 0);
        require(std::isfinite(texcoord_matrix.m11)
                    && std::isfinite(texcoord_matrix.m41)
                    && texcoord_matrix.m11 != 1.0F,
                "animated texcoord matrix mismatch");
        const auto selected_texture = fruityprime::model::select_texture_animation(
            animation.texture_groups[0],
            animation.texture_groups[0].animations.at("mat"), 1);
        require(selected_texture.found && selected_texture.texture_id == 10
                    && selected_texture.palette_id == 20,
                "animated texture selection mismatch");
        require(!fruityprime::model::select_texture_animation(
                     animation.texture_groups[0],
                     animation.texture_groups[0].animations.at("mat"), 3)
                     .found,
                "missing animated texture frame was selected");

        fruityprime::model::ModelInstance instance(animated_model);
        require(instance.animation_info().index[0] == -1
                    && instance.matrix_stack().size() == 1
                    && instance.matrix_stack_values().size() == 16,
                "model instance initial state mismatch");
        instance.set_animation(0, fruityprime::model::AnimationFlags::NoLoop);
        require(instance.animation_info().index[0] == 0
                    && instance.animation_info().prev_index[0] == -1
                    && instance.animation_info().frame[0] == 0
                    && instance.animation_info().frame_count[0] == 3
                    && instance.animation_info().node.group == 0
                    && instance.animation_info().material.group == 0
                    && instance.animation_info().texcoord.group == 0
                    && instance.animation_info().texture.group == 0,
                "model instance animation selection mismatch");
        instance.compute_node_matrices();
        require(std::fabs(instance.node_states()[0].transform.m41 - 0.25F)
                    < 0.0001F,
                "model instance static node transform mismatch");
        instance.animate_nodes();
        require(std::fabs(instance.node_states()[0].animation.m41 - 0.75F)
                    < 0.0001F,
                "model instance animated node transform mismatch");
        instance.update_matrix_stack();
        // The node is a cylinder billboard, so UpdateMatrixStack clears its
        // rotation.  OpenTK's Matrix4.ClearRotation keeps each row's length,
        // which is the node's scale -- it does not normalise to identity.
        // This model's animation asks for scale (1, 2, 1): the Y scale LUT
        // index is 1 and that entry is 0x2000.
        require(instance.matrix_stack()[0].m11 == 1.0F
                    && instance.matrix_stack()[0].m22 == 2.0F
                    && instance.matrix_stack()[0].m33 == 1.0F
                    && instance.matrix_stack()[0].m12 == 0.0F
                    && instance.matrix_stack()[0].m21 == 0.0F
                    && std::fabs(instance.matrix_stack()[0].m41 - 0.75F)
                        < 0.0001F
                    && std::fabs(instance.matrix_stack_values()[12] - 0.75F)
                        < 0.0001F,
                "model instance matrix stack mismatch");
        instance.update_materials();
        require(std::fabs(instance.material_states()[0].current_diffuse.x
                           - 11.0F / 31.0F)
                    < 0.0001F
                    && std::fabs(instance.material_states()[0].current_alpha
                                  - 11.0F / 31.0F)
                        < 0.0001F
                    && instance.material_states()[0].current_texture_id == 9
                    && instance.material_states()[0].current_palette_id == 19
                    && std::isfinite(instance.material_states()[0]
                                         .texcoord_matrix.m11),
                "model instance material animation mismatch");
        instance.update_anim_frames();
        require(instance.animation_info().frame[0] == 1
                    && !fruityprime::model::has_flag(
                        instance.animation_info().flags[0],
                        fruityprime::model::AnimationFlags::Ended),
                "model instance frame advance mismatch");
        instance.update_materials();
        require(instance.material_states()[0].current_texture_id == 10
                    && instance.material_states()[0].current_palette_id == 20,
                "model instance texture frame mismatch");
        instance.update_anim_frames();
        require(instance.animation_info().frame[0] == 2
                    && fruityprime::model::has_flag(
                        instance.animation_info().flags[0],
                        fruityprime::model::AnimationFlags::Ended),
                "model instance no-loop completion mismatch");

        fruityprime::model::ModelInstance partial(animated_model);
        partial.set_animation(0, 1, fruityprime::model::AnimationSetFlags::Node,
                              fruityprime::model::AnimationFlags::Reverse);
        require(partial.animation_info().node.slot == 1
                    && partial.animation_info().node.group == 0
                    && partial.animation_info().frame[1] == 2
                    && partial.animation_info().frame_count[1] == 3,
                "model instance partial animation selection mismatch");

        // SceneSetup.GetNodeLayer.
        require(fruityprime::model::node_layer_mask(
                    /*single_player=*/false, 0, 2, false) == (0x20 | 0x8),
                "two-player node layer mask mismatch");
        require(fruityprime::model::node_layer_mask(
                    /*single_player=*/false, 0, 4, false) == (0x20 | 0x10),
                "four-player node layer mask mismatch");
        require(fruityprime::model::node_layer_mask(
                    /*single_player=*/false, 0, 4, true)
                    == (0x20 | 0x10 | 0x4000),
                "capture node layer mask mismatch");
        require(fruityprime::model::node_layer_mask(
                    /*single_player=*/true, 1, 1, false) == (1 << 7),
                "story node layer mask mismatch");
        require(fruityprime::model::node_layer_mask(
                    /*single_player=*/true, 0, 1, false) == 0,
                "story node layer mask without a room layer mismatch");

        // Model.FilterNodes.  The last case is the one the managed comment
        // calls out: stepping four characters at a time means "_ml_s010blocks"
        // never sees the "_s01" it contains, so it is visible nowhere.
        struct FilterCase {
            const char* name;
            int mask;
            bool enabled;
        };
        const FilterCase filter_cases[] = {
            {"root", 0x20 | 0x10, true},        // no leading underscore
            {"_ml0", 0x20 | 0x8, true},
            {"_ml0", 0x20 | 0x10, false},
            {"_ml1", 0x20 | 0x10, true},
            {"_mpu", 0x20 | 0x10, true},
            {"_ctf", 0x20 | 0x10, false},
            {"_ctf", 0x20 | 0x10 | 0x4000, true},
            {"_s01", 1 << 7, true},
            {"_s01", 0x20 | 0x10, false},
            {"_ml_s010blocks", 0x20 | 0x10, false},
            {"_ml_s010blocks", 1 << 7, false}
        };
        for (const FilterCase& filter_case : filter_cases) {
            std::vector<std::uint8_t> filter_bytes = make_model();
            for (std::size_t i = 0; i < 64; ++i) {
                filter_bytes.at(NodeNameOffset + i) = 0;
            }
            const std::string filter_name = filter_case.name;
            for (std::size_t i = 0; i < filter_name.size(); ++i) {
                filter_bytes.at(NodeNameOffset + i) =
                    static_cast<std::uint8_t>(filter_name[i]);
            }
            auto filtered = fruityprime::model::File::from_bytes(
                std::move(filter_bytes));
            filtered.filter_nodes(filter_case.mask);
            require((filtered.nodes().at(0).enabled != 0)
                        == filter_case.enabled,
                    "node layer filtering mismatch");
        }

        // Renderer.DoDlist pins a room's matrix ID at 0 so room node
        // transforms stay togglable.
        const auto room_model = fruityprime::model::File::from_bytes(
            make_model());
        for (std::size_t list = 0; list < room_model.instructions().size();
             ++list) {
            for (const auto& primitive : room_model.decode_geometry(
                     list, 8, 8, false, /*is_room=*/true)) {
                for (const auto& vertex : primitive.vertices) {
                    require(vertex.matrix_id == 0,
                            "room display list kept a non-zero matrix ID");
                }
            }
        }

        bool rejected = false;
        try {
            static_cast<void>(fruityprime::model::File::from_bytes(
                std::vector<std::uint8_t>(99)));
        } catch (const std::runtime_error&) {
            rejected = true;
        }
        require(rejected, "short model was accepted");
        std::cout << "native model format tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "native model format tests failed: " << error.what()
                  << '\n';
        return 1;
    }
}
