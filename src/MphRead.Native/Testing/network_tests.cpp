#include "Mods/Network/dedicated_server.hpp"
#include "Assets/game_assets.hpp"
#include "Entities/Players/PlayerEntity.hpp"
#include "Entities/room_catalog.hpp"
#include "Entities/scene.hpp"
#include "Mods/Network/master_client.hpp"
#include "Mods/Network/master_server.hpp"
#include "Mods/Network/match_client.hpp"
#include "Metadata/metadata.hpp"
#include "Mods/Network/net_master.hpp"
#include "Mods/Network/net_client.hpp"
#include "Mods/Network/net_connect_command.hpp"
#include "Mods/Network/net_diagnostics.hpp"
#include "Mods/Network/net_host_session.hpp"
#include "Mods/Network/net_hooks.hpp"
#include "Mods/Network/net_match_end.hpp"
#include "Mods/Network/net_match_sync.hpp"
#include "Mods/Network/net_log.hpp"
#include "Mods/Network/net_launch.hpp"
#include "Mods/Network/net_player_bridge.hpp"
#include "Mods/Network/net_probe.hpp"
#include "Mods/Network/net_room_change.hpp"
#include "Mods/Network/net_status.hpp"
#include "Mods/Network/player_entity_net_hud.hpp"

#include <cassert>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>

namespace {

void require(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

bool room_fade_started = false;

void note_room_fade() noexcept {
    room_fade_started = true;
}

void noop_insert_player(fruityprime::players::PlayerEntity& player) noexcept {
    static_cast<void>(player);
}

void noop_initialize_player(
    fruityprime::players::PlayerEntity& player) noexcept {
    static_cast<void>(player);
}

void noop_init_player(fruityprime::players::PlayerEntity& player) noexcept {
    static_cast<void>(player);
}

void noop_init_halfturret(
    fruityprime::runtime::HalfturretEntity& halfturret) noexcept {
    static_cast<void>(halfturret);
}

template <typename Predicate>
void wait_until(Predicate&& predicate) {
    for (int i = 0; i < 200 && !predicate(); ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    assert(predicate());
}

void test_client_and_status() {
    assert(fruityprime::net::master_protocol_matches(
               fruityprime::net::NetConfig::ProtocolVersion));
    assert(!fruityprime::net::master_protocol_matches(
        static_cast<std::uint8_t>(fruityprime::net::NetConfig::ProtocolVersion
                                   + 1)));
    assert(fruityprime::net::NetMasterConfig::EntriesPerPacket
           == (fruityprime::net::NetConfig::MaxPacketSize - 3)
                  / fruityprime::net::MasterEntryPacket::Size);
    fruityprime::ServerOptions options;
    options.port = 0;
    options.max_players = 2;
    options.advertise = false;
    options.rotation = fruityprime::MapRotation::single_match(
        "TEST ARENA", fruityprime::GameMode::Battle, 0, 7);
    fruityprime::DedicatedServer server(std::move(options));
    std::thread thread([&server]() { server.run(); });
    try {
        wait_until([&server]() { return server.listening(); });
        const auto endpoint = fruityprime::net::NetClient::resolve_ipv4(
            "127.0.0.1", server.bound_port());
        fruityprime::net::NetClient client(endpoint);
        if (!client.connect()) {
            throw std::runtime_error("client connect failed: "
                                     + std::string(client.last_error()));
        }
        assert(client.local_slot() == 0);
        client.identify(0, "Native client");
        const auto status = fruityprime::net::NetClient::query_status(
            "127.0.0.1", server.bound_port());
        assert(status.online);
        assert(status.max_players == 2);
        assert(status.match.room_key == "TEST ARENA");
        const auto browser_status = fruityprime::net::query(
            "127.0.0.1", server.bound_port(), false,
            std::chrono::milliseconds(250));
        assert(browser_status.online);
        assert(browser_status.room_key == "TEST ARENA");
        assert(browser_status.mode == fruityprime::GameMode::Battle);
        assert(browser_status.players == 1);
        assert(browser_status.max_players == 2);
        assert(browser_status.message
               == "FruityPrime · Test Arena · Battle · 1/2 players");
        fruityprime::net::NetClient listener(endpoint);
        if (!listener.connect()) {
            throw std::runtime_error("chat listener connect failed: "
                                     + std::string(listener.last_error()));
        }
        listener.identify(1, "Chat listener");
        std::this_thread::sleep_for(std::chrono::milliseconds(25));
        fruityprime::net::ChatPacket chat;
        chat.slot = 99;
        chat.name = "spoofed name";
        chat.text = "hello from native chat";
        client.send(fruityprime::net::PacketType::Chat, chat.encode());
        bool saw_chat = false;
        for (int i = 0; i < 100 && !saw_chat; ++i) {
            for (const auto& packet : listener.drain()) {
                if (packet.type() != fruityprime::net::PacketType::Chat) {
                    continue;
                }
                const auto received =
                    fruityprime::net::ChatPacket::decode(packet.payload());
                if (received && received->text == chat.text) {
                    assert(received->slot == 0);
                    assert(received->name == "Native client");
                    saw_chat = true;
                }
            }
            if (!saw_chat) {
                std::this_thread::sleep_for(std::chrono::milliseconds(5));
            }
        }
        require(saw_chat, "server did not relay and stamp chat");
        listener.disconnect();
        const auto stats = fruityprime::net::run_match_client(
            endpoint, fruityprime::net::MatchClientOptions{
                "Native scripted client", 1,
                fruityprime::net::IntentButtons::MoveRight,
                fruityprime::net::Vec3{0, 0, 1}, fruityprime::net::Vec3{1, 0, 0},
                10, 20, {}, -1.0, -1.0
            }, std::chrono::milliseconds(200), std::chrono::milliseconds(5));
        if (stats.local_slot < 0 || stats.intents_sent < 5) {
            throw std::runtime_error(
                "native match client did not send its tick stream: slot="
                + std::to_string(stats.local_slot) + " intents="
                + std::to_string(stats.intents_sent));
        }
        client.disconnect();

        fruityprime::features::CheatSettings cheats;
        cheats.free_weapon_select = true;
        cheats.unlimited_jumps = true;
        cheats.quadruple_damage = true;
        fruityprime::net::JoinOptions launch;
        launch.address = "127.0.0.1";
        launch.port = server.bound_port();
        launch.player_name = "Native launch boundary";
        launch.hunter = static_cast<std::uint8_t>(
            fruityprime::metadata::Hunter::Random);
        launch.timeout_ms = 2000;
        launch.cheats = &cheats;
        require(fruityprime::net::NetLaunch::join(launch),
                "native NetLaunch did not join the test server");
        assert(fruityprime::net::NetLaunch::active());
        assert(fruityprime::net::NetLaunch::local_slot() == 0);
        const auto launch_room = fruityprime::net::NetLaunch::server_room();
        require(launch_room.has_value(),
                "native NetLaunch did not retain the server room");
        assert(launch_room->room_key == "TEST ARENA");
        assert(launch_room->mode == fruityprime::GameMode::Battle);
        const auto resolved_hunter = fruityprime::net::NetLaunch::local_hunter();
        assert(resolved_hunter < fruityprime::metadata::PlayableHunterCount);
        const auto plans = fruityprime::net::NetLaunch::build_players(
            static_cast<std::uint8_t>(fruityprime::metadata::Hunter::Random),
            3, 1);
        assert(plans.size() == fruityprime::net::NetConfig::SlotCapacity);
        assert(plans[0].active && plans[0].local);
        assert(plans[0].hunter == resolved_hunter && plans[0].recolor == 3);
        assert(!plans[1].active && !plans[1].is_bot);
        assert(!cheats.free_weapon_select);
        assert(!cheats.unlimited_jumps);
        assert(!cheats.quadruple_damage);
        fruityprime::net::NetLaunch::disconnect();
        const auto probe = fruityprime::net::NetProbe::probe(
            "127.0.0.1", server.bound_port(), 500);
        require(probe.ok, "native NetProbe did not receive Welcome");
        assert(probe.assigned_slot == 0);

        // The launcher listen-host path must use the same server and join
        // boundaries as a remote connection.  Exercise the lifecycle rather
        // than merely checking that the wrapper links.
        fruityprime::net::NetHostSession host;
        fruityprime::net::HostSessionOptions host_options;
        host_options.server.port = 0;
        host_options.server.max_players = 2;
        host_options.server.advertise = false;
        host_options.server.rotation = fruityprime::MapRotation::single_match(
            "HOST TEST", fruityprime::GameMode::Battle, 0, 7);
        host_options.join.player_name = "Embedded host";
        host_options.join.hunter = 3;
        host_options.join.timeout_ms = 2000;
        require(host.start_and_join(host_options),
                "native NetHostSession did not start and join");
        assert(host.running());
        assert(host.bound_port() != 0);
        assert(fruityprime::net::NetLaunch::active());
        const auto hosted_room = fruityprime::net::NetLaunch::server_room();
        require(hosted_room.has_value(),
                "native NetHostSession did not retain hosted room");
        assert(hosted_room->room_key == "HOST TEST");
        host.stop();
        assert(!host.running());
        assert(!fruityprime::net::NetLaunch::active());

        fruityprime::net::ConnectCommandOptions command;
        command.join.address = "127.0.0.1";
        command.join.port = server.bound_port();
        command.join.player_name = "Native command";
        command.join.hunter = static_cast<std::uint8_t>(
            fruityprime::metadata::Hunter::Random);
        command.join.timeout_ms = 2000;
        command.seconds = 0;
        const auto command_result = fruityprime::net::NetConnectCommand::run(
            command);
        require(command_result.ok,
                "native NetConnectCommand did not complete a join");
        assert(command_result.local_slot == 0);
        require(command_result.room.has_value(),
                "native NetConnectCommand did not return room state");
        assert(command_result.room->room_key == "TEST ARENA");
        assert(command_result.players[0].active);
        assert(command_result.players[0].local);
        assert(command_result.players[0].hunter
               < fruityprime::metadata::PlayableHunterCount);
        assert(!fruityprime::net::NetLaunch::active());
    } catch (...) {
        server.stop();
        thread.join();
        throw;
    }
    server.stop();
    thread.join();
}

void test_master_client() {
    fruityprime::MasterOptions options;
    options.port = 0;
    fruityprime::MasterServer master(std::move(options));
    std::thread thread([&master]() { master.run(); });
    try {
        wait_until([&master]() { return master.listening(); });
        const auto endpoint = fruityprime::net::NetClient::resolve_ipv4(
            "127.0.0.1", master.bound_port());
        fruityprime::net::NetTransport reporter(0);
        const fruityprime::net::MasterHeartbeatPacket heartbeat{
            fruityprime::net::NetConfig::ProtocolVersion,
            38101, 1, 4, static_cast<std::uint8_t>(fruityprime::GameMode::Battle),
            "native test server", "TEST ARENA"
        };
        const auto payload = heartbeat.encode();
        reporter.send(endpoint, fruityprime::net::PacketType::MasterHeartbeat,
                      payload);
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        const auto list = fruityprime::net::MasterClient::query(
            "127.0.0.1", master.bound_port());
        assert(list.answered);
        assert(list.servers.size() == 1);
        assert(list.servers[0].port == 38101);
        assert(list.servers[0].server_name == "native test server");
    } catch (...) {
        master.stop();
        thread.join();
        throw;
    }
    master.stop();
    thread.join();
}

void test_match_state_boundaries() {
    fruityprime::game::State local;
    local.match_time = 100.0F;
    fruityprime::net::MatchStatePacket state;
    state.room_key = "TEST ARENA";
    state.match_id = 7;
    state.time_remaining = 99.0F;
    state.time_elapsed = 1.0F;
    state.point_goal = 12;
    state.flags = fruityprime::net::MatchStatePacket::FlagInProgress
        | fruityprime::net::MatchStatePacket::FlagFriendlyFire;

    fruityprime::net::MatchSync sync;
    const auto first = sync.apply(local, state, false);
    assert(first.accepted);
    assert(first.new_match);
    assert(first.clock_snapped);
    assert(local.match_time == 99.0F);
    assert(local.point_goal == 12);
    assert(local.friendly_fire);

    state.time_remaining = 98.5F;
    local.match_time = 99.0F;
    const auto small_drift = sync.apply(local, state, false);
    assert(!small_drift.clock_snapped);
    assert(local.match_time == 99.0F);

    state.time_remaining = 80.0F;
    const auto large_drift = sync.apply(local, state, false);
    assert(large_drift.clock_snapped);
    assert(local.match_time == 80.0F);

    state.flags = fruityprime::net::MatchStatePacket::FlagEnding;
    local.match_time = 5.0F;
    static_cast<void>(sync.apply(local, state, true));
    assert(local.match_time == 5.0F);

    fruityprime::net::MatchEnd end;
    local.match_state = fruityprime::game::MatchState::GameOver;
    state.flags = fruityprime::net::MatchStatePacket::FlagInProgress;
    const auto report = end.sync(1, true, true, local, nullptr);
    assert(report.should_report);
    assert(!end.sync(2, true, true, local, nullptr).should_report);
    assert(end.sync(16, true, true, local, nullptr).should_report);

    fruityprime::net::MatchStatePacket ending = state;
    ending.flags = fruityprime::net::MatchStatePacket::FlagEnding;
    assert(!end.sync(17, true, true, local, &ending).should_report);
    assert(end.acknowledged());

    end.reset();
    local.match_state = fruityprime::game::MatchState::GameOver;
    state.flags = fruityprime::net::MatchStatePacket::FlagInProgress;
    static_cast<void>(end.sync(100, true, false, local, &state));
    const auto recovered = end.sync(
        100 + fruityprime::net::MatchEnd::StrandedFrames,
        true, false, local, &state);
    assert(recovered.recovered);
    assert(local.match_state == fruityprime::game::MatchState::InProgress);
    assert(local.match_time == state.time_remaining);

    fruityprime::net::NetRoomChange::Reset();
    fruityprime::game::State room_state;
    fruityprime::net::NetLog room_log;
    room_state.room_name = "MP1 SANCTORUS";
    state.room_key = "MP3 PROVING GROUND";
    state.match_id = 1;
    room_fade_started = false;
    fruityprime::net::NetRoomChange::Sync({
        true, false, room_state.room_name, state, 1, room_state,
        &note_room_fade, room_log
    });
    assert(room_fade_started);
    room_fade_started = false;
    fruityprime::net::NetRoomChange::Sync({
        true, true, room_state.room_name, state, 2, room_state,
        &note_room_fade, room_log
    });
    assert(!room_fade_started);

    room_state.room_name = "MP3 PROVING GROUND";
    room_state.transition_state = fruityprime::game::TransitionState::None;
    fruityprime::net::NetRoomChange::Sync({
        true, false, room_state.room_name, state, 3, room_state,
        &note_room_fade, room_log
    });
    assert(room_state.transition_room_id != 0);
    state.match_id = 2;
    room_fade_started = false;
    fruityprime::net::NetRoomChange::Sync({
        true, false, room_state.room_name, state, 10, room_state,
        &note_room_fade, room_log
    });
    assert(room_fade_started);
    // C# AfterRebuild is called only after PlayerEntity.Construct and the
    // room's player table has been populated.  This network-only test has no
    // room fixture unless a ROM is supplied, so do not call the production
    // method against an unconstructed table.  The real-room branch exercises
    // the same lifecycle and callbacks when the fixture is available.
    const char* rom_value = std::getenv("FRUITY_PRIME_TEST_NDS");
    if (rom_value == nullptr || rom_value[0] == '\0') {
        std::cout << "network room rebuild test skipped: "
                     "set FRUITY_PRIME_TEST_NDS\n";
        assert(!fruityprime::net::NetRoomChange::Settling(20));
        return;
    }

    const auto assets = fruityprime::assets::Store::from_rom(rom_value);
    fruityprime::runtime::HalfturretEntity::bind_asset_store(&assets);
    const auto* room_entry = fruityprime::scene::find_multiplayer_room(
        "MP1 SANCTORUS");
    assert(room_entry != nullptr);
    const auto room = fruityprime::scene::Room::load(
        assets, room_entry->definition);
    fruityprime::gameplay::Session session(room);
    fruityprime::players::PlayerEntity::Reset();
    fruityprime::players::PlayerEntity::Construct(session);
    fruityprime::players::PlayerEntity::MaxPlayers(2);
    auto* local_player = fruityprime::players::PlayerEntity::Create(
        fruityprime::metadata::Hunter::Samus, 0);
    auto* remote = fruityprime::players::PlayerEntity::Create(
        fruityprime::metadata::Hunter::Weavel, 0);
    assert(local_player != nullptr && remote != nullptr);
    const auto active_flags = static_cast<fruityprime::formats::LoadFlags>(
        static_cast<std::uint8_t>(fruityprime::formats::LoadFlags::SlotActive)
        | static_cast<std::uint8_t>(fruityprime::formats::LoadFlags::Active)
        | static_cast<std::uint8_t>(fruityprime::formats::LoadFlags::Initial));
    local_player->LoadFlags(active_flags);
    remote->LoadFlags(active_flags);
    fruityprime::players::PlayerEntity::PlayerCount(1);
    fruityprime::players::PlayerEntity::MainPlayerIndex(0);
    fruityprime::net::NetPlayerBridge room_bridge;
    fruityprime::net::NetRoomChange::AfterRebuild({
        20, room_bridge, room_log,
        &noop_insert_player, &noop_initialize_player,
        &noop_init_player, &noop_init_halfturret
    });
    assert(fruityprime::net::NetRoomChange::Settling(20));
    assert(!fruityprime::net::NetRoomChange::Settling(80));
    fruityprime::players::PlayerEntity::Reset();
    fruityprime::runtime::HalfturretEntity::bind_asset_store(nullptr);
}

void test_net_log() {
    const auto directory = std::filesystem::temp_directory_path()
        / ("fruityprime-netlog-test-"
           + std::to_string(std::chrono::steady_clock::now()
                               .time_since_epoch().count()));
    std::filesystem::create_directories(directory);
    try {
        fruityprime::net::NetLog log;
        require(log.open(directory, "Native Test"),
                "native net log did not open");
        fruityprime::game::State state;
        state.begin_room("TEST ARENA", 93, true,
                         fruityprime::game::Mode::Battle);
        state.nicknames[0] = "Tester";
        fruityprime::net::NetLog::SnapshotContext context;
        context.game_state = &state;
        context.role = "client";
        context.local_slot = 0;
        context.main_player = 0;
        context.occupied[0] = true;
        context.pings[0] = 41;
        log.snapshot(0.5, context);
        log.snapshot(1.0, context);
        log.collision_range(0, "pre-check", {0, 0, 0}, {1, 2, 3});
        log.event("test event");
        log.close();

        const auto file = directory / "netlog-Native_Test.txt";
        std::ifstream input(file);
        const std::string contents((std::istreambuf_iterator<char>(input)),
                                   std::istreambuf_iterator<char>());
        require(contents.find("MphRead net log") != std::string::npos,
                "native net log header missing");
        require(contents.find("STATE  role=client") != std::string::npos,
                "native net log snapshot missing");
        require(contents.find("collision pre-check slot=0")
                    != std::string::npos,
                "native net log collision event missing");
        require(contents.find("EVENT  test event") != std::string::npos,
                "native net log event missing");
    } catch (...) {
        std::error_code ignored;
        std::filesystem::remove_all(directory, ignored);
        throw;
    }
    std::error_code ignored;
    std::filesystem::remove_all(directory, ignored);
}

void test_net_diagnostics() {
    fruityprime::game::State state;
    state.active_players = 2;
    fruityprime::net::MatchStatePacket server_match;
    server_match.room_key = "TEST ARENA";
    server_match.player_count = 2;

    fruityprime::net::NetDiagnostics diagnostics;
    diagnostics.set_enabled(true);
    fruityprime::net::NetDiagnostics::Context context;
    context.game_state = &state;
    context.server_match = &server_match;
    context.role = "client";
    context.active = true;
    context.local_slot = 1;
    context.occupied[0] = true;
    context.occupied[1] = true;
    context.state_valid[0] = true;
    context.intent_valid[1] = true;

    std::ostringstream captured;
    std::streambuf* previous = std::cout.rdbuf(captured.rdbuf());
    try {
        diagnostics.report(1.0, context);
        diagnostics.report(1.5, context);
        diagnostics.report(2.0, context);
    } catch (...) {
        std::cout.rdbuf(previous);
        throw;
    }
    std::cout.rdbuf(previous);

    const std::string output = captured.str();
    require(output.find("[netdbg] role=client slot=1")
                != std::string::npos,
            "native net diagnostics header missing");
    require(output.find("remoteState=[ynn") != std::string::npos,
            "native net diagnostics state map missing");
    require(output.find("remoteIntent=[nyn") != std::string::npos,
            "native net diagnostics intent map missing");
    require(output.find("serverMap=TEST ARENA serverPlayers=2")
                != std::string::npos,
            "native net diagnostics server state missing");
    // The once-per-second throttle should suppress the middle sample.
    std::size_t lines = 0;
    for (char value : output) {
        lines += value == '\n' ? 1U : 0U;
    }
    require(lines == 2, "native net diagnostics throttle failed");
}

void test_net_hud() {
    const auto offline = fruityprime::net::hud::score_columns(false);
    const auto online = fruityprime::net::hud::score_columns(true);
    assert(offline.first == 160.0F && offline.second == 215.0F);
    assert(online.first == 145.0F && online.second == 193.0F
           && online.ping == 236.0F);
    assert(fruityprime::net::hud::ping_label(0).text == "--");
    assert(fruityprime::net::hud::ping_label(40).tone
           == fruityprime::net::hud::PingTone::Good);
    assert(fruityprime::net::hud::ping_label(80).tone
           == fruityprime::net::hud::PingTone::Warning);
    assert(fruityprime::net::hud::ping_label(160).tone
           == fruityprime::net::hud::PingTone::Bad);
    assert(fruityprime::net::hud::ping_label(5000).text == "999");

    fruityprime::game::State state;
    state.player_time[0] = 61.0F;
    state.points[0] = 7;
    state.deaths[0] = 2;
    state.kills[0] = 9;
    assert(fruityprime::net::hud::score_header_one(
               fruityprime::game::Mode::Battle) == "points");
    assert(fruityprime::net::hud::score_header_two(
               fruityprime::game::Mode::Battle) == "deaths");
    assert(fruityprime::net::hud::score_value_one(
               state, fruityprime::game::Mode::Battle, 0) == "7");
    assert(fruityprime::net::hud::score_value_two(
               state, fruityprime::game::Mode::Battle, 0) == "2");
    assert(fruityprime::net::hud::score_value_one(
               state, fruityprime::game::Mode::Survival, 0) == "1:01");
    state.player_time[0] = -1.0F;
    assert(fruityprime::net::hud::score_value_one(
               state, fruityprime::game::Mode::Survival, 0) == "MAX");
}

void test_net_hooks() {
    assert(fruityprime::net::NetHooks::local_slot(false, -1, false) == 0);
    assert(fruityprime::net::NetHooks::local_slot(true, -1, false) == 0);
    assert(fruityprime::net::NetHooks::local_slot(true, 3, false) == 3);
    assert(fruityprime::net::NetHooks::local_slot(true, 3, true) == -1);

    fruityprime::net::NetHookContext authority{
        true, true, false, false, 0};
    assert(fruityprime::net::NetHooks::keep_slot_alive(authority));
    fruityprime::net::NetHookContext offline{};
    assert(!fruityprime::net::NetHooks::keep_slot_alive(offline));

    std::array<bool, fruityprime::net::NetConfig::SlotCapacity> occupied{};
    occupied[3] = true;
    assert(fruityprime::net::NetHooks::force_spawn(
        false, authority, 0, occupied));
    assert(fruityprime::net::NetHooks::force_spawn(
        false, authority, 3, occupied));
    assert(!fruityprime::net::NetHooks::force_spawn(
        false, authority, 2, occupied));
    assert(!fruityprime::net::NetHooks::force_spawn(
        false, offline, 3, occupied));
    assert(fruityprime::net::NetHooks::force_spawn(
        true, offline, 3, occupied));

    const fruityprime::net::IntentState intent{
        1, fruityprime::net::IntentButtons::None,
        fruityprime::net::Vec3{0, 0, 2}, 0xff, {},
        fruityprime::net::Vec3{10, 2, 5}, 0, 0};
    const auto origin = fruityprime::net::NetHooks::remote_shot_origin(
        {0, 0, 1}, {1, 2, 1}, intent, authority, 3);
    assert(origin.x == 9.0F && origin.y == 0.0F && origin.z == 5.0F);
    const auto direction = fruityprime::net::NetHooks::remote_shot_direction(
        {1, 0, 0}, intent, authority, 3);
    assert(direction.x == 0.0F && direction.y == 0.0F
           && direction.z == 1.0F);
    assert(fruityprime::net::NetHooks::remote_shot_direction(
               {1, 0, 0}, intent, offline, 3).x == 1.0F);
}

} // namespace

int main() {
    try {
        test_match_state_boundaries();
        test_net_log();
        test_net_diagnostics();
        test_net_hud();
        test_net_hooks();
        test_client_and_status();
        test_master_client();
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "network test failed: " << error.what() << '\n';
        return 1;
    }
}
