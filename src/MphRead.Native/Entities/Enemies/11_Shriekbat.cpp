#include "11_Shriekbat.hpp"

#include "../../Formats/Effects.hpp"
#include "../../Metadata/Enemies.hpp"
#include "../../Scene.hpp"
#include "../EnemySpawnEntity.hpp"
#include "../Players/PlayerEntity.hpp"
#include "../../NativeRuntime/System/Managed.hpp"
#include "../../Formats/Types.hpp"

#include <bit>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <functional>
#include <limits>
#include <memory>
#include <utility>
#include <vector>

using ::MphRead::NativeRuntime::ConvertToInt32Net9;
using ::MphRead::NativeRuntime::RequireReference;
using ::MphRead::NativeRuntime::UInt32ToInt32;
using ::MphRead::NativeRuntime::UncheckedAdd;
using ::MphRead::NativeRuntime::UncheckedMultiply;
using ::OpenTK::Mathematics::Length;
using ::OpenTK::Mathematics::ScaleVector;

namespace MphRead::Entities::Enemies
{
    namespace
    {
        using OpenTK::Mathematics::Matrix4;
        using OpenTK::Mathematics::Vector3;

        EnemySpawnEntity* CastSpawner(EntityBase* spawner) noexcept
        {
            EnemySpawnEntity* typedSpawner = dynamic_cast<EnemySpawnEntity*>(spawner);
            assert(typedSpawner != nullptr);
            return typedSpawner;
        }

        [[nodiscard]] Enemy11Entity& RequireEnemy(Enemy11Entity* enemy)
        {
            return RequireReference(enemy);
        }

        [[nodiscard]] PlayerEntity& MainPlayer()
        {
            const std::shared_ptr<PlayerEntity> player = PlayerEntity::Main();
            if (!player)
            {
                throw System::NullReferenceException();
            }
            return *player;
        }

    }
}

namespace MphRead::Entities::Enemies
{
    Enemy11Entity::Enemy11Entity(EnemyInstanceEntityData data,
        Formats::Culling::NodeRef nodeRef, Scene* scene)
        : EnemyInstanceEntity(data, nodeRef, scene),
          _spawner(CastSpawner(data.Spawner))
    {
        auto processes = std::make_shared<ManagedArray<std::function<void()>>>(5);
        (*processes)[0] = [this]() { State0(); };
        (*processes)[1] = [this]() { State1(); };
        (*processes)[2] = [this]() { State2(); };
        (*processes)[3] = [this]() { State3(); };
        (*processes)[4] = [this]() { State4(); };
        _stateProcesses = std::move(processes);
    }

    void Enemy11Entity::EnemyInitialize()
    {
        _health = _healthMax = 12;
        Flags |= EnemyFlags::Visible;
        Flags |= EnemyFlags::OnRadar;

        const Vector3 position
            = RequireReference(_spawner).Data.Header.Position.ToFloatVector();
        SetTransform(
            (static_cast<Vector3>(MainPlayer().Position) - position).Normalized(),
            Vector3(0.0F, 1.0F, 0.0F),
            position);
        _boundingRadius = 1.0F;
        _hurtVolumeInit = CollisionVolume(_spawner->Data.Fields.S02().Volume0);
        _rangeVolume = CollisionVolume::Move(
            _spawner->Data.Fields.S02().Volume1, position);
        _activeVolume = CollisionVolume::Move(
            _spawner->Data.Fields.S02().Volume2, position);
        _targetPos = position + _spawner->Data.Fields.S02().PathVector.ToFloatVector();

        if (11 >= static_cast<std::int32_t>(Metadata::EnemyModelNames.size()))
        {
            throw SceneDetail::IndexOutOfRangeException();
        }
        SetUpModel(Metadata::EnemyModelNames[11], 4);
    }

    void Enemy11Entity::Die()
    {
        TakeDamage(100, nullptr);
        if (_effect)
        {
            RequireReference(_scene).UnlinkEffectEntry(_effect);
            _effect.reset();
        }
    }

    void Enemy11Entity::EnemyProcess()
    {
        const Vector3 facing
            = (static_cast<Vector3>(MainPlayer().Position)
                - static_cast<Vector3>(Position)).Normalized();
        SetTransform(facing, Vector3(0.0F, 1.0F, 0.0F), Position);
        if (_effect)
        {
            _effect->Transform(
                static_cast<Vector3>(Position),
                static_cast<Matrix4>(Transform));
        }
        if (ContactDamagePlayer(20, false))
        {
            Die();
        }
        if (_state1 == 4 && HandleBlockingCollision(Position, _hurtVolume, true))
        {
            Die();
        }
        CallStateProcess();
    }

    void Enemy11Entity::State0()
    {
        (void)CallSubroutine<Enemy11Entity>(Metadata::Enemy11Subroutines, this);
    }

    void Enemy11Entity::State1()
    {
        State0();
    }

    void Enemy11Entity::State2()
    {
        State0();
    }

    void Enemy11Entity::State3()
    {
        State0();
    }

    void Enemy11Entity::State4()
    {
        State0();
    }

    bool Enemy11Entity::Behavior00()
    {
        return false;
    }

    bool Enemy11Entity::Behavior01()
    {
        if (_moveTimer > 0)
        {
            --_moveTimer;
            return false;
        }

        const Vector3 playerFacing = MainPlayer().FacingVector();
        const Vector3 playerPosition = static_cast<Vector3>(MainPlayer().Position);
        Vector3 target(
            -playerFacing.X + playerPosition.X,
            -playerFacing.Y + playerPosition.Y,
            -playerFacing.Z + playerPosition.Z);
        target.Y = static_cast<Vector3>(MainPlayer().Position).Y + 0.5F;
        _speed = target - static_cast<Vector3>(Position);
        const float mag = Length(_speed);
        _moveTimer = UncheckedAdd(ConvertToInt32Net9(mag / 0.6F), 1);
        _moveTimer = UncheckedMultiply(_moveTimer, 2);
        _speed = ScaleVector(_speed, 0.6F / mag);
        _speed.X /= 2.0F;
        _speed.Y /= 2.0F;
        _speed.Z /= 2.0F;
        _soundSource.PlaySfx(SfxId::SHRIEKBAT_ATTACK);
        _models[0].SetAnimation(1);
        return true;
    }

    bool Enemy11Entity::Behavior02()
    {
        if (_moveTimer > 0)
        {
            --_moveTimer;
            return false;
        }
        _models[0].SetAnimation(3);
        _moveTimer = 20 * 2;
        _speed = Vector3::Zero;
        return true;
    }

    bool Enemy11Entity::Behavior03()
    {
        if (!_activeVolume.TestPoint(static_cast<Vector3>(MainPlayer().Position)))
        {
            return false;
        }
        _effect = RequireReference(_scene).SpawnEffectGetEntry(
            29,
            Vector3(1.0F, 0.0F, 0.0F),
            Vector3(0.0F, 1.0F, 0.0F),
            Position);
        if (_effect)
        {
            _effect->SetElementExtension(true);
        }
        _speed = _targetPos - static_cast<Vector3>(Position);
        const float mag = Length(_speed);
        _moveTimer = UncheckedAdd(ConvertToInt32Net9(mag / 0.3F), 1);
        _moveTimer = UncheckedMultiply(_moveTimer, 2);
        _speed = ScaleVector(_speed, 0.3F / mag);
        _speed.X /= 2.0F;
        _speed.Y /= 2.0F;
        _speed.Z /= 2.0F;
        _models[0].SetAnimation(2);
        _soundSource.PlaySfx(SfxId::SHRIEKBAT_PRE_ATTACK_SCR);
        return true;
    }

    bool Enemy11Entity::Behavior04()
    {
        if (!_rangeVolume.TestPoint(static_cast<Vector3>(MainPlayer().Position)))
        {
            return false;
        }
        _models[0].SetAnimation(0);
        return true;
    }

    void Enemy11Entity::Destroy()
    {
        if (_effect)
        {
            RequireReference(_scene).UnlinkEffectEntry(_effect);
            _effect.reset();
        }
        EnemyInstanceEntity::Destroy();
    }

    bool Enemy11Entity::Behavior00(Enemy11Entity* enemy)
    {
        return RequireEnemy(enemy).Behavior00();
    }

    bool Enemy11Entity::Behavior01(Enemy11Entity* enemy)
    {
        return RequireEnemy(enemy).Behavior01();
    }

    bool Enemy11Entity::Behavior02(Enemy11Entity* enemy)
    {
        return RequireEnemy(enemy).Behavior02();
    }

    bool Enemy11Entity::Behavior03(Enemy11Entity* enemy)
    {
        return RequireEnemy(enemy).Behavior03();
    }

    bool Enemy11Entity::Behavior04(Enemy11Entity* enemy)
    {
        return RequireEnemy(enemy).Behavior04();
    }
}
