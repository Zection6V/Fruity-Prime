#pragma once

#include "GamepadState.hpp"

#include <cstdint>
#include <vector>

namespace MphRead::Mods::Input
{
    class GamepadDesktop final
    {
    public:
        GamepadDesktop() = delete;
        GamepadDesktop(const GamepadDesktop&) = delete;
        GamepadDesktop& operator=(const GamepadDesktop&) = delete;

        static void PollForMenu();
        static void Poll();

    private:
        static void PollUnsafe();
        [[nodiscard]] static bool TryRead(std::int32_t slot);
        [[nodiscard]] static bool TryReadRaw(std::int32_t slot);

        [[nodiscard]] static float Axis(
            const std::vector<float>& axes, std::int32_t index) noexcept;
        [[nodiscard]] static float Trigger(
            const std::vector<float>& axes, std::int32_t index, float& floor) noexcept;
        static void AddRaw(GamepadButtons& into,
            const std::vector<std::uint8_t>& buttons,
            std::int32_t index, GamepadButtons flag) noexcept;
        static void AddHat(GamepadButtons& into,
            std::uint8_t hat, std::uint8_t match, GamepadButtons flag) noexcept;
        static void Add(GamepadButtons& into,
            const std::uint8_t* buttons, std::int32_t index,
            GamepadButtons flag) noexcept;

        static constexpr std::int32_t RescanFrames = 60;
        static constexpr float TriggerPress = 0.65F;

        static std::int32_t _slot;
        static std::int32_t _rescanCountdown;
        static bool _rawSlot;
        static bool _initialised;
        static std::int32_t _floorSlot;
        static float _leftFloor;
        static float _rightFloor;
    };
}
