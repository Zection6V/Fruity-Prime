#pragma once

#include "GamepadState.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace MphRead::Mods::Input
{
    enum class GamepadCapabilities : std::int32_t;

    // Which axis and button a pad GLFW has no mapping for puts each control
    // on: a guess by axis count, and one known layout that GLFW gets wrong.
    struct GamepadLayout final
    {
        std::int32_t AxisLeftX = 0;
        std::int32_t AxisLeftY = 0;
        std::int32_t AxisRightX = 0;
        std::int32_t AxisRightY = 0;
        std::int32_t AxisLeftTrigger = 0;
        std::int32_t AxisRightTrigger = 0;
        std::int32_t ButtonA = 0;
        std::int32_t ButtonB = 0;
        std::int32_t ButtonX = 0;
        std::int32_t ButtonY = 0;
        std::int32_t ButtonLeftBumper = 0;
        std::int32_t ButtonRightBumper = 0;
        std::int32_t ButtonLeftTrigger = 0;
        std::int32_t ButtonRightTrigger = 0;
        std::int32_t ButtonBack = 0;
        std::int32_t ButtonStart = 0;
        std::int32_t ButtonLeftThumb = 0;
        std::int32_t ButtonRightThumb = 0;

        [[nodiscard]] GamepadCapabilities Capabilities(std::int32_t axes) const;
        [[nodiscard]] static bool IsMacXboxBluetooth(const std::string& guid, std::int32_t axes,
            std::int32_t buttons, std::int32_t hats, bool macOS);
        [[nodiscard]] static GamepadLayout Select(const std::string& guid, std::int32_t axes,
            std::int32_t buttons, std::int32_t hats, bool macOS);
        [[nodiscard]] static GamepadLayout For(std::int32_t slot);
        [[nodiscard]] GamepadState Read(const std::vector<float>& axes, const std::vector<std::uint8_t>& buttons,
            const std::vector<std::uint8_t>& hats, float& leftFloor, float& rightFloor) const;

        static const GamepadLayout Triggers;
        static const GamepadLayout Buttons;
        static const GamepadLayout MacXboxBluetooth;

    private:
        [[nodiscard]] static float Axis(const std::vector<float>& axes, std::int32_t index);
        [[nodiscard]] static float Trigger(const std::vector<float>& axes, std::int32_t index, float& floor);
        static void Add(GamepadButtons& flags, const std::vector<std::uint8_t>& buttons, std::int32_t index,
            GamepadButtons flag);
    };
}
