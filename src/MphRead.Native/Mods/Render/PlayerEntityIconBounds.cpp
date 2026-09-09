#include "render_mods.hpp"

#include <algorithm>

namespace fruityprime::mods::render {

IconBounds icon_bounds(std::span<const std::uint8_t> data, int frame,
                       int width, int height) noexcept {
    if (frame < 0 || width <= 0 || height <= 0) {
        return {};
    }
    const std::size_t tiles_x = static_cast<std::size_t>((width + 7) / 8);
    const std::size_t image = static_cast<std::size_t>(frame)
        * static_cast<std::size_t>(width)
        * static_cast<std::size_t>(height);
    IconBounds result{width, height, -1, -1};
    for (int y = 0; y < height; ++y) {
        const std::size_t tile_y = static_cast<std::size_t>(y / 8);
        const std::size_t pixel_y = static_cast<std::size_t>(y % 8);
        for (int x = 0; x < width; ++x) {
            const std::size_t index = image
                + tile_y * tiles_x * 64
                + static_cast<std::size_t>(x / 8) * 64
                + pixel_y * 8
                + static_cast<std::size_t>(x % 8);
            if (index >= data.size() || data[index] == 0) {
                continue;
            }
            result.min_x = std::min(result.min_x, x);
            result.min_y = std::min(result.min_y, y);
            result.max_x = std::max(result.max_x, x);
            result.max_y = std::max(result.max_y, y);
        }
    }
    if (result.max_x < result.min_x || result.max_y < result.min_y) {
        return {0, 0, width - 1, height - 1};
    }
    return result;
}

} // namespace fruityprime::mods::render
