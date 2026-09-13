#include "38_CrashPillar.hpp"

#include "../../Metadata/Enemies.hpp"
#include "../../Scene.hpp"
#include "../EnemySpawnEntity.hpp"
#include "../Players/PlayerEntity.hpp"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <utility>

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

        template <typename T>
        [[nodiscard]] T& RequireReference(T* value)
        {
            if (value == nullptr)
            {
                throw System::NullReferenceException();
            }
            return *value;
        }

        [[nodiscard]] Enemy38Entity& RequireEnemy(Enemy38Entity* enemy)
        {
            return RequireReference(enemy);
        }

        [[nodiscard]] PlayerEntity& MainPlayer()
        {
            return RequireReference(PlayerEntity::Main());
        }

        [[nodiscard]] Vector3 WithY(Vector3 value, float y) noexcept
        {
            value.Y = y;
            return value;
        }

        [[nodiscard]] float LengthSquared(Vector3 value) noexcept
        {
            return value.X * value.X + value.Y * value.Y + value.Z * value.Z;
        }

        [[nodiscard]] float RadiansToDegrees(float radians) noexcept
        {
            return radians * (180.0F / 3.14159265358979323846F);
        }

    }

    Enemy38Entity::Enemy38Entity(EnemyInstanceEntityData data,
        Formats::Culling::NodeRef nodeRef, Scene* scene)
        : EnemyInstanceEntity(data, nodeRef, scene),
          _spawner(CastSpawner(data.Spawner))
    {
        auto processes = std::make_shared<ManagedArray<std::function<void()>>>(17);
        (*processes)[0] = [this]() { State00(); };
        (*processes)[1] = [this]() { State01(); };
        (*processes)[2] = [this]() { State02(); };
        (*processes)[3] = [this]() { State03(); };
        (*processes)[4] = [this]() { State04(); };
        (*processes)[5] = [this]() { State05(); };
        (*processes)[6] = [this]() { State06(); };
        (*processes)[7] = [this]() { State07(); };
        (*processes)[8] = [this]() { State08(); };
        (*processes)[9] = [this]() { State09(); };
        (*processes)[10] = [this]() { State10(); };
        (*processes)[11] = [this]() { State11(); };
        (*processes)[12] = [this]() { State12(); };
        (*processes)[13] = [this]() { State13(); };
        (*processes)[14] = [this]() { State14(); };
        (*processes)[15] = [this]() { State15(); };
        (*processes)[16] = [this]() { State16(); };
        _stateProcesses = std::move(processes);
    }

    void Enemy38Entity::EnemyInitialize()
    {
        EnemySpawnEntity& spawner = RequireReference(_spawner);
        const Vector3 facing = spawner.FacingVector();
        SetTransform(facing, Vector3(0.0F, 1.0F, 0.0F), spawner.Position);
        Flags |= EnemyFlags::Visible;
        Flags |= EnemyFlags::Invincible;
        Flags |= EnemyFlags::OnRadar;
        _boundingRadius = 0.5F;
        _health = _healthMax = 150;
        _hurtVolumeInit = CollisionVolume(spawner.Data.Fields.S00().Volume0);
        _volume1 = CollisionVolume::Move(spawner.Data.Fields.S00().Volume2, Position);
        _volume2 = CollisionVolume::Move(spawner.Data.Fields.S00().Volume1, Position);
        _jumpTimer = 5 * 2;
        _delayTimer = 15 * 2;
        _aimSteps = 8 * 2;
        _targetVec = Vector3(1.0F, 0.0F, 0.0F);
        _initalPos = Position;
        _initalFacing = facing;
        if (38 >= static_cast<std::int32_t>(Metadata::EnemyModelNames.size()))
        {
            throw SceneDetail::IndexOutOfRangeException();
        }
        SetUpModel(Metadata::EnemyModelNames[38], 1);
    }

    void Enemy38Entity::EnemyProcess()
    {
        if (_state1 == 3 || _state1 == 4)
        {
            const Vector3 facing = WithY(
                static_cast<Vector3>(MainPlayer().Position)
                    - static_cast<Vector3>(Position),
                0.0F).Normalized();
            SetTransform(facing, Vector3(0.0F, 1.0F, 0.0F), Position);
        }
        if (_state1 == 7 || _state1 == 8)
        {
            _speed.Y -= 0.02F / 4.0F;
        }
        else if (_state1 != 9 && _state1 != 10)
        {
            _speed.Y -= Fixed::ToFloat(100) / 4.0F;
        }
        if (_state1 != 7 && _state1 != 10)
        {
            (void)HandleBlockingCollision(Position, _hurtVolume, true);
        }
        (void)ContactDamagePlayer(_state1 == 10 ? 40U : 10U, true);
        CallStateProcess();
    }

    void Enemy38Entity::State00()
    {
        (void)CallSubroutine<Enemy38Entity>(Metadata::Enemy38Subroutines, this);
    }

    void Enemy38Entity::State01()
    {
        (void)CallSubroutine<Enemy38Entity>(Metadata::Enemy38Subroutines, this);
    }

    void Enemy38Entity::State02()
    {
        (void)CallSubroutine<Enemy38Entity>(Metadata::Enemy38Subroutines, this);
    }

    void Enemy38Entity::State03()
    {
        (void)CallSubroutine<Enemy38Entity>(Metadata::Enemy38Subroutines, this);
    }

    void Enemy38Entity::SetCameraShake(float shakeMax)
    {
        const Vector3 between = static_cast<Vector3>(MainPlayer().Position)
            - static_cast<Vector3>(Position);
        const float shake = std::min(1.0F / LengthSquared(between) * 3.0F, shakeMax);
        MainPlayer().CameraInfo.SetShake(shake);
    }

    void Enemy38Entity::DoThing(float shakeMax)
    {
        const std::shared_ptr<AnimationInfo> animInfo = _models[0].AnimInfo;
        AnimationInfo& info = RequireReference(animInfo.get());
        ManagedArray<std::int32_t>& frames = RequireReference(info.Frame.get());
        const std::int32_t frame = frames[0];
        if (frame >= 17)
        {
            _speed.X = 0.0F;
            _speed.Z = 0.0F;
        }
        else
        {
            if (frame == 12 && RequireReference(_scene).FrameCount() % 2 == 0)
            {
                SetCameraShake(shakeMax);
            }
            const Vector3 facing = FacingVector();
            _speed.X = facing.X * 0.1465F / 2.0F;
            _speed.Z = facing.Z * 0.1465F / 2.0F;
        }
    }

    void Enemy38Entity::State04()
    {
        DoThing(0.08F);
        (void)CallSubroutine<Enemy38Entity>(Metadata::Enemy38Subroutines, this);
    }

    void Enemy38Entity::State05()
    {
        (void)CallSubroutine<Enemy38Entity>(Metadata::Enemy38Subroutines, this);
    }

    void Enemy38Entity::State06()
    {
        if (_delayTimer == 10 * 2)
        {
            _models[0].SetAnimation(6, AnimFlags::NoLoop);
        }
        (void)CallSubroutine<Enemy38Entity>(Metadata::Enemy38Subroutines, this);
    }

    void Enemy38Entity::State07()
    {
        const std::shared_ptr<AnimationInfo> animInfo = _models[0].AnimInfo;
        AnimationInfo& info = RequireReference(animInfo.get());
        ManagedArray<std::int32_t>& indices = RequireReference(info.Index.get());
        if (indices[0] == 6)
        {
            ManagedArray<AnimFlags>& flags = RequireReference(info.Flags.get());
            if ((flags[0] & AnimFlags::Ended) != AnimFlags::None)
            {
                _models[0].SetAnimation(4);
            }
        }
        (void)CallSubroutine<Enemy38Entity>(Metadata::Enemy38Subroutines, this);
    }

    void Enemy38Entity::State08()
    {
        State07();
    }

    void Enemy38Entity::State09()
    {
        (void)CallSubroutine<Enemy38Entity>(Metadata::Enemy38Subroutines, this);
    }

    void Enemy38Entity::State10()
    {
        (void)CallSubroutine<Enemy38Entity>(Metadata::Enemy38Subroutines, this);
    }

    void Enemy38Entity::State11()
    {
        (void)CallSubroutine<Enemy38Entity>(Metadata::Enemy38Subroutines, this);
    }

    void Enemy38Entity::FaceInitialPosition()
    {
        if (std::fabs(Position.X - _initalPos.X) >= 1.0F / 4096.0F
            || std::fabs(Position.Z - _initalPos.Z) >= 1.0F / 4096.0F)
        {
            const Vector3 facing = WithY(
                _initalPos - static_cast<Vector3>(Position),
                FacingVector().Y).Normalized();
            SetTransform(facing, Vector3(0.0F, 1.0F, 0.0F), Position);
        }
    }

    void Enemy38Entity::State12()
    {
        const Vector3 playerPos = MainPlayer().Position;
        if (!_volume2.TestPoint(playerPos) || !_volume1.TestPoint(playerPos))
        {
            FaceInitialPosition();
        }
        (void)CallSubroutine<Enemy38Entity>(Metadata::Enemy38Subroutines, this);
    }

    void Enemy38Entity::State13()
    {
        FaceInitialPosition();
        DoThing(0.08F);
        (void)CallSubroutine<Enemy38Entity>(Metadata::Enemy38Subroutines, this);
    }

    void Enemy38Entity::State14()
    {
        (void)CallSubroutine<Enemy38Entity>(Metadata::Enemy38Subroutines, this);
    }

    void Enemy38Entity::State15()
    {
        (void)CallSubroutine<Enemy38Entity>(Metadata::Enemy38Subroutines, this);
    }

    void Enemy38Entity::State16()
    {
        (void)CallSubroutine<Enemy38Entity>(Metadata::Enemy38Subroutines, this);
    }

    bool Enemy38Entity::Behavior00()
    {
        if (_aimSteps > 0)
        {
            --_aimSteps;
            return false;
        }
        _speed.Y = Fixed::ToFloat(-3000) / 2.0F;
        _aimSteps = 8 * 2;
        _models[0].SetAnimation(3);
        return true;
    }

    bool Enemy38Entity::Behavior01()
    {
        const std::shared_ptr<AnimationInfo> animInfo = _models[0].AnimInfo;
        AnimationInfo& info = RequireReference(animInfo.get());
        ManagedArray<AnimFlags>& flags = RequireReference(info.Flags.get());
        if ((flags[0] & AnimFlags::Ended) == AnimFlags::None)
        {
            return false;
        }
        _targetVec = WithY(
            static_cast<Vector3>(MainPlayer().Position)
                - static_cast<Vector3>(Position),
            0.0F).Normalized();
        const float angle = RadiansToDegrees(
            std::acos(Vector3::Dot(FacingVector(), _targetVec)));
        _aimSteps = 10 * 2;
        _aimAngleStep = angle / static_cast<float>(_aimSteps);
        _models[0].SetAnimation(7, AnimFlags::NoLoop);
        return true;
    }

    bool Enemy38Entity::Behavior02()
    {
        const std::shared_ptr<AnimationInfo> animInfo = _models[0].AnimInfo;
        AnimationInfo& info = RequireReference(animInfo.get());
        ManagedArray<AnimFlags>& flags = RequireReference(info.Flags.get());
        if ((flags[0] & AnimFlags::Ended) == AnimFlags::None)
        {
            return false;
        }
        _models[0].SetAnimation(1);
        return true;
    }

    bool Enemy38Entity::Behavior03()
    {
        const std::shared_ptr<AnimationInfo> animInfo = _models[0].AnimInfo;
        AnimationInfo& info = RequireReference(animInfo.get());
        ManagedArray<AnimFlags>& flags = RequireReference(info.Flags.get());
        return (flags[0] & AnimFlags::Ended) != AnimFlags::None;
    }

    bool Enemy38Entity::Behavior04()
    {
        if (!SeekTargetFacing(
                _targetVec, Vector3(0.0F, 1.0F, 0.0F),
                _aimSteps, _aimAngleStep))
        {
            return false;
        }
        _models[0].SetAnimation(
            0, AnimFlags::NoLoop | AnimFlags::Reverse);
        _soundSource.PlaySfx(SfxId::STATUE_TURN_SCR);
        return true;
    }

    bool Enemy38Entity::Behavior05()
    {
        if (!_volume1.TestPoint(MainPlayer().Position))
        {
            return false;
        }
        _models[0].SetAnimation(0, AnimFlags::NoLoop);
        _soundSource.PlaySfx(SfxId::STATUE_ACTIVATE_SCR);
        return true;
    }

    bool Enemy38Entity::Behavior06()
    {
        if (!HandleBlockingCollision(Position, _hurtVolume, true))
        {
            return false;
        }
        _speed = Vector3::Zero;
        Flags |= EnemyFlags::Invincible;
        SetCameraShake(0.7F);
        _models[0].SetAnimation(5, AnimFlags::NoLoop);
        return true;
    }

    bool Enemy38Entity::Behavior07()
    {
        if (!SeekTargetFacing(
                _targetVec, Vector3(0.0F, 1.0F, 0.0F),
                _aimSteps, _aimAngleStep))
        {
            return false;
        }
        _soundSource.PlaySfx(SfxId::STATUE_TURN_SCR);
        return true;
    }

    bool Enemy38Entity::Behavior08()
    {
        if (Position.Y < _jumpHeight)
        {
            return false;
        }
        _speed = Vector3::Zero;
        return true;
    }

    bool Enemy38Entity::Behavior09()
    {
        if (_jumpTimer > 0)
        {
            --_jumpTimer;
            return false;
        }
        Flags &= ~EnemyFlags::Invincible;
        _jumpTimer = 5 * 2;
        return true;
    }

    bool Enemy38Entity::Behavior10()
    {
        if (_delayTimer > 0)
        {
            --_delayTimer;
            return false;
        }
        const Vector3 between = static_cast<Vector3>(MainPlayer().Position)
            - static_cast<Vector3>(Position);
        const float factor = std::sqrt(
            Fixed::ToFloat(80) / Fixed::ToFloat(22937));
        _speed = Vector3(
            between.X * factor,
            std::sqrt(Fixed::ToFloat(448)),
            between.Z * factor);
        _speed = Vector3(
            _speed.X / 2.0F,
            _speed.Y / 2.0F,
            _speed.Z / 2.0F);
        _jumpHeight = Position.Y + 2.8F;
        _delayTimer = 15 * 2;
        _aimSteps = 8 * 2;
        _soundSource.PlaySfx(SfxId::STATUE_ATTACK_SCR);
        return true;
    }

    bool Enemy38Entity::Behavior11()
    {
        return SeekTargetFacing(
            _targetVec, Vector3(0.0F, 1.0F, 0.0F),
            _aimSteps, _aimAngleStep);
    }

    bool Enemy38Entity::Behavior12()
    {
        const std::shared_ptr<AnimationInfo> animInfo = _models[0].AnimInfo;
        AnimationInfo& info = RequireReference(animInfo.get());
        ManagedArray<AnimFlags>& flags = RequireReference(info.Flags.get());
        if ((flags[0] & AnimFlags::Ended) == AnimFlags::None)
        {
            return false;
        }
        _models[0].SetAnimation(2, AnimFlags::NoLoop);
        _soundSource.PlaySfx(SfxId::STATUE_HOP_SCR);
        return true;
    }

    bool Enemy38Entity::Behavior13()
    {
        if (!SeekTargetFacing(
                _targetVec, Vector3(0.0F, 1.0F, 0.0F),
                _aimSteps, _aimAngleStep))
        {
            return false;
        }
        _models[0].SetAnimation(2, AnimFlags::NoLoop);
        _soundSource.PlaySfx(SfxId::STATUE_HOP_SCR);
        return true;
    }

    bool Enemy38Entity::Behavior14()
    {
        if (_volume2.TestPoint(MainPlayer().Position))
        {
            return false;
        }
        _speed = Vector3::Zero;
        _targetVec = WithY(
            _initalPos - static_cast<Vector3>(Position),
            0.0F).Normalized();
        const float angle = RadiansToDegrees(
            std::acos(Vector3::Dot(FacingVector(), _targetVec)));
        _aimSteps = 10 * 2;
        _aimAngleStep = angle / static_cast<float>(_aimSteps);
        _models[0].SetAnimation(7, AnimFlags::NoLoop);
        return true;
    }

    bool Enemy38Entity::Behavior15()
    {
        const Vector3 between = static_cast<Vector3>(Position)
            - static_cast<Vector3>(MainPlayer().Position);
        if (LengthSquared(between) >= 5.0F * 5.0F)
        {
            return false;
        }
        _delayTimer = 40 * 2;
        return true;
    }

    bool Enemy38Entity::Behavior16()
    {
        if (_delayTimer > 0)
        {
            --_delayTimer;
            return false;
        }
        _delayTimer = 15 * 2;
        _models[0].SetAnimation(2, AnimFlags::NoLoop);
        _soundSource.PlaySfx(SfxId::STATUE_HOP_SCR);
        return true;
    }

    bool Enemy38Entity::Behavior17()
    {
        const Vector3 between = static_cast<Vector3>(Position) - _initalPos;
        if (LengthSquared(between) >= 1.0F)
        {
            return false;
        }
        _targetVec = _initalFacing;
        const float angle = RadiansToDegrees(
            std::acos(Vector3::Dot(FacingVector(), _targetVec)));
        _aimSteps = 10 * 2;
        _aimAngleStep = angle / static_cast<float>(_aimSteps);
        return true;
    }

    bool Enemy38Entity::Behavior18()
    {
        const Vector3 playerPos = MainPlayer().Position;
        if (!_volume2.TestPoint(playerPos) || !_volume1.TestPoint(playerPos))
        {
            return false;
        }
        _targetVec = WithY(
            static_cast<Vector3>(MainPlayer().Position)
                - static_cast<Vector3>(Position),
            0.0F).Normalized();
        const float angle = RadiansToDegrees(
            std::acos(Vector3::Dot(FacingVector(), _targetVec)));
        _aimSteps = 10 * 2;
        _aimAngleStep = angle / static_cast<float>(_aimSteps);
        _models[0].SetAnimation(7, AnimFlags::NoLoop);
        _delayTimer = 15 * 2;
        return true;
    }

    bool Enemy38Entity::Behavior00(Enemy38Entity* enemy)
    {
        return RequireEnemy(enemy).Behavior00();
    }

    bool Enemy38Entity::Behavior01(Enemy38Entity* enemy)
    {
        return RequireEnemy(enemy).Behavior01();
    }

    bool Enemy38Entity::Behavior02(Enemy38Entity* enemy)
    {
        return RequireEnemy(enemy).Behavior02();
    }

    bool Enemy38Entity::Behavior03(Enemy38Entity* enemy)
    {
        return RequireEnemy(enemy).Behavior03();
    }

    bool Enemy38Entity::Behavior04(Enemy38Entity* enemy)
    {
        return RequireEnemy(enemy).Behavior04();
    }

    bool Enemy38Entity::Behavior05(Enemy38Entity* enemy)
    {
        return RequireEnemy(enemy).Behavior05();
    }

    bool Enemy38Entity::Behavior06(Enemy38Entity* enemy)
    {
        return RequireEnemy(enemy).Behavior06();
    }

    bool Enemy38Entity::Behavior07(Enemy38Entity* enemy)
    {
        return RequireEnemy(enemy).Behavior07();
    }

    bool Enemy38Entity::Behavior08(Enemy38Entity* enemy)
    {
        return RequireEnemy(enemy).Behavior08();
    }

    bool Enemy38Entity::Behavior09(Enemy38Entity* enemy)
    {
        return RequireEnemy(enemy).Behavior09();
    }

    bool Enemy38Entity::Behavior10(Enemy38Entity* enemy)
    {
        return RequireEnemy(enemy).Behavior10();
    }

    bool Enemy38Entity::Behavior11(Enemy38Entity* enemy)
    {
        return RequireEnemy(enemy).Behavior11();
    }

    bool Enemy38Entity::Behavior12(Enemy38Entity* enemy)
    {
        return RequireEnemy(enemy).Behavior12();
    }

    bool Enemy38Entity::Behavior13(Enemy38Entity* enemy)
    {
        return RequireEnemy(enemy).Behavior13();
    }

    bool Enemy38Entity::Behavior14(Enemy38Entity* enemy)
    {
        return RequireEnemy(enemy).Behavior14();
    }

    bool Enemy38Entity::Behavior15(Enemy38Entity* enemy)
    {
        return RequireEnemy(enemy).Behavior15();
    }

    bool Enemy38Entity::Behavior16(Enemy38Entity* enemy)
    {
        return RequireEnemy(enemy).Behavior16();
    }

    bool Enemy38Entity::Behavior17(Enemy38Entity* enemy)
    {
        return RequireEnemy(enemy).Behavior17();
    }

    bool Enemy38Entity::Behavior18(Enemy38Entity* enemy)
    {
        return RequireEnemy(enemy).Behavior18();
    }
}
