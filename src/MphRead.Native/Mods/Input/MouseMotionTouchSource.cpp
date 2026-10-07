#include "MouseMotionTouchSource.hpp"

#include "NativeTouchState.hpp"

#include <cmath>

namespace MphRead::Mods::Input
{
    void MouseMotionTouchSource::Reset() noexcept
    {
        _pendingX = _pendingY = 0;
        _carryX = _carryY = 0;
        _idle = IdleTicksBeforeLift;
    }

    void MouseMotionTouchSource::AddMotion(float pixelDx, float pixelDy) noexcept
    {
        if (std::isfinite(pixelDx) && std::isfinite(pixelDy))
        {
            _pendingX += pixelDx;
            _pendingY += pixelDy;
        }
    }

    void MouseMotionTouchSource::Tick(NativeTouchState& touch) noexcept
    {
        const float pixelDx = _pendingX;
        const float pixelDy = _pendingY;
        _pendingX = _pendingY = 0;
        if (pixelDx != 0 || pixelDy != 0)
        {
            _idle = 0;
        }
        else if (_idle < IdleTicksBeforeLift)
        {
            ++_idle;
        }
        if (_idle >= IdleTicksBeforeLift)
        {
            _carryX = _carryY = 0;
            touch.UpdateRelative(false, 0, 0);
            return;
        }
        _carryX += pixelDx * DsUnitsPerPixel;
        _carryY += pixelDy * DsUnitsPerPixel;
        const auto dx = static_cast<std::int32_t>(std::trunc(_carryX));
        const auto dy = static_cast<std::int32_t>(std::trunc(_carryY));
        _carryX -= static_cast<float>(dx);
        _carryY -= static_cast<float>(dy);
        touch.UpdateRelative(true, dx, dy);
    }
}
