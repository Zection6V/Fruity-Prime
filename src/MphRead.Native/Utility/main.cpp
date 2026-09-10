#include "Utility/archive.hpp"
#include "Formats/collision_format.hpp"
#include "Formats/camera_sequence.hpp"
#include "Mods/credits.hpp"
#include "Utility/console_setup.hpp"
#include "Utility/command_line.hpp"
#include "Mods/Network/demo.hpp"
#include "Formats/demo_info.hpp"
#include "Mods/Network/dedicated_server.hpp"
#include "Formats/entity_format.hpp"
#include "Export/export.hpp"
#include "Utility/extract.hpp"
#include "Assets/game_assets.hpp"
#include "Mods/Input/gamepad_probe.hpp"
#include "Entities/gameplay.hpp"
#include "Mods/Network/master_client.hpp"
#include "Mods/Network/master_server.hpp"
#include "Mods/Network/match_client.hpp"
#include "Entities/match_flow.hpp"
#include "Mods/MapGen/mapgen.hpp"
#include "Mods/MapGen/map_report.hpp"
#include "Mods/Network/mechanics_dump.hpp"
#include "Metadata/metadata.hpp"
#include "Mods/mod_entry.hpp"
#include "Formats/model_format.hpp"
#include "Formats/movie.hpp"
#include "Assets/nds_rom.hpp"
#include "Mods/Network/net_client.hpp"
#include "Mods/Network/net_check_client.hpp"
#include "Mods/Network/net_connect_command.hpp"
#include "Mods/Network/net_launch.hpp"
#include "Mods/Network/weapon_dps.hpp"
#include "Entities/room_catalog.hpp"
#include "Utility/repack_entity.hpp"
#include "Utility/repack_collision.hpp"
#include "Utility/repack_model.hpp"
#include "Read.hpp"
#include "Entities/scene.hpp"
#include "Mods/shutdown_signals.hpp"
#include "Formats/sound_catalog.hpp"
#include "Sound/sseq_player.hpp"
#include "Mods/Update/update.hpp"
#include "Program.hpp"

#include "Entities/Players/PlayerAi.hpp"
#include "../Mods/MapGen/map_bundle.hpp"
#include "../Mods/MapGen/custom_rooms.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <memory>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

namespace {

using fruityprime::utility::command_line::has_flag;
using fruityprime::utility::command_line::hunter_after;
using fruityprime::utility::command_line::index_of_flag;
using fruityprime::utility::command_line::integer_after;
using fruityprime::utility::command_line::parse_port_range;
using fruityprime::utility::command_line::seconds_value;
using fruityprime::utility::command_line::spectate_arguments;
using fruityprime::utility::command_line::value_after;
using fruityprime::utility::command_line::values_after;
using fruityprime::utility::command_line::SpectateArguments;

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

[[nodiscard]] std::filesystem::path executable_directory(const char* executable) {
    std::error_code error;
    const auto absolute = std::filesystem::absolute(executable, error);
    if (!error && absolute.has_parent_path()) {
        return absolute.parent_path();
    }
    return std::filesystem::current_path();
}

void print_usage(const char* executable) {
    std::cout
        << "Fruity Prime native C++ dedicated server\n\n"
        << "Usage:\n"
        << "  " << executable << " -masterserver [options]\n"
        << "  " << executable << " -server [options]\n\n"
        << "  " << executable << " -status HOST -port N\n"
        << "  " << executable << " -servers [-master HOST] [-masterport N]\n"
        << "  " << executable << " -rooms\n"
        << "  " << executable << " -credits\n"
        << "  " << executable << " -mechanics\n"
        << "  " << executable << " -update\n"
        << "  " << executable << " -applyupdate TARGET PID (internal)\n"
        << "  " << executable << " -connect HOST -port N [-name NAME]\n"
        << "  " << executable << " -gamepad [-seconds N]\n"
        << "  " << executable << " -netcheck HOST -port N [-seconds N] [-recorddemo]"
           " [-spectate [SECONDS]] [-rejoin SECONDS]\n"
        << "  " << executable << " -weapondps ROOM -rom FILE [-hunter H]"
           " [-beam B] [-seconds N] [-distance N]\n"
        << "  " << executable << " -rominfo FILE\n"
        << "  " << executable << " -demoinfo FILE [-replay -rom ROM]\n"
        << "  " << executable << " -maptest ROOM -rom FILE [options]\n"
        << "  " << executable << " -extract-rom FILE [-out DIR]\n"
        << "  " << executable << " -roominfo ROM_OR_DIR [room options]\n"
        << "  " << executable << " -archive-info FILE\n"
        << "  " << executable << " -extract-archive FILE -out DIR\n"
        << "  " << executable << " -collision-info FILE\n\n"
        << "  " << executable << " -collision-repack FILE -out FILE\n\n"
        << "  " << executable << " -entity-info FILE\n\n"
        << "  " << executable << " -entity-repack FILE -out FILE\n\n"
        << "  " << executable << " -model-info FILE\n\n"
        << "  " << executable << " -model-repack FILE -out FILE"
           " [-separate-textures] [-texture-out FILE]"
           " [-bounds none|capped|uncapped] [-room]\n\n"
        << "  " << executable << " -model-export-obj FILE -out FILE\n"
        << "  " << executable << " -model-export-collada FILE -out FILE\n\n"
        << "  " << executable << " -model-export-script FILE -out FILE\n"
           "       [-model-name NAME] [-export-root DIR] [-recolor NAME]\n\n"
        << "  " << executable << " -model-export-textures FILE -out DIR\n\n"
        << "  " << executable << " -soundinfo ROM_OR_DIR\n\n"
        << "  " << executable << " -sseqinfo ROM_OR_DIR [-sequence N]"
           " [-seconds N]\n\n"
        << "  " << executable << " -movieinfo ROM_OR_DIR [-movie NAME]\n"
        << "  " << executable << " -movieexport ROM_OR_DIR [-movie NAME]"
           " [-out DIR]\n"
        << "  " << executable << " -export movie -rom ROM [-movie NAME]"
           " [-out DIR]\n\n"
        << "  " << executable << " -camseq-info ROM_OR_DIR"
           " [-sequence FILE]\n\n"
        << "  " << executable << " -q3convert FILE [-map NAME] [-name ROOM]"
           " [-noclip] [-scale N] [-texsize N] [-out DIR]\n\n"
        << "  " << executable << " -q3shaders FILE [-map NAME]\n"
        << "  " << executable << " -mapmaterials ROOM -rom FILE\n\n"
        << "  " << executable << " -mapgen MAP_JSON [-out DIR]\n"
        << "  " << executable << " -mapbundle [NAME|MAP_JSON] [-mapdir DIR]"
           " [-out FILE]\n\n"
        << "Options:\n"
        << "  -masterserver        run the server directory\n"
        << "  -public HOST         public IPv4/DNS address for hosted games\n"
        << "  -hostports A-B       ports the directory may use for hosted games\n"
        << "  -port N             UDP port (server/status default 27888)\n"
        << "  -players N           player cap / maptest count\n"
        << "  -servername NAME    name shown in the server browser\n"
        << "  -rotation FILE      map rotation (default maprotation.txt)\n"
        << "  -friendlyfire       enable same-team damage\n"
        << "  -nomaster           do not announce to the master server\n"
        << "  -netlag MS[:JITTER] add simulated round-trip latency to this client\n"
         << "  -netloss PERCENT    drop this percentage of client datagrams\n"
         << "  -spectate [SECONDS] stop playing at this time in -netcheck\n"
         << "  -rejoin SECONDS     resume a spectating -netcheck client\n"
        << "  -master HOST        master server host\n"
        << "  -masterport N       master server UDP port (default 27889)\n"
        << "  -seconds N          stop after N seconds (test convenience)\n"
        << "  -bots               drive non-local -maptest players\n"
        << "  -bot-level N        offline bot skill: 0 easy, 1 normal, 2 hard\n"
        << "  -renderprobe        decode all room render resources in -maptest\n"
        << "  -recorddemo         record netcheck packets to export/_demos\n"
        << "  -hunter H            hunter name or 0..6 for -weapondps\n"
        << "  -beam B              BeamType name or 0..8 for -weapondps\n"
        << "  -distance N          shooter distance for -weapondps\n"
        << "  -demoout FILE       path for a -recorddemo .fpdemo file\n"
        << "  -map NAME           Q3 BSP map name inside a PK3\n"
        << "  -scale N            Q3 units per native map unit\n"
        << "  -texsize N          generated texture edge length (default 64)\n"
        << "  -replay             replay a demo through the native session\n"
        << "  -rom FILE           NDS used by -maptest and -replay\n"
        << "  -room NAME          room label (default UNIT1_C0)\n"
        << "  -archive FILE       room archive relative to the ROM/directory\n"
        << "  -model-entry FILE   model entry inside the room archive\n"
        << "  -texture FILE       room texture resource relative to the ROM/directory\n"
        << "  -collision-entry FILE collision entry inside the room archive\n"
        << "  -entity FILE        entity table relative to the ROM/directory\n"
        << "  -help               show this help\n";
}

int run_update_command() {
    fruityprime::update::Updater updater;
    const auto available = updater.check();
    if (!available) {
        std::cout << "update: "
                  << fruityprime::update::UpdateCheck::last_reason() << '\n';
        return EXIT_SUCCESS;
    }
    std::cout << "update: " << updater.describe(*available) << '\n';
    if (!fruityprime::update::Updater::open_page(*available)) {
        std::cout << "open this page: " << available->page_url << '\n';
    }
    return EXIT_SUCCESS;
}

int run_apply_update_command(int argc, char** argv) {
    const int flag = index_of_flag(argc, argv, "-applyupdate");
    if (flag < 0 || flag + 2 >= argc || argv[flag + 1] == nullptr
        || argv[flag + 2] == nullptr || std::string(argv[flag + 1]).empty()
        || std::string(argv[flag + 2]).empty()) {
        throw std::invalid_argument("-applyupdate needs TARGET and PID");
    }
    const std::string pid_text(argv[flag + 2]);
    std::size_t consumed = 0;
    long long pid = 0;
    try {
        pid = std::stoll(pid_text, &consumed, 10);
    } catch (const std::exception&) {
        throw std::invalid_argument("-applyupdate PID is not an integer");
    }
    if (consumed != pid_text.size() || pid <= 0
        || pid > std::numeric_limits<int>::max()) {
        throw std::invalid_argument("-applyupdate PID is out of range");
    }
    return fruityprime::update::DesktopUpdate::apply(
        std::filesystem::path(argv[flag + 1]), static_cast<int>(pid),
        executable_directory(argc > 0 ? argv[0] : "FruityPrime"));
}

[[nodiscard]] std::optional<std::int32_t> parse_beam_type(
    std::string_view value) {
    if (value.empty()) {
        return std::nullopt;
    }
    constexpr std::array<std::string_view, fruityprime::metadata::WeaponCount>
        names{{"powerbeam", "voltdriver", "missile", "battlehammer",
               "imperialist", "judicator", "magmaul", "shockcoil",
               "omegacannon"}};
    std::string normalized;
    normalized.reserve(value.size());
    for (const char character : value) {
        if (character == '-' || character == '_' || character == ' ') {
            continue;
        }
        normalized.push_back(static_cast<char>(std::tolower(
            static_cast<unsigned char>(character))));
    }
    for (std::size_t i = 0; i < names.size(); ++i) {
        if (normalized == names[i]) {
            return static_cast<std::int32_t>(i);
        }
    }
    try {
        std::size_t consumed = 0;
        const int parsed = std::stoi(std::string(value), &consumed);
        if (consumed != value.size() || parsed < 0
            || parsed >= static_cast<int>(names.size())) {
            return std::nullopt;
        }
        return parsed;
    } catch (...) {
        return std::nullopt;
    }
}

int run_weapon_dps_command(int argc, char** argv) {
    const std::string room_name = value_after(argc, argv, "-weapondps");
    const std::string rom_path = value_after(argc, argv, "-rom");
    if (room_name.empty() || rom_path.empty()) {
        throw std::invalid_argument("-weapondps needs ROOM and -rom FILE");
    }

    const auto* catalog_entry = fruityprime::scene::find_room(room_name);
    const fruityprime::scene::RoomDefinition definition = catalog_entry != nullptr
        ? catalog_entry->definition
        : fruityprime::scene::RoomDefinition{
            room_name,
            "archives/unit1_C0.arc",
            "unit1_c0_model.bin",
            "levels/textures/unit1_c0_tex.bin",
            "unit1_c0_collision.bin",
            "levels/entities/Unit1_C0_Ent.bin", {}, {}, {}
        };

    fruityprime::net::WeaponDpsOptions options;
    const std::string hunter_value = value_after(argc, argv, "-hunter");
    const auto hunter = fruityprime::metadata::parse_hunter(
        hunter_value.empty() ? "0" : hunter_value);
    if (!hunter || *hunter >= fruityprime::metadata::PlayableHunterCount) {
        throw std::invalid_argument(
            "-hunter must be a playable hunter name or a number from 0 to 6");
    }
    options.hunter = *hunter;

    const std::string beam_value = value_after(argc, argv, "-beam");
    const auto beam = parse_beam_type(beam_value.empty() ? "0" : beam_value);
    if (!beam) {
        throw std::invalid_argument(
            "-beam must be a BeamType name or a number from 0 to 8");
    }
    options.beam_type = *beam;

    if (const auto seconds = seconds_value(
            value_after(argc, argv, "-seconds")); seconds.has_value()) {
        if (*seconds < 0.0) {
            throw std::invalid_argument("-seconds must be >= 0");
        }
        options.seconds = *seconds;
    }
    const std::string distance_value = value_after(argc, argv, "-distance");
    if (!distance_value.empty()) {
        try {
            std::size_t consumed = 0;
            options.distance = std::stof(distance_value, &consumed);
            if (consumed != distance_value.size()
                || !std::isfinite(options.distance)) {
                throw std::invalid_argument("invalid distance");
            }
        } catch (const std::exception&) {
            throw std::invalid_argument("-distance must be a finite number");
        }
    }

    const auto assets = fruityprime::assets::Store::from_rom(rom_path);
    const auto room = fruityprime::scene::Room::load(assets, definition);
    const auto result = fruityprime::net::WeaponDps::run(room, options);
    fruityprime::net::WeaponDps::print(std::cout, result);
    return result.ok ? EXIT_SUCCESS : EXIT_FAILURE;
}

int run_rooms_command() {
    for (const auto& room : fruityprime::scene::multiplayer_rooms()) {
        std::cout << room.id << " " << room.name
                  << " \"" << room.in_game_name << "\"\n";
    }
    std::cout << fruityprime::scene::multiplayer_rooms().size()
              << " multiplayer room(s)\n";
    return EXIT_SUCCESS;
}

int run_status_command(int argc, char** argv) {
    const std::string host = value_after(argc, argv, "-status");
    if (host.empty()) {
        throw std::invalid_argument("-status needs a host");
    }
    const int requested_port = integer_after(
        argc, argv, "-port", fruityprime::net::NetConfig::DefaultPort);
    const auto result = fruityprime::net::NetClient::query_status(
        host, static_cast<std::uint16_t>(std::clamp(requested_port, 1, 65'535)));
    if (!result.online) {
        std::cout << "offline: " << result.error << '\n';
        return EXIT_FAILURE;
    }
    std::cout << "online: " << host << ':' << requested_port
              << " name=\"" << result.server_name << "\""
              << " room=\"" << result.match.room_key << "\""
              << " mode=" << fruityprime::game_mode_name(
                  static_cast<fruityprime::GameMode>(result.match.mode))
              << " players=" << static_cast<int>(result.match.player_count)
              << '/' << static_cast<int>(result.max_players)
              << " latency=" << result.latency_ms << "ms\n";
    return EXIT_SUCCESS;
}

int run_servers_command(int argc, char** argv) {
    const std::string master = value_after(argc, argv, "-master").empty()
        ? "net.livetek.fr" : value_after(argc, argv, "-master");
    const int master_port = integer_after(argc, argv, "-masterport", 27889);
    const auto result = fruityprime::net::MasterClient::query(
        master, static_cast<std::uint16_t>(std::clamp(master_port, 1, 65'535)));
    if (!result.answered) {
        std::cout << "master offline: " << result.error << '\n';
        return EXIT_FAILURE;
    }
    for (const auto& server : result.servers) {
        std::cout << server.address << ':' << server.port
                  << " " << static_cast<int>(server.players)
                  << '/' << static_cast<int>(server.max_players)
                  << " " << fruityprime::game_mode_name(
                      static_cast<fruityprime::GameMode>(server.mode))
                  << " \"" << server.server_name << "\""
                  << " [" << server.room_key << "]\n";
    }
    std::cout << result.servers.size() << " server(s)\n";
    return EXIT_SUCCESS;
}

int run_connect_command(int argc, char** argv) {
    const std::string host = value_after(argc, argv, "-connect");
    if (host.empty()) {
        throw std::invalid_argument("-connect needs a host");
    }
    const int requested_port = integer_after(
        argc, argv, "-port", fruityprime::net::NetConfig::DefaultPort);
    const std::string netlag = value_after(argc, argv, "-netlag");
    const std::string netloss = value_after(argc, argv, "-netloss");
    if (has_flag(argc, argv, "-netlag") && netlag.empty()) {
        throw std::invalid_argument("-netlag needs a value");
    }
    if (has_flag(argc, argv, "-netloss") && netloss.empty()) {
        throw std::invalid_argument("-netloss needs a value");
    }
    const auto conditions = fruityprime::net::NetworkConditions::from_options(
        netlag, netloss);
    fruityprime::net::JoinOptions join;
    join.address = host;
    join.port = static_cast<std::uint16_t>(
        std::clamp(requested_port, 1, 65'535));
    join.player_name = value_after(argc, argv, "-name");
    if (join.player_name.empty()) {
        join.player_name = "FruityPrime";
    }
    join.hunter = hunter_after(argc, argv, "-hunter");
    join.network = conditions;
    const int seconds = std::max(0, integer_after(argc, argv, "-seconds", 1));
    const auto result = fruityprime::net::NetConnectCommand::run(
        fruityprime::net::ConnectCommandOptions{join, seconds});
    if (!result.ok) {
        std::cerr << "connect failed: " << result.error << '\n';
        return EXIT_FAILURE;
    }
    std::cout << "connected to " << host << ':' << requested_port
              << " as slot " << result.local_slot
              << '\n';
    if (const auto& room = result.room) {
        const auto active = std::count_if(
            result.players.begin(), result.players.end(),
            [](const auto& player) { return player.active; });
        std::cout << "server room " << room->room_key
                  << " mode=" << fruityprime::game_mode_name(room->mode)
                  << " layout_players="
                  << fruityprime::net::NetLaunch::RoomPlayerCount
                  << " active_slots=" << active << '\n';
    }
    if (const std::string description = conditions.describe();
        !description.empty()) {
        std::cout << "simulated line: " << description << '\n';
    }
    std::cout << "received " << result.packets << " server packet(s)\n";
    return EXIT_SUCCESS;
}

[[maybe_unused]] int run_recording_netcheck(
                           const fruityprime::net::Endpoint& endpoint,
                           const fruityprime::net::MatchClientOptions& options,
                           int seconds,
                           const std::filesystem::path& demo_path) {
    fruityprime::net::NetClient client(endpoint, options.network);
    if (!client.connect()) {
        throw std::runtime_error("match client connect failed: "
                                 + std::string(client.last_error()));
    }
    client.identify(options.hunter, options.name);
    fruityprime::demo::Writer writer(demo_path);
    fruityprime::net::SpectateSchedule schedule(
        options.spectate_at_seconds, options.rejoin_at_seconds);

    std::size_t intents_sent = 0;
    std::size_t snapshots_received = 0;
    std::size_t rosters_received = 0;
    std::size_t match_states_received = 0;
    std::size_t chats_received = 0;
    std::size_t demo_records = 0;
    std::uint32_t last_snapshot_frame = 0;
    std::uint8_t last_roster_count = 0;
    std::uint32_t demo_frame = 0;
    std::size_t spectating_frames = 0;
    std::array<std::size_t, fruityprime::net::NetConfig::SlotCapacity>
        remote_spectating_frames{};
    fruityprime::net::MatchStatePacket last_match_state;
    const auto started = std::chrono::steady_clock::now();
    const auto deadline = started + std::chrono::seconds(seconds);
    auto next_tick = started;

    while (std::chrono::steady_clock::now() < deadline) {
        for (const auto& packet : client.drain()) {
            // ReceivedPacket::data includes the PacketType byte, exactly as
            // DemoRecorder writes it in the managed client.
            writer.write_record(demo_frame, packet.data);
            ++demo_records;
            switch (packet.type()) {
            case fruityprime::net::PacketType::Snapshot: {
                const auto snapshot = fruityprime::net::SnapshotPacket::decode(
                    packet.payload());
                if (snapshot) {
                    ++snapshots_received;
                    last_snapshot_frame = snapshot->header.frame;
                    for (const auto& player : snapshot->players) {
                        if (player.slot_index != client.local_slot()
                            && player.slot_index < remote_spectating_frames.size()
                            && (player.flags
                                & fruityprime::net::PlayerState::FlagSpectating) != 0) {
                            ++remote_spectating_frames[player.slot_index];
                        }
                    }
                }
                break;
            }
            case fruityprime::net::PacketType::Roster: {
                const auto roster = fruityprime::net::RosterPacket::decode(
                    packet.payload());
                if (roster) {
                    ++rosters_received;
                    last_roster_count = roster->count;
                }
                break;
            }
            case fruityprime::net::PacketType::MatchState: {
                const auto state = fruityprime::net::MatchStatePacket::decode(
                    packet.payload());
                if (state) {
                    ++match_states_received;
                    last_match_state = *state;
                }
                break;
            }
            case fruityprime::net::PacketType::Chat:
                if (fruityprime::net::ChatPacket::decode(packet.payload())) {
                    ++chats_received;
                }
                break;
            default:
                break;
            }
        }

        const auto now = std::chrono::steady_clock::now();
        if (now >= next_tick) {
            fruityprime::net::IntentState intent;
            intent.frame = demo_frame + 1;
            schedule.update(intent.frame);
            intent.buttons = schedule.buttons(options.buttons);
            intent.aim = options.aim;
            intent.position = options.position;
            intent.ammo_ua = options.ammo_ua;
            intent.ammo_missiles = options.ammo_missiles;
            const auto encoded = intent.encode();
            client.send(fruityprime::net::PacketType::Intent, encoded);
            ++intents_sent;
            if (schedule.spectating()) {
                ++spectating_frames;
            }

            // The recording must contain the local player's intent in the
            // same SlotIntent shape that the server sends for other slots.
            const auto own_slot = static_cast<std::uint8_t>(
                std::max(0, client.local_slot()));
            std::vector<std::uint8_t> own_intent;
            own_intent.reserve(2 + encoded.size());
            own_intent.push_back(static_cast<std::uint8_t>(
                fruityprime::net::PacketType::SlotIntent));
            own_intent.push_back(own_slot);
            own_intent.insert(own_intent.end(), encoded.begin(), encoded.end());
            writer.write_record(demo_frame, own_intent);
            ++demo_records;

            ++demo_frame;
            next_tick += std::chrono::milliseconds(16);
            if (next_tick < now) {
                next_tick = now + std::chrono::milliseconds(16);
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    writer.close();
    const int local_slot = client.local_slot();
    client.disconnect();
    std::cout << "netcheck connected slot=" << local_slot
              << " intents=" << intents_sent
              << " snapshots=" << snapshots_received
              << " rosters=" << rosters_received
              << " match_states=" << match_states_received
              << " chats=" << chats_received
              << " demo=\"" << demo_path.string() << "\""
              << " demo_records=" << demo_records
              << " last_snapshot_frame=" << last_snapshot_frame
              << " roster_count=" << static_cast<int>(last_roster_count)
              << " last_mode=" << static_cast<int>(last_match_state.mode)
              << " spectating_frames=" << spectating_frames
              << " spectate_started_frame=" << schedule.started_frame()
              << " rejoined_frame=" << schedule.rejoined_frame()
              << '\n';
    for (std::size_t slot = 0; slot < remote_spectating_frames.size(); ++slot) {
        if (remote_spectating_frames[slot] > 0) {
            std::cout << "  slot " << slot << " was spectating on "
                      << remote_spectating_frames[slot]
                      << " received frame(s)\n";
        }
    }
    if (const std::string description = options.network.describe();
        !description.empty()) {
        std::cout << "SIMULATED LINE: " << description
                  << " -- these numbers are a reproduction, not a real-line measurement\n";
    }
    return intents_sent > 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}

int run_recording_netcheck_native(
    const fruityprime::net::Endpoint& endpoint,
    const fruityprime::net::NetCheckClientOptions& options,
    const std::filesystem::path& demo_path) {
    fruityprime::demo::Writer writer(demo_path);
    auto checked = options;
    checked.packet_sink = [&writer](std::uint32_t frame,
                                    std::span<const std::uint8_t> data) {
        writer.write_record(frame, data);
    };
    const auto report = fruityprime::net::run_net_check_client(
        endpoint, checked, std::cout);
    writer.close();
    std::cout << "  demo=\"" << demo_path.string() << "\"\n";
    return !report.connected ? EXIT_FAILURE
        : report.passed && report.features_passed ? EXIT_SUCCESS
        : EXIT_FAILURE;
}

int run_netcheck_command(int argc, char** argv) {
    const std::string host = value_after(argc, argv, "-netcheck");
    if (host.empty()) {
        throw std::invalid_argument("-netcheck needs a host");
    }
    const int requested_port = integer_after(
        argc, argv, "-port", fruityprime::net::NetConfig::DefaultPort);
    const auto endpoint = fruityprime::net::NetClient::resolve_ipv4(
        host, static_cast<std::uint16_t>(std::clamp(requested_port, 1, 65'535)));
    fruityprime::net::NetCheckClientOptions options;
    options.name = value_after(argc, argv, "-name");
    if (options.name.empty()) {
        options.name = "FruityPrime";
    }
    options.hunter = hunter_after(argc, argv, "-hunter");
    const std::string netlag = value_after(argc, argv, "-netlag");
    const std::string netloss = value_after(argc, argv, "-netloss");
    if (has_flag(argc, argv, "-netlag") && netlag.empty()) {
        throw std::invalid_argument("-netlag needs a value");
    }
    if (has_flag(argc, argv, "-netloss") && netloss.empty()) {
        throw std::invalid_argument("-netloss needs a value");
    }
    options.network = fruityprime::net::NetworkConditions::from_options(
        netlag, netloss);
    const auto spectate = spectate_arguments(argc, argv);
    options.spectate_at_seconds = spectate.spectate_at_seconds;
    options.rejoin_at_seconds = spectate.rejoin_at_seconds;
    const int seconds = std::max(1, integer_after(argc, argv, "-seconds", 5));
    options.seconds = seconds;
    const std::string demo_out = value_after(argc, argv, "-demoout");
    if (has_flag(argc, argv, "-recorddemo") || !demo_out.empty()) {
        const auto default_name = std::string("netcheck_")
            + std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count())
            + std::string(fruityprime::demo::Extension);
        const auto demo_path = demo_out.empty()
            ? executable_directory(argv[0]) / "export" / "_demos" / default_name
            : std::filesystem::path(demo_out);
        return run_recording_netcheck_native(endpoint, options, demo_path);
    }
    const auto report = fruityprime::net::run_net_check_client(
        endpoint, options, std::cout);
    return report.connected && report.passed && report.features_passed
        ? EXIT_SUCCESS : EXIT_FAILURE;
}

int run_demo_command(int argc, char** argv) {
    const std::string source = value_after(argc, argv, "-demoinfo");
    if (source.empty()) {
        throw std::invalid_argument("-demoinfo needs a .fpdemo file");
    }
    const auto info = fruityprime::demo::inspect(source);
    if (!info.has_value()) {
        std::cout << "invalid demo header: " << source << '\n';
        return EXIT_FAILURE;
    }
    fruityprime::demo::print(std::cout, source, *info);
    if (info->snapshots == 0) {
        std::cout << "  NO SNAPSHOTS -- nothing in this file ever places a player,"
                     " so it will play back as an empty room.\n";
        return EXIT_FAILURE;
    }
    if (has_flag(argc, argv, "-replay")) {
        const std::string rom_path = value_after(argc, argv, "-rom");
        if (rom_path.empty()) {
            std::cout << "  replay needs -rom FILE so the native session can "
                         "load the recorded room\n";
            return EXIT_FAILURE;
        }

        auto replay_reader = fruityprime::demo::Reader::open(source);
        if (!replay_reader.has_value()) {
            std::cout << "  replay could not reopen the demo\n";
            return EXIT_FAILURE;
        }
        std::vector<fruityprime::demo::Record> replay_records;
        replay_records.reserve(info->records);
        std::optional<fruityprime::net::MatchStatePacket> first_match;
        while (auto record = replay_reader->read_next()) {
            if (!record->data.empty()
                && record->data.front() == static_cast<std::uint8_t>(
                    fruityprime::net::PacketType::MatchState)
                && !first_match.has_value()) {
                const std::span<const std::uint8_t> packet(record->data);
                first_match = fruityprime::net::MatchStatePacket::decode(
                    packet.subspan(1));
            }
            replay_records.push_back(std::move(*record));
        }

        if (!first_match.has_value() || first_match->room_key.empty()) {
            std::cout << "  replay failed: no usable MatchState/room was "
                         "recorded\n";
            return EXIT_FAILURE;
        }

        std::cout << "  --- replayed through native gameplay::Session ---\n";
        const auto assets = fruityprime::assets::Store::from_rom(rom_path);
        std::unique_ptr<fruityprime::scene::Room> room;
        std::unique_ptr<fruityprime::gameplay::Session> session;
        std::string loaded_room;
        fruityprime::net::RosterPacket roster;
        bool have_roster = false;
        std::array<std::uint8_t, fruityprime::net::NetConfig::SlotCapacity>
            hunters{};
        std::size_t room_loads = 0;
        std::size_t invalid_packets = 0;
        std::size_t intents_applied = 0;
        std::size_t snapshots_applied = 0;
        std::size_t intents_out_of_order = 0;
        std::size_t snapshots_out_of_order = 0;
        std::array<std::uint32_t,
                   fruityprime::net::NetConfig::SlotCapacity> last_intent{};
        std::uint32_t last_snapshot_frame = 0;
        std::uint32_t late_snapshot_run = 0;

        const auto definition_for_room =
            [](std::string_view room_name) {
                const auto* entry = fruityprime::scene::find_room(
                    room_name);
                if (entry == nullptr) {
                    throw std::invalid_argument(
                        "No room with this name is known.");
                }
                return entry->definition;
            };

        const auto add_roster_players =
            [&](fruityprime::gameplay::Session& target) {
                if (!have_roster) {
                    return;
                }
                for (std::size_t i = 0; i < roster.count; ++i) {
                    const std::uint8_t slot = roster.slots[i];
                    if (slot >= fruityprime::net::NetConfig::SlotCapacity
                        || target.has_player(slot)) {
                        continue;
                    }
                    static_cast<void>(target.add_player(slot, hunters[slot]));
                }
            };

        const auto load_match_room =
            [&](const fruityprime::net::MatchStatePacket& state) {
                if (room != nullptr && loaded_room == state.room_key) {
                    session->set_match_mode(state.mode, state.point_goal);
                    session->set_team_mode(
                        fruityprime::match::is_team_mode(state.mode));
                    session->set_friendly_fire(state.friendly_fire());
                    return;
                }
                const auto definition = definition_for_room(state.room_key);
                auto next_room = std::make_unique<fruityprime::scene::Room>(
                    fruityprime::scene::Room::load(assets, definition));
                fruityprime::gameplay::Config config;
                config.mode = state.mode;
                config.point_goal = state.point_goal;
                config.objective_authority = false;
                config.team_mode = fruityprime::match::is_team_mode(state.mode);
                config.friendly_fire = state.friendly_fire();
                config.survival_mode = state.mode == 5 || state.mode == 6;
                config.survival_lives = fruityprime::match::defaults_for_mode(
                    state.mode).point_goal;
                auto next_session =
                    std::make_unique<fruityprime::gameplay::Session>(
                        *next_room, config);
                add_roster_players(*next_session);
                room = std::move(next_room);
                session = std::move(next_session);
                loaded_room = state.room_key;
                ++room_loads;
                last_intent.fill(0);
                last_snapshot_frame = 0;
                late_snapshot_run = 0;
            };

        load_match_room(*first_match);
        const std::uint32_t last_record_frame = replay_records.empty()
            ? 0 : replay_records.back().frame;
        constexpr std::uint64_t max_replay_frames = 60ull * 60ull * 30ull;
        const std::uint64_t requested_frames = replay_records.empty()
            ? 0 : static_cast<std::uint64_t>(last_record_frame) + 1;
        const std::uint64_t frame_count = std::min(
            requested_frames, max_replay_frames);
        const bool replay_truncated = requested_frames > max_replay_frames;
        std::size_t record_cursor = 0;
        std::size_t frames_with_snapshot = 0;
        std::size_t frames_with_several = 0;
        std::size_t longest_gap = 0;
        std::size_t gap = 0;

        for (std::uint64_t frame = 0; frame < frame_count; ++frame) {
            std::vector<fruityprime::net::SnapshotPacket> snapshots_for_frame;
            while (record_cursor < replay_records.size()
                   && replay_records[record_cursor].frame <= frame) {
                const auto& record = replay_records[record_cursor++];
                if (record.data.empty()) {
                    ++invalid_packets;
                    continue;
                }
                const auto packet =
                    std::span<const std::uint8_t>(record.data);
                const auto type = static_cast<fruityprime::net::PacketType>(
                    packet.front());
                switch (type) {
                case fruityprime::net::PacketType::MatchState: {
                    const auto state =
                        fruityprime::net::MatchStatePacket::decode(
                            packet.subspan(1));
                    if (!state) {
                        ++invalid_packets;
                        break;
                    }
                    load_match_room(*state);
                    break;
                }
                case fruityprime::net::PacketType::Roster: {
                    const auto decoded = fruityprime::net::RosterPacket::decode(
                        packet.subspan(1));
                    if (!decoded) {
                        ++invalid_packets;
                        break;
                    }
                    roster = *decoded;
                    have_roster = true;
                    for (std::size_t i = 0; i < roster.count; ++i) {
                        if (roster.slots[i]
                            < fruityprime::net::NetConfig::SlotCapacity) {
                            hunters[roster.slots[i]] = roster.hunters[i];
                        }
                    }
                    if (session != nullptr) {
                        add_roster_players(*session);
                    }
                    break;
                }
                case fruityprime::net::PacketType::SlotIntent: {
                    if (packet.size() < 2 + fruityprime::net::IntentState::Size) {
                        ++invalid_packets;
                        break;
                    }
                    const std::uint8_t slot = packet[1];
                    if (slot >= fruityprime::net::NetConfig::SlotCapacity) {
                        ++invalid_packets;
                        break;
                    }
                    const auto intent = fruityprime::net::IntentState::decode(
                        packet.subspan(2));
                    if (!intent) {
                        ++invalid_packets;
                        break;
                    }
                    if (last_intent[slot] != 0
                        && intent->frame <= last_intent[slot]
                        && last_intent[slot] - intent->frame
                            < fruityprime::net::NetConfig::IntentResetGap) {
                        ++intents_out_of_order;
                        break;
                    }
                    last_intent[slot] = intent->frame;
                    if (session == nullptr) {
                        ++invalid_packets;
                        break;
                    }
                    if (!session->has_player(slot)) {
                        static_cast<void>(session->add_player(
                            slot, hunters[slot]));
                    }
                    session->set_input(slot,
                        fruityprime::gameplay::Input{
                            intent->buttons, intent->aim, intent->weapon_select});
                    ++intents_applied;
                    break;
                }
                case fruityprime::net::PacketType::Snapshot: {
                    const auto snapshot =
                        fruityprime::net::SnapshotPacket::decode(
                            packet.subspan(1));
                    if (!snapshot) {
                        ++invalid_packets;
                        break;
                    }
                    snapshots_for_frame.push_back(*snapshot);
                    break;
                }
                default:
                    break;
                }
            }

            if (session == nullptr) {
                ++invalid_packets;
                continue;
            }
            session->tick();
            std::size_t accepted_this_frame = 0;
            for (const auto& snapshot : snapshots_for_frame) {
                if (last_snapshot_frame != 0
                    && snapshot.header.frame <= last_snapshot_frame
                    && last_snapshot_frame - snapshot.header.frame < 600) {
                    ++snapshots_out_of_order;
                    if (++late_snapshot_run < 12) {
                        continue;
                    }
                }
                late_snapshot_run = 0;
                last_snapshot_frame = snapshot.header.frame;
                session->apply_snapshot(snapshot);
                ++snapshots_applied;
                ++accepted_this_frame;
            }
            if (accepted_this_frame == 0) {
                longest_gap = std::max(longest_gap, ++gap);
            } else {
                gap = 0;
                ++frames_with_snapshot;
                if (accepted_this_frame > 1) {
                    ++frames_with_several;
                }
            }
        }

        std::size_t alive = 0;
        if (session != nullptr) {
            for (const auto& player : session->players()) {
                if (player.health > 0
                    && (player.flags & fruityprime::net::PlayerState::FlagActive)
                        != 0) {
                    ++alive;
                }
            }
        }
        std::cout << "  room=\"" << loaded_room << "\" loads=" << room_loads
                  << " frames=" << frame_count
                  << " snapshots=" << snapshots_applied
                  << " frame_snapshots=" << frames_with_snapshot
                  << " several=" << frames_with_several
                  << " longest_gap=" << longest_gap
                  << " intents=" << intents_applied
                  << " alive=" << alive << '\n';
        if (replay_reader->had_deflate_error()) {
            std::cout << "  warning: the compressed tail was truncated; "
                         "replayed the complete prefix\n";
        }
        if (replay_truncated) {
            std::cout << "  replay stopped at the 30-minute safety limit\n";
        }
        if (invalid_packets != 0 || intents_out_of_order != 0
            || snapshots_out_of_order != 0) {
            std::cout << "  discarded malformed/out-of-order records: "
                      << invalid_packets + intents_out_of_order
                             + snapshots_out_of_order << '\n';
        }
        const std::size_t replayed_frames = static_cast<std::size_t>(
            frame_count);
        const bool unhealthy = replay_truncated || invalid_packets != 0
            || frames_with_several > replayed_frames / 20
            || longest_gap > 10;
        return unhealthy ? EXIT_FAILURE : EXIT_SUCCESS;
    }
    return EXIT_SUCCESS;
}

int run_room_command(int argc, char** argv) {
    const std::string source = value_after(argc, argv, "-roominfo");
    if (source.empty()) {
        throw std::invalid_argument("-roominfo needs a ROM or asset directory");
    }
    const auto assets = fruityprime::assets::Store::from_path(source);
    const std::string room_name = value_after(argc, argv, "-room").empty()
        ? "UNIT1_C0" : value_after(argc, argv, "-room");
    const auto* catalog_entry = fruityprime::scene::find_room(
        room_name);
    fruityprime::scene::RoomDefinition definition = catalog_entry != nullptr
        ? catalog_entry->definition
        : fruityprime::scene::RoomDefinition{
            room_name,
            "archives/unit1_C0.arc",
            "unit1_c0_model.bin",
            "levels/textures/unit1_c0_tex.bin",
            "unit1_c0_collision.bin",
            "levels/entities/Unit1_C0_Ent.bin", {}, {}, {}
        };
    if (!value_after(argc, argv, "-archive").empty()) {
        definition.model_archive = value_after(argc, argv, "-archive");
    }
    if (!value_after(argc, argv, "-model-entry").empty()) {
        definition.model_entry = value_after(argc, argv, "-model-entry");
    }
    if (!value_after(argc, argv, "-texture").empty()) {
        definition.texture_path = value_after(argc, argv, "-texture");
    }
    if (!value_after(argc, argv, "-collision-entry").empty()) {
        definition.collision_entry = value_after(argc, argv, "-collision-entry");
    }
    if (!value_after(argc, argv, "-entity").empty()) {
        definition.entity_path = value_after(argc, argv, "-entity");
    }
    const auto room = fruityprime::scene::Room::load(assets, definition);
    std::cout << "room=\"" << room.definition().name << "\""
              << " meshes=" << room.model().meshes().size()
              << " nodes=" << room.model().nodes().size()
              << " entities=" << room.entities().size()
              << " collision="
              << (room.collision().is_mph() ? "MPH" : "FirstHunt") << '\n';
    return EXIT_SUCCESS;
}

int run_archive_command(int argc, char** argv) {
    const std::string info_path = value_after(argc, argv, "-archive-info");
    const std::string extract_path = value_after(argc, argv, "-extract-archive");
    const std::string source = !info_path.empty() ? info_path : extract_path;
    if (source.empty()) {
        throw std::invalid_argument("archive command needs a file");
    }
    const auto archive = fruityprime::archive::Archive::read_file(source);
    if (!info_path.empty()) {
        std::cout << "SNDFILE " << archive.entries().size() << " file(s)\n";
        for (const auto& entry : archive.entries()) {
            std::cout << "  " << entry.filename << " " << entry.target_size
                      << " bytes\n";
        }
        return EXIT_SUCCESS;
    }
    const std::string output = value_after(argc, argv, "-out");
    if (output.empty()) {
        throw std::invalid_argument("-extract-archive needs -out DIR");
    }
    std::cout << "extracted " << archive.extract(output) << " file(s) to "
              << output << '\n';
    return EXIT_SUCCESS;
}

int run_collision_command(int argc, char** argv) {
    const std::string source = value_after(argc, argv, "-collision-info");
    if (source.empty()) {
        throw std::invalid_argument("-collision-info needs a file");
    }
    const auto collision = fruityprime::collision::File::read_file(source);
    if (collision.is_mph()) {
        const auto& data = collision.mph();
        std::cout << "format=MPH type="
                  << std::string(data.header.type.begin(), data.header.type.end())
                  << " points=" << data.points.size()
                  << " planes=" << data.planes.size()
                  << " data=" << data.data.size()
                  << " entries=" << data.entries.size()
                  << " portals=" << data.portals.size() << '\n';
    } else {
        const auto& data = collision.first_hunt();
        std::cout << "format=FirstHunt points=" << data.points.size()
                  << " planes=" << data.planes.size()
                  << " vectors=" << data.vectors.size()
                  << " data=" << data.data.size()
                  << " entries=" << data.entries.size()
                  << " tree_nodes=" << data.tree_nodes.size()
                  << " portals=" << data.portals.size() << '\n';
    }
    return EXIT_SUCCESS;
}

int run_collision_repack_command(int argc, char** argv) {
    const std::string source = value_after(argc, argv, "-collision-repack");
    if (source.empty()) {
        throw std::invalid_argument("-collision-repack needs a file");
    }
    const std::string destination = value_after(argc, argv, "-out");
    if (destination.empty()) {
        throw std::invalid_argument("-collision-repack needs -out FILE");
    }
    const auto input = fruityprime::read::file(source);
    const auto collision = fruityprime::collision::File::from_bytes(input);
    const auto output = fruityprime::utility::repack_collision::repack(collision);
    const std::filesystem::path destination_path(destination);
    if (const auto parent = destination_path.parent_path(); !parent.empty()) {
        std::error_code error;
        std::filesystem::create_directories(parent, error);
        if (error) {
            throw std::runtime_error("could not create collision output directory: "
                                     + error.message());
        }
    }
    if (!fruityprime::read::write_file(destination_path, output)) {
        throw std::runtime_error("could not write repacked collision file: "
                                 + destination);
    }
    std::cout << "format=" << (collision.is_mph() ? "MPH" : "FirstHunt")
              << " input_bytes=" << input.size()
              << " output_bytes=" << output.size()
              << " output=\"" << destination << "\"\n";
    return EXIT_SUCCESS;
}

int run_camera_sequence_command(int argc, char** argv) {
    const std::string source = value_after(argc, argv, "-camseq-info");
    if (source.empty()) {
        throw std::invalid_argument("-camseq-info needs a ROM or asset directory");
    }
    std::string sequence_path = value_after(argc, argv, "-sequence");
    if (sequence_path.empty()) {
        sequence_path = "cameraEditor/unit1_land_intro.bin";
    } else if (!sequence_path.starts_with("cameraEditor/")) {
        sequence_path = "cameraEditor/" + sequence_path;
    }
    const auto store = fruityprime::assets::Store::from_path(source);
    const auto sequence = fruityprime::camera::File::from_bytes(
        store.bytes(sequence_path), -1, sequence_path);
    std::cout << "file=\"" << sequence_path << '"'
              << " version=" << static_cast<int>(sequence.version())
              << " keyframes=" << sequence.keyframes().size()
              << " duration=" << sequence.duration() << "\n";
    if (!sequence.keyframes().empty()) {
        const auto first = sequence.sample(0, 0.0F);
        std::cout << "  first_node=\"" << sequence.keyframes().front().node_name
                  << "\" position=" << first.position.x << ','
                  << first.position.y << ',' << first.position.z
                  << " target=" << first.target.x << ',' << first.target.y
                  << ',' << first.target.z << " fov=" << first.fov << '\n';
    }
    return EXIT_SUCCESS;
}

int run_entity_command(int argc, char** argv) {
    const std::string source = value_after(argc, argv, "-entity-info");
    if (source.empty()) {
        throw std::invalid_argument("-entity-info needs a file");
    }
    const auto entities = fruityprime::entity::File::read_file(source);
    if (entities.is_first_hunt()) {
        std::cout << "format=FirstHunt version=" << entities.version()
                  << " records=" << entities.first_hunt_records().size()
                  << '\n';
        for (const auto& record : entities.first_hunt_records()) {
            std::cout << "  id=" << record.header.entity_id
                      << " type=" << record.header.type
                      << " node=\"" << record.entry.node_name << "\"\n";
        }
    } else {
        std::cout << "format=MPH version=" << entities.version()
                  << " records=" << entities.records().size() << '\n';
        for (const auto& record : entities.records()) {
            std::cout << "  id=" << record.header.entity_id
                      << " type=" << record.header.type
                      << " layer_mask=" << record.entry.layer_mask
                      << " bytes=" << record.payload.size()
                      << " node=\"" << record.entry.node_name << "\"\n";
        }
    }
    return EXIT_SUCCESS;
}

int run_entity_repack_command(int argc, char** argv) {
    const std::string source = value_after(argc, argv, "-entity-repack");
    if (source.empty()) {
        throw std::invalid_argument("-entity-repack needs a file");
    }
    const std::string destination = value_after(argc, argv, "-out");
    if (destination.empty()) {
        throw std::invalid_argument("-entity-repack needs -out FILE");
    }
    const auto input = fruityprime::read::file(source);
    const auto output = fruityprime::utility::entity_repack::repack(input);
    const std::filesystem::path destination_path(destination);
    if (const auto parent = destination_path.parent_path(); !parent.empty()) {
        std::error_code error;
        std::filesystem::create_directories(parent, error);
        if (error) {
            throw std::runtime_error("could not create entity output directory: "
                                     + error.message());
        }
    }
    if (!fruityprime::read::write_file(destination_path, output)) {
        throw std::runtime_error("could not write repacked entity file: "
                                 + destination);
    }
    const auto comparison = fruityprime::utility::entity_repack::compare(
        input, output);
    const auto parsed = fruityprime::entity::File::from_bytes(output);
    const std::size_t records = parsed.is_first_hunt()
        ? parsed.first_hunt_records().size() : parsed.records().size();
    std::cout << "format=" << (parsed.is_first_hunt() ? "FirstHunt" : "MPH")
              << " records=" << records
              << " input_bytes=" << input.size()
              << " output_bytes=" << output.size()
              << " byte_equal=" << (comparison.equal ? "true" : "false")
              << " differing_bytes=" << comparison.differing_bytes << '\n';
    return EXIT_SUCCESS;
}

int run_model_command(int argc, char** argv) {
    const std::string source = value_after(argc, argv, "-model-info");
    if (source.empty()) {
        throw std::invalid_argument("-model-info needs a file");
    }
    const auto model = fruityprime::model::File::read_file(source);
    const auto& header = model.header();
    std::cout << "scale=" << model.world_scale()
              << " meshes=" << model.meshes().size()
              << " materials=" << model.materials().size()
              << " nodes=" << model.nodes().size()
              << " instructions=" << model.instructions().size()
              << " textures=" << model.textures().size()
              << " palettes=" << model.palettes().size() << '\n';
    for (std::size_t i = 0; i < model.textures().size(); ++i) {
        const auto& texture = model.textures()[i];
        std::cout << "  texture[" << i << "] format="
                  << static_cast<int>(texture.format)
                  << " size=" << texture.width << 'x' << texture.height
                  << " image=" << texture.image_size << '\n';
    }
    std::cout << "  header primitive_count=" << header.primitive_count
              << " vertex_count=" << header.vertex_count << '\n';
    std::size_t decoded_primitives = 0;
    std::size_t decoded_vertices = 0;
    float largest_coordinate = 0.0F;
    for (const auto& mesh : model.meshes()) {
        int texture_width = 0;
        int texture_height = 0;
        bool texgen = false;
        if (mesh.material_id < model.materials().size()) {
            const auto& material = model.materials()[mesh.material_id];
            texgen = material.texcoord_transform_mode == 2;
            if (material.texture_id >= 0
                && static_cast<std::size_t>(material.texture_id)
                    < model.textures().size()) {
                texture_width = model.textures()[material.texture_id].width;
                texture_height = model.textures()[material.texture_id].height;
            }
        }
        for (const auto& primitive : model.decode_geometry(
                 mesh.display_list_id, texture_width, texture_height, texgen)) {
            ++decoded_primitives;
            decoded_vertices += primitive.vertices.size();
            for (const auto& vertex : primitive.vertices) {
                largest_coordinate = std::max(
                    largest_coordinate, std::abs(vertex.x));
                largest_coordinate = std::max(
                    largest_coordinate, std::abs(vertex.y));
                largest_coordinate = std::max(
                    largest_coordinate, std::abs(vertex.z));
            }
        }
    }
    std::cout << "  decoded_primitives=" << decoded_primitives
              << " decoded_vertices=" << decoded_vertices
              << " largest_coordinate=" << largest_coordinate << '\n';
    return EXIT_SUCCESS;
}

int run_model_repack_command(int argc, char** argv) {
    const std::string source = value_after(argc, argv, "-model-repack");
    if (source.empty()) {
        throw std::invalid_argument("-model-repack needs a model file");
    }
    const std::string destination = value_after(argc, argv, "-out");
    if (destination.empty()) {
        throw std::invalid_argument("-model-repack needs -out FILE");
    }

    fruityprime::utility::repack_model::ModelPackOptions options;
    options.texture = has_flag(argc, argv, "-separate-textures")
        ? fruityprime::utility::repack_model::TextureStorage::Separate
        : fruityprime::utility::repack_model::TextureStorage::Inline;
    options.is_room = has_flag(argc, argv, "-room");
    const std::string bounds = value_after(argc, argv, "-bounds");
    if (bounds.empty() || bounds == "none") {
        options.bounds = fruityprime::utility::repack_model::BoundsMode::None;
    } else if (bounds == "capped") {
        options.bounds = fruityprime::utility::repack_model::BoundsMode::Capped;
    } else if (bounds == "uncapped") {
        options.bounds = fruityprime::utility::repack_model::BoundsMode::Uncapped;
    } else {
        throw std::invalid_argument(
            "-bounds must be none, capped, or uncapped");
    }

    const auto input = fruityprime::read::file(source);
    const auto model = fruityprime::model::File::from_bytes(input);
    const auto packed = fruityprime::utility::repack_model::pack_model(
        model, options);
    const auto write_output = [](const std::filesystem::path& path,
                                 const std::vector<std::uint8_t>& bytes,
                                 const char* description) {
        if (const auto parent = path.parent_path(); !parent.empty()) {
            std::error_code error;
            std::filesystem::create_directories(parent, error);
            if (error) {
                throw std::runtime_error(
                    std::string("could not create ") + description
                    + " output directory: " + error.message());
            }
        }
        if (!fruityprime::read::write_file(path, bytes)) {
            throw std::runtime_error(std::string("could not write ")
                                     + description + " output: " + path.string());
        }
    };

    const std::filesystem::path destination_path(destination);
    write_output(destination_path, packed.model, "model");
    std::filesystem::path texture_path(
        value_after(argc, argv, "-texture-out"));
    if (options.texture
        == fruityprime::utility::repack_model::TextureStorage::Separate) {
        if (texture_path.empty()) {
            texture_path = destination_path;
            texture_path.replace_filename(
                destination_path.stem().string() + "_Tex"
                + destination_path.extension().string());
        }
        write_output(texture_path, packed.texture, "texture");
    }
    std::cout << "format=model input_bytes=" << input.size()
              << " output_bytes=" << packed.model.size()
              << " textures=" << model.textures().size()
              << " palettes=" << model.palettes().size()
              << " storage=" << (packed.texture.empty() ? "inline" : "separate")
              << " output=\"" << destination_path.string() << '"';
    if (!packed.texture.empty()) {
        std::cout << " texture_output=\"" << texture_path.string() << '"';
    }
    std::cout << '\n';
    return EXIT_SUCCESS;
}

int run_model_export_command(int argc, char** argv) {
    const bool collada = has_flag(argc, argv, "-model-export-collada");
    const bool scripting = has_flag(argc, argv, "-model-export-script");
    const bool textures = has_flag(argc, argv, "-model-export-textures");
    const std::string command = scripting ? "-model-export-script"
        : textures ? "-model-export-textures"
        : collada ? "-model-export-collada" : "-model-export-obj";
    const std::string source = value_after(argc, argv, command);
    if (source.empty()) {
        throw std::invalid_argument(command + " needs a model file");
    }
    const std::string output = value_after(argc, argv, "-out");
    if (output.empty()) {
        throw std::invalid_argument(command + " needs -out FILE");
    }
    const auto model = fruityprime::model::File::read_file(source);
    if (scripting) {
        fruityprime::exporter::ScriptingOptions options;
        const auto source_path = std::filesystem::path(source);
        options.model_name = value_after(argc, argv, "-model-name");
        if (options.model_name.empty()) {
            options.model_name = source_path.stem().string();
        }
        const std::string export_root = value_after(argc, argv, "-export-root");
        options.export_root = export_root.empty()
            ? std::filesystem::path(output).parent_path()
            : std::filesystem::path(export_root);
        const auto recolors = values_after(argc, argv, "-recolor");
        if (!recolors.empty()) {
            options.recolor_names = recolors;
        }
        fruityprime::exporter::write_script(model, output, options);
        std::cout << "exported Blender script=\"" << output
                  << "\" model=\"" << options.model_name
                  << "\" recolors=" << options.recolor_names.size() << '\n';
        return EXIT_SUCCESS;
    }
    if (textures) {
        const auto stats = fruityprime::exporter::write_model_textures(
            model, output);
        std::cout << "exported textures=\"" << output << "\""
                  << " images=" << stats.images
                  << " pixels=" << stats.pixels << '\n';
        return EXIT_SUCCESS;
    }
    const auto stats = collada
        ? fruityprime::exporter::write_collada(model, output)
        : fruityprime::exporter::write_obj(model, output);
    std::cout << "exported " << (collada ? "COLLADA" : "OBJ")
              << "=\"" << output << "\"";
    if (!collada) {
        std::cout << " MTL=\"";
        auto material = std::filesystem::path(output);
        material.replace_extension(".mtl");
        std::cout << material.string() << '\"';
    }
    std::cout
              << " meshes=" << stats.meshes
              << " primitives=" << stats.primitives
              << " vertices=" << stats.vertices
              << " faces=" << stats.faces << '\n';
    return EXIT_SUCCESS;
}

int run_sound_command(int argc, char** argv) {
    const bool sequence_mode = has_flag(argc, argv, "-sseqinfo");
    const std::string source = value_after(
        argc, argv, sequence_mode ? "-sseqinfo" : "-soundinfo");
    if (source.empty()) {
        throw std::invalid_argument(
            std::string(sequence_mode ? "-sseqinfo" : "-soundinfo")
                + " needs a ROM or asset directory");
    }
    const auto catalog = fruityprime::sound::Catalog::load(
        fruityprime::assets::Store::from_path(source));
    std::size_t present_samples = 0;
    for (const auto& sample : catalog.samples) {
        if (sample.present) {
            ++present_samples;
        }
    }
    std::size_t present_wfs_samples = 0;
    for (const auto& sample : catalog.wfs_samples) {
        if (sample.present) {
            ++present_wfs_samples;
        }
    }
    std::cout << "sound samples=" << catalog.samples.size()
              << " present=" << present_samples
              << " wfs_samples=" << catalog.wfs_samples.size()
              << " wfs_present=" << present_wfs_samples
              << " bgm=" << catalog.bgm_select.size()
              << " sfx=" << catalog.sfx_select.size()
              << " sound3d=" << catalog.sound_3d.size()
              << " tables=" << catalog.sound_tables.entries.size()
              << " room_music=" << catalog.room_music.size()
              << " tracks=" << catalog.music_tracks.size()
              << " scripts=" << catalog.sfx_script_files.size()
              << " dgn=" << catalog.dgn_files.size()
              << " streams=" << catalog.streams.size() << '\n';
    if (sequence_mode) {
        if (!catalog.sdat.has_value()) {
            throw std::runtime_error("sound catalog has no SDAT");
        }
        const int requested_sequence = integer_after(
            argc, argv, "-sequence", -1);
        std::size_t sequence_index = 0;
        if (requested_sequence >= 0) {
            sequence_index = static_cast<std::size_t>(requested_sequence);
            if (sequence_index >= catalog.sdat->sequences().size()
                || !catalog.sdat->sequences()[sequence_index].present) {
                throw std::invalid_argument(
                    "-sequence is outside the usable SDAT sequence table");
            }
        } else {
            bool found = false;
            for (std::size_t index = 0;
                 index < catalog.sdat->sequences().size(); ++index) {
                if (catalog.sdat->sequences()[index].present
                    && catalog.sdat->sequences()[index].bank
                        < catalog.sdat->banks().size()
                    && catalog.sdat->banks()[catalog.sdat->sequences()[index].bank]
                        .present) {
                    sequence_index = index;
                    found = true;
                    break;
                }
            }
            if (!found) {
                throw std::runtime_error(
                    "sound catalog has no usable SSEQ/SBNK pair");
            }
        }
        const double seconds = seconds_value(
            value_after(argc, argv, "-seconds")).value_or(1.0);
        if (!(seconds > 0.0) || !std::isfinite(seconds)) {
            throw std::invalid_argument("-seconds must be finite and > 0");
        }
        fruityprime::sound::SseqPlayer player(*catalog.sdat, sequence_index);
        constexpr std::size_t max_ticks = 8192;
        constexpr std::size_t max_events = 10000;
        const auto timeline = player.timeline(max_ticks, max_events);
        const auto rendered = player.render(
            seconds, fruityprime::sound::SseqRenderOptions{
                22050, 1.0F, max_ticks, max_events
            });
        float peak = 0.0F;
        for (const float sample : rendered) {
            peak = std::max(peak, std::abs(sample));
        }
        std::cout << "SSEQ sequence=" << sequence_index
                  << " ticks=" << timeline.stats.ticks
                  << " commands=" << timeline.stats.commands
                  << " notes=" << timeline.notes.size()
                  << " tempo=" << timeline.stats.final_tempo
                  << " ended=" << (timeline.stats.ended ? "yes" : "no")
                  << " truncated=" << (timeline.stats.truncated ? "yes" : "no")
                  << " rendered_frames=" << rendered.size() / 2
                  << " peak=" << std::fixed << std::setprecision(6)
                  << peak << '\n';
    }
    return EXIT_SUCCESS;
}

int run_movie_command(int argc, char** argv) {
    const bool export_mode = has_flag(argc, argv, "-movieexport")
        || (has_flag(argc, argv, "-export")
            && value_after(argc, argv, "-export") == "movie");
    std::string source = value_after(argc, argv, "-movieinfo");
    if (source.empty()) {
        source = value_after(argc, argv, "-movieexport");
    }
    if (source.empty()) {
        source = value_after(argc, argv, "-rom");
    }
    if (source.empty()) {
        throw std::invalid_argument(
            "movie command needs a ROM, asset directory, or VX file");
    }
    const std::string requested = value_after(argc, argv, "-movie");
    const auto matches = [&requested](std::string_view name) {
        if (requested.empty()) {
            return true;
        }
        std::string normalized(name);
        std::replace(normalized.begin(), normalized.end(), '\\', '/');
        std::string wanted = requested;
        std::replace(wanted.begin(), wanted.end(), '\\', '/');
        return normalized == wanted
            || std::filesystem::path(normalized).filename().generic_string()
                == std::filesystem::path(wanted).filename().generic_string();
    };
    const std::filesystem::path source_path(source);
    const std::string output_value = value_after(argc, argv, "-out");
    const auto output_root = output_value.empty()
        ? executable_directory(argc > 0 ? argv[0] : "FruityPrime")
            / "export" / "_movies"
        : std::filesystem::path(output_value);
    std::size_t selected = 0;
    const auto consume = [&](std::string name,
                             fruityprime::movie::VxDecoder decoder) {
        if (!matches(name)) {
            return;
        }
        decoder.decode();
        const auto& header = decoder.header();
        if (export_mode) {
            const std::string stem = std::filesystem::path(name).stem().string();
            const auto stats = fruityprime::exporter::write_movie(
                decoder, output_root / stem);
            std::cout << "exported movie=\"" << name << '"'
                      << " frames=" << stats.frames
                      << " audio_frames=" << stats.audio_frames
                      << " output=\"" << (output_root / stem).string()
                      << "\"\n";
        } else {
            std::cout << "movie=\"" << name << '"'
                      << " width=" << header.frame_width
                      << " height=" << header.frame_height
                      << " fps=" << std::fixed << std::setprecision(4)
                      << header.frame_rate
                      << " frames=" << decoder.frame_count()
                      << " audio_frames=" << decoder.audio_frame_total()
                      << " sample_rate=" << header.audio_sample_rate << '\n';
        }
        ++selected;
    };

    std::error_code source_error;
    if (std::filesystem::is_regular_file(source_path, source_error)
        && !source_error && (source_path.extension() == ".vx"
                             || source_path.extension() == ".VX")) {
        consume(source_path.filename().generic_string(),
                fruityprime::movie::VxDecoder::read_file(source_path));
    } else if (source_path.extension() == ".nds"
               || source_path.extension() == ".NDS") {
        const auto rom = fruityprime::nds::Rom::read_file(source_path);
        for (const auto& file : rom.files()) {
            if (file.path.rfind("movies/", 0) != 0
                || (file.path.size() < 3
                    || !file.path.ends_with(".vx"))) {
                continue;
            }
            consume(file.path, fruityprime::movie::VxDecoder::from_bytes(
                rom.file(file.file_id)));
        }
    } else if (std::filesystem::is_directory(source_path, source_error)
               && !source_error) {
        const auto movies_root = source_path / "movies";
        if (!std::filesystem::is_directory(movies_root, source_error)
            || source_error) {
            throw std::invalid_argument(
                "asset directory has no movies subdirectory: "
                + source_path.string());
        }
        std::filesystem::recursive_directory_iterator iterator(
            movies_root, std::filesystem::directory_options::skip_permission_denied,
            source_error);
        const std::filesystem::recursive_directory_iterator end;
        while (iterator != end && !source_error) {
            if (iterator->is_regular_file(source_error)
                && (iterator->path().extension() == ".vx"
                    || iterator->path().extension() == ".VX")) {
                const auto relative = std::filesystem::relative(
                    iterator->path(), source_path, source_error);
                if (!source_error) {
                    consume(relative.generic_string(),
                            fruityprime::movie::VxDecoder::read_file(
                                iterator->path()));
                }
            }
            iterator.increment(source_error);
        }
        if (source_error) {
            throw std::runtime_error("could not enumerate movies: "
                                     + source_error.message());
        }
    } else {
        throw std::invalid_argument("movie source does not exist: " + source);
    }
    if (selected == 0) {
        throw std::invalid_argument("movie file was not found: "
                                    + (requested.empty() ? source : requested));
    }
    return EXIT_SUCCESS;
}

[[nodiscard]] std::filesystem::path find_map_definition(
    int argc, char** argv, std::string_view requested) {
    const std::filesystem::path direct(requested);
    std::error_code error;
    if (std::filesystem::is_regular_file(direct, error) && !error) {
        return direct;
    }

    const auto normalize = [](std::string_view value) {
        std::string result;
        result.reserve(value.size());
        for (const char character : value) {
            result.push_back(static_cast<char>(std::tolower(
                static_cast<unsigned char>(character))));
        }
        return result;
    };
    const std::string wanted = normalize(requested);
    std::vector<std::filesystem::path> roots;
    const auto add_ancestors = [&roots](std::filesystem::path start) {
        std::error_code absolute_error;
        start = std::filesystem::absolute(start, absolute_error);
        if (absolute_error) {
            return;
        }
        for (;;) {
            roots.push_back(start / "maps");
            const auto parent = start.parent_path();
            if (parent == start) {
                break;
            }
            start = parent;
        }
    };
    add_ancestors(std::filesystem::current_path());
    if (argc > 0) {
        add_ancestors(executable_directory(argv[0]));
    }
    for (const auto& root : roots) {
        std::error_code iterator_error;
        if (!std::filesystem::is_directory(root, iterator_error)
            || iterator_error) {
            continue;
        }
        std::filesystem::recursive_directory_iterator iterator(
            root, std::filesystem::directory_options::skip_permission_denied,
            iterator_error);
        const std::filesystem::recursive_directory_iterator end;
        while (iterator != end && !iterator_error) {
            if (iterator->is_regular_file(iterator_error)
                && iterator->path().extension() == ".json") {
                try {
                    const auto definition = fruityprime::mapgen::load_definition(
                        iterator->path());
                    if (normalize(definition.name) == wanted) {
                        return iterator->path();
                    }
                } catch (const std::exception&) {
                    // A malformed example in the map tree must not prevent a
                    // different named recipe from being found.
                }
            }
            iterator.increment(iterator_error);
        }
    }
    throw std::invalid_argument("map definition was not found: "
                                + std::string(requested));
}

int run_mapgen_command(int argc, char** argv) {
    const std::string requested = value_after(argc, argv, "-mapgen");
    if (requested.empty()) {
        throw std::invalid_argument("-mapgen needs MAP_JSON or a map name");
    }
    const auto definition_path = find_map_definition(argc, argv, requested);
    const auto definition = fruityprime::mapgen::load_definition(definition_path);
    const auto generated = fruityprime::mapgen::build(definition);
    const std::string output_value = value_after(argc, argv, "-out");
    const auto output = output_value.empty()
        ? fruityprime::mapgen::custom_rooms::generated_directory(definition)
        : fruityprime::utility::console::resolve_launch_path(output_value);
    const auto entity_output = output_value.empty()
        ? fruityprime::mapgen::custom_rooms::entity_directory()
        : output;
    const auto node_output = output_value.empty()
        ? fruityprime::mapgen::custom_rooms::node_directory()
        : output;
    fruityprime::mapgen::write_generated(definition, generated, output,
                                         entity_output, node_output);
    const auto prefix = fruityprime::mapgen::file_prefix(definition);
    std::cout << "mapgen name=\"" << definition.name << '"'
              << " prefix=\"" << prefix << '"'
              << " model_faces=" << generated.stats.model_faces
              << " collision_faces=" << generated.stats.collision_faces
              << " entities=" << generated.stats.entities
              << " nodes=" << generated.stats.navigation_nodes
              << " edges=" << generated.stats.navigation_edges
              << " output=\"" << output.string() << '"' << '\n';
    if (!definition.import_source.empty()) {
        std::cout << "mapgen note=Q3 BSP/PK3 geometry, collision, entities, and "
                     "named/auto-baked FPTX textures were converted; source "
                     "image baking is automatic on Windows when needed\n";
    }
    return EXIT_SUCCESS;
}

int run_mapbundle_command(int argc, char** argv) {
    const std::string requested = value_after(argc, argv, "-mapbundle");
    const std::string mapdir_value = value_after(argc, argv, "-mapdir");
    std::filesystem::path map_directory = mapdir_value.empty()
        ? fruityprime::utility::console::resolve_launch_path("maps")
        : fruityprime::utility::console::resolve_launch_path(mapdir_value);
    std::error_code path_error;
    map_directory = std::filesystem::absolute(map_directory, path_error);
    if (path_error) {
        throw std::runtime_error("could not resolve map directory: "
                                 + path_error.message());
    }

    const auto lower = [](std::string_view value) {
        std::string result;
        result.reserve(value.size());
        for (const char character : value) {
            result.push_back(static_cast<char>(std::tolower(
                static_cast<unsigned char>(character))));
        }
        return result;
    };
    const auto extension_is = [&lower](const std::filesystem::path& path,
                                       std::string_view extension) {
        return lower(path.extension().string()) == lower(extension);
    };
    const auto regular_file = [](const std::filesystem::path& path) {
        std::error_code error;
        return std::filesystem::is_regular_file(path, error) && !error;
    };

    std::vector<std::filesystem::path> bundles;
    std::vector<std::filesystem::path> recipes;
    std::error_code directory_error;
    if (std::filesystem::is_directory(map_directory, directory_error)
        && !directory_error) {
        std::filesystem::recursive_directory_iterator iterator(
            map_directory, std::filesystem::directory_options::skip_permission_denied,
            directory_error);
        const std::filesystem::recursive_directory_iterator end;
        while (iterator != end && !directory_error) {
            std::error_code entry_error;
            if (iterator->is_regular_file(entry_error) && !entry_error) {
                const auto& path = iterator->path();
                if (extension_is(path, fruityprime::mapgen::bundle::Extension)) {
                    bundles.push_back(path);
                } else if (extension_is(path, ".json")) {
                    recipes.push_back(path);
                }
            }
            iterator.increment(directory_error);
        }
        if (directory_error) {
            throw std::runtime_error("could not enumerate map directory: "
                                     + directory_error.message());
        }
    } else if (directory_error) {
        throw std::runtime_error("could not inspect map directory: "
                                 + directory_error.message());
    }
    std::sort(bundles.begin(), bundles.end());
    std::sort(recipes.begin(), recipes.end());

    // A shipped bundle wins over the working recipe with the same stem, just
    // as CustomRooms.MapFiles does. This prevents -mapbundle all from cooking
    // a second copy of a map that is already packaged.
    std::vector<std::string> bundle_stems;
    bundle_stems.reserve(bundles.size());
    for (const auto& path : bundles) {
        bundle_stems.push_back(lower(path.stem().string()));
    }
    recipes.erase(std::remove_if(recipes.begin(), recipes.end(),
                                 [&bundle_stems, &lower](const auto& path) {
        return std::find(bundle_stems.begin(), bundle_stems.end(),
                         lower(path.stem().string())) != bundle_stems.end();
    }), recipes.end());

    std::vector<std::filesystem::path> candidates;
    const bool all = requested.empty() || lower(requested) == "all";
    if (!all) {
        const auto direct = fruityprime::utility::console::resolve_launch_path(
            requested);
        if (regular_file(direct)) {
            candidates.push_back(direct);
        } else {
            const std::string wanted = lower(requested);
            const auto consider = [&candidates, &wanted, &lower](
                                      const std::filesystem::path& path) {
                if (lower(path.stem().string()) == wanted) {
                    candidates.push_back(path);
                    return;
                }
                try {
                    const auto definition = fruityprime::mapgen::load_definition(path);
                    if (lower(definition.name) == wanted) {
                        candidates.push_back(path);
                    }
                } catch (const std::exception&) {
                    // A malformed sibling is not a reason to hide a valid
                    // named recipe elsewhere in the map tree.
                }
            };
            for (const auto& path : bundles) {
                consider(path);
            }
            for (const auto& path : recipes) {
                consider(path);
            }
        }
    } else {
        candidates = bundles;
        candidates.insert(candidates.end(), recipes.begin(), recipes.end());
    }

    if (!all && candidates.empty()) {
        throw std::invalid_argument("map to bundle was not found: "
                                    + requested);
    }

    const std::string output_value = value_after(argc, argv, "-out");
    if (!output_value.empty() && candidates.size() > 1) {
        throw std::invalid_argument(
            "-out can name one file only when -mapbundle selects one map");
    }

    int cooked = 0;
    int failed = 0;
    for (const auto& recipe_path : candidates) {
        if (fruityprime::mapgen::bundle::is_bundle(recipe_path)) {
            continue;
        }
        try {
            const auto definition = fruityprime::mapgen::load_definition(recipe_path);
            if (definition.import_source.empty()) {
                continue;
            }
            const auto output = output_value.empty()
                ? map_directory / (recipe_path.stem().string()
                                   + std::string(fruityprime::mapgen::bundle::Extension))
                : fruityprime::utility::console::resolve_launch_path(output_value);
            const auto bundled = fruityprime::mapgen::bundle::cook(
                definition, recipe_path, output, true);
            static_cast<void>(bundled);
            ++cooked;
        } catch (const std::exception& exception) {
            std::cerr << recipe_path.filename().string() << ": "
                      << exception.what() << '\n';
            ++failed;
        }
    }
    if (cooked == 0 && failed == 0) {
        std::cout << "No map to bundle. A bundle is cooked from a recipe and "
                     "the level it converts; put both in "
                  << map_directory.string() << ".\n";
    }
    return failed == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}

int run_q3convert_command(int argc, char** argv) {
    const std::string source = value_after(argc, argv, "-q3convert");
    if (source.empty()) {
        throw std::invalid_argument("-q3convert needs a .bsp or .pk3 file");
    }
    fruityprime::mapgen::Q3ConvertOptions options;
    options.source = source;
    options.map_name = value_after(argc, argv, "-map");
    options.room_name = value_after(argc, argv, "-name");
    const std::string output = value_after(argc, argv, "-out");
    if (!output.empty()) {
        options.output_directory = output;
    }
    options.drop_clip = has_flag(argc, argv, "-noclip");
    options.texture_size = integer_after(argc, argv, "-texsize", 64);
    const std::string scale = value_after(argc, argv, "-scale");
    if (!scale.empty()) {
        try {
            std::size_t consumed = 0;
            options.forced_units_per_unit = std::stof(scale, &consumed);
            if (consumed != scale.size()
                || !std::isfinite(options.forced_units_per_unit)
                || options.forced_units_per_unit <= 0.0F) {
                throw std::invalid_argument("invalid scale");
            }
        } catch (const std::exception&) {
            throw std::invalid_argument("-scale must be a positive number");
        }
    }
    const auto result = fruityprime::mapgen::convert_q3(options);
    std::cout << "q3convert room=\"" << result.room_name << '"'
              << " units_per_unit=" << result.units_per_unit
              << " baked_textures=" << result.baked_textures
              << " spawns=" << result.spawn_count
              << " recipe=\"" << result.recipe_path.string() << '"'
              << " source=\"" << result.level_path.string() << '"'
              << " textures=\"" << result.texture_pack_path.string() << '"'
              << '\n';
    return EXIT_SUCCESS;
}

int run_q3_shaders_command(int argc, char** argv) {
    const std::string source = value_after(argc, argv, "-q3shaders");
    if (source.empty()) {
        throw std::invalid_argument("-q3shaders needs a .bsp or .pk3 file");
    }
    return fruityprime::mapgen::list_shaders(
        std::cout, source, value_after(argc, argv, "-map"));
}

int run_map_materials_command(int argc, char** argv) {
    const std::string room = value_after(argc, argv, "-mapmaterials");
    const std::string rom = value_after(argc, argv, "-rom");
    if (room.empty() || rom.empty()) {
        throw std::invalid_argument("-mapmaterials needs ROOM and -rom FILE");
    }
    const auto assets = fruityprime::assets::Store::from_path(rom);
    return fruityprime::mapgen::list_materials(std::cout, assets, room);
}

} // namespace

int main(int argc, char** argv) {
    std::vector<std::string> mod_arguments;
    if (argc > 1 && argv != nullptr) {
        mod_arguments.reserve(static_cast<std::size_t>(argc - 1));
        for (int index = 1; index < argc; ++index) {
            mod_arguments.emplace_back(argv[index] != nullptr ? argv[index] : "");
        }
    }
    // Program.Main is the sole managed top-level. This executable remains a
    // temporary adapter only for the normal branches whose RenderWindow and
    // export implementations have not yet been moved to Program.cpp; it must
    // not run a second ModEntry dispatch of its own.
    try {
        if (MphReadNative::Program::Main(mod_arguments)
            == MphReadNative::Program::MainResult::Handled) {
            return MphReadNative::Program::ExitCode();
        }
    } catch (const std::exception& error) {
        std::cerr << "[program] fatal: " << error.what() << '\n';
        return EXIT_FAILURE;
    }
    if (has_flag(argc, argv, "-help") || has_flag(argc, argv, "--help")) {
        print_usage(argc > 0 ? argv[0] : "FruityPrime");
        return EXIT_SUCCESS;
    }

    try {
        if (has_flag(argc, argv, "-applyupdate")) {
            return run_apply_update_command(argc, argv);
        }
        fruityprime::update::DesktopUpdate::clean(
            executable_directory(argc > 0 ? argv[0] : "FruityPrime"));
        const std::string mapdir_value = value_after(argc, argv, "-mapdir");
        if (!mapdir_value.empty()) {
            fruityprime::mapgen::custom_rooms::set_map_directory(
                fruityprime::utility::console::resolve_launch_path(mapdir_value));
        }
        if (has_flag(argc, argv, "-masterserver")) {
            fruityprime::MasterOptions options;
            const int requested_port = integer_after(
                argc, argv, "-port", static_cast<int>(options.port));
            options.port = static_cast<std::uint16_t>(
                std::clamp(requested_port, 0, 65'535));
            options.public_address = value_after(argc, argv, "-public");

            const std::string host_ports = value_after(argc, argv, "-hostports");
            if (!host_ports.empty()) {
                int first = 0;
                int last = -1;
                if (!parse_port_range(host_ports, first, last)) {
                    throw std::invalid_argument(
                        "-hostports must be an inclusive range such as 28000-28007");
                }
                options.host_port_first = first;
                options.host_port_last = last;
            }

            fruityprime::MasterServer master(std::move(options));
            g_master = &master;
            fruityprime::mods::ShutdownSignals shutdown_signals;
            shutdown_signals.on_shutdown(stop_services);
            const int seconds = integer_after(argc, argv, "-seconds", 0);
            const auto duration = seconds > 0
                ? std::chrono::seconds(seconds)
                : std::chrono::seconds(0);
            master.run(std::chrono::duration_cast<std::chrono::milliseconds>(duration));
            g_master = nullptr;
            return EXIT_SUCCESS;
        }

        if (has_flag(argc, argv, "-status")) {
            return run_status_command(argc, argv);
        }
        if (has_flag(argc, argv, "-rooms")) {
            return run_rooms_command();
        }
        if (has_flag(argc, argv, "-credits")) {
            fruityprime::credits::print(std::cout);
            return EXIT_SUCCESS;
        }
        if (has_flag(argc, argv, "-mechanics")) {
            fruityprime::mechanics::print(std::cout);
            return EXIT_SUCCESS;
        }
        if (has_flag(argc, argv, "-update")) {
            return run_update_command();
        }
        if (has_flag(argc, argv, "-servers")) {
            return run_servers_command(argc, argv);
        }
        if (has_flag(argc, argv, "-connect")) {
            return run_connect_command(argc, argv);
        }
        if (has_flag(argc, argv, "-gamepad")) {
            double seconds = 15.0;
            if (const auto parsed = seconds_value(
                    value_after(argc, argv, "-seconds"));
                parsed && *parsed > 0.0) {
                seconds = *parsed;
            }
            return fruityprime::input::run_gamepad_probe(seconds);
        }
        if (has_flag(argc, argv, "-netcheck")) {
            return run_netcheck_command(argc, argv);
        }
        if (has_flag(argc, argv, "-demoinfo")) {
            return run_demo_command(argc, argv);
        }
        if (has_flag(argc, argv, "-weapondps")) {
            return run_weapon_dps_command(argc, argv);
        }
        if (has_flag(argc, argv, "-roominfo")) {
            return run_room_command(argc, argv);
        }
        if (has_flag(argc, argv, "-rominfo")
            || has_flag(argc, argv, "-extract-rom")) {
            return fruityprime::utility::extract::run_command(argc, argv);
        }
        if (has_flag(argc, argv, "-archive-info")
            || has_flag(argc, argv, "-extract-archive")) {
            return run_archive_command(argc, argv);
        }
        if (has_flag(argc, argv, "-collision-info")) {
            return run_collision_command(argc, argv);
        }
        if (has_flag(argc, argv, "-collision-repack")) {
            return run_collision_repack_command(argc, argv);
        }
        if (has_flag(argc, argv, "-camseq-info")) {
            return run_camera_sequence_command(argc, argv);
        }
        if (has_flag(argc, argv, "-entity-info")) {
            return run_entity_command(argc, argv);
        }
        if (has_flag(argc, argv, "-entity-repack")) {
            return run_entity_repack_command(argc, argv);
        }
        if (has_flag(argc, argv, "-model-repack")) {
            return run_model_repack_command(argc, argv);
        }
        if (has_flag(argc, argv, "-model-export-obj")
            || has_flag(argc, argv, "-model-export-collada")
            || has_flag(argc, argv, "-model-export-script")
            || has_flag(argc, argv, "-model-export-textures")) {
            return run_model_export_command(argc, argv);
        }
        if (has_flag(argc, argv, "-model-info")) {
            return run_model_command(argc, argv);
        }
        if (has_flag(argc, argv, "-soundinfo")
            || has_flag(argc, argv, "-sseqinfo")) {
            return run_sound_command(argc, argv);
        }
        if (has_flag(argc, argv, "-movieinfo")
            || has_flag(argc, argv, "-movieexport")
            || (has_flag(argc, argv, "-export")
                && value_after(argc, argv, "-export") == "movie")) {
            return run_movie_command(argc, argv);
        }
        if (has_flag(argc, argv, "-q3convert")) {
            return run_q3convert_command(argc, argv);
        }
        if (has_flag(argc, argv, "-q3shaders")) {
            return run_q3_shaders_command(argc, argv);
        }
        if (has_flag(argc, argv, "-mapmaterials")) {
            return run_map_materials_command(argc, argv);
        }
        if (has_flag(argc, argv, "-mapgen")) {
            return run_mapgen_command(argc, argv);
        }
        if (has_flag(argc, argv, "-mapbundle")) {
            return run_mapbundle_command(argc, argv);
        }

        // A missing command is a normal-program fallback in C#; it is not a
        // request to start a dedicated server. This target currently has no
        // normal game host, so expose the utility usage and stop here until
        // the server build is wired through Program.Main as its own target.
        print_usage(argc > 0 ? argv[0] : "FruityPrimeServer");
        return EXIT_FAILURE;
    } catch (const std::exception& error) {
        g_server = nullptr;
        g_master = nullptr;
        std::cerr << "[server] fatal: " << error.what() << '\n';
        return EXIT_FAILURE;
    }
}
