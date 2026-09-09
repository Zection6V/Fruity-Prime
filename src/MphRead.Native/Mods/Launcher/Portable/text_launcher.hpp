#pragma once

#include "Mods/Launcher/Portable/game_files.hpp"
#include "Mods/Launcher/Portable/launch_plan.hpp"
#include "Mods/Launcher/Portable/launcher_prefs.hpp"

#include <filesystem>
#include <iosfwd>
#include <optional>
#include <string>

namespace fruityprime::launcher {

struct TextLauncherOptions {
    std::filesystem::path executable_directory;
    std::istream* input = nullptr;
    std::ostream* output = nullptr;
};

// Terminal front end for platforms without the native window toolkit.  It
// owns only menu/setup state and returns a common LaunchPlan; MatchStart and
// the renderer remain the consumers of that plan.
class TextLauncher final {
public:
    explicit TextLauncher(TextLauncherOptions options);

    // nullopt means quit, EOF, or cancelled setup. A non-null plan has a
    // resolved hunter and can be handed to the shared match-start boundary.
    [[nodiscard]] std::optional<LaunchPlan> run();

private:
    [[nodiscard]] std::string ask(std::string prompt,
                                  std::string fallback);
    [[nodiscard]] bool setup_game_files();
    [[nodiscard]] bool settings();
    [[nodiscard]] std::optional<LaunchPlan> adventure();
    [[nodiscard]] std::optional<LaunchPlan> offline();
    [[nodiscard]] std::optional<LaunchPlan> online(bool host);
    [[nodiscard]] std::optional<std::string> choose_room();
    [[nodiscard]] std::uint8_t choose_mode();
    [[nodiscard]] metadata::Hunter choose_hunter();
    [[nodiscard]] std::string choose_name();
    [[nodiscard]] int choose_int(std::string prompt, int fallback,
                                 int minimum, int maximum);
    [[nodiscard]] bool choose_yes_no(std::string prompt, bool fallback);
    [[nodiscard]] bool parse_endpoint(std::string value,
                                       std::string& host,
                                       std::uint16_t& port) const;
    void refresh_status();
    void print_home() const;

    std::filesystem::path directory_;
    std::istream* input_ = nullptr;
    std::ostream* output_ = nullptr;
    Preferences preferences_;
    GameFilesStatus status_;
    bool input_closed_ = false;
};

} // namespace fruityprime::launcher
