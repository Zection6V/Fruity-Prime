#include "PlayerCamera.hpp"

#include "../CamSeq/CameraSequence.hpp"
#include "../DoorEntity.hpp"
#include "../MorphCameraEntity.hpp"
#include "../RoomEntity.hpp"
#include "../../Formats/CollisionDetection.hpp"
#include "../../Scene.hpp"
#include "../../Utility/Rng.hpp"
#include "PlayerEntity.hpp"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <memory>
#include <optional>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

namespace
{
    using MphRead::Formats::CollisionCandidate;
    using MphRead::Formats::CollisionDetection;
    using MphRead::Formats::CollisionResult;
    using MphRead::Formats::TestFlags;
    using OpenTK::Mathematics::Matrix4;
    using OpenTK::Mathematics::Vector3;
    using OpenTK::Mathematics::Vector4;

    constexpr Vector3 UnitX(1.0F, 0.0F, 0.0F);
    constexpr Vector3 UnitY(0.0F, 1.0F, 0.0F);
    constexpr Vector3 UnitZ(0.0F, 0.0F, 1.0F);
    constexpr float Pi = 3.14159265358979323846F;

    template <typename T>
    [[nodiscard]] T& RequireReference(T* value)
    {
        if (value == nullptr)
        {
            throw System::NullReferenceException();
        }
        return *value;
    }

    template <typename T>
    [[nodiscard]] T& RequireReference(const std::shared_ptr<T>& value)
    {
        if (!value)
        {
            throw System::NullReferenceException();
        }
        return *value;
    }

    template <typename TEnum>
    [[nodiscard]] constexpr bool TestFlag(TEnum value, TEnum flag) noexcept
    {
        using Underlying = std::underlying_type_t<TEnum>;
        return (static_cast<Underlying>(value) & static_cast<Underlying>(flag)) != 0;
    }

    [[nodiscard]] constexpr bool IsZero(Vector3 value) noexcept
    {
        return value.X == 0.0F && value.Y == 0.0F && value.Z == 0.0F;
    }

    [[nodiscard]] constexpr Vector3 Negate(Vector3 value) noexcept
    {
        return Vector3(-value.X, -value.Y, -value.Z);
    }

    [[nodiscard]] constexpr Vector3 ScaleVector(Vector3 value, float scale) noexcept
    {
        return Vector3(value.X * scale, value.Y * scale, value.Z * scale);
    }

    [[nodiscard]] constexpr Vector3 Divide(Vector3 value, float divisor) noexcept
    {
        return Vector3(value.X / divisor, value.Y / divisor, value.Z / divisor);
    }

    [[nodiscard]] constexpr Vector3 ComponentMultiply(Vector3 left, Vector3 right) noexcept
    {
        return Vector3(left.X * right.X, left.Y * right.Y, left.Z * right.Z);
    }

    [[nodiscard]] constexpr float LengthSquared(Vector3 value) noexcept
    {
        return value.X * value.X + value.Y * value.Y + value.Z * value.Z;
    }

    [[nodiscard]] Vector3 Clamp(Vector3 value, Vector3 min, Vector3 max) noexcept
    {
        const auto clamp = [](float component, float minimum, float maximum) noexcept
        {
            if (component < minimum)
            {
                return minimum;
            }
            if (component > maximum)
            {
                return maximum;
            }
            return component;
        };
        return Vector3(
            clamp(value.X, min.X, max.X),
            clamp(value.Y, min.Y, max.Y),
            clamp(value.Z, min.Z, max.Z));
    }

    [[nodiscard]] constexpr float DegreesToRadians(float degrees) noexcept
    {
        return degrees * (Pi / 180.0F);
    }

    [[nodiscard]] constexpr float RadiansToDegrees(float radians) noexcept
    {
        return radians * (180.0F / Pi);
    }

    [[nodiscard]] Matrix4 LookAt(Vector3 eye, Vector3 target, Vector3 up)
    {
        const Vector3 z = (eye - target).Normalized();
        const Vector3 x = Vector3::Cross(up, z).Normalized();
        const Vector3 y = Vector3::Cross(z, x).Normalized();
        return Matrix4(
            Vector4(x.X, y.X, z.X, 0.0F),
            Vector4(x.Y, y.Y, z.Y, 0.0F),
            Vector4(x.Z, y.Z, z.Z, 0.0F),
            Vector4(-Vector3::Dot(x, eye), -Vector3::Dot(y, eye),
                -Vector3::Dot(z, eye), 1.0F));
    }

    [[nodiscard]] Vector4 PlaneFromDirection(Vector3 direction) noexcept
    {
        return Vector4(direction, 0.0F);
    }
}

namespace MphRead::Entities
{
    std::shared_ptr<CameraInfo> PlayerEntity::CameraInfo() const noexcept
    {
        return _cameraInfo;
    }

    CameraType PlayerEntity::CameraType() const noexcept
    {
        return _cameraType;
    }

    void PlayerEntity::SwitchCamera(Entities::CameraType type, Vector3 facing)
    {
        ::MphRead::Entities::CameraInfo& camera = RequireReference(_cameraInfo);
        if (type == Entities::CameraType::Third1)
        {
            camera.Target = camera.Position + facing;
            camera.Position = camera.Position - Divide(facing, 64.0F);
        }
        else if (type == Entities::CameraType::Free)
        {
            camera.Target = facing;
            if (_cameraType == Entities::CameraType::First)
            {
                Vector3 camVec = camera.Position - camera.Target;
                if (!IsZero(camVec))
                {
                    camVec = camVec.Normalized();
                }
                else
                {
                    camVec = UnitX;
                }
                camera.Position = camera.Position + Divide(camVec, 64.0F);
            }
        }
        _cameraType = type;
        _field544 = camera.Position;
        _camSwitchTimer = static_cast<std::uint16_t>(
            Values().CamSwitchTime * 2 - _camSwitchTimer);
        camera.Shake = 0.0F;
    }

    void PlayerEntity::UpdateCamera()
    {
        ::MphRead::Entities::CameraInfo& camera = RequireReference(_cameraInfo);
        camera.PrevPosition = camera.Position;
        if (_camSwitchTimer < Values().CamSwitchTime * 2)
        {
            ++_camSwitchTimer;
            if (!IsAltForm() && _camSwitchTimer == Values().CamSwitchTime * 2)
            {
                SetGunAnimation(GunAnimation::UpDown, AnimFlags::NoLoop);
            }
        }
        if (IsMainPlayer() && Formats::CameraSequence::Current() != nullptr)
        {
            return;
        }
        if (_cameraType == Entities::CameraType::Third1)
        {
            UpdateCameraThird1();
        }
        else if (_cameraType == Entities::CameraType::Third2)
        {
            UpdateCameraThird2();
        }
        else if (_cameraType == Entities::CameraType::Free)
        {
            UpdateCameraFree();
        }
        else if (_cameraType == Entities::CameraType::Spectator)
        {
            UpdateCameraSpectator();
        }
        else
        {
            UpdateCameraFirst();
        }
        camera.Update();
    }

    void PlayerEntity::UpdateCameraFirst()
    {
        ::MphRead::Entities::CameraInfo& camera = RequireReference(_cameraInfo);
        Vector3 position = Position;
        if (!_field6D0)
        {
            position.Y += Fixed::ToFloat(Values().AimYOffset)
                + std::cos(DegreesToRadians(_gunViewBob)) * _walkViewBob;
        }
        if (_timeStanding < 9 * 2)
        {
            const float angle = DegreesToRadians(static_cast<float>(
                360 * _timeStanding / (9 * 2)));
            position.Y += std::cos(angle) * _field44C - _field44C;
        }
        const float switchTime = static_cast<float>(Values().CamSwitchTime * 2);
        if (_camSwitchTimer < switchTime)
        {
            const float pct = static_cast<float>(_camSwitchTimer) / switchTime;
            camera.Position = _field544 + ScaleVector(position - _field544, pct);
            const Vector3 target = camera.Position + _facingVector;
            camera.Target = static_cast<Vector3>(Position)
                + ScaleVector(target - static_cast<Vector3>(Position), pct);
        }
        else
        {
            camera.Position = Position;
            camera.Target = camera.Position + _facingVector;
        }
        camera.Target.Y += Fixed::ToFloat(Values().ViewTiltFactor)
            * std::sin(DegreesToRadians(_viewTiltAngleV));
        if (std::abs(_viewTiltAngleH) >= 1.0F / 4096.0F)
        {
            const Vector3 toTarget = camera.Target - camera.Position;
            const Vector3 upVec(-toTarget.Z, 0.0F, toTarget.X);
            const Vector3 normalizedUp = upVec.Normalized();
            const float factor = Fixed::ToFloat(Values().ViewTiltFactor)
                * std::sin(DegreesToRadians(_viewTiltAngleH));
            camera.UpVector = Vector3(
                normalizedUp.X * factor, 1.0F, normalizedUp.Z * factor);
        }
        else
        {
            camera.UpVector = UnitY;
        }
        if (RequireReference(EquipInfo()).Zoomed
            && TestFlag(Flags1(), PlayerFlags1::Walking))
        {
            camera.Target.Y += std::cos(DegreesToRadians(_gunViewBob)) * 0.025F;
        }
    }

    void PlayerEntity::UpdateCameraThird1()
    {
        ::MphRead::Entities::CameraInfo& camera = RequireReference(_cameraInfo);
        float v5;
        float v6;
        float v7;
        if (!TestFlag(Flags1(), PlayerFlags1::NoUnmorph))
        {
            v5 = Fixed::ToFloat(Values().Field78);
            v6 = Fixed::ToFloat(Values().Field7C);
            v7 = Fixed::ToFloat(Values().Field80);
        }
        else
        {
            v5 = 1.5F;
            v6 = 0.7F;
            v7 = 0.5F;
        }

        const CollisionVolume volume = Volume();
        camera.Target.X = volume.SpherePosition.X;
        camera.Target.Y -= v7;
        camera.Target.Y += (volume.SpherePosition.Y - camera.Target.Y) / 2.0F;
        camera.Target.Z = volume.SpherePosition.Z;

        const std::shared_ptr<MorphCameraEntity> morphCamera = MorphCamera();
        if (morphCamera != nullptr)
        {
            camera.Position = morphCamera->Position;
            return;
        }

        Vector3 posVec;
        if (_jumpPadControlLock > 0)
        {
            Vector3 camVec = TypeExtensions::WithY(
                camera.Position - camera.Target, 0.0F).Normalized();
            posVec = Vector3(
                camera.Target.X + camVec.X,
                camera.Target.Y + v6,
                camera.Target.Z + camVec.Z);
        }
        else if (_field551 <= 1)
        {
            const Vector3 camVec = (camera.Position - camera.Target).Normalized();
            posVec = Vector3(
                camera.Target.X + camVec.X * v5,
                camera.Position.Y,
                camera.Target.Z + camVec.Z * v5);
        }
        else
        {
            Vector3 camVec;
            if (_camSwitchTimer >= Values().CamSwitchTime * 2)
            {
                camVec = TypeExtensions::WithY(
                    camera.Position - camera.Target, 0.0F);
            }
            else
            {
                camVec = Negate(TypeExtensions::WithY(camera.Facing, 0.0F));
                _field544 = _field544
                    + Divide(static_cast<Vector3>(Position) - PrevPosition(), 2.0F);
            }
            camVec = camVec.Normalized();
            posVec = Vector3(
                camera.Target.X + camVec.X * v5,
                camera.Target.Y + v6,
                camera.Target.Z + camVec.Z * v5);
        }

        camera.Target.Y += v7;
        if (_camSwitchTimer < Values().CamSwitchTime * 2)
        {
            const float pct = static_cast<float>(_camSwitchTimer)
                / (static_cast<float>(Values().CamSwitchTime) * 2.0F);
            camera.Position = _field544 + ScaleVector(posVec - _field544, pct);
            const Vector3 facingVec = camera.Position + camera.Facing;
            camera.Target = facingVec + ScaleVector(camera.Target - facingVec, pct);
        }
        else
        {
            const float factor = Fixed::ToFloat(Values().Field84);
            camera.Position = camera.Position + ScaleVector(posVec - camera.Position, factor);
        }

        if (_field553 > 0)
        {
            --_field553;
        }

        if (LengthSquared(camera.Position - volume.SpherePosition) >= 6.0F * 6.0F)
        {
            _field551 = 255;
        }
        else
        {
            float speedMagSqr = LengthSquared(Divide(Speed(), 2.0F));
            if (speedMagSqr > Fixed::ToFloat(36) && _field551 != 255)
            {
                ++_field551;
            }

            bool blocked1 = false;
            bool blocked2 = false;
            bool blocked4 = false;
            bool blocked8 = false;
            Vector3 point1 = volume.SpherePosition;
            Vector3 point2 = camera.Position;
            float margin = volume.SphereRadius;
            const std::vector<std::shared_ptr<CollisionCandidate>>& candidatesRef
                = CollisionDetection::GetCandidatesForLimits(
                    point1, point2, margin, std::nullopt, Vector3::Zero, true, _scene);
            const auto* candidates = &candidatesRef;
            const float v35 = camera.Field50 * margin;
            const float v36 = camera.Field54 * margin;

            point1 = TypeExtensions::AddZ(
                TypeExtensions::AddX(camera.Position, v35), v36);
            point2 = TypeExtensions::AddZ(
                TypeExtensions::AddX(volume.SpherePosition, v35), v36);
            CollisionResult res{};
            if (CollisionDetection::CheckBetweenPoints(
                candidates, point1, point1, TestFlags::Players, _scene, res))
            {
                blocked1 = true;
            }

            point1 = TypeExtensions::AddZ(
                TypeExtensions::AddX(camera.Position, -v35), -v36);
            point2 = TypeExtensions::AddZ(
                TypeExtensions::AddX(volume.SpherePosition, -v35), -v36);
            if (CollisionDetection::CheckBetweenPoints(
                candidates, point1, point1, TestFlags::Players, _scene, res))
            {
                blocked2 = true;
            }

            point1 = camera.Position + ScaleVector(camera.UpVector, margin);
            point2 = volume.SpherePosition + ScaleVector(camera.UpVector, margin);
            if (CollisionDetection::CheckBetweenPoints(
                candidates, point1, point1, TestFlags::Players, _scene, res))
            {
                blocked4 = true;
                _field551 = 0;
            }

            point1 = camera.Position - ScaleVector(camera.UpVector, margin / 2.0F);
            point2 = volume.SpherePosition - ScaleVector(camera.UpVector, margin / 2.0F);
            if (CollisionDetection::CheckBetweenPoints(
                candidates, point1, point1, TestFlags::Players, _scene, res))
            {
                blocked8 = true;
                _field551 = 0;
            }

            float max = Fixed::ToFloat(100);
            if (speedMagSqr > max)
            {
                speedMagSqr = max;
            }

            if (!blocked1 || (_field558 <= 0.0F && blocked2))
            {
                if (!blocked2)
                {
                    _field558 = 0.0F;
                }
                else
                {
                    if (_field558 > 0.0F)
                    {
                        _field558 = 0.0F;
                    }
                    _field558 -= Fixed::ToFloat(Values().Field88)
                        * speedMagSqr / max / 2.0F;
                    if (_field558 < -Fixed::ToFloat(Values().Field8C))
                    {
                        _field558 = -Fixed::ToFloat(Values().Field8C);
                    }
                    const float angle = DegreesToRadians(_field558 * 22.5F);
                    float cos;
                    float sin;
                    if (angle <= 0.0F)
                    {
                        cos = std::cos(angle);
                        sin = std::sin(angle);
                    }
                    else
                    {
                        cos = std::cos(-angle);
                        sin = -std::sin(-angle);
                    }
                    Vector3 v119 = camera.Position - camera.Target;
                    const float x = v119.X;
                    const float z = v119.Z;
                    v119.X = x * cos + z * sin;
                    v119.Z = x * -sin + z * cos;
                    camera.Position = v119 + camera.Target;
                }
            }
            else
            {
                if (_field558 < 0.0F)
                {
                    _field558 = 0.0F;
                }
                _field558 += Fixed::ToFloat(Values().Field88)
                    * speedMagSqr / max / 2.0F;
                if (_field558 > Fixed::ToFloat(Values().Field8C))
                {
                    _field558 = Fixed::ToFloat(Values().Field8C);
                }
                const float angle = DegreesToRadians(_field558 * 22.5F);
                float cos;
                float sin;
                if (angle <= 0.0F)
                {
                    cos = std::cos(angle);
                    sin = std::sin(angle);
                }
                else
                {
                    cos = std::cos(-angle);
                    sin = -std::sin(-angle);
                }
                Vector3 v219 = camera.Position - camera.Target;
                const float x = v219.X;
                const float z = v219.Z;
                v219.X = x * cos + z * sin;
                v219.Z = x * -sin + z * cos;
                camera.Position = v219 + camera.Target;
            }

            if (!blocked8 || (_field554 <= 0.0F && blocked4))
            {
                if (!blocked4)
                {
                    _field554 = 0.0F;
                }
                else
                {
                    if (_field558 > 0.0F)
                    {
                        _field558 = 0.0F;
                    }
                    _field554 -= Fixed::ToFloat(Values().Field88)
                        * speedMagSqr / max / 2.0F;
                    if (_field554 < -Fixed::ToFloat(Values().Field8C))
                    {
                        _field554 = -Fixed::ToFloat(Values().Field8C);
                    }
                    const float angle = DegreesToRadians(_field554);
                    float cos;
                    float sin;
                    if (angle <= 0.0F)
                    {
                        cos = std::cos(angle);
                        sin = std::sin(angle);
                    }
                    else
                    {
                        cos = std::cos(-angle);
                        sin = -std::sin(-angle);
                    }
                    Vector3 v319 = camera.Position - camera.Target;
                    const float x = v319.X;
                    const float y = v319.Y;
                    const float z = v319.Z;
                    v319.X = camera.UpVector.X * sin + x * cos;
                    v319.Y = camera.UpVector.Y * sin + y * cos;
                    v319.Z = camera.UpVector.Z * sin + z * cos;
                    camera.Position = v319 + camera.Target;
                }
            }
            else
            {
                if (_field558 < 0.0F)
                {
                    _field558 = 0.0F;
                }
                _field554 += Fixed::ToFloat(Values().Field88)
                    * speedMagSqr / max / 2.0F;
                if (_field554 > Fixed::ToFloat(Values().Field8C))
                {
                    _field554 = Fixed::ToFloat(Values().Field8C);
                }
                const float angle = DegreesToRadians(_field554);
                float cos;
                float sin;
                if (angle <= 0.0F)
                {
                    cos = std::cos(angle);
                    sin = std::sin(angle);
                }
                else
                {
                    cos = std::cos(-angle);
                    sin = -std::sin(-angle);
                }
                Vector3 v419 = camera.Position - camera.Target;
                const float x = v419.X;
                const float y = v419.Y;
                const float z = v419.Z;
                v419.X = camera.UpVector.X * sin + x * cos;
                v419.Y = camera.UpVector.Y * sin + y * cos;
                v419.Z = camera.UpVector.Z * sin + z * cos;
                camera.Position = v419 + camera.Target;
            }

            point1 = camera.PrevPosition;
            point2 = camera.Position;
            margin = Fixed::ToFloat(Values().Field90);
            const std::vector<std::shared_ptr<CollisionCandidate>>& sweepCandidatesRef
                = CollisionDetection::GetCandidatesForLimits(
                    point1, point2, margin, std::nullopt, Vector3::Zero, true, _scene);
            const auto* sweepCandidates = &sweepCandidatesRef;
            ManagedArray<CollisionResult> results(8);
            const std::int32_t count = CollisionDetection::CheckSphereBetweenPoints(
                sweepCandidates, point1, point2, margin, 8, true,
                TestFlags::Players, _scene, &results);
            bool v85 = false;
            for (std::int32_t i = 0; i < count; ++i)
            {
                const CollisionResult result = results[static_cast<std::size_t>(i)];
                if (result.Field0 == 0)
                {
                    const float dot = -(Vector3::Dot(camera.Position, result.Plane.Xyz())
                        - result.Plane.W - margin);
                    if (dot > 0.0F)
                    {
                        camera.Position = camera.Position
                            + ScaleVector(result.Plane.Xyz(), dot);
                        v85 = true;
                    }
                }
            }
            if (v85)
            {
                const Vector3 toTarget = camera.Target - camera.Position;
                for (std::int32_t i = 0; i < count; ++i)
                {
                    const CollisionResult result = results[static_cast<std::size_t>(i)];
                    if (result.Field0 == 1
                        && Vector3::Dot(toTarget, result.Plane.Xyz()) >= 0.0F)
                    {
                        const float dot = -(Vector3::Dot(camera.Position, result.Plane.Xyz())
                            - result.Plane.W - margin);
                        if (dot > 0.0F)
                        {
                            camera.Position = camera.Position
                                + ScaleVector(result.Plane.Xyz(), dot);
                        }
                    }
                }
            }

            auto doors = RequireReference(_scene).GetDoorEntities().GetEnumerator();
            while (doors.MoveNext())
            {
                const std::shared_ptr<DoorEntity> doorPtr = doors.Current();
                DoorEntity& door = RequireReference(doorPtr);
                if (TestFlag(door.Flags(), DoorFlags::Open) || door.ConnectorInactive())
                {
                    continue;
                }
                const Vector3 lockPosition = door.LockPosition();
                const Vector3 toLock = static_cast<Vector3>(Position) - lockPosition;
                const Vector3 doorFacing = door.FacingVector();
                Vector4 doorPlane;
                if (Vector3::Dot(toLock, doorFacing) >= 0.0F)
                {
                    doorPlane = PlaneFromDirection(doorFacing);
                }
                else
                {
                    doorPlane = PlaneFromDirection(Negate(doorFacing));
                }
                const Vector3 planeXyz = doorPlane.Xyz();
                const Vector3 wvec = ComponentMultiply(
                    planeXyz, lockPosition + ScaleVector(planeXyz, 0.4F));
                doorPlane.W = wvec.X + wvec.Y + wvec.Z;
                CollisionResult planeRes{};
                if (CollisionDetection::CheckCylinderIntersectPlane(
                    Position, camera.Position, doorPlane, planeRes))
                {
                    if (LengthSquared(planeRes.Position - lockPosition)
                        < door.RadiusSquared() + 1.0F)
                    {
                        float dot = Vector3::Dot(camera.Position, doorPlane.Xyz())
                            - doorPlane.W;
                        if (dot <= 0.0F)
                        {
                            dot += 0.1F;
                            camera.Position = camera.Position
                                - ScaleVector(doorPlane.Xyz(), dot);
                        }
                    }
                }
            }

            if (!blocked1 && !blocked2 && !blocked4 && !blocked8)
            {
                _field552 = 255;
            }
        }

        CollisionResult targResult{};
        if (CollisionDetection::CheckBetweenPoints(
            camera.Target, camera.Position, TestFlags::Players, _scene, targResult))
        {
            if (_field552 < 15 * 2)
            {
                ++_field552;
            }
            else
            {
                const Vector3 between = camera.Position - camera.Target;
                camera.Position = camera.Target + ScaleVector(between, targResult.Distance);
            }
        }
        else
        {
            _field552 = 0;
        }
    }

    void PlayerEntity::UpdateCameraThird2()
    {
        ::MphRead::Entities::CameraInfo& camera = RequireReference(_cameraInfo);
        const float switchTime = static_cast<float>(Values().CamSwitchTime * 2);
        if (_camSwitchTimer < switchTime)
        {
            _field68C = Fixed::ToFloat(Values().Field80);
            _field690 = Fixed::ToFloat(Values().Field78);
        }
        else if (TestFlag(Flags1(), PlayerFlags1::NoUnmorph))
        {
            assert(_field68C >= 0.0F);
            _field68C += -0.2F * _field68C / 2.0F;
            _field690 += 0.2F * (1.5F - _field690) / 2.0F;
        }
        else
        {
            _field68C += 0.2F
                * (Fixed::ToFloat(Values().Field80) - _field68C) / 2.0F;
            _field690 += 0.2F
                * (Fixed::ToFloat(Values().Field78) - _field690) / 2.0F;
        }

        const CollisionVolume volume = Volume();
        camera.Target = volume.SpherePosition;
        const Vector3 camTarget = camera.Target;
        camera.Target.Y += _field68C;
        const std::shared_ptr<MorphCameraEntity> morphCamera = MorphCamera();
        if (morphCamera != nullptr)
        {
            camera.Position = morphCamera->Position;
            return;
        }

        Vector3 camVec;
        if (TestFlag(Flags1(), PlayerFlags1::NoUnmorph))
        {
            camVec = Vector3(
                _field70 * _field690, 0.0F, _field74 * _field690);
        }
        else
        {
            camVec = ScaleVector(_facingVector, _field690);
        }
        const Vector3 posVec = camera.Target - camVec;
        if (_camSwitchTimer < switchTime)
        {
            const float pct = static_cast<float>(_camSwitchTimer) / switchTime;
            camera.Position = _field544 + ScaleVector(posVec - _field544, pct);
            const Vector3 facingVec = camera.Position + _facingVector;
            camera.Target = facingVec + ScaleVector(camera.Target - facingVec, pct);
        }
        else
        {
            const float factor = Fixed::ToFloat(Values().Field84);
            camera.Position = camera.Position + ScaleVector(posVec - camera.Position, factor);
        }

        CollisionResult result{};
        if (CollisionDetection::CheckBetweenPoints(
            camTarget, camera.Position, TestFlags::Players, _scene, result))
        {
            const Vector3 toTarget = camera.Position - camTarget;
            camera.Position = camTarget + ScaleVector(toTarget, result.Distance);
            camera.Position = camera.Position + ScaleVector(result.Plane.Xyz(), 0.15F);
        }
    }

    void PlayerEntity::UpdateCameraFree()
    {
        ::MphRead::Entities::CameraInfo& camera = RequireReference(_cameraInfo);
        Scene& scene = RequireReference(_scene);
        assert(scene.Room() != nullptr);

        const std::uint16_t switchTime = static_cast<std::uint16_t>(
            Values().CamSwitchTime * 2);
        if (_camSwitchTimer < switchTime)
        {
            const float pct = static_cast<float>(_camSwitchTimer)
                / static_cast<float>(switchTime);
            Vector3 camVec = camera.Position - camera.Target;
            camVec = !IsZero(camVec) ? camVec.Normalized() : _facingVector;
            Vector3 posVec = Volume().SpherePosition
                + ScaleVector(camVec, Fixed::ToFloat(Values().Field78));
            if (RequireReference(scene.Room()).Meta().HasLimits)
            {
                posVec = Clamp(posVec,
                    RequireReference(scene.Room()).Meta().CameraMin,
                    RequireReference(scene.Room()).Meta().CameraMax);
            }
            camera.Position = _field544 + ScaleVector(posVec - _field544, pct);
        }
        else
        {
            const float limit = Fixed::ToFloat(40);
            if (camera.Facing.X < limit && camera.Facing.X > -limit
                && camera.Facing.Z < limit && camera.Facing.Z > -limit)
            {
                if (std::abs(camera.Facing.X) >= 1.0F / 4096.0F
                    || std::abs(camera.Facing.X) >= 1.0F / 4096.0F)
                {
                    camera.Facing.X *= 4.0F;
                    camera.Facing.Z *= 4.0F;
                }
                else
                {
                    camera.Facing.X = limit;
                }
            }

            if (Controls().MoveUp().IsDown())
            {
                camera.Position = camera.Position
                    + Divide(ScaleVector(camera.Facing, 0.4F), 2.0F);
            }
            else if (Controls().MoveDown().IsDown())
            {
                camera.Position = camera.Position
                    - Divide(ScaleVector(camera.Facing, 0.4F), 2.0F);
            }
            if (Controls().MoveLeft().IsDown())
            {
                camera.Position.X += camera.Field50 * 0.4F / 2.0F;
                camera.Position.Z += camera.Field54 * 0.4F / 2.0F;
            }
            else if (Controls().MoveRight().IsDown())
            {
                camera.Position.X -= camera.Field50 * 0.4F / 2.0F;
                camera.Position.Z -= camera.Field54 * 0.4F / 2.0F;
            }

            float aimY = 0.0F;
            float aimX = 0.0F;
            if (Controls().MouseAim() && !IsBot())
            {
                if (!Controls().KeyboardAim()
                    || (!Controls().AimUp().IsDown() && !Controls().AimDown().IsDown()))
                {
                    aimY = -_input.MouseDeltaY() / 4.0F
                        * Mods::InputSettings::MouseSensitivity;
                }
                if (!Controls().KeyboardAim()
                    || (!Controls().AimLeft().IsDown() && !Controls().AimRight().IsDown()))
                {
                    aimX = -_input.MouseDeltaX() / 4.0F
                        * Mods::InputSettings::MouseSensitivity;
                }
            }

            if (Controls().KeyboardAim() || IsBot())
            {
                const float maxAimX = _maxButtonAimX * 30.0F;
                const float aimStepX = maxAimX * (40.0F / 4096.0F);
                const float maxAimY = _maxButtonAimY * 30.0F;
                const float aimStepY = maxAimY * (40.0F / 4096.0F);

                if (Controls().AimRight().IsDown())
                {
                    std::tie(_buttonAimX, aimX) = ConstantAcceleration(
                        -aimStepX, _buttonAimX, -maxAimX, -maxAimX * 0.4F);
                }
                else if (Controls().AimLeft().IsDown())
                {
                    std::tie(_buttonAimX, aimX) = ConstantAcceleration(
                        aimStepX, _buttonAimX, maxAimY * 0.4F, maxAimX);
                }
                else if (_buttonAimX != 0.0F)
                {
                    if ((_buttonAimX > 0.0F && _buttonAimX < 1.0F / 4096.0F)
                        || (_buttonAimX < 0.0F && _buttonAimX > -1.0F / 4096.0F))
                    {
                        _buttonAimX = 0.0F;
                    }
                    else
                    {
                        float updateAimX;
                        std::tie(_buttonAimX, updateAimX) = Drag(0.4F, _buttonAimX);
                        if (aimX == 0.0F)
                        {
                            aimX = updateAimX;
                        }
                    }
                }

                if (Controls().AimUp().IsDown())
                {
                    std::tie(_buttonAimY, aimY) = ConstantAcceleration(
                        aimStepY, _buttonAimY, maxAimY * 0.4F, maxAimY);
                }
                else if (Controls().AimDown().IsDown())
                {
                    std::tie(_buttonAimY, aimY) = ConstantAcceleration(
                        -aimStepY, _buttonAimY, -maxAimY, -maxAimY * 0.4F);
                }
                else if (_buttonAimY != 0.0F)
                {
                    if ((_buttonAimY > 0.0F && _buttonAimY < 1.0F / 4096.0F)
                        || (_buttonAimY < 0.0F && _buttonAimY > -1.0F / 4096.0F))
                    {
                        _buttonAimY = 0.0F;
                    }
                    else
                    {
                        float updateAimY;
                        std::tie(_buttonAimY, updateAimY) = Drag(0.4F, _buttonAimY);
                        if (aimY == 0.0F)
                        {
                            aimY = updateAimY;
                        }
                    }
                }
            }

            if (Controls().InvertAimY())
            {
                aimY *= -1.0F;
            }
            if (Controls().InvertAimX())
            {
                aimX *= -1.0F;
            }
            const bool updateY = aimY != 0.0F
                && (camera.Facing.Y < 0.985F || aimY < 0.0F)
                && (camera.Facing.Y > -0.985F || aimY > 0.0F);
            const bool updateX = aimX != 0.0F;
            if (updateY || updateX)
            {
                float pitch = RadiansToDegrees(std::asin(camera.Facing.Y));
                float yaw = RadiansToDegrees(std::atan2(
                    camera.Facing.X, camera.Facing.Z));
                if (updateY)
                {
                    pitch = std::fmod(pitch + aimY, 360.0F);
                }
                if (updateX)
                {
                    yaw = std::fmod(yaw + aimX, 360.0F);
                }
                pitch = DegreesToRadians(pitch);
                yaw = DegreesToRadians(yaw);
                const float cos = std::cos(pitch);
                camera.Facing = Vector3(
                    std::sin(yaw) * cos,
                    std::sin(pitch),
                    std::cos(yaw) * cos);
            }

            Vector3 pos = camera.Position;
            if (RequireReference(scene.Room()).Meta().HasLimits)
            {
                pos = Clamp(pos,
                    RequireReference(scene.Room()).Meta().CameraMin,
                    RequireReference(scene.Room()).Meta().CameraMax);
            }
            camera.Position = pos;
            camera.Target = camera.Position + camera.Facing;
        }

        Vector3 point1 = camera.PrevPosition;
        Vector3 point2 = camera.Position;
        const float margin = 0.5F;
        const std::vector<std::shared_ptr<CollisionCandidate>>& candidatesRef
            = CollisionDetection::GetCandidatesForLimits(
                point1, point2, margin, std::nullopt, Vector3::Zero, true, _scene);
        const auto* candidates = &candidatesRef;
        ManagedArray<CollisionResult> results(8);
        const std::int32_t count = CollisionDetection::CheckSphereBetweenPoints(
            candidates, point1, point2, margin, 8, false,
            TestFlags::Players, _scene, &results);
        for (std::int32_t i = 0; i < count; ++i)
        {
            const CollisionResult result = results[static_cast<std::size_t>(i)];
            const float dot = -(Vector3::Dot(camera.Position, result.Plane.Xyz())
                - result.Plane.W - margin);
            if (dot > 0.0F)
            {
                camera.Position = camera.Position
                    + ScaleVector(result.Plane.Xyz(), dot);
                _camSwitchTimer = switchTime;
            }
        }
        if (_camSwitchTimer >= switchTime)
        {
            camera.Target = camera.Position + camera.Facing;
        }
    }

    void PlayerEntity::UpdateCameraSpectator()
    {
    }

    void PlayerEntity::SetUpMatchEndCamera()
    {
        ::MphRead::Entities::CameraInfo& camera = RequireReference(_cameraInfo);
        _field70 = camera.Field48;
        _field74 = camera.Field4C;
        _gunVec2 = Vector3(camera.Field50, 0.0F, camera.Field54);
        _facingVector = camera.Facing;
        SetTransform(_facingVector, _upVector, Position);
    }

    void PlayerEntity::UpdateMatchEndCamera(
        std::shared_ptr<PlayerEntity> winner, float timeSinceMatchEnd)
    {
        PlayerEntity& winnerRef = RequireReference(winner);
        ::MphRead::Entities::CameraInfo& camera = RequireReference(_cameraInfo);
        const Vector3 winnerFacing = winnerRef.FacingVector();

        _cameraType = Entities::CameraType::Third2;
        camera.Target = winnerRef.Position;
        if (winnerRef.IsAltForm()
            || winnerRef.IsMorphing()
            || winnerRef.IsUnmorphing())
        {
            camera.Position = TypeExtensions::AddZ(
                TypeExtensions::AddY(
                    TypeExtensions::AddX(
                        winnerRef.Position, -(2.75F * winnerRef.Field70())),
                    3.0F),
                -(2.75F * winnerRef.Field74()));
        }
        else
        {
            camera.Target = camera.Target + ScaleVector(winnerFacing, 10.0F);
            camera.Position = TypeExtensions::AddZ(
                TypeExtensions::AddY(
                    TypeExtensions::AddX(
                        winnerRef.Position,
                        -(1.5F * winnerRef.Field70() + winnerRef._gunVec2.X / 2.0F)),
                    1.75F),
                -(1.5F * winnerRef.Field74() + winnerRef._gunVec2.X / 2.0F));
        }

        if (winnerFacing.Y < 0.0F)
        {
            camera.Position = TypeExtensions::AddZ(
                TypeExtensions::AddY(
                    TypeExtensions::AddX(
                        camera.Position,
                        -(2.15F * winnerFacing.Y * winnerRef.Field70())),
                    -(winnerFacing.Y / 2.0F)),
                -(2.15F * winnerFacing.Y * winnerRef.Field74()));
        }
        else
        {
            camera.Position = TypeExtensions::AddZ(
                TypeExtensions::AddY(
                    TypeExtensions::AddX(
                        camera.Position,
                        0.75F * winnerFacing.Y * winnerRef.Field70()),
                    -(winnerFacing.Y * 2.0F)),
                0.75F * winnerFacing.Y * winnerRef.Field74());
        }

        const float factor = Fixed::ToFloat(15) * timeSinceMatchEnd * 30.0F;
        camera.Position = TypeExtensions::AddZ(
            TypeExtensions::AddX(
                camera.Position, winnerRef._gunVec2.X * factor),
            winnerRef._gunVec2.Z * factor);

        CollisionResult result{};
        if (CollisionDetection::CheckBetweenPoints(
            winnerRef.Position, camera.Position,
            TestFlags::Players, _scene, result))
        {
            const Vector3 between = camera.Position
                - static_cast<Vector3>(winnerRef.Position);
            camera.Position = static_cast<Vector3>(winnerRef.Position)
                + ScaleVector(between, result.Distance)
                + ScaleVector(result.Plane.Xyz(), 0.05F);
        }

        camera.UpVector = UnitY;
        camera.Shake = 0.0F;
        camera.Fov = Fixed::ToFloat(Values().NormalFov) * 2.0F;
        camera.Update();
        camera.NodeRef = RequireReference(_scene).UpdateNodeRef(
            winnerRef.NodeRef, winnerRef.Position, camera.Position);
    }

    void PlayerEntity::RefreshExternalCamera()
    {
        SetFlags1(Flags1() | PlayerFlags1::AltDirOverride);
        _timeSinceMorphCamera = 0;
    }

    void PlayerEntity::ResumeOwnCamera()
    {
        ::MphRead::Entities::CameraInfo& camera = RequireReference(_cameraInfo);
        if (_cameraType == Entities::CameraType::Third1)
        {
            camera.Target = Position;
            camera.Position = camera.Target;
            camera.Position.X -= _field80 * Fixed::ToFloat(Values().Field78);
            camera.Position.Z -= _field84 * Fixed::ToFloat(Values().Field78);
            _field544 = camera.Position;
            camera.Target.Y += Fixed::ToFloat(Values().Field80);
        }
        else
        {
            _field68C = Fixed::ToFloat(Values().Field80);
            _field690 = Fixed::ToFloat(Values().Field78);
            camera.Target = TypeExtensions::AddY(
                Position, _field68C + Fixed::ToFloat(Values().AltColYPos));
            camera.Position = camera.Target - ScaleVector(_facingVector, _field690);
            _field544 = camera.Position;
        }
    }

    void CameraInfo::Reset()
    {
        PrevPosition = UnitZ;
        Position = PrevPosition;
        Target = Vector3::Zero;
        UpVector = UnitY;
        Fov = 39.0F * 2.0F;
    }

    void CameraInfo::Update()
    {
        const Vector3 toTarget = Target - Position;
        const Vector3 camUp = Vector3::Cross(
            toTarget, Vector3::Cross(UpVector, toTarget));

        if (Shake > 0.0F && _shake)
        {
            Target.X += Fixed::ToFloat(Rng::GetRandomInt2(Fixed::ToInt(Shake)))
                - Shake / 2.0F;
            Target.Y += Fixed::ToFloat(Rng::GetRandomInt2(Fixed::ToInt(Shake)))
                - Shake / 2.0F;
            Target.Z += Fixed::ToFloat(Rng::GetRandomInt2(Fixed::ToInt(Shake)))
                - Shake / 2.0F;
            if (toTarget.X * (Target.X - Position.X)
                + toTarget.Z * (Target.Z - Position.Z) < 0.0F)
            {
                Target.X = Position.X + toTarget.X / 2.0F;
                Target.Z = Position.Z + toTarget.Z / 2.0F;
            }
            Shake *= 0.85F;
            if (Shake < 0.01F)
            {
                Shake = 0.0F;
            }
        }

        _shake = !_shake;
        Facing = Target - Position;
        const float facingX = Facing.X;
        const float facingZ = Facing.Z;
        const float hMag = std::sqrt(facingX * facingX + facingZ * facingZ);
        Facing = Facing.Normalized();
        Field48 = facingX / hMag;
        Field4C = facingZ / hMag;
        Field50 = Field4C;
        Field54 = -Field48;
        ViewMatrix = LookAt(Position, Target, camUp);
        TrueUp = camUp;
    }

    void CameraInfo::SetShake(float value)
    {
        if (Shake < value)
        {
            Shake = value;
        }
    }
}
