#pragma once

#include <cstdint>

namespace MphRead::Mods::Input
{
    // The DS touchscreen as a discrete 256x192 surface, and the conversion of
    // a host position on some rectangle of the window onto it.
    namespace DsTouchSurface
    {
        inline constexpr std::int32_t MaxX = 255;
        inline constexpr std::int32_t MaxY = 191;
        inline constexpr std::uint8_t CenterX = 128;
        inline constexpr std::uint8_t CenterY = 96;

        // normalized is the host position as a fraction of the window; origin
        // and extent are the rectangle that stands for the DS screen, in the
        // same units. Rounded, clamped to the surface; a degenerate rectangle
        // or a non-finite position reads as 0.
        [[nodiscard]] std::int32_t ToX(float normalized, float origin, float extent) noexcept;
        [[nodiscard]] std::int32_t ToY(float normalized, float origin, float extent) noexcept;
    }
}
