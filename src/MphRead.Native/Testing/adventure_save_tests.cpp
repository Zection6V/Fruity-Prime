#include "Mods/Launcher/Portable/adventure_save.hpp"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>

namespace {

void require(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

} // namespace

int main() {
    const auto directory = std::filesystem::temp_directory_path()
        / ("fruity_prime_native_save_test_"
           + std::to_string(std::chrono::steady_clock::now()
                                .time_since_epoch().count()));
    try {
        std::filesystem::create_directories(directory);
        fruityprime::launcher::SaveStore store(directory);
        require(!store.exists(0), "slot zero was accepted");
        require(!store.read(0).has_value(), "slot zero was readable");

        fruityprime::game::StorySave save;
        save.checkpoint_entity_id = 17;
        save.checkpoint_room_id = 58;
        save.health = 123;
        save.health_max = 198;
        save.scan_count = 42;
        save.equipment_count = 9;
        save.weapons = 0x0055;
        save.found_octoliths = 0x0005;
        save.current_octoliths = 0x0001;
        save.stats.hunter_kills = 12;
        save.stats.deaths = 3;
        save.room_state[0][0] = 0xA5;
        save.room_state[63][7] = 0x5A;
        save.enemy_encounters[7][7] = 0x80;
        save.set_visited_room(58);
        save.set_visited_connector(33, 0);
        require(store.write(1, save), "save write failed");
        require(store.path_for_slot(1)
                        == directory / "Savedata" / "save001.json",
                "save path does not match the managed layout");

        const auto loaded = store.read(1);
        require(loaded.has_value(), "written save could not be read");
        require(loaded->checkpoint_entity_id == 17
                    && loaded->checkpoint_room_id == 58
                    && loaded->health == 123 && loaded->health_max == 198
                    && loaded->scan_count == 42
                    && loaded->weapons == 0x0055
                    && loaded->found_octoliths == 0x0005
                    && loaded->room_state[0][0] == 0xA5
                    && loaded->room_state[63][7] == 0x5A
                    && loaded->enemy_encounters[7][7] == 0x80
                    && loaded->stats.hunter_kills == 12,
                "save values did not survive the JSON round trip");
        require(fruityprime::launcher::SaveStore::area_name(*loaded)
                    == "Celestial Archives"
                    && fruityprime::launcher::SaveStore::start_room(*loaded)
                        == "UNIT2_RM4",
                "save room metadata mismatch");

        const auto slots = store.read_all();
        require(slots[0].used && slots[0].slot == 1
                    && slots[0].octoliths == 2
                    && slots[0].describe().find("2/8") != std::string::npos
                    && slots[0].describe().find("\xE2\x80\x94")
                        != std::string::npos
                    && !slots[1].used && slots[1].slot == 2,
                "save slot listing mismatch");

        // GameState accepts every nonzero byte slot even though the launcher
        // only offers three; the persistence adapter must not add a cap.
        require(store.write(4, save) && store.exists(4),
                "nonzero save slot was incorrectly capped to the UI count");

        fruityprime::game::State current_state;
        std::uint8_t selected_slot = 0;
        fruityprime::launcher::AdventureSave::BindRuntime(
            directory, &current_state, &selected_slot);
        const auto static_slot = fruityprime::launcher::AdventureSave::Read(1);
        require(static_slot.used && static_slot.health == 123,
                "AdventureSave.Read did not peek the requested slot");
        const auto static_slots = fruityprime::launcher::AdventureSave::ReadAll();
        require(static_slots[0].used && static_slots[2].slot == 3,
                "AdventureSave.ReadAll did not return slots one through three");
        require(fruityprime::launcher::AdventureSave::Begin(1, false)
                    == "UNIT2_RM4"
                    && selected_slot == 1
                    && current_state.story_save.health == 123,
                "AdventureSave.Begin did not select and load the slot");
        require(fruityprime::launcher::AdventureSave::Begin(1, true)
                    == "UNIT2_LAND"
                    && current_state.story_save.health == 99,
                "AdventureSave.Begin did not start a fresh unsaved game");
        require(store.read(1)->health == 123,
                "starting a new game overwrote the existing save");

        fruityprime::game::StorySave committed = save;
        committed.weapons = 0xFFFF;
        committed.weapon_slots[2] = 8;
        committed.room_state[64].fill(0xFF);
        committed.room_state[65].fill(0xFF);
        require(store.commit(2, committed), "save commit failed");
        require(committed.weapons == 0x00FF && committed.weapon_slots[2] == -1
                    && committed.room_state[64][0] == 0
                    && committed.room_state[65][59] == 0,
                "save commit safety corrections mismatch");
        const auto committed_read = store.read(2);
        require(committed_read.has_value()
                    && committed_read->weapon_slots[2] == -1
                    && committed_read->room_state[64][0] == 0,
                "committed save was not persisted");

        {
            std::ofstream malformed(store.path_for_slot(3), std::ios::trunc);
            malformed << "{\"not_a_save\": true}";
        }
        require(!store.read(3).has_value(),
                "malformed save was presented as a used slot");

        std::error_code cleanup_error;
        std::filesystem::remove_all(directory, cleanup_error);
        require(!std::filesystem::exists(directory),
                "save test directory could not be removed");
        std::cout << "native adventure save tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::error_code cleanup_error;
        std::filesystem::remove_all(directory, cleanup_error);
        std::cerr << error.what() << '\n';
        return 1;
    }
}
