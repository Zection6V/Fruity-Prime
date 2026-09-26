#pragma once

#include "GamepadHaptics.hpp"

#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>

namespace MphRead::Mods::Input
{
    // XInput rumble for the one Xbox pad that can be told apart from any
    // others: with two, there is no knowing which XInput index is which slot.
    class WindowsGamepadHaptics final : public IGamepadHaptics
    {
    public:
        ~WindowsGamepadHaptics() override;

        static void Synchronize(const std::optional<std::string>& uniqueId);
        void Rumble(float lowFrequency, float highFrequency, std::chrono::milliseconds duration) override;
        void Stop() override;

    private:
        explicit WindowsGamepadHaptics(std::uint32_t index);
        void Dispose();
        void TimerLoop();
        void SetVibration(std::uint16_t low, std::uint16_t high);

        inline static std::optional<std::string> _id{};
        inline static std::shared_ptr<WindowsGamepadHaptics> _current{};

        const std::uint32_t _index;
        bool _disposed = false;
        std::recursive_mutex _gate;
        // System.Threading.Timer: one pending due time, or none.
        std::mutex _timerLock;
        std::condition_variable _timerSignal;
        std::optional<std::chrono::steady_clock::time_point> _due{};
        bool _timerExit = false;
        std::thread _timer;
    };
}
