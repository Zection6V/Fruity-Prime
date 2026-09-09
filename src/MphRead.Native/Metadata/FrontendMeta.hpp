#pragma once

// Direct native counterpart of Metadata/FrontendMeta.cs.  The model records
// below are generated from that source; keep the generator and source in sync.

#include "MetadataClasses.hpp"
#include "Metadata/metadata_extra.hpp"
#include "Metadata/movie_files.hpp"

#include <array>
#include <cstddef>
#include <span>
#include <string_view>

namespace fruityprime::metadata {

struct FrontendModelEntry {
    std::string_view Key;
    ModelMetadata Value;
};

extern const ModelMetadata Ad2Dm2;

inline constexpr std::size_t HudModelsCount = 20;
extern const std::array<FrontendModelEntry, HudModelsCount> HudModels;

inline constexpr std::size_t TouchToStartModelsCount = 1;
extern const std::array<FrontendModelEntry, TouchToStartModelsCount> TouchToStartModels;

inline constexpr std::size_t MultiplayerModelsCount = 14;
extern const std::array<FrontendModelEntry, MultiplayerModelsCount> MultiplayerModels;

inline constexpr std::size_t LogoModelsCount = 11;
extern const std::array<FrontendModelEntry, LogoModelsCount> LogoModels;

inline constexpr std::size_t FrontendModelsCount = 244;
extern const std::array<FrontendModelEntry, FrontendModelsCount> FrontendModels;

[[nodiscard]] const ModelMetadata* find_hud_model(std::string_view name) noexcept;
[[nodiscard]] const ModelMetadata* find_touchtostart_model(std::string_view name) noexcept;
[[nodiscard]] const ModelMetadata* find_multiplayer_model(std::string_view name) noexcept;
[[nodiscard]] const ModelMetadata* find_logo_model(std::string_view name) noexcept;
[[nodiscard]] const ModelMetadata* find_frontend_model(std::string_view name) noexcept;

} // namespace fruityprime::metadata
