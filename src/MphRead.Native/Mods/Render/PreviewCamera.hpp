#pragma once

#include <cstdint>

namespace OpenTK::Mathematics
{
    struct Vector3
    {
        float X = 0.0f;
        float Y = 0.0f;
        float Z = 0.0f;

        constexpr Vector3() noexcept = default;
        constexpr Vector3(float x, float y, float z) noexcept
            : X(x), Y(y), Z(z)
        {
        }
    };
}

namespace MphRead
{
    enum class CameraMode : std::int32_t
    {
        Pivot,
        Roam,
        Player
    };

    class Scene
    {
    public:
        void SetPreviewCamera(OpenTK::Mathematics::Vector3 position,
            OpenTK::Mathematics::Vector3 target);

    private:
        enum class InputMode : std::int32_t
        {
            All,
            PlayerOnly,
            CameraOnly
        };

        Scene() = default;

        CameraMode _cameraMode = CameraMode::Pivot;
        InputMode _inputMode = InputMode::All;
        OpenTK::Mathematics::Vector3 _cameraPosition{};
        OpenTK::Mathematics::Vector3 _cameraFacing{-0.0f, -0.0f, -1.0f};
        OpenTK::Mathematics::Vector3 _cameraUp{0.0f, 1.0f, 0.0f};
        OpenTK::Mathematics::Vector3 _cameraRight{1.0f, 0.0f, 0.0f};
    };
}
