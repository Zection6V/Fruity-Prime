#include "Mods/Network/net_client.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstring>
#include <stdexcept>
#include <thread>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/socket.h>
#endif

namespace fruityprime::net {

Endpoint NetClient::resolve_ipv4(std::string_view host, std::uint16_t port) {
    addrinfo hints{};
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;
    addrinfo* result = nullptr;
    const std::string name(host);
#ifdef _WIN32
    WSADATA wsa_data{};
    if (WSAStartup(MAKEWORD(2, 2), &wsa_data) != 0) {
        throw std::runtime_error("WSAStartup failed while resolving " + name);
    }
#endif
    const int error = getaddrinfo(name.c_str(), nullptr, &hints, &result);
    if (error != 0 || result == nullptr) {
        if (result != nullptr) {
            freeaddrinfo(result);
        }
#ifdef _WIN32
        WSACleanup();
#endif
        throw std::runtime_error("could not resolve " + name);
    }
    Endpoint endpoint;
    endpoint.address = *reinterpret_cast<const sockaddr_in*>(result->ai_addr);
    endpoint.address.sin_port = htons(port);
    freeaddrinfo(result);
#ifdef _WIN32
    WSACleanup();
#endif
    return endpoint;
}

NetClient::NetClient(Endpoint server, NetworkConditions conditions)
    : server_(server), conditions_(conditions) {}

NetClient::~NetClient() {
    disconnect();
}

std::uint16_t NetClient::local_port() const noexcept {
    return transport_ == nullptr ? 0 : transport_->local_port();
}

bool NetClient::connect(std::uint8_t requested_slot,
                        std::chrono::milliseconds timeout) {
    disconnect();
    last_error_.clear();
    try {
        // NetLag.cs is process-wide. An empty value means this caller did
        // not inject a private native test condition, so use the transport's
        // global path and let the startup option apply to every socket.
        transport_ = conditions_.active()
            ? std::make_unique<NetTransport>(0, conditions_)
            : std::make_unique<NetTransport>(0);
        const std::array<std::uint8_t, 2> hello{
            NetConfig::ProtocolVersion, requested_slot
        };
        // UDP has no delivery guarantee.  A lost first Hello used to make a
        // healthy server look offline until the caller retried the whole
        // connection.  Re-send the idempotent handshake while waiting; the
        // relay treats a repeated Hello from the same endpoint as a refresh.
        constexpr auto hello_retry = std::chrono::milliseconds(250);
        const auto deadline = std::chrono::steady_clock::now() + timeout;
        auto next_hello = std::chrono::steady_clock::now();
        while (std::chrono::steady_clock::now() < deadline) {
            const auto now = std::chrono::steady_clock::now();
            if (now >= next_hello) {
                transport_->send(server_, PacketType::Hello, hello);
                next_hello = now + hello_retry;
            }
            for (const ReceivedPacket& packet : transport_->drain()) {
                if (!(packet.sender == server_)) {
                    continue;
                }
                if (packet.type() == PacketType::Welcome
                    && !packet.payload().empty()) {
                    local_slot_ = packet.payload()[0];
                    connected_ = true;
                    return true;
                }
                if (packet.type() == PacketType::Refused) {
                    const auto refusal = RefusedPacket::decode(packet.payload());
                    if (refusal) {
                        last_error_ = refusal->reason == RefusedPacket::ReasonFull
                            ? "server is full"
                            : refusal->reason == RefusedPacket::ReasonProtocol
                                ? "server protocol mismatch"
                                : "server refused the connection";
                    } else {
                        last_error_ = "server sent a malformed refusal";
                    }
                    transport_.reset();
                    return false;
                }
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        last_error_ = "server did not assign a slot before the timeout (target "
            + server_.to_string() + ", local UDP "
            + std::to_string(transport_ == nullptr ? 0 : transport_->local_port())
            + ")";
    } catch (const std::exception& error) {
        last_error_ = error.what();
    }
    transport_.reset();
    local_slot_ = -1;
    connected_ = false;
    return false;
}

void NetClient::disconnect() noexcept {
    if (transport_ != nullptr && connected_) {
        try {
            transport_->send(server_, PacketType::Bye);
        } catch (...) {
            // The socket is going away; there is no useful recovery here.
        }
    }
    connected_ = false;
    local_slot_ = -1;
    transport_.reset();
}

void NetClient::identify(std::uint8_t hunter, std::string_view name) {
    if (!connected_ || transport_ == nullptr) {
        throw std::logic_error("client is not connected");
    }
    const std::size_t count = std::min(name.size(),
                                       RosterPacket::MaxNameBytes);
    std::vector<std::uint8_t> payload(1 + count);
    payload[0] = hunter;
    std::transform(name.begin(), name.begin() + static_cast<std::ptrdiff_t>(count),
                   payload.begin() + 1,
                   [](char value) { return static_cast<std::uint8_t>(value); });
    transport_->send(server_, PacketType::Identify, payload);
}

void NetClient::send(PacketType type, std::span<const std::uint8_t> payload) {
    if (!connected_ || transport_ == nullptr) {
        throw std::logic_error("client is not connected");
    }
    transport_->send(server_, type, payload);
}

std::vector<ReceivedPacket> NetClient::drain() {
    return transport_ == nullptr ? std::vector<ReceivedPacket>{}
                                 : transport_->drain();
}

ServerStatusResult NetClient::query_status(std::string_view host,
                                           std::uint16_t port,
                                           std::chrono::milliseconds timeout) {
    ServerStatusResult result;
    try {
        const Endpoint endpoint = resolve_ipv4(host, port);
        NetTransport transport(0);
        const std::array<std::uint8_t, 1> query{NetConfig::ProtocolVersion};
        const auto started = std::chrono::steady_clock::now();
        transport.send(endpoint, PacketType::StatusQuery, query);
        const auto deadline = started + timeout;
        while (std::chrono::steady_clock::now() < deadline) {
            for (const ReceivedPacket& packet : transport.drain()) {
                if (!(packet.sender == endpoint)
                    || packet.type() != PacketType::StatusReply) {
                    continue;
                }
                const auto status = ServerStatusPacket::decode(packet.payload());
                if (!status) {
                    result.error = "server sent a malformed status";
                    return result;
                }
                result.online = true;
                result.match = status->match;
                result.max_players = status->max_players;
                result.protocol = status->protocol;
                result.server_name = status->server_name;
                result.latency_ms = static_cast<int>(std::chrono::duration_cast<
                    std::chrono::milliseconds>(std::chrono::steady_clock::now()
                                               - started).count());
                return result;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        result.error = "no status reply before the timeout";
    } catch (const std::exception& error) {
        result.error = error.what();
    }
    return result;
}

} // namespace fruityprime::net
