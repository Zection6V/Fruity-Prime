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

    void TouchInputAdapter::StepStylusZone() noexcept
    {
        const PointerSample& sample = PointerDevice::Current();
        _state.Update(true,
            DsTouchSurface::ToX(sample.X / PointerDevice::SurfaceWidth(), StylusZone::Left(), StylusZone::Width()),
            DsTouchSurface::ToY(sample.Y / PointerDevice::SurfaceHeight(), StylusZone::Top(), StylusZone::Height()));
    }

    void TouchInputAdapter::Step(const Frame& frame) noexcept
    {
        if (HostTouch::Published())
        {
            _state.Update(HostTouch::Down(), HostTouch::X(), HostTouch::Y());
        }
        else if (frame.PointerActive)
        {
            if (StylusZoneContact())
            {
                StepStylusZone();
            }
            else
            {
                _state.Update(false, 0, 0);
            }
        }
        else if (frame.MouseAim)
        {
            _mouseMotion.Step(frame.MouseDeltaX, frame.MouseDeltaY, _state);
        }
        else
        {
            _state.Update(false, 0, 0);
        }
    }

    void TouchInputAdapter::Suspend() noexcept
    {
        _state.Clear();
        _mouseMotion.Reset();
    }
}
