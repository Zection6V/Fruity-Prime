#include "ServerSim.hpp"

#include "../../Formats/Formats.hpp"
#include "../../GameState.hpp"
#include "../../NativeRuntime/System/Runtime.hpp"
#include "../../Read.hpp"
#include "../../Scene.hpp"

#include "../../Entities/Players/PlayerEntity.hpp"
#include "../../Metadata/Metadata.hpp"
#include "../Headless.hpp"
#include "../Input/SyntheticInput.hpp"
#include "../Render/FrameTiming.hpp"
#include "NetLaunch.hpp"
#include "NetLog.hpp"
#include "NetUnlagged.hpp"
#include "../../NativeRuntime/System/IO.hpp"
#include "../../NativeRuntime/System/Managed.hpp"

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

using ::MphRead::NativeRuntime::IncrementInPlace;
using ::MphRead::NativeRuntime::UncheckedAdd;

namespace
{
    [[nodiscard]] std::string FormatFixed(double value, std::int32_t digits)
    {
        std::ostringstream stream;
        stream << std::fixed << std::setprecision(digits) << value;
        return stream.str();
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
            IncrementInPlace(_stalls);
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
            _droppedSteps = UncheckedAdd(_droppedSteps, dropped);
            _accumulator = 0.0;
        }
    }

    std::string ServerSim::Room() const
    {
        if (_scene == nullptr)
        {
            return "";
        }

        const std::int32_t roomId = (*_scene).RoomId();
        const RoomMetadata* metadata = Metadata::GetRoomById(roomId, true);
        const std::string current = metadata != nullptr ? metadata->Name : "";
        return !current.empty() ? current : _room;
    }

    bool ServerSim::Available(std::string& reason)
    {
        try
        {
            const std::string root = Paths::FileSystem();
            if (root.empty() || !MphRead::NativeRuntime::DirectoryExists(root))
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
            // The DS's own projection: nothing here builds one, so a stray
            // aspect ratio is at least the right one.
            std::shared_ptr<Scene> scene = std::make_shared<Scene>(
                OpenTK::Mathematics::Vector2i(256, 192), *keyboard, *mouse,
                [](std::string) {}, []() {});

            NetLaunch::BuildPlayers(*scene, Hunter::Samus, 0,
                GameState::IsTeamMode(mode), -1);
            scene->AddRoom(roomKey, mode, NetLaunch::RoomPlayerCount);
            scene->OnLoad();

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
            _scene->OnSimulationFrame();
        }
        catch (const std::exception& ex)
        {
            IncrementInPlace(_stepFailures);
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
        IncrementInPlace(_frames);
        _stepSeconds += elapsed;
        if (elapsed > _worstStepSeconds)
        {
            _worstStepSeconds = elapsed;
        }
        if (elapsed > 1.0 / 60.0)
        {
            IncrementInPlace(_overrunSteps);
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
        Read::ClearCache();
        NativeRuntime::ForceFullGc();
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

        std::string result = Room() + " (" + ::MphRead::ToString(GameState::Mode())
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
