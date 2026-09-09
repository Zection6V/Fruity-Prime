#include "Mods/Network/map_rotation.hpp"
#include "Mods/Network/net_protocol.hpp"

#include <cassert>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>

using namespace fruityprime;
using namespace fruityprime::net;

int main() {
    static_assert(RefusedPacket::Size == 3);
    static_assert(HostRequestPacket::Size == 79);
    static_assert(HostReplyPacket::Size == 99);
    static_assert(MatchStatePacket::Size == 95);
    static_assert(ServerStatusPacket::Size == 129);
    static_assert(MasterEntryPacket::Size == 82);
    static_assert(MasterHeartbeatPacket::Size == 78);
    static_assert(RosterPacket::Size == 161);
    static_assert(ChatPacket::Size == 114);
    static_assert(IntentPacket::Size == 69);
    static_assert(IntentState::Size == 69);
    static_assert(PlayerState::Size == 64);
    static_assert(SnapshotHeader::Size == 13);

    const auto refused = RefusedPacket{RefusedPacket::ReasonFull, 4, 8}.encode();
    assert(refused[0] == RefusedPacket::ReasonFull);
    assert(refused[1] == 4 && refused[2] == 8);
    assert(RefusedPacket::decode(refused)->players == 4);
    assert(!RefusedPacket::decode(std::span<const std::uint8_t>(refused).first(2)));

    HostRequestPacket host_request;
    host_request.protocol = NetConfig::ProtocolVersion;
    host_request.max_players = 8;
    host_request.mode = static_cast<std::uint8_t>(GameMode::Battle);
    host_request.time_limit = 420;
    host_request.point_goal = 7;
    host_request.room_key = "MP3 PROVING GROUND";
    host_request.server_name = "Native server";
    const auto host_request_bytes = host_request.encode();
    const auto decoded_host_request = HostRequestPacket::decode(host_request_bytes);
    assert(decoded_host_request && decoded_host_request->max_players == 8);
    assert(decoded_host_request->room_key == host_request.room_key);

    const HostReplyPacket host_reply{true, 27900, ""};
    const auto host_reply_bytes = host_reply.encode();
    assert(HostReplyPacket::decode(host_reply_bytes)->port == 27900);

    MatchStatePacket state;
    state.mode = static_cast<std::uint8_t>(GameMode::BattleTeams);
    state.time_remaining = 123.5f;
    state.time_elapsed = 456.25f;
    state.player_count = 6;
    state.flags = MatchStatePacket::FlagEnding | MatchStatePacket::FlagFriendlyFire;
    state.point_goal = 17;
    state.match_id = 33;
    state.room_key = "MP3 PROVING GROUND";
    state.next_room_key = "MP4 HIGHGROUND";
    const auto state_bytes = state.encode();
    assert(state_bytes[0] == 4);
    assert(state_bytes[11] == 17 && state_bytes[13] == 33);
    const auto decoded_state = MatchStatePacket::decode(state_bytes);
    assert(decoded_state);
    assert(decoded_state->mode == state.mode);
    assert(std::fabs(decoded_state->time_remaining - state.time_remaining) < 0.001f);
    assert(decoded_state->room_key == state.room_key);
    assert(decoded_state->ending() && decoded_state->friendly_fire());

    RosterPacket roster;
    roster.count = 2;
    roster.slots[0] = 0;
    roster.slots[1] = 7;
    roster.hunters[0] = 1;
    roster.hunters[1] = 6;
    roster.pings[0] = 12;
    roster.pings[1] = 345;
    roster.names[0] = "Samus";
    roster.names[1] = "n\x01x";
    const auto roster_bytes = roster.encode();
    assert(roster_bytes[0] == 2);
    assert(RosterPacket::decode(roster_bytes)->names[1] == "n?x");

    ChatPacket chat;
    chat.slot = 3;
    chat.kind = ChatPacket::KindSay;
    chat.name = "Samus";
    chat.text = "hello\x01";
    const auto chat_bytes = chat.encode();
    const auto decoded_chat = ChatPacket::decode(chat_bytes);
    assert(decoded_chat && decoded_chat->text == "hello?");

    const std::array<std::uint8_t, IntentPacket::Size> intent_bytes{
        0x78, 0x56, 0x34, 0x12
    };
    assert(IntentPacket::frame(intent_bytes)
        && *IntentPacket::frame(intent_bytes) == 0x12345678u);

    IntentState intent;
    intent.frame = 1234;
    intent.buttons = IntentButtons::Shoot;
    intent.aim = Vec3{1.0f, -2.0f, 3.0f};
    intent.weapon_select = 5;
    intent.presses[0] = 0x11223344;
    intent.position = Vec3{-4.0f, 5.0f, -6.0f};
    intent.ammo_ua = 77;
    intent.ammo_missiles = 88;
    const auto intent_state_bytes = intent.encode();
    const auto decoded_intent = IntentState::decode(intent_state_bytes);
    assert(decoded_intent && decoded_intent->frame == 1234
        && decoded_intent->buttons == IntentButtons::Shoot
        && decoded_intent->presses[0] == 0x11223344
        && decoded_intent->ammo_missiles == 88);

    PlayerState player;
    player.slot_index = 2;
    player.flags = PlayerState::FlagActive | PlayerState::FlagSpawned;
    player.position = Vec3{7.0f, 8.0f, 9.0f};
    player.points = -12;
    player.kills = 4;
    player.deaths = 5;
    const auto player_bytes = player.encode();
    const auto decoded_player = PlayerState::decode(player_bytes);
    assert(decoded_player && decoded_player->slot_index == 2
        && decoded_player->position.z == 9.0f
        && decoded_player->points == -12 && decoded_player->deaths == 5);

    SnapshotPacket snapshot;
    snapshot.header.frame = 900;
    snapshot.header.rng1 = 0xabcdef01;
    snapshot.header.rng2 = 0x10203040;
    snapshot.players.push_back(player);
    const auto snapshot_bytes = snapshot.encode();
    const auto decoded_snapshot = SnapshotPacket::decode(snapshot_bytes);
    assert(decoded_snapshot && decoded_snapshot->header.frame == 900
        && decoded_snapshot->header.player_count == 1
        && decoded_snapshot->players[0].slot_index == 2);

    const auto datagram = make_datagram(PacketType::Welcome, refused);
    assert(datagram.size() == RefusedPacket::Size + 1);
    assert(datagram[0] == static_cast<std::uint8_t>(PacketType::Welcome));

    const auto temporary = std::filesystem::temp_directory_path()
        / "fruity_prime_native_rotation_test.txt";
    {
        std::ofstream output(temporary, std::ios::trunc);
        output << "# comment\n"
               << "MP1 SANCTORUS | BattleTeams | 1.5 | 12\n"
               << "MP3 PROVING GROUND | Nodes Teams | bad | bad\n";
    }
    const MapRotation rotation = MapRotation::load(temporary);
    assert(rotation.entries().size() == 2);
    assert(rotation.current().mode == GameMode::BattleTeams);
    assert(std::fabs(rotation.current().time_limit - 90.0f) < 0.001f);
    assert(rotation.current().point_goal == 12);
    std::filesystem::remove(temporary);

    return 0;
}
