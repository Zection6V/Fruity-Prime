#pragma once

#include "q3_import.hpp"

namespace fruityprime::mapgen::texture_pack_io {

[[nodiscard]] std::vector<detail::TexturePackEntry> load(
    const MapDefinition& definition);

} // namespace fruityprime::mapgen::texture_pack_io
