#pragma once

#include <string>
#include <string_view>

namespace fruityprime::net {

struct NetworkConditions;

// Value-side counterpart of NetLag.cs.  The transport keeps the random
// generator per socket, while this module owns the option grammar and the
// player-facing description so every caller reports the same simulated line.
class NetLag final {
public:
    [[nodiscard]] static NetworkConditions from_options(
        std::string_view netlag, std::string_view netloss);
    [[nodiscard]] static std::string describe(
        const NetworkConditions& conditions);
};

} // namespace fruityprime::net
