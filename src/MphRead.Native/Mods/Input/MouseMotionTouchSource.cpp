#include "MouseMotionTouchSource.hpp"

#include "NativeTouchState.hpp"

#include <cmath>

namespace MphRead::Mods::Input
{
    void MouseMotionTouchSource::Reset() noexcept
    {
        _carryX = _carryY = 0;
        _idle = IdleStepsBeforeLift;
    }

    void MouseMotionTouchSource::Step(float pixelDx, float pixelDy, NativeTouchState& touch) noexcept
    {
        if (!std::isfinite(pixelDx) || !std::isfinite(pixelDy))
        {
            pixelDx = pixelDy = 0;
        }
        if (pixelDx != 0 || pixelDy != 0)
        {
            _idle = 0;
        }
        else if (_idle < IdleStepsBeforeLift)
        {
            ++_idle;
        }
        const bool down = _idle < IdleStepsBeforeLift;
        if (!down)
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
