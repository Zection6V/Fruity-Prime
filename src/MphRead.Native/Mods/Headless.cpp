#include "Headless.hpp"

namespace MphRead::Mods
{
    bool Headless::_active = false;

    bool Headless::Active() noexcept
    {
        return _active;
    }

    void Headless::Enter() noexcept
    {
        _active = true;
    }
}
