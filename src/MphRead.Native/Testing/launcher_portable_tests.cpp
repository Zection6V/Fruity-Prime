#include "Mods/Launcher/Portable/game_files.hpp"
#include "Mods/Launcher/Portable/launcher_prefs.hpp"
#include "Mods/Launcher/Portable/launch_plan.hpp"
#include "Mods/Launcher/Portable/setup_progress.hpp"
#include "Utility/extract.hpp"

#include <zlib.h>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace {

void require(bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

void put_u32(std::vector<std::uint8_t>& bytes, std::size_t offset,
             std::uint32_t value) {
    bytes[offset] = static_cast<std::uint8_t>(value);
    bytes[offset + 1] = static_cast<std::uint8_t>(value >> 8);
    bytes[offset + 2] = static_cast<std::uint8_t>(value >> 16);
    bytes[offset + 3] = static_cast<std::uint8_t>(value >> 24);
}

void put_u16(std::vector<std::uint8_t>& bytes, std::size_t offset,
             std::uint16_t value) {
    bytes[offset] = static_cast<std::uint8_t>(value);
    bytes[offset + 1] = static_cast<std::uint8_t>(value >> 8);
}

void put_text(std::vector<std::uint8_t>& bytes, std::size_t offset,
              std::string_view value) {
    std::copy(value.begin(), value.end(), bytes.begin() + offset);
}

void write_test_rom(const std::filesystem::path& path) {
    std::vector<std::uint8_t> bytes(0xB40, 0);
    put_text(bytes, 0, "SETUP TEST");
    put_text(bytes, 0x0C, "AMFE");
    put_text(bytes, 0x10, "01");
    put_u32(bytes, 0x20, 0x100);
    put_u32(bytes, 0x2C, 8);
    put_u32(bytes, 0x30, 0x110);
    put_u32(bytes, 0x3C, 4);
    put_u32(bytes, 0x40, 0x160);
    put_u32(bytes, 0x44, 0x20);
    put_u32(bytes, 0x48, 0x180);
    put_u32(bytes, 0x4C, 8);
    put_u32(bytes, 0x50, 0x120);
    put_u32(bytes, 0x54, 32);
    put_u32(bytes, 0x68, 0x300);

    put_text(bytes, 0x100, "ARM9");
    put_u32(bytes, 0x104, 0);
    put_text(bytes, 0x110, "ARM7");
    put_u32(bytes, 0x120, 2);
    put_u32(bytes, 0x120 + 24, 0);

    put_u32(bytes, 0x160, 8);
    put_u16(bytes, 0x164, 0);
    put_u16(bytes, 0x166, 1);
    bytes[0x168] = 9;
    put_text(bytes, 0x169, "asset.bin");
    bytes[0x172] = 0;
    put_u32(bytes, 0x180, 0x200);
    put_u32(bytes, 0x184, 0x209);
    put_text(bytes, 0x200, "OVRLY");
    put_u32(bytes, 0x205, 0);

    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    output.write(reinterpret_cast<const char*>(bytes.data()),
                 static_cast<std::streamsize>(bytes.size()));
}

std::vector<std::uint8_t> read_bytes(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(input),
            std::istreambuf_iterator<char>()};
}

void write_test_sdat(const std::filesystem::path& path) {
    std::vector<std::uint8_t> bytes(0xC0, 0);
    put_text(bytes, 0, "SDAT");
    put_u32(bytes, 0x10, 0x40);
    put_u32(bytes, 0x18, 0x80);

    put_text(bytes, 0x40, "SYMB");
    put_u32(bytes, 0x48, 0x28);
    put_u32(bytes, 0x68, 2);
    put_u32(bytes, 0x6C, 0x34);
    put_u32(bytes, 0x70, 0);
    put_text(bytes, 0x74, "TITLE");

    put_text(bytes, 0x80, "INFO");
    put_u32(bytes, 0x88, 0x28);
    put_u32(bytes, 0xA8, 2);
    put_u32(bytes, 0xAC, 0x34);
    put_u32(bytes, 0xB0, 0);
    put_u32(bytes, 0xB4, 7);

    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    output.write(reinterpret_cast<const char*>(bytes.data()),
                 static_cast<std::streamsize>(bytes.size()));
}

std::uint32_t get_u32(const std::vector<std::uint8_t>& bytes,
                      std::size_t offset) {
    return static_cast<std::uint32_t>(bytes.at(offset))
        | static_cast<std::uint32_t>(bytes.at(offset + 1)) << 8
        | static_cast<std::uint32_t>(bytes.at(offset + 2)) << 16
        | static_cast<std::uint32_t>(bytes.at(offset + 3)) << 24;
}

void test_sdat_conversion(const std::filesystem::path& directory) {
    const auto input = directory / "sound_data.sdat";
    const auto output = directory / "seq";
    write_test_sdat(input);
    require(fruityprime::utility::extract::convert_sdat(input, output) == 1,
            "Extract.ConvertSdat returned the wrong sequence count");

    const auto library = read_bytes(output / "mph.ncsflib");
    require(library.size() > 16 && library[0] == 'P' && library[1] == 'S'
                && library[2] == 'F' && library[3] == 0x25
                && get_u32(library, 4) == 0,
            "Extract.ConvertSdat wrote an invalid NCSF library header");
    const std::uint32_t compressed_size = get_u32(library, 8);
    require(library.size() == 16 + compressed_size,
            "Extract.ConvertSdat wrote an invalid NCSF program size");
    require(get_u32(library, 12)
                == static_cast<std::uint32_t>(crc32(
                    crc32(0L, Z_NULL, 0), library.data() + 16,
                    compressed_size)),
            "Extract.ConvertSdat wrote the wrong NCSF CRC32");
    std::vector<std::uint8_t> restored(0xC0);
    uLongf restored_size = restored.size();
    require(uncompress(restored.data(), &restored_size, library.data() + 16,
                       compressed_size) == Z_OK
                && restored_size == restored.size()
                && restored == read_bytes(input),
            "Extract.ConvertSdat did not preserve the SDAT program");

    const auto mini = read_bytes(output / "0000 - TITLE.minincsf");
    require(mini.size() > 20 && mini[3] == 0x25
                && get_u32(mini, 4) == 4 && get_u32(mini, 8) == 0
                && get_u32(mini, 12) == 0 && get_u32(mini, 16) == 0,
            "Extract.ConvertSdat wrote an invalid miniNCSF header");
    const std::string tags(mini.begin() + 20, mini.end());
    require(tags == "[TAG]_lib=mph.ncsflib\nutf8=1\n"
                    "ncsfby=MphRead\norigFilename=TITLE\n",
            "Extract.ConvertSdat did not preserve managed NCSF tags");
}

} // namespace

int main() {
    const auto directory = std::filesystem::temp_directory_path()
        / ("fruity_prime_native_launcher_portable_test_"
           + std::to_string(std::chrono::steady_clock::now()
                                .time_since_epoch().count()));
    try {
        // Cleared first, not just created: steady_clock is coarse enough
        // on Windows that two runs in the same tick pick the same name,
        // and a launcher.txt left by the previous one makes this run read
        // its answers instead of the defaults.
        std::error_code stale;
        std::filesystem::remove_all(directory, stale);
        std::filesystem::create_directories(directory);
        test_sdat_conversion(directory);

        {
            std::ofstream output(directory / "launcher.txt");
            output << "# managed launcher preferences\n"
                   << "server_address=192.0.2.20\n"
                   << "server_port=28000\n"
                   << "master_host=directory.example\n"
                   << "master_port=28001\n"
                   << "last_role=2\n"
                   << "player_name=Pilot\n"
                   << "hunter=Random\n"
                   << "bots=6\n"
                   << "bot_level=2\n"
                   << "host_port=28002\n"
                   << "list_hosted=false\n"
                   << "host_on_master=false\n"
                   << "last_kind=4\n"
                   << "auto_update=false\n"
                   << "debug_logs=true\n"
                   << "window_mode=borderless\n"
                   << "server_port=not-a-port\n"
                   << "rom_path=C:/test.nds\n"
                   << "room=UNIT1_C0\n";
        }
        const auto preferences = fruityprime::launcher::load_preferences(directory);
        require(preferences.server_address == "192.0.2.20"
                    && preferences.server_port == 28000
                    && preferences.master_host == "directory.example"
                    && preferences.master_port == 28001,
                "managed network preferences did not load");
        require(preferences.player_name == "Pilot"
                    && preferences.last_hunter
                           == fruityprime::metadata::Hunter::Random
                    && preferences.bots == 6 && preferences.bot_level == 2,
                "managed player preferences did not load");
        require(!preferences.list_hosted_game && !preferences.host_on_master
                    && preferences.last_kind == 4 && !preferences.auto_update
                    && preferences.debug_logs
                    && preferences.window_mode
                           == fruityprime::launcher::WindowStartMode::BorderlessFullscreen,
                "managed launcher flags did not load");
        require(preferences.server_port == 28000,
                "malformed preference overwrote a valid port");
        require(preferences.rom_path == "C:/test.nds"
                    && preferences.room_name == "UNIT1_C0",
                "native launcher compatibility keys did not load");

        auto saved = preferences;
        saved.last_hunter = fruityprime::metadata::Hunter::Weavel;
        require(fruityprime::launcher::save_preferences(directory, saved),
                "launcher preferences could not be saved");
        const auto reloaded = fruityprime::launcher::load_preferences(directory);
        require(reloaded.last_hunter == fruityprime::metadata::Hunter::Weavel,
                "saved hunter preference did not round-trip");
        std::ifstream saved_file(directory / "launcher.txt");
        const std::string saved_text((std::istreambuf_iterator<char>(saved_file)),
                                     std::istreambuf_iterator<char>());
        require(saved_text.find("server_address=192.0.2.20")
                    != std::string::npos
                    && saved_text.find("hunter=Weavel") != std::string::npos,
                "launcher preferences were not written in managed form");
        saved_file.close();

        fruityprime::formats::Paths paths;
        const auto game_directory = directory / "files" / "AMHE1";
        std::filesystem::create_directories(game_directory);
        (void)paths.set_path("AMHE1", game_directory);
        paths.choose_mph_path();
        require(paths.mph_key == "AMHE1",
                "paths did not choose the newest MPH entry");
        require(paths.write(directory), "paths.txt could not be written");
        const auto status = fruityprime::launcher::GameFiles::inspect(directory);
        require(status.ready && status.mph_key == "AMHE1"
                    && status.mph_file_system == game_directory,
                "game-file readiness did not use paths.txt");
        require(status.describe() == "Ready -- AMHE1",
                "game-file readiness description changed");

        {
            std::ofstream output(directory / "paths.txt", std::ios::trunc);
            output << "0.18.0.0\nAMHE1=" << game_directory.generic_string()
                   << '\n';
        }
        const auto old_status = fruityprime::launcher::GameFiles::inspect(directory);
        require(!old_status.ready && old_status.problem.find("older version")
                    != std::string::npos,
                "old paths.txt was accepted");
        const auto missing_setup = fruityprime::launcher::GameFiles::run_setup(
            directory, directory / "missing.nds");
        require(!missing_setup.ok && missing_setup.error.find("does not exist")
                    != std::string::npos,
                "missing ROM setup did not fail cleanly");

        const auto setup_rom = directory / "setup.nds";
        write_test_rom(setup_rom);
        const auto setup_root = directory / "setup-root";
        const auto setup = fruityprime::launcher::GameFiles::run_setup(
            setup_root, setup_rom);
        require(setup.ok && setup.first_hunt && setup.game_key == "AMFE0",
                "First Hunt Extract.Setup did not complete");
        require(std::filesystem::file_size(
                    setup.game_root / "_bin" / "arm9.bin") == 4
                    && std::filesystem::file_size(
                        setup.game_root / "_bin" / "overlay9_2") == 5,
                "Extract.Setup did not reproduce ARM9/overlay decompression");
        require(std::filesystem::exists(setup.game_root / "ftc" / "fnt.bin")
                    && std::filesystem::exists(setup_root / "paths.txt"),
                "Extract.Setup did not write FTC data or paths.txt");

        fruityprime::launcher::SetupProgress progress;
        require(progress.observe("Writing files/AMHE1/data"),
                "setup progress did not enter file phase");
        const double file_fraction = progress.fraction();
        (void)progress.observe("Reading game archive...");
        require(progress.fraction() >= file_fraction
                    && progress.stage() == "Unpacking archives",
                "setup progress moved backwards between phases");
        (void)progress.observe("[thumbnails] 8/32 ...");
        require(progress.fraction() >= 0.79
                    && progress.stage() == "Rendering map previews (8/32)",
                "setup preview progress was not parsed");
        progress.finish(true);
        require(progress.done() && progress.fraction() == 1.0
                    && progress.bar().find("100%") != std::string::npos,
                "setup progress did not finish");

        fruityprime::launcher::Hunters choices(1234);
        fruityprime::launcher::LaunchPlan plan;
        plan.hunter = fruityprime::metadata::Hunter::Random;
        plan.resolve_hunter(choices);
        require(plan.hunter != fruityprime::metadata::Hunter::Random
                    && static_cast<std::uint8_t>(plan.hunter)
                           < fruityprime::metadata::PlayableHunterCount,
                "random launch hunter was not resolved");
        const auto first_roll = plan.hunter;
        plan.hunter = fruityprime::metadata::Hunter::Random;
        plan.resolve_hunter(choices);
        require(plan.hunter == first_roll,
                "random launch hunter was rolled more than once");

        std::error_code cleanup_error;
        std::filesystem::remove_all(directory, cleanup_error);
        require(!std::filesystem::exists(directory),
                "portable launcher test directory could not be removed");
        std::cout << "native portable launcher tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::error_code cleanup_error;
        std::filesystem::remove_all(directory, cleanup_error);
        std::cerr << error.what() << '\n';
        return 1;
    }
}
