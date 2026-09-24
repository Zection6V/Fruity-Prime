#include "01_Zoomer.hpp"

#include "../../Formats/CollisionDetection.hpp"
#include "../../Metadata/Enemies.hpp"
#include "../../Utility/Rng.hpp"
#include "../EnemySpawnEntity.hpp"
#include "../../NativeRuntime/System/Managed.hpp"
#include "../../Formats/Types.hpp"

#include <bit>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>

using ::MphRead::NativeRuntime::ConvertToInt32Net9;
using ::MphRead::NativeRuntime::UInt32ToInt32;
using ::MphRead::NativeRuntime::UncheckedAdd;
using ::MphRead::NativeRuntime::UncheckedMultiply;
using ::MphRead::NativeRuntime::UncheckedNegate;
using ::OpenTK::Mathematics::Equal;
using ::OpenTK::Mathematics::MathHelper::DegreesToRadians;
using ::OpenTK::Mathematics::ScaleVector;
using ::OpenTK::Mathematics::WithY;

namespace MphRead::Entities::Enemies
{
    namespace
    {
        using OpenTK::Mathematics::Vector3;

        EnemySpawnEntity* CastSpawner(EntityBase* spawner) noexcept
        {
            EnemySpawnEntity* typedSpawner = dynamic_cast<EnemySpawnEntity*>(spawner);
            assert(typedSpawner != nullptr);
            return typedSpawner;
        }

        [[nodiscard]] std::int32_t ShiftRightInt32(std::int32_t value, std::uint32_t count) noexcept
        {
            count &= 31U;
            if (count == 0U)
            {
                return value;
            }
            std::uint32_t bits = static_cast<std::uint32_t>(value);
            std::uint32_t shifted = bits >> count;
            if ((bits & 0x80000000U) != 0U)
            {
                shifted |= 0xFFFFFFFFU << (32U - count);
            }
            return UInt32ToInt32(shifted);
        }

        [[nodiscard]] float CollisionCorrection(float value, std::int32_t rmd) noexcept
        {
            std::int32_t n = ConvertToInt32Net9(value * 4096.0F);
            std::int32_t v20 = UncheckedMultiply(n, rmd);
            std::int64_t product = static_cast<std::int64_t>(n) * static_cast<std::int64_t>(rmd);
            std::uint64_t productBits = std::bit_cast<std::uint64_t>(product);
            std::int32_t v21 = UInt32ToInt32(static_cast<std::uint32_t>(productBits >> 32U));
            std::int32_t v22;
            if (v21 < 0)
            {
                std::int32_t first = ShiftRightInt32(UncheckedNegate(v20), 12U);
                std::int32_t carry = UncheckedAdd(v21, v20 != 0 ? 1 : 0);
                std::int32_t second = UncheckedMultiply(-1048576, carry);
                v22 = UncheckedNegate(UInt32ToInt32(
                    static_cast<std::uint32_t>(first) | static_cast<std::uint32_t>(second)));
            }
            else
            {
                std::int32_t first = ShiftRightInt32(v20, 12U);
                std::uint32_t second = static_cast<std::uint32_t>(v21) << 20U;
                v22 = UInt32ToInt32(static_cast<std::uint32_t>(first) | second);
            }
            return static_cast<float>(v22) / 4096.0F;
        }
    }

    Enemy01Entity::Enemy01Entity(EnemyInstanceEntityData data,
        Formats::Culling::NodeRef nodeRef, Scene* scene)
        : EnemyInstanceEntity(data, nodeRef, scene),
          _spawner(CastSpawner(data.Spawner))
    {
    }

    void Enemy01Entity::EnemyInitialize()
    {
        const std::uint32_t facingX = Rng::GetRandomInt2(4096);
        const std::uint32_t facingZ = Rng::GetRandomInt2(4096);
        Vector3 facing(
            static_cast<float>(facingX) / 4096.0F,
            0.0F,
            static_cast<float>(facingZ) / 4096.0F);
        if (_spawner == nullptr)
        {
            throw System::NullReferenceException();
        }
        if (facing.X == 0.0F && facing.Z == 0.0F)
        {
            facing = _spawner->Transform.Row2().Xyz();
        }
        facing = facing.Normalized(); // the game doesn't do this
        Vector3 up(0.0F, _spawner->Transform.Row1().Y, 0.0F);
        SetTransform(facing, up, _spawner->Position);
        Flags |= EnemyFlags::Visible;
        Flags |= EnemyFlags::OnRadar;
        _health = _healthMax = 12;
        _boundingRadius = 0.25F;
        const EnemySpawnFields00 spawnFields = _spawner->Data.Fields.S00();
        _hurtVolumeInit = CollisionVolume(spawnFields.Volume0);
        SetUpModel(Metadata::EnemyModelNames.at(1));
        _field1A0 = _field1AC = up;
        _angleInc = Fixed::ToFloat(Rng::GetRandomInt2(0x3000)) + 3.0F;
        _angleInc /= 2.0F; // todo: FPS stuff
        _maxAngle = Fixed::ToFloat(Rng::GetRandomInt2(0)) + 40.0F;
        _angleCos = std::cos(DegreesToRadians(_angleInc));
        _homeVolume = CollisionVolume::Move(
            spawnFields.Volume1, _spawner->Data.Header.Position.ToFloatVector());
        _direction = Vector3::Cross(facing, up).Normalized();
    }

    void Enemy01Entity::EnemyProcess()
    {
        ModelInstance& model = _models[0];
        if (((*model.AnimInfo->Flags)[0] & AnimFlags::Ended) != AnimFlags::None
            && (*model.AnimInfo->Index)[0] != 0)
        {
            model.SetAnimation(0);
        }
        _soundSource.PlaySfx(SfxId::ZOOMER_IDLE_LOOP, true);
        if (!_seekingVolume)
        {
            if (_volumeCheckDelay > 0)
            {
                _volumeCheckDelay--;
            }
            else if (!_homeVolume.TestPoint(Position))
            {
                _volumeCheckDelay = 16 * 2; // todo: FPS stuff
                Vector3 facing = WithY(
                    static_cast<Vector3>(Position) - _spawner->Data.Header.Position.ToFloatVector(),
                    0.0F);
                if (facing.X == 0.0F && facing.Z == 0.0F)
                {
                    facing = WithY(FacingVector(), 0.0F);
                    if (facing.X == 0.0F && facing.Z == 0.0F)
                    {
                        facing = WithY(_spawner->Data.Header.FacingVector.ToFloatVector(), 0.0F);
                    }
                }
                _intendedDir = Vector3::Cross(UpVector(), facing).Normalized();
                if (Vector3::Dot(_intendedDir, _direction) < Fixed::ToFloat(-4091))
                {
                    _direction = RotateVector(_direction, UpVector(), 1.0F / 2.0F); // todo: FPS stuff
                }
                _seekingVolume = true;
            }
        }
        Vector3 testPos = static_cast<Vector3>(Position) + ScaleVector(UpVector(), _boundingRadius);
        ManagedArray<Formats::CollisionResult> results(8);
        std::int32_t colCount = Formats::CollisionDetection::CheckInRadius(
            testPos, _boundingRadius, 8, true, Formats::TestFlags::None, _scene, &results);
        if (colCount > 0)
        {
            Vector3 facing = FacingVector();
            Vector3 vec = Vector3::Zero;
            for (std::int32_t i = 0; i < colCount; i++)
            {
                Formats::CollisionResult result = results[static_cast<std::size_t>(i)];
                float dot = Vector3::Dot(testPos, result.Plane.Xyz()) - result.Plane.W;
                float radMinusDot = _boundingRadius - dot;
                if (radMinusDot > 0.0F
                    && radMinusDot < _boundingRadius
                    && result.Field0 == 0
                    && Vector3::Dot(result.Plane.Xyz(), _speed) < 0.0F)
                {
                    // sktodo: convert this to float math
                    std::int32_t rmd = ConvertToInt32Net9(radMinusDot * 4096.0F);
                    Vector3 b(
                        CollisionCorrection(result.Plane.X, rmd),
                        CollisionCorrection(result.Plane.Y, rmd),
                        CollisionCorrection(result.Plane.Z, rmd));
                    testPos = testPos + b;
                }
                float facingDot = Vector3::Dot(result.Plane.Xyz(), facing);
                if (Vector3::Dot(_field1A0, result.Plane.Xyz()) < Fixed::ToFloat(4094)
                    && ((dot < _boundingRadius - Fixed::ToFloat(408)
                            && facingDot >= Fixed::ToFloat(-143))
                        || (dot >= _boundingRadius - Fixed::ToFloat(408)
                            && facingDot <= Fixed::ToFloat(143))))
                {
                    vec = vec + result.Plane.Xyz();
                }
            }
            Vector3 position = testPos - ScaleVector(UpVector(), _boundingRadius);
            if (!Equal(vec, Vector3::Zero))
            {
                vec = vec.Normalized();
                _field1AC = vec;
            }
            Vector3 upVector = UpVector();
            Vector3 upVec = upVector
                + ScaleVector(_field1AC - upVector, Fixed::ToFloat(819) / 2.0F); // todo: FPS stuff
            upVec = upVec.Normalized();
            Vector3 facingVec = Vector3::Cross(upVec, _direction).Normalized();
            SetTransform(facingVec, upVec, position);
        }
        _speed = ScaleVector(UpVector(), Fixed::ToFloat(-245) / 2.0F); // todo: FPS stuff
        if (_seekingVolume)
        {
            _direction = _direction
                + ScaleVector(_intendedDir - _direction, Fixed::ToFloat(819) / 2.0F); // todo: FPS stuff
            _direction = _direction.Normalized();
            if (Vector3::Dot(_intendedDir, _direction) > _angleCos)
            {
                _direction = _intendedDir;
                _seekingVolume = false;
                _curAngle = 0.0F;
            }
        }
        else if (colCount > 0)
        {
            float dot = Vector3::Dot(_field1AC, UpVector());
            if (dot >= Fixed::ToFloat(3712))
            {
                _speed = _speed + ScaleVector(FacingVector(), Fixed::ToFloat(204) / 2.0F); // todo: FPS stuff
                if (dot >= Fixed::ToFloat(4095))
                {
                    _field1A0 = _field1AC;
                    _curAngle += _angleInc;
                    if (_curAngle > _maxAngle || _curAngle < -_maxAngle)
                    {
                        _angleInc *= -1.0F;
                    }
                    Vector3 direction = RotateVector(FacingVector(), UpVector(), _angleInc);
                    _direction = Vector3::Cross(direction, UpVector()).Normalized();
                }
            }
        }
        (void)ContactDamagePlayer(15, true);
    }
}
