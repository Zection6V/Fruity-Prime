#pragma once

#include "Formats/model_format.hpp"

#include <array>
#include <cstdint>
#include <optional>
#include <vector>

namespace fruityprime::model {

// Runtime animation flags mirror Formats/Model.cs.  They are kept separate
// from the on-cartridge animation record flags: these flags describe the
// playback state of one ModelInstance, not the authored resource.
enum class AnimationFlags : std::uint16_t {
    None = 0x0000,
    PingPong = 0x0001,
    Reverse = 0x0002,
    Paused = 0x0004,
    NoLoop = 0x0008,
    Ended = 0x0010
};

[[nodiscard]] constexpr AnimationFlags operator|(AnimationFlags left,
                                                  AnimationFlags right) noexcept {
    return static_cast<AnimationFlags>(
        static_cast<std::uint16_t>(left)
        | static_cast<std::uint16_t>(right));
}

[[nodiscard]] constexpr AnimationFlags operator&(AnimationFlags left,
                                                  AnimationFlags right) noexcept {
    return static_cast<AnimationFlags>(
        static_cast<std::uint16_t>(left)
        & static_cast<std::uint16_t>(right));
}

[[nodiscard]] constexpr AnimationFlags operator^(AnimationFlags left,
                                                  AnimationFlags right) noexcept {
    return static_cast<AnimationFlags>(
        static_cast<std::uint16_t>(left)
        ^ static_cast<std::uint16_t>(right));
}

constexpr AnimationFlags& operator|=(AnimationFlags& left,
                                     AnimationFlags right) noexcept {
    left = left | right;
    return left;
}

constexpr AnimationFlags& operator&=(AnimationFlags& left,
                                     AnimationFlags right) noexcept {
    left = left & right;
    return left;
}

constexpr AnimationFlags& operator^=(AnimationFlags& left,
                                     AnimationFlags right) noexcept {
    left = left ^ right;
    return left;
}

[[nodiscard]] constexpr bool has_flag(AnimationFlags value,
                                       AnimationFlags flag) noexcept {
    return (value & flag) != AnimationFlags::None;
}

// These values mirror the managed SetFlags enum.  Unused is retained so
// callers can pass the same masks as the C# side even though it has no native
// channel yet.
enum class AnimationSetFlags : std::uint16_t {
    None = 0x0000,
    Node = 0x0002,
    Unused = 0x0004,
    Material = 0x0008,
    Texcoord = 0x0010,
    Texture = 0x0020,
    All = 0x003e
};

[[nodiscard]] constexpr AnimationSetFlags operator|(
    AnimationSetFlags left, AnimationSetFlags right) noexcept {
    return static_cast<AnimationSetFlags>(
        static_cast<std::uint16_t>(left)
        | static_cast<std::uint16_t>(right));
}

[[nodiscard]] constexpr AnimationSetFlags operator&(
    AnimationSetFlags left, AnimationSetFlags right) noexcept {
    return static_cast<AnimationSetFlags>(
        static_cast<std::uint16_t>(left)
        & static_cast<std::uint16_t>(right));
}

[[nodiscard]] constexpr bool has_flag(AnimationSetFlags value,
                                       AnimationSetFlags flag) noexcept {
    return (value & flag) != AnimationSetFlags::None;
}

enum class MaterialAnimationFlags : std::uint8_t {
    None = 0x00,
    DisableColor = 0x01,
    DisableAlpha = 0x02
};

[[nodiscard]] constexpr bool has_flag(MaterialAnimationFlags value,
                                       MaterialAnimationFlags flag) noexcept {
    return (static_cast<std::uint8_t>(value)
            & static_cast<std::uint8_t>(flag)) != 0;
}

struct AnimationChannel {
    int slot = 0;
    int group = -1;
    // SetModel keeps the selected managed group alive. Files referenced here
    // must outlive the instance, just like the File passed to its constructor.
    const AnimationResults* source = nullptr;
};

struct AnimationState {
    std::array<int, 2> index{-1, -1};
    std::array<int, 2> prev_index{-1, -1};
    std::array<int, 2> frame{0, 0};
    std::array<int, 2> frame_count{0, 0};
    std::array<AnimationFlags, 2> flags{
        AnimationFlags::None, AnimationFlags::None};
    std::array<int, 2> step{1, 1};
    AnimationChannel node;
    AnimationChannel material;
    AnimationChannel texcoord;
    AnimationChannel texture;

    [[nodiscard]] int node_frame() const noexcept { return frame[node.slot]; }
    [[nodiscard]] int material_frame() const noexcept {
        return frame[material.slot];
    }
    [[nodiscard]] int texcoord_frame() const noexcept {
        return frame[texcoord.slot];
    }
    [[nodiscard]] int texture_frame() const noexcept {
        return frame[texture.slot];
    }
    [[nodiscard]] int node_index() const noexcept { return index[node.slot]; }
    [[nodiscard]] int material_index() const noexcept {
        return index[material.slot];
    }
    [[nodiscard]] int texcoord_index() const noexcept {
        return index[texcoord.slot];
    }
    [[nodiscard]] int texture_index() const noexcept {
        return index[texture.slot];
    }
};

struct NodeRuntimeState {
    bool enabled = true;
    bool anim_ignore_parent = false;
    bool anim_ignore_child = false;
    formats::Vector3 scale{1, 1, 1};
    formats::Vector3 angle{};
    formats::Vector3 position{};
    formats::Matrix4 transform{};
    formats::Matrix4 animation{};
    std::optional<formats::Matrix4> before_transform;
    std::optional<formats::Matrix4> after_transform;
};

struct MaterialRuntimeState {
    formats::Vector3 current_diffuse{};
    formats::Vector3 current_ambient{};
    formats::Vector3 current_specular{};
    float current_alpha = 1.0F;
    int current_texture_id = -1;
    int current_palette_id = -1;
    formats::Matrix4 texcoord_matrix{};
};

class ModelInstance {
public:
    explicit ModelInstance(const File& model);

    void set_model(const File& model);

    [[nodiscard]] const File& model() const noexcept { return *model_; }
    [[nodiscard]] const AnimationState& animation_info() const noexcept {
        return animation_;
    }
    [[nodiscard]] AnimationState& animation_info() noexcept { return animation_; }
    [[nodiscard]] const std::vector<NodeRuntimeState>& node_states()
        const noexcept {
        return nodes_;
    }
    [[nodiscard]] std::vector<NodeRuntimeState>& node_states() noexcept {
        return nodes_;
    }
    [[nodiscard]] const std::vector<MaterialRuntimeState>& material_states()
        const noexcept {
        return materials_;
    }
    [[nodiscard]] const std::vector<formats::Matrix4>& matrix_stack()
        const noexcept {
        return matrix_stack_;
    }
    [[nodiscard]] const std::vector<float>& matrix_stack_values()
        const noexcept {
        return matrix_stack_values_;
    }

    [[nodiscard]] bool node_animation_ignore_root() const noexcept {
        return node_animation_ignore_root_;
    }
    void set_node_animation_ignore_root(bool value) noexcept {
        node_animation_ignore_root_ = value;
    }

    // The overloads intentionally preserve the two frame-count precedence
    // rules from ModelInstance.SetAnimation in the managed implementation.
    void set_animation(int index,
                       AnimationFlags flags = AnimationFlags::None);
    void set_animation(int index, int slot, AnimationSetFlags set_flags,
                       AnimationFlags flags = AnimationFlags::None);
    void set_node_anim(int index);
    void set_material_anim(int index);
    bool active = true;
    bool is_placeholder = false;

    void set_animation_step(int slot, int step);
    // Synchronize a decoded gameplay animation cursor without resetting the
    // selected node/material/texture groups.  Enemy controllers own their
    // animation timeline, so the renderer must be able to consume the exact
    // frame selected by the simulation rather than advancing a second cursor.
    void set_animation_frame(int slot, int frame);
    void update_anim_frames();

    // Compute the model-space node hierarchy, then apply the selected node
    // animation and optional attachment transform.  Both operations are
    // renderer independent and are therefore usable by exporters and tests.
    void compute_node_matrices();
    void compute_node_matrices(int index);
    void animate_nodes(
        bool use_node_transform = true,
        formats::Matrix4 parent_transform = formats::Matrix4{});
    void animate_nodes(int index, bool use_node_transform,
                       formats::Matrix4 parent_transform, formats::Vector3 scale);
    void animate_nodes2(int index, bool use_node_transform,
                        formats::Matrix4 parent_transform, formats::Vector3 scale);
    void filter_nodes(int layer_mask);
    [[nodiscard]] bool node_parents_enabled(std::size_t node_index) const;
    void update_matrix_stack();

    void animate_materials();
    void animate_textures();
    void animate_texcoords();
    void update_materials();

private:
    void rebuild_runtime_state();
    void clear_animation_groups();
    void update_animation_slot(int slot);
    void animate_node_tree(int index, bool use_node_transform,
                           formats::Matrix4 parent_transform,
                           formats::Vector3 scale, bool attachments_before_children,
                           std::vector<bool>& visiting,
                           std::vector<bool>& visited);

    [[nodiscard]] const NodeAnimationGroup* node_group() const noexcept;
    [[nodiscard]] const MaterialAnimationGroup* material_group() const noexcept;
    [[nodiscard]] const TexcoordAnimationGroup* texcoord_group() const noexcept;
    [[nodiscard]] const TextureAnimationGroup* texture_group() const noexcept;

    [[nodiscard]] static formats::Matrix4 multiply(
        const formats::Matrix4& first, const formats::Matrix4& second) noexcept;
    [[nodiscard]] static formats::Matrix4 clear_rotation(
        formats::Matrix4 matrix) noexcept;
    [[nodiscard]] formats::Matrix4 base_texcoord_matrix(
        const Material& material) const noexcept;

    const File* model_ = nullptr;
    AnimationState animation_;
    std::vector<NodeRuntimeState> nodes_;
    std::vector<MaterialRuntimeState> materials_;
    std::vector<formats::Matrix4> matrix_stack_;
    std::vector<float> matrix_stack_values_;
    bool node_animation_ignore_root_ = false;
    bool node_matrices_valid_ = false;
};

} // namespace fruityprime::model
