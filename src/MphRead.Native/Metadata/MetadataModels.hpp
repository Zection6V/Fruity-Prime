#pragma once

// Complete native counterpart of Metadata.cs ModelMetadata and
// FirstHuntModels.  The records are generated from the managed constructors;
// do not replace them with a reduced name-only catalogue.

#include "MetadataClasses.hpp"

#include <array>
#include <cstddef>
#include <span>
#include <string_view>

namespace fruityprime::metadata {

struct MetadataModelEntry {
    std::string_view Key;
    ModelMetadata Value;
};

inline constexpr std::size_t ModelMetadataCount = 253;
inline constexpr std::size_t FirstHuntModelsCount = 62;

extern const ModelMetadata DoubleDamageImg;
extern const std::array<MetadataModelEntry, ModelMetadataCount> ModelMetadataTable;
extern const std::array<MetadataModelEntry, FirstHuntModelsCount> FirstHuntModels;

[[nodiscard]] const ModelMetadata* get_model_by_name(
    std::string_view name, MetaDir dir = MetaDir::Models) noexcept;
[[nodiscard]] const ModelMetadata* get_first_hunt_model_by_name(
    std::string_view name) noexcept;
[[nodiscard]] const ModelMetadata* get_entity_by_path(
    std::string_view path) noexcept;

} // namespace fruityprime::metadata
