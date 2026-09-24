#pragma once

#include "MatchWorldProfile.hpp"

#include "../../Formats/Entity.hpp"
#include "../../Formats/Enums.hpp"
#include "../../Formats/Formats.hpp"

#include <memory>
#include <vector>

namespace MphRead
{
    class RoomMetadata;
}

namespace MphRead::Mods::Multiplayer
{
    // Only references to locations already authored in each room. The
    // player's own assets supply the positions, node names and pickup
    // definitions.
    class MapResourceRules final
    {
    public:
        MapResourceRules() = delete;
        ~MapResourceRules() = delete;
        MapResourceRules(const MapResourceRules&) = delete;
        MapResourceRules& operator=(const MapResourceRules&) = delete;
        MapResourceRules(MapResourceRules&&) = delete;
        MapResourceRules& operator=(MapResourceRules&&) = delete;

        using EntityList = std::shared_ptr<const std::vector<std::shared_ptr<::MphRead::Entity>>>;

        [[nodiscard]] static bool IsHealth(::MphRead::ItemType type) noexcept;

        [[nodiscard]] static EntityList Resolve(const ::MphRead::RoomMetadata& room,
            ResourceSpawnProfile profile, EntityList original);

        [[nodiscard]] static ::MphRead::ItemSpawnEntityData ResolveData(
            const ::MphRead::RoomMetadata& room, ResourceSpawnProfile profile,
            const ::MphRead::ItemSpawnEntityData& data);
    };
}
