#include "WeaponWheel.hpp"

#include "StylusZone.hpp"
#include "../../NativeRuntime/System/Runtime.hpp"

namespace MphRead::Mods::Input
{
    bool WeaponWheel::Absolute()
    {
        return StylusZone::Enabled() || ::MphRead::NativeRuntime::IsAndroid();
    }

    void WeaponWheel::Close() noexcept
    {
        _open = false;
        _travel = 0;
        _index = -1;
    }

    std::int32_t WeaponWheel::Drag(float deltaY, float step, std::span<const bool> available, std::int32_t current) noexcept
    {
        if (!_open)
        {
            _open = true;
            _travel = 0;
            _index = current;
        }
        if (step <= 0)
        {
            step = 64;
        }
        _travel += deltaY;
        while (_travel >= step)
        {
            _travel -= step;
            _index = Step(_index, 1, available);
        }
        while (_travel <= -step)
        {
            _travel += step;
            _index = Step(_index, -1, available);
        }
        return _index;
    }

    std::int32_t WeaponWheel::Step(std::int32_t from, std::int32_t direction, std::span<const bool> available) noexcept
    {
        if (from < 0)
        {
            const std::int32_t start = direction > 0 ? 0 : Slots - 1;
            for (std::int32_t i = start; i >= 0 && i < Slots; i += direction)
            {
                if (Has(available, i))
                {
                    return i;
                }
            }
            return -1;
        }
        for (std::int32_t i = from + direction; i >= 0 && i < Slots; i += direction)
        {
            if (Has(available, i))
            {
                return i;
            }
        }
        return from;
    }

    bool WeaponWheel::Has(std::span<const bool> available, std::int32_t index) noexcept
    {
        return index >= 0 && index < static_cast<std::int32_t>(available.size()) && available[static_cast<std::size_t>(index)];
    }
}
