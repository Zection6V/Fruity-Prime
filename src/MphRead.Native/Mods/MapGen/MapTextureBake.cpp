#include "map_texture_bake.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <map>
#include <stdexcept>
#include <string>

namespace fruityprime::mapgen::texture_bake {
namespace {

struct QuantizationBox {
    std::size_t start = 0;
    std::size_t length = 0;
};

} // namespace

std::vector<std::uint8_t> downsample_rgb(
    const detail::image::RgbImage& source, int size,
    std::string_view source_name) {
    if (size <= 0 || source.width <= 0 || source.height <= 0
        || source.pixels.size() != static_cast<std::size_t>(source.width)
            * static_cast<std::size_t>(source.height) * 3) {
        throw std::runtime_error("decoded image dimensions are invalid: "
                                 + std::string(source_name));
    }
    const std::size_t output_size = static_cast<std::size_t>(size) * size * 3;
    std::vector<std::uint8_t> result(output_size);
    for (int y = 0; y < size; ++y) {
        const int y0 = y * source.height / size;
        const int y1 = std::max(y0 + 1, (y + 1) * source.height / size);
        for (int x = 0; x < size; ++x) {
            const int x0 = x * source.width / size;
            const int x1 = std::max(x0 + 1, (x + 1) * source.width / size);
            std::uint64_t red = 0;
            std::uint64_t green = 0;
            std::uint64_t blue = 0;
            std::uint64_t count = 0;
            for (int sy = y0; sy < y1 && sy < source.height; ++sy) {
                for (int sx = x0; sx < x1 && sx < source.width; ++sx) {
                    const std::size_t offset =
                        (static_cast<std::size_t>(sy) * source.width + sx) * 3;
                    red += source.pixels[offset + 0];
                    green += source.pixels[offset + 1];
                    blue += source.pixels[offset + 2];
                    ++count;
                }
            }
            const std::size_t destination =
                (static_cast<std::size_t>(y) * size + x) * 3;
            const std::uint64_t divisor = std::max<std::uint64_t>(1, count);
            result[destination + 0] = static_cast<std::uint8_t>(red / divisor);
            result[destination + 1] = static_cast<std::uint8_t>(green / divisor);
            result[destination + 2] = static_cast<std::uint8_t>(blue / divisor);
        }
    }
    return result;
}

std::pair<std::vector<std::uint16_t>, std::vector<std::uint8_t>>
quantize_rgb(const std::vector<std::uint8_t>& rgb, int size) {
    const std::size_t count = static_cast<std::size_t>(size) * size;
    if (rgb.size() != count * 3) {
        throw std::runtime_error("texture quantizer input has invalid dimensions");
    }
    std::vector<std::size_t> indices(count);
    for (std::size_t i = 0; i < count; ++i) {
        indices[i] = i;
    }
    std::vector<QuantizationBox> boxes{{0, count}};
    while (boxes.size() < 256) {
        int widest = -1;
        int widest_spread = 0;
        int widest_channel = 0;
        for (std::size_t box_index = 0; box_index < boxes.size(); ++box_index) {
            const QuantizationBox box = boxes[box_index];
            if (box.length < 2) {
                continue;
            }
            for (int channel = 0; channel < 3; ++channel) {
                int low = 255;
                int high = 0;
                for (std::size_t i = box.start; i < box.start + box.length; ++i) {
                    const int value = rgb[indices[i] * 3 + channel];
                    low = std::min(low, value);
                    high = std::max(high, value);
                }
                if (high - low > widest_spread) {
                    widest_spread = high - low;
                    widest = static_cast<int>(box_index);
                    widest_channel = channel;
                }
            }
        }
        if (widest < 0 || widest_spread == 0) {
            break;
        }
        const QuantizationBox box = boxes[static_cast<std::size_t>(widest)];
        std::sort(indices.begin() + static_cast<std::ptrdiff_t>(box.start),
                  indices.begin() + static_cast<std::ptrdiff_t>(
                      box.start + box.length),
                  [&](std::size_t left, std::size_t right) {
                      return rgb[left * 3 + widest_channel]
                          < rgb[right * 3 + widest_channel];
                  });
        const std::size_t half = box.length / 2;
        boxes[static_cast<std::size_t>(widest)] = {box.start, half};
        boxes.push_back({box.start + half, box.length - half});
    }
    std::vector<std::uint16_t> palette(std::max<std::size_t>(1, boxes.size()));
    std::vector<std::uint8_t> lookup(count);
    for (std::size_t box_index = 0; box_index < boxes.size(); ++box_index) {
        const QuantizationBox box = boxes[box_index];
        std::uint64_t red = 0;
        std::uint64_t green = 0;
        std::uint64_t blue = 0;
        for (std::size_t i = box.start; i < box.start + box.length; ++i) {
            red += rgb[indices[i] * 3 + 0];
            green += rgb[indices[i] * 3 + 1];
            blue += rgb[indices[i] * 3 + 2];
        }
        const std::uint64_t divisor = std::max<std::size_t>(1, box.length);
        const std::uint16_t r = static_cast<std::uint16_t>((red / divisor) >> 3);
        const std::uint16_t g = static_cast<std::uint16_t>((green / divisor) >> 3);
        const std::uint16_t b = static_cast<std::uint16_t>((blue / divisor) >> 3);
        palette[box_index] = static_cast<std::uint16_t>(
            (b << 10) | (g << 5) | r);
        for (std::size_t i = box.start; i < box.start + box.length; ++i) {
            lookup[indices[i]] = static_cast<std::uint8_t>(box_index);
        }
    }
    return {std::move(palette), std::move(lookup)};
}

BakeResult bake_q3_textures_with_report(
    const detail::Q3Bsp& bsp, const MapDefinition& definition, int texture_size,
    const ImageLoader& load_image) {
    std::map<int, bool> seen;
    std::vector<std::pair<int, std::string>> used;
    for (const detail::Q3Face& face : bsp.faces) {
        if (face.type != 1 && face.type != 2 && face.type != 3) {
            continue;
        }
        if (face.texture < 0
            || static_cast<std::size_t>(face.texture) >= bsp.textures.size()) {
            throw std::runtime_error("Q3 face texture index is outside the lump");
        }
        const detail::Q3Texture& texture = bsp.textures[
            static_cast<std::size_t>(face.texture)];
        if ((texture.flags & (0x80 | 0x100 | 0x200)) != 0
            || ((texture.flags & 0x4) != 0 && !definition.import_keep_sky)
            || !seen.emplace(face.texture, true).second) {
            continue;
        }
        used.emplace_back(face.texture, texture.name);
    }
    std::sort(used.begin(), used.end(),
              [](const auto& left, const auto& right) {
                  return left.first < right.first;
              });
    if (texture_size <= 0
        || texture_size > std::numeric_limits<std::uint16_t>::max()) {
        throw std::invalid_argument(
            "Q3 texture size must be between 1 and 65535");
    }
    if (!load_image) {
        throw std::invalid_argument("Q3 texture image loader is empty");
    }
    BakeResult result;
    for (const auto& [source_index, name] : used) {
        const std::optional<detail::image::RgbImage> decoded = load_image(name);
        if (!decoded.has_value()) {
            result.missing.push_back(name);
            continue;
        }
        const auto reduced = downsample_rgb(*decoded, texture_size, name);
        auto [palette, pixels] = quantize_rgb(reduced, texture_size);
        detail::TexturePackEntry entry;
        entry.source_index = static_cast<std::uint16_t>(source_index);
        entry.name = name;
        entry.width = static_cast<std::uint16_t>(texture_size);
        entry.height = static_cast<std::uint16_t>(texture_size);
        entry.palette = std::move(palette);
        entry.pixels = std::move(pixels);
        result.entries.push_back(std::move(entry));
    }
    return result;
}

std::vector<detail::TexturePackEntry> bake_q3_textures(
    const detail::Q3Bsp& bsp, const MapDefinition& definition, int texture_size,
    const ImageLoader& load_image) {
    return bake_q3_textures_with_report(
        bsp, definition, texture_size, load_image).entries;
}

} // namespace fruityprime::mapgen::texture_bake
