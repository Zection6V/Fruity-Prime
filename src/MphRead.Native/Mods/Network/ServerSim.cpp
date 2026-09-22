#include "ServerSim.hpp"

#include "../../Entities/Players/PlayerEntity.hpp"
#include "../../Metadata/Metadata.hpp"
#include "../Headless.hpp"
#include "../Input/SyntheticInput.hpp"
#include "../Render/FrameTiming.hpp"
#include "NetLaunch.hpp"
#include "NetLog.hpp"
#include "NetUnlagged.hpp"

#include <bit>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>

namespace MphRead::Mods::Network::Detail
{
    // Scene's constructor and these members occur in a later Native owner.
    // These declarations carry exactly the referenced C# operations and add no fallback.
    [[nodiscard]] std::shared_ptr<Scene> ServerSimCreateScene(
        std::int32_t width, std::int32_t height,
        Mods::Input::KeyboardState keyboard, Mods::Input::MouseState mouse);
    [[nodiscard]] std::int32_t ServerSimSceneRoomId(const Scene& scene);
    void ServerSimSceneAddRoom(
        Scene& scene, const std::string& roomKey, GameMode mode, std::int32_t playerCount);
    void ServerSimSceneOnLoad(Scene& scene);
    void ServerSimSceneOnSimulationFrame(Scene& scene);

    // These declarations bind later static owners without introducing fallback behavior.
    [[nodiscard]] std::string ServerSimFileSystem();
    [[nodiscard]] bool ServerSimIsTeamMode(GameMode mode);
    [[nodiscard]] GameMode ServerSimGameStateMode();
    void ServerSimReadClearCache();
    void ServerSimCollectGeneration2ForcedBlockingCompacting();
}

namespace
{
    [[nodiscard]] std::int64_t AddInt64Unchecked(
        std::int64_t left, std::int64_t right) noexcept
    {
        return std::bit_cast<std::int64_t>(
            static_cast<std::uint64_t>(left) + static_cast<std::uint64_t>(right));
    }

    void IncrementInt64(std::int64_t& value) noexcept
    {
        value = AddInt64Unchecked(value, 1);
    }

    [[nodiscard]] std::string FormatFixed(double value, std::int32_t digits)
    {
        std::ostringstream stream;
        stream << std::fixed << std::setprecision(digits) << value;
        return stream.str();
    }

    [[nodiscard]] std::string GameModeName(MphRead::GameMode mode)
    {
        switch (static_cast<std::int32_t>(mode))
        {
        case 0: return "None";
        case 2: return "SinglePlayer";
        case 3: return "Battle";
        case 4: return "BattleTeams";
        case 5: return "Survival";
        case 6: return "SurvivalTeams";
        case 7: return "Capture";
        case 8: return "Bounty";
        case 9: return "BountyTeams";
        case 10: return "Nodes";
        case 11: return "NodesTeams";
        case 12: return "Defender";
        case 13: return "DefenderTeams";
        case 14: return "PrimeHunter";
        case 15: return "Unknown15";
        default: return std::to_string(static_cast<std::int32_t>(mode));
        }
    }
}

namespace MphRead::Mods::Network
{
    void ServerSim::Advance(double now)
    {
        if (_scene == nullptr)
        {
            return;
        }
        if (_lastAdvance < 0.0)
        {
            _lastAdvance = now;
            return;
        }

        const double elapsed = now - _lastAdvance;
        _lastAdvance = now;
        if (elapsed > StallSeconds)
        {
            IncrementInt64(_stalls);
            _accumulator = 0.0;
            return;
        }

        _accumulator += elapsed;
        std::int32_t steps = 0;
        while (_accumulator >= Mods::Render::FrameTiming::StepSeconds
            && steps < Mods::Render::FrameTiming::MaxCatchUpSteps)
        {
            _accumulator -= Mods::Render::FrameTiming::StepSeconds;
            Step();
            ++steps;
        }
        if (_accumulator >= Mods::Render::FrameTiming::StepSeconds)
        {
            const auto dropped = static_cast<std::int64_t>(
                _accumulator / Mods::Render::FrameTiming::StepSeconds);
            _droppedSteps = AddInt64Unchecked(_droppedSteps, dropped);
            _accumulator = 0.0;
        }
    }

    std::string ServerSim::Room() const
    {
        if (_scene == nullptr)
        {
            return "";
        }

        const std::int32_t roomId = Detail::ServerSimSceneRoomId(*_scene);
        const RoomMetadata* metadata = Metadata::GetRoomById(roomId, true);
        const std::string current = metadata != nullptr ? metadata->Name : "";
        return !current.empty() ? current : _room;
    }

    bool ServerSim::Available(std::string& reason)
    {
        try
        {
            const std::string root = Detail::ServerSimFileSystem();
            std::error_code error;
            const bool exists = !root.empty() && std::filesystem::is_directory(root, error);
            if (root.empty() || error || !exists)
            {
                reason = "no game files are set up on this machine (see paths.txt)";
                return false;
            }
        }
        catch (const std::exception& ex)
        {
            reason = "game files could not be located (" + std::string(ex.what()) + ")";
            return false;
        }

        std::string inputReason;
        if (!Mods::Input::SyntheticInput::Available(inputReason))
        {
            reason = "a keyboard could not be synthesised (" + inputReason + ")";
            return false;
        }

        reason.clear();
        return true;
    }

    bool ServerSim::Start(const std::string& roomKey, GameMode mode,
        std::int32_t maxPlayers, SnapshotSink sink, std::function<void()> matchEnded)
    {
        Stop();
        Mods::Headless::Enter();

        try
        {
            _room = roomKey;
            _mode = mode;

            const std::int32_t capacity = Entities::PlayerEntity::SlotCapacity;
            const std::int32_t clampedPlayers
                = maxPlayers < 2 ? 2 : (maxPlayers > capacity ? capacity : maxPlayers);
            Entities::PlayerEntity::SetMaxPlayers(clampedPlayers);

            NetSession::StartServerAuthority(sink, matchEnded);

            auto keyboard = Mods::Input::SyntheticInput::CreateKeyboard();
            auto mouse = Mods::Input::SyntheticInput::CreateMouse();
            std::shared_ptr<Scene> scene = Detail::ServerSimCreateScene(
                256, 192, std::move(keyboard), std::move(mouse));

            NetLaunch::BuildPlayers(*scene, Hunter::Samus, 0,
                Detail::ServerSimIsTeamMode(mode), -1);
            Detail::ServerSimSceneAddRoom(
                *scene, roomKey, mode, NetLaunch::RoomPlayerCount);
            Detail::ServerSimSceneOnLoad(*scene);

            _scene = std::move(scene);
            _frames = 0;
            _stepSeconds = 0.0;
            _worstStepSeconds = 0.0;
            _overrunSteps = 0;
            _stepFailures = 0;
            _droppedSteps = 0;
            _stalls = 0;
            _accumulator = 0.0;
            _lastAdvance = -1.0;
            return true;
        }
        catch (const std::exception& ex)
        {
            const std::string error = ex.what();
            std::cout << "[sim] could not load \"" << roomKey << "\": " << error << '\n';
            NetLog::Event("server simulation failed to start: " + error);
            Stop();
            return false;
        }
    }

    void ServerSim::Step()
    {
        if (_scene == nullptr)
        {
            return;
        }

        const auto start = std::chrono::steady_clock::now();
        try
        {
            Detail::ServerSimSceneOnSimulationFrame(*_scene);
        }
        catch (const std::exception& ex)
        {
            IncrementInt64(_stepFailures);
            if (_stepFailures == 1)
            {
                std::cout << "[sim] step failed: " << ex.what() << '\n';
            }
            else
            {
                std::cout << "[sim] step failed: " << ex.what() << '\n';
            }
            NetLog::Event("server simulation step failed: " + std::string(ex.what()));
        }

        const double elapsed = std::chrono::duration<double>(
            std::chrono::steady_clock::now() - start).count();
        IncrementInt64(_frames);
        _stepSeconds += elapsed;
        if (elapsed > _worstStepSeconds)
        {
            _worstStepSeconds = elapsed;
        }
        if (elapsed > 1.0 / 60.0)
        {
            IncrementInt64(_overrunSteps);
        }
    }

    void ServerSim::Stop()
    {
        if (_scene == nullptr)
        {
            return;
        }

        _scene.reset();
        _room.clear();
        NetSession::Stop();
        Detail::ServerSimReadClearCache();
        Detail::ServerSimCollectGeneration2ForcedBlockingCompacting();
    }

    std::string ServerSim::DescribeUnlagged() const
    {
        return NetUnlagged::Describe();
    }

    std::string ServerSim::Describe() const
    {
        if (_scene == nullptr)
        {
            return "not simulating";
        }

        const double mean = _frames > 0
            ? _stepSeconds / static_cast<double>(_frames) * 1000.0
            : 0.0;

        std::string result = Room() + " (" + GameModeName(Detail::ServerSimGameStateMode())
            + "), " + std::to_string(_frames) + " step(s), "
            + FormatFixed(mean, 2) + " ms mean, "
            + FormatFixed(_worstStepSeconds * 1000.0, 1) + " ms worst, "
            + std::to_string(_overrunSteps) + " overrun, "
            + std::to_string(_droppedSteps) + " dropped, "
            + std::to_string(_stalls) + " stall(s)";
        if (_stepFailures > 0)
        {
            result += ", " + std::to_string(_stepFailures) + " FAILED";
        }
        return result;
    }
}
