#pragma once

// System.Numerics.Vector2 and Vector3: value types with component-wise
// arithmetic, distinct from OpenTK's, as the C# keeps them distinct.

#include <algorithm>
#include <cmath>

namespace System::Numerics
{
    struct Vector2 final
    {
        float X = 0.0F;
        float Y = 0.0F;

        constexpr Vector2() noexcept = default;
        constexpr Vector2(float x, float y) noexcept : X(x), Y(y) {}
        // new Vector2(value): both components.
        constexpr explicit Vector2(float value) noexcept : X(value), Y(value) {}

        [[nodiscard]] static constexpr Vector2 Zero() noexcept { return {}; }

        [[nodiscard]] float Length() const noexcept { return std::sqrt(X * X + Y * Y); }

        [[nodiscard]] static constexpr Vector2 Lerp(Vector2 from, Vector2 to, float amount) noexcept
        {
            return {from.X + (to.X - from.X) * amount, from.Y + (to.Y - from.Y) * amount};
        }

        // Vector2.Clamp: Min(Max(value, min), max), per component.
        [[nodiscard]] static constexpr Vector2 Clamp(Vector2 value, Vector2 min, Vector2 max) noexcept
        {
            return {std::min(std::max(value.X, min.X), max.X), std::min(std::max(value.Y, min.Y), max.Y)};
        }

        friend constexpr Vector2 operator+(Vector2 left, Vector2 right) noexcept { return {left.X + right.X, left.Y + right.Y}; }
        friend constexpr Vector2 operator-(Vector2 left, Vector2 right) noexcept { return {left.X - right.X, left.Y - right.Y}; }
        friend constexpr Vector2 operator*(Vector2 left, float right) noexcept { return {left.X * right, left.Y * right}; }
        friend constexpr Vector2 operator*(float left, Vector2 right) noexcept { return right * left; }
        friend constexpr Vector2 operator/(Vector2 left, float right) noexcept { return {left.X / right, left.Y / right}; }
        friend constexpr bool operator==(Vector2 left, Vector2 right) noexcept = default;
    };

    struct Vector3 final
    {
        float X = 0.0F;
        float Y = 0.0F;
        float Z = 0.0F;

        constexpr Vector3() noexcept = default;
        constexpr Vector3(float x, float y, float z) noexcept : X(x), Y(y), Z(z) {}

        [[nodiscard]] float Length() const noexcept { return std::sqrt(X * X + Y * Y + Z * Z); }

        friend constexpr bool operator==(Vector3 left, Vector3 right) noexcept = default;
    };
}
