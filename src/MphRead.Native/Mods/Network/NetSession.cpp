#include "Mods/Network/net_session.hpp"

#include "Metadata/metadata.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <limits>
#include <stdexcept>

namespace fruityprime::net {
namespace {

constexpr std::uint32_t StreamResetGap = NetConfig::IntentResetGap;
constexpr int LateSnapshotsBeforeReset = 12;

bool valid_slot(std::size_t slot) noexcept {
    return slot < NetConfig::SlotCapacity;
}

} // namespace

NetSession::~NetSession() {
    stop();
}

bool NetSession::start_client(Endpoint server, NetworkConditions conditions,
                              std::chrono::milliseconds timeout) {
    stop();
    client_ = std::make_unique<NetClient>(server, conditions);
    if (!client_->connect(0xff, timeout)) {
        last_error_ = std::string(client_->last_error());
        client_.reset();
        role_ = NetRole::Offline;
        return false;
    }
    role_ = NetRole::Client;
    reset_state();
    return true;
}

void NetSession::stop() noexcept {
    if (client_ != nullptr) {
        client_->disconnect();
    }
    client_.reset();
    role_ = NetRole::Offline;
    reset_state();
}

void NetSession::reset_state() noexcept {
    net_frame_ = 0;
    last_snapshot_frame_ = 0;
    last_slot_intent_frame_.fill(0);
    remote_intent_arrived_.fill(0);
    remote_states_.fill({});
    remote_state_valid_.fill(false);
    remote_intents_.fill({});
    remote_intent_valid_.fill(false);
    slot_occupied_.fill(false);
    slot_ping_.fill(0);
    roster_ = {};
    server_match_ = {};
    roster_valid_ = false;
    server_match_valid_ = false;
    authority_ = false;
    refused_ = false;
    refused_reason_ = {};
    last_server_packet_time_ = 0.0;
    longest_server_silence_ = 0.0;
    reannouncements_ = 0;
    late_snapshot_run_ = 0;
    snapshot_stream_resets_ = 0;
    snapshots_received_ = 0;
    snapshots_sent_ = 0;
    states_applied_ = 0;
    intents_received_ = 0;
    snapshots_out_of_order_ = 0;
    intents_out_of_order_ = 0;
}

void NetSession::set_player(std::uint8_t hunter, std::string_view name) {
    local_hunter_ = hunter;
    player_name_.assign(name);
}

void NetSession::identify() {
    if (client_ != nullptr && client_->connected()) {
        client_->identify(local_hunter_, player_name_);
    }
}

std::vector<ReceivedPacket> NetSession::update(double time_seconds) {
    if (client_ == nullptr || !client_->connected()) {
        return {};
    }
    ++net_frame_;
    std::vector<ReceivedPacket> packets = client_->drain();
    for (const ReceivedPacket& packet : packets) {
        retain_packet(packet, time_seconds);
    }

    if (last_server_packet_time_ > 0.0 && time_seconds > last_server_packet_time_) {
        const double silence = time_seconds - last_server_packet_time_;
        longest_server_silence_ = std::max(longest_server_silence_, silence);
        if (silence > 5.0 && net_frame_ % 60 == 0) {
            reannounce();
        }
    }
    return packets;
}

void NetSession::retain_packet(const ReceivedPacket& packet,
                               double time_seconds) {
    if (role_ != NetRole::Client) {
        return;
    }
    if (last_server_packet_time_ > 0.0 && time_seconds > last_server_packet_time_) {
        longest_server_silence_ = std::max(
            longest_server_silence_, time_seconds - last_server_packet_time_);
    }
    last_server_packet_time_ = time_seconds;

    switch (packet.type()) {
    case PacketType::Welcome:
        // NetClient completes the handshake and owns the authoritative local
        // slot.  A later Welcome is a re-admission, so it is still harmless
        // to retain the slot through the client object.
        break;
    case PacketType::Roster:
        if (const auto roster = RosterPacket::decode(packet.payload())) {
            retain_roster(*roster);
        }
        break;
    case PacketType::MatchState:
    case PacketType::MapChange:
        if (const auto match = MatchStatePacket::decode(packet.payload())) {
            retain_match_state(*match);
        }
        break;
    case PacketType::SlotIntent:
        retain_slot_intent(packet.payload());
        break;
    case PacketType::Snapshot:
        if (const auto snapshot = SnapshotPacket::decode(packet.payload())) {
            retain_snapshot(*snapshot);
        }
        break;
    case PacketType::Authority:
        authority_ = packet.payload().empty() || packet.payload()[0] != 0;
        break;
    case PacketType::Refused:
        if (const auto refused = RefusedPacket::decode(packet.payload())) {
            refused_reason_ = *refused;
            refused_ = true;
        }
        break;
    case PacketType::Ping:
        client_->send(PacketType::Pong, packet.payload());
        break;
    case PacketType::Bye:
        last_error_ = "server closed the session";
        break;
    default:
        break;
    }
}

void NetSession::retain_roster(const RosterPacket& roster) noexcept {
    roster_ = roster;
    roster_valid_ = true;
    slot_occupied_.fill(false);
    slot_ping_.fill(0);
    const std::size_t count = std::min<std::size_t>(
        roster.count, NetConfig::SlotCapacity);
    for (std::size_t index = 0; index < count; ++index) {
        const std::size_t slot = roster.slots[index];
        if (!valid_slot(slot)) {
            continue;
        }
        slot_occupied_[slot] = true;
        if (roster.hunters[index]
            <= static_cast<std::uint8_t>(metadata::Hunter::Random)) {
            slot_hunters_[slot] = roster.hunters[index];
        }
        slot_ping_[slot] = roster.pings[index];
    }
}

void NetSession::retain_match_state(const MatchStatePacket& match) noexcept {
    server_match_ = match;
    server_match_valid_ = !match.room_key.empty();
}

void NetSession::retain_snapshot(const SnapshotPacket& snapshot) {
    const std::uint32_t frame = snapshot.header.frame;
    if (last_snapshot_frame_ != 0 && frame <= last_snapshot_frame_
        && last_snapshot_frame_ - frame < StreamResetGap) {
        ++snapshots_out_of_order_;
        if (++late_snapshot_run_ < LateSnapshotsBeforeReset) {
            return;
        }
        ++snapshot_stream_resets_;
    }
    late_snapshot_run_ = 0;
    last_snapshot_frame_ = frame;
    ++snapshots_received_;
    remote_state_valid_.fill(false);
    for (const PlayerState& player : snapshot.players) {
        if (!valid_slot(player.slot_index)) {
            continue;
        }
        remote_states_[player.slot_index] = player;
        remote_state_valid_[player.slot_index] = true;
    }
}

void NetSession::retain_slot_intent(std::span<const std::uint8_t> payload) {
    if (payload.size() < 1 + IntentState::Size) {
        return;
    }
    const std::size_t slot = payload[0];
    if (!valid_slot(slot) || static_cast<int>(slot) == local_slot()) {
        return;
    }
    const auto intent = IntentState::decode(payload.subspan(1));
    if (!intent) {
        return;
    }
    if (last_slot_intent_frame_[slot] != 0
        && intent->frame <= last_slot_intent_frame_[slot]
        && last_slot_intent_frame_[slot] - intent->frame < StreamResetGap) {
        ++intents_out_of_order_;
        return;
    }
    last_slot_intent_frame_[slot] = intent->frame;
    remote_intents_[slot] = *intent;
    remote_intent_valid_[slot] = true;
    remote_intent_arrived_[slot] = std::max(net_frame_, 1U);
    ++intents_received_;
}

void NetSession::reannounce() {
    if (client_ == nullptr || !client_->connected()) {
        return;
    }
    const std::array<std::uint8_t, 2> hello{
        NetConfig::ProtocolVersion,
        local_slot() >= 0 && local_slot() < 0xff
            ? static_cast<std::uint8_t>(local_slot())
            : static_cast<std::uint8_t>(0xff)
    };
    client_->send(PacketType::Hello, hello);
    identify();
    ++reannouncements_;
    last_server_packet_time_ = 0.0;
}

void NetSession::send_intent(IntentState intent) {
    if (client_ == nullptr || !client_->connected()) {
        return;
    }
    if (intent.frame == 0) {
        intent.frame = net_frame_;
    } else {
        net_frame_ = std::max(net_frame_, intent.frame);
    }
    client_->send(PacketType::Intent, intent.encode());
}

void NetSession::send_match_end() {
    if (client_ != nullptr && client_->connected()) {
        client_->send(PacketType::MatchEnd);
    }
}

void NetSession::send_chat(std::string_view text) {
    if (client_ == nullptr || !client_->connected() || text.empty()) {
        return;
    }
    ChatPacket packet;
    packet.slot = local_slot() >= 0 ? static_cast<std::uint8_t>(local_slot()) : 0xff;
    packet.kind = ChatPacket::KindSay;
    packet.name = player_name_;
    packet.text = std::string(text);
    client_->send(PacketType::Chat, packet.encode());
}

void NetSession::send(PacketType type, std::span<const std::uint8_t> payload) {
    if (client_ != nullptr && client_->connected()) {
        client_->send(type, payload);
    }
}

std::optional<MatchStatePacket> NetSession::server_match() const {
    return server_match_valid_ ? std::optional<MatchStatePacket>(server_match_)
                               : std::nullopt;
}

std::uint32_t NetSession::remote_intent_age(int slot) const noexcept {
    if (slot < 0 || slot >= static_cast<int>(remote_intent_arrived_.size())
        || remote_intent_arrived_[static_cast<std::size_t>(slot)] == 0) {
        return std::numeric_limits<std::uint32_t>::max();
    }
    const std::uint32_t arrived = remote_intent_arrived_[static_cast<std::size_t>(slot)];
    return net_frame_ >= arrived ? net_frame_ - arrived : 0;
}

void NetSession::forget_slot(int slot) noexcept {
    if (slot < 0 || slot >= static_cast<int>(NetConfig::SlotCapacity)) {
        return;
    }
    const std::size_t index = static_cast<std::size_t>(slot);
    last_slot_intent_frame_[index] = 0;
    remote_intents_[index] = {};
    remote_intent_valid_[index] = false;
    remote_intent_arrived_[index] = 0;
    remote_states_[index] = {};
    remote_state_valid_[index] = false;
    slot_occupied_[index] = false;
    slot_ping_[index] = 0;
}

} // namespace fruityprime::net
