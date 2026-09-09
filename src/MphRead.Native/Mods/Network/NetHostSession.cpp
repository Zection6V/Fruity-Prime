#include "Mods/Network/net_host_session.hpp"

#include <chrono>
#include <exception>
#include <utility>
#include <thread>

namespace fruityprime::net {

NetHostSession::~NetHostSession() {
    stop();
}

bool NetHostSession::start_and_join(const HostSessionOptions& options) {
    stop();
    {
        std::lock_guard lock(mutex_);
        last_error_.clear();
    }
    try {
        server_ = std::make_unique<DedicatedServer>(options.server);
        DedicatedServer* server = server_.get();
        thread_ = std::thread([this, server]() {
            try {
                server->run();
            } catch (const std::exception& error) {
                std::lock_guard lock(mutex_);
                last_error_ = error.what();
            } catch (...) {
                std::lock_guard lock(mutex_);
                last_error_ = "host server stopped with an unknown error";
            }
        });

        const auto deadline = std::chrono::steady_clock::now()
            + std::chrono::seconds(3);
        while (!server->listening()
               && std::chrono::steady_clock::now() < deadline) {
            if (!last_error().empty()) {
                break;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
        if (!server->listening()) {
            {
                std::lock_guard lock(mutex_);
                if (last_error_.empty()) {
                    last_error_ = "host server did not start listening";
                }
            }
            stop();
            return false;
        }

        JoinOptions join = options.join;
        join.address = "127.0.0.1";
        // A listen-host join is necessarily for the server we just started;
        // do not let JoinOptions' remote-server default (27888) leak into
        // this path.
        join.port = server->bound_port();
        if (!NetLaunch::join(join)) {
            {
                std::lock_guard lock(mutex_);
                last_error_ = std::string(NetLaunch::last_join_error());
            }
            stop();
            return false;
        }
        return true;
    } catch (const std::exception& error) {
        {
            std::lock_guard lock(mutex_);
            last_error_ = error.what();
        }
        stop();
        return false;
    }
}

void NetHostSession::stop() noexcept {
    NetLaunch::disconnect();
    if (server_ != nullptr) {
        server_->stop();
    }
    if (thread_.joinable()) {
        thread_.join();
    }
    server_.reset();
}

bool NetHostSession::running() const noexcept {
    return server_ != nullptr && server_->listening();
}

std::uint16_t NetHostSession::bound_port() const noexcept {
    return server_ == nullptr ? 0 : server_->bound_port();
}

std::string NetHostSession::last_error() const {
    std::lock_guard lock(mutex_);
    return last_error_;
}

} // namespace fruityprime::net
