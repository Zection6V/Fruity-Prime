#pragma once

#include "q3_import.hpp"

#include <optional>

namespace fruityprime::mapgen::texture_pack_io {

// nullopt means no pack was resolved; an engaged empty vector means an FPTX
// file was resolved and contained zero entries. MapImport treats those two
// states differently (the latter must not fall back to baked/source textures).
[[nodiscard]] std::optional<std::vector<detail::TexturePackEntry>>
load_optional(const MapDefinition& definition);

[[nodiscard]] std::vector<detail::TexturePackEntry> load(
    const MapDefinition& definition);

} // namespace fruityprime::mapgen::texture_pack_io
