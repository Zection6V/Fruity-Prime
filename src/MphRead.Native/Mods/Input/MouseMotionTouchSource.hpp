#pragma once

#include <cstdint>

namespace MphRead::Mods::Input
{
    struct NativeTouchState;

    // A DS touch contact synthesised from the mouse's relative motion -- not
    // the Stylus pointer mode, and not mouse aim itself. Host pixels become DS
    // units at a fixed gain (an input-side setting, never a gameplay
    // threshold), with the fraction carried so slow motion is not lost. The
    // contact is "down" while the mouse moves and for a few idle steps after,
    // so a polling gap does not lift it; once it rests it lifts, which is what
    // re-arms Touch Boost.
    class MouseMotionTouchSource final
    {
    public:
        static constexpr float DsUnitsPerPixel = 0.25F;
        static constexpr std::int32_t IdleStepsBeforeLift = 4;

        // One simulation step: writes one producer tick into touch.
        void Step(float pixelDx, float pixelDy, NativeTouchState& touch) noexcept;
        void Reset() noexcept;

    private:
        float _carryX = 0;
        float _carryY = 0;
        std::int32_t _idle = IdleStepsBeforeLift;
    };
}
