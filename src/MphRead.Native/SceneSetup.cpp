#include "SceneSetup.hpp"

#include "Metadata/metadata.hpp"
#include "Metadata/Rooms.hpp"
#include "Entities/room_catalog.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <exception>

namespace fruityprime::scene_setup {
namespace {

[[nodiscard]] net::Vec3 to_net(formats::Vector3Fx value) noexcept {
    return {value.x.to_float(), value.y.to_float(), value.z.to_float()};
}

[[nodiscard]] bool story_room(std::int32_t room_id) noexcept {
    return room_id >= 27 && room_id <= 92;
}

[[nodiscard]] bool story_area(std::int32_t area_id) noexcept {
    return area_id >= 0 && area_id < 8;
}

[[nodiscard]] bool story_weapon_unlocked(
    const game::StorySave& save, std::int32_t beam_type) noexcept {
    return beam_type >= 0 && beam_type < 16
        && (save.weapons & (std::uint16_t{1} << beam_type)) != 0;
}

} // namespace

const scene::RoomCatalogEntry* find_room(std::string_view name) noexcept {
    return scene::find_room(name);
}

bool load_room(scene_runtime::Scene& destination, std::string_view name,
               gameplay::Config config) {
    return destination.load_room(name, config);
}

std::string resolve_node_data_path(std::string_view node_path,
                                   std::int32_t room_id, game::Mode mode,
                                   bool active_encounter_bot) {
    if (mode == game::Mode::SinglePlayer && active_encounter_bot) {
        if (const auto override =
                metadata::encounter_node_data_override(room_id)) {
            return std::string(*override);
        }
    } else if (mode == game::Mode::Capture) {
        if (const auto override = metadata::ctf_node_data_override(room_id)) {
            return std::string(*override);
        }
    } else if ((mode == game::Mode::Nodes
                || mode == game::Mode::NodesTeams
                || mode == game::Mode::Defender
                || mode == game::Mode::DefenderTeams)
               && room_id == 107) {
        return R"(levels\nodeData\mp14_KOTH_node.bin)";
    }
    return std::string(node_path);
}

std::vector<StoryHunterSpawner> collect_story_hunter_spawners(
    const scene::Room& room) {
    std::vector<StoryHunterSpawner> result;
    for (const auto& entity : room.entities()) {
        if (entity.kind != scene::EntityKind::EnemySpawn
            || entity.payload.size() < enemy_spawn::Data::Size) {
            continue;
        }
        enemy_spawn::Data data;
        try {
            data = enemy_spawn::decode(entity.payload);
        } catch (const std::exception&) {
            continue;
        }
        if (data.enemy_type != formats::EnemyType::Hunter) {
            continue;
        }
        result.push_back(StoryHunterSpawner{
            entity.entity_id,
            to_net(entity.position),
            to_net(entity.facing_vector),
            to_net(entity.up_vector),
            data.fields.hunter_id,
            data.fields.encounter_type,
            data.fields.hunter_weapon,
            data.fields.hunter_health,
            data.fields.hunter_health_max,
            data.fields.hunter_health_threshold,
            data.fields.hunter_color,
            data.fields.hunter_chance
        });
    }
    return result;
}

void update_area_hunters(game::State& state, game::StorySave& save,
                         utility::Rng& rng,
                         bool reset_completed_random_encounters) noexcept {
    if (reset_completed_random_encounters) {
        state.completed_random_encounter_rooms.fill(false);
    }
    save.area_hunters.fill(0);

    std::uint8_t chance = 0;
    std::array<std::uint8_t, 4> chances{};
    std::array<std::uint8_t, 4> counts{};
    for (std::size_t index = 0; index < chances.size(); ++index) {
        const auto area = static_cast<std::int32_t>(index * 2);
        if (state.area_state(area, &save) != game::AreaState::Clear) {
            continue;
        }
        const auto lost_octoliths = save.lost_octoliths;
        const auto first = (lost_octoliths >> (8 * index)) & 15u;
        const auto second = (lost_octoliths >> (8 * index + 4)) & 15u;
        chance = static_cast<std::uint8_t>(chance
            + ((first == 15u || second == 15u) ? 2u : 1u));
        chances[index] = chance;
    }

    for (std::size_t hunter = 0; hunter < metadata::HunterCount; ++hunter) {
        if ((save.defeated_hunters & (std::uint8_t{1} << hunter)) == 0) {
            continue;
        }
        const auto random = rng.random2(chance);
        for (std::size_t area = 0; area < chances.size(); ++area) {
            if (random >= chances[area]) {
                continue;
            }
            save.area_hunters[area] |= static_cast<std::uint8_t>(
                std::uint8_t{1} << hunter);
            if (++counts[area] >= 3) {
                for (std::size_t shifted = chances.size() - 1;
                     shifted > area; --shifted) {
                    chances[shifted] = chances[shifted - 1];
                }
                chances[area] = 0;
                chance = chances.back();
            }
            break;
        }
    }
}

StoryHunterPlan select_hunter_spawns(
    std::span<const StoryHunterSpawner> spawners, std::int32_t room_id,
    std::int32_t area_id, const game::State& state,
    const game::StorySave& save,
    const features::FeatureSettings& feature_settings,
    const features::CheatSettings& cheat_settings, utility::Rng& rng,
    metadata::Hunter local_hunter, std::uint8_t local_recolor) {
    StoryHunterPlan result;
    if (!story_area(area_id)) {
        return result;
    }

    // Data Shrine 02 stops offering the story hunter once Battlehammer has
    // been acquired. The save bit is a managed BeamType bit, not a native
    // weapon-table index.
    const bool skip_shrine = state.area_state(area_id, &save)
        == game::AreaState::Clear && room_id == 50
        && story_weapon_unlocked(save, 3);
    if (skip_shrine) {
        return result;
    }

    int random_hunters = save.area_hunters[static_cast<std::size_t>(area_id / 2)]
        & 0x7e;
    int random_hunter_count = std::popcount(
        static_cast<unsigned>(random_hunters));
    int extra_count = 0;
    for (const auto& spawner : spawners) {
        if (result.spawns.size() >= 7) {
            break;
        }
        if (spawner.hunter_id == static_cast<std::uint32_t>(
                metadata::Hunter::Random)
            && (cheat_settings.no_random_encounters
                || (feature_settings.no_repeat_encounters
                    && story_room(room_id)
                    && state.completed_random_encounter_rooms[
                        static_cast<std::size_t>(room_id - 27)]))) {
            result.blocked_by_completed_random_encounter = true;
            return result;
        }
        if (rng.random2(100) >= spawner.hunter_chance) {
            continue;
        }

        metadata::Hunter hunter = metadata::Hunter::Guardian;
        if (spawner.hunter_id == static_cast<std::uint32_t>(
                metadata::Hunter::Random)) {
            const int total = random_hunter_count + extra_count;
            const auto random = rng.random2(static_cast<std::uint32_t>(total));
            if (static_cast<int>(random) < random_hunter_count) {
                int ordinal = 0;
                int selected = static_cast<int>(metadata::Hunter::Guardian);
                for (int candidate = 0;
                     candidate < static_cast<int>(metadata::Hunter::Guardian);
                     ++candidate) {
                    if ((random_hunters & (1 << candidate)) != 0
                        && ordinal++ == static_cast<int>(random)) {
                        selected = candidate;
                        break;
                    }
                }
                hunter = static_cast<metadata::Hunter>(selected);
            }
        } else if (spawner.hunter_id < metadata::HunterCount) {
            hunter = static_cast<metadata::Hunter>(spawner.hunter_id);
        } else {
            continue;
        }

        if (hunter != metadata::Hunter::Guardian) {
            extra_count = 1;
        }
        const auto hunter_bit = static_cast<int>(hunter);
        if ((random_hunters & (1 << hunter_bit)) != 0) {
            random_hunters &= ~(1 << hunter_bit);
            --random_hunter_count;
        }

        std::uint8_t color = spawner.hunter_color;
        if (hunter == local_hunter && color == local_recolor
            && feature_settings.alternate_hunters_1p) {
            color = local_recolor == 0 ? 1 : 0;
        }
        result.spawns.push_back(StoryHunterSpawn{
            spawner.entity_id,
            spawner.position,
            spawner.facing,
            spawner.up,
            hunter,
            color,
            spawner.encounter_type,
            spawner.hunter_weapon,
            spawner.hunter_health,
            spawner.hunter_health_max,
            spawner.hunter_health_threshold
        });
    }
    return result;
}

} // namespace fruityprime::scene_setup
