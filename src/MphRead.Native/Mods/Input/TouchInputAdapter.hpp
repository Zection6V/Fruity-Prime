#pragma once

#include "MouseMotionTouchSource.hpp"
#include "NativeTouchSample.hpp"

namespace MphRead::Mods::Input
{
    // Decides which host input is the DS stylus and feeds the producer from
    // it, once a native touch tick (every other simulation step; see
    // NativeTouchClock). In priority order:
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

        // One simulation step of the local player.
        void Step(const Frame& frame) noexcept;
        // Input taken away (a menu, chat): the stylus lifts and starts over.
        void Suspend() noexcept;
        // A player driven by somebody else's intent: the state the owner's
        // producer had, as it reported it. See NetPlayerBridge.
        void ApplyReported(const NativeTouchState::Reported& reported,
            std::uint16_t generation, std::uint16_t life) noexcept;

        void BeginStep() noexcept { _sample.BeginStep(); }
        [[nodiscard]] const NativeTouchState& State() const noexcept { return _sample.State(); }
        [[nodiscard]] const NativeTouchSample& Sample() const noexcept { return _sample; }

    private:
        [[nodiscard]] static bool StylusZoneContact() noexcept;
        void TickStylusZone() noexcept;

        NativeTouchSample _sample{};
        MouseMotionTouchSource _mouseMotion{};
    };
}
