#pragma once

#include "Formats/fixed.hpp"
#include "Formats/raw_formats.hpp"

#include <cstddef>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <map>
#include <string>
#include <span>
#include <bit>
#include <string_view>
#include <vector>

namespace fruityprime::model {

[[nodiscard]] bool node_enabled_for_layer(std::string_view name, int layer_mask);

struct Header {
    static constexpr std::size_t Size = 100;

    std::uint32_t scale_factor = 0;
    formats::Fixed scale_base;
    std::uint32_t primitive_count = 0;
    std::uint32_t vertex_count = 0;
    std::uint32_t material_offset = 0;
    std::uint32_t display_list_offset = 0;
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
    std::uint32_t node_position_counts = 0;
    std::uint32_t node_position_scales = 0;
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

struct Mesh {
    static constexpr std::size_t Size = 4;
    std::uint16_t material_id = 0;
    std::uint16_t display_list_id = 0;
};

struct ColorRgb {
    std::uint8_t red = 0;
    std::uint8_t green = 0;
    std::uint8_t blue = 0;
};

struct Material {
    static constexpr std::size_t Size = 132;

    std::string name;
    // Material.InitLighting is the value the file carries; `lighting` is the
    // live one an entity may override for a frame.
    std::uint8_t init_lighting = 0;
    std::uint8_t lighting = 0;
    std::uint8_t culling = 0;
    std::uint8_t alpha = 0;
    std::uint8_t wireframe = 0;
    std::int16_t palette_id = -1;
    std::int16_t texture_id = -1;
    std::uint8_t x_repeat = 0;
    std::uint8_t y_repeat = 0;
    ColorRgb diffuse;
    ColorRgb ambient;
    ColorRgb specular;
    std::uint8_t padding_53 = 0;
    std::uint32_t polygon_mode = 0;
    std::uint8_t render_mode = 0;
    std::uint8_t animation_flags = 0;
    std::uint16_t padding_5a = 0;
    std::uint32_t texcoord_transform_mode = 0;
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

struct DisplayList {
    static constexpr std::size_t Size = 32;
    std::uint32_t offset = 0;
    std::uint32_t size = 0;
    formats::Vector3Fx min_bounds;
    formats::Vector3Fx max_bounds;
};

enum class InstructionCode : std::uint32_t {
    Nop = 0x400,
    MtxRestore = 0x450,
    Color = 0x480,
    Normal = 0x484,
    Texcoord = 0x488,
    Vtx16 = 0x48c,
    Vtx10 = 0x490,
    VtxXy = 0x494,
    VtxXz = 0x498,
    VtxYz = 0x49c,
    VtxDiff = 0x4a0,
    DifAmb = 0x4c0,
    BeginVtxs = 0x500,
    EndVtxs = 0x504
};

struct RenderInstruction {
    InstructionCode code = InstructionCode::Nop;
    std::vector<std::uint32_t> arguments;
};

enum class GeometryPrimitiveType {
    Triangles,
    Quads,
    TriangleStrip,
    QuadStrip
};

struct GeometryVertex {
    float x = 0;
    float y = 0;
    float z = 0;
    float normal_x = 0;
    float normal_y = 0;
    float normal_z = 0;
    float texture_s = 0;
    float texture_t = 0;
    std::uint32_t color = 0;
    std::uint32_t ambient_color = 0;
    std::uint32_t matrix_id = 0;
    // The managed display-list path keeps the alpha marker written by
    // DIF_AMB.  A native decoder that only retains the RGB value cannot tell
    // that opcode from COLOR, which changes the material diffuse term when
    // lighting is enabled.  Keep the two pieces of state explicit so the
    // renderer can apply the same rule per vertex.
    bool has_color = false;
    bool diffuse_ambient = false;
};

struct GeometryPrimitive {
    GeometryPrimitiveType type = GeometryPrimitiveType::Triangles;
    std::vector<GeometryVertex> vertices;
};

struct Texture {
    static constexpr std::size_t Size = 40;
    std::uint8_t format = 0;
    std::uint8_t padding_1 = 0;
    std::uint16_t width = 0;
    std::uint16_t height = 0;
    std::uint16_t padding_6 = 0;
    std::uint32_t image_offset = 0;
    std::uint32_t image_size = 0;
    std::uint32_t unused_offset = 0;
    std::uint32_t unused_count = 0;
    std::uint32_t vram_offset = 0;
    std::uint32_t opaque = 0;
    std::uint32_t skip_vram = 0;
    std::uint8_t packed_size = 0;
    std::uint8_t native_texture_format = 0;
    std::uint16_t object_ref = 0;
};

struct Palette {
    static constexpr std::size_t Size = 16;
    std::uint32_t offset = 0;
    std::uint32_t size = 0;
    std::uint32_t vram_offset = 0;
    std::uint32_t object_ref = 0;
};

struct TexturePixel {
    std::uint32_t data = 0;
    std::uint8_t alpha = 255;
};

struct Node {
    static constexpr std::size_t Size = 240;
    std::string name;
    std::int16_t parent_id = -1;
    std::int16_t child_id = -1;
    std::int16_t next_id = -1;
    std::uint32_t enabled = 0;
    std::uint16_t mesh_count = 0;
    std::uint16_t mesh_id = 0;
    formats::Vector3Fx scale;
    std::int16_t angle_x = 0;
    std::int16_t angle_y = 0;
    std::int16_t angle_z = 0;
    formats::Vector3Fx position;
    formats::Fixed bounding_radius;
    formats::Vector3Fx min_bounds;
    formats::Vector3Fx max_bounds;
    std::uint32_t billboard_mode = 0;
    // Runtime fields the managed Node carries alongside the decoded record:
    // which room part owns this node, whether that part is active, and the
    // two animation link overrides an entity can set.  Room drawing walks
    // parts, not the raw node list.
    std::int32_t room_part_id = -1;
    bool room_part_active = true;
    bool anim_ignore_parent = false;
    bool anim_ignore_child = false;

    // The managed Node exposes the raw link ids under these names.
    [[nodiscard]] std::int16_t child_index() const noexcept { return child_id; }
    [[nodiscard]] std::int16_t next_index() const noexcept { return next_id; }
    [[nodiscard]] std::int16_t parent_index() const noexcept {
        return parent_id;
    }

    // Node.GetMeshIds(): the meshes this node owns.  MeshId is a byte offset
    // into the mesh table, so it is halved to reach an index.
    [[nodiscard]] std::vector<std::uint16_t> mesh_ids() const {
        std::vector<std::uint16_t> result;
        result.reserve(mesh_count);
        const std::uint16_t start = static_cast<std::uint16_t>(mesh_id / 2);
        for (std::uint16_t i = 0; i < mesh_count; ++i) {
            result.push_back(static_cast<std::uint16_t>(start + i));
        }
        return result;
    }

    // Node.GetAllMeshIds(): this node's meshes, then its siblings' (unless
    // this is the root the walk started from), then its children's.  The
    // sibling/child order is the managed one and decides draw order.
    void append_all_mesh_ids(const std::vector<Node>& nodes, bool root,
                             std::vector<std::uint16_t>& out) const {
        const std::uint16_t start = static_cast<std::uint16_t>(mesh_id / 2);
        for (std::uint16_t i = 0; i < mesh_count; ++i) {
            out.push_back(static_cast<std::uint16_t>(start + i));
        }
        if (!root && next_id != -1
            && static_cast<std::size_t>(next_id) < nodes.size()) {
            nodes[static_cast<std::size_t>(next_id)]
                .append_all_mesh_ids(nodes, false, out);
        }
        if (child_id != -1
            && static_cast<std::size_t>(child_id) < nodes.size()) {
            nodes[static_cast<std::size_t>(child_id)]
                .append_all_mesh_ids(nodes, false, out);
        }
    }

    [[nodiscard]] std::vector<std::uint16_t> all_mesh_ids(
        const std::vector<Node>& nodes, bool root) const {
        std::vector<std::uint16_t> out;
        append_all_mesh_ids(nodes, root, out);
        return out;
    }
};

// Decoded counterparts of the animation groups loaded by Read.LoadAnimation
// and Formats.Model.  The raw records remain available in the maps because
// the renderer and entity code need the original LUT indices and blend flags.
struct NodeAnimationGroup {
    std::uint32_t frame_count = 0;
    std::uint16_t current_frame = 0;
    std::vector<float> scales;
    std::vector<float> rotations;
    std::vector<float> translations;
    std::map<std::string, raw::NodeAnimation> animations;
};

struct MaterialAnimationGroup {
    std::uint32_t frame_count = 0;
    std::uint16_t current_frame = 0;
    std::uint16_t unused_frame = 0;
    std::vector<float> colors;
    std::map<std::string, raw::MaterialAnimation> animations;
};

struct TexcoordAnimationGroup {
    std::uint32_t frame_count = 0;
    std::uint16_t current_frame = 0;
    std::uint16_t unused_frame = 0;
    std::vector<float> scales;
    std::vector<float> rotations;
    std::vector<float> translations;
    std::map<std::string, raw::TexcoordAnimation> animations;
};

struct TextureAnimationGroup {
    std::uint16_t frame_count = 0;
    std::uint16_t current_frame = 0;
    std::uint16_t unused_frame = 0;
    std::uint16_t unused_a = 0;
    std::vector<std::uint16_t> frame_indices;
    std::vector<std::uint16_t> texture_ids;
    std::vector<std::uint16_t> palette_ids;
    std::map<std::string, raw::TextureAnimation> animations;
};

struct AnimationResults {
    std::vector<std::uint32_t> node_group_offsets;
    std::vector<std::uint32_t> material_group_offsets;
    std::vector<std::uint32_t> texcoord_group_offsets;
    std::vector<std::uint32_t> texture_group_offsets;
    std::vector<NodeAnimationGroup> node_groups;
    std::vector<MaterialAnimationGroup> material_groups;
    std::vector<TexcoordAnimationGroup> texcoord_groups;
    std::vector<TextureAnimationGroup> texture_groups;

    [[nodiscard]] bool any() const noexcept {
        return !node_groups.empty() || !material_groups.empty()
            || !texcoord_groups.empty() || !texture_groups.empty();
    }
};

// These are the renderer-independent calculations used by ModelInstance in
// the managed implementation.  Keeping them here makes animation playback
// testable without choosing OpenGL, Direct3D, or an Android graphics API.
[[nodiscard]] float interpolate_animation(
    std::span<const float> values, int start, int frame,
    int blend, int lut_length, int frame_count,
    bool rotation = false);

[[nodiscard]] formats::Matrix4 animate_node(
    const NodeAnimationGroup& group, const raw::NodeAnimation& animation,
    int current_frame, formats::Vector3 model_scale);

[[nodiscard]] formats::Matrix4 animate_texcoords(
    const TexcoordAnimationGroup& group,
    const raw::TexcoordAnimation& animation, int current_frame);

struct TextureAnimationSelection {
    bool found = false;
    std::uint16_t texture_id = 0;
    std::uint16_t palette_id = 0;
};

[[nodiscard]] TextureAnimationSelection select_texture_animation(
    const TextureAnimationGroup& group,
    const raw::TextureAnimation& animation, std::uint16_t current_frame);

// Model.NodeLayer.  A room model carries the geometry for every game mode and
// detail level at once; the mask picks the set that is actually drawn.
enum class NodeLayer : std::uint16_t {
    None = 0x0,
    MultiplayerLod0 = 0x8,
    MultiplayerLod1 = 0x10,
    MultiplayerU = 0x20,
    Unknown40 = 0x40,
    Unknown1000 = 0x1000,
    CaptureTheFlag = 0x4000
};

// SceneSetup.GetNodeLayer.  `room_layer` only matters in the story; a
// multiplayer match picks its detail level from the player count instead.
[[nodiscard]] int node_layer_mask(bool single_player, int room_layer,
                                  int player_count, bool capture) noexcept;

class File {
public:
    [[nodiscard]] static File read_file(const std::filesystem::path& path);
    [[nodiscard]] static File from_bytes(std::vector<std::uint8_t> bytes);
    [[nodiscard]] static File from_resources(
        std::vector<std::uint8_t> model_bytes,
        std::vector<std::uint8_t> texture_bytes,
        std::vector<std::uint8_t> palette_bytes = {});
    // Some C# recolors keep geometry/materials in one model and replace the
    // texture/palette table with a small companion model. The image and color
    // data can then live in a third resource. Preserve those three sources so
    // native rendering follows the same ownership model.
    [[nodiscard]] static File from_recolor_resources(
        std::vector<std::uint8_t> model_bytes,
        std::vector<std::uint8_t> table_bytes,
        std::vector<std::uint8_t> texture_bytes,
        std::vector<std::uint8_t> palette_bytes = {});

    // Animation files are separate resources in the managed metadata.  Keep
    // loading explicit so a model can be decoded from a ROM archive without
    // forcing the asset store to know about model ownership.
    void load_animations(std::vector<std::uint8_t> bytes,
                         std::string model_name = {});
    // A few managed models combine a model-local animation file with a
    // shared animation file.  Keep the decoded group indexes in that same
    // order so ModelInstance can use one animation index for both resources.
    void append_animations(std::vector<std::uint8_t> bytes,
                           std::string model_name = {});

    [[nodiscard]] const Header& header() const noexcept { return header_; }
    // C# Model.Scale is applied by EntityBase before a model is rendered.
    // Keep it available to native frontends instead of baking it into the
    // decoded display-list vertices, which remain in model space.
    [[nodiscard]] float world_scale() const noexcept {
        // C# int shifts mask the shift count to five bits and are signed.
        const auto multiplier = std::bit_cast<std::int32_t>(
            std::uint32_t{1} << (header_.scale_factor & 31));
        return scale_base_value() * static_cast<float>(multiplier);
    }
    [[nodiscard]] const std::vector<Mesh>& meshes() const noexcept { return meshes_; }
    [[nodiscard]] const std::vector<Material>& materials() const noexcept {
        return materials_;
    }
    [[nodiscard]] const std::vector<DisplayList>& display_lists() const noexcept {
        return display_lists_;
    }
    [[nodiscard]] const std::vector<std::vector<RenderInstruction>>& instructions()
        const noexcept {
        return instructions_;
    }
    // `is_room` mirrors the managed Renderer.DoDlist rule: a room's display
    // lists keep the matrix ID at 0 so its node transforms can be toggled
    // independently.  The game never applies them, because the cartridge does
    // not do the scale division that would make them line up.
    [[nodiscard]] std::vector<GeometryPrimitive> decode_geometry(
        std::size_t display_list_id, int texture_width = 0,
        int texture_height = 0, bool texgen = false,
        bool is_room = false) const;
    [[nodiscard]] const std::vector<Node>& nodes() const noexcept { return nodes_; }
    // Model.FilterNodes: disable every node whose name asks for a layer the
    // mask does not contain.  Without this the room draws its multiplayer
    // LOD0 and LOD1 geometry, and its story-only variants, all at once.
    void filter_nodes(int layer_mask);
    [[nodiscard]] int get_node_index_by_name(std::string_view name) const noexcept;
    [[nodiscard]] const Node* get_node_by_name(std::string_view name) const noexcept;
    [[nodiscard]] const Material* get_material_by_name(std::string_view name) const noexcept;
    // These tables are the model-side equivalent of Model.NodeMatrixIds,
    // NodePos, NodeInitialPos, NodePosCounts, and NodePosScales.  They are
    // kept separate from the decoded Node records because the cartridge uses
    // them to build the runtime matrix stack and vertex position streams.
    [[nodiscard]] const std::vector<std::int32_t>& node_weights()
        const noexcept {
        return node_weights_;
    }
    [[nodiscard]] const std::vector<formats::Vector3Fx>& node_positions()
        const noexcept {
        return node_positions_;
    }
    [[nodiscard]] const std::vector<formats::Vector3Fx>&
    node_init_pos() const noexcept {
        return node_init_pos_;
    }
    [[nodiscard]] const std::vector<std::int32_t>& node_position_counts()
        const noexcept {
        return node_position_counts_;
    }
    [[nodiscard]] const std::vector<formats::Fixed>& node_position_scales()
        const noexcept {
        return node_position_scales_;
    }
    // Model.TextureMatrices.  In RAM these are 4x4 but only the upper-left
    // 3x2 is ever read, and the rest of the record is garbage, so the managed
    // reader does not decode the table at all -- it supplies the one model
    // that uses the feature.  Reproducing that exactly means the name has to
    // reach the file; set_name applies the same rule.
    [[nodiscard]] const std::vector<formats::Matrix4>& texture_matrices()
        const noexcept {
        return texture_matrices_;
    }
    [[nodiscard]] const std::string& name() const noexcept { return name_; }
    void set_name(std::string value);

    [[nodiscard]] const std::vector<Texture>& textures() const noexcept {
        return textures_;
    }
    [[nodiscard]] const std::vector<Palette>& palettes() const noexcept {
        return palettes_;
    }
    [[nodiscard]] const std::vector<std::uint8_t>& animation_bytes()
        const noexcept {
        return animation_bytes_;
    }
    [[nodiscard]] const AnimationResults& animations() const noexcept {
        return animations_;
    }
    [[nodiscard]] std::vector<TexturePixel> decode_texture(
        std::size_t texture_id) const;
    [[nodiscard]] std::vector<std::uint16_t> decode_palette(
        std::size_t palette_id) const;
    // A File holds one recolor resource (including separate companion tables).
    [[nodiscard]] std::vector<formats::ColorRgba> get_pixels(int texture_id, int palette_id) const;
    [[nodiscard]] std::vector<formats::ColorRgba> get_palette_pixels(int palette_id) const;
    // Repackers need the encoded cartridge bytes, not only the decoded pixels.
    // Keep this accessor explicit so a caller cannot accidentally re-encode a
    // palette texture with a different index or alpha quantisation.
    [[nodiscard]] std::vector<std::uint8_t> read_texture_data(
        std::size_t texture_id) const;
    [[nodiscard]] std::vector<std::uint8_t> read_palette_data(
        std::size_t palette_id) const;

private:
    explicit File(std::vector<std::uint8_t> bytes);
    File(std::vector<std::uint8_t> model_bytes,
         std::vector<std::uint8_t> texture_bytes,
         std::vector<std::uint8_t> palette_bytes,
         std::vector<std::uint8_t> table_bytes = {});

    void parse();

    [[nodiscard]] float scale_base_value() const noexcept {
        return header_.scale_base.to_float();
    }

    std::vector<std::uint8_t> bytes_;
    std::vector<std::uint8_t> table_bytes_;
    std::vector<std::uint8_t> texture_bytes_;
    std::vector<std::uint8_t> palette_bytes_;
    std::vector<std::uint8_t> animation_bytes_;
    Header header_;
    std::vector<Mesh> meshes_;
    std::vector<Material> materials_;
    std::vector<DisplayList> display_lists_;
    std::vector<std::vector<RenderInstruction>> instructions_;
    std::vector<Node> nodes_;
    std::vector<std::int32_t> node_weights_;
    std::vector<formats::Vector3Fx> node_positions_;
    std::vector<formats::Vector3Fx> node_init_pos_;
    std::vector<std::int32_t> node_position_counts_;
    std::vector<formats::Fixed> node_position_scales_;
    std::vector<Texture> textures_;
    std::vector<Palette> palettes_;
    std::vector<formats::Matrix4> texture_matrices_;
    std::string name_;
    AnimationResults animations_;
};

} // namespace fruityprime::model
