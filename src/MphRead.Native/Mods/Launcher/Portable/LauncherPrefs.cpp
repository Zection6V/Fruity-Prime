#include "Mods/Launcher/Portable/launcher_prefs.hpp"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <cctype>
#include <fstream>
#include <limits>
#include <string_view>
#include <utility>

namespace fruityprime::launcher {
namespace {

[[nodiscard]] std::string trim_copy(std::string value) {
    const std::size_t first = value.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) {
        return {};
    }
    const std::size_t last = value.find_last_not_of(" \t\r\n");
    return value.substr(first, last - first + 1);
}

[[nodiscard]] std::string lower_ascii(std::string value) {
    for (char& character : value) {
        character = static_cast<char>(std::tolower(
            static_cast<unsigned char>(character)));
    }
    return value;
}

template <typename Integer>
[[nodiscard]] bool parse_integer(std::string_view value, Integer& output) {
    const auto result = std::from_chars(value.data(),
                                         value.data() + value.size(), output);
    return result.ec == std::errc{}
        && result.ptr == value.data() + value.size();
}

[[nodiscard]] bool parse_port(std::string_view value, std::uint16_t& output) {
    int parsed = 0;
    if (!parse_integer(value, parsed) || parsed < 1 || parsed > 65'535) {
        return false;
    }
    output = static_cast<std::uint16_t>(parsed);
    return true;
}

[[nodiscard]] bool parse_bool(std::string_view value, bool& output) {
    const std::string normalized = lower_ascii(trim_copy(std::string(value)));
    if (normalized == "true" || normalized == "1" || normalized == "yes"
        || normalized == "on") {
        output = true;
        return true;
    }
    if (normalized == "false" || normalized == "0" || normalized == "no"
        || normalized == "off") {
        output = false;
        return true;
    }
    return false;
}

[[nodiscard]] bool parse_float(std::string_view value, float& output) {
    try {
        std::size_t consumed = 0;
        const std::string text = trim_copy(std::string(value));
        const float parsed = std::stof(text, &consumed);
        if (consumed != text.size() || !std::isfinite(parsed)) {
            return false;
        }
        output = parsed;
        return true;
    } catch (...) {
        return false;
    }
}

[[nodiscard]] std::string hunter_name(metadata::Hunter hunter) {
    if (hunter == metadata::Hunter::Random) {
        return "Random";
    }
    return std::string(metadata::hunter_info(
        static_cast<std::uint8_t>(hunter)).name);
}

} // namespace

Preferences load_preferences(const std::filesystem::path& directory) {
    Preferences result;
    std::ifstream input(directory / "launcher.txt");
    if (!input) {
        return result;
    }
    try {
        std::string raw;
        while (std::getline(input, raw)) {
            const std::string line = trim_copy(std::move(raw));
            if (line.empty() || line.front() == '#') {
                continue;
            }
            const std::size_t split = line.find('=');
            if (split == std::string::npos || split == 0) {
                continue;
            }
            const std::string key = trim_copy(line.substr(0, split));
            const std::string value = trim_copy(line.substr(split + 1));
            if (key == "server_address") {
                result.server_address = value;
            } else if (key == "server_port") {
                (void)parse_port(value, result.server_port);
            } else if (key == "master_host") {
                if (!value.empty()) result.master_host = value;
            } else if (key == "master_port") {
                (void)parse_port(value, result.master_port);
            } else if (key == "last_role") {
                (void)parse_integer(value, result.last_role);
            } else if (key == "player_name") {
                if (!value.empty()) result.player_name = value;
            } else if (key == "hunter") {
                if (const auto parsed = metadata::parse_hunter(value)) {
                    result.last_hunter = static_cast<metadata::Hunter>(*parsed);
                }
            } else if (key == "bots") {
                (void)parse_integer(value, result.bots);
            } else if (key == "bot_level") {
                (void)parse_integer(value, result.bot_level);
            } else if (key == "host_port") {
                (void)parse_port(value, result.host_port);
            } else if (key == "list_hosted") {
                (void)parse_bool(value, result.list_hosted_game);
            } else if (key == "host_on_master") {
                (void)parse_bool(value, result.host_on_master);
            } else if (key == "last_kind") {
                (void)parse_integer(value, result.last_kind);
            } else if (key == "auto_update") {
                (void)parse_bool(value, result.auto_update);
            } else if (key == "debug_logs") {
                (void)parse_bool(value, result.debug_logs);
            } else if (key == "window_mode") {
                result.window_mode = window::parse_start_mode(
                    value, result.window_mode);
            } else if (key == "rom_path") {
                result.rom_path = value;
            } else if (key == "room") {
                if (!value.empty()) result.room_name = value;
            } else if (key == "mode") {
                int parsed = 0;
                if (parse_integer(value, parsed) && parsed >= 0 && parsed <= 255) {
                    result.mode = static_cast<std::uint8_t>(parsed);
                }
            } else if (key == "time_limit_seconds") {
                (void)parse_float(value, result.time_limit_seconds);
            } else if (key == "point_goal") {
                int parsed = 0;
                if (parse_integer(value, parsed) && parsed >= 0
                    && parsed <= 65'535) {
                    result.point_goal = static_cast<std::uint16_t>(parsed);
                }
            } else if (key == "friendly_fire") {
                (void)parse_bool(value, result.friendly_fire);
            }
        }
    } catch (...) {
        // A launcher preference is never allowed to prevent startup.
    }
    return result;
}

bool save_preferences(const std::filesystem::path& directory,
                      const Preferences& preferences) {
    std::error_code error;
    std::filesystem::create_directories(directory, error);
    if (error) {
        return false;
    }
    std::ofstream output(directory / "launcher.txt", std::ios::trunc);
    if (!output) {
        return false;
    }
    output << "# Fruity Prime launcher preferences.\n"
           << "server_address=" << preferences.server_address << '\n'
           << "server_port=" << preferences.server_port << '\n'
           << "master_host=" << preferences.master_host << '\n'
           << "master_port=" << preferences.master_port << '\n'
           << "last_role=" << preferences.last_role << '\n'
           << "player_name=" << preferences.player_name << '\n'
           << "hunter=" << hunter_name(preferences.last_hunter) << '\n'
           << "# Legacy native hunter id: hunter="
           << static_cast<int>(preferences.last_hunter) << '\n'
           << "bots=" << preferences.bots << '\n'
           << "bot_level=" << preferences.bot_level << '\n'
           << "host_port=" << preferences.host_port << '\n'
           << "list_hosted=" << (preferences.list_hosted_game ? "true" : "false")
           << '\n'
           << "host_on_master=" << (preferences.host_on_master ? "true" : "false")
           << '\n'
           << "last_kind=" << preferences.last_kind << '\n'
           << "auto_update=" << (preferences.auto_update ? "true" : "false")
           << '\n'
           << "debug_logs=" << (preferences.debug_logs ? "true" : "false")
           << '\n'
           << "window_mode=" << window::start_mode_name(
                                  preferences.window_mode) << '\n'
           << "# Native match dialog compatibility keys.\n"
           << "rom_path=" << preferences.rom_path << '\n'
           << "room=" << preferences.room_name << '\n'
           << "mode=" << static_cast<int>(preferences.mode) << '\n'
           << "time_limit_seconds=" << preferences.time_limit_seconds << '\n'
           << "point_goal=" << preferences.point_goal << '\n'
           << "friendly_fire=" << (preferences.friendly_fire ? "true" : "false")
           << '\n';
    return static_cast<bool>(output);
}

} // namespace fruityprime::launcher
