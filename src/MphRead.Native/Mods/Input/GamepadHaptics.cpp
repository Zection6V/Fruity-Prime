#include "GamepadHaptics.hpp"

#include "GamepadAnalog.hpp"
#include "GamepadManager.hpp"
#include "GamepadOptions.hpp"
#include "GamepadUiRouter.hpp"
#include "HapticScheduler.hpp"
#include "InputSourceTracker.hpp"
#include "../../NativeRuntime/System/AppDomain.hpp"
#include "../../NativeRuntime/System/Runtime.hpp"

#include <tuple>

namespace MphRead::Mods::Input
{
    namespace
    {
        struct HapticsState final
        {
            std::map<std::string, std::shared_ptr<IGamepadHaptics>> Backends;
            std::recursive_mutex Gate;
            HapticScheduler Scheduler;
        };

        HapticsState& State()
        {
            static HapticsState& state = *new HapticsState();
            return state;
        }
    }

    void GamepadHaptics::EnsureInitialized()
    {
        static const bool initialized = []()
        {
            static_cast<void>(GamepadManager::ActiveChanged.Add([]() { Stop(); }));
            ::MphRead::NativeRuntime::AppDomainAddProcessExitHandler([]() { Stop(); });
            return true;
        }();
        static_cast<void>(initialized);
    }

    void GamepadHaptics::Register(const std::string& id, std::shared_ptr<IGamepadHaptics> backend)
    {
        EnsureInitialized();
        HapticsState& state = State();
        const std::lock_guard<std::recursive_mutex> guard(state.Gate);
        state.Backends[id] = std::move(backend);
    }

    bool GamepadHaptics::Available(const std::string& id)
    {
        EnsureInitialized();
        HapticsState& state = State();
        const std::lock_guard<std::recursive_mutex> guard(state.Gate);
        return state.Backends.contains(id);
    }

    void GamepadHaptics::Unregister(const std::string& id)
    {
        EnsureInitialized();
        HapticsState& state = State();
        const std::lock_guard<std::recursive_mutex> guard(state.Gate);
        const auto found = state.Backends.find(id);
        if (found != state.Backends.end())
        {
            const std::shared_ptr<IGamepadHaptics> backend = found->second;
            state.Backends.erase(found);
            backend->Stop();
        }
    }

    void GamepadHaptics::Stop()
    {
        EnsureInitialized();
        HapticsState& state = State();
        const std::lock_guard<std::recursive_mutex> guard(state.Gate);
        state.Scheduler.Reset();
        for (const auto& [id, backend] : state.Backends)
        {
            backend->Stop();
        }
    }

    void GamepadHaptics::Play(GamepadFeedback feedback)
    {
        EnsureInitialized();
        if (!GamepadContexts::Focused() || GamepadContexts::MenuVisible() || GamepadContexts::Capturing()
            || !GamepadOptions::Vibration() || InputSourceTracker::Current() != InputSource::Gamepad)
        {
            return;
        }
        const std::optional<std::string> id = GamepadManager::Snapshot().DeviceId;
        if (!id.has_value())
        {
            return;
        }
        const std::int64_t now = ::MphRead::NativeRuntime::EnvironmentTickCount64();
        float low = 0.38F;
        float high = 0.18F;
        std::int32_t ms = 220;
        switch (feedback)
        {
        case GamepadFeedback::Fire: std::tie(low, high, ms) = std::tuple(0.08F, 0.18F, 45); break;
        case GamepadFeedback::ChargedShot: std::tie(low, high, ms) = std::tuple(0.22F, 0.30F, 95); break;
        case GamepadFeedback::Damage: std::tie(low, high, ms) = std::tuple(0.32F, 0.15F, 100); break;
        case GamepadFeedback::Explosion: std::tie(low, high, ms) = std::tuple(0.40F, 0.20F, 140); break;
        case GamepadFeedback::Boost: std::tie(low, high, ms) = std::tuple(0.22F, 0.10F, 90); break;
        case GamepadFeedback::Landing: std::tie(low, high, ms) = std::tuple(0.16F, 0.06F, 65); break;
        default: break;
        }
        HapticsState& state = State();
        const std::lock_guard<std::recursive_mutex> guard(state.Gate);
        const auto found = state.Backends.find(*id);
        if (found != state.Backends.end())
        {
            if (!state.Scheduler.Accept(feedback, now, ms))
            {
                return;
            }
            const float scale = GamepadAnalog::Finite(GamepadOptions::VibrationStrength(), 0, 1);
            if (scale <= 0)
            {
                found->second->Stop();
                return;
            }
            found->second->Rumble(low * scale, high * scale, std::chrono::milliseconds(ms));
        }
    }
}
