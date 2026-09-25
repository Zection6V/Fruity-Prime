#include "NetHostSession.hpp"

#include "../../GameState.hpp"
#include "NetLaunch.hpp"
#include "NetMaster.hpp"

#include "DedicatedServer.hpp"
#include "MapRotation.hpp"

#include <chrono>
#include <exception>
#include <iostream>
#include <memory>
#include <optional>
#include <stop_token>
#include <string>
#include <thread>
#include <tuple>
#include <utility>

namespace MphRead::Mods::Network
{
    std::shared_ptr<DedicatedServer> NetHostSession::_server{};
    std::shared_ptr<std::stop_source> NetHostSession::_cancel{};
    std::shared_ptr<std::thread> NetHostSession::_thread{};
    std::optional<std::string> NetHostSession::_lastError{};
    std::mutex NetHostSession::_lastErrorMutex{};

    bool NetHostSession::Running() noexcept
    {
        return _server != nullptr;
    }

    std::optional<std::string> NetHostSession::LastError()
    {
        const std::lock_guard lock(_lastErrorMutex);
        return _lastError;
    }

    void NetHostSession::SetLastError(std::optional<std::string> value)
    {
        const std::lock_guard lock(_lastErrorMutex);
        _lastError = std::move(value);
    }

    bool NetHostSession::StartAndJoin(std::int32_t port,
        const std::string& playerName, Hunter hunter,
        const std::string& roomKey, GameMode mode, float timeLimit,
        std::int32_t pointGoal, std::int32_t maxPlayers,
        std::optional<std::tuple<std::string, std::int32_t, std::string>> listing)
    {
        Stop();
        SetLastError(std::nullopt);
        std::shared_ptr<MapRotation> rotation
            = MapRotation::SingleMatch(roomKey, mode, timeLimit, pointGoal);
        auto server = std::make_shared<DedicatedServer>(port, maxPlayers, rotation);
        server->FriendlyFire(GameState::FriendlyFire());
        server->ShadowFreeze(GameState::ShadowFreeze());
        // The host's own affinity-weapons rule: a different row of the
        // damage table, so it is broadcast rather than left to each guest.
        server->AffinityWeapons(GameState::AffinityWeapons());
        // A thread inside the host's own game cannot run the match; the
        // host's client takes the authority. DedicatedServer.RunsTheMatch.
        server->RunsTheMatch(false);

        if (listing.has_value())
        {
            const auto& value = listing.value();
            server->ServerName(std::get<2>(value));
            server->Reporter(std::make_shared<MasterReporter>(
                std::get<0>(value), std::get<1>(value)));
            std::cout << "[net] listing this game on " << std::get<0>(value)
                << ":" << std::get<1>(value) << " as \""
                << std::get<2>(value) << "\"" << std::endl;
        }

        auto cancel = std::make_shared<std::stop_source>();
        _server = server;
        _cancel = cancel;
        _thread = std::make_shared<std::thread>();
        *_thread = std::thread([server, cancel]()
        {
            try
            {
                server->Run(cancel->get_token());
            }
            catch (const std::exception& ex)
            {
                SetLastError(std::string(ex.what()));
            }
        });
        _thread->detach();

        std::this_thread::sleep_for(std::chrono::milliseconds(250));
        if (LastError().has_value())
        {
            Stop();
            return false;
        }
        if (!NetLaunch::Join("127.0.0.1", port, playerName, hunter))
        {
            Stop();
            return false;
        }
        return true;
    }

    void NetHostSession::Stop()
    {
        if (_cancel != nullptr)
        {
            _cancel->request_stop();
        }
        if (_server != nullptr)
        {
            _server->Stop();
        }
        _server.reset();
        _cancel.reset();
        _thread.reset();
    }
}
