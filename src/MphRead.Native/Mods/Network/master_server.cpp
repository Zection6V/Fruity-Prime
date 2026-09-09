#include "Mods/Network/master_server.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <sstream>
#include <stdexcept>
#include <utility>

#ifdef _WIN32
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

namespace fruityprime {
namespace {

constexpr double HeartbeatReportSeconds = 60.0;
constexpr double PortCooldownSeconds = 5.0;
constexpr double HostedStartupSeconds = 180.0;
constexpr double HostedEmptySeconds = 45.0;

[[nodiscard]] std::uint16_t read_u16_le(std::span<const std::uint8_t> source) {
    return static_cast<std::uint16_t>(source[0])
        | static_cast<std::uint16_t>(source[1]) << 8;
}

[[nodiscard]] bool valid_game_mode(std::uint8_t value) noexcept {
    return value == static_cast<std::uint8_t>(GameMode::None)
        || (value >= static_cast<std::uint8_t>(GameMode::SinglePlayer)
            && value <= static_cast<std::uint8_t>(GameMode::Unknown15));
}

[[nodiscard]] std::string clock_string() {
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

MasterServer::MasterServer(MasterOptions options)
    : options_(std::move(options)) {}

MasterServer::~MasterServer() {
    shutdown();
}

void MasterServer::set_host_ports(int first, int last) noexcept {
    options_.host_port_first = first;
    options_.host_port_last = last;
}

bool MasterServer::can_host() const noexcept {
    return options_.host_port_first > 0
        && options_.host_port_last >= options_.host_port_first;
}

bool MasterServer::set_public_address(std::string_view host) {
    addrinfo hints{};
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;
    addrinfo* result = nullptr;
    const std::string name(host);
#ifdef _WIN32
    WSADATA data{};
    const bool started_winsock = WSAStartup(MAKEWORD(2, 2), &data) == 0;
#endif
    const int error = getaddrinfo(name.c_str(), nullptr, &hints, &result);
    if (error != 0 || result == nullptr) {
        log("cannot publish local servers as \"" + name + "\": no IPv4 address");
        if (result != nullptr) {
            freeaddrinfo(result);
        }
#ifdef _WIN32
        if (started_winsock) {
            WSACleanup();
        }
#endif
        return false;
    }
    const auto* address = reinterpret_cast<const sockaddr_in*>(result->ai_addr);
    public_address_ = ntohl(address->sin_addr.s_addr);
    char text[INET_ADDRSTRLEN]{};
    inet_ntop(AF_INET, &address->sin_addr, text, sizeof(text));
    public_name_ = name + " (" + text + ")";
    freeaddrinfo(result);
#ifdef _WIN32
    if (started_winsock) {
        WSACleanup();
    }
#endif
    return true;
}

std::uint16_t MasterServer::bound_port() const noexcept {
    const std::uint16_t bound = bound_port_.load();
    return bound == 0 ? options_.port : bound;
}

void MasterServer::run(std::chrono::milliseconds duration) {
    if (running_.exchange(true)) {
        throw std::logic_error("master server is already running");
    }
    try {
        transport_ = std::make_unique<net::NetTransport>(options_.port);
        bound_port_.store(transport_->local_port());
        listening_.store(true);
        if (!options_.public_address.empty()) {
            set_public_address(options_.public_address);
        }
        log("listening on UDP " + std::to_string(transport_->local_port()));
        log("servers are dropped after "
            + std::to_string(net::NetMasterConfig::ExpirySeconds)
            + " s of silence");
        if (public_address_ != 0) {
            log("servers on this machine are listed as " + public_name_);
        }
        log(can_host()
            ? "can start games on ports " + std::to_string(options_.host_port_first)
                + "-" + std::to_string(options_.host_port_last)
                + " for players who cannot open one of their own"
            : "not starting games for anybody (no host port range)");

        const auto started_at = std::chrono::steady_clock::now();
        double last_report = 0;
        while (running_.load()) {
            const double now = std::chrono::duration<double>(
                std::chrono::steady_clock::now() - started_at).count();
            if (duration.count() > 0
                && now >= std::chrono::duration<double>(duration).count()) {
                running_.store(false);
                break;
            }
            for (const net::ReceivedPacket& packet : transport_->drain()) {
                handle(packet, now);
            }
            expire(now);
            reap_hosted(now);
            if (now - last_report >= HeartbeatReportSeconds) {
                last_report = now;
                log(std::to_string(entries_.size()) + " server(s) listed"
                    + (hosted_.empty()
                        ? std::string{}
                        : ", " + std::to_string(hosted_.size()) + " started here"));
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
        }
    } catch (...) {
        shutdown();
        throw;
    }
    shutdown();
}

void MasterServer::handle(const net::ReceivedPacket& packet, double now) {
    switch (packet.type()) {
    case net::PacketType::MasterHeartbeat:
        handle_heartbeat(packet, now);
        break;
    case net::PacketType::MasterQuery:
        send_list(packet.sender);
        break;
    case net::PacketType::HostRequest:
        handle_host_request(packet, now);
        break;
    case net::PacketType::Bye:
        handle_farewell(packet);
        break;
    default:
        break;
    }
}

void MasterServer::handle_heartbeat(const net::ReceivedPacket& packet, double now) {
    const auto heartbeat = net::MasterHeartbeatPacket::decode(packet.payload());
    if (!heartbeat) {
        return;
    }
    const std::uint32_t observed_address = address_number(packet.sender.address);
    std::uint32_t advertised_address = observed_address;
    if (public_address_ != 0 && is_local_address(packet.sender.address)) {
        advertised_address = public_address_;
    }
    const std::uint16_t game_port = heartbeat->port != 0
        ? heartbeat->port
        : ntohs(packet.sender.address.sin_port);
    net::Endpoint key = packet.sender;
    key.address.sin_port = htons(game_port);

    auto entry = std::find_if(entries_.begin(), entries_.end(),
        [&key](const Entry& candidate) { return candidate.key == key; });
    if (entry == entries_.end()) {
        Entry created;
        created.key = key;
        entries_.push_back(std::move(created));
        entry = std::prev(entries_.end());
        log("+ " + key.to_string() + " \"" + heartbeat->server_name + "\"");
    }
    entry->address = advertised_address;
    entry->port = game_port;
    entry->players = heartbeat->players;
    entry->max_players = heartbeat->max_players;
    entry->mode = heartbeat->mode;
    entry->protocol = heartbeat->protocol;
    entry->server_name = heartbeat->server_name;
    entry->room_key = heartbeat->room_key;
    entry->last_seen = now;
}

void MasterServer::handle_farewell(const net::ReceivedPacket& packet) {
    const auto payload = packet.payload();
    if (payload.size() < 2) {
        return;
    }
    const std::uint16_t port = read_u16_le(payload);
    net::Endpoint key = packet.sender;
    key.address.sin_port = htons(port);
    const auto entry = std::find_if(entries_.begin(), entries_.end(),
        [&key](const Entry& candidate) { return candidate.key == key; });
    if (entry != entries_.end()) {
        log("- " + entry->key.to_string() + " \"" + entry->server_name
            + "\" (said goodbye)");
        entries_.erase(entry);
    }
}

void MasterServer::send_list(const net::Endpoint& target) {
    constexpr std::size_t entries_per_packet =
        net::NetMasterConfig::EntriesPerPacket;
    const std::size_t total = std::min<std::size_t>(entries_.size(), 255);
    std::size_t sent = 0;
    do {
        const std::size_t count = std::min(entries_per_packet, total - sent);
        std::vector<std::uint8_t> payload(2 + count * net::MasterEntryPacket::Size);
        payload[0] = static_cast<std::uint8_t>(count);
        payload[1] = static_cast<std::uint8_t>(total);
        std::size_t offset = 2;
        for (std::size_t i = 0; i < count; ++i) {
            const Entry& entry = entries_[sent + i];
            const net::MasterEntryPacket wire{
                entry.address, entry.port, entry.players, entry.max_players,
                entry.mode, entry.protocol, entry.server_name, entry.room_key
            };
            const auto bytes = wire.encode();
            std::copy(bytes.begin(), bytes.end(), payload.begin()
                                                   + static_cast<std::ptrdiff_t>(offset));
            offset += net::MasterEntryPacket::Size;
        }
        transport_->send(target, net::PacketType::MasterList, payload);
        sent += count;
    } while (sent < total);
}

void MasterServer::handle_host_request(const net::ReceivedPacket& packet,
                                       double now) {
    net::HostReplyPacket reply;
    const auto request = net::HostRequestPacket::decode(packet.payload());
    if (!request) {
        reply.reason = "malformed request";
    } else if (request->protocol != net::NetConfig::ProtocolVersion) {
        reply.reason = "this directory speaks protocol "
            + std::to_string(net::NetConfig::ProtocolVersion)
            + ", your build speaks " + std::to_string(request->protocol);
    } else if (!can_host()) {
        reply.reason = "this directory does not start games";
    } else {
        reply = start_hosted(*request, packet.sender, now);
    }
    const auto payload = reply.encode();
    transport_->send(packet.sender, net::PacketType::HostReply, payload);
    if (!reply.started) {
        log("refused a game for " + packet.sender.to_string() + ": " + reply.reason);
    }
}

net::HostReplyPacket MasterServer::start_hosted(
    const net::HostRequestPacket& request, const net::Endpoint& asker,
    double now) {
    for (int i = static_cast<int>(hosted_.size()) - 1; i >= 0; --i) {
        if (hosted_[i].asker.address.sin_addr.s_addr == asker.address.sin_addr.s_addr
            && hosted_[i].server->peer_count() == 0) {
            stop_hosted(static_cast<std::size_t>(i),
                        "the same player asked for another game", now);
        }
    }
    const int port = free_host_port(now);
    if (port < 0) {
        return net::HostReplyPacket{
            false, 0,
            "all " + std::to_string(options_.host_port_last
                - options_.host_port_first + 1) + " game slots are busy"
        };
    }

    const GameMode mode = valid_game_mode(request.mode)
        ? static_cast<GameMode>(request.mode) : GameMode::Battle;
    const std::string name = request.server_name.empty()
        ? "Hosted game" : request.server_name;
    ServerOptions server_options;
    server_options.port = static_cast<std::uint16_t>(port);
    server_options.max_players = std::clamp<int>(
        static_cast<int>(request.max_players), 2,
        static_cast<int>(net::NetConfig::SlotCapacity));
    server_options.rotation = MapRotation::single_match(
        request.room_key, mode, static_cast<float>(request.time_limit),
        request.point_goal);
    server_options.server_name = name;
    server_options.advertise = true;
    server_options.master_host = "127.0.0.1";
    server_options.master_port = bound_port();

    auto server = std::make_unique<DedicatedServer>(std::move(server_options));
    DedicatedServer* server_pointer = server.get();
    std::thread thread([server_pointer, port]() {
        try {
            server_pointer->run();
        } catch (const std::exception& error) {
            std::cerr << "[master] game on " << port << " stopped: "
                      << error.what() << '\n';
        }
    });

    for (int i = 0; i < 50 && !server->listening(); ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    if (!server->listening()) {
        server->stop();
        if (thread.joinable()) {
            thread.join();
        }
        return net::HostReplyPacket{false, 0,
                                    "could not listen on port " + std::to_string(port)};
    }

    Hosted hosted;
    hosted.server = std::move(server);
    hosted.thread = std::move(thread);
    hosted.port = port;
    hosted.name = name;
    hosted.asker = asker;
    hosted.started_at = now;
    hosted.last_occupied = now;
    hosted_.push_back(std::move(hosted));
    log("started \"" + name + "\" on port " + std::to_string(port)
        + " for " + asker.to_string() + " (" + request.room_key + ")");
    return net::HostReplyPacket{true, static_cast<std::uint16_t>(port), {}};
}

int MasterServer::free_host_port(double now) {
    for (int port = options_.host_port_first; port <= options_.host_port_last;
         ++port) {
        const bool taken = std::any_of(hosted_.begin(), hosted_.end(),
            [port](const Hosted& hosted) { return hosted.port == port; });
        if (taken) {
            continue;
        }
        const auto cooling = cooling_.find(port);
        if (cooling != cooling_.end()) {
            if (now - cooling->second < PortCooldownSeconds) {
                continue;
            }
            cooling_.erase(cooling);
        }
        return port;
    }
    return -1;
}

void MasterServer::reap_hosted(double now) {
    for (int i = static_cast<int>(hosted_.size()) - 1; i >= 0; --i) {
        Hosted& hosted = hosted_[i];
        if (hosted.server->peer_count() > 0) {
            hosted.last_occupied = now;
            continue;
        }
        const double grace = hosted.server->ever_occupied()
            ? HostedEmptySeconds : HostedStartupSeconds;
        if (now - hosted.last_occupied > grace) {
            stop_hosted(static_cast<std::size_t>(i),
                        hosted.server->ever_occupied()
                            ? "everyone left" : "nobody joined", now);
        }
    }
}

void MasterServer::stop_hosted(std::size_t index, std::string_view reason,
                               double now) {
    if (index >= hosted_.size()) {
        return;
    }
    Hosted& hosted = hosted_[index];
    log("stopping \"" + hosted.name + "\" on port "
        + std::to_string(hosted.port) + ": " + std::string(reason));
    hosted.server->stop();
    if (hosted.thread.joinable()) {
        hosted.thread.join();
    }
    cooling_[hosted.port] = now;
    const int port = hosted.port;
    hosted_.erase(hosted_.begin() + static_cast<std::ptrdiff_t>(index));
    unlist(port);
}

void MasterServer::unlist(int port) {
    entries_.erase(std::remove_if(entries_.begin(), entries_.end(),
        [port](const Entry& entry) {
            return entry.port == port
                && ntohl(entry.key.address.sin_addr.s_addr) == 0x7f000001u;
        }), entries_.end());
}

void MasterServer::expire(double now) {
    for (int i = static_cast<int>(entries_.size()) - 1; i >= 0; --i) {
        if (now - entries_[i].last_seen
            > net::NetMasterConfig::ExpirySeconds) {
            log("- " + entries_[i].key.to_string() + " \""
                + entries_[i].server_name + "\"");
            entries_.erase(entries_.begin() + i);
        }
    }
}

void MasterServer::shutdown() {
    running_.store(false);
    const double now = 0;
    while (!hosted_.empty()) {
        stop_hosted(hosted_.size() - 1, "the directory is shutting down", now);
    }
    listening_.store(false);
    transport_.reset();
    bound_port_.store(0);
}

bool MasterServer::is_local_address(const sockaddr_in& address) noexcept {
    const std::uint32_t value = address_number(address);
    const std::uint8_t first = static_cast<std::uint8_t>(value >> 24);
    const std::uint8_t second = static_cast<std::uint8_t>(value >> 16);
    return first == 127 || first == 10
        || (first == 172 && second >= 16 && second <= 31)
        || (first == 192 && second == 168)
        || (first == 169 && second == 254);
}

std::uint32_t MasterServer::address_number(const sockaddr_in& address) noexcept {
    return ntohl(address.sin_addr.s_addr);
}

void MasterServer::log(std::string_view message) {
    std::cout << '[' << clock_string() << "] [master] " << message << '\n';
}

} // namespace fruityprime
