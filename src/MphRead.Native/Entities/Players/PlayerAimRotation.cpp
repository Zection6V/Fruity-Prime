#include "PlayerEntity.hpp"
#include "../../Features.hpp"
#include "../../Formats/Types.hpp"
#include "../../Mods/Input/AimNumericPolicy.hpp"
#include <algorithm>
#include <cmath>

using ::OpenTK::Mathematics::Add;
using ::OpenTK::Mathematics::Multiply;
using ::OpenTK::Mathematics::Subtract;
using ::OpenTK::Mathematics::MathHelper::DegreesToRadians;

namespace MphRead::Entities
{
    using OpenTK::Mathematics::Vector3;
    using OpenTK::Mathematics::Matrix4;
    float PlayerEntity::AimSensitivity() const
    {
        if (!_equipInfo->Zoomed) return 1;
        const auto normalHalfFov = Fixed::ToFloat(_values.NormalFov);
        if (_aimFrame.Native)
            return Mods::Input::NativeZoomSensitivity(_cameraInfo->Fov / 2, normalHalfFov,
                Fixed::ToFloat(_values.ZoomSensitivityFactor));
        return normalHalfFov != 0 ? _cameraInfo->Fov / (normalHalfFov * 2) : 1;
    }
    void PlayerEntity::ProjectAimTarget()
    {
        _aimTrace.Count(Mods::Input::AimOperation::Projection);
        _aimPosition = _cameraInfo->Position + OpenTK::Mathematics::Multiply(_gunVec1, Fixed::ToFloat(_values.AimDistance));
    }

    void PlayerEntity::UpdateAimFacing()
    {
        _aimTrace.Count(Mods::Input::AimOperation::Follow);
        if (Features::FixedCrosshair())
        {
            _facingVector = _gunVec1;
            return;
        }
        const float dot = Vector3::Dot(_gunVec1, _facingVector);
        if (dot < Mods::Input::AimEnvelopeCos)
        {
            const Vector3 temp1 = Subtract(_facingVector, Multiply(_gunVec1, dot)).Normalized();
            _aimTrace.Count(Mods::Input::AimOperation::Normalize);
            _facingVector = Add(Multiply(_gunVec1, Mods::Input::AimEnvelopeCos),
                Multiply(temp1, Mods::Input::AimEnvelopeSin));
        }
        const float follow = Mods::Input::AimFollow;
        _facingVector = Add(_facingVector, Multiply(Subtract(_gunVec1, _facingVector), follow)).Normalized();
        _aimTrace.Count(Mods::Input::AimOperation::Normalize);
    }

    void PlayerEntity::UpdateAimY(float amount)
    {
        _aimTrace.Count(Mods::Input::AimOperation::Pitch);
        if (_controls.InvertAimY())
        {
            amount *= -1.0F;
        }
        amount *= AimSensitivity();
        const float prevAim = _aimY;
        _aimY += amount;
        _aimY = _aimFrame.Exact && IsAltForm() ? std::clamp(_aimY, -25.0F, 5.0F) : std::clamp(_aimY, -85.0F, 85.0F);
        const float diff = DegreesToRadians(_aimY - prevAim);
        const Matrix4 transform = GetTransformMatrix(_aimFrame.Exact ? _gunVec1 : _facingVector, Vector3(0.0F, 1.0F, 0.0F));
        const Vector3 vector(0.0F, std::sin(diff), std::cos(diff));
        _aimTrace.Count(Mods::Input::AimOperation::Normalize, 2); // basis + rotated direction
        const auto rotated = Matrix::Vec3MultMtx3(vector, transform).Normalized();
        if (_aimFrame.Exact)
        {
            _gunVec1 = rotated;
            ProjectAimTarget();
            UpdateAimFacing();
        }
        else
        {
            _facingVector = rotated;
            RebuildAimBasis();
        }
    }

    void PlayerEntity::UpdateAimX(float amount)
    {
        _aimTrace.Count(Mods::Input::AimOperation::Yaw);
        if (_controls.InvertAimX())
        {
            amount *= -1.0F;
        }
        amount *= AimSensitivity();
        const float angle = DegreesToRadians(amount);
        const float sinValue = std::sin(angle), cosValue = std::cos(angle);
        const auto rotate = [sinValue, cosValue](Vector3 direction)
        {
            return Vector3(direction.X * cosValue + direction.Z * sinValue, direction.Y,
                direction.X * -sinValue + direction.Z * cosValue).Normalized();
        };
        if (!_aimFrame.Exact)
        {
            _facingVector = rotate(_facingVector);
            _gunVec2 = rotate(_gunVec2);
            _upVector = Vector3::Cross(_facingVector, _gunVec2).Normalized();
            _aimTrace.Count(Mods::Input::AimOperation::Normalize, 3);
            return;
        }
        _gunVec1 = rotate(_gunVec1);
        _aimTrace.Count(Mods::Input::AimOperation::Normalize);
        ProjectAimTarget();
        if (_equipInfo->Zoomed)
        {
            _facingVector = _gunVec1;
            _aimTrace.Count(Mods::Input::AimOperation::Snap);
        }
        else
        {
            UpdateAimFacing();
        }
    }

    void PlayerEntity::RebuildAimBasis()
    {
        _gunVec2 = Vector3::Cross(Vector3(0, 1, 0), _facingVector).Normalized();
        _upVector = Vector3::Cross(_facingVector, _gunVec2).Normalized();
        _aimTrace.Count(Mods::Input::AimOperation::Normalize, 2);
    }

    void PlayerEntity::MaintainNonExactAimTarget()
    {
        const auto desired = Add(_cameraInfo->Position, Multiply(_facingVector, Fixed::ToFloat(_values.AimDistance)));
        _aimPosition = Add(_aimPosition, Multiply(Subtract(desired, _aimPosition), Mods::Input::AimTargetFollow));
        const auto direction = Subtract(_aimPosition, _cameraInfo->Position);
        if (OpenTK::Mathematics::LengthSquared(direction) > 0)
        {
            _gunVec1 = direction.Normalized();
            _aimTrace.Count(Mods::Input::AimOperation::Normalize);
        }
        _aimTrace.Count(Mods::Input::AimOperation::Projection);
    }
}
