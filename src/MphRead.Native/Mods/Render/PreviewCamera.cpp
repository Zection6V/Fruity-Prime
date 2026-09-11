#include "PreviewCamera.hpp"

namespace MphRead
{
    void Scene::SetPreviewCamera(OpenTK::Mathematics::Vector3 position,
        OpenTK::Mathematics::Vector3 target)
    {
        using OpenTK::Mathematics::Vector3;

        _cameraMode = CameraMode::Roam;
        _inputMode = InputMode::CameraOnly;
        _cameraPosition = position;
        Vector3 facing = target - position;
        _cameraFacing = facing.LengthSquared() < 0.0001f ? -Vector3::UnitZ : facing.Normalized();
        _cameraRight = Vector3::Cross(_cameraFacing, Vector3::UnitY);
        _cameraRight = _cameraRight.LengthSquared() < 0.0001f
            ? Vector3::UnitX
            : _cameraRight.Normalized();
        _cameraUp = Vector3::Cross(_cameraRight, _cameraFacing).Normalized();
    }
}
