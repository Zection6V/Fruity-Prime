#pragma once

#include "Formats/enemy_spawn.hpp"
#include "Features.hpp"
#include "GameState.hpp"
#include "Utility/rng.hpp"
#include "Scene.hpp"
#include "Entities/room_catalog.hpp"

#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace fruityprime::scene_setup {

[[nodiscard]] const scene::RoomCatalogEntry* find_room(
    std::string_view name) noexcept;
[[nodiscard]] bool load_room(scene_runtime::Scene& destination,
                             std::string_view name,
                             gameplay::Config config = {});

// SceneSetup.LoadNodeData's mode-dependent path selection, separated from
// file I/O so all hosts use the exact same cartridge override rules.
[[nodiscard]] std::string resolve_node_data_path(
    std::string_view node_path, std::int32_t room_id, game::Mode mode,
    bool active_encounter_bot = false);

// The managed SceneSetup hunter path reads these eight fields from the S09
// enemy-spawn union before it creates a temporary story player.  Keep a small
// value type here so selection can be tested without constructing a renderer
// or a complete Session.
struct StoryHunterSpawner {
    std::int16_t entity_id = -1;
    net::Vec3 position{};
    net::Vec3 facing{0.0F, 0.0F, 1.0F};
    net::Vec3 up{0.0F, 1.0F, 0.0F};
    std::uint32_t hunter_id = 8;
    std::uint32_t encounter_type = 0;
    std::uint32_t hunter_weapon = 255;
    std::uint16_t hunter_health = 0;
    std::uint16_t hunter_health_max = 0;
    std::uint16_t hunter_health_threshold = 0;
    std::uint8_t hunter_color = 0;
    std::uint8_t hunter_chance = 0;
};

struct StoryHunterSpawn {
    std::int16_t spawner_entity_id = -1;
    net::Vec3 position{};
    net::Vec3 facing{0.0F, 0.0F, 1.0F};
    net::Vec3 up{0.0F, 1.0F, 0.0F};
    metadata::Hunter hunter = metadata::Hunter::Guardian;
    std::uint8_t color = 0;
    std::uint32_t encounter_type = 0;
    std::uint32_t hunter_weapon = 255;
    std::uint16_t health = 0;
    std::uint16_t health_max = 0;
    std::uint16_t health_threshold = 0;
};

struct StoryHunterPlan {
    std::vector<StoryHunterSpawn> spawns;
    bool blocked_by_completed_random_encounter = false;
};

// Decode the story Hunter (S09) spawners from a loaded room. Malformed or
// non-Hunter records are ignored, matching the runtime's defensive room
// loading boundary.
[[nodiscard]] std::vector<StoryHunterSpawner> collect_story_hunter_spawners(
    const scene::Room& room);

// SceneSetup.UpdateAreaHunters equivalent. Passing a save explicitly mirrors
// the managed overload: the save is mutated, while the process RNG is supplied
// by the caller so tests and replay tools can isolate the sequence.
void update_area_hunters(
    game::State& state, game::StorySave& save, utility::Rng& rng,
    bool reset_completed_random_encounters = false) noexcept;

// SceneSetup.InitHunterSpawns selection boundary. It returns the bot plan;
// frontends decide how to materialize the selected players and artifacts.
[[nodiscard]] StoryHunterPlan select_hunter_spawns(
    std::span<const StoryHunterSpawner> spawners,
    std::int32_t room_id, std::int32_t area_id, const game::State& state,
    const game::StorySave& save, const features::FeatureSettings& feature_settings,
    const features::CheatSettings& cheat_settings, utility::Rng& rng,
    metadata::Hunter local_hunter, std::uint8_t local_recolor = 0);

} // namespace fruityprime::scene_setup

namespace MphReadNative {
namespace SceneSetup = ::fruityprime::scene_setup;
}
