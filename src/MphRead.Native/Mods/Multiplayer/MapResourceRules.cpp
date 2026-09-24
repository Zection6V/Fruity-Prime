#include "MapResourceRules.hpp"

#include "../../Metadata/Rooms.hpp"
#include "../../NativeRuntime/System/Globalization.hpp"
#include "../../Read.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <set>
#include <span>
#include <string_view>
#include <utility>
#include <vector>

namespace MphRead::Mods::Multiplayer
{
    namespace
    {
        struct HighHealth final
        {
            std::string_view Room;
            std::vector<std::int16_t> Ids;
        };

        // Dictionary<string, short[]>(StringComparer.OrdinalIgnoreCase).
        const std::vector<HighHealth>& HighHealthRooms()
        {
            static const std::vector<HighHealth> rooms = {
                {"MP1 SANCTORUS", {7, 11, 46, 47, 4, 5}},
                {"MP2 HARVESTER", {27, 29, 30, 38, 57, 58, 63, 64, 60, 59, 2, 6}},
                {"MP3 PROVING GROUND", {10, 11, 12, 26, 0}},
                {"MP4 HIGHGROUND - EXPANDED", {1, 2, 16, 28, 32, 54, 73, 74, 75}},
                {"MP4 HIGHGROUND", {1, 2, 16, 27, 28, 46}},
                {"MP5 FUEL SLUICE", {16, 17, 22, 27, 31, 3, 4}},
                {"MP6 HEADSHOT", {28, 30, 32, 34, 40, 49, 54, 55, 69, 70, 71, 72, 12, 4, 5, 13}},
                {"MP7 PROCESSOR CORE", {6, 9, 14, 15, 19, 20, 21}},
                {"MP8 FIRE CONTROL", {26, 63, 7, 17, 18, 35}},
                {"MP9 CRYOCHASM", {0, 3, 23, 37, 36, 15, 26}},
                {"MP10 OVERLOAD", {1, 4, 10, 6, 19}},
                {"MP11 BREAKTHROUGH", {2, 3, 8, 19}},
                {"MP12 SIC TRANSIT", {8, 15, 43, 10}},
                {"MP13 ACCELERATOR", {6, 7, 8, 15, 16, 17, 23, 45}},
                {"MP14 OUTER REACH", {35, 36, 19, 18}},
                {"CTF1 FAULT LINE - EXPANDED", {9, 27, 34, 18, 28, 32, 54, 56, 75, 76}},
                {"CTF1_FAULT LINE", {9, 43, 44, 25, 49, 40}},
                {"AD1 TRANSFER LOCK BT", {5, 11, 12, 26, 37, 51, 52, 53, 57, 58}},
                {"AD1 TRANSFER LOCK DM", {9, 33, 34, 41, 44, 45}},
                {"AD2 MAGMA VENTS", {31, 32, 33, 35, 34, 17, 58, 70, 71}},
                {"AD2 ALINOS PERCH", {22, 25, 27, 42, 48, 49, 18, 14, 2}},
                {"UNIT1 ALINOS LANDFALL", {26, 29, 30, 19, 34, 37}},
                {"UNIT2 LANDING BAY", {12, 13, 16, 15, 21, 22, 24, 25, 26, 29}},
                {"UNIT 3 VESPER STARPORT", {2, 19, 23, 40, 41, 42, 43, 44, 60, 61, 62, 63}},
                {"UNIT 4 ARCTERRA BASE", {53, 55, 56, 58}},
                {"Gorea Prison", {13, 20, 21}},
                {"E3 FIRST HUNT", {15, 16, 10, 20, 50}}};
            return rooms;
        }

        [[nodiscard]] const std::vector<std::int16_t>* FindHighHealth(std::string_view room)
        {
            for (const HighHealth& entry : HighHealthRooms())
            {
                if (::MphRead::NativeRuntime::StringEqualsOrdinalIgnoreCase(entry.Room, room))
                {
                    return &entry.Ids;
                }
            }
            return nullptr;
        }

        [[nodiscard]] const ::MphRead::EntityOf<::MphRead::ItemSpawnEntityData>* AsItemSpawn(
            const ::MphRead::Entity& entity)
        {
            return dynamic_cast<const ::MphRead::EntityOf<::MphRead::ItemSpawnEntityData>*>(&entity);
        }

    }

    bool MapResourceRules::IsHealth(::MphRead::ItemType type) noexcept
    {
        return type == ::MphRead::ItemType::HealthSmall
            || type == ::MphRead::ItemType::HealthMedium
            || type == ::MphRead::ItemType::HealthBig;
    }

    MapResourceRules::EntityList MapResourceRules::Resolve(
        const ::MphRead::RoomMetadata& room, ResourceSpawnProfile profile, EntityList original)
    {
        const std::vector<std::int16_t>* found = FindHighHealth(room.Name);
        if (!room.Multiplayer || room.FirstHunt || !room.EntityPath.has_value() || found == nullptr)
        {
            return original;
        }
        std::vector<std::int16_t> ids = *found;
        if (profile != ResourceSpawnProfile::High)
        {
            // Transfer Lock's bounty variant has no Battle health in any
            // population layer. Use its authored bounty health routes.
            if (!::MphRead::NativeRuntime::StringEqualsOrdinalIgnoreCase(
                    room.Name, "AD1 TRANSFER LOCK BT"))
            {
                return original;
            }
            ids = profile == ResourceSpawnProfile::Low
                ? std::vector<std::int16_t>{5, 26, 37}
                : std::vector<std::int16_t>{5, 11, 12, 26, 37, 57, 58};
        }

        auto result = std::make_shared<std::vector<std::shared_ptr<::MphRead::Entity>>>(*original);
        std::set<std::int16_t> used;
        for (const std::shared_ptr<::MphRead::Entity>& entity : *original)
        {
            used.insert(entity->EntityId);
        }
        const std::shared_ptr<const std::vector<std::shared_ptr<::MphRead::Entity>>> all
            = ::MphRead::Read::GetEntities(*room.EntityPath, -1, false, true);
        // Source-file order is stable across server, clients and reconnects.
        for (const std::shared_ptr<::MphRead::Entity>& entity : *all)
        {
            const auto* item = AsItemSpawn(*entity);
            if (std::find(ids.begin(), ids.end(), entity->EntityId) == ids.end()
                || used.contains(entity->EntityId)
                || item == nullptr || !IsHealth(item->Data.ItemType)
                || item->Data.Enabled == 0
                || !(item->Data.ParentId == -1 || item->Data.ParentId == 65535))
            {
                continue;
            }
            bool overlaps = false;
            for (const std::shared_ptr<::MphRead::Entity>& existing : *result)
            {
                const auto* other = AsItemSpawn(*existing);
                if (other != nullptr && other->Data.Enabled != 0
                    && IsHealth(other->Data.ItemType)
                    && DistanceSquared(existing->Position, entity->Position) < 1.0F)
                {
                    overlaps = true;
                    break;
                }
            }
            if (overlaps)
            {
                continue;
            }
            result->push_back(entity);
            used.insert(entity->EntityId);
        }
        return result;
    }

    ::MphRead::ItemSpawnEntityData MapResourceRules::ResolveData(
        const ::MphRead::RoomMetadata& room, ResourceSpawnProfile profile,
        const ::MphRead::ItemSpawnEntityData& data)
    {
        if (profile != ResourceSpawnProfile::High || !room.Multiplayer || room.FirstHunt
            || FindHighHealth(room.Name) == nullptr || !IsHealth(data.ItemType))
        {
            return data;
        }
        // High population keeps the original routes but caps health downtime
        // at ten seconds (the source format stores thirty ticks per second).
        if (data.SpawnInterval <= 300)
        {
            return data;
        }
        std::array<std::uint8_t, 72> bytes{};
        static_assert(sizeof(::MphRead::ItemSpawnEntityData) == 72);
        std::memcpy(bytes.data(), &data, bytes.size());
        // BinaryPrimitives.WriteUInt16LittleEndian(bytes[54..], 300).
        bytes[54] = static_cast<std::uint8_t>(300 & 0xFF);
        bytes[55] = static_cast<std::uint8_t>(300 >> 8);
        return ::MphRead::Read::ReadStruct<::MphRead::ItemSpawnEntityData>(
            std::span<const std::uint8_t>(bytes));
    }
}
