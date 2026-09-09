#pragma once

#include "Mods/MapGen/mapgen.hpp"

#include <span>

namespace fruityprime::mapgen::repack {

// Native equivalent of Mods/MapGen/RepackAccess.cs.  MapDefinition and
// BuiltFace are the native editor-side representations; keeping this adapter
// public lets map importers and tests reach the same private packers without
// duplicating their binary-writing logic.
[[nodiscard]] std::vector<std::uint8_t> pack_entities(
    const MapDefinition& definition, BuildStats& stats);

[[nodiscard]] std::vector<std::uint8_t> pack_mph_collision(
    std::span<const BuiltFace> faces, BuildStats& stats);

} // namespace fruityprime::mapgen::repack
