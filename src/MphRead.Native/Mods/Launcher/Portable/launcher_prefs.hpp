#pragma once

#include "Metadata/metadata.hpp"
#include "Mods/window_mode.hpp"

#include <cstdint>
#include <filesystem>
#include <string>

namespace fruityprime::launcher {

using WindowStartMode = window::StartMode;

// launcher.txt-compatible values shared by the portable launcher and the
// native Win32 front end.  The last five fields are a native bridge for the
// current C++ match dialog; the managed launcher ignores unknown keys, so an
// existing installation can move between both implementations.
struct Preferences {
    static constexpr const char* DefaultServer = "89.160.162.50";
    static constexpr const char* DefaultMasterHost = "net.livetek.fr";

    std::string server_address = DefaultServer;
    std::uint16_t server_port = 27888;
    std::string master_host = DefaultMasterHost;
    std::uint16_t master_port = 27889;
    int last_role = 0;
    std::string player_name = "Player";
    metadata::Hunter last_hunter = metadata::Hunter::Samus;
    int bots = 3;
    int bot_level = 1;
    std::uint16_t host_port = 27888;
    bool list_hosted_game = true;
    bool host_on_master = true;
    int last_kind = 0;
    bool auto_update = true;
    bool debug_logs = false;
    WindowStartMode window_mode = WindowStartMode::Windowed;

    std::string rom_path;
    std::string room_name;
    std::uint8_t mode = 3;
    float time_limit_seconds = 7.0F * 60.0F;
    std::uint16_t point_goal = 7;
    bool friendly_fire = false;
};

[[nodiscard]] Preferences load_preferences(
    const std::filesystem::path& executable_directory);

// Preferences are convenience state.  A failed write is reported to the
// caller but must never make a playable ROM or server fail to start.
[[nodiscard]] bool save_preferences(
    const std::filesystem::path& executable_directory,
    const Preferences& preferences);

} // namespace fruityprime::launcher
