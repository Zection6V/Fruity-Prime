#pragma once

#include "Mods/MapGen/mapgen.hpp"
#include "Mods/MapGen/q3_import.hpp"

namespace fruityprime::mapgen::packer {

[[nodiscard]] std::vector<std::uint8_t> build_model(
    const MapDefinition& definition, const std::vector<BuiltFace>& faces,
    const std::vector<detail::TexturePackEntry>* texture_pack,
    BuildStats& stats);

[[nodiscard]] std::vector<std::uint8_t> build_entities(
    const MapDefinition& definition, BuildStats& stats);

void write_generated(const MapDefinition& definition,
                     const GeneratedMap& generated,
                     const std::filesystem::path& output_directory);

} // namespace fruityprime::mapgen::packer
