#include "Metadata/Rooms.hpp"
#include "Formats/fixed.hpp"
#include "Mods/MapGen/custom_rooms.hpp"
#include "metadata_tables.hpp"

#include <array>
#include <cstddef>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace fruityprime::metadata {
namespace {

// Metadata._roomIds.  These are IDs in the ROM room table, not the Id member
// on First Hunt RoomMetadata records (which restarts at zero).
constexpr std::array<std::string_view, 138> BaseRoomIds{{
    "UNIT1_CX", "UNIT1_CX", "UNIT1_CZ", "UNIT1_CZ",
    "UNIT1_MORPH_CX", "UNIT1_MORPH_CX", "UNIT1_MORPH_CZ",
    "UNIT1_MORPH_CZ", "UNIT2_CX", "UNIT2_CX", "UNIT2_CZ", "UNIT2_CZ",
    "UNIT3_CX", "UNIT3_CX", "UNIT3_CZ", "UNIT3_CZ", "UNIT4_CX",
    "UNIT4_CX", "UNIT4_CZ", "UNIT4_CZ", "CYLINDER_C1", "BIGEYE_C1",
    "UNIT1_RM1_CX", "UNIT1_RM1_CX", "GOREA_C1", "UNIT3_MORPH_CZ",
    "UNIT3_MORPH_CZ", "UNIT1_LAND", "UNIT1_C0", "UNIT1_RM1",
    "UNIT1_C4", "UNIT1_RM6", "CRYSTALROOM", "UNIT1_RM4", "UNIT1_TP1",
    "UNIT1_B1", "UNIT1_C1", "UNIT1_C2", "UNIT1_C5", "UNIT1_RM2",
    "UNIT1_RM3", "UNIT1_RM5", "UNIT1_C3", "UNIT1_TP2", "UNIT1_B2",
    "UNIT2_LAND", "UNIT2_C0", "UNIT2_C1", "UNIT2_RM1", "UNIT2_C2",
    "UNIT2_RM2", "UNIT2_C3", "UNIT2_RM3", "UNIT2_C4", "UNIT2_TP1",
    "UNIT2_B1", "UNIT2_C6", "UNIT2_C7", "UNIT2_RM4", "UNIT2_RM5",
    "UNIT2_RM6", "UNIT2_RM7", "UNIT2_RM8", "UNIT2_TP2", "UNIT2_B2",
    "UNIT3_LAND", "UNIT3_C0", "UNIT3_C2", "UNIT3_RM1", "UNIT3_RM4",
    "UNIT3_TP1", "UNIT3_B1", "UNIT3_C1", "UNIT3_RM2", "UNIT3_RM3",
    "UNIT3_TP2", "UNIT3_B2", "UNIT4_LAND", "UNIT4_RM1", "UNIT4_RM3",
    "UNIT4_C0", "UNIT4_TP1", "UNIT4_B1", "UNIT4_C1", "UNIT4_RM2",
    "UNIT4_RM4", "UNIT4_RM5", "UNIT4_TP2", "UNIT4_B2", "Gorea_Land",
    "Gorea_Peek", "Gorea_b1", "Gorea_b2", "MP1 SANCTORUS",
    "MP2 HARVESTER", "MP3 PROVING GROUND", "MP4 HIGHGROUND - EXPANDED",
    "MP4 HIGHGROUND", "MP5 FUEL SLUICE", "MP6 HEADSHOT",
    "MP7 PROCESSOR CORE", "MP8 FIRE CONTROL", "MP9 CRYOCHASM",
    "MP10 OVERLOAD", "MP11 BREAKTHROUGH", "MP12 SIC TRANSIT",
    "MP13 ACCELERATOR", "MP14 OUTER REACH", "CTF1 FAULT LINE - EXPANDED",
    "CTF1_FAULT LINE", "AD1 TRANSFER LOCK BT", "AD1 TRANSFER LOCK DM",
    "AD2 MAGMA VENTS", "AD2 ALINOS PERCH", "UNIT1 ALINOS LANDFALL",
    "UNIT2 LANDING BAY", "UNIT 3 VESPER STARPORT", "UNIT 4 ARCTERRA BASE",
    "Gorea Prison", "E3 FIRST HUNT", "Level TestLevel", "Level AbeTest",
    "biodefense chamber 06", "biodefense chamber 05",
    "biodefense chamber 03", "biodefense chamber 08",
    "biodefense chamber 04", "biodefense chamber 07", "Level MP1",
    "Level MP2", "Level MP3", "Level SP Morphball", "Level SP Regulator",
    "Level SP Survivor", "Level FhTestLevel", "Level MP5", "Level MP1b",
    "E3 level",
}};

struct CustomRoomStorage {
    std::string archive;
    std::string model_path;
    std::string animation_path;
    std::string collision_path;
    std::string entity_path;
    std::string entity_filename;
    std::string node_path;
    RoomMetadata metadata;
};

[[nodiscard]] formats::ColorRgb color(
    const std::array<int, 3>& values) noexcept {
    return {static_cast<std::uint8_t>(values[0]),
            static_cast<std::uint8_t>(values[1]),
            static_cast<std::uint8_t>(values[2])};
}

[[nodiscard]] formats::Vector3 vector(mapgen::Vec3 value) noexcept {
    return formats::Vector3{value.x, value.y, value.z}.normalized();
}

[[nodiscard]] const std::vector<CustomRoomStorage>& custom_room_storage() {
    static const std::vector<CustomRoomStorage> storage = [] {
        const auto& definitions = mapgen::custom_rooms::definitions();
        std::vector<CustomRoomStorage> result;
        result.reserve(definitions.size());
        for (const auto& definition : definitions) {
            result.emplace_back();
            auto& item = result.back();

            // CustomRooms.MakeMetadata uses Name.ToLowerInvariant verbatim.
            // Definitions are normalized to uppercase ASCII when loaded, so
            // the map generator's lower-case spelling is the same value.
            item.archive = mapgen::file_prefix(definition);
            const std::string& prefix = item.archive;
            item.model_path = "_archives\\" + prefix + "\\" + prefix
                + "_Model.bin";
            item.animation_path = "_archives\\" + prefix + "\\" + prefix
                + "_Anim.bin";
            item.collision_path = "_archives\\" + prefix + "\\" + prefix
                + "_Collision.bin";
            item.entity_filename = prefix + "_Ent.bin";
            item.entity_path = "levels\\entities\\" + item.entity_filename;
            item.node_path = "levels\\nodeData\\" + prefix + "_Node.bin";

            const int far_clip = formats::Fixed::to_int(definition.far_clip);
            const int kill_height = formats::Fixed::to_int(
                definition.kill_height);
            item.metadata = RoomMetadata{
                static_cast<int>(BaseRoomIds.size() + result.size() - 1),
                definition.name,
                definition.in_game_name.empty() ? std::string_view{definition.name}
                                                : std::string_view{definition.in_game_name},
                item.archive, item.model_path, item.animation_path,
                item.collision_path, {}, item.entity_path,
                item.entity_filename, item.node_path, {},
                definition.battle_time_limit, definition.battle_time_limit,
                definition.point_limit, 0, definition.fog_enabled, false,
                color(definition.fog_color), definition.fog_slope,
                static_cast<int>(static_cast<std::uint16_t>(
                    definition.fog_offset) & 0x7FFFU),
                color(definition.light1_color), vector(definition.light1_vector),
                color(definition.light2_color), vector(definition.light2_vector),
                formats::Fixed{far_clip}.to_float(), far_clip,
                formats::Fixed{kill_height}.to_float(), RoomSize::Large,
                true, false, false, {}, {}, {}, {}, false};
        }
        return result;
    }();
    return storage;
}

template <std::size_t Size>
[[nodiscard]] std::optional<std::string_view> find_override(
    const std::array<std::pair<int, std::string_view>, Size>& values,
    int room_id) noexcept {
    for (const auto& [id, path] : values) {
        if (id == room_id) {
            return path;
        }
    }
    return std::nullopt;
}

} // namespace

const std::vector<std::string_view>& room_ids() {
    static const std::vector<std::string_view> ids = [] {
        std::vector<std::string_view> result(BaseRoomIds.begin(),
                                             BaseRoomIds.end());
        const auto& custom = custom_room_storage();
        result.reserve(result.size() + custom.size());
        for (const auto& room : custom) {
            result.push_back(room.metadata.name);
        }
        return result;
    }();
    return ids;
}

std::pair<const RoomMetadata*, int> get_room_by_name(std::string_view name) {
    const RoomMetadata* metadata = find_room_metadata(name);
    if (metadata == nullptr) {
        for (const auto& room : custom_room_storage()) {
            if (room.metadata.name == name) {
                metadata = &room.metadata;
                break;
            }
        }
    }
    if (metadata == nullptr) {
        return {nullptr, -1};
    }
    const auto& ids = room_ids();
    for (std::size_t index = 0; index < ids.size(); ++index) {
        if (ids[index] == metadata->name) {
            return {metadata, static_cast<int>(index)};
        }
    }
    return {metadata, -1};
}

const RoomMetadata* get_room_by_id(int id, bool no_throw) {
    const auto& ids = room_ids();
    if (id < 0 || id > static_cast<int>(ids.size())) {
        if (no_throw) {
            return nullptr;
        }
        throw std::invalid_argument("id");
    }
    if (id == static_cast<int>(ids.size())) {
        throw std::out_of_range("id");
    }
    return get_room_by_name(ids[static_cast<std::size_t>(id)]).first;
}

std::optional<std::string_view> encounter_node_data_override(
    int room_id) noexcept {
    return find_override(EncounterNodeDataOverrides, room_id);
}

std::optional<std::string_view> ctf_node_data_override(
    int room_id) noexcept {
    return find_override(CtfNodeDataOverrides, room_id);
}

const GameModeInfo* game_mode_info(std::uint8_t id) noexcept {
    for (const auto& mode : detail::ModeTable) {
        if (mode.id == id) {
            return &mode;
        }
    }
    return nullptr;
}

std::string_view game_mode_name(std::uint8_t id) noexcept {
    const auto* mode = game_mode_info(id);
    return mode == nullptr ? std::string_view{"MATCH"} : mode->name;
}

std::int32_t area_info(std::int32_t room_id) noexcept {
    // Keep the boundary identical to Metadata.GetAreaInfo.  The final story
    // rooms (Gorea/Oubliette) and all multiplayer rooms are area 8.
    if (room_id >= 27 && room_id < 36) {
        return 0; // Alinos 1
    }
    if (room_id >= 36 && room_id < 45) {
        return 1; // Alinos 2
    }
    if (room_id >= 45 && room_id < 56) {
        return 2; // Celestial Archives 1
    }
    if (room_id >= 56 && room_id < 65) {
        return 3; // Celestial Archives 2
    }
    if (room_id >= 65 && room_id < 72) {
        return 4; // Vesper Defense Outpost 1
    }
    if (room_id >= 72 && room_id < 77) {
        return 5; // Vesper Defense Outpost 2
    }
    if (room_id >= 77 && room_id < 83) {
        return 6; // Arcterra 1
    }
    if (room_id >= 83 && room_id < 89) {
        return 7; // Arcterra 2
    }
    return 8; // Oubliette and non-story room IDs
}


} // namespace fruityprime::metadata
