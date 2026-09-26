#include "InputSourceTracker.hpp"

#include "../../NativeRuntime/System/Runtime.hpp"

namespace MphRead::Mods::Input
{
    void InputSourceTracker::Reset() noexcept
    {
        _current = InputSource::KeyboardMouse;
        _changed = 0;
    }

    void InputSourceTracker::Note(InputSource source)
    {
        Note(source, ::MphRead::NativeRuntime::EnvironmentTickCount64());
    }

    void InputSourceTracker::Note(InputSource source, std::int64_t milliseconds) noexcept
    {
        if (source == _current || milliseconds - _changed < 180)
        {
            return;
        }
        _current = source;
        _changed = milliseconds;
    }
}
