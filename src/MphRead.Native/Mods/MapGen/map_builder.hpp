#pragma once

#include "Mods/MapGen/mapgen.hpp"

namespace fruityprime::mapgen::builder {

[[nodiscard]] std::vector<BuiltFace> make_faces(
    const MapDefinition& definition, bool solid_only);

[[nodiscard]] BuiltMap build(const MapDefinition& definition);

} // namespace fruityprime::mapgen::builder
