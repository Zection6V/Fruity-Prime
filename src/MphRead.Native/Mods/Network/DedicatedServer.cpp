#include "Mods/Network/dedicated_server.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <chrono>
#include <cmath>
#include <cstring>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <utility>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

namespace fruityprime {
namespace {

constexpr double ReportSeconds = 30.0;

#ifdef _WIN32
using RawSocket = SOCKET;
constexpr RawSocket InvalidSocket = INVALID_SOCKET;
#else
using RawSocket = int;
constexpr RawSocket InvalidSocket = -1;
#endif

void close_raw_socket(RawSocket socket) noexcept {
    if (socket == InvalidSocket) {
        return;
    }
#ifdef _WIN32
    closesocket(socket);
#else
    close(socket);
#endif
}

[[nodiscard]] std::string trim_ascii(std::string value) {
    while (!value.empty()
        && std::isspace(static_cast<unsigned char>(value.front())) != 0) {
        value.erase(value.begin());
    }
    while (!value.empty()
        && std::isspace(static_cast<unsigned char>(value.back())) != 0) {
        value.pop_back();
    }
    return value;
}

[[nodiscard]] std::string now_string() {
    const std::time_t now = std::time(nullptr);
    std::tm local{};
#ifdef _WIN32
    localtime_s(&local, &now);
#else
    localtime_r(&now, &local);
#endif
    std::ostringstream output;
    output << std::put_time(&local, "%H:%M:%S");
    return output.str();
}

} // namespace

MasterReporter::MasterReporter(std::string host, std::uint16_t port)
    : host_(std::move(host)), port_(port) {}

MasterReporter::~MasterReporter() noexcept {
    close_raw_socket(socket_);
    socket_ = InvalidSocket;
}

bool MasterReporter::resolve() {
    addrinfo hints{};
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;
    addrinfo* result = nullptr;
    const std::string service = std::to_string(port_);
    const int error = getaddrinfo(host_.c_str(), service.c_str(), &hints, &result);
    if (error != 0 || result == nullptr) {
#ifdef _WIN32
        complain("name lookup failed (WSA error "
                 + std::to_string(WSAGetLastError()) + ")");
#else
        complain("name lookup failed");
#endif
        if (result != nullptr) {
            freeaddrinfo(result);
        }
        return false;
    }
    endpoint_ = *reinterpret_cast<const sockaddr_in*>(result->ai_addr);
    freeaddrinfo(result);
    if (socket_ == InvalidSocket) {
        socket_ = ::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        if (socket_ == InvalidSocket) {
            complain("could not create a heartbeat socket");
            have_endpoint_ = false;
            return false;
        }
    }
    have_endpoint_ = true;
    complained_ = false;
    return true;
}

void MasterReporter::beat(double now, std::string_view server_name,
                          std::uint16_t game_port, std::uint8_t players,
                          std::uint8_t max_players, std::uint8_t mode,
                          std::string_view room_key) {
    if (now - last_beat_ < net::NetMasterConfig::HeartbeatSeconds) {
        return;
    }
    last_beat_ = now;
    if (!have_endpoint_ || now - last_resolve_ >= 3600.0) {
        if (!resolve()) {
            return;
        }
        last_resolve_ = now;
    }

    net::MasterHeartbeatPacket heartbeat;
    heartbeat.protocol = net::NetConfig::ProtocolVersion;
    heartbeat.port = game_port;
    heartbeat.players = players;
    heartbeat.max_players = max_players;
    heartbeat.mode = mode;
    heartbeat.server_name = std::string(server_name);
    heartbeat.room_key = std::string(room_key);
    const auto payload = heartbeat.encode();
    const auto datagram = net::make_datagram(net::PacketType::MasterHeartbeat,
                                              payload);
#ifdef _WIN32
    const int sent = sendto(socket_, reinterpret_cast<const char*>(datagram.data()),
                            static_cast<int>(datagram.size()), 0,
                            reinterpret_cast<const sockaddr*>(&endpoint_),
                            static_cast<int>(sizeof(endpoint_)));
#else
    const ssize_t sent = sendto(socket_, datagram.data(), datagram.size(), 0,
                                reinterpret_cast<const sockaddr*>(&endpoint_),
                                sizeof(endpoint_));
#endif
    if (sent < 0) {
        complain("could not send heartbeat");
    }
}

void MasterReporter::farewell(std::uint16_t game_port) noexcept {
    if (!have_endpoint_ || socket_ == InvalidSocket) {
        return;
    }
    const std::array<std::uint8_t, 2> payload{
        static_cast<std::uint8_t>(game_port & 0xffu),
        static_cast<std::uint8_t>((game_port >> 8) & 0xffu)
    };
    const auto datagram = net::make_datagram(net::PacketType::Bye, payload);
#ifdef _WIN32
    static_cast<void>(sendto(socket_, reinterpret_cast<const char*>(datagram.data()),
                             static_cast<int>(datagram.size()), 0,
                             reinterpret_cast<const sockaddr*>(&endpoint_),
                             static_cast<int>(sizeof(endpoint_))));
#else
    static_cast<void>(sendto(socket_, datagram.data(), datagram.size(), 0,
                             reinterpret_cast<const sockaddr*>(&endpoint_),
                             sizeof(endpoint_)));
#endif
}

void MasterReporter::complain(std::string_view message) {
    if (complained_) {
        return;
    }
    complained_ = true;
    std::cout << "[master] not listed on " << host_ << ':' << port_ << " -- "
              << message << '\n'
              << "[master] the server is running normally; pass -nomaster to "
                 "stop trying, or -master HOST to point elsewhere\n";
}

DedicatedServer::DedicatedServer(ServerOptions options)
    : options_(std::move(options)) {
    options_.max_players = std::clamp(options_.max_players, 2,
                                      static_cast<int>(net::NetConfig::SlotCapacity));
    if (options_.server_name.empty()) {
        options_.server_name = "FruityPrime";
    }
}

std::uint16_t DedicatedServer::bound_port() const noexcept {
    const std::uint16_t bound = bound_port_.load();
    return bound == 0 ? options_.port : bound;
}

void DedicatedServer::run(std::chrono::milliseconds duration) {
    if (running_.exchange(true)) {
        throw std::logic_error("dedicated server is already running");
    }
    try {
        transport_ = std::make_unique<net::NetTransport>(options_.port);
        bound_port_.store(transport_->local_port());
        listening_.store(true);
        if (options_.advertise) {
            reporter_ = std::make_unique<MasterReporter>(options_.master_host,
                                                          options_.master_port);
        }

        log("listening on UDP " + std::to_string(transport_->local_port())
            + ", up to " + std::to_string(options_.max_players) + " players");
        log("relay mode: the first client to connect is the simulation authority");
        log("rotation: " + std::to_string(options_.rotation.entries().size())
            + " map(s), starting on " + options_.rotation.current().room_key);

        const auto started_at = std::chrono::steady_clock::now();
        double last_report = 0;
        double last_state_broadcast = 0;
        match_started_ = 0;
        match_ended_at_ = -1;
        while (running_.load()) {
            const auto elapsed = std::chrono::duration<double>(
                std::chrono::steady_clock::now() - started_at).count();
            if (duration.count() > 0
                && elapsed >= std::chrono::duration<double>(duration).count()) {
                running_.store(false);
                break;
            }

            for (const net::ReceivedPacket& packet : transport_->drain()) {
                handle(packet, elapsed);
            }
            drop_timed_out(elapsed);
            peer_count_.store(peers_.size());

            const float time_limit = options_.rotation.current().time_limit;
            if (match_ended_at_ < 0 && time_limit > 0 && !peers_.empty()
                && elapsed - match_started_ >= time_limit) {
                end_match(elapsed, "time limit");
            } else if (match_ended_at_ >= 0
                       && elapsed - match_ended_at_ >= EndSequenceSeconds) {
                advance_map(elapsed);
            }

            if (elapsed - last_state_broadcast >= 1.0) {
                last_state_broadcast = elapsed;
                ping_peers(elapsed);
                broadcast_match_state(elapsed);
                broadcast_roster();
                if (authority_slot_ >= 0) {
                    const int authority = find_peer_by_slot(authority_slot_);
                    if (authority >= 0) {
                        notify_authority(peers_[authority]);
                    }
                }
                if (reporter_ != nullptr) {
                    reporter_->beat(elapsed, options_.server_name, bound_port(),
                                    static_cast<std::uint8_t>(peers_.size()),
                                    static_cast<std::uint8_t>(options_.max_players),
                                    static_cast<std::uint8_t>(options_.rotation.current().mode),
                                    options_.rotation.current().room_key);
                }
            }
            if (elapsed - last_report >= ReportSeconds) {
                last_report = elapsed;
                std::ostringstream message;
                message << peers_.size() << " peer(s) connected";
                if (authority_slot_ >= 0) {
                    message << ", authority = slot " << authority_slot_;
                } else {
                    message << ", no authority";
                }
                message << ", map " << options_.rotation.current().room_key;
                if (time_limit > 0) {
                    message << ", " << std::max(0.0, time_limit
                        - (elapsed - match_started_)) << " s left";
                }
                if (transport_->packets_dropped() > 0) {
                    message << ", " << transport_->packets_dropped()
                            << " packet(s) dropped";
                }
                log(message.str());
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(
                peers_.empty() ? 20 : 1));
        }
    } catch (...) {
        shutdown();
        throw;
    }
    shutdown();
}

void DedicatedServer::shutdown() {
    running_.store(false);
    listening_.store(false);
    peer_count_.store(0);
    if (reporter_ != nullptr) {
        reporter_->farewell(bound_port());
        reporter_.reset();
    }
    transport_.reset();
}

int DedicatedServer::find_peer(const net::Endpoint& endpoint) const noexcept {
    for (std::size_t i = 0; i < peers_.size(); ++i) {
        if (peers_[i].endpoint == endpoint) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

int DedicatedServer::find_peer_by_slot(int slot) const noexcept {
    for (std::size_t i = 0; i < peers_.size(); ++i) {
        if (peers_[i].slot == slot) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

bool DedicatedServer::slot_free(int slot) const noexcept {
    return find_peer_by_slot(slot) < 0;
}

int DedicatedServer::next_free_slot() const noexcept {
    for (int slot = 0; slot < options_.max_players; ++slot) {
        if (slot_free(slot)) {
            return slot;
        }
    }
    return -1;
}

net::MatchStatePacket DedicatedServer::build_state(double now) const {
    const RotationEntry& entry = options_.rotation.current();
    const float elapsed = static_cast<float>(now - match_started_);
    const bool ending = match_ended_at_ >= 0;
    net::MatchStatePacket state;
    state.mode = static_cast<std::uint8_t>(entry.mode);
    state.time_remaining = ending || entry.time_limit <= 0
        ? 0.0f
        : std::max(0.0f, entry.time_limit - elapsed);
    state.time_elapsed = elapsed;
    state.player_count = static_cast<std::uint8_t>(peers_.size());
    state.flags = static_cast<std::uint8_t>(ending
        ? net::MatchStatePacket::FlagEnding
        : net::MatchStatePacket::FlagInProgress);
    if (options_.friendly_fire) {
        state.flags |= net::MatchStatePacket::FlagFriendlyFire;
    }
    state.point_goal = static_cast<std::uint16_t>(std::clamp(
        entry.point_goal, 0, 65'535));
    state.match_id = match_id_;
    state.room_key = entry.room_key;
    state.next_room_key = options_.rotation.next().room_key;
    return state;
}

void DedicatedServer::handle(const net::ReceivedPacket& packet, double now) {
    switch (packet.type()) {
    case net::PacketType::Hello:
        handle_hello(packet, now);
        break;
    case net::PacketType::Intent:
        handle_intent(packet, now);
        break;
    case net::PacketType::Snapshot:
        handle_snapshot(packet, now);
        break;
    case net::PacketType::Bye: {
        const int index = find_peer(packet.sender);
        if (index >= 0) {
            remove_peer(static_cast<std::size_t>(index), "left");
        }
        break;
    }
    case net::PacketType::Identify:
        handle_identify(packet, now);
        break;
    case net::PacketType::Ping:
        transport_->send(packet.sender, net::PacketType::Pong);
        break;
    case net::PacketType::Pong:
        handle_pong(packet, now);
        break;
    case net::PacketType::StatusQuery:
        send_status(packet.sender, now);
        break;
    case net::PacketType::MatchEnd:
        handle_match_end(packet, now);
        break;
    case net::PacketType::Chat:
        handle_chat(packet, now);
        break;
    default:
        // The relay intentionally ignores packet types it does not own. This
        // keeps additive client features compatible with an older server.
        break;
    }
}

void DedicatedServer::send_refusal(const net::Endpoint& target,
                                    std::uint8_t reason) {
    const net::RefusedPacket refusal{
        reason,
        static_cast<std::uint8_t>(peers_.size()),
        static_cast<std::uint8_t>(options_.max_players)
    };
    const auto payload = refusal.encode();
    transport_->send(target, net::PacketType::Refused, payload);
}

void DedicatedServer::send_status(const net::Endpoint& target, double now) {
    net::ServerStatusPacket status;
    status.match = build_state(now);
    status.max_players = static_cast<std::uint8_t>(options_.max_players);
    status.protocol = net::NetConfig::ProtocolVersion;
    status.server_name = options_.server_name;
    const auto payload = status.encode();
    transport_->send(target, net::PacketType::StatusReply, payload);
}

void DedicatedServer::handle_hello(const net::ReceivedPacket& packet, double now) {
    const auto payload = packet.payload();
    if (payload.empty() || payload[0] != net::NetConfig::ProtocolVersion) {
        log("rejected " + packet.sender.to_string() + ": protocol mismatch");
        send_refusal(packet.sender, net::RefusedPacket::ReasonProtocol);
        return;
    }

    int index = find_peer(packet.sender);
    if (index < 0) {
        int slot = -1;
        if (payload.size() >= 2 && payload[1] != 0xff
            && payload[1] < options_.max_players
            && slot_free(payload[1])) {
            slot = payload[1];
        }
        if (slot < 0) {
            slot = next_free_slot();
        }
        if (slot < 0) {
            log("rejected " + packet.sender.to_string() + ": session full");
            send_refusal(packet.sender, net::RefusedPacket::ReasonFull);
            return;
        }
        if (peers_.empty()) {
            match_started_ = now;
            match_ended_at_ = -1;
            ++match_id_;
        }
        Peer peer;
        peer.endpoint = packet.sender;
        peer.slot = slot;
        peer.last_seen = now;
        peers_.push_back(std::move(peer));
        index = static_cast<int>(peers_.size() - 1);
        ever_occupied_.store(true);
        if (authority_slot_ < 0) {
            authority_slot_ = slot;
            log(packet.sender.to_string() + " joined as slot "
                + std::to_string(slot) + " (authority)");
            notify_authority(peers_[index]);
        } else {
            log(packet.sender.to_string() + " joined as slot "
                + std::to_string(slot));
        }
    }

    peers_[index].last_seen = now;
    const std::array<std::uint8_t, 1> slot_payload{
        static_cast<std::uint8_t>(peers_[index].slot)
    };
    transport_->send(peers_[index].endpoint, net::PacketType::Welcome,
                     slot_payload);
    const auto state = build_state(now).encode();
    transport_->send(peers_[index].endpoint, net::PacketType::MatchState, state);
    broadcast_roster();
}

void DedicatedServer::handle_identify(const net::ReceivedPacket& packet,
                                       double now) {
    const int index = find_peer(packet.sender);
    const auto payload = packet.payload();
    if (index < 0 || payload.empty()) {
        return;
    }
    Peer& peer = peers_[index];
    peer.last_seen = now;
    std::string name;
    for (std::size_t i = 1; i < payload.size() && payload[i] != 0; ++i) {
        name.push_back(static_cast<char>(payload[i]));
    }
    name = trim_ascii(std::move(name));
    if (name.empty()) {
        return;
    }
    if (name.size() > net::RosterPacket::MaxNameBytes) {
        name.resize(net::RosterPacket::MaxNameBytes);
    }
    const std::uint8_t hunter = payload[0];
    if (peer.name == name && peer.hunter == hunter) {
        return;
    }
    const bool first_name = peer.name.empty();
    peer.name = name;
    peer.hunter = hunter;
    log("slot " + std::to_string(peer.slot) + " is \"" + name
        + "\" playing hunter " + std::to_string(hunter));
    if (first_name) {
        announce(name + " joined");
    }
    broadcast_roster();
}

void DedicatedServer::handle_intent(const net::ReceivedPacket& packet,
                                     double now) {
    const int index = find_peer(packet.sender);
    if (index < 0 || authority_slot_ < 0) {
        return;
    }
    Peer& peer = peers_[index];
    peer.last_seen = now;
    const auto payload = packet.payload();
    if (const auto frame = net::IntentPacket::frame(payload)) {
        if (peer.last_intent_frame != 0 && *frame <= peer.last_intent_frame
            && peer.last_intent_frame - *frame < net::NetConfig::IntentResetGap) {
            return;
        }
        peer.last_intent_frame = *frame;
    }

    if (payload.size() + 1 > net::NetConfig::MaxPacketSize) {
        return;
    }
    std::vector<std::uint8_t> relayed(payload.size() + 1);
    relayed[0] = static_cast<std::uint8_t>(peer.slot);
    std::copy(payload.begin(), payload.end(), relayed.begin() + 1);
    for (std::size_t i = 0; i < peers_.size(); ++i) {
        if (static_cast<int>(i) != index) {
            transport_->send(peers_[i].endpoint, net::PacketType::SlotIntent,
                             relayed);
        }
    }
}

void DedicatedServer::handle_snapshot(const net::ReceivedPacket& packet,
                                       double now) {
    const int index = find_peer(packet.sender);
    if (index < 0) {
        return;
    }
    Peer& peer = peers_[index];
    peer.last_seen = now;
    if (peer.slot != authority_slot_) {
        return;
    }
    const auto payload = packet.payload();
    last_snapshot_.assign(payload.begin(), payload.end());
    for (std::size_t i = 0; i < peers_.size(); ++i) {
        if (static_cast<int>(i) != index) {
            transport_->send(peers_[i].endpoint, net::PacketType::Snapshot,
                             payload);
        }
    }
}

void DedicatedServer::handle_chat(const net::ReceivedPacket& packet, double now) {
    const int index = find_peer(packet.sender);
    const auto payload = packet.payload();
    if (index < 0 || payload.size() < net::ChatPacket::Size) {
        return;
    }
    Peer& peer = peers_[index];
    peer.last_seen = now;
    auto chat = net::ChatPacket::decode(payload);
    if (!chat || chat->text.empty()) {
        return;
    }
    peer.chat_credit = std::min(ChatBurst,
        peer.chat_credit + (now - peer.chat_credit_at) * ChatRatePerSecond);
    peer.chat_credit_at = now;
    if (peer.chat_credit < 1.0) {
        if (peer.chat_dropped++ == 0) {
            log("chat from slot " + std::to_string(peer.slot) + " ("
                + peer.endpoint.to_string() + ") dropped: too fast");
        }
        return;
    }
    peer.chat_credit -= 1.0;
    peer.chat_dropped = 0;
    chat->slot = static_cast<std::uint8_t>(peer.slot);
    chat->kind = net::ChatPacket::KindSay;
    chat->name = peer.name.empty() ? "Player" + std::to_string(peer.slot)
                                   : peer.name;
    const auto encoded = chat->encode();
    for (std::size_t i = 0; i < peers_.size(); ++i) {
        if (static_cast<int>(i) != index) {
            transport_->send(peers_[i].endpoint, net::PacketType::Chat, encoded);
        }
    }
    log("chat " + chat->name + ": " + chat->text);
}

void DedicatedServer::handle_pong(const net::ReceivedPacket& packet, double now) {
    const int index = find_peer(packet.sender);
    const auto payload = packet.payload();
    if (index < 0 || payload.empty()) {
        return;
    }
    Peer& peer = peers_[index];
    if (!peer.ping_pending || payload[0] != peer.ping_id) {
        return;
    }
    peer.ping_pending = false;
    peer.last_seen = now;
    int rtt = static_cast<int>(std::lround((now - peer.ping_sent_at) * 1000.0));
    rtt = std::clamp(rtt, 0, 9999);
    peer.ping = peer.ping == 0 ? rtt : (peer.ping * 2 + rtt) / 3;
}

void DedicatedServer::handle_match_end(const net::ReceivedPacket& packet,
                                        double now) {
    const int index = find_peer(packet.sender);
    if (index < 0) {
        return;
    }
    peers_[index].last_seen = now;
    if (peers_[index].slot == authority_slot_) {
        end_match(now, "a player reached the goal");
    }
}

void DedicatedServer::announce(std::string_view text) {
    if (peers_.empty()) {
        return;
    }
    const net::ChatPacket chat{
        0xff, net::ChatPacket::KindSystem, {}, std::string(text)
    };
    const auto payload = chat.encode();
    for (const Peer& peer : peers_) {
        transport_->send(peer.endpoint, net::PacketType::Chat, payload);
    }
}

void DedicatedServer::notify_authority(const Peer& peer) {
    if (!last_snapshot_.empty()) {
        transport_->send(peer.endpoint, net::PacketType::Snapshot, last_snapshot_);
    }
    const std::array<std::uint8_t, 1> payload{1};
    transport_->send(peer.endpoint, net::PacketType::Authority, payload);
}

void DedicatedServer::ping_peers(double now) {
    for (Peer& peer : peers_) {
        if (peer.ping_pending && now - peer.ping_sent_at < 5.0) {
            continue;
        }
        ++peer.ping_id;
        peer.ping_sent_at = now;
        peer.ping_pending = true;
        const std::array<std::uint8_t, 1> payload{peer.ping_id};
        transport_->send(peer.endpoint, net::PacketType::Ping, payload);
    }
}

void DedicatedServer::broadcast_roster() {
    if (peers_.empty()) {
        return;
    }
    net::RosterPacket roster;
    roster.count = static_cast<std::uint8_t>(
        std::min<std::size_t>(peers_.size(), net::RosterPacket::MaxSlots));
    for (std::size_t i = 0; i < roster.count; ++i) {
        roster.slots[i] = static_cast<std::uint8_t>(peers_[i].slot);
        roster.hunters[i] = peers_[i].hunter;
        roster.pings[i] = static_cast<std::uint16_t>(
            std::clamp(peers_[i].ping, 0, 9999));
        roster.names[i] = peers_[i].name.empty()
            ? "Player" + std::to_string(peers_[i].slot + 1)
            : peers_[i].name;
    }
    const auto payload = roster.encode();
    for (const Peer& peer : peers_) {
        transport_->send(peer.endpoint, net::PacketType::Roster, payload);
    }
}

void DedicatedServer::broadcast_match_state(double now) {
    if (peers_.empty()) {
        return;
    }
    const auto payload = build_state(now).encode();
    for (const Peer& peer : peers_) {
        transport_->send(peer.endpoint, net::PacketType::MatchState, payload);
    }
}

void DedicatedServer::remove_peer(std::size_t index, std::string_view reason) {
    if (index >= peers_.size()) {
        return;
    }
    const Peer departed = peers_[index];
    const bool was_authority = departed.slot == authority_slot_;
    peers_.erase(peers_.begin() + static_cast<std::ptrdiff_t>(index));
    broadcast_roster();
    log(departed.endpoint.to_string() + " " + std::string(reason)
        + " (slot " + std::to_string(departed.slot) + ")");
    if (!departed.name.empty()) {
        announce(departed.name + " " + std::string(reason));
    }
    if (!was_authority) {
        return;
    }
    authority_slot_ = peers_.empty() ? -1 : peers_.front().slot;
    if (authority_slot_ >= 0) {
        log("authority moved to slot " + std::to_string(authority_slot_));
        notify_authority(peers_.front());
    } else {
        log("no peers left; waiting for a new authority");
    }
}

void DedicatedServer::drop_timed_out(double now) {
    for (std::size_t i = peers_.size(); i-- > 0;) {
        if (now - peers_[i].last_seen > net::NetConfig::TimeoutSeconds) {
            remove_peer(i, "timed out");
        }
    }
}

void DedicatedServer::end_match(double now, std::string_view reason) {
    if (match_ended_at_ >= 0) {
        return;
    }
    match_ended_at_ = now;
    log("match over on " + options_.rotation.current().room_key + " ("
        + std::string(reason) + "); " + options_.rotation.next().room_key
        + " in " + std::to_string(EndSequenceSeconds) + " s");
    broadcast_match_state(now);
}

void DedicatedServer::advance_map(double now) {
    const RotationEntry& entry = options_.rotation.advance();
    match_started_ = now;
    match_ended_at_ = -1;
    ++match_id_;
    log("rotating to " + entry.room_key);
    const auto payload = build_state(now).encode();
    for (const Peer& peer : peers_) {
        transport_->send(peer.endpoint, net::PacketType::MapChange, payload);
    }
}

void DedicatedServer::log(std::string_view message) {
    std::cout << '[' << now_string() << "] [server] " << message << '\n';
}

} // namespace fruityprime
