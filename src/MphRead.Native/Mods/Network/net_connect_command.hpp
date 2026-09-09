#pragma once

#include "Mods/Network/net_launch.hpp"

#include <array>
#include <cstddef>
#include <optional>
#include <string>

namespace fruityprime::net {

struct ConnectCommandOptions {
    JoinOptions join;
    int seconds = 1;
};

struct ConnectCommandResult {
    bool ok = false;
    int local_slot = -1;
    std::size_t packets = 0;
    std::optional<LaunchRoom> room;
    std::array<PlayerSlotPlan, NetConfig::SlotCapacity> players{};
    std::string error;
};

// Headless native counterpart of NetConnectCommand.cs. The managed command
// creates a RenderWindow after joining; the native GUI owns that frontend, so
// this portable command owns only the join/pump/cleanup lifetime and returns
// the same room and player plan for either frontend to consume.
class NetConnectCommand final {
public:
    [[nodiscard]] static ConnectCommandResult run(
        const ConnectCommandOptions& options);
};

} // namespace fruityprime::net
