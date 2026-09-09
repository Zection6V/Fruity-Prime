#pragma once

#include "Mods/Network/net_protocol.hpp"

#include <cstddef>
#include <cstdint>
#include <string_view>

namespace fruityprime::net {

// Configuration shared by the dedicated-server reporter, directory server,
// and launcher browser. These values are the native counterpart of the
// managed NetMasterConfig class; keeping them here prevents the wire owners
// from drifting apart.
struct NetMasterConfig {
    static constexpr std::string_view DefaultHost = "net.livetek.fr";
    static constexpr std::uint16_t DefaultPort = 27889;
    static constexpr double HeartbeatSeconds = 15.0;
    static constexpr double ExpirySeconds = 50.0;
    static constexpr std::size_t EntriesPerPacket =
        (NetConfig::MaxPacketSize - 1 - 2) / MasterEntryPacket::Size;
};

[[nodiscard]] bool master_protocol_matches(std::uint8_t protocol) noexcept;

} // namespace fruityprime::net
