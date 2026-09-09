#include "Mods/Network/net_master.hpp"

namespace fruityprime::net {

bool master_protocol_matches(std::uint8_t protocol) noexcept {
    return protocol == NetConfig::ProtocolVersion;
}

} // namespace fruityprime::net
