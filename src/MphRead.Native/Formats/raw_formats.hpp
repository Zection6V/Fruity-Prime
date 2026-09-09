#pragma once

#include "Formats/Types.hpp"

#include <array>
#include <string>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>

// Fixed-layout cartridge records from Formats/RawFormats.cs.  These are kept
// in a separate namespace from the decoded model/entity APIs: the records are
// byte contracts, while the decoded objects are allowed to own strings and
// vectors.  All records use the DS little-endian field order; callers that
// read bytes on a different host should decode into these members explicitly
// rather than memcpy a file into a C++ object.
namespace fruityprime::raw {

// RawFormats.MarshalExtensions.MarshalString and the NameString / IdString /
// NodeNameString / ModelNameString properties built on it: a fixed-width
// cartridge name field is NUL-padded, so the string is what precedes the
// first NUL rather than the whole field.
template <std::size_t N>
[[nodiscard]] inline std::string marshal_string(
    const std::array<std::uint8_t, N>& field) {
    std::size_t length = 0;
    while (length < N && field[length] != 0) {
        ++length;
    }
    return std::string(reinterpret_cast<const char*>(field.data()), length);
}


// The managed reader exposes these records through Marshal.SizeOf.  Keep a
// small runtime catalogue as well so format tools can report the exact
// serialized contract without depending on C# reflection.
struct LayoutEntry {
    std::string_view name;
    std::size_t managed_size = 0;
    std::size_t native_size = 0;
};

[[nodiscard]] std::span<const LayoutEntry> layout() noexcept;
[[nodiscard]] bool validate_layout() noexcept;

struct RawMesh {
    std::uint16_t material_id = 0;
    std::uint16_t dlist_id = 0;
};

struct DisplayList {
    std::uint32_t offset = 0;
    std::uint32_t size = 0;
    formats::Vector3Fx min_bounds;
    formats::Vector3Fx max_bounds;
};

struct RawMaterial {
    std::array<std::uint8_t, 64> name{};
    std::uint8_t lighting = 0;
    formats::CullingMode culling = formats::CullingMode::Back;
    std::uint8_t alpha = 0;
    std::uint8_t wireframe = 0;
    std::int16_t palette_id = -1;
    std::int16_t texture_id = -1;
    formats::RepeatMode x_repeat = formats::RepeatMode::Clamp;
    formats::RepeatMode y_repeat = formats::RepeatMode::Clamp;
    formats::ColorRgb diffuse;
    formats::ColorRgb ambient;
    formats::ColorRgb specular;
    std::uint8_t padding_53 = 0;
    formats::PolygonMode polygon_mode = formats::PolygonMode::Modulate;
    formats::RenderMode render_mode = formats::RenderMode::Normal;
    std::uint8_t animation_flags = 0;
    std::uint16_t padding_5a = 0;
    formats::TexgenMode texcoord_transform_mode = formats::TexgenMode::None;
    std::uint16_t texcoord_animation_id = 0;
    std::uint16_t padding_62 = 0;
    std::uint32_t matrix_id = 0;
    formats::Fixed scale_s;
    formats::Fixed scale_t;
    std::uint16_t rotate_z = 0;
    std::uint16_t padding_72 = 0;
    formats::Fixed translate_s;
    formats::Fixed translate_t;
    std::uint16_t material_animation_id = 0;
    std::uint16_t texture_animation_id = 0;
    std::uint8_t packed_repeat_mode = 0;
    std::uint8_t padding_81 = 0;
    std::uint16_t padding_82 = 0;
};

struct AnimationHeader {
    std::uint32_t node_group_offset = 0;
    std::uint32_t unused_group_offset = 0;
    std::uint32_t material_group_offset = 0;
    std::uint32_t texcoord_group_offset = 0;
    std::uint32_t texture_group_offset = 0;
    std::uint16_t count = 0;
    std::uint16_t padding_16 = 0;
};

struct RawMaterialAnimationGroup {
    std::uint32_t frame_count = 0;
    std::uint32_t color_lut_offset = 0;
    std::uint32_t animation_count = 0;
    std::uint32_t animation_offset = 0;
    std::uint16_t animation_frame = 0;
    std::uint16_t unused_12 = 0;
};

struct RawTextureAnimationGroup {
    std::uint16_t frame_count = 0;
    std::uint16_t frame_index_count = 0;
    std::uint16_t texture_id_count = 0;
    std::uint16_t palette_id_count = 0;
    std::uint16_t animation_count = 0;
    std::uint16_t unused_0a = 0;
    std::uint32_t frame_index_offset = 0;
    std::uint32_t texture_id_offset = 0;
    std::uint32_t palette_id_offset = 0;
    std::uint32_t animation_offset = 0;
    std::uint16_t animation_frame = 0;
    std::uint16_t unused_1c = 0;
};

struct RawTexcoordAnimationGroup {
    std::uint32_t frame_count = 0;
    std::uint32_t scale_lut_offset = 0;
    std::uint32_t rotate_lut_offset = 0;
    std::uint32_t translate_lut_offset = 0;
    std::uint32_t animation_count = 0;
    std::uint32_t animation_offset = 0;
    std::uint16_t animation_frame = 0;
    std::uint16_t unused_1a = 0;
};

struct RawNodeAnimationGroup {
    std::uint32_t frame_count = 0;
    std::uint32_t scale_lut_offset = 0;
    std::uint32_t rotate_lut_offset = 0;
    std::uint32_t translate_lut_offset = 0;
    std::uint32_t animation_offset = 0;
};

struct MaterialAnimation {
    std::array<std::uint8_t, 64> name{};
    std::uint32_t unused_40 = 0;
    std::uint8_t diffuse_blend_r = 0;
    std::uint8_t diffuse_blend_g = 0;
    std::uint8_t diffuse_blend_b = 0;
    std::uint8_t unused_47 = 0;
    std::uint16_t diffuse_lut_length_r = 0;
    std::uint16_t diffuse_lut_length_g = 0;
    std::uint16_t diffuse_lut_length_b = 0;
    std::uint16_t diffuse_lut_index_r = 0;
    std::uint16_t diffuse_lut_index_g = 0;
    std::uint16_t diffuse_lut_index_b = 0;
    std::uint8_t ambient_blend_r = 0;
    std::uint8_t ambient_blend_g = 0;
    std::uint8_t ambient_blend_b = 0;
    std::uint8_t unused_57 = 0;
    std::uint16_t ambient_lut_length_r = 0;
    std::uint16_t ambient_lut_length_g = 0;
    std::uint16_t ambient_lut_length_b = 0;
    std::uint16_t ambient_lut_index_r = 0;
    std::uint16_t ambient_lut_index_g = 0;
    std::uint16_t ambient_lut_index_b = 0;
    std::uint8_t specular_blend_r = 0;
    std::uint8_t specular_blend_g = 0;
    std::uint8_t specular_blend_b = 0;
    std::uint8_t unused_67 = 0;
    std::uint16_t specular_lut_length_r = 0;
    std::uint16_t specular_lut_length_g = 0;
    std::uint16_t specular_lut_length_b = 0;
    std::uint16_t specular_lut_index_r = 0;
    std::uint16_t specular_lut_index_g = 0;
    std::uint16_t specular_lut_index_b = 0;
    std::uint32_t unused_74 = 0;
    std::uint32_t unused_78 = 0;
    std::uint32_t unused_7c = 0;
    std::uint32_t unused_80 = 0;
    std::uint8_t alpha_blend = 0;
    std::uint8_t unused_85 = 0;
    std::uint16_t alpha_lut_length = 0;
    std::uint16_t alpha_lut_index = 0;
    std::uint16_t material_id = 0;
};

struct TextureAnimation {
    std::array<std::uint8_t, 32> name{};
    std::uint16_t count = 0;
    std::uint16_t start_index = 0;
    std::uint16_t minimum_palette_id = 0;
    std::uint16_t material_id = 0;
    std::uint16_t minimum_texture_id = 0;
    std::uint16_t field_2a = 0;
};

struct TexcoordAnimation {
    std::array<std::uint8_t, 32> name{};
    std::uint8_t scale_blend_s = 0;
    std::uint8_t scale_blend_t = 0;
    std::uint16_t scale_lut_length_s = 0;
    std::uint16_t scale_lut_length_t = 0;
    std::uint16_t scale_lut_index_s = 0;
    std::uint16_t scale_lut_index_t = 0;
    std::uint8_t rotate_blend_z = 0;
    std::uint8_t unused_2b = 0;
    std::uint16_t rotate_lut_length_z = 0;
    std::uint16_t rotate_lut_index_z = 0;
    std::uint8_t translate_blend_s = 0;
    std::uint8_t translate_blend_t = 0;
    std::uint16_t translate_lut_length_s = 0;
    std::uint16_t translate_lut_length_t = 0;
    std::uint16_t translate_lut_index_s = 0;
    std::uint16_t translate_lut_index_t = 0;
    std::uint16_t padding_3a = 0;
};

struct NodeAnimation {
    std::uint8_t scale_blend_x = 0;
    std::uint8_t scale_blend_y = 0;
    std::uint8_t scale_blend_z = 0;
    std::uint8_t flags = 0;
    std::uint16_t scale_lut_length_x = 0;
    std::uint16_t scale_lut_length_y = 0;
    std::uint16_t scale_lut_length_z = 0;
    std::uint16_t scale_lut_index_x = 0;
    std::uint16_t scale_lut_index_y = 0;
    std::uint16_t scale_lut_index_z = 0;
    std::uint8_t rotate_blend_x = 0;
    std::uint8_t rotate_blend_y = 0;
    std::uint8_t rotate_blend_z = 0;
    std::uint8_t padding_13 = 0;
    std::uint16_t rotate_lut_length_x = 0;
    std::uint16_t rotate_lut_length_y = 0;
    std::uint16_t rotate_lut_length_z = 0;
    std::uint16_t rotate_lut_index_x = 0;
    std::uint16_t rotate_lut_index_y = 0;
    std::uint16_t rotate_lut_index_z = 0;
    std::uint8_t translate_blend_x = 0;
    std::uint8_t translate_blend_y = 0;
    std::uint8_t translate_blend_z = 0;
    std::uint8_t padding_23 = 0;
    std::uint16_t translate_lut_length_x = 0;
    std::uint16_t translate_lut_length_y = 0;
    std::uint16_t translate_lut_length_z = 0;
    std::uint16_t translate_lut_index_x = 0;
    std::uint16_t translate_lut_index_y = 0;
    std::uint16_t translate_lut_index_z = 0;
};

struct Texture {
    formats::TextureFormat format = formats::TextureFormat::Palette2Bit;
    std::uint8_t padding_1 = 0;
    std::uint16_t width = 0;
    std::uint16_t height = 0;
    std::uint16_t padding_6 = 0;
    std::uint32_t image_offset = 0;
    std::uint32_t image_size = 0;
    std::uint32_t unused_offset = 0;
    std::uint32_t unused_count = 0;
    std::uint32_t vram_offset = 0;
    std::uint32_t opaque = 1;
    std::uint32_t skip_vram = 0;
    std::uint8_t packed_size = 0;
    std::uint8_t native_texture_format = 0;
    std::uint16_t object_ref = 0;
};

struct Palette {
    std::uint32_t offset = 0;
    std::uint32_t size = 0;
    std::uint32_t vram_offset = 0;
    std::uint32_t object_ref = 0;
};

struct Header {
    std::uint32_t scale_factor = 0;
    formats::Fixed scale_base;
    std::uint32_t primitive_count = 0;
    std::uint32_t vertex_count = 0;
    std::uint32_t material_offset = 0;
    std::uint32_t dlist_offset = 0;
    std::uint32_t node_offset = 0;
    std::uint16_t node_weight_count = 0;
    std::uint8_t flags = 0;
    std::uint8_t padding_1f = 0;
    std::uint32_t node_weight_offset = 0;
    std::uint32_t mesh_offset = 0;
    std::uint16_t texture_count = 0;
    std::uint16_t padding_2a = 0;
    std::uint32_t texture_offset = 0;
    std::uint16_t palette_count = 0;
    std::uint16_t padding_32 = 0;
    std::uint32_t palette_offset = 0;
    std::uint32_t node_pos_counts = 0;
    std::uint32_t node_pos_scales = 0;
    std::uint32_t node_initial_position = 0;
    std::uint32_t node_position = 0;
    std::uint16_t material_count = 0;
    std::uint16_t node_count = 0;
    std::uint32_t texture_matrix_offset = 0;
    std::uint32_t node_animation_offset = 0;
    std::uint32_t texture_coordinate_animations = 0;
    std::uint32_t material_animations = 0;
    std::uint32_t texture_animations = 0;
    std::uint16_t mesh_count = 0;
    std::uint16_t texture_matrix_count = 0;
};

struct RawNode {
    std::array<std::uint8_t, 64> name{};
    std::int16_t parent_id = -1;
    std::int16_t child_id = -1;
    std::int16_t next_id = -1;
    std::uint16_t padding_46 = 0;
    std::uint32_t enabled = 0;
    std::uint16_t mesh_count = 0;
    std::uint16_t mesh_id = 0;
    formats::Vector3Fx scale;
    std::int16_t angle_x = 0;
    std::int16_t angle_y = 0;
    std::int16_t angle_z = 0;
    std::uint16_t padding_62 = 0;
    formats::Vector3Fx position;
    formats::Fixed bounding_radius;
    formats::Vector3Fx min_bounds;
    formats::Vector3Fx max_bounds;
    formats::BillboardMode billboard_mode = formats::BillboardMode::None;
    std::uint8_t padding_8d = 0;
    std::uint16_t padding_8e = 0;
    formats::Matrix43Fx transform;
    std::uint32_t before_transform = 0;
    std::uint32_t after_transform = 0;
    std::uint32_t unused_c8 = 0;
    std::uint32_t unused_cc = 0;
    std::uint32_t unused_d0 = 0;
    std::uint32_t unused_d4 = 0;
    std::uint32_t unused_d8 = 0;
    std::uint32_t unused_dc = 0;
    std::uint32_t unused_e0 = 0;
    std::uint32_t unused_e4 = 0;
    std::uint32_t unused_e8 = 0;
    std::uint32_t unused_ec = 0;
};

struct RawCollisionVolume {
    formats::VolumeType type = formats::VolumeType::Box;
    union {
        struct {
            formats::Vector3Fx vector1;
            formats::Vector3Fx vector2;
            formats::Vector3Fx vector3;
            formats::Vector3Fx position;
            formats::Fixed dot1;
            formats::Fixed dot2;
            formats::Fixed dot3;
        } box;
        struct {
            formats::Vector3Fx vector;
            formats::Vector3Fx position;
            formats::Fixed radius;
            formats::Fixed dot;
            std::array<std::uint8_t, 28> unused{};
        } cylinder;
        struct {
            formats::Vector3Fx position;
            formats::Fixed radius;
            std::array<std::uint8_t, 40> unused{};
        } sphere;
    } data;
};

struct FhRawCollisionVolume {
    formats::FhVolumeType type = formats::FhVolumeType::Sphere;
    union {
        struct {
            formats::Vector3Fx position;
            formats::Vector3Fx vector1;
            formats::Vector3Fx vector2;
            formats::Vector3Fx vector3;
            formats::Fixed dot1;
            formats::Fixed dot2;
            formats::Fixed dot3;
        } box;
        struct {
            formats::Vector3Fx position;
            formats::Vector3Fx vector;
            formats::Fixed dot;
            formats::Fixed radius;
            std::array<std::uint8_t, 28> unused{};
        } cylinder;
        struct {
            formats::Vector3Fx position;
            formats::Fixed radius;
            std::array<std::uint8_t, 40> unused{};
        } sphere;
    } data;
};

struct CameraSequenceHeader {
    std::uint16_t count = 0;
    std::uint8_t version = 0;
    std::uint8_t padding_3 = 0;
    std::uint32_t padding_4 = 0;
};

struct RawCameraSequenceKeyframe {
    formats::Vector3Fx position;
    formats::Vector3Fx to_target;
    formats::Fixed roll;
    formats::Fixed fov;
    formats::Fixed move_time;
    formats::Fixed hold_time;
    formats::Fixed fade_in_time;
    formats::Fixed fade_out_time;
    formats::FadeType fade_in_type = formats::FadeType::None;
    formats::FadeType fade_out_type = formats::FadeType::None;
    std::uint8_t prev_frame_influence = 0;
    std::uint8_t after_frame_influence = 0;
    std::uint8_t use_entity_transform = 0;
    std::uint8_t padding_35 = 0;
    std::uint16_t padding_36 = 0;
    std::int16_t pos_entity_type = 0;
    std::int16_t pos_entity_id = 0;
    std::int16_t target_entity_type = 0;
    std::int16_t target_entity_id = 0;
    std::int16_t message_target_type = 0;
    std::int16_t message_target_id = 0;
    std::uint16_t message_id = 0;
    std::uint16_t message_param = 0;
    formats::Fixed easing;
    std::uint32_t unused_4c = 0;
    std::uint32_t unused_50 = 0;
    std::array<std::uint8_t, 16> node_name{};
};

struct RawEffect {
    std::uint32_t field0 = 0;
    std::uint32_t func_count = 0;
    std::uint32_t func_offset = 0;
    std::uint32_t count2 = 0;
    std::uint32_t offset2 = 0;
    std::uint32_t element_count = 0;
    std::uint32_t element_offset = 0;
};

struct RawEffectElement {
    std::array<std::uint8_t, 32> name{};
    std::array<std::uint8_t, 32> model_name{};
    std::uint32_t particle_count = 0;
    std::uint32_t particle_offset = 0;
    formats::EffElemFlags flags = formats::EffElemFlags::None;
    formats::Vector3Fx acceleration;
    std::uint32_t child_effect_id = 0;
    formats::Fixed lifespan;
    formats::Fixed drain_time;
    formats::Fixed buffer_time;
    std::int32_t draw_type = 0;
    std::uint32_t func_count = 0;
    std::uint32_t func_offset = 0;
};

struct RawStringTableEntry {
    std::array<std::uint8_t, 4> id{};
    std::uint32_t offset = 0;
    std::uint16_t length = 0;
    std::uint8_t speed = 0;
    std::uint8_t category = 0;
};

struct TextFileEntry {
    std::uint32_t offset1 = 0;
    std::uint32_t offset2 = 0;
    std::uint16_t length1 = 0;
    std::uint16_t length2 = 0;
};

struct Sizes {
    static constexpr std::size_t Header =
        sizeof(::fruityprime::raw::Header);
    static constexpr std::size_t Texture =
        sizeof(::fruityprime::raw::Texture);
    static constexpr std::size_t Palette =
        sizeof(::fruityprime::raw::Palette);
    static constexpr std::size_t Material =
        sizeof(::fruityprime::raw::RawMaterial);
    static constexpr std::size_t Node =
        sizeof(::fruityprime::raw::RawNode);
    static constexpr std::size_t Mesh =
        sizeof(::fruityprime::raw::RawMesh);
    static constexpr std::size_t Dlist =
        sizeof(::fruityprime::raw::DisplayList);
    static constexpr std::size_t AnimationHeader =
        sizeof(::fruityprime::raw::AnimationHeader);
    static constexpr std::size_t NodeAnimation =
        sizeof(::fruityprime::raw::NodeAnimation);
    static constexpr std::size_t CameraSequenceHeader =
        sizeof(::fruityprime::raw::CameraSequenceHeader);
    static constexpr std::size_t CameraSequenceKeyframe =
        sizeof(::fruityprime::raw::RawCameraSequenceKeyframe);
};

static_assert(sizeof(RawMesh) == 4);
static_assert(sizeof(DisplayList) == 32);
static_assert(sizeof(RawMaterial) == 132);
static_assert(sizeof(AnimationHeader) == 24);
static_assert(sizeof(RawMaterialAnimationGroup) == 20);
static_assert(sizeof(RawTextureAnimationGroup) == 32);
static_assert(sizeof(RawTexcoordAnimationGroup) == 28);
static_assert(sizeof(RawNodeAnimationGroup) == 20);
static_assert(sizeof(MaterialAnimation) == 140);
static_assert(sizeof(TextureAnimation) == 44);
static_assert(sizeof(TexcoordAnimation) == 60);
static_assert(sizeof(NodeAnimation) == 48);
static_assert(sizeof(Texture) == 40);
static_assert(sizeof(Palette) == 16);
static_assert(sizeof(Header) == 100);
static_assert(sizeof(RawNode) == 240);
static_assert(sizeof(RawCollisionVolume) == 64);
static_assert(sizeof(FhRawCollisionVolume) == 64);
static_assert(sizeof(CameraSequenceHeader) == 8);
static_assert(sizeof(RawCameraSequenceKeyframe) == 100);
static_assert(sizeof(RawEffect) == 28);
static_assert(sizeof(RawEffectElement) == 116);
static_assert(sizeof(TextFileEntry) == 12);

} // namespace fruityprime::raw

namespace MphReadNative {
namespace RawFormats = ::fruityprime::raw;
using Sizes = ::fruityprime::raw::Sizes;
}
