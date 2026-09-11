#pragma once

#include <cstdint>
#include <new>

namespace MphRead::Mods::Input
{
    struct GamepadLayout final
    {
        const std::int32_t AxisLeftX = 0;
        const std::int32_t AxisLeftY = 0;
        const std::int32_t AxisRightX = 0;
        const std::int32_t AxisRightY = 0;
        const std::int32_t AxisLeftTrigger = 0;
        const std::int32_t AxisRightTrigger = 0;
        const std::int32_t ButtonA = 0;
        const std::int32_t ButtonB = 0;
        const std::int32_t ButtonX = 0;
        const std::int32_t ButtonY = 0;
        const std::int32_t ButtonLeftBumper = 0;
        const std::int32_t ButtonRightBumper = 0;
        const std::int32_t ButtonLeftTrigger = 0;
        const std::int32_t ButtonRightTrigger = 0;
        const std::int32_t ButtonBack = 0;
        const std::int32_t ButtonStart = 0;
        const std::int32_t ButtonLeftThumb = 0;
        const std::int32_t ButtonRightThumb = 0;

        constexpr GamepadLayout() noexcept = default;
        GamepadLayout(const GamepadLayout&) noexcept = default;

        GamepadLayout& operator=(const GamepadLayout& other) noexcept
        {
            if (this != &other)
            {
                this->~GamepadLayout();
                ::new (static_cast<void*>(this)) GamepadLayout(other);
            }
            return *this;
        }

        static GamepadLayout For(std::int32_t slot);

    private:
        constexpr GamepadLayout(
            std::int32_t axisLeftX,
            std::int32_t axisLeftY,
            std::int32_t axisRightX,
            std::int32_t axisRightY,
            std::int32_t axisLeftTrigger,
            std::int32_t axisRightTrigger,
            std::int32_t buttonA,
            std::int32_t buttonB,
            std::int32_t buttonX,
            std::int32_t buttonY,
            std::int32_t buttonLeftBumper,
            std::int32_t buttonRightBumper,
            std::int32_t buttonLeftTrigger,
            std::int32_t buttonRightTrigger,
            std::int32_t buttonBack,
            std::int32_t buttonStart,
            std::int32_t buttonLeftThumb,
            std::int32_t buttonRightThumb) noexcept
            : AxisLeftX(axisLeftX),
              AxisLeftY(axisLeftY),
              AxisRightX(axisRightX),
              AxisRightY(axisRightY),
              AxisLeftTrigger(axisLeftTrigger),
              AxisRightTrigger(axisRightTrigger),
              ButtonA(buttonA),
              ButtonB(buttonB),
              ButtonX(buttonX),
              ButtonY(buttonY),
              ButtonLeftBumper(buttonLeftBumper),
              ButtonRightBumper(buttonRightBumper),
              ButtonLeftTrigger(buttonLeftTrigger),
              ButtonRightTrigger(buttonRightTrigger),
              ButtonBack(buttonBack),
              ButtonStart(buttonStart),
              ButtonLeftThumb(buttonLeftThumb),
              ButtonRightThumb(buttonRightThumb)
        {
        }

        static const GamepadLayout Triggers;
        static const GamepadLayout Buttons;
    };
}
