#pragma once

#include "image_decode.hpp"
#include "q3_bsp.hpp"
#include "q3_import.hpp"

#include <cstdint>
#include <functional>
#include <optional>
#include <string_view>
#include <utility>
#include <vector>

namespace fruityprime::mapgen::texture_bake {

[[nodiscard]] std::vector<std::uint8_t> downsample_rgb(
    const detail::image::RgbImage& source, int size,
    std::string_view source_name);

[[nodiscard]] std::pair<std::vector<std::uint16_t>, std::vector<std::uint8_t>>
quantize_rgb(const std::vector<std::uint8_t>& rgb, int size);

using ImageLoader = std::function<std::optional<detail::image::RgbImage>(
    std::string_view)>;

struct BakeResult {
    std::vector<detail::TexturePackEntry> entries;
    std::vector<std::string> missing;
};

// The managed Result exposes both the entries that were written and the
// shader names whose images were not found.  Keep that report available to
// command code while retaining the old entry-only helper for the importer.
[[nodiscard]] BakeResult bake_q3_textures_with_report(
    const detail::Q3Bsp& bsp, const MapDefinition& definition, int texture_size,
    const ImageLoader& load_image);

// Select the renderable Q3 textures and perform the managed-compatible
// downsample/quantize step. Archive lookup and decompression stay with the
// importer; this module owns the texture-bake policy and output records.
[[nodiscard]] std::vector<detail::TexturePackEntry> bake_q3_textures(
    const detail::Q3Bsp& bsp, const MapDefinition& definition, int texture_size,
    const ImageLoader& load_image);

} // namespace fruityprime::mapgen::texture_bake
