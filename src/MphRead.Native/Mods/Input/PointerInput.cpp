#include "PointerInput.hpp"

#include <bit>
#include <cmath>
#include <cstdint>
#include <string_view>

// DebugLog has not been ported yet. Keep its only dependency in this translation
// unit; there is deliberately no fallback logger or substitute behavior. The
// bridge must apply valueFormat with the calling thread's current .NET culture
// before calling DebugLog.Line so C# interpolation semantics are preserved.
namespace MphRead::Mods::Input::PointerInputAdapters
{
    void DebugLogLine(
        std::string_view category,
        std::string_view messagePrefix,
        float value,
        std::string_view valueFormat,
        std::string_view messageSuffix);
}

namespace MphRead::Mods::Input
{
    float PointerInput::_jumpPixels = 600.0F;
    bool PointerInput::_guardJumps = true;
    std::int32_t PointerInput::_jumpsIgnored = 0;
    bool PointerInput::_jumpingPointerSeen = false;

    float PointerInput::JumpPixels() noexcept
    {
        return _jumpPixels;
    }

    void PointerInput::JumpPixels(float value) noexcept
    {
        _jumpPixels = value;
    }

    bool PointerInput::GuardJumps() noexcept
    {
        return _guardJumps;
    }

    void PointerInput::GuardJumps(bool value) noexcept
    {
        _guardJumps = value;
    }

    std::int32_t PointerInput::JumpsIgnored() noexcept
    {
        return _jumpsIgnored;
    }

    bool PointerInput::JumpingPointerSeen() noexcept
    {
        return _jumpingPointerSeen;
    }

    float PointerInput::Filter(float delta)
    {
        if (!GuardJumps() || JumpPixels() <= 0.0F)
        {
            return delta;
        }
        if (std::fabs(delta) < JumpPixels())
        {
            return delta;
        }

        const std::uint32_t incremented = static_cast<std::uint32_t>(_jumpsIgnored) + 1U;
        _jumpsIgnored = std::bit_cast<std::int32_t>(incremented);
        if (!JumpingPointerSeen())
        {
            _jumpingPointerSeen = true;
            PointerInputAdapters::DebugLogLine(
                "input",
                "pointer jumped ",
                delta,
                "0",
                " px in a frame and was ignored -- a pen, a touchscreen, or a cursor warp");
        }
        return 0.0F;
    }

    void PointerInput::Reset() noexcept
    {
        _jumpsIgnored = 0;
        _jumpingPointerSeen = false;
    }
}
