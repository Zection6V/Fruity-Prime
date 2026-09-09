#pragma once

#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

namespace fruityprime::mapgen::detail::image {

struct RgbImage {
    int width = 0;
    int height = 0;
    std::vector<std::uint8_t> pixels;
};

// Decode one image from a Q3 archive. Windows uses WIC so the native MinGW
// build accepts the same JPG/PNG/TGA assets as the managed STB path. The
// portable fallback intentionally stays dependency-free and handles TGA/PPM,
// which is enough for generated fixtures and non-Windows tooling.
[[nodiscard]] RgbImage decode(std::span<const std::uint8_t> bytes,
                              std::string_view source_name);

} // namespace fruityprime::mapgen::detail::image
