#pragma once

#include "Mods/MapGen/mapgen.hpp"

#include <cstddef>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace fruityprime::mapgen::custom_rooms {

// Custom map recipes are discovered once, after the caller has selected the
// map directory. Bundles win over a loose JSON recipe with the same filename
// stem, matching the managed CustomRooms.MapFiles contract.
void set_map_directory(const std::filesystem::path& path);

[[nodiscard]] const std::filesystem::path& map_directory() noexcept;

[[nodiscard]] const std::vector<MapDefinition>& definitions();

// Generated files live outside the source tree. The path is also carried by
// the room catalog so the runtime can load a custom room without pretending
// its five files are a cartridge archive.
[[nodiscard]] std::filesystem::path generated_directory(
    const MapDefinition& definition);

[[nodiscard]] std::size_t generate_all(bool force = false,
                                       bool verbose = true);

[[nodiscard]] std::size_t generate_missing(bool verbose = false);

[[nodiscard]] std::optional<std::string> why_unplayable(
    std::string_view room_name);

} // namespace fruityprime::mapgen::custom_rooms

