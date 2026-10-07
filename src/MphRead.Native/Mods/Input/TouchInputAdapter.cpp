#include "TouchInputAdapter.hpp"

#include "DsTouchSurface.hpp"
#include "HostTouch.hpp"
#include "PointerDevice.hpp"
#include "StylusZone.hpp"

namespace MphRead::Mods::Input
{
    bool TouchInputAdapter::StylusZoneContact() noexcept
    {
        // Only the aim surface is the DS touchscreen: a button, the weapon
        // wheel or the zone being placed never raise a contact.
        return StylusZone::Enabled() && !StylusZone::Placing() && StylusZone::Contact()
            && StylusZone::Held() == StylusRegion::Aim;
    }

    void TouchInputAdapter::TickStylusZone() noexcept
    {
        if (!StylusZoneContact())
        {
            _state.Update(false, 0, 0);
            return;
        }
        const PointerSample& sample = PointerDevice::Current();
        _state.Update(true,
            DsTouchSurface::ToX(sample.X / PointerDevice::SurfaceWidth(), StylusZone::Left(), StylusZone::Width()),
            DsTouchSurface::ToY(sample.Y / PointerDevice::SurfaceHeight(), StylusZone::Top(), StylusZone::Height()));
    }

    void TouchInputAdapter::Step(const Frame& frame) noexcept
    {
        const bool tick = _clock.Advance();
        if (HostTouch::Published())
        {
            if (tick)
            {
                _state.Update(HostTouch::Down(), HostTouch::X(), HostTouch::Y());
            }
        }
        else if (frame.PointerActive)
        {
            if (tick)
            {
                TickStylusZone();
            }
        }
        else if (frame.MouseAim)
        {
            // Motion between ticks is not lost: it is collected every step.
            _mouseMotion.AddMotion(frame.MouseDeltaX, frame.MouseDeltaY);
            if (tick)
            {
                _mouseMotion.Tick(_state);
            }
        }
        else if (tick)
        {
            _state.Update(false, 0, 0);
        }
    }

    void TouchInputAdapter::Suspend() noexcept
    {
        _state.Clear();
        _clock.Reset();
        _mouseMotion.Reset();
    }

    void TouchInputAdapter::ApplyReported(const NativeTouchState::Reported& reported) noexcept
    {
        _state.Assign(reported);
    }
}
