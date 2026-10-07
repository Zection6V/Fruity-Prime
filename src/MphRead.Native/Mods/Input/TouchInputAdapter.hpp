#pragma once

#include "MouseMotionTouchSource.hpp"
#include "NativeTouchState.hpp"

namespace MphRead::Mods::Input
{
    // Decides, once per simulation step, which host input is the DS stylus
    // and feeds exactly one producer tick from it. In priority order:
    //   1. a contact another head published (HostTouch -- Android's finger)
    //   2. a pen/touch contact held on the stylus zone's aim surface
    //   3. the mouse's relative motion, when the mouse aims (MouseMotionTouchSource)
    // and otherwise no contact. Nothing past this class knows which it was.
    class TouchInputAdapter final
    {
    public:
        struct Frame
        {
            // PointerDevice owns the pointer this step (pen/touch mode).
            bool PointerActive = false;
            // The mouse is the aim device.
            bool MouseAim = false;
            // This step's filtered relative mouse motion, in host pixels.
            float MouseDeltaX = 0;
            float MouseDeltaY = 0;
        };

        void Step(const Frame& frame) noexcept;
        // Input taken away (a menu, chat): the stylus lifts and starts over.
        void Suspend() noexcept;

        [[nodiscard]] const NativeTouchState& State() const noexcept { return _state; }

    private:
        [[nodiscard]] static bool StylusZoneContact() noexcept;
        void StepStylusZone() noexcept;

        NativeTouchState _state{};
        MouseMotionTouchSource _mouseMotion{};
    };
}
