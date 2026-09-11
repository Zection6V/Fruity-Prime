#pragma once

#include <cstdint>

namespace OpenTK::Mathematics
{
    // Transitional declaration surface for the OpenTK 4.9.4 Vector3 dependency.
    // The operations are declared here, not reimplemented by PreviewCamera.
    struct Vector3
    {
        float X;
        float Y;
        float Z;

        static const Vector3 UnitX;
        static const Vector3 UnitY;
        static const Vector3 UnitZ;

        [[nodiscard]] float LengthSquared() const;
        [[nodiscard]] Vector3 Normalized() const;
        [[nodiscard]] static Vector3 Cross(Vector3 left, Vector3 right);
    };

    [[nodiscard]] Vector3 operator-(Vector3 left, Vector3 right);
    [[nodiscard]] Vector3 operator-(Vector3 value);
}

namespace MphRead
{
    enum class CameraMode : std::int32_t
    {
        Pivot = 0,
        Roam = 1,
        Player = 2
    };

    // Transitional declaration surface for the not-yet-ported native Scene partial.
    // It contains only the members PreviewCamera.cs consumes.
    class Scene
    {
    public:
        void SetPreviewCamera(OpenTK::Mathematics::Vector3 position,
            OpenTK::Mathematics::Vector3 target);

    private:
        enum class InputMode : std::int32_t
        {
            All = 0,
            PlayerOnly = 1,
            CameraOnly = 2
        };

        CameraMode _cameraMode;
        InputMode _inputMode;
        OpenTK::Mathematics::Vector3 _cameraPosition;
        OpenTK::Mathematics::Vector3 _cameraFacing;
        OpenTK::Mathematics::Vector3 _cameraUp;
        OpenTK::Mathematics::Vector3 _cameraRight;
    };
}
