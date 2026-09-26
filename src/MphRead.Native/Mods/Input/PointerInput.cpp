#include "PointerInput.hpp"

#include "../DebugLog.hpp"
#include "../../NativeRuntime/System/Number.hpp"

namespace MphRead::Mods::Input
{
    std::pair<float, float> PointerInput::Filter(float x, float y)
    {
        if (!_stylusMode || !_guardJumps || _jumpPixels <= 0
            || x * x + y * y < _jumpPixels * _jumpPixels)
        {
            return {x, y};
        }
        _jumpsIgnored++;
        if (!_jumpingPointerSeen)
        {
            _jumpingPointerSeen = true;
            DebugLog::Line("input", "pointer jumped (" + ::MphRead::NativeRuntime::ToString(x, "0") + ", "
                + ::MphRead::NativeRuntime::ToString(y, "0") + ") px in a frame; sample ignored");
        }
        return {0.0F, 0.0F};
    }

    void PointerInput::Reset() noexcept
    {
        _jumpsIgnored = 0;
        _jumpingPointerSeen = false;
    }
}
