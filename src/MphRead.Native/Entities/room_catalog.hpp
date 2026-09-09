#pragma once

#include "Entities/scene.hpp"

#include <string>
#include <string_view>
#include <vector>

namespace fruityprime::scene {

struct RoomCatalogEntry {
    int id = -1;
    std::string name;
    std::string in_game_name;
    RoomDefinition definition;
};

// Standard room tables copied from the current C# metadata. The asset paths
// use the ROM filesystem layout, so the same definitions work with
// Store::from_rom and Store::from_directory.
[[nodiscard]] const std::vector<RoomCatalogEntry>& story_rooms();
[[nodiscard]] const std::vector<RoomCatalogEntry>& multiplayer_rooms();

[[nodiscard]] const RoomCatalogEntry* find_room(
    std::string_view name) noexcept;

[[nodiscard]] const RoomCatalogEntry* find_multiplayer_room(
    std::string_view name) noexcept;

} // namespace fruityprime::scene
