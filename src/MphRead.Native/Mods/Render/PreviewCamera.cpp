#include "PreviewCamera.hpp"

#include <cmath>

namespace
{
    using OpenTK::Mathematics::Vector3;

    constexpr Vector3 UnitX{1.0f, 0.0f, 0.0f};
    constexpr Vector3 UnitY{0.0f, 1.0f, 0.0f};
    constexpr Vector3 NegativeUnitZ{-0.0f, -0.0f, -1.0f};

    [[nodiscard]] constexpr Vector3 Subtract(Vector3 left, Vector3 right) noexcept
    {
        return Vector3{left.X - right.X, left.Y - right.Y, left.Z - right.Z};
    }

    [[nodiscard]] constexpr float LengthSquared(Vector3 value) noexcept
    {
        return value.X * value.X + value.Y * value.Y + value.Z * value.Z;
    }

    [[nodiscard]] Vector3 Normalized(Vector3 value) noexcept
    {
        const float scale = 1.0f / std::sqrt(LengthSquared(value));
        return Vector3{value.X * scale, value.Y * scale, value.Z * scale};
    }

    [[nodiscard]] constexpr Vector3 Cross(Vector3 left, Vector3 right) noexcept
    {
        return Vector3{
            left.Y * right.Z - left.Z * right.Y,
            left.Z * right.X - left.X * right.Z,
            left.X * right.Y - left.Y * right.X
        };
    }
}

namespace MphRead
{
    void Scene::SetPreviewCamera(OpenTK::Mathematics::Vector3 position,
        OpenTK::Mathematics::Vector3 target)
    {
        _cameraMode = CameraMode::Roam;
        _inputMode = InputMode::CameraOnly;
        _cameraPosition = position;
        const OpenTK::Mathematics::Vector3 facing = Subtract(target, position);
        _cameraFacing = LengthSquared(facing) < 0.0001f ? NegativeUnitZ : Normalized(facing);
        _cameraRight = Cross(_cameraFacing, UnitY);
        _cameraRight = LengthSquared(_cameraRight) < 0.0001f ? UnitX : Normalized(_cameraRight);
        _cameraUp = Normalized(Cross(_cameraRight, _cameraFacing));
    }
}
