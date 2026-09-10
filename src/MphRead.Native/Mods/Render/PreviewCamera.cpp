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
    // This is the native Scene representation of the managed fields set by
    // SetPreviewCamera. The managed method does not stop an authored camera
    // sequence and does not replace the current FOV.
    camera::CameraState result = camera_state_.value_or(camera::CameraState{});
    result.position = position;
    result.target = target;

    const formats::Vector3 facing = normalized_or(
        target - position, {0.0F, 0.0F, -1.0F});
    const formats::Vector3 right = normalized_or(
        formats::cross(facing, {0.0F, 1.0F, 0.0F}),
        {1.0F, 0.0F, 0.0F});
    result.facing = facing;
    result.up_vector = formats::cross(right, facing).normalized();
    camera_state_ = result;
}

} // namespace fruityprime::scene_runtime
