#pragma once

#include <cstdint>

namespace MphRead::Mods::Gameplay
{
    // One scene-wide phase. Input ownership and per-entity resets cannot move it.
    class NativeGameplayClock final
    {
    public:
        NativeGameplayClock() = delete;
        [[nodiscard]] static constexpr bool IsNativeTick(std::uint64_t frame) noexcept
        {
            return frame != 0 && (frame & 1U) == 0;
        }
    };
}
