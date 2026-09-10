#pragma once

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

} // namespace fruityprime::mods::render
