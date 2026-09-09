#include "Renderer.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <bit>
#include <stdexcept>
#include <utility>

namespace fruityprime::renderer {

float Viewport::aspect() const noexcept {
    return width > 0 && height > 0
        ? static_cast<float>(width) / static_cast<float>(height)
        : 0.0F;
}

namespace {

constexpr float Pi = 3.14159265358979323846F;
constexpr float DegreesToRadians = Pi / 180.0F;
constexpr float Epsilon = 1.0e-6F;

[[nodiscard]] formats::Vector3 safe_normalized(
    formats::Vector3 value, formats::Vector3 fallback) noexcept {
    const float squared = value.length_squared();
    if (!std::isfinite(squared) || squared <= Epsilon * Epsilon) {
        return fallback;
    }
    return value / std::sqrt(squared);
}

[[nodiscard]] float finite_or(float value, float fallback) noexcept {
    return std::isfinite(value) ? value : fallback;
}

[[nodiscard]] float round_four_places(float value) noexcept {
    return std::round(value * 10'000.0F) / 10'000.0F;
}

[[nodiscard]] formats::Matrix4 look_at(formats::Vector3 position,
                                        formats::Vector3 facing,
                                        formats::Vector3 up) noexcept {
    const formats::Vector3 forward = safe_normalized(
        facing, {0.0F, 0.0F, -1.0F});
    const formats::Vector3 right = safe_normalized(
        formats::cross(forward, up), {1.0F, 0.0F, 0.0F});
    const formats::Vector3 corrected_up = safe_normalized(
        formats::cross(right, forward), {0.0F, 1.0F, 0.0F});
    return {
        right.x, corrected_up.x, -forward.x, 0.0F,
        right.y, corrected_up.y, -forward.y, 0.0F,
        right.z, corrected_up.z, -forward.z, 0.0F,
        -formats::dot(right, position),
        -formats::dot(corrected_up, position),
        formats::dot(forward, position), 1.0F
    };
}

[[nodiscard]] formats::Matrix4 transpose_rotation(
    const formats::Matrix4& matrix) noexcept {
    formats::Matrix4 result;
    result.m11 = matrix.m11;
    result.m12 = matrix.m21;
    result.m13 = matrix.m31;
    result.m21 = matrix.m12;
    result.m22 = matrix.m22;
    result.m23 = matrix.m32;
    result.m31 = matrix.m13;
    result.m32 = matrix.m23;
    result.m33 = matrix.m33;
    return result;
}

[[nodiscard]] formats::FadeType fade_in_type(
    formats::FadeType type) noexcept {
    switch (type) {
    case formats::FadeType::FadeOutInBlack:
        return formats::FadeType::FadeInBlack;
    case formats::FadeType::FadeOutInWhite:
        return formats::FadeType::FadeInWhite;
    default:
        return formats::FadeType::None;
    }
}

[[nodiscard]] bool fade_in(formats::FadeType type) noexcept {
    return type == formats::FadeType::FadeInBlack
        || type == formats::FadeType::FadeInWhite;
}

[[nodiscard]] bool white_fade(formats::FadeType type) noexcept {
    return type == formats::FadeType::FadeInWhite
        || type == formats::FadeType::FadeOutWhite
        || type == formats::FadeType::FadeOutInWhite;
}

} // namespace

formats::Matrix4 Projection::matrix(Viewport viewport) const {
    const float aspect = static_cast<float>(viewport.width) / static_cast<float>(viewport.height);
    const float fov = fov_radians;
    const float near_plane = near_clip;
    const float far_plane = use_clip ? far_clip : 10'000.0F;
    if (fov <= 0 || fov > Pi || aspect <= 0 || near_plane <= 0
        || far_plane <= 0 || near_plane >= far_plane) {
        throw std::invalid_argument("invalid perspective projection parameters");
    }
    const float scale = 1.0F / std::tan(fov * 0.5F);
    return {
        scale / aspect, 0.0F, 0.0F, 0.0F,
        0.0F, scale, 0.0F, 0.0F,
        0.0F, 0.0F,
        (far_plane + near_plane) / (near_plane - far_plane), -1.0F,
        0.0F, 0.0F,
        (2.0F * far_plane * near_plane) / (near_plane - far_plane), 0.0F
    };
}

Camera::Camera() noexcept {
    rebuild();
}

void Camera::set_viewport(Viewport viewport) noexcept {
    viewport_ = viewport;
    rebuild();
}

void Camera::set_mode(CameraMode mode) noexcept {
    mode_ = mode;
    rebuild();
}

void Camera::set_pivot(float angle_x_degrees, float angle_y_degrees,
                       float distance) noexcept {
    pivot_angle_x_degrees_ = finite_or(angle_x_degrees, 0.0F);
    pivot_angle_y_degrees_ = finite_or(angle_y_degrees, 0.0F);
    pivot_distance_ = std::max(0.001F, finite_or(distance, 5.0F));
    rebuild();
}

void Camera::set_pose(formats::Vector3 position, formats::Vector3 facing,
                      formats::Vector3 up) noexcept {
    position_ = position;
    facing_ = safe_normalized(facing, {0.0F, 0.0F, -1.0F});
    up_ = safe_normalized(up, {0.0F, 1.0F, 0.0F});
    right_ = safe_normalized(
        formats::cross(facing_, up_), {1.0F, 0.0F, 0.0F});
    rebuild();
}

void Camera::set_fov_degrees(float degrees) noexcept {
    const float safe_degrees = std::clamp(
        finite_or(degrees, 78.0F), 1.0F, 179.0F);
    fov_radians_ = safe_degrees * DegreesToRadians;
    rebuild();
}

void Camera::set_clip(float far_clip, bool enabled) noexcept {
    far_clip_ = std::max(near_clip_ + 0.001F,
                         finite_or(far_clip, 10'000.0F));
    use_clip_ = enabled;
    rebuild();
}

void Camera::update() noexcept {
    rebuild();
}

void Camera::reset() noexcept {
    if (mode_ == CameraMode::Pivot) {
        pivot_angle_x_degrees_ = 0.0F;
        pivot_angle_y_degrees_ = 0.0F;
        pivot_distance_ = 5.0F;
    } else {
        position_ = {};
        facing_ = {0.0F, 0.0F, -1.0F};
        up_ = {0.0F, 1.0F, 0.0F};
        right_ = {1.0F, 0.0F, 0.0F};
    }
    rebuild();
}

void Camera::rotate(float horizontal_radians,
                    float vertical_radians) noexcept {
    const formats::Vector3 facing = safe_normalized(
        facing_, {0.0F, 0.0F, -1.0F});
    const float horizontal = std::atan2(facing.x, -facing.z)
        + finite_or(horizontal_radians, 0.0F);
    const float almost_half_pi = Pi * 0.5F - 0.000001F;
    const float vertical = std::clamp(
        std::asin(std::clamp(facing.y, -1.0F, 1.0F))
            + finite_or(vertical_radians, 0.0F),
        -almost_half_pi, almost_half_pi);
    facing_ = safe_normalized({
        std::cos(vertical) * std::sin(horizontal),
        std::sin(vertical),
        -(std::cos(vertical) * std::cos(horizontal))
    }, {0.0F, 0.0F, -1.0F});
    right_ = safe_normalized(
        formats::cross(facing_, {0.0F, 1.0F, 0.0F}),
        {1.0F, 0.0F, 0.0F});
    up_ = safe_normalized(
        formats::cross(right_, facing_), {0.0F, 1.0F, 0.0F});
    if (mode_ == CameraMode::Pivot) {
        mode_ = CameraMode::Roam;
    }
    rebuild();
}

void Camera::rebuild() noexcept {
    if (mode_ == CameraMode::Pivot) {
        const float theta = (pivot_angle_y_degrees_ + 90.0F)
            * DegreesToRadians;
        const float phi = (pivot_angle_x_degrees_ + 90.0F)
            * DegreesToRadians;
        position_ = {
            round_four_places(pivot_distance_ * std::cos(theta)),
            -round_four_places(pivot_distance_ * std::sin(theta)
                                * std::cos(phi)),
            round_four_places(pivot_distance_ * std::sin(theta)
                              * std::sin(phi))
        };
        facing_ = safe_normalized(-position_, {0.0F, 0.0F, -1.0F});
        up_ = {0.0F, 1.0F, 0.0F};
        right_ = safe_normalized(
            formats::cross(facing_, up_), {1.0F, 0.0F, 0.0F});
    } else {
        facing_ = safe_normalized(facing_, {0.0F, 0.0F, -1.0F});
        right_ = safe_normalized(
            formats::cross(facing_, up_), {1.0F, 0.0F, 0.0F});
        up_ = safe_normalized(
            formats::cross(right_, facing_), {0.0F, 1.0F, 0.0F});
    }

    view_matrix_ = look_at(position_, facing_, up_);
    view_inverse_rotation_ = transpose_rotation(view_matrix_);

    const formats::Vector3 horizontal_facing = safe_normalized(
        {facing_.x, 0.0F, facing_.z}, {0.0F, 0.0F, -1.0F});
    const formats::Vector3 horizontal_right = safe_normalized(
        formats::cross(horizontal_facing, {0.0F, 1.0F, 0.0F}),
        {1.0F, 0.0F, 0.0F});
    view_inverse_rotation_y_ = {
        horizontal_right.x, 0.0F, horizontal_right.z, 0.0F,
        0.0F, 1.0F, 0.0F, 0.0F,
        -horizontal_facing.x, 0.0F, -horizontal_facing.z, 0.0F,
        0.0F, 0.0F, 0.0F, 1.0F
    };

    // Before the host's first resize there is no usable viewport yet.
    const Viewport projection_viewport = viewport_.width > 0 && viewport_.height > 0
        ? viewport_ : Viewport{1, 1};
    perspective_matrix_ = Projection{
        fov_radians_, near_clip_, far_clip_, use_clip_
    }.matrix(projection_viewport);

    frustum_ = build_frustum(view_matrix_, position_, fov_radians_, projection_viewport, near_clip_);
}

culling::FrustumInfo build_frustum(const formats::Matrix4& view,
    formats::Vector3 camera_position, float fov, Viewport viewport, float near_clip) noexcept {
    const formats::Vector3 right{view.m11, view.m12, -view.m13};
    const formats::Vector3 up{view.m21, view.m22, -view.m23};
    const formats::Vector3 facing{view.m31, view.m32, -view.m33};
    const auto plane = [&](formats::Vector3 input, float offset = 0.0F) {
        const formats::Vector3 normal{formats::dot(input, right), formats::dot(input, up), formats::dot(input, facing)};
        return Camera::set_bounds_indices({normal, formats::dot(normal, camera_position) + offset});
    };
    const float aspect = static_cast<float>(viewport.width) / static_cast<float>(viewport.height);
    const float cosine = std::cos(fov / 2.0F), sine = std::sin(fov / 2.0F);
    const auto unit = [](formats::Vector3 v) { return v / v.length(); };
    culling::FrustumInfo info;
    info.index = 1; info.count = 5;
    info.planes[0] = plane({0, 0, 1}, near_clip);
    info.planes[1] = plane(unit({cosine / aspect, 0, sine}));
    info.planes[2] = plane(unit({-cosine / aspect, 0, sine}));
    info.planes[3] = plane(unit({0, -cosine, sine}));
    info.planes[4] = plane(unit({0, cosine, sine}));
    return info;
}

culling::FrustumPlane Camera::set_bounds_indices(
    formats::Vector4 plane) noexcept {
    culling::FrustumPlane result;
    result.x_index1 = plane.x < 0.0F ? 3 : 0;
    result.x_index2 = plane.x < 0.0F ? 0 : 3;
    result.y_index1 = plane.y < 0.0F ? 4 : 1;
    result.y_index2 = plane.y < 0.0F ? 1 : 4;
    result.z_index1 = plane.z < 0.0F ? 5 : 2;
    result.z_index2 = plane.z < 0.0F ? 2 : 5;
    result.plane = plane;
    return result;
}

void FadeController::begin_type(formats::FadeType type) noexcept {
    state_.type = type;
    state_.color = white_fade(type) ? 1.0F : 0.0F;
    state_.percent = 0.0F;
    state_.opacity = fade_in(type) ? 1.0F : 0.0F;
    state_.active = type != formats::FadeType::None;
    state_.ended = false;
}

void FadeController::set(formats::FadeType type, float length_seconds,
                         bool overwrite, AfterFade after,
                         float delay_seconds) noexcept {
    if (!overwrite && state_.type != formats::FadeType::None) return;
    length_seconds_ = length_seconds;
    delay_seconds_ = delay_seconds;
    start_seconds_ = elapsed_seconds_;
    ended_pending_ = false;
    pending_action_.reset();
    state_.after = after;
    begin_type(type);
}

void FadeController::update(float seconds) noexcept {
    elapsed_seconds_ += seconds;
    if (state_.type == formats::FadeType::None) {
        state_.ended = false;
        return;
    }
    // Managed delay expires on a frame boundary. An overshooting frame is
    // wholly consumed by the delay, rather than advancing the fade's cursor.
    if (delay_seconds_ > 0) {
        delay_seconds_ -= seconds;
        start_seconds_ = elapsed_seconds_;
    }
    state_.percent = (elapsed_seconds_ - start_seconds_) / length_seconds_;
    if (state_.percent >= 1) {
        state_.percent = 1;
        if (!state_.ended) {
            const auto action = state_.after;
            if (action != AfterFade::None) pending_action_ = action;
            if (action != AfterFade::Exit && action != AfterFade::EnterShip) {
                const auto next = fade_in_type(state_.type);
                if (next != formats::FadeType::None) {
                    set(next, length_seconds_, true);
                } else if (action == AfterFade::LoadRoom) {
                    const auto color = state_.type == formats::FadeType::FadeOutWhite
                        ? formats::FadeType::FadeInWhite : formats::FadeType::FadeInBlack;
                    set(color, 10.0F / 30.0F, true);
                } else if (action != AfterFade::PlayMovie && action != AfterFade::StopMovie) {
                    state_.type = formats::FadeType::None;
                    state_.color = 0;
                    state_.percent = 0;
                    start_seconds_ = 0;
                    length_seconds_ = 0;
                }
            }
            if (action != AfterFade::None) pending_action_ = action;
            state_.ended = true;
            ended_pending_ = true;
        }
    } else {
        state_.ended = false;
    }
    state_.active = state_.type != formats::FadeType::None;
    state_.opacity = fade_in(state_.type) ? 1 - state_.percent : state_.percent;
}

void FadeController::clear() noexcept {
    state_ = {};
    length_seconds_ = 0;
    delay_seconds_ = 0;
    start_seconds_ = elapsed_seconds_;
    ended_pending_ = false;
    pending_action_.reset();
}

bool FadeController::consume_ended() noexcept {
    const bool result = ended_pending_;
    ended_pending_ = false;
    return result;
}

std::optional<AfterFade> FadeController::consume_action() noexcept {
    const auto action = pending_action_;
    pending_action_.reset();
    return action;
}

std::uint32_t TextureMap::key(int texture_id, int palette_id, int recolor_id) {
    if (palette_id == -1) palette_id = 4095;
    if (texture_id < 0 || texture_id >= 4096 || palette_id < 0 || palette_id >= 4096
        || recolor_id < 0 || recolor_id >= 255) {
        throw std::out_of_range("texture binding key is outside the managed bit layout");
    }
    return static_cast<std::uint32_t>(texture_id)
        | (static_cast<std::uint32_t>(palette_id) << 12)
        | (static_cast<std::uint32_t>(recolor_id) << 24);
}

TextureBinding TextureMap::get(int texture_id, int palette_id, int recolor_id) const {
    return bindings_.at(key(texture_id, palette_id, recolor_id));
}

void TextureMap::add(int texture_id, int palette_id, int recolor_id,
                     int binding_id, bool only_opaque) {
    bindings_[key(texture_id, palette_id, recolor_id)] = {binding_id, only_opaque};
}

formats::Vector3 average_color(std::span<const formats::ColorRgba> pixels) noexcept {
    formats::Vector3 weighted{}, plain{};
    float weight = 0, count = 0;
    for (const auto pixel : pixels) {
        const float alpha = pixel.alpha / 255.0F;
        weighted.x += pixel.red * alpha;
        weighted.y += pixel.green * alpha;
        weighted.z += pixel.blue * alpha;
        weight += alpha;
        plain.x += pixel.red; plain.y += pixel.green; plain.z += pixel.blue;
        ++count;
    }
    if (weight > 0.01F) return weighted / weight / 255.0F;
    if (count > 0) return plain / count / 255.0F;
    return {1, 1, 1};
}

formats::RenderMode update_material_mode(formats::RenderMode current,
    float alpha, bool only_opaque) noexcept {
    using formats::RenderMode;
    if (alpha < 1) return RenderMode::Translucent;
    if (current != RenderMode::Normal && only_opaque) return RenderMode::Normal;
    if (current == RenderMode::Normal && !only_opaque) return RenderMode::Translucent;
    return current;
}

void RenderQueue::clear() noexcept {
    items_.clear(); decals_.clear(); non_decals_.clear(); translucent_.clear();
    next_polygon_id_ = 1;
}

int RenderQueue::next_polygon_id() noexcept {
    return std::bit_cast<std::int32_t>(next_polygon_id_++);
}

void RenderQueue::submit(formats::RenderItem item) {
    const auto index = items_.size();
    const bool decal = item.render_mode == formats::RenderMode::Decal;
    const bool translucent = item.render_mode == formats::RenderMode::Translucent || item.alpha < 1;
    items_.push_back(std::move(item));
    (decal ? decals_ : non_decals_).push_back(index);
    if (translucent) translucent_.push_back(index);
}

std::span<const std::size_t> RenderQueue::pass_items(ScenePass pass) const noexcept {
    switch (pass) {
    case ScenePass::Opaque: case ScenePass::RebuildDepth: return non_decals_;
    case ScenePass::Decal: return decals_;
    default: return translucent_;
    }
}

void RenderQueue::render(ScenePassBackend& backend) const {
    backend.begin_scene();
    ScenePassState state;
    const auto draw = [&](ScenePass pass) {
        backend.begin_pass(pass, state);
        for (const auto index : pass_items(pass)) {
            const auto& current = items_[index];
            backend.draw(current, state.stencil == StencilTest::Always ? 0 : current.polygon_id);
        }
    };
    draw(ScenePass::Opaque);
    state.alpha = AlphaTest::Disabled;
    state.depth = DepthTest::LessOrEqual;
    state.blend = true; state.polygon_offset = true;
    draw(ScenePass::Decal);
    state.polygon_offset = false;
    state.alpha = AlphaTest::LessThanOne;
    state.color_write = false;
    state.stencil_action = {StencilAction::Keep, StencilAction::Keep, StencilAction::Replace};
    state.stencil = StencilTest::Greater;
    draw(ScenePass::MarkTranslucent);
    state.clear_depth = true;
    state.stencil_action = {StencilAction::Keep, StencilAction::Keep, StencilAction::Keep};
    state.stencil = StencilTest::Always;
    state.alpha = AlphaTest::EqualOne;
    draw(ScenePass::RebuildDepth);
    state.clear_depth = false;
    state.alpha = AlphaTest::LessThanOne;
    state.color_write = true; state.depth_write = false;
    state.stencil = StencilTest::NotEqual;
    draw(ScenePass::TranslucentBehind);
    state.stencil = StencilTest::Equal;
    draw(ScenePass::TranslucentBefore);
    backend.end_scene();
}

void RenderQueue::add_mesh(formats::RenderItem item, float alpha_scale,
    float scale_factor, std::span<const float> stack, std::optional<int> binding_override,
    std::optional<formats::Vector4> selection_color) {
    if (stack.size() % 16 != 0 || stack.size() > item.matrix_stack.size())
        throw std::out_of_range("mesh matrix stack must contain at most 31 complete matrices");
    item.type = formats::RenderItemType::Mesh;
    item.alpha *= alpha_scale;
    auto& t = item.transform;
    t.m11 *= scale_factor; t.m12 *= scale_factor; t.m13 *= scale_factor;
    t.m21 *= scale_factor; t.m22 *= scale_factor; t.m23 *= scale_factor;
    t.m31 *= scale_factor; t.m32 *= scale_factor; t.m33 *= scale_factor;
    item.no_lines = false;
    item.matrix_stack_count = static_cast<int>(stack.size() / 16);
    for (std::size_t i = 0; i < stack.size(); ++i) {
        const auto element = i % 16;
        item.matrix_stack[i] = stack[i] * (element < 12 && element % 4 != 3 ? scale_factor : 1.0F);
    }
    if (binding_override) {
        item.texgen_mode = formats::TexgenMode::Normal;
        item.x_repeat = item.y_repeat = formats::RepeatMode::Mirror;
        item.has_texture = true;
        item.texture_binding_id = *binding_override;
    }
    if (selection_color) {
        item.override_color = selection_color;
        item.palette_override.reset();
    }
    item.points.clear(); item.scale_s = item.scale_t = 1;
    submit(std::move(item));
}

void RenderQueue::add_volume(formats::CullingMode culling, int polygon_id,
    formats::Vector4 color, formats::RenderItemType type,
    std::span<const formats::Vector3> points, int vertex_count, bool no_lines) {
    formats::RenderItem item;
    item.type = type; item.polygon_id = polygon_id;
    item.render_mode = formats::RenderMode::Translucent;
    item.culling_mode = culling; item.no_lines = no_lines;
    item.diffuse = {}; item.texture_binding_id = 0; item.list_id = 0;
    item.override_color = color;
    item.points.assign(points.begin(), points.end()); item.item_count = vertex_count;
    submit(std::move(item));
}

void RenderQueue::add_effect(formats::RenderItemType type, float alpha, int polygon_id,
    formats::Vector3 color, formats::RepeatMode x_repeat, formats::RepeatMode y_repeat,
    float scale_s, float scale_t, formats::Matrix4 transform,
    std::span<const formats::Vector3> points, int binding_id,
    formats::BillboardMode billboard, int trail_count) {
    formats::RenderItem item;
    item.type = type; item.alpha = alpha; item.polygon_id = polygon_id;
    item.render_mode = formats::RenderMode::Translucent;
    item.culling_mode = formats::CullingMode::Neither;
    item.billboard_mode = billboard; item.diffuse = color;
    item.x_repeat = x_repeat; item.y_repeat = y_repeat;
    item.has_texture = true; item.texture_binding_id = binding_id;
    item.transform = transform; item.list_id = 0;
    item.points.assign(points.begin(), points.end());
    item.scale_s = scale_s; item.scale_t = scale_t; item.item_count = trail_count;
    submit(std::move(item));
}

void RenderQueue::add_trail(formats::RenderItemType type, int polygon_id,
    formats::Vector3 color, formats::RepeatMode x_repeat, formats::RepeatMode y_repeat,
    float scale_s, float scale_t, int matrix_count, std::span<const float> stack,
    std::span<const formats::Vector3> points, int segments, int binding_id) {
    if (matrix_count < 0 || matrix_count > 31 || stack.size() < static_cast<std::size_t>(matrix_count) * 16)
        throw std::out_of_range("trail matrix stack is incomplete or exceeds 31 matrices");
    add_effect(type, 1, polygon_id, color, x_repeat, y_repeat, scale_s, scale_t,
               formats::Matrix4{}, points, binding_id, formats::BillboardMode::None, segments);
    auto& item = items_.back();
    item.matrix_stack_count = matrix_count;
    std::copy_n(stack.begin(), matrix_count * 16, item.matrix_stack.begin());
}

void Frame::begin(Viewport viewport) noexcept {
    viewport_ = viewport;
    items_.clear();
}

void Frame::submit(DrawItem item) {
    items_.push_back(item);
}

void Frame::clear() noexcept {
    viewport_ = {};
    items_.clear();
}

bool NullRenderer::begin(Viewport viewport) noexcept {
    frame_.begin(viewport);
    return viewport.width > 0 && viewport.height > 0;
}

void NullRenderer::draw(DrawItem item) {
    frame_.submit(item);
}

void NullRenderer::end() noexcept {}

} // namespace fruityprime::renderer
