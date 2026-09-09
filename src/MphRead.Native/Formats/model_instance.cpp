#include "Formats/model_animation_groups.hpp"
#include "Formats/model_instance.hpp"

#include <algorithm>
#include <cmath>
#include <functional>
#include <limits>
#include <stdexcept>

namespace fruityprime::model {
namespace {

constexpr float FullTurn = 2.0F * 3.14159265358979323846F;

void validate_slot(int slot) {
    if (slot < 0 || slot >= 2) {
        throw std::out_of_range("model animation slot must be 0 or 1");
    }
}

template <typename Group>
int group_index_for(const std::vector<Group>& groups, int index) {
    if (index < 0 || static_cast<std::size_t>(index) >= groups.size()) {
        throw std::out_of_range("model animation index is outside its groups");
    }
    return index;
}

[[nodiscard]] formats::Vector3 node_angle(const Node& node) noexcept {
    return {
        static_cast<float>(node.angle_x) / 65536.0F * FullTurn,
        static_cast<float>(node.angle_y) / 65536.0F * FullTurn,
        static_cast<float>(node.angle_z) / 65536.0F * FullTurn
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

} // namespace

ModelInstance::ModelInstance(const File& model) {
    set_model(model);
}

void ModelInstance::set_model(const File& model) {
    model_ = &model;
    rebuild_runtime_state();
}

void ModelInstance::rebuild_runtime_state() {
    if (model_ == nullptr) {
        throw std::logic_error("model instance has no model");
    }

    nodes_.clear();
    nodes_.reserve(model_->nodes().size());
    for (const Node& node : model_->nodes()) {
        NodeRuntimeState state;
        state.enabled = node.enabled != 0;
        state.scale = {node.scale.x.to_float(), node.scale.y.to_float(), node.scale.z.to_float()};
        state.angle = node_angle(node);
        state.position = {node.position.x.to_float(), node.position.y.to_float(), node.position.z.to_float()};
        nodes_.push_back(state);
    }

    materials_.clear();
    materials_.reserve(model_->materials().size());
    for (const Material& material : model_->materials()) {
        MaterialRuntimeState state;
        state.current_diffuse = {
            static_cast<float>(material.diffuse.red) / 31.0F,
            static_cast<float>(material.diffuse.green) / 31.0F,
            static_cast<float>(material.diffuse.blue) / 31.0F
        };
        state.current_ambient = {
            static_cast<float>(material.ambient.red) / 31.0F,
            static_cast<float>(material.ambient.green) / 31.0F,
            static_cast<float>(material.ambient.blue) / 31.0F
        };
        state.current_specular = {
            static_cast<float>(material.specular.red) / 31.0F,
            static_cast<float>(material.specular.green) / 31.0F,
            static_cast<float>(material.specular.blue) / 31.0F
        };
        state.current_alpha = static_cast<float>(material.alpha) / 31.0F;
        state.current_texture_id = material.texture_id;
        state.current_palette_id = material.palette_id;
        state.texcoord_matrix = base_texcoord_matrix(material);
        materials_.push_back(state);
    }

    matrix_stack_.assign(model_->node_weights().size(), formats::Matrix4{});
    matrix_stack_values_.assign(model_->node_weights().size() * 16, 0.0F);
    for (std::size_t index = 0; index < matrix_stack_.size(); ++index) {
        const formats::Matrix4& matrix = matrix_stack_[index];
        const std::size_t base = index * 16;
        matrix_stack_values_[base + 0] = matrix.m11;
        matrix_stack_values_[base + 1] = matrix.m12;
        matrix_stack_values_[base + 2] = matrix.m13;
        matrix_stack_values_[base + 3] = matrix.m14;
        matrix_stack_values_[base + 4] = matrix.m21;
        matrix_stack_values_[base + 5] = matrix.m22;
        matrix_stack_values_[base + 6] = matrix.m23;
        matrix_stack_values_[base + 7] = matrix.m24;
        matrix_stack_values_[base + 8] = matrix.m31;
        matrix_stack_values_[base + 9] = matrix.m32;
        matrix_stack_values_[base + 10] = matrix.m33;
        matrix_stack_values_[base + 11] = matrix.m34;
        matrix_stack_values_[base + 12] = matrix.m41;
        matrix_stack_values_[base + 13] = matrix.m42;
        matrix_stack_values_[base + 14] = matrix.m43;
        matrix_stack_values_[base + 15] = matrix.m44;
    }
    node_matrices_valid_ = false;
}

void ModelInstance::clear_animation_groups() {
    animation_.node.group = -1;
    animation_.node.source = nullptr;
    animation_.material.group = -1;
    animation_.material.source = nullptr;
    animation_.texcoord.group = -1;
    animation_.texcoord.source = nullptr;
    animation_.texture.group = -1;
    animation_.texture.source = nullptr;
}

void ModelInstance::set_animation(int index, AnimationFlags flags) {
    if (model_ == nullptr || !model_->animations().any() || index < 0) {
        clear_animation_groups();
        return;
    }

    const AnimationResults& groups = model_->animations();
    const std::size_t maximum = std::max({
        groups.node_groups.size(), groups.material_groups.size(),
        groups.texcoord_groups.size(), groups.texture_groups.size()
    });
    if (static_cast<std::size_t>(index) >= maximum) {
        throw std::out_of_range("model animation index is outside all groups");
    }

    animation_.step[0] = 1;
    animation_.flags[0] = flags;
    animation_.prev_index[0] = animation_.index[0];
    animation_.index[0] = index;
    animation_.material.slot = 0;
    animation_.texture.slot = 0;
    animation_.texcoord.slot = 0;
    animation_.node.slot = 0;
    animation_.material.group = group_index_for(groups.material_groups, index);
    animation_.material.source = &groups;
    animation_.texture.group = group_index_for(groups.texture_groups, index);
    animation_.texture.source = &groups;
    animation_.texcoord.group = group_index_for(groups.texcoord_groups, index);
    animation_.texcoord.source = &groups;
    animation_.node.group = group_index_for(groups.node_groups, index);
    animation_.node.source = &groups;

    if (animation_.node.group >= 0
        && groups.node_groups[static_cast<std::size_t>(animation_.node.group)]
                .animations.size() > 0) {
        animation_.frame_count[0] = static_cast<int>(
            groups.node_groups[static_cast<std::size_t>(animation_.node.group)]
                .frame_count);
    } else if (animation_.material.group >= 0
               && groups.material_groups[static_cast<std::size_t>(
                      animation_.material.group)].animations.size() > 0) {
        animation_.frame_count[0] = static_cast<int>(
            groups.material_groups[static_cast<std::size_t>(
                animation_.material.group)].frame_count);
    } else if (animation_.texture.group >= 0
               && groups.texture_groups[static_cast<std::size_t>(
                      animation_.texture.group)].animations.size() > 0) {
        animation_.frame_count[0] = static_cast<int>(
            groups.texture_groups[static_cast<std::size_t>(
                animation_.texture.group)].frame_count);
    } else if (animation_.texcoord.group >= 0
               && groups.texcoord_groups[static_cast<std::size_t>(
                      animation_.texcoord.group)].animations.size() > 0) {
        animation_.frame_count[0] = static_cast<int>(
            groups.texcoord_groups[static_cast<std::size_t>(
                animation_.texcoord.group)].frame_count);
    }
    animation_.frame[0] = has_flag(flags, AnimationFlags::Reverse)
        ? animation_.frame_count[0] - 1
        : 0;
}

void ModelInstance::set_animation(int index, int slot,
                                  AnimationSetFlags set_flags,
                                  AnimationFlags flags) {
    validate_slot(slot);
    if (model_ == nullptr || !model_->animations().any() || index < 0) {
        clear_animation_groups();
        return;
    }

    const AnimationResults& groups = model_->animations();
    animation_.step[slot] = 1;
    animation_.flags[slot] = flags;
    animation_.prev_index[slot] = animation_.index[slot];
    animation_.index[slot] = index;

    // Keep this order in sync with ModelInstance.SetAnimation(int, int,
    // SetFlags, AnimFlags). The last selected non-empty group determines the
    // frame count for this overload.
    if (has_flag(set_flags, AnimationSetFlags::Material)) {
        animation_.material.slot = slot;
        animation_.material.group = group_index_for(groups.material_groups,
                                                     index);
    animation_.material.source = &groups;
        if (!groups.material_groups.at(index).animations.empty()) {
            animation_.frame_count[slot] = static_cast<int>(groups.material_groups.at(index).frame_count);
        }
    }
    if (has_flag(set_flags, AnimationSetFlags::Texcoord)) {
        animation_.texcoord.slot = slot;
        animation_.texcoord.group = group_index_for(groups.texcoord_groups,
                                                    index);
    animation_.texcoord.source = &groups;
        if (!groups.texcoord_groups.at(index).animations.empty()) {
            animation_.frame_count[slot] = static_cast<int>(groups.texcoord_groups.at(index).frame_count);
        }
    }
    if (has_flag(set_flags, AnimationSetFlags::Texture)) {
        animation_.texture.slot = slot;
        animation_.texture.group = group_index_for(groups.texture_groups,
                                                   index);
    animation_.texture.source = &groups;
        if (!groups.texture_groups.at(index).animations.empty()) {
            animation_.frame_count[slot] = static_cast<int>(groups.texture_groups.at(index).frame_count);
        }
    }
    if (has_flag(set_flags, AnimationSetFlags::Node)) {
        animation_.node.slot = slot;
        animation_.node.group = group_index_for(groups.node_groups, index);
    animation_.node.source = &groups;
        if (!groups.node_groups.at(index).animations.empty()) {
            animation_.frame_count[slot] = static_cast<int>(groups.node_groups.at(index).frame_count);
        }
    }
    animation_.frame[slot] = has_flag(flags, AnimationFlags::Reverse)
        ? animation_.frame_count[slot] - 1
        : 0;
}

void ModelInstance::set_node_anim(int index) {
    if (index <= -1 || static_cast<std::size_t>(index) >= model_->animations().node_groups.size()) {
        animation_.index.at(animation_.node.slot) = -1;
        animation_.node.group = -1;
        animation_.node.source = nullptr;
    } else {
        set_animation(index, animation_.node.slot, AnimationSetFlags::Node);
    }
}

void ModelInstance::set_material_anim(int index) {
    if (index <= -1 || static_cast<std::size_t>(index) >= model_->animations().material_groups.size()) {
        animation_.index.at(animation_.material.slot) = -1;
        animation_.material.group = -1;
        animation_.material.source = nullptr;
    } else {
        set_animation(index, animation_.material.slot, AnimationSetFlags::Material);
    }
}

void ModelInstance::filter_nodes(int layer_mask) {
    for (std::size_t i = 0; i < nodes_.size(); ++i) {
        nodes_[i].enabled = node_enabled_for_layer(model_->nodes()[i].name, layer_mask);
    }
}

bool ModelInstance::node_parents_enabled(std::size_t node_index) const {
    int parent = model_->nodes().at(node_index).parent_id;
    std::size_t remaining = nodes_.size();
    while (parent != -1) {
        if (remaining-- == 0) throw std::runtime_error("model node parent cycle");
        if (!nodes_.at(parent).enabled) return false;
        parent = model_->nodes().at(parent).parent_id;
    }
    return true;
}

void ModelInstance::set_animation_step(int slot, int step) {
    validate_slot(slot);
    animation_.step[slot] = step;
}

void ModelInstance::set_animation_frame(int slot, int frame) {
    validate_slot(slot);
    if (frame < 0) {
        throw std::invalid_argument("model animation frame must be non-negative");
    }
    const int frame_count = animation_.frame_count[slot];
    if (frame_count <= 0) {
        animation_.frame[slot] = 0;
        return;
    }
    // Gameplay cursors are decoded from the managed state machine.  Clamp a
    // cursor at the authored last frame when a controller's logical timer is
    // longer than the model LUT (or has just reached its no-loop end).
    animation_.frame[slot] = std::min(frame, frame_count - 1);
}

void ModelInstance::update_anim_frames() {
    const bool slot_zero = animation_.node.slot == 0
        || animation_.material.slot == 0
        || animation_.texcoord.slot == 0
        || animation_.texture.slot == 0;
    const bool slot_one = animation_.node.slot == 1
        || animation_.material.slot == 1
        || animation_.texcoord.slot == 1
        || animation_.texture.slot == 1;
    if (slot_zero) {
        update_animation_slot(0);
    }
    if (slot_one) {
        update_animation_slot(1);
    }
}

void ModelInstance::update_animation_slot(int slot) {
    validate_slot(slot);
    AnimationFlags flags = animation_.flags[slot];
    int frame = animation_.frame[slot];
    const int step = animation_.step[slot];
    const int frame_count = animation_.frame_count[slot];
    if (has_flag(flags, AnimationFlags::Paused)
        || has_flag(flags, AnimationFlags::Ended)) {
        return;
    }
    if (has_flag(flags, AnimationFlags::PingPong)) {
        if (has_flag(flags, AnimationFlags::Reverse)) {
            if (frame <= step) {
                animation_.frame[slot] = step - frame;
                animation_.flags[slot] ^= AnimationFlags::Reverse;
            } else {
                animation_.frame[slot] = frame - step;
            }
        } else {
            frame += step;
            animation_.frame[slot] = frame;
            if (frame >= frame_count - 1) {
                animation_.frame[slot] = 2 * frame_count - frame - 2;
                animation_.flags[slot] ^= AnimationFlags::Reverse;
            }
        }
    } else if (has_flag(flags, AnimationFlags::Reverse)) {
        if (frame > step) {
            animation_.frame[slot] = frame - step;
        } else if (has_flag(flags, AnimationFlags::NoLoop)) {
            animation_.frame[slot] = 0;
            animation_.flags[slot] |= AnimationFlags::Ended;
        } else if (frame == step) {
            animation_.frame[slot] = 0;
        } else {
            animation_.frame[slot] = frame_count - (step - frame);
        }
    } else {
        frame += step;
        animation_.frame[slot] = frame;
        if (frame >= frame_count - 1) {
            if (has_flag(flags, AnimationFlags::NoLoop)) {
                animation_.frame[slot] = frame_count - 1;
                animation_.flags[slot] |= AnimationFlags::Ended;
            } else if (frame >= frame_count) {
                animation_.frame[slot] = frame - frame_count;
            }
        }
    }
}

const NodeAnimationGroup* ModelInstance::node_group() const noexcept {
    const auto* source = animation_.node.source;
    if (animation_.node.group < 0 || source == nullptr
        || static_cast<std::size_t>(animation_.node.group) >= source->node_groups.size()) {
        return nullptr;
    }
    return &source->node_groups[static_cast<std::size_t>(animation_.node.group)];
}

const MaterialAnimationGroup* ModelInstance::material_group() const noexcept {
    const auto* source = animation_.material.source;
    if (animation_.material.group < 0 || source == nullptr
        || static_cast<std::size_t>(animation_.material.group) >= source->material_groups.size()) {
        return nullptr;
    }
    return &source->material_groups[static_cast<std::size_t>(animation_.material.group)];
}

const TexcoordAnimationGroup* ModelInstance::texcoord_group() const noexcept {
    const auto* source = animation_.texcoord.source;
    if (animation_.texcoord.group < 0 || source == nullptr
        || static_cast<std::size_t>(animation_.texcoord.group) >= source->texcoord_groups.size()) {
        return nullptr;
    }
    return &source->texcoord_groups[static_cast<std::size_t>(animation_.texcoord.group)];
}

const TextureAnimationGroup* ModelInstance::texture_group() const noexcept {
    const auto* source = animation_.texture.source;
    if (animation_.texture.group < 0 || source == nullptr
        || static_cast<std::size_t>(animation_.texture.group) >= source->texture_groups.size()) {
        return nullptr;
    }
    return &source->texture_groups[static_cast<std::size_t>(animation_.texture.group)];
}

formats::Matrix4 ModelInstance::multiply(const formats::Matrix4& first,
                                          const formats::Matrix4& second)
    noexcept {
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

formats::Matrix4 ModelInstance::clear_rotation(formats::Matrix4 matrix)
    noexcept {
    const float sx = formats::Vector3(matrix.m11, matrix.m12, matrix.m13).length();
    const float sy = formats::Vector3(matrix.m21, matrix.m22, matrix.m23).length();
    const float sz = formats::Vector3(matrix.m31, matrix.m32, matrix.m33).length();
    matrix.m11 = sx;
    matrix.m12 = 0.0F;
    matrix.m13 = 0.0F;
    matrix.m21 = 0.0F;
    matrix.m22 = sy;
    matrix.m23 = 0.0F;
    matrix.m31 = 0.0F;
    matrix.m32 = 0.0F;
    matrix.m33 = sz;
    return matrix;
}

formats::Matrix4 ModelInstance::base_texcoord_matrix(
    const Material& material) const noexcept {
    const float scale_s = material.scale_s.to_float();
    const float scale_t = material.scale_t.to_float();
    const float rotate = static_cast<float>(material.rotate_z) / 65536.0F
        * FullTurn;
    formats::Matrix4 matrix = translation_matrix(
        scale_s * material.translate_s.to_float(),
        scale_t * material.translate_t.to_float(), 0.0F);
    matrix = multiply(scale_matrix(scale_s, scale_t, 1.0F), matrix);
    return multiply(rotation_z_matrix(rotate), matrix);
}

void ModelInstance::compute_node_matrices() {
    compute_node_matrices(0);
}

void ModelInstance::compute_node_matrices(int index) {
    if (nodes_.empty() || index == -1) {
        node_matrices_valid_ = true;
        return;
    }
    const float scale = model_->world_scale();
    std::vector<bool> visiting(nodes_.size(), false);
    std::function<void(int)> visit = [&](int current) {
        if (current == -1) return;
        const auto& node = model_->nodes().at(current);
        auto& state = nodes_.at(current);
        if (visiting.at(current)) throw std::runtime_error("model node hierarchy contains a cycle");
        visiting[current] = true;
        auto transform = formats::MatrixOps::get_transform_srt(
            state.scale, state.angle, state.position / scale);
        if (node.parent_id != -1) {
            transform = multiply(transform, nodes_.at(node.parent_id).transform);
        }
        state.transform = transform;
        visit(node.child_id);
        visit(node.next_id);
        visiting[current] = false;
    };
    visit(index);
    node_matrices_valid_ = true;
}

void ModelInstance::animate_nodes(bool use_node_transform,
                                  formats::Matrix4 parent_transform) {
    if (nodes_.empty()) return;
    if (!node_matrices_valid_) compute_node_matrices();
    const float scale = model_->world_scale();
    animate_nodes(0, use_node_transform, parent_transform, {scale, scale, scale});
}

void ModelInstance::animate_nodes(int index, bool use_node_transform,
    formats::Matrix4 parent_transform, formats::Vector3 scale) {
    if (nodes_.empty()) return;
    std::vector<bool> visiting(nodes_.size(), false), visited(nodes_.size(), false);
    animate_node_tree(index, use_node_transform, parent_transform, scale, false, visiting, visited);
}

void ModelInstance::animate_nodes2(int index, bool use_node_transform,
    formats::Matrix4 parent_transform, formats::Vector3 scale) {
    if (nodes_.empty()) return;
    std::vector<bool> visiting(nodes_.size(), false), visited(nodes_.size(), false);
    animate_node_tree(index, use_node_transform, parent_transform, scale, true, visiting, visited);
}

void ModelInstance::animate_node_tree(int index, bool use_node_transform,
                                      formats::Matrix4 parent_transform,
                                      formats::Vector3 scale, bool attachments_before_children,
                                      std::vector<bool>& visiting,
                                      std::vector<bool>& visited) {
    if (index < 0) {
        return;
    }
    const std::size_t current = static_cast<std::size_t>(index);
    if (current >= model_->nodes().size()) {
        throw std::out_of_range("model node link is outside the model");
    }
    if (visited[current]) {
        return;
    }
    if (visiting[current]) {
        throw std::runtime_error("model node links contain a cycle");
    }
    visiting[current] = true;

    const Node& node = model_->nodes()[current];
    NodeRuntimeState& state = nodes_[current];
    formats::Matrix4 transform = use_node_transform
        ? state.transform
        : formats::Matrix4{};
    const NodeAnimationGroup* group = node_group();
    if (group != nullptr) {
        const auto animation = group->animations.find(node.name);
        if (animation != group->animations.end()) {
            transform = fruityprime::model::animate_node(
                *group, animation->second, animation_.node_frame(),
                scale);
            if (node.parent_id >= 0 && !state.anim_ignore_parent) {
                const std::size_t parent = static_cast<std::size_t>(
                    node.parent_id);
                if (parent >= nodes_.size()) {
                    throw std::out_of_range(
                        "animated model node parent is outside the model");
                }
                transform = multiply(transform, nodes_[parent].animation);
            }
        }
    }
    state.animation = transform;

    const auto apply_attachment = [&] {
        if (state.after_transform.has_value()) {
            state.animation = multiply(multiply(*state.after_transform, state.animation), parent_transform);
        } else if (state.before_transform.has_value()) {
            state.animation = multiply(multiply(state.animation, parent_transform), *state.before_transform);
        }
    };
    if (attachments_before_children) apply_attachment();
    if (node.child_id != -1 && !state.anim_ignore_child) {
        animate_node_tree(node.child_id, use_node_transform, parent_transform,
                          scale, attachments_before_children, visiting, visited);
    }
    if (!attachments_before_children) apply_attachment();
    state.animation = multiply(state.animation, parent_transform);

    visiting[current] = false;
    visited[current] = true;
    if (node.next_id >= 0) {
        animate_node_tree(node.next_id, use_node_transform, parent_transform,
                          scale, attachments_before_children, visiting, visited);
    } else if (node.next_id != -1) {
        throw std::out_of_range("model node sibling is outside the model");
    }
}

void ModelInstance::update_matrix_stack() {
    if (model_ == nullptr) {
        return;
    }
    const std::vector<std::int32_t>& weights = model_->node_weights();
    matrix_stack_.assign(weights.size(), formats::Matrix4{});
    matrix_stack_values_.assign(weights.size() * 16, 0.0F);
    for (std::size_t index = 0; index < weights.size(); ++index) {
        const std::int32_t node_index = weights[index];
        if (node_index < 0
            || static_cast<std::size_t>(node_index) >= nodes_.size()) {
            throw std::out_of_range("model matrix-stack node is outside the model");
        }
        formats::Matrix4 matrix = nodes_[static_cast<std::size_t>(node_index)]
            .animation;
        const std::uint32_t billboard = model_->nodes()[
            static_cast<std::size_t>(node_index)].billboard_mode;
        if (billboard == 1 || billboard == 2) {
            matrix = clear_rotation(matrix);
        }
        matrix_stack_[index] = matrix;
        const std::size_t base = index * 16;
        matrix_stack_values_[base + 0] = matrix.m11;
        matrix_stack_values_[base + 1] = matrix.m12;
        matrix_stack_values_[base + 2] = matrix.m13;
        matrix_stack_values_[base + 3] = matrix.m14;
        matrix_stack_values_[base + 4] = matrix.m21;
        matrix_stack_values_[base + 5] = matrix.m22;
        matrix_stack_values_[base + 6] = matrix.m23;
        matrix_stack_values_[base + 7] = matrix.m24;
        matrix_stack_values_[base + 8] = matrix.m31;
        matrix_stack_values_[base + 9] = matrix.m32;
        matrix_stack_values_[base + 10] = matrix.m33;
        matrix_stack_values_[base + 11] = matrix.m34;
        matrix_stack_values_[base + 12] = matrix.m41;
        matrix_stack_values_[base + 13] = matrix.m42;
        matrix_stack_values_[base + 14] = matrix.m43;
        matrix_stack_values_[base + 15] = matrix.m44;
    }
}

void ModelInstance::animate_materials() {
    const MaterialAnimationGroup* group = material_group();
    for (std::size_t index = 0; index < model_->materials().size(); ++index) {
        const Material& material = model_->materials()[index];
        MaterialRuntimeState& state = materials_[index];
        state.current_diffuse = {
            static_cast<float>(material.diffuse.red) / 31.0F,
            static_cast<float>(material.diffuse.green) / 31.0F,
            static_cast<float>(material.diffuse.blue) / 31.0F
        };
        state.current_ambient = {
            static_cast<float>(material.ambient.red) / 31.0F,
            static_cast<float>(material.ambient.green) / 31.0F,
            static_cast<float>(material.ambient.blue) / 31.0F
        };
        state.current_specular = {
            static_cast<float>(material.specular.red) / 31.0F,
            static_cast<float>(material.specular.green) / 31.0F,
            static_cast<float>(material.specular.blue) / 31.0F
        };
        state.current_alpha = static_cast<float>(material.alpha) / 31.0F;
        if (group == nullptr) {
            continue;
        }
        const auto animation = group->animations.find(material.name);
        if (animation == group->animations.end()) {
            continue;
        }
        const raw::MaterialAnimation& value = animation->second;
        const int frame = animation_.material_frame();
        const auto sample = [&](std::uint16_t start, std::uint8_t blend,
                                std::uint16_t length) {
            return interpolate_animation(group->colors, start, frame, blend,
                                         length, group->frame_count);
        };
        const MaterialAnimationFlags material_flags =
            static_cast<MaterialAnimationFlags>(material.animation_flags);
        if (!has_flag(material_flags, MaterialAnimationFlags::DisableColor)) {
            state.current_diffuse = {
                sample(value.diffuse_lut_index_r, value.diffuse_blend_r,
                       value.diffuse_lut_length_r) / 31.0F,
                sample(value.diffuse_lut_index_g, value.diffuse_blend_g,
                       value.diffuse_lut_length_g) / 31.0F,
                sample(value.diffuse_lut_index_b, value.diffuse_blend_b,
                       value.diffuse_lut_length_b) / 31.0F
            };
            state.current_ambient = {
                sample(value.ambient_lut_index_r, value.ambient_blend_r,
                       value.ambient_lut_length_r) / 31.0F,
                sample(value.ambient_lut_index_g, value.ambient_blend_g,
                       value.ambient_lut_length_g) / 31.0F,
                sample(value.ambient_lut_index_b, value.ambient_blend_b,
                       value.ambient_lut_length_b) / 31.0F
            };
            state.current_specular = {
                sample(value.specular_lut_index_r, value.specular_blend_r,
                       value.specular_lut_length_r) / 31.0F,
                sample(value.specular_lut_index_g, value.specular_blend_g,
                       value.specular_lut_length_g) / 31.0F,
                sample(value.specular_lut_index_b, value.specular_blend_b,
                       value.specular_lut_length_b) / 31.0F
            };
        }
        if (!has_flag(material_flags, MaterialAnimationFlags::DisableAlpha)) {
            state.current_alpha = sample(value.alpha_lut_index,
                                         value.alpha_blend,
                                         value.alpha_lut_length) / 31.0F;
        }
    }
}

void ModelInstance::animate_textures() {
    const TextureAnimationGroup* group = texture_group();
    for (std::size_t index = 0; index < model_->materials().size(); ++index) {
        const Material& material = model_->materials()[index];
        MaterialRuntimeState& state = materials_[index];
        state.current_texture_id = material.texture_id;
        state.current_palette_id = material.palette_id;
        if (group == nullptr) {
            continue;
        }
        const auto animation = group->animations.find(material.name);
        if (animation == group->animations.end()) {
            continue;
        }
        const int frame = animation_.texture_frame();
        if (frame < 0
            || static_cast<std::uint64_t>(frame)
                   > std::numeric_limits<std::uint16_t>::max()) {
            continue;
        }
        const TextureAnimationSelection selection = select_texture_animation(
            *group, animation->second, static_cast<std::uint16_t>(frame));
        if (selection.found) {
            state.current_texture_id = selection.texture_id;
            state.current_palette_id = selection.palette_id;
        }
    }
}

void ModelInstance::animate_texcoords() {
    const TexcoordAnimationGroup* group = texcoord_group();
    for (std::size_t index = 0; index < model_->materials().size(); ++index) {
        const Material& material = model_->materials()[index];
        MaterialRuntimeState& state = materials_[index];
        state.texcoord_matrix = base_texcoord_matrix(material);
        if (group == nullptr) {
            continue;
        }
        const auto animation = group->animations.find(material.name);
        if (animation != group->animations.end()) {
            state.texcoord_matrix = fruityprime::model::animate_texcoords(
                *group, animation->second, animation_.texcoord_frame());
        }
    }
}

void ModelInstance::update_materials() {
    animate_materials();
    animate_textures();
    animate_texcoords();
}

} // namespace fruityprime::model
