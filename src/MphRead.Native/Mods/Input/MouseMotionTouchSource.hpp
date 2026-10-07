#pragma once

#include <cstdint>

namespace MphRead::Mods::Input
{
    struct NativeTouchState;

    // A DS touch contact synthesised from the mouse's relative motion -- not
    // the Stylus pointer mode, and not mouse aim itself. Host pixels become DS
    // units at a fixed gain (an input-side setting, never a gameplay
    // threshold), with the fraction carried so slow motion is not lost.
    //
    // Motion is collected every simulation step and handed to the producer
    // once a native touch tick (NativeTouchClock), so a tick's delta is all
    // the motion since the last one. The contact is "down" while the mouse
    // moves and for a few idle ticks after, so a polling gap does not lift
    // it; once it rests it lifts, which is what re-arms Touch Boost.
    //
    // None of these numbers are the ROM's; they are how a mouse is mapped
    // onto a stylus, and nothing in the gameplay rules reads them.
    class MouseMotionTouchSource final
    {
    public:
        static constexpr float DsUnitsPerPixel = 0.25F;
        // Native ticks: two are 66.7 ms.
        static constexpr std::int32_t IdleTicksBeforeLift = 2;

        // Every simulation step: this step's motion in host pixels.
        void AddMotion(float pixelDx, float pixelDy) noexcept;
        // Every native touch tick: writes one producer tick into touch.
        void Tick(NativeTouchState& touch) noexcept;
        void Reset() noexcept;

    private:
        float _pendingX = 0;
        float _pendingY = 0;
        float _carryX = 0;
        float _carryY = 0;
        std::int32_t _idle = IdleTicksBeforeLift;
    };
}
