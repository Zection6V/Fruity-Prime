#include "ResourceAudit.hpp"

#include "MapResourceRules.hpp"
#include "MatchWorldProfile.hpp"

#include "../ThumbnailGenerator.hpp"
#include "../../Metadata/Metadata.hpp"
#include "../../Metadata/Rooms.hpp"
#include "../../NativeRuntime/OpenTK/Mathematics.hpp"
#include "../../NativeRuntime/System/Console.hpp"
#include "../../NativeRuntime/System/Cryptography.hpp"
#include "../../NativeRuntime/System/Number.hpp"
#include "../../Program.hpp"
#include "../../Read.hpp"

#include <algorithm>
#include <array>
#include <cstring>
#include <exception>
#include <iostream>
#include <limits>
#include <memory>
#include <set>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace MphRead::Mods::Multiplayer
{
    namespace
    {
        using ItemSpawn = ::MphRead::EntityOf<::MphRead::ItemSpawnEntityData>;
        using PlayerSpawn = ::MphRead::EntityOf<::MphRead::PlayerSpawnEntityData>;
        using EntityList = MapResourceRules::EntityList;
        using OpenTK::Mathematics::Vector3;

        struct Scenario final
        {
            std::string_view Label;
            ::MphRead::GameMode Mode;
            std::int32_t Players;
        };

        [[nodiscard]] std::vector<std::shared_ptr<ItemSpawn>> Health(const EntityList& entities)
        {
            std::vector<std::shared_ptr<ItemSpawn>> result;
            for (const std::shared_ptr<::MphRead::Entity>& entity : *entities)
            {
                auto item = std::dynamic_pointer_cast<ItemSpawn>(entity);
                if (item != nullptr && item->Data.Enabled != 0
                    && MapResourceRules::IsHealth(item->Data.ItemType))
                {
                    result.push_back(std::move(item));
                }
            }
            return result;
        }

        [[nodiscard]] std::string Distance(
            const std::vector<Vector3>& from, const std::vector<std::shared_ptr<ItemSpawn>>& health)
        {
            if (health.empty())
            {
                return "n/a";
            }
            float max = 0.0F;
            bool any = false;
            for (const Vector3& position : from)
            {
                any = true;
                float nearest = std::numeric_limits<float>::max();
                for (const std::shared_ptr<ItemSpawn>& item : health)
                {
                    nearest = std::min(nearest,
                        OpenTK::Mathematics::Length(item->Position - position));
                }
                max = std::max(max, nearest);
            }
            return any ? ::MphRead::NativeRuntime::ToStringInvariant(max, "F2") : "n/a";
        }

        [[nodiscard]] std::string Fingerprint(const ::MphRead::RoomMetadata& room,
            ResourceSpawnProfile profile, const std::vector<std::shared_ptr<ItemSpawn>>& health)
        {
            ::MphRead::NativeRuntime::IncrementalSha256 hash;
            std::array<std::uint8_t, 72> bytes{};
            for (const std::shared_ptr<ItemSpawn>& item : health)
            {
                const ::MphRead::ItemSpawnEntityData data
                    = MapResourceRules::ResolveData(room, profile, item->Data);
                // MemoryMarshal.Write(bytes, in data).
                std::memcpy(bytes.data(), &data, bytes.size());
                hash.AppendData(bytes);
            }
            const std::array<std::uint8_t, 32> digest = hash.GetHashAndReset();
            return ::MphRead::NativeRuntime::ConvertToHexString(digest).substr(0, 16);
        }

        [[nodiscard]] std::vector<Vector3> Positions(
            const std::vector<std::shared_ptr<::MphRead::Entity>>& entities)
        {
            std::vector<Vector3> result;
            for (const std::shared_ptr<::MphRead::Entity>& entity : entities)
            {
                result.push_back(entity->Position);
            }
            return result;
        }
    }

    std::int32_t ResourceAudit::Run()
    {
        using ::MphRead::GameMode;
        const std::array<Scenario, 18> scenarios = {{
            {"FFA2", GameMode::Battle, 2}, {"FFA3", GameMode::Battle, 3},
            {"FFA4", GameMode::Battle, 4}, {"FFA8", GameMode::Battle, 8},
            {"1v1", GameMode::BattleTeams, 2}, {"2v2", GameMode::BattleTeams, 4},
            {"3v3", GameMode::BattleTeams, 6}, {"4v4", GameMode::BattleTeams, 8},
            {"4v2", GameMode::BattleTeams, 6}, {"2v2v2v2", GameMode::BattleTeams, 8},
            {"BountyFFA4", GameMode::Bounty, 4}, {"BountyTeams8", GameMode::BountyTeams, 8},
            {"NodesFFA8", GameMode::Nodes, 8}, {"NodesTeams8", GameMode::NodesTeams, 8},
            {"DefenderFFA8", GameMode::Defender, 8}, {"DefenderTeams8", GameMode::DefenderTeams, 8},
            {"Capture2v2", GameMode::Capture, 4}, {"Capture4v4", GameMode::Capture, 8}}};
        std::int32_t checkedCount = 0;
        std::int32_t missingLayers = 0;
        std::int32_t missingObjectives = 0;
        std::int32_t failures = 0;
        std::cout << "room\tlayout\tlayer\tprofile\tspawns\tbaseHealth\thealth\thealthUnits\t"
            "maxSpawnDistance\tmaxObjectiveDistance\trespawnSeconds\tobjectives\tfingerprint"
            << ::MphRead::NativeRuntime::EnvironmentNewLine();
        for (const std::string& key : ThumbnailGenerator::MultiplayerRooms())
        {
            const ::MphRead::RoomMetadata& room
                = ::MphRead::NativeRuntime::RequireReference(::MphRead::Metadata::RoomMetadata.at(key));
            if (room.FirstHunt || !room.EntityPath.has_value())
            {
                continue;
            }
            for (const Scenario& scenario : scenarios)
            {
                try
                {
                    const MatchWorldProfile profile = MatchWorldProfile::Resolve(scenario.Players);
                    const std::int32_t layer = ::MphRead::Metadata::GetMultiplayerEntityLayer(
                        scenario.Mode, profile.EntityLayerPlayers);
                    const EntityList original
                        = ::MphRead::Read::GetEntities(*room.EntityPath, layer, false, true);
                    const EntityList actual
                        = MapResourceRules::Resolve(room, profile.Resources, original);
                    std::vector<std::shared_ptr<::MphRead::Entity>> spawns;
                    for (const std::shared_ptr<::MphRead::Entity>& entity : *actual)
                    {
                        const auto spawn = std::dynamic_pointer_cast<PlayerSpawn>(entity);
                        if (spawn != nullptr && spawn->Data.Active != 0)
                        {
                            spawns.push_back(entity);
                        }
                    }
                    if (spawns.empty())
                    {
                        ++missingLayers;
                    }
                    const std::vector<std::shared_ptr<ItemSpawn>> health = Health(actual);
                    const std::size_t baseHealth = Health(original).size();
                    std::vector<std::shared_ptr<::MphRead::Entity>> objectives;
                    for (const std::shared_ptr<::MphRead::Entity>& entity : *actual)
                    {
                        if (entity->Type == ::MphRead::EntityType::FlagBase
                            || entity->Type == ::MphRead::EntityType::OctolithFlag
                            || entity->Type == ::MphRead::EntityType::NodeDefense)
                        {
                            objectives.push_back(entity);
                        }
                    }
                    if (scenario.Mode != GameMode::Battle && scenario.Mode != GameMode::BattleTeams
                        && objectives.empty())
                    {
                        ++missingObjectives;
                    }
                    std::int32_t units = 0;
                    std::set<std::int32_t> intervals;
                    for (const std::shared_ptr<ItemSpawn>& item : health)
                    {
                        const ::MphRead::ItemSpawnEntityData data
                            = MapResourceRules::ResolveData(room, profile.Resources, item->Data);
                        units += data.ItemType == ::MphRead::ItemType::HealthSmall ? 60
                            : data.ItemType == ::MphRead::ItemType::HealthMedium ? 30 : 100;
                        intervals.insert(data.SpawnInterval / 30);
                    }
                    const std::string fingerprint = Fingerprint(room, profile.Resources, health);
                    const std::string repeated = Fingerprint(room, profile.Resources,
                        Health(MapResourceRules::Resolve(room, profile.Resources, original)));
                    std::set<std::int16_t> ids;
                    for (const std::shared_ptr<::MphRead::Entity>& entity : *actual)
                    {
                        ids.insert(entity->EntityId);
                    }
                    if (fingerprint != repeated || ids.size() != actual->size())
                    {
                        throw ::MphRead::ProgramException("Nondeterministic or duplicate entity IDs.");
                    }
                    for (const std::shared_ptr<ItemSpawn>& item : health)
                    {
                        const bool inOriginal = std::find(original->begin(), original->end(),
                            std::static_pointer_cast<::MphRead::Entity>(item)) != original->end();
                        if (inOriginal)
                        {
                            continue;
                        }
                        for (const std::shared_ptr<ItemSpawn>& other : health)
                        {
                            const Vector3 between = item->Position - other->Position;
                            if (other != item && OpenTK::Mathematics::LengthSquared(between) < 1.0F)
                            {
                                throw ::MphRead::ProgramException(
                                    "Supplemental health overlaps another pickup.");
                            }
                        }
                    }
                    std::string joined;
                    for (const std::int32_t interval : intervals)
                    {
                        if (!joined.empty())
                        {
                            joined.push_back('/');
                        }
                        joined += std::to_string(interval);
                    }
                    std::cout << key << '\t' << scenario.Label << '\t' << layer << '\t'
                        << ToString(profile.Resources) << '\t' << spawns.size() << '\t'
                        << baseHealth << '\t' << health.size() << '\t' << units << '\t'
                        << Distance(Positions(spawns), health) << '\t'
                        << Distance(Positions(objectives), health) << '\t' << joined << '\t'
                        << objectives.size() << '\t' << fingerprint
                        << ::MphRead::NativeRuntime::EnvironmentNewLine();
                    ++checkedCount;
                }
                catch (const std::exception& ex)
                {
                    ++failures;
                    ::MphRead::NativeRuntime::ConsoleErrorWriteLine("RESOURCEFAIL " + key + " "
                        + std::string(scenario.Label) + ": " + ex.what());
                }
            }
        }
        ::MphRead::NativeRuntime::ConsoleErrorWriteLine("Resource audit: "
            + std::to_string(checkedCount) + " layouts checked, " + std::to_string(failures)
            + " failures, " + std::to_string(missingLayers)
            + " layouts have no native player spawns; " + std::to_string(missingObjectives)
            + " objective layouts lack native objectives. "
            + "Distances are geometric; routes, reachability and competitive balance require "
            + "gameplay acceptance.");
        return failures == 0 ? 0 : 1;
    }
}
