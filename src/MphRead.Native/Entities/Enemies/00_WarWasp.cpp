#include "00_WarWasp.hpp"

#include "../../Formats/CollisionDetection.hpp"
#include "../../Metadata/Enemies.hpp"
#include "../EnemySpawnEntity.hpp"
#include "../Players/PlayerEntity.hpp"

#include <cassert>
#include <cmath>
#include <cstddef>
#include <functional>
#include <memory>
#include <optional>
#include <utility>

namespace MphRead::Entities::Enemies
{
    namespace
    {
        using OpenTK::Mathematics::Vector3;

        EnemySpawnEntity* CastSpawner(EntityBase* spawner)
        {
            EnemySpawnEntity* typedSpawner = dynamic_cast<EnemySpawnEntity*>(spawner);
            assert(typedSpawner != nullptr);
            if (typedSpawner == nullptr)
            {
                throw System::NullReferenceException();
            }
            return typedSpawner;
        }

        [[nodiscard]] bool Equal(Vector3 left, Vector3 right) noexcept
        {
            return left.X == right.X && left.Y == right.Y && left.Z == right.Z;
        }

        [[nodiscard]] float Length(Vector3 value)
        {
            return std::sqrt((value.X * value.X) + (value.Y * value.Y) + (value.Z * value.Z));
        }

        [[nodiscard]] Vector3 Scale(Vector3 value, float scale) noexcept
        {
            return Vector3(value.X * scale, value.Y * scale, value.Z * scale);
        }

        [[nodiscard]] PlayerEntity& MainPlayer()
        {
            PlayerEntity* player = PlayerEntity::Main();
            if (player == nullptr)
            {
                throw System::NullReferenceException();
            }
            return *player;
        }

        [[nodiscard]] Enemy00Entity& RequireEnemy(Enemy00Entity* enemy)
        {
            if (enemy == nullptr)
            {
                throw System::NullReferenceException();
            }
            return *enemy;
        }

    }

    Enemy00Entity::Enemy00Entity(EnemyInstanceEntityData data,
        Formats::Culling::NodeRef nodeRef, Scene* scene)
        : EnemyInstanceEntity(data, nodeRef, scene),
          _spawner(CastSpawner(data.Spawner))
    {
        auto processes = std::make_shared<ManagedArray<std::function<void()>>>(7);
        (*processes)[0] = [this]() { State0(); };
        (*processes)[1] = [this]() { State1(); };
        (*processes)[2] = [this]() { State2(); };
        (*processes)[3] = [this]() { State3(); };
        (*processes)[4] = [this]() { State4(); };
        (*processes)[5] = [this]() { State5(); };
        (*processes)[6] = [this]() { State6(); };
        _stateProcesses = std::move(processes);
    }

    void Enemy00Entity::EnemyInitialize()
    {
        Vector3 facing = _spawner->Data.Header.FacingVector.ToFloatVector();
        Vector3 up = FixParallelVectors(facing, Vector3(0.0F, 1.0F, 0.0F));
        SetTransform(facing, up, _spawner->Data.Header.Position.ToFloatVector());
        _movementType = _spawner->Data.Fields.S01.WarWasp.MovementType;
        _health = _healthMax = static_cast<std::uint16_t>(_movementType == 3 ? 8 : 40);
        Flags |= EnemyFlags::Visible;
        Flags |= EnemyFlags::OnRadar;
        _boundingRadius = 1.0F;
        _hurtVolumeInit = CollisionVolume(Vector3(0.0F, -0.45F, 0.0F), 1.4F);
        _homeVolume = CollisionVolume::Move(_spawner->Data.Fields.S01.WarWasp.Volume2, Position);
        _movementVolume = CollisionVolume::Move(_spawner->Data.Fields.S01.WarWasp.Volume1, Position);
        SetUpModel(Metadata::EnemyModelNames[0], 1);
        _stepDistance = 0.2F;
        _attackDelay = 30 * 2; // todo: FPS stuff
        _attackTarget = _initialPos = Position;
        if (_movementType == 1)
        {
            _moveIndex = 1;
            _maxMoveIndex = 3;
            _finalMoveIndex = _maxMoveIndex;
            float xx = _movementVolume.BoxVector3.X * _movementVolume.BoxDot1;
            float xz = _movementVolume.BoxVector3.X * _movementVolume.BoxDot3;
            float zx = _movementVolume.BoxVector3.Z * _movementVolume.BoxDot1;
            float zz = _movementVolume.BoxVector3.Z * _movementVolume.BoxDot3;
            _movePositions[0] = _movementVolume.BoxPosition;
            _movePositions[1] = Vector3(
                _movementVolume.BoxPosition.X + xz,
                _movementVolume.BoxPosition.Y,
                _movementVolume.BoxPosition.Z + zz);
            _movePositions[2] = Vector3(
                _movementVolume.BoxPosition.X + (xz - zx),
                _movementVolume.BoxPosition.Y,
                _movementVolume.BoxPosition.Z + (zz + xx));
            _movePositions[3] = Vector3(
                _movementVolume.BoxPosition.X - zx,
                _movementVolume.BoxPosition.Y,
                _movementVolume.BoxPosition.Z + xx);
        }
        else if (_movementType == 2 || _movementType == 3)
        {
            _maxMoveIndex = static_cast<std::uint8_t>(
                _spawner->Data.Fields.S01.WarWasp.PositionCount - 1);
            _finalMoveIndex = _maxMoveIndex;
            for (std::int32_t i = 0; i < 16; i++)
            {
                _movePositions[static_cast<std::size_t>(i)]
                    = _spawner->Data.Fields.S01.WarWasp.MovementVectors[i].ToFloatVector()
                    + static_cast<Vector3>(Position);
            }
        }
        if (_movementType != 0)
        {
            StartMovingTowardPosition();
        }
    }

    void Enemy00Entity::StartMovingToward(Vector3 target, float step)
    {
        Vector3 travel = target - static_cast<Vector3>(Position);
        _stepDistance = step;
        float distance = Length(travel);
        _stepCount = static_cast<std::int32_t>(distance / _stepDistance) + 1;
        if (distance == 0.0F)
        {
            _speed = Vector3::Zero;
        }
        else
        {
            _speed = Scale(travel, _stepDistance / distance);
            // todo: FPS stuff
            _speed.X /= 2.0F;
            _speed.Y /= 2.0F;
            _speed.Z /= 2.0F;
            _stepCount *= 2;
        }
    }

    void Enemy00Entity::StartMovingTowardPosition()
    {
        _moveTarget = _movePositions.at(_moveIndex);
        StartMovingToward(_moveTarget, _movementType == 3 ? 0.25F : 0.2F);
        Vector3 facing = Equal(_speed, Vector3::Zero) ? FacingVector() : _speed.Normalized();
        (void)facing;
        SetTransform(FacingVector(), Vector3(0.0F, 1.0F, 0.0F), Position);
    }

    void Enemy00Entity::MoveInCircle()
    {
        if (_movementType == 0)
        {
            _circleAngle += 1.5F / 2.0F; // todo: FPS stuff
            if (_circleAngle >= 360.0F)
            {
                _circleAngle -= 360.0F;
            }
            float angle = _circleAngle * (3.14159265358979323846F / 180.0F);
            _speed.X = _initialPos.X + std::sin(angle) * _movementVolume.CylinderRadius;
            _speed.Z = _initialPos.Z + std::cos(angle) * _movementVolume.CylinderRadius;
            SetTransform(_speed.Normalized(), Vector3(0.0F, 1.0F, 0.0F), Position);
        }
    }

    void Enemy00Entity::EnemyProcess()
    {
        if (_state1 != 4 && _state1 != 5)
        {
            (void)ContactDamagePlayer(3, false);
        }
        _soundSource.PlaySfx(SfxId::WASP_IDLE, true);
        CallStateProcess();
    }

    void Enemy00Entity::State0()
    {
        MoveInCircle();
        if (!Equal(static_cast<Vector3>(Position), _moveTarget) && _movementType != 0)
        {
            SetTransform((_moveTarget - static_cast<Vector3>(Position)).Normalized(),
                Vector3(0.0F, 1.0F, 0.0F), Position);
        }
        (void)CallSubroutine<Enemy00Entity>(Metadata::Enemy00Subroutines, this);
    }

    void Enemy00Entity::State1()
    {
        Vector3 playerPos = MainPlayer().Position;
        if (!Equal(static_cast<Vector3>(Position), playerPos))
        {
            SetTransform((playerPos - static_cast<Vector3>(Position)).Normalized(),
                Vector3(0.0F, 1.0F, 0.0F), Position);
        }
        (void)CallSubroutine<Enemy00Entity>(Metadata::Enemy00Subroutines, this);
    }

    void Enemy00Entity::State2()
    {
        State1();
    }

    void Enemy00Entity::State3()
    {
        State1();
    }

    void Enemy00Entity::State4()
    {
        PlayerEntity& player = MainPlayer();
        if (HitPlayers.at(static_cast<std::size_t>(player.SlotIndex())))
        {
            player.TakeDamage(25, DamageFlags::None, std::nullopt, this);
            _stepCount = 0;
        }
        (void)CallSubroutine<Enemy00Entity>(Metadata::Enemy00Subroutines, this);
    }

    void Enemy00Entity::State5()
    {
        PlayerEntity& player = MainPlayer();
        if (HitPlayers.at(static_cast<std::size_t>(player.SlotIndex())))
        {
            player.TakeDamage(25, DamageFlags::None, std::nullopt, this);
        }
        (void)CallSubroutine<Enemy00Entity>(Metadata::Enemy00Subroutines, this);
    }

    void Enemy00Entity::State6()
    {
        if (!Equal(static_cast<Vector3>(Position), _moveTarget))
        {
            SetTransform((_moveTarget - static_cast<Vector3>(Position)).Normalized(),
                Vector3(0.0F, 1.0F, 0.0F), Position);
        }
        (void)CallSubroutine<Enemy00Entity>(Metadata::Enemy00Subroutines, this);
    }

    bool Enemy00Entity::Behavior00()
    {
        if (_stepCount > 0)
        {
            _stepCount--;
            return false;
        }
        StartMovingToward(_attackTarget, 1.2F);
        return true;
    }

    bool Enemy00Entity::Behavior01()
    {
        _attackDelay = 30 * 2; // todo: FPS stuff
        if (_stepCount > 0)
        {
            _stepCount--;
            return false;
        }
        if (_movementType == 0)
        {
            _speed = Vector3::Zero;
        }
        else
        {
            StartMovingTowardPosition();
            _models[0].SetAnimation(1);
        }
        return true;
    }

    bool Enemy00Entity::Behavior02()
    {
        if (_movementType == 0 && (_state1 == 0 || _state1 == 1))
        {
            return false;
        }
        if (_stepCount > 0)
        {
            _stepCount--;
            return false;
        }
        if (_movementType != 0)
        {
            if (_pattern == 1)
            {
                _moveIndex = static_cast<std::uint8_t>(
                    _moveIndex == 0 ? _maxMoveIndex : _moveIndex - 1);
            }
            else if (_pattern == 2 || _pattern == 0)
            {
                _moveIndex = static_cast<std::uint8_t>(
                    _moveIndex >= _maxMoveIndex ? 0 : _moveIndex + 1);
                if (_pattern == 0 && _movementType == 3 && _moveIndex == 0)
                {
                    _health = 0;
                }
            }
            else if (_pattern == 3)
            {
                _moveIndex--;
            }
            _moveTarget = _movePositions.at(_moveIndex);
            StartMovingToward(_moveTarget, _state1 == 6 ? 0.2F : _stepDistance);
            SetTransform(_speed.Normalized(), Vector3(0.0F, 1.0F, 0.0F), Position);
        }
        return true;
    }

    bool Enemy00Entity::Behavior03()
    {
        if (_movementType == 3 || !_homeVolume.TestPoint(MainPlayer().Position))
        {
            return false;
        }
        _finalMoveIndex = _moveIndex;
        _nextPattern = _pattern = 2;
        _stepDistance = 0.15F;
        return true;
    }

    bool Enemy00Entity::Behavior04()
    {
        if (_stepCount > 0)
        {
            _stepCount--;
            return false;
        }
        StartMovingToward(_moveTarget, 1.2F);
        return true;
    }

    bool Enemy00Entity::Behavior05()
    {
        if (!HandleBlockingCollision(Position, _hurtVolume, true))
        {
            return false;
        }
        StartMovingToward(_moveTarget, 1.2F);
        SetTransform(_speed.Normalized(), Vector3(0.0F, 1.0F, 0.0F), Position);
        _models[0].SetAnimation(1);
        return true;
    }

    bool Enemy00Entity::Behavior06()
    {
        if (_movementType != 0 && _finalMoveIndex != _moveIndex)
        {
            return false;
        }
        _speed = Vector3::Zero;
        Vector3 playerPos = MainPlayer().Position;
        Vector3 facing = playerPos - static_cast<Vector3>(Position);
        if (!Equal(static_cast<Vector3>(Position), playerPos))
        {
            facing = facing.Normalized();
        }
        SetTransform(facing, Vector3(0.0F, 1.0F, 0.0F), Position);
        return true;
    }

    bool Enemy00Entity::Behavior07()
    {
        if (_homeVolume.TestPoint(Position))
        {
            return false;
        }
        ReachTargetOrReversePattern();
        return true;
    }

    bool Enemy00Entity::Behavior08()
    {
        if (_homeVolume.TestPoint(MainPlayer().Position))
        {
            return false;
        }
        ReversePattern();
        return true;
    }

    void Enemy00Entity::ReachTargetOrReversePattern()
    {
        _speed = _moveTarget - static_cast<Vector3>(Position);
        if (Length(_speed) == 0.0F)
        {
            _stepCount = 0;
        }
        else
        {
            ReversePattern();
        }
    }

    void Enemy00Entity::ReversePattern()
    {
        _pattern = _nextPattern;
        if (_pattern == 0)
        {
            _finalMoveIndex = _maxMoveIndex;
            _moveIndex = static_cast<std::uint8_t>(
                _moveIndex >= _maxMoveIndex ? 0 : _moveIndex + 1);
        }
        else
        {
            _finalMoveIndex = 0;
            _moveIndex = static_cast<std::uint8_t>(
                _moveIndex == 0 ? _maxMoveIndex : _moveIndex - 1);
        }
        StartMovingTowardPosition();
    }

    bool Enemy00Entity::Behavior09()
    {
        if (_attackDelay > 0)
        {
            _attackDelay--;
            return false;
        }
        _attackTarget = MainPlayer().Position;
        _stepCount = 40 * 2; // todo: FPS stuff
        _models[0].SetAnimation(3, AnimFlags::NoLoop);
        _soundSource.PlaySfx(SfxId::WASP_ATTACK_SCR);
        return true;
    }

    bool Enemy00Entity::Behavior10()
    {
        Formats::CollisionResult res{};
        if (!Formats::CollisionDetection::CheckBetweenPoints(
            Position, MainPlayer().Position, Formats::TestFlags::None, _scene, res))
        {
            return false;
        }
        ReachTargetOrReversePattern();
        return true;
    }

    bool Enemy00Entity::Behavior00(Enemy00Entity* enemy) { return RequireEnemy(enemy).Behavior00(); }
    bool Enemy00Entity::Behavior01(Enemy00Entity* enemy) { return RequireEnemy(enemy).Behavior01(); }
    bool Enemy00Entity::Behavior02(Enemy00Entity* enemy) { return RequireEnemy(enemy).Behavior02(); }
    bool Enemy00Entity::Behavior03(Enemy00Entity* enemy) { return RequireEnemy(enemy).Behavior03(); }
    bool Enemy00Entity::Behavior04(Enemy00Entity* enemy) { return RequireEnemy(enemy).Behavior04(); }
    bool Enemy00Entity::Behavior05(Enemy00Entity* enemy) { return RequireEnemy(enemy).Behavior05(); }
    bool Enemy00Entity::Behavior06(Enemy00Entity* enemy) { return RequireEnemy(enemy).Behavior06(); }
    bool Enemy00Entity::Behavior07(Enemy00Entity* enemy) { return RequireEnemy(enemy).Behavior07(); }
    bool Enemy00Entity::Behavior08(Enemy00Entity* enemy) { return RequireEnemy(enemy).Behavior08(); }
    bool Enemy00Entity::Behavior09(Enemy00Entity* enemy) { return RequireEnemy(enemy).Behavior09(); }
    bool Enemy00Entity::Behavior10(Enemy00Entity* enemy) { return RequireEnemy(enemy).Behavior10(); }
}
