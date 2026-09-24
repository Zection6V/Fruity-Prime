#include "03_Petrasyl1.hpp"

#include "../../Metadata/Enemies.hpp"
#include "../../Scene.hpp"
#include "../../Utility/Rng.hpp"
#include "../EnemySpawnEntity.hpp"
#include "../Players/PlayerEntity.hpp"
#include "../../NativeRuntime/System/Managed.hpp"
#include "../../NativeRuntime/OpenTK/Mathematics.hpp"
#include "../../Formats/Types.hpp"

#include <bit>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <limits>
#include <memory>
#include <utility>
#include <vector>

using ::MphRead::NativeRuntime::ConvertToInt32Net9;
using ::MphRead::NativeRuntime::ManagedAs;
using ::MphRead::NativeRuntime::RequireReference;
using ::MphRead::NativeRuntime::UInt32ToInt32;
using ::MphRead::NativeRuntime::UncheckedDecrement;
using ::MphRead::NativeRuntime::UncheckedMultiply;
using ::OpenTK::Mathematics::Equal;
using ::OpenTK::Mathematics::LengthSquared;
using ::OpenTK::Mathematics::MathHelper::DegreesToRadians;
using ::OpenTK::Mathematics::Negate;
using ::OpenTK::Mathematics::ScaleVector;
using ::OpenTK::Mathematics::WithY;

namespace MphRead::Entities::Enemies
{
    namespace
    {
        using OpenTK::Mathematics::Vector3;

    }
}

namespace MphRead::Entities::Enemies
{
    Enemy03Entity::Enemy03Entity(EnemyInstanceEntityData data,
        Formats::Culling::NodeRef nodeRef, Scene* scene)
        : EnemyInstanceEntity(data, nodeRef, scene),
          _spawner(ManagedAs<EnemySpawnEntity>(data.Spawner))
    {
        assert(_spawner != nullptr);
        auto processes = std::make_shared<ManagedArray<std::function<void()>>>(3);
        (*processes)[0] = [this]() { State00(); };
        (*processes)[1] = [this]() { State01(); };
        (*processes)[2] = [this]() { State02(); };
        _stateProcesses = std::move(processes);
    }

    void Enemy03Entity::EnemyInitialize()
    {
        _health = _healthMax = 8;
        Flags |= EnemyFlags::Visible;
        Flags |= EnemyFlags::NoHomingNc;
        Flags |= EnemyFlags::Invincible;
        Flags |= EnemyFlags::OnRadar;
        _boundingRadius = 0.5F;
        if (_spawner == nullptr)
        {
            throw System::NullReferenceException();
        }
        _hurtVolumeInit = CollisionVolume(_spawner->Data.Fields.S03().Volume0);
        SetUpModel(Metadata::EnemyModelNames.at(3));
        _models[0].SetAnimation(
            9, 0, SetFlags::Texture | SetFlags::Material | SetFlags::Node, AnimFlags::NoLoop);
        _models[0].SetAnimation(2, 1, SetFlags::Texcoord);

        _idleRangeX = _spawner->Data.Fields.S03().IdleRange.X.FloatValue();
        _idleRangeZ = _spawner->Data.Fields.S03().IdleRange.Z.FloatValue();
        Vector3 facing = WithY(_spawner->Data.Fields.S03().Facing.ToFloatVector(), 0.0F).Normalized();
        Vector3 position = _spawner->Data.Fields.S03().Position.ToFloatVector()
            + _spawner->Data.Header.Position.ToFloatVector();
        position.Y += Fixed::ToFloat(5461);
        _initialPos = position;
        _idleLimits = Vector3(
            position.X + facing.X * _idleRangeZ - facing.Z * _idleRangeX,
            position.Y,
            position.Z + facing.Z * _idleRangeZ + facing.X * _idleRangeX);
        facing.X *= -1.0F;
        facing.Z *= -1.0F;
        SetTransform(facing, Vector3(0.0F, 1.0F, 0.0F), position);
        _field194 = facing;
        _field1A0 = facing;
        _bobOffset = Fixed::ToFloat(Rng::GetRandomInt2(0x1800) + 2048U) / 2.0F;
        _bobSpeed = Fixed::ToFloat(Rng::GetRandomInt2(0x6000)) + 1.0F;
        _field170 = _field172 = 20 * 2;
        UpdateState();
    }

    void Enemy03Entity::UpdateState()
    {
        if (_state2 == 0)
        {
            _models[0].SetAnimation(
                3, 0, SetFlags::Texture | SetFlags::Material | SetFlags::Node, AnimFlags::NoLoop);
            if (_teleportInAtInitial)
            {
                Position = Vector3(_initialPos.X, Position.Y, _initialPos.Z);
                _teleportInAtInitial = false;
            }
            else
            {
                Position = Vector3(_idleLimits.X, Position.Y, _idleLimits.Z);
                _teleportInAtInitial = true;
            }
            _field194 = Negate(_field194).Normalized();
            SetTransform(_field194, UpVector(), Position);
            _field170 = 20 * 2;
            _soundSource.PlaySfx(SfxId::MOCHTROID_TELEPORT_IN);
        }
        else if (_state2 == 1)
        {
            _models[0].SetAnimation(
                0, 0, SetFlags::Texture | SetFlags::Material | SetFlags::Node);
            Flags &= ~EnemyFlags::NoHomingNc;
            Flags &= ~EnemyFlags::Invincible;
            _field18C = UncheckedMultiply(ConvertToInt32Net9(_idleRangeZ / 0.7F), 2);
            _speed = WithY(ScaleVector(_field194, 0.7F), 0.0F);
            _speed.X /= 2.0F;
            _speed.Y /= 2.0F;
            _speed.Z /= 2.0F;
        }
        else if (_state2 == 2)
        {
            _models[0].SetAnimation(
                4, 0, SetFlags::Texture | SetFlags::Material | SetFlags::Node, AnimFlags::NoLoop);
            Flags |= EnemyFlags::NoHomingNc;
            Flags |= EnemyFlags::Invincible;
            _speed = Vector3::Zero;
            _field172 = 20 * 2;
            _soundSource.PlaySfx(SfxId::MOCHTROID_TELEPORT_OUT);
        }
    }

    void Enemy03Entity::EnemyProcess()
    {
        CallStateProcess();
    }

    void Enemy03Entity::State00()
    {
        if (CallSubroutine<Enemy03Entity>(Metadata::Enemy03Subroutines, this))
        {
            UpdateState();
        }
    }

    void Enemy03Entity::State01()
    {
        _soundSource.PlaySfx(SfxId::MOCHTROID_FLY, true);
        _bobAngle += _bobSpeed / 2.0F;
        if (_bobAngle >= 360.0F)
        {
            _bobAngle -= 360.0F;
        }
        float sin = std::sin(DegreesToRadians(_bobAngle));
        _speed.Y = _initialPos.Y + sin * _bobOffset - Position.Y;
        _speed.Y /= 2.0F;

        PlayerEntity& hitPlayer = RequireReference(PlayerEntity::Main());
        const std::int32_t slotIndex = hitPlayer.SlotIndex();
        if (slotIndex < 0 || static_cast<std::size_t>(slotIndex) >= HitPlayers.size())
        {
            throw SceneDetail::IndexOutOfRangeException();
        }
        if (HitPlayers[static_cast<std::size_t>(slotIndex)])
        {
            PlayerEntity& damagePlayer = RequireReference(PlayerEntity::Main());
            Vector3 damageFacing = FacingVector();
            damagePlayer.TakeDamage(12, DamageFlags::None, damageFacing, this);
        }

        Vector3 playerPosition = RequireReference(PlayerEntity::Main()).Position;
        Vector3 currentPosition = Position;
        Vector3 between = playerPosition - currentPosition;
        if (LengthSquared(between) >= 7.0F * 7.0F)
        {
            _field1A0 = WithY(_field194, 0.0F).Normalized();
        }
        else
        {
            _field1A0 = WithY(between, 0.0F).Normalized();
        }

        Vector3 prevFacing = FacingVector();
        Vector3 newFacing = prevFacing;
        newFacing.X += (_field1A0.X - prevFacing.X) / 8.0F / 2.0F;
        newFacing.Z += (_field1A0.Z - prevFacing.Z) / 8.0F / 2.0F;
        if (newFacing.X == 0.0F && newFacing.Z == 0.0F)
        {
            newFacing = prevFacing;
        }
        assert(!Equal(newFacing, Vector3::Zero));
        newFacing = newFacing.Normalized();
        if (std::fabs(newFacing.X - prevFacing.X) < 1.0F / 4096.0F
            && std::fabs(newFacing.Z - prevFacing.Z) < 1.0F / 4096.0F)
        {
            newFacing.X += 0.125F / 2.0F;
            newFacing.Z -= 0.125F / 2.0F;
            if (newFacing.X == 0.0F && newFacing.Z == 0.0F)
            {
                newFacing.X += 0.125F / 2.0F;
                newFacing.Z -= 0.125F / 2.0F;
            }
            newFacing = newFacing.Normalized();
        }
        SetTransform(newFacing, UpVector(), Position);

        const std::shared_ptr<AnimationInfo> animInfo = _models[0].AnimInfo;
        if (animInfo == nullptr)
        {
            throw System::NullReferenceException();
        }
        if (animInfo->Index == nullptr)
        {
            throw System::NullReferenceException();
        }
        if ((*animInfo->Index)[0] != 0)
        {
            if (animInfo->Flags == nullptr)
            {
                throw System::NullReferenceException();
            }
            if (((*animInfo->Flags)[0] & AnimFlags::Ended) != AnimFlags::None)
            {
                _models[0].SetAnimation(
                    0, 0, SetFlags::Texture | SetFlags::Material | SetFlags::Node);
            }
        }

        if (CallSubroutine<Enemy03Entity>(Metadata::Enemy03Subroutines, this))
        {
            _soundSource.StopSfx(SfxId::MOCHTROID_FLY);
            UpdateState();
        }
    }

    void Enemy03Entity::State02()
    {
        State00();
    }

    bool Enemy03Entity::Behavior00()
    {
        if (_field172 == 0)
        {
            return true;
        }
        _field172--;
        return false;
    }

    bool Enemy03Entity::Behavior01()
    {
        if (_field18C == 0)
        {
            return true;
        }
        _field18C = UncheckedDecrement(_field18C);
        return false;
    }

    bool Enemy03Entity::Behavior02()
    {
        if (_field170 == 0)
        {
            return true;
        }
        _field170--;
        return false;
    }

    bool Enemy03Entity::Behavior00(Enemy03Entity* enemy)
    {
        return RequireReference(enemy).Behavior00();
    }

    bool Enemy03Entity::Behavior01(Enemy03Entity* enemy)
    {
        return RequireReference(enemy).Behavior01();
    }

    bool Enemy03Entity::Behavior02(Enemy03Entity* enemy)
    {
        return RequireReference(enemy).Behavior02();
    }
}
