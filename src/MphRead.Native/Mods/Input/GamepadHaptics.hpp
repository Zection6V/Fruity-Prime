#pragma once

#include <chrono>
#include <cstdint>
#include <map>
#include <memory>
#include <mutex>
#include <string>

namespace MphRead::Mods::Input
{
    class HapticScheduler;

    class IGamepadHaptics
    {
    public:
        virtual ~IGamepadHaptics() = default;
        virtual void Rumble(float lowFrequency, float highFrequency, std::chrono::milliseconds duration) = 0;
        virtual void Stop() = 0;
    };

    enum class GamepadFeedback : std::int32_t { Fire, ChargedShot, Damage, Explosion, Boost, Landing, Death };

    class GamepadHaptics final
    {
    public:
        GamepadHaptics() = delete;

        static void Register(const std::string& id, std::shared_ptr<IGamepadHaptics> backend);
        [[nodiscard]] static bool Available(const std::string& id);
        static void Unregister(const std::string& id);
        static void Stop();
        static void Play(GamepadFeedback feedback);

    private:
        // The static constructor: runs before the first member is used.
        static void EnsureInitialized();
    };
}
