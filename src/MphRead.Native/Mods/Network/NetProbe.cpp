#include "Mods/Network/net_probe.hpp"

#include "Mods/Network/net_client.hpp"
#include "Mods/Network/net_transport.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <thread>

namespace fruityprime::net {

ProbeResult NetProbe::probe(std::string_view address, std::uint16_t port,
                            int timeout_ms) {
    ProbeResult result;
    if (address.empty()) {
        result.message = "Could not resolve an empty address.";
        return result;
    }
    const int bounded_timeout = std::max(1, timeout_ms);
    try {
        const Endpoint endpoint = NetClient::resolve_ipv4(address, port);
        NetTransport transport(0);
        const std::array<std::uint8_t, 1> hello{
            NetConfig::ProtocolVersion};
        transport.send(endpoint, PacketType::Hello, hello);
        const auto deadline = std::chrono::steady_clock::now()
            + std::chrono::milliseconds(bounded_timeout);
        while (std::chrono::steady_clock::now() < deadline) {
            for (const ReceivedPacket& packet : transport.drain()) {
                if (!(packet.sender == endpoint)) {
                    continue;
                }
                if (packet.type() == PacketType::Welcome
                    && !packet.payload().empty()) {
                    result.ok = true;
                    result.assigned_slot = packet.payload().front();
                    transport.send(endpoint, PacketType::Bye);
                    result.message = "Connected to " + endpoint.to_string()
                        + " -- server assigned slot "
                        + std::to_string(result.assigned_slot) + ".";
                    return result;
                }
                const auto type = static_cast<std::uint8_t>(packet.type());
                if (type >= static_cast<std::uint8_t>(PacketType::Hello)
                    && type <= static_cast<std::uint8_t>(PacketType::Authority)) {
                    continue;
                }
                result.message = endpoint.to_string()
                    + " replied, but not as an MphRead server.";
                return result;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        transport.send(endpoint, PacketType::Bye);
        result.message = "No reply from " + endpoint.to_string()
            + ". The server may be down, or UDP may be blocked by a firewall.";
    } catch (const std::exception& error) {
        result.message = "Could not reach " + std::string(address) + ":"
            + std::to_string(port) + ": " + error.what();
    }
    return result;
}

} // namespace fruityprime::net
