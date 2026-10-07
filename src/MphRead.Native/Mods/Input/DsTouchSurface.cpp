#include "DsTouchSurface.hpp"

#include <algorithm>
#include <cmath>

namespace MphRead::Mods::Input::DsTouchSurface
{
    namespace
    {
        std::int32_t ToDs(float normalized, float origin, float extent, std::int32_t last) noexcept
        {
            if (!(extent > 0.0F))
            {
                return 0;
            }
            const float local = (normalized - origin) / extent;
            if (!std::isfinite(local))
            {
                return 0;
            }
            return std::clamp(static_cast<std::int32_t>(std::lround(local * static_cast<float>(last))), 0, last);
        }
    }

    std::int32_t ToX(float normalized, float origin, float extent) noexcept
    {
        return ToDs(normalized, origin, extent, MaxX);
    }

    std::int32_t ToY(float normalized, float origin, float extent) noexcept
    {
        return ToDs(normalized, origin, extent, MaxY);
    }
}
