#include "Mods/Network/master_client.hpp"

#include "Mods/Network/net_client.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <stdexcept>
#include <thread>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#endif

namespace fruityprime::net {
namespace {

[[nodiscard]] std::string address_string(std::uint32_t address) {
    in_addr value{};
    value.s_addr = htonl(address);
    char text[INET_ADDRSTRLEN]{};
    if (inet_ntop(AF_INET, &value, text, sizeof(text)) == nullptr) {
        return "0.0.0.0";
    }
    return text;
}

} // namespace

MasterListResult MasterClient::query(std::string_view host,
                                     std::uint16_t port,
                                     std::chrono::milliseconds timeout) {
    MasterListResult result;
    try {
        const Endpoint endpoint = NetClient::resolve_ipv4(host, port);
        NetTransport transport(0);
        // NetMasterClient.Query writes the protocol byte after the packet
        // type.  NetTransport does not add payload bytes automatically, so
        // omitting it makes the native request differ from NetMaster.cs even
        // though the current directory happens to ignore the byte.
        const std::array<std::uint8_t, 1> query{
            NetConfig::ProtocolVersion};
        transport.send(endpoint, PacketType::MasterQuery, query);
        const auto deadline = std::chrono::steady_clock::now() + timeout;
        std::size_t expected = 0;
        while (std::chrono::steady_clock::now() < deadline) {
            for (const ReceivedPacket& packet : transport.drain()) {
                if (packet.type() != PacketType::MasterList) {
                    continue;
                }
                const auto payload = packet.payload();
                if (payload.size() < 2) {
                    result.error = "master sent a malformed list";
                    return result;
                }
                const std::size_t count = payload[0];
                expected = std::max(expected, static_cast<std::size_t>(payload[1]));
                if (payload.size() < 2 + count * MasterEntryPacket::Size) {
                    result.error = "master sent a truncated list";
                    return result;
                }
                std::size_t offset = 2;
                for (std::size_t i = 0; i < count; ++i) {
                    const auto entry = MasterEntryPacket::decode(
                        payload.subspan(offset, MasterEntryPacket::Size));
                    if (!entry) {
                        result.error = "master sent a malformed entry";
                        return result;
                    }
                    // The managed client appends every valid wire entry in
                    // packet order.  Do not deduplicate here: the directory's
                    // total/count framing is the contract the caller sees.
                    result.servers.push_back(MasterListing{
                        address_string(entry->address), entry->port,
                        entry->players, entry->max_players, entry->mode,
                        entry->protocol, entry->server_name, entry->room_key
                    });
                    offset += MasterEntryPacket::Size;
                }
                result.answered = true;
                if (result.servers.size() >= expected) {
                    return result;
                }
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        if (!result.answered) {
            result.error = "master did not answer before the timeout";
        }
    } catch (const std::exception& error) {
        result.error = error.what();
    }
    return result;
}

HostReplyPacket MasterClient::request_host(std::string_view host,
                                           std::uint16_t port,
                                           const HostRequestPacket& request,
                                           std::chrono::milliseconds timeout) {
    HostReplyPacket result;
    try {
        const Endpoint endpoint = NetClient::resolve_ipv4(host, port);
        NetTransport transport(0);
        const auto payload = request.encode();
        transport.send(endpoint, PacketType::HostRequest, payload);
        const auto deadline = std::chrono::steady_clock::now() + timeout;
        while (std::chrono::steady_clock::now() < deadline) {
            for (const ReceivedPacket& packet : transport.drain()) {
                if (packet.type() != PacketType::HostReply) {
                    continue;
                }
                const auto reply = HostReplyPacket::decode(packet.payload());
                if (reply) {
                    return *reply;
                }
                result.reason = "master sent a malformed host reply";
                return result;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        result.reason = "master did not answer before the timeout";
    } catch (const std::exception& error) {
        result.reason = error.what();
    }
    return result;
}

} // namespace fruityprime::net
