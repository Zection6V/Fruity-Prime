#include "Mods/Launcher/native_launcher.hpp"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {

void require(bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

} // namespace

int main() {
    const auto directory = std::filesystem::temp_directory_path()
        / ("fruity_prime_native_launcher_test_"
           + std::to_string(std::chrono::steady_clock::now()
                                .time_since_epoch().count()));
    try {
        std::filesystem::create_directories(directory);
        const auto rom = directory / "fixture.nds";
        {
            std::ofstream output(rom, std::ios::binary);
            output << "fixture";
        }
        {
            std::ofstream output(directory / "launcher.txt");
            output << "rom_path=" << rom.string() << '\n';
            output << "room=MP1 SANCTORUS\n"
                   << "player_name=Pilot\n"
                   << "hunter=3\n"
                   << "bots=2\n"
                   << "bot_level=2\n"
                   << "mode=4\n"
                   << "time_limit_seconds=90\n"
                   << "point_goal=11\n"
                   << "friendly_fire=true\n";
        }

        const auto selection = fruityprime::launcher::choose_game(
            directory, "UNIT1_C0", false);
        require(selection.has_value(), "saved ROM was not selected");
        require(selection->rom_path == rom.string(),
                "saved ROM path changed unexpectedly");
        require(selection->room_name == "MP1 SANCTORUS",
                "saved room was not selected");
        require(selection->player_name == "Pilot" && selection->hunter == 3
                    && selection->bots == 2 && selection->bot_level == 2,
                "saved player settings were not selected");
        require(selection->mode == 4
                    && selection->time_limit_seconds == 90.0F
                    && selection->point_goal == 11
                    && selection->friendly_fire,
                "saved match settings were not selected");

        std::ifstream preferences(directory / "launcher.txt");
        const std::string saved((std::istreambuf_iterator<char>(preferences)),
                                std::istreambuf_iterator<char>());
        require(saved.find("rom_path=" + rom.string()) != std::string::npos,
                "launcher did not persist the ROM path");
        require(saved.find("room=MP1 SANCTORUS") != std::string::npos,
                "launcher did not persist the room");
        require(saved.find("player_name=Pilot") != std::string::npos
                    && saved.find("hunter=3") != std::string::npos
                    && saved.find("bots=2") != std::string::npos
                    && saved.find("bot_level=2") != std::string::npos
                    && saved.find("mode=4") != std::string::npos
                    && saved.find("time_limit_seconds=90")
                        != std::string::npos
                    && saved.find("point_goal=11") != std::string::npos
                    && saved.find("friendly_fire=true") != std::string::npos,
                "launcher did not persist player settings");
        preferences.close();

        std::error_code cleanup_error;
        std::filesystem::remove_all(directory, cleanup_error);
        require(!std::filesystem::exists(directory),
                "launcher test directory could not be removed");
        std::cout << "native launcher tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::error_code cleanup_error;
        std::filesystem::remove_all(directory, cleanup_error);
        std::cerr << error.what() << '\n';
        return 1;
    }
}
