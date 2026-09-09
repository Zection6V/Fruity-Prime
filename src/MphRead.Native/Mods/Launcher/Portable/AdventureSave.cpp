#include "Mods/Launcher/Portable/adventure_save.hpp"
#include "Metadata/metadata.hpp"

#include <algorithm>
#include <charconv>
#include <fstream>
#include <iomanip>
#include <limits>
#include <sstream>
#include <string_view>
#include <system_error>
#include <type_traits>
#include <utility>
#include <vector>

namespace fruityprime::launcher {
namespace {

using Number = std::int64_t;

std::filesystem::path adventure_root;
game::State* adventure_state = nullptr;
std::uint8_t* adventure_save_slot = nullptr;

[[nodiscard]] std::size_t skip_space(std::string_view text,
                                     std::size_t position) noexcept {
    while (position < text.size()) {
        const char character = text[position];
        if (character != ' ' && character != '\t' && character != '\r'
            && character != '\n') {
            break;
        }
        ++position;
    }
    return position;
}

[[nodiscard]] std::optional<std::pair<std::size_t, std::size_t>> value_range(
    std::string_view text, std::string_view property) noexcept {
    const std::string quoted = "\"" + std::string(property) + "\"";
    std::size_t search = 0;
    while ((search = text.find(quoted, search)) != std::string_view::npos) {
        std::size_t position = skip_space(text, search + quoted.size());
        if (position >= text.size() || text[position] != ':') {
            search += quoted.size();
            continue;
        }
        position = skip_space(text, position + 1);
        if (position >= text.size()) {
            return std::nullopt;
        }
        const char first = text[position];
        if (first != '[' && first != '{' && first != '"') {
            std::size_t end = position;
            while (end < text.size() && text[end] != ',' && text[end] != '}'
                   && text[end] != '\r' && text[end] != '\n') {
                ++end;
            }
            while (end > position && (text[end - 1] == ' '
                                      || text[end - 1] == '\t')) {
                --end;
            }
            return std::pair{position, end};
        }
        if (first == '"') {
            bool escaped = false;
            for (std::size_t end = position + 1; end < text.size(); ++end) {
                const char character = text[end];
                if (character == '"' && !escaped) {
                    return std::pair{position, end + 1};
                }
                if (character == '\\' && !escaped) {
                    escaped = true;
                } else {
                    escaped = false;
                }
            }
            return std::nullopt;
        }

        std::vector<char> brackets;
        bool in_string = false;
        bool escaped = false;
        for (std::size_t end = position; end < text.size(); ++end) {
            const char character = text[end];
            if (in_string) {
                if (character == '"' && !escaped) {
                    in_string = false;
                }
                if (character == '\\' && !escaped) {
                    escaped = true;
                } else {
                    escaped = false;
                }
                continue;
            }
            if (character == '"') {
                in_string = true;
                continue;
            }
            if (character == '[' || character == '{') {
                brackets.push_back(character);
                continue;
            }
            if (character != ']' && character != '}') {
                continue;
            }
            if (brackets.empty()) {
                return std::nullopt;
            }
            const char expected = character == ']' ? '[' : '{';
            if (brackets.back() != expected) {
                return std::nullopt;
            }
            brackets.pop_back();
            if (brackets.empty()) {
                return std::pair{position, end + 1};
            }
        }
        return std::nullopt;
    }
    return std::nullopt;
}

[[nodiscard]] bool parse_number(std::string_view text, Number& value) noexcept {
    const std::size_t start = skip_space(text, 0);
    if (start == text.size()) {
        return false;
    }
    const char* first = text.data() + start;
    const char* last = text.data() + text.size();
    const auto result = std::from_chars(first, last, value);
    return result.ec == std::errc{};
}

[[nodiscard]] bool parse_numbers(std::string_view text,
                                 std::vector<Number>& values) noexcept {
    bool found = false;
    std::size_t position = 0;
    while (position < text.size()) {
        const char character = text[position];
        if (character != '-' && (character < '0' || character > '9')) {
            ++position;
            continue;
        }
        const std::size_t start = position;
        if (text[position] == '-') {
            ++position;
        }
        const std::size_t digits = position;
        while (position < text.size() && text[position] >= '0'
               && text[position] <= '9') {
            ++position;
        }
        if (position == digits) {
            return false;
        }
        Number value = 0;
        if (!parse_number(text.substr(start, position - start), value)) {
            return false;
        }
        values.push_back(value);
        found = true;
    }
    return found;
}

[[nodiscard]] bool read_numbers(
    std::string_view text, std::string_view property,
    std::vector<Number>& values) noexcept {
    const auto range = value_range(text, property);
    return range.has_value()
        && parse_numbers(text.substr(range->first, range->second - range->first),
                         values);
}

[[nodiscard]] std::optional<Number> read_number(
    std::string_view text, std::string_view property) noexcept {
    std::vector<Number> values;
    if (!read_numbers(text, property, values) || values.empty()) {
        return std::nullopt;
    }
    return values.front();
}

template <typename T>
void copy_numbers(const std::vector<Number>& values, std::size_t& cursor,
                  T* destination, std::size_t count) noexcept {
    std::size_t index = 0;
    while (cursor < values.size() && index < count) {
        destination[index] = static_cast<T>(values[cursor]);
        ++index;
        ++cursor;
    }
}

template <typename T, std::size_t N>
void read_array(std::string_view text, std::string_view property,
                std::array<T, N>& destination) noexcept {
    std::vector<Number> values;
    if (!read_numbers(text, property, values)) {
        return;
    }
    std::size_t cursor = 0;
    copy_numbers(values, cursor, destination.data(), destination.size());
}

template <typename T, std::size_t Rows, std::size_t Columns>
void read_matrix(std::string_view text, std::string_view property,
                 std::array<std::array<T, Columns>, Rows>& destination) noexcept {
    std::vector<Number> values;
    if (!read_numbers(text, property, values)) {
        return;
    }
    std::size_t cursor = 0;
    for (auto& row : destination) {
        copy_numbers(values, cursor, row.data(), row.size());
    }
}

template <typename T>
void write_array(std::ostream& output, const T* values, std::size_t count) {
    output << '[';
    for (std::size_t index = 0; index < count; ++index) {
        if (index != 0) {
            output << ',';
        }
        output << static_cast<std::int64_t>(values[index]);
    }
    output << ']';
}

template <typename T, std::size_t N>
void write_array(std::ostream& output, const std::array<T, N>& values) {
    write_array(output, values.data(), values.size());
}

template <typename T, std::size_t Rows, std::size_t Columns>
void write_matrix(std::ostream& output,
                  const std::array<std::array<T, Columns>, Rows>& values) {
    output << '[';
    for (std::size_t row = 0; row < Rows; ++row) {
        if (row != 0) {
            output << ',';
        }
        write_array(output, values[row]);
    }
    output << ']';
}

[[nodiscard]] std::string make_slot_name(std::uint8_t slot) {
    std::ostringstream name;
    name << "save" << std::setfill('0') << std::setw(3)
         << static_cast<unsigned>(slot) << ".json";
    return name.str();
}

[[nodiscard]] const std::array<std::string_view, game::StorySave::RoomCount>&
story_room_names() {
    static constexpr std::array<std::string_view, game::StorySave::RoomCount>
        names{{
            "UNIT1_LAND", "UNIT1_C0", "UNIT1_RM1", "UNIT1_C4",
            "UNIT1_RM6", "CRYSTALROOM", "UNIT1_RM4", "UNIT1_TP1",
            "UNIT1_B1", "UNIT1_C1", "UNIT1_C2", "UNIT1_C5",
            "UNIT1_RM2", "UNIT1_RM3", "UNIT1_RM5", "UNIT1_C3",
            "UNIT1_TP2", "UNIT1_B2", "UNIT2_LAND", "UNIT2_C0",
            "UNIT2_C1", "UNIT2_RM1", "UNIT2_C2", "UNIT2_RM2",
            "UNIT2_C3", "UNIT2_RM3", "UNIT2_C4", "UNIT2_TP1",
            "UNIT2_B1", "UNIT2_C6", "UNIT2_C7", "UNIT2_RM4",
            "UNIT2_RM5", "UNIT2_RM6", "UNIT2_RM7", "UNIT2_RM8",
            "UNIT2_TP2", "UNIT2_B2", "UNIT3_LAND", "UNIT3_C0",
            "UNIT3_C2", "UNIT3_RM1", "UNIT3_RM4", "UNIT3_TP1",
            "UNIT3_B1", "UNIT3_C1", "UNIT3_RM2", "UNIT3_RM3",
            "UNIT3_TP2", "UNIT3_B2", "UNIT4_LAND", "UNIT4_RM1",
            "UNIT4_RM3", "UNIT4_C0", "UNIT4_TP1", "UNIT4_B1",
            "UNIT4_C1", "UNIT4_RM2", "UNIT4_RM4", "UNIT4_RM5",
            "UNIT4_TP2", "UNIT4_B2", "Gorea_Land", "Gorea_Peek",
            "Gorea_b1", "Gorea_b2"
        }};
    return names;
}

void load_save(std::string_view text, game::StorySave& save) noexcept {
    read_matrix(text, "RoomState", save.room_state);
    read_array(text, "VisitedRooms", save.visited_rooms);
    read_array(text, "VisitedConnectors", save.visited_connectors);
    read_array(text, "TriggerState", save.trigger_state);
    read_array(text, "Logbook", save.logbook);
    read_matrix(text, "EnemyEncounters", save.enemy_encounters);

    const auto assign = [&](std::string_view name, auto& destination) {
        if (const auto value = read_number(text, name); value.has_value()) {
            destination = static_cast<std::decay_t<decltype(destination)>>(
                *value);
        }
    };
    assign("ScanCount", save.scan_count);
    assign("EquipmentCount", save.equipment_count);
    assign("CheckpointEntityId", save.checkpoint_entity_id);
    assign("CheckpointRoomId", save.checkpoint_room_id);
    assign("Health", save.health);
    assign("HealthMax", save.health_max);
    read_array(text, "Ammo", save.ammo);
    read_array(text, "AmmoMax", save.ammo_max);
    read_array(text, "WeaponSlots", save.weapon_slots);
    assign("Weapons", save.weapons);
    assign("Artifacts", save.artifacts);
    assign("FoundOctoliths", save.found_octoliths);
    assign("CurrentOctoliths", save.current_octoliths);
    assign("LostOctoliths", save.lost_octoliths);
    assign("Areas", save.areas);
    assign("BossFlags", save.boss_flags);
    read_array(text, "AreaHunters", save.area_hunters);
    assign("DefeatedHunters", save.defeated_hunters);
    if (const auto value = read_number(text, "HunterKills"); value.has_value()) {
        save.stats.hunter_kills = static_cast<std::uint32_t>(*value);
    }
    if (const auto value = read_number(text, "Deaths"); value.has_value()) {
        save.stats.deaths = static_cast<std::uint32_t>(*value);
    }
    if (const auto value = read_number(text, "EnemyHunterDeaths");
        value.has_value()) {
        save.stats.enemy_hunter_deaths = static_cast<std::uint32_t>(*value);
    }
    if (const auto value = read_number(text, "EnemyKills"); value.has_value()) {
        save.stats.enemy_kills = static_cast<std::uint32_t>(*value);
    }
}

void write_save(std::ostream& output, const game::StorySave& save) {
    output << "{\n  \"RoomState\": ";
    write_matrix(output, save.room_state);
    output << ",\n  \"VisitedRooms\": ";
    write_array(output, save.visited_rooms);
    output << ",\n  \"VisitedConnectors\": ";
    write_array(output, save.visited_connectors);
    output << ",\n  \"TriggerState\": ";
    write_array(output, save.trigger_state);
    output << ",\n  \"Logbook\": ";
    write_array(output, save.logbook);
    output << ",\n  \"EnemyEncounters\": ";
    write_matrix(output, save.enemy_encounters);
    output << ",\n  \"ScanCount\": " << save.scan_count
           << ",\n  \"EquipmentCount\": " << save.equipment_count
           << ",\n  \"CheckpointEntityId\": " << save.checkpoint_entity_id
           << ",\n  \"CheckpointRoomId\": " << save.checkpoint_room_id
           << ",\n  \"Health\": " << save.health
           << ",\n  \"HealthMax\": " << save.health_max
           << ",\n  \"Ammo\": ";
    write_array(output, save.ammo);
    output << ",\n  \"AmmoMax\": ";
    write_array(output, save.ammo_max);
    output << ",\n  \"WeaponSlots\": ";
    write_array(output, save.weapon_slots);
    output << ",\n  \"Weapons\": "
           << static_cast<std::uint32_t>(save.weapons)
           << ",\n  \"Artifacts\": " << save.artifacts
           << ",\n  \"FoundOctoliths\": "
           << static_cast<std::uint32_t>(save.found_octoliths)
           << ",\n  \"CurrentOctoliths\": "
           << static_cast<std::uint32_t>(save.current_octoliths)
           << ",\n  \"LostOctoliths\": " << save.lost_octoliths
           << ",\n  \"Areas\": "
           << static_cast<std::uint32_t>(save.areas)
           << ",\n  \"BossFlags\": " << save.boss_flags
           << ",\n  \"AreaHunters\": ";
    write_array(output, save.area_hunters);
    output << ",\n  \"DefeatedHunters\": "
           << static_cast<std::uint32_t>(save.defeated_hunters)
           << ",\n  \"Stats\": {\"HunterKills\": "
           << save.stats.hunter_kills << ", \"Deaths\": "
           << save.stats.deaths << ", \"EnemyHunterDeaths\": "
           << save.stats.enemy_hunter_deaths << ", \"EnemyKills\": "
           << save.stats.enemy_kills << "}\n}\n";
}

} // namespace

std::string SlotInfo::describe() const {
    if (!used) {
        return "Empty";
    }
    return area + " \xE2\x80\x94 " + std::to_string(octoliths)
        + "/8 octoliths";
}

SaveStore::SaveStore(std::filesystem::path root_directory)
    : root_directory_(root_directory.empty()
          ? std::filesystem::current_path()
          : std::move(root_directory)) {}

std::filesystem::path SaveStore::path_for_slot(std::uint8_t slot) const {
    return root_directory_ / "Savedata" / make_slot_name(slot);
}

bool SaveStore::exists(std::uint8_t slot) const noexcept {
    if (slot == 0) {
        return false;
    }
    std::error_code error;
    return std::filesystem::is_regular_file(path_for_slot(slot), error);
}

std::optional<game::StorySave> SaveStore::read(std::uint8_t slot) const {
    if (!exists(slot)) {
        return std::nullopt;
    }
    std::ifstream input(path_for_slot(slot), std::ios::binary);
    if (!input) {
        return std::nullopt;
    }
    const std::string text((std::istreambuf_iterator<char>(input)),
                           std::istreambuf_iterator<char>());
    // RoomState is the large required member in the managed type. Requiring
    // it also makes a half-written or unrelated JSON file an empty slot,
    // matching PeekSave's exception-safe menu behavior.
    const auto room_state = value_range(text, "RoomState");
    if (!room_state.has_value()) {
        return std::nullopt;
    }
    game::StorySave save;
    load_save(text, save);
    return save;
}

std::array<SlotInfo, SlotCount> SaveStore::read_all() const {
    std::array<SlotInfo, SlotCount> slots{};
    for (std::size_t index = 0; index < slots.size(); ++index) {
        const auto slot = static_cast<std::uint8_t>(index + 1);
        slots[index].slot = slot;
        const auto save = read(slot);
        if (!save.has_value()) {
            continue;
        }
        slots[index].used = true;
        slots[index].area = area_name(*save);
        slots[index].octoliths = save->found_octolith_count();
        slots[index].health = save->health;
        slots[index].health_max = save->health_max;
    }
    return slots;
}

bool SaveStore::write(std::uint8_t slot, const game::StorySave& save) const {
    if (slot == 0) {
        return false;
    }
    std::error_code error;
    std::filesystem::create_directories(path_for_slot(slot).parent_path(),
                                        error);
    if (error) {
        return false;
    }
    std::ofstream output(path_for_slot(slot), std::ios::binary | std::ios::trunc);
    if (!output) {
        return false;
    }
    write_save(output, save);
    return static_cast<bool>(output);
}

bool SaveStore::commit(std::uint8_t slot, game::StorySave& save) const {
    if (slot == 0) {
        return false;
    }
    save.weapons = static_cast<std::uint16_t>(save.weapons & 0x00ffU);
    if (save.weapon_slots[2] == 8) {
        save.weapon_slots[2] = -1;
    }
    for (std::int32_t room_id = 91; room_id <= 92; ++room_id) {
        save.room_state[static_cast<std::size_t>(room_id - 27)].fill(0);
    }
    return write(slot, save);
}

std::string SaveStore::area_name(const game::StorySave& save) {
    const std::int32_t room_id = save.checkpoint_room_id;
    if (room_id < 0) {
        return "Celestial Archives";
    }
    static constexpr std::array<std::string_view, 9> names{{
        "Alinos", "Alinos", "Celestial Archives", "Celestial Archives",
        "Vesper Defense Outpost", "Vesper Defense Outpost",
        "Arcterra", "Arcterra", "Oubliette"
    }};
    const std::int32_t area_id = metadata::area_info(room_id);
    return area_id >= 0 && area_id < static_cast<std::int32_t>(names.size())
        ? std::string(names[static_cast<std::size_t>(area_id)])
        : "Unknown";
}

std::string SaveStore::start_room(const game::StorySave& save) {
    constexpr std::int32_t NewGameRoomId = 45;
    std::int32_t room_id = save.checkpoint_room_id;
    if (room_id < 27 || room_id > 92) {
        room_id = NewGameRoomId;
    }
    const auto& names = story_room_names();
    return std::string(names[static_cast<std::size_t>(room_id - 27)]);
}

void AdventureSave::BindRuntime(std::filesystem::path root_directory,
                                game::State* state,
                                std::uint8_t* selected_save_slot) noexcept {
    adventure_root = root_directory.empty()
        ? std::filesystem::current_path() : std::move(root_directory);
    adventure_state = state;
    adventure_save_slot = selected_save_slot;
}

AdventureSave::SlotInfo AdventureSave::Read(std::uint8_t slot) {
    SlotInfo result;
    result.slot = slot;
    const auto save = SaveStore(adventure_root).read(slot);
    if (!save.has_value()) {
        return result;
    }
    result.used = true;
    result.area = SaveStore::area_name(*save);
    result.octoliths = save->found_octolith_count();
    result.health = save->health;
    result.health_max = save->health_max;
    return result;
}

std::array<AdventureSave::SlotInfo, AdventureSave::SlotCount>
AdventureSave::ReadAll() {
    std::array<SlotInfo, SlotCount> result{};
    for (std::size_t index = 0; index < result.size(); ++index) {
        result[index] = Read(static_cast<std::uint8_t>(index + 1));
    }
    return result;
}

std::string AdventureSave::Begin(std::uint8_t slot, bool new_game) {
    if (adventure_save_slot != nullptr) {
        *adventure_save_slot = slot;
    }
    if (adventure_state == nullptr) {
        return {};
    }
    if (new_game) {
        adventure_state->start_new_save();
    } else {
        adventure_state->story_save = SaveStore(adventure_root).read(slot)
            .value_or(game::StorySave{});
    }
    return StartRoom(adventure_state->story_save);
}

std::string AdventureSave::StartRoom(const game::StorySave& save) {
    return SaveStore::start_room(save);
}

} // namespace fruityprime::launcher
