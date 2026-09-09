// Native counterpart of MphRead/Mods/Render/PreviewCamera.cs.
// The camera basis is frontend-neutral and is shared by room previews and
// thumbnail callers.
#include "render_mods.hpp"

#include <cmath>
#include <limits>

namespace fruityprime::mods::render {
namespace {

[[nodiscard]] formats::Vector3 subtract(formats::Vector3 left,
                                         formats::Vector3 right) noexcept {
    return {left.x - right.x, left.y - right.y, left.z - right.z};
}

[[nodiscard]] formats::Vector3 cross_product(formats::Vector3 left,
                                               formats::Vector3 right) noexcept {
    return {left.y * right.z - left.z * right.y,
            left.z * right.x - left.x * right.z,
            left.x * right.y - left.y * right.x};
}

[[nodiscard]] float length_squared(formats::Vector3 value) noexcept {
    return value.x * value.x + value.y * value.y + value.z * value.z;
}

[[nodiscard]] formats::Vector3 normalized_or(formats::Vector3 value,
                                             formats::Vector3 fallback) noexcept {
    const float squared = length_squared(value);
    if (squared <= std::numeric_limits<float>::epsilon()) {
        return fallback;
    }
    const float inverse = 1.0F / std::sqrt(squared);
    return {value.x * inverse, value.y * inverse, value.z * inverse};
}

} // namespace

PreviewPose preview_pose(formats::Vector3 position,
                         formats::Vector3 target) noexcept {
    PreviewPose result;
    result.position = position;
    result.target = target;
    result.facing = normalized_or(subtract(target, position),
                                  {0.0F, 0.0F, -1.0F});
    result.right = normalized_or(cross_product(
                                     result.facing, {0.0F, 1.0F, 0.0F}),
                                 {1.0F, 0.0F, 0.0F});
    result.up = normalized_or(cross_product(result.right, result.facing),
                              {0.0F, 1.0F, 0.0F});
    return result;
}

camera::CameraState preview_camera(formats::Vector3 position,
                                    formats::Vector3 target,
                                    float fov) noexcept {
    const PreviewPose pose = preview_pose(position, target);
    camera::CameraState result;
    result.position = pose.position;
    result.previous_position = pose.position;
    result.target = pose.target;
    result.up_vector = pose.up;
    result.facing = pose.facing;
    result.fov = std::isfinite(fov) && fov > 0.0F ? fov : 45.0F;
    return result;
}

} // namespace fruityprime::mods::render
