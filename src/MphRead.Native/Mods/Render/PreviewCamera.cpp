// Native counterpart of MphRead/Mods/Render/PreviewCamera.cs.
#include "Scene.hpp"

namespace fruityprime::scene_runtime {
namespace {

[[nodiscard]] formats::Vector3 normalized_or(
    formats::Vector3 value, formats::Vector3 fallback) noexcept {
    // PreviewCamera.cs uses LengthSquared < 0.0001f, including for the
    // camera-right fallback. Do not replace that managed boundary with the
    // vector type's machine-epsilon check.
    if (value.length_squared() < 0.0001F) {
        return fallback;
    }
    return value.normalized();
}

} // namespace

void Scene::set_preview_camera(formats::Vector3 position,
                               formats::Vector3 target) noexcept {
    // Keep the assignment order from PreviewCamera.cs. In particular, this
    // method changes the Scene camera fields; it does not stop an authored
    // camera sequence or replace the current FOV.
    camera_mode_ = renderer::CameraMode::Roam;
    input_mode_ = InputMode::CameraOnly;
    camera_position_ = position;

    const formats::Vector3 facing = normalized_or(
        target - position, {0.0F, 0.0F, -1.0F});
    camera_facing_ = facing;
    camera_right_ = normalized_or(
        formats::cross(camera_facing_, {0.0F, 1.0F, 0.0F}),
        {1.0F, 0.0F, 0.0F});
    camera_up_ = formats::cross(camera_right_, camera_facing_).normalized();

    // camera_state_ is the native camera-sequence adapter still consumed by
    // the room/runtime tests. Mirror only the pose that this managed method
    // actually changes: target is an input to Facing, not a stored field.
    const bool had_camera_state = camera_state_.has_value();
    camera::CameraState result = camera_state_.value_or(camera::CameraState{});
    result.position = camera_position_;
    result.facing = camera_facing_;
    result.up_vector = camera_up_;
    if (!had_camera_state) {
        // CameraState stores authored FOV in degrees; the Scene field above
        // stores the Renderer.cs value in radians.
        result.fov = 78.0F;
    }
    camera_state_ = result;
}

} // namespace fruityprime::scene_runtime
