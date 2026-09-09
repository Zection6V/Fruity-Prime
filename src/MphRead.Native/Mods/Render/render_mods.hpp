#pragma once

#include "Formats/camera_sequence.hpp"

#include <cstdint>
#include <span>

namespace fruityprime::mods::render {

// Native counterpart of PlayerEntityIconBounds.cs. The HUD weapon sheets are
// DS-tiled character data, so the visible ink bounds must be measured in the
// tiled source rather than in a post-scaled texture.
struct IconBounds {
    int min_x = 0;
    int min_y = 0;
    int max_x = -1;
    int max_y = -1;

    [[nodiscard]] int width() const noexcept { return max_x - min_x + 1; }
    [[nodiscard]] int height() const noexcept { return max_y - min_y + 1; }
    [[nodiscard]] float centre_x() const noexcept {
        return (min_x + max_x + 1) / 2.0F;
    }
    [[nodiscard]] float centre_y() const noexcept {
        return (min_y + max_y + 1) / 2.0F;
    }
};

[[nodiscard]] IconBounds icon_bounds(std::span<const std::uint8_t> data,
                                     int frame, int width,
                                     int height) noexcept;

// The managed PreviewCamera mod computes a camera basis from a position and
// look-at point before handing it to Scene. Keep the basis independent from a
// graphics API so room thumbnails, the Win32 preview, and future frontends
// can share the same fallback for a zero-length direction.
struct PreviewPose {
    formats::Vector3 position;
    formats::Vector3 target;
    formats::Vector3 facing{0.0F, 0.0F, 1.0F};
    formats::Vector3 right{1.0F, 0.0F, 0.0F};
    formats::Vector3 up{0.0F, 1.0F, 0.0F};
};

[[nodiscard]] PreviewPose preview_pose(formats::Vector3 position,
                                        formats::Vector3 target) noexcept;
[[nodiscard]] camera::CameraState preview_camera(
    formats::Vector3 position, formats::Vector3 target,
    float fov = 45.0F) noexcept;

} // namespace fruityprime::mods::render
