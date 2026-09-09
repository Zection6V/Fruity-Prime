#include "Mods/Launcher/Portable/text_launcher.hpp"

#include "Mods/Launcher/Portable/adventure_save.hpp"
#include "Mods/branding.hpp"
#include "Mods/Launcher/Portable/game_files.hpp"
#include "Mods/Launcher/Portable/launcher_prefs.hpp"
#include "Metadata/metadata.hpp"
#include "Entities/room_catalog.hpp"
#include "Mods/Launcher/Portable/setup_progress.hpp"
#include "Mods/window_mode.hpp"

#include <algorithm>
#include <array>
#include <charconv>
#include <cctype>
#include <iostream>
#include <limits>
#include <ostream>
#include <string_view>
#include <utility>

namespace fruityprime::launcher {
namespace {

constexpr std::array<std::uint8_t, 12> MultiplayerModes{{
    3, 4, 5, 6, 7, 8, 9, 12, 13, 10, 11, 14
}};

[[nodiscard]] std::string lower_ascii(std::string value) {
    for (char& character : value) {
        character = static_cast<char>(std::tolower(
            static_cast<unsigned char>(character)));
    }
    return value;
}

[[nodiscard]] std::string trim(std::string value) {
    const auto first = value.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) {
        return {};
    }
    const auto last = value.find_last_not_of(" \t\r\n");
    return value.substr(first, last - first + 1);
}

[[nodiscard]] std::string hunter_label(metadata::Hunter hunter) {
    if (hunter == metadata::Hunter::Random) {
        return "Random";
    }
    return std::string(metadata::hunter_info(
        static_cast<std::uint8_t>(hunter)).name);
}

[[nodiscard]] std::optional<int> parse_int(std::string_view text) {
    text = std::string_view(text.data(), text.size());
    int value = 0;
    const auto result = std::from_chars(text.data(), text.data() + text.size(),
                                        value);
    if (result.ec != std::errc{} || result.ptr != text.data() + text.size()) {
        return std::nullopt;
    }
    return value;
}

} // namespace

TextLauncher::TextLauncher(TextLauncherOptions options)
    : directory_(std::move(options.executable_directory)),
      input_(options.input != nullptr ? options.input : &std::cin),
      output_(options.output != nullptr ? options.output : &std::cout) {
    if (directory_.empty()) {
        directory_ = std::filesystem::current_path();
    }
}

void TextLauncher::refresh_status() {
    status_ = GameFiles::inspect(directory_);
}

std::string TextLauncher::ask(std::string prompt, std::string fallback) {
    *output_ << prompt;
    std::string line;
    if (!std::getline(*input_, line)) {
        input_closed_ = true;
        *output_ << '\n';
        return fallback;
    }
    line = trim(std::move(line));
    return line.empty() ? fallback : line;
}

void TextLauncher::print_home() const {
    *output_ << '\n' << "  " << branding::name_and_version() << '\n'
             << "  --------------------------------------------\n"
             << "  Game files : " << status_.describe() << '\n'
             << "  Player     : " << preferences_.player_name << " as "
             << hunter_label(preferences_.last_hunter) << "\n\n";
}

std::optional<LaunchPlan> TextLauncher::run() {
    preferences_ = load_preferences(directory_);
    refresh_status();
    while (!input_closed_) {
        if (!status_.ready) {
            print_home();
            *output_ << "  " << status_.problem << ".\n"
                     << "\n  [1] Game files       point this at your .nds dump\n"
                     << "  [q] Quit\n\n";
            const std::string choice = lower_ascii(ask("  Choose [1]: ", "1"));
            if (choice == "q" || choice == "quit") {
                return std::nullopt;
            }
            if (choice == "1" && setup_game_files()) {
                refresh_status();
            }
            continue;
        }

        print_home();
        *output_ << "  [1] Adventure        the story, from a save slot\n"
                 << "  [2] Play online      join a server\n"
                 << "  [3] Play offline     a match against bots\n"
                 << "  [4] Host a game      run a server and play on it\n"
                 << "  [5] Settings         name, hunter, window, addresses\n"
                 << "  [6] Game files       point this at your .nds dump\n"
                 << "  [q] Quit\n\n";
        const std::string choice = lower_ascii(ask("  Choose [1]: ", "1"));
        if (input_closed_ || choice == "q" || choice == "quit") {
            return std::nullopt;
        }
        if (choice == "5") {
            (void)settings();
            continue;
        }
        if (choice == "6") {
            if (setup_game_files()) {
                refresh_status();
            }
            continue;
        }

        std::optional<LaunchPlan> plan;
        if (choice == "1") {
            plan = adventure();
        } else if (choice == "2") {
            plan = online(false);
        } else if (choice == "3") {
            plan = offline();
        } else if (choice == "4") {
            plan = online(true);
        }
        if (plan.has_value()) {
            return plan;
        }
    }
    return std::nullopt;
}

bool TextLauncher::setup_game_files() {
    *output_ << "\n  " << branding::Name
             << " needs your own Metroid Prime Hunters cartridge dump.\n"
             << "  It unpacks what it needs next to this program and leaves the"
                " file alone.\n"
             << "  No game data is included or downloaded.\n\n";
    std::string path = ask("  Path to the .nds file (blank to cancel): ", {});
    if (input_closed_ || path.empty()) {
        return false;
    }
    if ((path.front() == '"' && path.back() == '"')
        || (path.front() == '\'' && path.back() == '\'')) {
        path = path.substr(1, path.size() - 2);
    }
    if (!std::filesystem::is_regular_file(path)) {
        *output_ << "  There is no file at " << path << "\n";
        return false;
    }
    SetupProgress progress;
    const auto report = [this, &progress](std::string_view line) {
        if (progress.observe(line)) {
            *output_ << "  " << progress.bar() << "  "
                     << progress.stage() << '\n';
        }
    };
    const auto result = GameFiles::run_setup(directory_, path, report);
    progress.finish(result.ok);
    *output_ << (result.ok ? "  Ready to play.\n" : "  Setup did not finish: ")
             << (result.ok ? std::string{} : result.error) << '\n';
    return result.ok;
}

bool TextLauncher::settings() {
    *output_ << "\n  Settings\n  --------------------------------------------\n";
    preferences_.player_name = choose_name();
    preferences_.last_hunter = choose_hunter();
    const bool fullscreen = choose_yes_no(
        "  Start fullscreen (y/n)",
        preferences_.window_mode == WindowStartMode::BorderlessFullscreen);
    preferences_.window_mode = fullscreen
        ? WindowStartMode::BorderlessFullscreen : WindowStartMode::Windowed;
    std::string endpoint = ask(
        "  Default server [" + preferences_.server_address + ":"
            + std::to_string(preferences_.server_port) + "]: ",
        preferences_.server_address + ":"
            + std::to_string(preferences_.server_port));
    if (!parse_endpoint(std::move(endpoint), preferences_.server_address,
                        preferences_.server_port)) {
        *output_ << "  Default server was left unchanged.\n";
    }
    std::string master = ask(
        "  Server directory [" + preferences_.master_host + ":"
            + std::to_string(preferences_.master_port) + "]: ",
        preferences_.master_host + ":"
            + std::to_string(preferences_.master_port));
    if (!parse_endpoint(std::move(master), preferences_.master_host,
                        preferences_.master_port)) {
        *output_ << "  Server directory was left unchanged.\n";
    }
    const bool saved = save_preferences(directory_, preferences_);
    *output_ << (saved ? "  Saved.\n" : "  Could not save settings.\n");
    return saved;
}

std::optional<LaunchPlan> TextLauncher::adventure() {
    SaveStore saves(directory_);
    const auto slots = saves.read_all();
    *output_ << "\n  Adventure\n  --------------------------------------------\n";
    for (std::size_t index = 0; index < slots.size(); ++index) {
        *output_ << "  [" << index + 1 << "] Slot "
                 << static_cast<unsigned>(slots[index].slot) << "  "
                 << slots[index].describe() << '\n';
    }
    const std::string choice = lower_ascii(ask("  Choose a slot [1]: ", "1"));
    if (input_closed_ || choice == "b" || choice == "back") {
        return std::nullopt;
    }
    const auto parsed = parse_int(choice);
    if (!parsed.has_value() || *parsed < 1
        || *parsed > static_cast<int>(slots.size())) {
        return std::nullopt;
    }
    const SlotInfo& slot = slots[static_cast<std::size_t>(*parsed - 1)];
    bool new_game = !slot.used;
    if (slot.used) {
        const std::string what = lower_ascii(
            ask("  [1] Continue  [2] New game  [b] Back [1]: ", "1"));
        if (what == "b" || what == "back") {
            return std::nullopt;
        }
        if (what == "2") {
            new_game = true;
        } else if (what != "1") {
            return std::nullopt;
        }
    }
    // Keep the launch plan independent from the save implementation, but
    // resolve the same checkpoint room the managed story launcher would use.
    // A new game starts from StorySave's default checkpoint, while Continue
    // reads the selected PascalCase JSON slot before the plan crosses into
    // the common match-start boundary.
    const auto save = new_game
        ? game::StorySave{}
        : saves.read(slot.slot).value_or(game::StorySave{});
    LaunchPlan plan;
    plan.kind = LaunchKind::Adventure;
    plan.hunter = choose_hunter();
    plan.room_key = SaveStore::start_room(save);
    plan.mode = static_cast<std::uint8_t>(game::Mode::Story);
    plan.player_name = preferences_.player_name;
    plan.save_slot = slot.slot;
    plan.new_game = new_game;
    Hunters choices;
    plan.resolve_hunter(choices);
    preferences_.last_kind = static_cast<int>(LaunchKind::Adventure);
    (void)save_preferences(directory_, preferences_);
    return plan;
}

std::optional<std::string> TextLauncher::choose_room() {
    const auto& rooms = scene::multiplayer_rooms();
    if (rooms.empty()) {
        *output_ << "  No multiplayer rooms were found.\n";
        return std::nullopt;
    }
    std::size_t current = 0;
    for (std::size_t index = 0; index < rooms.size(); ++index) {
        if (rooms[index].name == preferences_.room_name) {
            current = index;
            break;
        }
    }
    *output_ << '\n';
    for (std::size_t index = 0; index < rooms.size(); ++index) {
        *output_ << "  [" << index + 1 << "] " << rooms[index].name << '\n';
    }
    const std::string choice = ask(
        "  Map [" + std::to_string(current + 1) + "]: ",
        std::to_string(current + 1));
    const auto parsed = parse_int(choice);
    if (!parsed.has_value() || *parsed < 1
        || *parsed > static_cast<int>(rooms.size())) {
        return std::nullopt;
    }
    preferences_.room_name = rooms[static_cast<std::size_t>(*parsed - 1)].name;
    return preferences_.room_name;
}

std::uint8_t TextLauncher::choose_mode() {
    std::size_t current = 0;
    for (std::size_t index = 0; index < MultiplayerModes.size(); ++index) {
        if (MultiplayerModes[index] == preferences_.mode) {
            current = index;
            break;
        }
    }
    *output_ << "\n";
    for (std::size_t index = 0; index < MultiplayerModes.size(); ++index) {
        *output_ << "  [" << index + 1 << "] "
                 << metadata::game_mode_name(MultiplayerModes[index]) << '\n';
    }
    const auto parsed = parse_int(ask(
        "  Mode [" + std::to_string(current + 1) + "]: ",
        std::to_string(current + 1)));
    if (!parsed.has_value() || *parsed < 1
        || *parsed > static_cast<int>(MultiplayerModes.size())) {
        return preferences_.mode;
    }
    return MultiplayerModes[static_cast<std::size_t>(*parsed - 1)];
}

metadata::Hunter TextLauncher::choose_hunter() {
    *output_ << "\n  Hunters: [1] Samus [2] Kanden [3] Trace [4] Sylux"
             << " [5] Noxus [6] Spire [7] Weavel [8] Random\n";
    const std::string fallback = std::to_string(
        static_cast<unsigned>(preferences_.last_hunter)
            == static_cast<unsigned>(metadata::Hunter::Random)
            ? 8 : static_cast<unsigned>(preferences_.last_hunter) + 1);
    const std::string answer = ask("  Hunter [" + fallback + "]: ", fallback);
    const auto parsed = parse_int(answer);
    metadata::Hunter result = preferences_.last_hunter;
    if (parsed.has_value() && *parsed >= 1 && *parsed <= 8) {
        result = static_cast<metadata::Hunter>(*parsed - 1);
        if (*parsed == 8) {
            result = metadata::Hunter::Random;
        }
    } else {
        const auto named = metadata::parse_hunter(answer);
        if (named.has_value()) {
            result = static_cast<metadata::Hunter>(*named);
        }
    }
    preferences_.last_hunter = result;
    return result;
}

std::string TextLauncher::choose_name() {
    const std::string value = ask("  Your name [" + preferences_.player_name
                                      + "]: ", preferences_.player_name);
    if (!value.empty()) {
        preferences_.player_name = value;
    }
    return preferences_.player_name;
}

int TextLauncher::choose_int(std::string prompt, int fallback,
                             int minimum, int maximum) {
    const auto parsed = parse_int(ask(std::move(prompt) + " ["
                                          + std::to_string(fallback) + "]: ",
                                      std::to_string(fallback)));
    return parsed.has_value()
        ? std::clamp(*parsed, minimum, maximum) : fallback;
}

bool TextLauncher::choose_yes_no(std::string prompt, bool fallback) {
    std::string answer = lower_ascii(ask(std::move(prompt), fallback ? "y" : "n"));
    return !answer.empty() ? answer.front() == 'y' : fallback;
}

std::optional<LaunchPlan> TextLauncher::offline() {
    const auto room = choose_room();
    if (!room.has_value()) {
        return std::nullopt;
    }
    const std::uint8_t mode = choose_mode();
    const int bots = choose_int("  Bots (0-7)", preferences_.bots, 0, 7);
    const int level = choose_int("  Bot skill (0 easy, 1 normal, 2 hard)",
                                preferences_.bot_level, 0, 2);
    LaunchPlan plan;
    plan.kind = LaunchKind::Offline;
    plan.hunter = choose_hunter();
    plan.room_key = *room;
    plan.mode = mode;
    plan.bots = bots;
    plan.bot_level = level;
    plan.player_name = preferences_.player_name;
    Hunters choices;
    plan.resolve_hunter(choices);
    preferences_.mode = mode;
    preferences_.bots = bots;
    preferences_.bot_level = level;
    preferences_.last_kind = static_cast<int>(LaunchKind::Offline);
    (void)save_preferences(directory_, preferences_);
    return plan;
}

std::optional<LaunchPlan> TextLauncher::online(bool host) {
    const auto room = choose_room();
    if (!room.has_value()) {
        return std::nullopt;
    }
    LaunchPlan plan;
    plan.kind = host ? LaunchKind::Host : LaunchKind::Online;
    plan.hunter = choose_hunter();
    plan.room_key = *room;
    plan.mode = choose_mode();
    plan.player_name = choose_name();
    plan.server_address = preferences_.server_address;
    plan.master_host = preferences_.master_host;
    plan.port = host ? preferences_.host_port : preferences_.server_port;
    if (host) {
        plan.port = choose_int("  Host port", preferences_.host_port, 1, 65535);
        preferences_.host_port = static_cast<std::uint16_t>(plan.port);
        preferences_.list_hosted_game = choose_yes_no(
            "  List it so others can find it (y/n)",
            preferences_.list_hosted_game);
    } else {
        std::string endpoint = ask(
            "  Server [" + preferences_.server_address + ":"
                + std::to_string(preferences_.server_port) + "]: ",
            preferences_.server_address + ":"
                + std::to_string(preferences_.server_port));
        if (!parse_endpoint(std::move(endpoint), plan.server_address,
                            preferences_.server_port)) {
            plan.server_address = preferences_.server_address;
        }
        plan.port = preferences_.server_port;
    }
    Hunters choices;
    plan.resolve_hunter(choices);
    preferences_.mode = plan.mode;
    preferences_.room_name = plan.room_key;
    preferences_.last_kind = static_cast<int>(plan.kind);
    (void)save_preferences(directory_, preferences_);
    return plan;
}

bool TextLauncher::parse_endpoint(std::string value, std::string& host,
                                  std::uint16_t& port) const {
    value = trim(std::move(value));
    if (value.empty()) {
        return false;
    }
    const std::size_t colon = value.rfind(':');
    if (colon == std::string::npos) {
        host = std::move(value);
        return !host.empty();
    }
    if (colon == 0 || colon + 1 >= value.size()) {
        return false;
    }
    const auto parsed = parse_int(std::string_view(value).substr(colon + 1));
    if (!parsed.has_value() || *parsed < 1 || *parsed > 65535) {
        return false;
    }
    host = value.substr(0, colon);
    port = static_cast<std::uint16_t>(*parsed);
    return !host.empty();
}

} // namespace fruityprime::launcher
