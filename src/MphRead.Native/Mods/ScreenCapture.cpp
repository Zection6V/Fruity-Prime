#include "Mods/screen_capture.hpp"

#include "Export/export.hpp"

#include <cstddef>
#include <limits>
#include <vector>

namespace fruityprime::mods::screen_capture {
namespace {

constexpr double MinLitFraction = 0.01;

[[nodiscard]] bool expected_rgb_size(int width, int height,
                                     std::size_t& result) noexcept {
    if (width <= 0 || height <= 0) {
        return false;
    }
    const auto width_size = static_cast<std::size_t>(width);
    const auto height_size = static_cast<std::size_t>(height);
    if (width_size > std::numeric_limits<std::size_t>::max() / 3
        || height_size > std::numeric_limits<std::size_t>::max()
            / (width_size * 3)) {
        return false;
    }
    result = width_size * height_size * 3;
    return true;
}

} // namespace

double lit_fraction(std::span<const std::uint8_t> rgb) noexcept {
    std::size_t total = 0;
    std::size_t lit = 0;
    for (std::size_t offset = 0; offset + 2 < rgb.size(); offset += 3) {
        ++total;
        if (rgb[offset] > 8 || rgb[offset + 1] > 8
            || rgb[offset + 2] > 8) {
            ++lit;
        }
    }
    return total == 0 ? 0.0 : static_cast<double>(lit) / total;
}

bool save_rgb(const std::filesystem::path& output, int width, int height,
              std::span<const std::uint8_t> bottom_up_rgb) {
    std::size_t expected = 0;
    if (output.empty() || !expected_rgb_size(width, height, expected)
        || bottom_up_rgb.size() != expected
        || lit_fraction(bottom_up_rgb) < MinLitFraction) {
        return false;
    }

    const auto width_size = static_cast<std::size_t>(width);
    const auto height_size = static_cast<std::size_t>(height);
    std::vector<std::uint8_t> rgba(expected / 3 * 4);
    for (std::size_t output_y = 0; output_y < height_size; ++output_y) {
        const std::size_t source_y = height_size - output_y - 1;
        for (std::size_t x = 0; x < width_size; ++x) {
            const std::size_t source = (source_y * width_size + x) * 3;
            const std::size_t destination =
                (output_y * width_size + x) * 4;
            rgba[destination] = bottom_up_rgb[source];
            rgba[destination + 1] = bottom_up_rgb[source + 1];
            rgba[destination + 2] = bottom_up_rgb[source + 2];
            rgba[destination + 3] = 0xff;
        }
    }

    try {
        exporter::write_png_rgba(output, width, height, rgba);
    } catch (...) {
        return false;
    }
    return true;
}

} // namespace fruityprime::mods::screen_capture
