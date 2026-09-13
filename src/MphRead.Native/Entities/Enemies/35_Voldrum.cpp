#include "35_Voldrum.hpp"

#include "../../Formats/CollisionDetection.hpp"
#include "../../Metadata/Enemies.hpp"
#include "../../Scene.hpp"
#include "../../Utility/Rng.hpp"
#include "../EnemySpawnEntity.hpp"
#include "../Players/PlayerEntity.hpp"

#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <utility>

namespace MphRead::Entities::Enemies
{
    namespace
    {
        using OpenTK::Mathematics::Matrix4;
        using OpenTK::Mathematics::Vector3;
        using OpenTK::Mathematics::Vector4;

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

        [[nodiscard]] Enemy35Entity& RequireEnemy(Enemy35Entity* enemy)
        {
            return RequireReference(enemy);
        }

        [[nodiscard]] PlayerEntity& MainPlayer()
        {
            return RequireReference(PlayerEntity::Main());
        }

        [[nodiscard]] bool Equal(Vector3 left, Vector3 right) noexcept
        {
            return left.X == right.X && left.Y == right.Y && left.Z == right.Z;
        }

        [[nodiscard]] float LengthSquared(Vector3 value) noexcept
        {
            return value.X * value.X + value.Y * value.Y + value.Z * value.Z;
        }

        [[nodiscard]] Vector3 Scale(Vector3 value, float scale) noexcept
        {
            return Vector3(value.X * scale, value.Y * scale, value.Z * scale);
        }

        [[nodiscard]] Vector3 WithY(Vector3 value, float y) noexcept
        {
            value.Y = y;
            return value;
        }

        [[nodiscard]] float RadiansToDegrees(float radians) noexcept
        {
            return radians * (180.0F / 3.14159265358979323846F);
        }

        [[nodiscard]] float DegreesToRadians(float degrees) noexcept
        {
            return degrees * (3.14159265358979323846F / 180.0F);
        }

        [[nodiscard]] Matrix4 CreateRotationY(float angle) noexcept
        {
            const float cosine = std::cos(angle);
            const float sine = std::sin(angle);
            return Matrix4(
                Vector4(cosine, 0.0F, -sine, 0.0F),
                Vector4(0.0F, 1.0F, 0.0F, 0.0F),
                Vector4(sine, 0.0F, cosine, 0.0F),
                Vector4(0.0F, 0.0F, 0.0F, 1.0F));
        }
    }

    Enemy35Entity::Enemy35Entity(EnemyInstanceEntityData data,
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

    void Enemy35Entity::EnemyInitialize()
    {
        Setup();
    }

    void Enemy35Entity::Setup()
    {
        EnemySpawnEntity& spawner = RequireReference(_spawner);
        Vector3 facing = spawner.Data.Header.FacingVector.ToFloatVector().Normalized();
        Vector3 up = FixParallelVectors(facing, Vector3(0.0F, 1.0F, 0.0F));
        if (Equal(facing, Vector3(0.0F, 1.0F, 0.0F)))
        {
            Vector3 swap = facing;
            facing = up;
            up = swap;
        }
        SetTransform(facing, up, spawner.Data.Header.Position.ToFloatVector());
        Flags |= EnemyFlags::Visible;
        Flags |= EnemyFlags::OnRadar;
        _boundingRadius = 0.5F;
        _hurtVolumeInit = CollisionVolume(spawner.Data.Fields.S00().Volume0);
        _homeVolume = CollisionVolume::Move(
            spawner.Data.Fields.S00().Volume1, Position);
        assert(_homeVolume.Type == VolumeType::Cylinder);
        _health = _healthMax = 42;
        _speedInc = _speedIncAmount;
        _speedFactor = _minSpeedFactor;
        _ramDamageNeeded = true;
        _ramDelay = 40 * 2;
        if (spawner.Data.SpawnerHealth == 0
            || (std::fabs(Position.X - _homeVolume.CylinderPosition.X) < 1.0F / 4096.0F
                && std::fabs(Position.Z - _homeVolume.CylinderPosition.Z) < 1.0F / 4096.0F))
        {
            PickRoamTarget();
        }
        else
        {
            UpdateMoveTarget(WithY(_homeVolume.CylinderPosition, Position.Y));
        }
        _state1 = _state2 = 1;
        _subId = _state1;
        if (35 >= static_cast<std::int32_t>(Metadata::EnemyModelNames.size()))
        {
            throw SceneDetail::IndexOutOfRangeException();
        }
        SetUpModel(Metadata::EnemyModelNames[35], 4);
    }

    void Enemy35Entity::EnemyProcess()
    {
        bool sfxGrounded = true;
        if (!_grounded)
        {
            _speed.Y -= Fixed::ToFloat(110) / 4.0F;
        }
        if (_state1 == 2)
        {
            bool discard = false;
            if (!HandleBlockingCollision(
                    Position, _hurtVolume, true, _grounded, discard))
            {
                sfxGrounded = false;
            }
        }
        else if (_state1 == 0 || _state1 == 6)
        {
            if (!HandleCollision())
            {
                sfxGrounded = false;
            }
        }
        if (_state1 != 3 && _state1 != 4)
        {
            (void)ContactDamagePlayer(2, true);
        }
        CallStateProcess();
        if (_state1 != 0 && _state1 != 6)
        {
            sfxGrounded = false;
        }
        const float amount = 0xFFFF * _speedFactor * 2.0F / 0.2F;
        UpdateRollSfx(amount, sfxGrounded);
    }

    void Enemy35Entity::UpdateRollSfx(float newAmount, bool grounded)
    {
        const float prevAmount = _rollSfxAmount;
        if (!grounded)
        {
            newAmount = ExponentialDecay(0.5F, prevAmount);
        }
        else if (RequireReference(_scene).FrameCount() % 2 == 0)
        {
            if (newAmount < prevAmount)
            {
                newAmount = prevAmount + (newAmount - prevAmount) / 4.0F;
            }
            else
            {
                newAmount = prevAmount + (newAmount - prevAmount) / 2.0F;
            }
        }
        else
        {
            newAmount = _rollSfxAmount;
        }
        if (newAmount < 20.0F)
        {
            newAmount = 0.0F;
        }
        _rollSfxAmount = newAmount;
        _soundSource.PlaySfx(
            SfxId::DGN_GUARD_BOT_ROLL,
            true, false, -1.0F, false, false, _rollSfxAmount);
    }

    bool Enemy35Entity::HandleCollision()
    {
        return HandleCollision(5, 6);
    }

    bool Enemy35Entity::HandleCollision(std::int32_t stateA, std::int32_t stateB)
    {
        _grounded = false;
        ManagedArray<Formats::CollisionResult> results(30);
        const std::int32_t count = Formats::CollisionDetection::CheckInRadius(
            Position, _boundingRadius, 30, false,
            Formats::TestFlags::None, _scene, &results);
        if (count == 0)
        {
            return false;
        }
        for (std::int32_t i = 0; i < count; ++i)
        {
            const Formats::CollisionResult result
                = results[static_cast<std::size_t>(i)];
            float v7;
            if (result.Field0 != 0)
            {
                v7 = _boundingRadius - result.Field14;
            }
            else
            {
                v7 = _boundingRadius + result.Plane.W
                    - Vector3::Dot(Position, result.Plane.Xyz());
            }
            if (v7 > 0.0F)
            {
                Position = static_cast<Vector3>(Position)
                    + Scale(result.Plane.Xyz(), v7);
                if (result.Plane.Y >= 0.1F || result.Plane.Y <= -0.1F)
                {
                    _grounded = true;
                }
                else if (_state1 != 1 && _state1 != stateA)
                {
                    _airborne = true;
                    if (_state1 != 0 && _state1 != stateB)
                    {
                        _speed = -_speed;
                        _speed.Y = Fixed::ToFloat(1000) / 2.0F;
                    }
                    else
                    {
                        ++_timeInAir;
                    }
                }
                const float dot = Vector3::Dot(_speed, result.Plane.Xyz());
                if (dot < 0.0F)
                {
                    _speed = _speed + Scale(result.Plane.Xyz(), -dot);
                }
            }
        }
        return true;
    }

    void Enemy35Entity::PickRoamTarget()
    {
        const float dist = Fixed::ToFloat(
            Rng::GetRandomInt2(Fixed::ToInt(_homeVolume.CylinderRadius)));
        Vector3 vec(dist, 0.0F, 0.0F);
        _roamAngleSign *= -1.0F;
        const float angle = Fixed::ToFloat(
            Rng::GetRandomInt2(0xB4000)) * _roamAngleSign;
        const Matrix4 rotY = CreateRotationY(DegreesToRadians(angle));
        vec = Matrix::Vec3MultMtx3(vec, rotY);
        const Vector3 moveTarget(
            _homeVolume.CylinderPosition.X + vec.X,
            Position.Y,
            _homeVolume.CylinderPosition.Z + vec.Z);
        UpdateMoveTarget(moveTarget);
    }

    void Enemy35Entity::UpdateMoveTarget(Vector3 targetPoint)
    {
        _moveTarget = targetPoint;
        _moveStart = Position;
        _targetVec = _moveTarget - static_cast<Vector3>(Position);
        _moveDistSqr = LengthSquared(_targetVec);
        _moveDistSqrHalf = _moveDistSqr / 2.0F;
        _increaseSpeed = true;
        _targetVec = _targetVec.Normalized();
        const float angle = RadiansToDegrees(
            std::acos(Vector3::Dot(FacingVector(), _targetVec)));
        _aimSteps = _aimStepCount;
        _aimAngleStep = angle / static_cast<float>(_aimSteps);
    }

    void Enemy35Entity::UpdateSpeed()
    {
        if (_increaseSpeed)
        {
            const Vector3 between
                = _moveTarget - static_cast<Vector3>(Position);
            if (LengthSquared(between) < _moveDistSqrHalf)
            {
                _increaseSpeed = false;
            }
        }
        if (_increaseSpeed)
        {
            _speedFactor += _speedInc;
            if (_speedFactor > _maxSpeedFactor)
            {
                _speedFactor = _maxSpeedFactor;
            }
        }
        else
        {
            _speedFactor -= _speedInc;
            if (_speedFactor < _minSpeedFactor)
            {
                _speedFactor = _minSpeedFactor;
            }
        }
        const Vector3 facing = FacingVector();
        _speed.X = facing.X * _speedFactor;
        _speed.Z = facing.Z * _speedFactor;
    }

    void Enemy35Entity::State0()
    {
        UpdateSpeed();
        (void)CallSubroutine<Enemy35Entity>(Metadata::Enemy35Subroutines, this);
    }

    void Enemy35Entity::State1()
    {
        (void)CallSubroutine<Enemy35Entity>(Metadata::Enemy35Subroutines, this);
    }

    void Enemy35Entity::State2()
    {
        const Vector3 facing = WithY(
            static_cast<Vector3>(MainPlayer().Position)
                - static_cast<Vector3>(Position),
            0.0F).Normalized();
        SetTransform(facing, Vector3(0.0F, 1.0F, 0.0F), Position);
        (void)CallSubroutine<Enemy35Entity>(Metadata::Enemy35Subroutines, this);
    }

    void Enemy35Entity::State3()
    {
        _speedFactor -= _speedInc;
        if (_speedFactor < _minSpeedFactor)
        {
            _speedFactor = _minSpeedFactor;
        }
        const Vector3 facing = FacingVector();
        _speed.X = facing.X * _speedFactor;
        _speed.Z = facing.Z * _speedFactor;
        (void)CallSubroutine<Enemy35Entity>(Metadata::Enemy35Subroutines, this);
    }

    void Enemy35Entity::State4()
    {
        if (_handledRamCol && _ramDamageNeeded)
        {
            MainPlayer().TakeDamage(
                15, DamageFlags::NoDmgInvuln, std::nullopt, this);
            _ramDamageNeeded = false;
        }
        else
        {
            (void)CallSubroutine<Enemy35Entity>(Metadata::Enemy35Subroutines, this);
        }
    }

    void Enemy35Entity::State5()
    {
        (void)CallSubroutine<Enemy35Entity>(Metadata::Enemy35Subroutines, this);
    }

    void Enemy35Entity::State6()
    {
        State0();
    }

    bool Enemy35Entity::Behavior00()
    {
        const bool collided = HandleCollision();
        if (!SeekTargetFacing(_targetVec, UpVector(), _aimSteps, _aimAngleStep)
            || !collided)
        {
            return false;
        }
        _speedInc = _speedIncAmount;
        _speedFactor = _minSpeedFactor;
        _speed = Scale(FacingVector(), _speedFactor);
        return true;
    }

    bool Enemy35Entity::Behavior01()
    {
        if (!HandleCollision())
        {
            return false;
        }
        PickRoamTarget();
        _ramDamageNeeded = true;
        _handledRamCol = false;
        _speedInc = _speedIncAmount;
        _speedFactor = _minSpeedFactor;
        _speed = Scale(FacingVector(), _speedFactor);
        return true;
    }

    bool Enemy35Entity::Behavior02()
    {
        const Vector3 between
            = static_cast<Vector3>(Position) - _moveStart;
        if (LengthSquared(between) <= _moveDistSqr
            && (!_airborne || _timeInAir <= 5 * 2))
        {
            return false;
        }
        PickRoamTarget();
        _speed = Vector3(0.0F, 0.2F / 2.0F, 0.0F);
        _timeInAir = 0;
        _airborne = false;
        _soundSource.PlaySfx(SfxId::GUARD_BOT_JUMP);
        return true;
    }

    bool Enemy35Entity::Behavior03()
    {
        if (_ramDelay > 0)
        {
            --_ramDelay;
            return false;
        }
        const Vector3 facing = WithY(
            static_cast<Vector3>(MainPlayer().Position)
                - static_cast<Vector3>(Position),
            0.0F).Normalized();
        SetTransform(facing, Vector3(0.0F, 1.0F, 0.0F), Position);
        _timeInAir = 0;
        _airborne = false;
        _moveTarget = WithY(MainPlayer().Position, Position.Y);
        _targetVec = _moveTarget - static_cast<Vector3>(Position);
        _moveDistSqr = LengthSquared(_targetVec);
        _moveDistSqrHalf = _moveDistSqr / 2.0F;
        _speedInc = 0.005F / 2.0F;
        _speedFactor = 0.6F / 2.0F;
        _ramDelay = 40 * 2;
        _models[0].SetAnimation(0);
        _soundSource.PlaySfx(SfxId::GUARD_BOT_ATTACK1);
        return true;
    }

    bool Enemy35Entity::Behavior04()
    {
        const bool collided = HandleCollision();
        if (!_handledRamCol)
        {
            const std::int32_t slotIndex = MainPlayer().SlotIndex();
            if (slotIndex < 0
                || static_cast<std::size_t>(slotIndex) >= HitPlayers.size())
            {
                throw SceneDetail::IndexOutOfRangeException();
            }
            if (HitPlayers[static_cast<std::size_t>(slotIndex)])
            {
                MainPlayer().TakeDamage(
                    15, DamageFlags::NoDmgInvuln, std::nullopt, this);
                _handledRamCol = true;
                _ramDamageNeeded = false;
                _speed = -_speed;
                _speed.Y = Fixed::ToFloat(1000) / 2.0F;
                _models[0].SetAnimation(4);
                return true;
            }
        }
        if (collided && _airborne)
        {
            _airborne = false;
            _timeInAir = 0;
            _models[0].SetAnimation(4);
            return true;
        }
        return false;
    }

    bool Enemy35Entity::Behavior05()
    {
        if (_speedFactor != _maxSpeedFactor)
        {
            assert(std::fabs(_speedFactor - _maxSpeedFactor) >= 1.0F / 4096.0F);
            if (_homeVolume.TestPoint(Position))
            {
                return false;
            }
        }
        PickRoamTarget();
        _speed = Vector3(0.0F, 0.2F / 2.0F, 0.0F);
        _models[0].SetAnimation(4);
        _soundSource.PlaySfx(SfxId::GUARD_BOT_JUMP);
        return true;
    }

    bool Enemy35Entity::Behavior06()
    {
        if (MainPlayer().Health() == 0)
        {
            return false;
        }
        const Vector3 between = (
            static_cast<Vector3>(MainPlayer().Position)
                - static_cast<Vector3>(Position)).Normalized();
        if (Vector3::Dot(FacingVector(), between) <= -1.0F
            || !_homeVolume.TestPoint(MainPlayer().Position))
        {
            return false;
        }
        _speed = Vector3::Zero;
        _models[0].SetAnimation(1);
        return true;
    }

    bool Enemy35Entity::Behavior00(Enemy35Entity* enemy)
    {
        return RequireEnemy(enemy).Behavior00();
    }

    bool Enemy35Entity::Behavior01(Enemy35Entity* enemy)
    {
        return RequireEnemy(enemy).Behavior01();
    }

    bool Enemy35Entity::Behavior02(Enemy35Entity* enemy)
    {
        return RequireEnemy(enemy).Behavior02();
    }

    bool Enemy35Entity::Behavior03(Enemy35Entity* enemy)
    {
        return RequireEnemy(enemy).Behavior03();
    }

    bool Enemy35Entity::Behavior04(Enemy35Entity* enemy)
    {
        return RequireEnemy(enemy).Behavior04();
    }

    bool Enemy35Entity::Behavior05(Enemy35Entity* enemy)
    {
        return RequireEnemy(enemy).Behavior05();
    }

    bool Enemy35Entity::Behavior06(Enemy35Entity* enemy)
    {
        return RequireEnemy(enemy).Behavior06();
    }
}
