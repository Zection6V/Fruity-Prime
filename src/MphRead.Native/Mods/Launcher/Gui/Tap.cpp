#include "Tap.hpp"

#include <cmath>

namespace MphRead::Mods::Launcher::Gui
{
    void Tap::Press(const void* pointer, GuiPoint origin, bool drags) noexcept
    {
        _pointer = pointer;
        _origin = origin;
        _drags = drags;
    }

    bool Tap::Moved(const void* pointer, GuiPoint p) noexcept
    {
        if (_pointer == nullptr || pointer != _pointer || !_drags)
        {
            return false;
        }
        const GuiVector travel = p - _origin;
        if (std::abs(travel.X) <= Slop && std::abs(travel.Y) <= Slop)
        {
            return false;
        }
        _pointer = nullptr;
        return true;
    }

    bool Tap::Release(const void* pointer, GuiPoint p, GuiSize size) noexcept
    {
        if (_pointer == nullptr || pointer != _pointer)
        {
            // Somebody else's pointer, or one that was cancelled on the way.
            return false;
        }
        _pointer = nullptr;
        // Where the release landed, not IsPointerOver: a finger hovers nothing.
        return p.X >= 0 && p.Y >= 0 && p.X <= size.Width && p.Y <= size.Height;
    }

    bool Tap::Sideways(GuiVector travel) noexcept
    {
        return std::abs(travel.X) > Slop && std::abs(travel.X) >= std::abs(travel.Y);
    }

    bool Tap::Cancel() noexcept
    {
        const bool was = _pointer != nullptr;
        _pointer = nullptr;
        return was;
    }
}
