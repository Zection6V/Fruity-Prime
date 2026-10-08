#pragma once
#include <algorithm>
#include "NativeAimControl.hpp"
#include "NativeInputSlot.hpp"

namespace MphRead::Mods::Input::NativeAim
{
    enum class Form : std::uint8_t { Biped, AltStrafe, FreeCamera };
    constexpr float Floor(float maximum, Form /*form*/, bool negative) noexcept
    {
        return maximum * (negative ? -0.4F : 0.4F);
    }
    constexpr float ProduceAxis(float old, float maximum, int direction, Form form, bool halfStep = false) noexcept
    {
        if (maximum <= 0) return 0;
        const auto step = maximum * (halfStep ? 0.005F : 0.01F);
        if (direction < 0) return std::min(std::max(old - step, -maximum), Floor(maximum, form, true));
        if (direction > 0) return std::max(std::min(old + step, maximum), Floor(maximum, form, false));
        // Two DirectPC 60 Hz substeps preserve one 30 Hz decay period.
        return old * (halfStep ? 0.6324555320336759F : 0.4F);
    }
    struct DualState final
    {
        float X = 0, Y = 0;
        // Called AFTER old X/Y consumption for Biped/Alt, BEFORE consumption
        // for FreeCamera. Consumers, including zero-input Follow, remain separate.
        constexpr bool Produce(const InputSlot& slot, const Control& control, Form form, bool halfStep = false) noexcept
        {
            if (form != Form::FreeCamera && control.Flag84E != 0) return false;
            const bool unconditional = (control.Flags & 8U) != 0;
            const bool enabled = TestAction(slot, control.EnableAction);
            const bool aim = TestAction(slot, control.AimAction);
            const bool horizontal = form == Form::FreeCamera || unconditional || (!((control.Flags & 4U) && !enabled)
                && !((control.Flags & 2U) && (slot.Bytes[0x34] & 1U)) && !aim);
            const bool vertical = form == Form::FreeCamera || unconditional || enabled;
            X = ProduceAxis(X, control.MaxX, !horizontal ? 0 : TestAction(slot, control.Right) ? -1 : TestAction(slot, control.Left) ? 1 : 0, form, halfStep);
            const bool up = vertical && TestAction(slot, control.Up);
            const bool down = vertical && TestAction(slot, control.Down);
            Y = ProduceAxis(Y, control.MaxY, up ? 1 : down ? -1 : 0, form, halfStep);
            return form != Form::FreeCamera && vertical && !up && !down && !(control.Flags & 0x40U) && !aim
                && control.AutoPitchTimer <= control.AutoPitchLimit;
        }
    };
}
