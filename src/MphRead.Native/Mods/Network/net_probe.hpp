#pragma once

#include <cstdint>
#include <string>
#include <string_view>

namespace fruityprime::net {

struct ProbeResult {
    bool ok = false;
    int assigned_slot = -1;
    std::string message;
};

// One-shot reachability check. It owns a separate UDP transport so a
// launcher connection test cannot disturb the live NetSession.
class NetProbe final {
public:
    [[nodiscard]] static ProbeResult probe(
        std::string_view address, std::uint16_t port, int timeout_ms = 3000);
};

} // namespace fruityprime::net
