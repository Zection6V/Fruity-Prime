#include "Export/export.hpp"

#include <cmath>
#include <fstream>
#include <limits>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

#include <zlib.h>

namespace fruityprime::exporter {
namespace {

void append_u32_be(std::vector<std::uint8_t>& bytes, std::uint32_t value) {
    bytes.push_back(static_cast<std::uint8_t>(value >> 24));
    bytes.push_back(static_cast<std::uint8_t>(value >> 16));
    bytes.push_back(static_cast<std::uint8_t>(value >> 8));
    bytes.push_back(static_cast<std::uint8_t>(value));
}

void append_png_chunk(std::vector<std::uint8_t>& png, const char type[4],
                      std::span<const std::uint8_t> data) {
    if (data.size() > std::numeric_limits<std::uint32_t>::max()) {
        throw std::runtime_error("PNG chunk is too large");
    }
    append_u32_be(png, static_cast<std::uint32_t>(data.size()));
    const auto type_bytes = std::span<const std::uint8_t>(
        reinterpret_cast<const std::uint8_t*>(type), 4);
    png.insert(png.end(), type_bytes.begin(), type_bytes.end());
    png.insert(png.end(), data.begin(), data.end());
    uLong crc = crc32(0L, Z_NULL, 0);
    crc = crc32(crc, type_bytes.data(), static_cast<uInt>(type_bytes.size()));
    if (!data.empty()) {
        crc = crc32(crc, data.data(), static_cast<uInt>(data.size()));
    }
    append_u32_be(png, static_cast<std::uint32_t>(crc));
}

[[nodiscard]] std::uint8_t expand_channel(std::uint16_t color,
                                           int shift) noexcept {
    return static_cast<std::uint8_t>(std::lround(
        static_cast<float>((color >> shift) & 0x1f) / 31.0F * 255.0F));
}

[[nodiscard]] std::string texture_file_name(int texture_id, int palette_id) {
    return "texture-" + std::to_string(texture_id) + '-'
        + (palette_id < 0 ? "direct" : "p" + std::to_string(palette_id))
        + ".png";
}

} // namespace

void write_png_rgba(const std::filesystem::path& output_path, int width,
                    int height, std::span<const std::uint8_t> rgba) {
    if (output_path.empty() || width <= 0 || height <= 0) {
        throw std::invalid_argument("PNG output or dimensions are invalid");
    }
    const auto width_size = static_cast<std::size_t>(width);
    const auto height_size = static_cast<std::size_t>(height);
    if (width_size > std::numeric_limits<std::size_t>::max() / 4
        || height_size > std::numeric_limits<std::size_t>::max()
            / (width_size * 4)) {
        throw std::invalid_argument("PNG dimensions are too large");
    }
    const std::size_t row_bytes = width_size * 4;
    const std::size_t pixel_bytes = row_bytes * height_size;
    if (rgba.size() != pixel_bytes) {
        throw std::invalid_argument("PNG RGBA buffer has the wrong size");
    }
    if (static_cast<std::uintmax_t>(width)
            > std::numeric_limits<std::uint32_t>::max()
        || static_cast<std::uintmax_t>(height)
            > std::numeric_limits<std::uint32_t>::max()) {
        throw std::invalid_argument("PNG dimensions do not fit the format");
    }

    std::vector<std::uint8_t> raw;
    raw.reserve(height_size * (row_bytes + 1));
    for (std::size_t y = 0; y < height_size; ++y) {
        raw.push_back(0); // PNG filter: None
        const auto row = rgba.subspan(y * row_bytes, row_bytes);
        raw.insert(raw.end(), row.begin(), row.end());
    }
    uLongf compressed_size = compressBound(static_cast<uLong>(raw.size()));
    std::vector<std::uint8_t> compressed(compressed_size);
    const int result = compress2(compressed.data(), &compressed_size, raw.data(),
                                 static_cast<uLong>(raw.size()), Z_BEST_SPEED);
    if (result != Z_OK) {
        throw std::runtime_error("zlib could not compress PNG pixels");
    }
    compressed.resize(static_cast<std::size_t>(compressed_size));

    std::vector<std::uint8_t> png{
        0x89, 'P', 'N', 'G', 0x0d, 0x0a, 0x1a, 0x0a
    };
    std::vector<std::uint8_t> header;
    header.reserve(13);
    append_u32_be(header, static_cast<std::uint32_t>(width));
    append_u32_be(header, static_cast<std::uint32_t>(height));
    header.insert(header.end(), {8, 6, 0, 0, 0}); // RGBA8, no interlace
    append_png_chunk(png, "IHDR", header);
    append_png_chunk(png, "IDAT", compressed);
    append_png_chunk(png, "IEND", {});

    if (output_path.has_parent_path()) {
        std::error_code error;
        std::filesystem::create_directories(output_path.parent_path(), error);
        if (error) {
            throw std::runtime_error("could not create PNG output directory: "
                                     + error.message());
        }
    }
    std::ofstream output(output_path, std::ios::binary | std::ios::trunc);
    if (!output) {
        throw std::runtime_error("could not create PNG file: "
                                 + output_path.string());
    }
    output.write(reinterpret_cast<const char*>(png.data()),
                 static_cast<std::streamsize>(png.size()));
    if (!output) {
        throw std::runtime_error("could not write PNG file: "
                                 + output_path.string());
    }
}

TextureExportStats write_model_textures(
    const model::File& model, const std::filesystem::path& output_directory) {
    if (output_directory.empty()) {
        throw std::invalid_argument("texture output directory is empty");
    }
    std::error_code error;
    std::filesystem::create_directories(output_directory, error);
    if (error) {
        throw std::runtime_error("could not create texture output directory: "
                                 + error.message());
    }

    std::set<std::pair<int, int>> pairs;
    for (const auto& material : model.materials()) {
        if (material.texture_id < 0
            || static_cast<std::size_t>(material.texture_id)
                >= model.textures().size()) {
            continue;
        }
        const auto& texture = model.textures()[material.texture_id];
        if (texture.format == 5) {
            pairs.emplace(material.texture_id, -1);
        } else if (material.palette_id >= 0
                   && static_cast<std::size_t>(material.palette_id)
                       < model.palettes().size()) {
            pairs.emplace(material.texture_id, material.palette_id);
        }
    }
    TextureExportStats stats;
    for (const auto& [texture_id, palette_id] : pairs) {
        const auto& texture = model.textures()[static_cast<std::size_t>(
            texture_id)];
        const std::size_t expected = static_cast<std::size_t>(texture.width)
            * static_cast<std::size_t>(texture.height);
        const auto pixels = model.decode_texture(
            static_cast<std::size_t>(texture_id));
        if (pixels.size() != expected) {
            throw std::runtime_error("decoded model texture has the wrong size");
        }
        std::vector<std::uint16_t> palette;
        if (palette_id >= 0) {
            palette = model.decode_palette(static_cast<std::size_t>(palette_id));
        }
        std::vector<std::uint8_t> rgba(expected * 4, 0);
        for (std::size_t i = 0; i < pixels.size(); ++i) {
            std::uint16_t color = 0x7fff;
            if (palette_id >= 0) {
                if (pixels[i].data >= palette.size()) {
                    throw std::runtime_error(
                        "indexed model texture references a missing palette color");
                }
                color = palette[pixels[i].data];
            } else {
                color = static_cast<std::uint16_t>(pixels[i].data);
            }
            rgba[i * 4 + 0] = expand_channel(color, 0);
            rgba[i * 4 + 1] = expand_channel(color, 5);
            rgba[i * 4 + 2] = expand_channel(color, 10);
            rgba[i * 4 + 3] = pixels[i].alpha;
        }
        write_png_rgba(output_directory / texture_file_name(
                           texture_id, palette_id), texture.width,
                       texture.height, rgba);
        ++stats.images;
        stats.pixels += expected;
    }
    return stats;
}

} // namespace fruityprime::exporter
