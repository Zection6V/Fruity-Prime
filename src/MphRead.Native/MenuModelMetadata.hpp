#pragma once

#include "Formats/Types.hpp"

#include <cstdint>
#include <span>
#include <string_view>

namespace fruityprime::menu {

// The set names are the branches in Metadata.GetModelByName.  MetaDir values
// other than the four special frontend sets use Frontend, exactly as the C#
// implementation's `dir != MetaDir.Models` branch does.
enum class ModelMetadataSet : std::uint8_t {
    Models,
    FirstHunt,
    Hud,
    TouchToStart,
    Multiplayer,
    Logo,
    Frontend,
    Special,
};

struct MenuModelMetadata {
    std::string_view name;
    ModelMetadataSet set = ModelMetadataSet::Models;
    std::uint8_t recolor_count = 1;
};

[[nodiscard]] std::span<const MenuModelMetadata> menu_model_metadata() noexcept;

} // namespace fruityprime::menu
