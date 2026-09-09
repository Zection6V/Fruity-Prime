#pragma once

#include <filesystem>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace fruityprime::launcher {

struct Selection {
    std::string rom_path;
    std::string room_name;
    std::string player_name = "Player";
    std::uint8_t hunter = 0;
    int bots = 0;
    int bot_level = 1;
    std::uint8_t mode = 3; // GameMode.Battle
    float time_limit_seconds = 7.0F * 60.0F;
    std::uint16_t point_goal = 7;
    bool friendly_fire = false;

    // Set by the launcher when the story was chosen rather than a
    // match.  The caller applies these the way -adventure, -save-slot
    // and -newgame would: the room, mode and limits all come from the
    // save rather than from the rows above.
    bool adventure = false;
    std::uint8_t save_slot = 1;
    bool new_game = false;

    // Set by the launcher when a server was picked rather than a match
    // started here.  The caller applies these the way -connect and
    // -port would.
    std::string connect_host;
    std::uint16_t connect_port = 0;

    // Set by the launcher when a recording was chosen.  The caller
    // applies it the way -replay FILE would.
    std::string demo_path;
};

// Native launcher selection. The optional interactive flag opens the Win32
// front screen; tests and headless callers can leave it false and use the
// saved selection without creating a window.
[[nodiscard]] std::optional<Selection> choose_game(
    const std::filesystem::path& executable_directory,
    std::string_view default_room = "UNIT1_C0",
    bool interactive = false, Selection defaults = {});

} // namespace fruityprime::launcher
