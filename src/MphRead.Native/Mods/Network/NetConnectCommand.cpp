#include "Mods/Network/net_connect_command.hpp"

#include <algorithm>
#include <chrono>
#include <exception>
#include <thread>

namespace fruityprime::net {

ConnectCommandResult NetConnectCommand::run(
    const ConnectCommandOptions& options) {
    ConnectCommandResult result;
    try {
        if (!NetLaunch::join(options.join)) {
            result.error = std::string(NetLaunch::last_join_error());
            NetLaunch::disconnect();
            return result;
        }

        result.local_slot = NetLaunch::local_slot();
        result.room = NetLaunch::server_room();
        result.players = NetLaunch::build_players(
            options.join.hunter, 0, -1, result.local_slot);

        const int seconds = std::max(0, options.seconds);
        const auto deadline = std::chrono::steady_clock::now()
            + std::chrono::seconds(seconds);
        while (std::chrono::steady_clock::now() < deadline) {
            result.packets += NetLaunch::update().size();
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        result.packets += NetLaunch::update().size();
        result.ok = true;
    } catch (const std::exception& error) {
        result.error = error.what();
    } catch (...) {
        result.error = "native connect command failed with an unknown error";
    }
    NetLaunch::disconnect();
    return result;
}

} // namespace fruityprime::net
