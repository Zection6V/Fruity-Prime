#pragma once

// Native counterpart of the lookup surface in Metadata/Rooms.cs and the
// room helpers in Metadata/Metadata.cs.  The large immutable room records
// remain in room_metadata.hpp; this pair preserves the managed ID table,
// custom-room append order, lookup rules, and node-data overrides.

#include "Metadata/room_metadata.hpp"

#include <array>
#include <cstdint>
#include <optional>
#include <string_view>
#include <utility>
#include <vector>

namespace fruityprime::metadata {

inline constexpr std::array<std::pair<int, std::string_view>, 8>
    EncounterNodeDataOverrides{{
        {28, R"(levels\nodeData\unit1_C0_Boss_Node.bin)"},
        {29, R"(levels\nodeData\unit1_RM1_Boss_Node.bin)"},
        {31, R"(levels\nodeData\unit1_RM6_Boss_Node.bin)"},
        {50, R"(levels\nodeData\unit2_RM2_Boss_Node.bin)"},
        {52, R"(levels\nodeData\unit2_RM3_Boss_Node.bin)"},
        {65, R"(levels\nodeData\unit3_Land_Boss_Node.bin)"},
        {68, R"(levels\nodeData\unit3_RM1_Boss_Node.bin)"},
        {79, R"(levels\nodeData\unit4_RM3_Boss_Node.bin)"},
    }};

inline constexpr std::array<std::pair<int, std::string_view>, 8>
    CtfNodeDataOverrides{{
        {93, R"(levels\nodeData\mp1_CTF_node.bi))"},
        {99, R"(levels\nodeData\mp6_CTF_node.bi))"},
        {101, R"(levels\nodeData\mp8_CTF_node.bin)"},
        {102, R"(levels\nodeData\mp9_CTF_node.bin)"},
        {105, R"(levels\nodeData\mp12_CTF_node.bin)"},
        {107, R"(levels\nodeData\mp14_CTF_node.bin)"},
        {108, R"(levels\nodeData\ctf1_CTF_node.bin)"},
        {119, R"(levels\nodeData\e3Level_CTF_Node.bin)"},
    }};

// Metadata._roomIds, followed by CustomRooms.AppendIds in the same order.
[[nodiscard]] const std::vector<std::string_view>& room_ids();

// Metadata.GetRoomByName. The integer is the first matching _roomIds index.
[[nodiscard]] std::pair<const RoomMetadata*, int> get_room_by_name(
    std::string_view name);

// Metadata.GetRoomById. Invalid values throw unless no_throw is true.  The
// managed source has an intentional id == Count edge that reaches the list
// indexer; the native implementation preserves that distinction.
[[nodiscard]] const RoomMetadata* get_room_by_id(int id,
                                                 bool no_throw = false);

[[nodiscard]] constexpr std::uint32_t time_limit(
    std::uint32_t minutes, std::uint32_t seconds,
    std::uint32_t frames) noexcept {
    return minutes * 1800U + seconds * 30U + frames;
}

[[nodiscard]] std::optional<std::string_view>
encounter_node_data_override(int room_id) noexcept;

[[nodiscard]] std::optional<std::string_view>
ctf_node_data_override(int room_id) noexcept;

} // namespace fruityprime::metadata

