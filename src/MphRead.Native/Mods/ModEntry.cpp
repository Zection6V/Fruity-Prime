#include "Mods/mod_entry.hpp"

#include "Assets/game_assets.hpp"
#include "Entities/scene.hpp"
#include "Formats/demo_info.hpp"
#include "Formats/paths.hpp"
#include "Features.hpp"
#include "Mods/InputSettings.hpp"
#include "Mods/Input/gamepad_probe.hpp"
#include "Mods/Launcher/Portable/launcher_prefs.hpp"
#include "Mods/MapGen/custom_rooms.hpp"
#include "Mods/MapGen/map_bundle.hpp"
#include "Mods/MapGen/map_report.hpp"
#include "Mods/MapGen/mapgen.hpp"
#include "Mods/MapGen/q3_import.hpp"
#include "Mods/Network/dedicated_server.hpp"
#include "Mods/Network/map_audit.hpp"
#include "Mods/Network/master_client.hpp"
#include "Mods/Network/master_server.hpp"
#include "Mods/Network/mechanics_dump.hpp"
#include "Mods/Network/net_diagnostics.hpp"
#include "Mods/Network/net_status.hpp"
#include "Mods/Network/net_lag.hpp"
#include "Mods/Network/net_protocol.hpp"
#include "Mods/Network/map_rotation.hpp"
#include "Mods/Network/net_transport.hpp"
#include "Mods/Update/update.hpp"
#include "Mods/credits.hpp"
#include "Mods/debug_log.hpp"
#include "Mods/render_options.hpp"
#include "Mods/shutdown_signals.hpp"
#include "Mods/thumbnail_generator.hpp"
#include "Mods/window_mode.hpp"
#include "Program.hpp"
#include "Utility/console_setup.hpp"

#include <algorithm>
#include <charconv>
#include <chrono>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <limits>
#include <locale>
#include <memory>
#include <ostream>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace fruityprime::mods {
namespace {

[[nodiscard]] std::string_view without_dashes(std::string_view value) noexcept {
    while (!value.empty() && value.front() == '-') {
        value.remove_prefix(1);
    }
    return value;
}

[[nodiscard]] bool equal_name(std::string_view left,
                              std::string_view right) noexcept {
    left = without_dashes(left);
    right = without_dashes(right);
    if (left.size() != right.size()) {
        return false;
    }
    for (std::size_t i = 0; i < left.size(); ++i) {
        const auto lhs = static_cast<unsigned char>(left[i]);
        const auto rhs = static_cast<unsigned char>(right[i]);
        if (std::tolower(lhs) != std::tolower(rhs)) {
            return false;
        }
    }
    return true;
}

[[nodiscard]] bool raw_has_flag(std::span<const std::string> args,
                                std::string_view name) noexcept {
    for (const std::string& argument : args) {
        if (equal_name(argument, name)) {
            return true;
        }
    }
    return false;
}

[[nodiscard]] int raw_index_of_flag(
    std::span<const std::string> args, std::string_view name) noexcept {
    for (std::size_t index = 0; index < args.size(); ++index) {
        if (equal_name(args[index], name)) {
            return static_cast<int>(index);
        }
    }
    return -1;
}

[[nodiscard]] std::optional<std::string_view> raw_value_after(
    std::span<const std::string> args, std::string_view name) noexcept {
    for (std::size_t index = 0; index + 1 < args.size(); ++index) {
        if (equal_name(args[index], name)) {
            return args[index + 1];
        }
    }
    return std::nullopt;
}

[[nodiscard]] std::optional<int> raw_int(
    std::optional<std::string_view> value) noexcept {
    if (!value.has_value() || value->empty()) {
        return std::nullopt;
    }
    std::string_view text = *value;
    while (!text.empty()
           && std::isspace(static_cast<unsigned char>(text.front())) != 0) {
        text.remove_prefix(1);
    }
    while (!text.empty()
           && std::isspace(static_cast<unsigned char>(text.back())) != 0) {
        text.remove_suffix(1);
    }
    if (text.empty()) {
        return std::nullopt;
    }
    bool negative = false;
    if (text.front() == '+' || text.front() == '-') {
        negative = text.front() == '-';
        text.remove_prefix(1);
    }
    if (text.empty()) {
        return std::nullopt;
    }
    constexpr std::uint64_t max_positive =
        static_cast<std::uint64_t>(std::numeric_limits<int>::max());
    constexpr std::uint64_t max_negative = max_positive + 1U;
    std::uint64_t magnitude = 0;
    for (const char character : text) {
        if (character < '0' || character > '9') {
            return std::nullopt;
        }
        magnitude = magnitude * 10U
            + static_cast<std::uint64_t>(character - '0');
        if (magnitude > (negative ? max_negative : max_positive)) {
            return std::nullopt;
        }
    }
    return negative
        ? magnitude == max_negative
            ? std::optional<int>(std::numeric_limits<int>::min())
            : std::optional<int>(-static_cast<int>(magnitude))
        : std::optional<int>(static_cast<int>(magnitude));
}

[[nodiscard]] std::optional<double> raw_double(
    std::optional<std::string_view> value) noexcept {
    if (!value.has_value() || value->empty()) {
        return std::nullopt;
    }
    try {
        std::istringstream input{std::string(*value)};
        input.imbue(std::locale::classic());
        double result = 0.0;
        input >> result;
        if (input.fail()) {
            return std::nullopt;
        }
        input >> std::ws;
        if (!input.eof()) {
            return std::nullopt;
        }
        return result;
    } catch (...) {
        return std::nullopt;
    }
}

[[nodiscard]] bool parse_host_port_range(
    std::string_view value, int& first, int& last) noexcept {
    const std::size_t dash = value.find('-');
    if (dash == std::string_view::npos || dash == 0
        || dash + 1 >= value.size()) {
        return false;
    }
    const auto parsed_first = raw_int(value.substr(0, dash));
    const auto parsed_last = raw_int(value.substr(dash + 1));
    if (!parsed_first.has_value() || !parsed_last.has_value()
        || *parsed_first <= 0 || *parsed_last < *parsed_first) {
        return false;
    }
    first = *parsed_first;
    last = *parsed_last;
    return true;
}

[[nodiscard]] std::uint16_t port_value(
    std::optional<std::string_view> value, std::uint16_t fallback,
    int minimum = 0) noexcept {
    const int parsed = raw_int(value).value_or(static_cast<int>(fallback));
    return static_cast<std::uint16_t>(std::clamp(parsed, minimum, 65'535));
}

fruityprime::DedicatedServer* g_server = nullptr;
fruityprime::MasterServer* g_master = nullptr;

void stop_services() noexcept {
    if (g_server != nullptr) {
        g_server->stop();
    }
    if (g_master != nullptr) {
        g_master->stop();
    }
}

[[nodiscard]] std::chrono::milliseconds requested_duration(
    std::span<const std::string> args) noexcept {
    const auto seconds = raw_int(raw_value_after(args, "seconds"));
    return seconds.has_value() && *seconds > 0
        ? std::chrono::seconds(*seconds) : std::chrono::milliseconds{};
}

[[nodiscard]] std::string machine_name();

[[nodiscard]] int run_master_server(
    std::span<const std::string> args) {
    fruityprime::MasterOptions options;
    options.port = port_value(raw_value_after(args, "port")
                                  .has_value()
                              ? raw_value_after(args, "port")
                              : raw_value_after(args, "masterport"),
                              options.port);
    options.public_address = std::string(
        raw_value_after(args, "public").value_or(
            raw_value_after(args, "publicaddress").value_or(
                std::string_view{})));

    const auto host_ports = raw_value_after(args, "hostports")
        .value_or(std::string_view{"27900-27919"});
    if (!equal_name(host_ports, "none")) {
        int first = 0;
        int last = -1;
        if (!parse_host_port_range(host_ports, first, last)) {
            std::cout << "[master] ignoring -hostports " << host_ports
                      << " (expected e.g. 27900-27919, or none)\n";
        } else {
            options.host_port_first = first;
            options.host_port_last = last;
        }
    }

    fruityprime::MasterServer master(std::move(options));
    g_master = &master;
    fruityprime::mods::ShutdownSignals signals;
    signals.on_shutdown(stop_services);
    master.run(requested_duration(args));
    g_master = nullptr;
    return true;
}

[[nodiscard]] bool run_server(
    std::span<const std::string> args) {
    fruityprime::ServerOptions options;
    options.port = port_value(raw_value_after(args, "port"), options.port);
    options.max_players = std::clamp(
        raw_int(raw_value_after(args, "players")).value_or(options.max_players),
        1, static_cast<int>(fruityprime::net::NetConfig::SlotCapacity));
    options.server_name = std::string(raw_value_after(
        args, "servername").value_or(raw_value_after(
            args, "name").value_or(std::string_view{})));
    if (options.server_name.empty()) {
        options.server_name = machine_name();
    }
    options.friendly_fire = raw_has_flag(args, "friendlyfire");
    options.advertise = !raw_has_flag(args, "nomaster")
        && !raw_has_flag(args, "unlisted");
    if (const auto master = raw_value_after(args, "master")) {
        options.master_host = std::string(*master);
    }
    options.master_port = port_value(
        raw_value_after(args, "masterport"), options.master_port, 1);
    const auto rotation = raw_value_after(args, "rotation");
    const auto rotation_path = rotation.has_value()
        ? std::filesystem::path(*rotation)
        : fruityprime::utility::console::resolve_launch_path("maprotation.txt");
    options.rotation = fruityprime::MapRotation::load_or_create(rotation_path);

    fruityprime::DedicatedServer server(std::move(options));
    g_server = &server;
    fruityprime::mods::ShutdownSignals signals;
    signals.on_shutdown(stop_services);
    server.run(requested_duration(args));
    g_server = nullptr;
    return true;
}

[[nodiscard]] bool run_servers(
    std::span<const std::string> args) {
    const std::string host = std::string(raw_value_after(
        args, "master").value_or(fruityprime::net::NetMasterConfig::DefaultHost));
    const std::uint16_t port = port_value(
        raw_value_after(args, "masterport"),
        fruityprime::net::NetMasterConfig::DefaultPort, 1);
    const auto result = fruityprime::net::MasterClient::query(host, port);
    if (!result.answered) {
        std::cout << "[servers] no answer from " << host << ':' << port << '\n';
        return true;
    }
    if (result.servers.empty()) {
        std::cout << "[servers] the directory is up and has nobody listed\n";
        return true;
    }
    for (const auto& listing : result.servers) {
        const auto status = fruityprime::net::query(
            listing.address, listing.port, false);
        const std::string endpoint = listing.address + ':'
            + std::to_string(listing.port);
        const std::string name = !status.server_name.empty()
            ? status.server_name
            : !listing.server_name.empty() ? listing.server_name : endpoint;
        if (!status.online) {
            std::cout << "  " << name << " " << endpoint
                      << " did not answer\n";
            continue;
        }
        std::cout << "  " << name << " " << endpoint << ' '
                  << status.room_key << ' '
                  << fruityprime::net::mode_name(status.mode) << ' '
                  << status.players << '/' << status.max_players << ' '
                  << status.latency_ms << " ms\n";
    }
    return true;
}

[[nodiscard]] std::string machine_name() {
#ifdef _WIN32
    if (const char* value = std::getenv("COMPUTERNAME"); value != nullptr
        && *value != '\0') {
        return value;
    }
#else
    if (const char* value = std::getenv("HOSTNAME"); value != nullptr
        && *value != '\0') {
        return value;
    }
#endif
    return "FruityPrime";
}

void apply_render_overrides(std::span<const std::string> args) {
    auto& options = fruityprime::mods::render::options();
    const auto apply_toggle = [&](std::string_view name, bool& target,
                                  bool bare_sets_true) {
        const auto value = raw_value_after(args, name);
        if (value.has_value() && !value->starts_with('-')) {
            target = fruityprime::mods::render::Options::parse_on_off(
                *value, target);
        } else if (bare_sets_true && raw_has_flag(args, name)) {
            target = true;
        }
    };

    bool cel = options.cel_shading();
    apply_toggle("cel", cel, true);
    options.set_cel_shading(cel);
    bool fog = options.fog();
    apply_toggle("fog", fog, false);
    options.set_fog(fog);
    bool fps = options.show_fps();
    apply_toggle("fps", fps, true);
    options.set_show_fps(fps);

    if (const auto value = raw_value_after(args, "celbands")) {
        if (const auto parsed = raw_int(value)) {
            options.set_cel_bands(*parsed);
        }
    }
    if (const auto value = raw_value_after(args, "celedge")) {
        std::string edge(*value);
        while (!edge.empty() && edge.back() == '%') {
            edge.pop_back();
        }
        if (const auto parsed = raw_int(edge)) {
            options.set_cel_edge(static_cast<float>(*parsed) / 100.0F);
        }
    }

    bool pro_hud = fruityprime::features::Features::ProHud;
    apply_toggle("prohud", pro_hud, true);
    fruityprime::features::Features::ProHud = pro_hud;
}

void initialize_startup_mods(std::span<const std::string> args) {
    const auto& base = fruityprime::ConsoleSetup::LaunchDirectory();
    fruityprime::mods::InputSettings::Load(base);
    const auto preferences = fruityprime::launcher::load_preferences(base);
    fruityprime::window::startup_mode() = preferences.window_mode;

    static fruityprime::debug::Log debug_log;
    const bool forced = raw_has_flag(args, "debuglog");
    static bool force_debug_log = false;
    force_debug_log = force_debug_log || forced;
    static_cast<void>(debug_log.attach(
        base, preferences.debug_logs, force_debug_log));

    fruityprime::update::Updater::set_disabled(
        raw_has_flag(args, "noupdate"));
    apply_render_overrides(args);
}

void set_net_debug_environment() noexcept {
    fruityprime::net::NetDiagnostics::set_process_enabled(true);
#ifdef _WIN32
    static_cast<void>(_putenv_s("MPHREAD_NET_DEBUG", "1"));
#else
    static_cast<void>(setenv("MPHREAD_NET_DEBUG", "1", 1));
#endif
}

[[nodiscard]] bool selected_map(
    const fruityprime::mapgen::MapDefinition& definition,
    std::optional<std::string_view> requested) noexcept {
    return !requested.has_value()
        || equal_name(*requested, "all")
        || equal_name(definition.name, *requested);
}

[[nodiscard]] bool run_map_generation(
    std::span<const std::string> args) {
    const auto requested = raw_value_after(args, "mapgen");
    int generated_count = 0;
    int failed_count = 0;
    for (const auto& definition
         : fruityprime::mapgen::custom_rooms::definitions()) {
        if (!selected_map(definition, requested)) {
            continue;
        }
        try {
            const auto generated = fruityprime::mapgen::build(definition);
            fruityprime::mapgen::write_generated(
                definition, generated,
                fruityprime::mapgen::custom_rooms::generated_directory(
                    definition),
                fruityprime::mapgen::custom_rooms::entity_directory(),
                fruityprime::mapgen::custom_rooms::node_directory());
            const auto& stats = generated.stats;
            std::cout << definition.name << ": " << stats.model_faces
                      << " polygons (" << stats.model_vertices << " vertices), "
                      << stats.collision_faces << " collision faces, "
                      << stats.entities << " entities\n"
                      << "  " << stats.navigation_nodes
                      << " bot waypoints, " << stats.navigation_edges
                      << " routes between them\n"
                      << "  model " << generated.model.size() << " B, collision "
                      << generated.collision.size() << " B, entities "
                      << generated.entities.size() << " B, nodes "
                      << generated.nodes.size() << " B\n";
            ++generated_count;
        } catch (const std::exception& error) {
            std::cout << definition.name << ": " << error.what() << '\n';
            ++failed_count;
        }
    }
    if (generated_count == 0 && failed_count == 0) {
        std::cout << "No maps to generate. Put a map JSON in "
                  << fruityprime::mapgen::custom_rooms::map_directory().string()
                  << ".\n";
    }
    MphReadNative::Program::SetExitCode(failed_count == 0 ? 0 : 1);
    return true;
}

[[nodiscard]] bool run_q3_convert(
    std::span<const std::string> args, std::string_view source) {
    fruityprime::mapgen::Q3ConvertOptions options;
    options.source = std::filesystem::path(source);
    if (const auto map_name = raw_value_after(args, "map")) {
        options.map_name = std::string(*map_name);
    }
    if (const auto room_name = raw_value_after(args, "name")) {
        options.room_name = std::string(*room_name);
    }
    if (const auto output = raw_value_after(args, "out")) {
        options.output_directory = std::filesystem::path(*output);
    }
    options.drop_clip = raw_has_flag(args, "noclip");
    if (const auto scale = raw_double(raw_value_after(args, "scale"));
        scale.has_value() && *scale > 0.0) {
        options.forced_units_per_unit = static_cast<float>(*scale);
    }
    if (const auto texture_size = raw_int(raw_value_after(args, "texsize"));
        texture_size.has_value() && *texture_size >= 8 && *texture_size <= 256) {
        options.texture_size = *texture_size;
    }

    // Q3Convert.Run handles File.Exists itself and prints this exact
    // diagnostic before attempting to open or parse the source.  Keeping the
    // check at the command boundary also prevents the lower-level converter's
    // absolute-path validation message from leaking into the managed surface.
    std::error_code source_error;
    if (!std::filesystem::is_regular_file(options.source, source_error)
        || source_error) {
        std::cout << "No such file: " << source << '\n';
        MphReadNative::Program::SetExitCode(1);
        return true;
    }

    try {
        const std::optional<std::string_view> selected_map =
            options.map_name.empty()
            ? std::nullopt
            : std::optional<std::string_view>(options.map_name);
        // Q3Convert.Run catches only Load's failure.  Directory creation,
        // baking and recipe writing escape to ModEntry's catch below.
        static_cast<void>(fruityprime::mapgen::Q3Bsp::load(
            options.source, selected_map));
        const auto result = fruityprime::mapgen::convert_q3(options);
        std::cout << "  " << result.baked_textures << " textures at "
                  << options.texture_size << "x" << options.texture_size
                  << " -> " << result.texture_pack_bytes << " B  "
                  << result.texture_pack_path.filename().string() << '\n';
        if (!result.missing_textures.empty()) {
            std::cout << "  no image for " << result.missing_textures.size()
                      << ": ";
            const std::size_t shown = std::min<std::size_t>(
                6, result.missing_textures.size());
            for (std::size_t index = 0; index < shown; ++index) {
                if (index != 0) {
                    std::cout << ", ";
                }
                std::cout << result.missing_textures[index];
            }
            if (result.missing_textures.size() > shown) {
                std::cout << " ...";
            }
            std::cout << '\n'
                      << "  those surfaces are dropped rather than painted with somebody else's"
                         " texture; pass another .pk3 in the same folder if it has them\n";
        }
        std::cout << "  " << result.spawn_count << " spawn points, "
                  << std::setprecision(1) << std::fixed
                  << result.units_per_unit << std::defaultfloat
                  << " Quake units per unit -> "
                  << std::lround(result.drawn_extent.x
                                  / result.units_per_unit)
                  << " x "
                  << std::lround(result.drawn_extent.z
                                  / result.units_per_unit)
                  << " x "
                  << std::lround(result.drawn_extent.y
                                  / result.units_per_unit)
                  << " units\n"
                  << "  wrote " << result.recipe_path.string() << '\n';
        if (result.spawn_count < 4) {
            std::cout << "  only " << result.spawn_count
                      << " places to appear: this level was not built for a"
                         " deathmatch. Add spawns to the map file before playing"
                         " it with a full house.\n";
        }
        if (result.clip_brushes > 0 && !options.drop_clip) {
            std::cout << "  " << result.clip_brushes
                      << " player-clip brushes kept. They are the level's"
                         " invisible walls; on a race map they fence the route."
                         " -noclip converts without them.\n";
        }
        std::cout << "  no weapons or powerups were placed: where those go"
                     " decides how the map plays. Add them under \"items\", from:\n"
                  << "  HealthSmall, HealthMedium, HealthBig, UASmall, UABig,"
                     " MissileSmall, MissileBig, DoubleDamage, Cloak, Deathalt,"
                     " VoltDriver, Battlehammer, Imperialist, Judicator, Magmaul,"
                     " ShockCoil, OmegaCannon, AffinityWeapon\n"
                  << "  then: FruityPrime -mapgen \"" << result.room_name << "\"\n";
        MphReadNative::Program::SetExitCode(0);
    } catch (const std::exception& error) {
        std::cout << "Could not convert " << source << ": "
                  << error.what() << '\n';
        MphReadNative::Program::SetExitCode(1);
    }
    return true;
}

[[nodiscard]] bool run_map_materials(
    std::string_view room_name) {
    try {
        const auto& root = fruityprime::formats::global_paths().file_system();
        const auto assets = fruityprime::assets::Store::from_path(root);
        MphReadNative::Program::SetExitCode(
            fruityprime::mapgen::list_materials(std::cout, assets, room_name));
    } catch (const std::exception& error) {
        std::cout << "Could not load " << room_name << ": "
                  << error.what() << '\n';
        MphReadNative::Program::SetExitCode(1);
    }
    return true;
}

[[nodiscard]] int run_map_audit(
    std::span<const std::string> args) {
    // MapAudit's current lower layer still accepts the argv-shaped surface
    // used by the original native probe.  Build that temporary adapter here,
    // at the ModEntry boundary, so Utility/main.cpp cannot become a second
    // owner of the managed -maptest dispatch.
    std::vector<std::string> owned;
    owned.reserve(args.size() + 1);
    owned.emplace_back("FruityPrime");
    owned.insert(owned.end(), args.begin(), args.end());
    std::vector<char*> argv;
    argv.reserve(owned.size());
    for (std::string& value : owned) {
        argv.push_back(value.data());
    }
    return fruityprime::map_audit::run(
        static_cast<int>(argv.size()), argv.data());
}

} // namespace

bool try_handle_headless(std::span<const std::string> args) {
    initialize_startup_mods(args);

    const int apply_at = raw_index_of_flag(args, "applyupdate");
    if (apply_at >= 0 && static_cast<std::size_t>(apply_at + 2) < args.size()) {
        const auto pid = raw_int(std::string_view(
            args[static_cast<std::size_t>(apply_at + 2)]));
        MphReadNative::Program::SetExitCode(
            fruityprime::update::DesktopUpdate::apply(
            args[static_cast<std::size_t>(apply_at + 1)],
            pid.value_or(-1), std::filesystem::current_path()));
        return true;
    }

    fruityprime::update::DesktopUpdate::clean(std::filesystem::current_path());
    fruityprime::update::UpdateInstall::use_desktop_if_possible(
        fruityprime::ConsoleSetup::LaunchDirectory());

    if (const auto net_lag = raw_value_after(args, "netlag")) {
        if (!fruityprime::net::NetLag::configure(*net_lag)) {
            std::cout << "[net] -netlag " << *net_lag
                      << " is not a number of milliseconds (try -netlag 200 or "
                         "-netlag 200:40)\n";
            return true;
        }
    }
    if (const auto net_loss = raw_value_after(args, "netloss")) {
        if (!fruityprime::net::NetLag::configure_loss(*net_loss)) {
            std::cout << "[net] -netloss " << *net_loss
                      << " is not a percentage\n";
            return true;
        }
    }
    if (fruityprime::net::NetLag::active()) {
        std::cout << "[net] simulating a bad line: "
                  << fruityprime::net::NetLag::describe() << '\n';
    }

    if (raw_has_flag(args, "credits")) {
        fruityprime::mods::Credits::Print(std::cout);
        return true;
    }

    if (const auto map_directory = raw_value_after(args, "mapdir")) {
        fruityprime::mapgen::custom_rooms::set_map_directory(
            fruityprime::utility::console::resolve_launch_path(*map_directory));
    }

    if (raw_has_flag(args, "mapbundle")) {
        const auto requested = raw_value_after(args, "mapbundle");
        const auto output = raw_value_after(args, "out");
        int cooked = 0;
        int failed = 0;
        for (const auto& definition
             : fruityprime::mapgen::custom_rooms::definitions()) {
            if (requested.has_value()
                && !equal_name(definition.name, *requested)
                && !equal_name(*requested, "all")) {
                continue;
            }
            if (definition.source_path.empty()
                || fruityprime::mapgen::bundle::is_bundle(definition.source_path)
                || definition.import_source.empty()) {
                continue;
            }
            try {
                static_cast<void>(fruityprime::mapgen::bundle::cook(
                    definition, definition.source_path,
                    output.has_value() ? std::filesystem::path(*output)
                                       : std::filesystem::path{}, true));
                ++cooked;
            } catch (const std::exception& error) {
                std::cout << definition.name << ": " << error.what() << '\n';
                ++failed;
            }
        }
        if (cooked == 0 && failed == 0) {
            std::cout << "No map to bundle. A bundle is cooked from a recipe and "
                         "the level it converts; put both in "
                      << fruityprime::mapgen::custom_rooms::map_directory().string()
                      << ".\n";
        }
        MphReadNative::Program::SetExitCode(failed == 0 ? 0 : 1);
        return true;
    }

    if (raw_has_flag(args, "update")) {
        fruityprime::update::Updater::set_disabled(false);
        fruityprime::update::Updater updater;
        const auto available = updater.check();
        if (!available) {
            std::cout << "[update] "
                      << fruityprime::update::UpdateCheck::last_reason() << '\n';
            return true;
        }
        std::cout << "[update] " << updater.describe(*available) << '\n';
        std::cout << "[update] " << available->page_url << '\n';
        static_cast<void>(fruityprime::update::Updater::open_page(*available));
        return true;
    }

    if (raw_has_flag(args, "masterserver")) {
        if (!fruityprime::update::Updater::disabled()) {
            fruityprime::update::Updater updater(true);
            if (const auto available = updater.check()) {
                std::cout << "[update] " << updater.describe(*available) << '\n'
                          << "[update] " << available->page_url << '\n'
                          << "[update] this server keeps running on "
                          << fruityprime::update::BuildVersion::display(
                                 fruityprime::update::BuildVersion::current())
                          << "; clients on the new build will be refused until "
                             "it is updated by hand\n";
            }
        }
        return run_master_server(args);
    }
    if (raw_has_flag(args, "servers")) {
        return run_servers(args);
    }
    if (raw_has_flag(args, "server") || raw_has_flag(args, "dedicated")) {
        if (!fruityprime::update::Updater::disabled()) {
            fruityprime::update::Updater updater(true);
            if (const auto available = updater.check()) {
                std::cout << "[update] " << updater.describe(*available) << '\n'
                          << "[update] " << available->page_url << '\n'
                          << "[update] this server keeps running on "
                          << fruityprime::update::BuildVersion::display(
                                 fruityprime::update::BuildVersion::current())
                          << "; clients on the new build will be refused until "
                             "it is updated by hand\n";
            }
        }
        return run_server(args);
    }
    return false;
}

bool try_handle(std::span<const std::string> args) {
    if (!raw_has_flag(args, "mapgen")) {
        static_cast<void>(fruityprime::mapgen::custom_rooms::generate_missing());
    }

    if (raw_has_flag(args, "fullscreen") || raw_has_flag(args, "borderless")) {
        fruityprime::window::startup_mode() =
            fruityprime::window::StartMode::BorderlessFullscreen;
    } else if (raw_has_flag(args, "windowed")) {
        fruityprime::window::startup_mode() =
            fruityprime::window::StartMode::Windowed;
    }
    if (raw_has_flag(args, "nohelmet")) {
        fruityprime::features::Features::SetHelmetOpacity(0.0F);
        fruityprime::features::Features::SetVisorOpacity(0.0F);
    }
    if (raw_has_flag(args, "netdebug")) {
        set_net_debug_environment();
    }

    if (raw_has_flag(args, "mechanics")) {
        fruityprime::mechanics::print(std::cout);
        MphReadNative::Program::SetExitCode(0);
        return true;
    }
    if (raw_has_flag(args, "gamepad")) {
        const auto seconds = raw_double(raw_value_after(args, "seconds"));
        const double run_seconds = seconds.has_value() && *seconds > 0.0
            ? *seconds : 15.0;
        MphReadNative::Program::SetExitCode(
            fruityprime::input::run_gamepad_probe(run_seconds));
        return true;
    }
    if (raw_has_flag(args, "rooms")) {
        for (const auto& room : fruityprime::mods::thumbnail::multiplayer_rooms()) {
            std::cout << room << '\n';
        }
        return true;
    }
    if (raw_has_flag(args, "dpstest")) {
        // The native WeaponDps helper currently owns a headless Session probe,
        // while the managed command owns a hidden GameWindow and real Scene
        // frame loop. Do not silently substitute the former here: until the
        // renderer host is ported, leaving this command for the normal host is
        // more faithful than changing what the C# command measures.
        return false;
    }
    if (raw_has_flag(args, "mapgen")) {
        return run_map_generation(args);
    }
    if (const auto source = raw_value_after(args, "q3maps")) {
        try {
            for (const auto& name : fruityprime::mapgen::Q3Bsp::list_maps(
                     std::filesystem::path(*source))) {
                std::cout << name << '\n';
            }
            MphReadNative::Program::SetExitCode(0);
        } catch (const std::exception& error) {
            std::cout << "Could not read " << *source << ": "
                      << error.what() << '\n';
            MphReadNative::Program::SetExitCode(1);
        }
        return true;
    }
    if (const auto source = raw_value_after(args, "q3convert")) {
        return run_q3_convert(args, *source);
    }
    if (const auto source = raw_value_after(args, "q3shaders")) {
        MphReadNative::Program::SetExitCode(
            fruityprime::mapgen::list_shaders(
                std::cout, std::filesystem::path(*source),
                raw_value_after(args, "map").value_or(std::string_view{})));
        return true;
    }
    if (const auto room = raw_value_after(args, "mapmaterials")) {
        return run_map_materials(*room);
    }
    if (raw_value_after(args, "maptest")) {
        MphReadNative::Program::SetExitCode(run_map_audit(args));
        return true;
    }
    // C# handles hostgame/connect/netcheck before demoinfo.  Those three
    // scene-backed command paths are still pending their 1:1 host port, so
    // keep the completed inspection path after maptest rather than allowing
    // it to steal precedence from mapgen or the Q3 commands above.
    if (const auto demo = raw_value_after(args, "demoinfo")) {
        // DemoInfo.cs delegates replay to DemoPlayback, which in turn drives
        // the real NetSession/Scene frame boundary.  The native headless
        // replay helper is not that boundary, so keep -replay out of this
        // branch until that file-level port is complete.
        if (raw_has_flag(args, "replay")) {
            return false;
        }
        MphReadNative::Program::SetExitCode(
            fruityprime::demo::print_command(std::filesystem::path(*demo)));
        return true;
    }
    return false;
}

} // namespace fruityprime::mods
