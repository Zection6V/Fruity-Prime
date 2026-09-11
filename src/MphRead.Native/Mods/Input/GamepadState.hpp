#pragma once

#include <cstdint>
#include <optional>
#include <string>

namespace MphRead::Mods::Input
{
    enum class GamepadButtons : std::int32_t
    {
        None = 0,
        A = 1 << 0,
        B = 1 << 1,
        X = 1 << 2,
        Y = 1 << 3,
        LeftBumper = 1 << 4,
        RightBumper = 1 << 5,
        Back = 1 << 6,
        Start = 1 << 7,
        LeftThumb = 1 << 8,
        RightThumb = 1 << 9,
        DpadUp = 1 << 10,
        DpadRight = 1 << 11,
        DpadDown = 1 << 12,
        DpadLeft = 1 << 13,
        LeftTrigger = 1 << 14,
        RightTrigger = 1 << 15
    };

    struct GamepadState
    {
        bool Connected = false;
        float LeftX = 0.0F;
        float LeftY = 0.0F;
        float RightX = 0.0F;
        float RightY = 0.0F;
        float LeftTrigger = 0.0F;
        float RightTrigger = 0.0F;
        GamepadButtons Buttons = GamepadButtons::None;
        std::optional<std::string> Name{};

        [[nodiscard]] bool Down(GamepadButtons button) const noexcept;
    };
}
