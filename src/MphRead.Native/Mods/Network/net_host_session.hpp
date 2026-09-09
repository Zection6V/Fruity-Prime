#pragma once

#include "Mods/Network/dedicated_server.hpp"
#include "Mods/Network/net_launch.hpp"

#include <memory>
#include <mutex>
#include <string>
#include <thread>

namespace fruityprime::net {

struct HostSessionOptions {
    ServerOptions server;
    JoinOptions join;
};

// Listen-host lifecycle.  The server remains the single owner of roster,
// match-clock, and authority state; this wrapper only starts it and joins the
// local client through the same NetLaunch path as a remote launcher join.
class NetHostSession final {
public:
    NetHostSession() = default;
    NetHostSession(const NetHostSession&) = delete;
    NetHostSession& operator=(const NetHostSession&) = delete;
    ~NetHostSession();

    [[nodiscard]] bool start_and_join(const HostSessionOptions& options);
    void stop() noexcept;

    [[nodiscard]] bool running() const noexcept;
    [[nodiscard]] std::uint16_t bound_port() const noexcept;
    [[nodiscard]] std::string last_error() const;

private:
    std::unique_ptr<DedicatedServer> server_;
    std::thread thread_;
    mutable std::mutex mutex_;
    std::string last_error_;
};

} // namespace fruityprime::net
