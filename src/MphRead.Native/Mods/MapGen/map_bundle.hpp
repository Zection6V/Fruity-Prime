#pragma once

#include "Mods/MapGen/mapgen.hpp"

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace fruityprime::mapgen::bundle {

inline constexpr std::string_view Extension = ".fpmap";

[[nodiscard]] bool is_bundle(const std::filesystem::path& path) noexcept;

[[nodiscard]] std::optional<std::string> read_recipe(
    const std::filesystem::path& bundle_path);

[[nodiscard]] std::optional<std::vector<std::uint8_t>> read_entry(
    const std::filesystem::path& bundle_path, std::string_view name);

// Cook the source recipe, its trimmed Q3 level, and its FPTX texture pack
// into one portable .fpmap archive.
[[nodiscard]] std::filesystem::path cook(
    const MapDefinition& definition,
    const std::filesystem::path& recipe_path,
    const std::filesystem::path& output_path = {}, bool verbose = true);

} // namespace fruityprime::mapgen::bundle
