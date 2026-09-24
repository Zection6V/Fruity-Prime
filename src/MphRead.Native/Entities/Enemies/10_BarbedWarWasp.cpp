#include "10_BarbedWarWasp.hpp"

#include "../../Formats/CollisionDetection.hpp"
#include "../../Metadata/Enemies.hpp"
#include "../../Metadata/SoundMeta.hpp"
#include "../../Metadata/Weapons.hpp"
#include "../../Scene.hpp"
#include "../../Utility/Rng.hpp"
#include "../BeamProjectileEntity.hpp"
#include "../EnemySpawnEntity.hpp"
#include "../Players/PlayerEntity.hpp"
#include "../../NativeRuntime/System/Managed.hpp"
#include "../../Formats/Types.hpp"

#include <array>
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
using ::MphRead::NativeRuntime::RequireReference;
using ::OpenTK::Mathematics::AddY;
using ::OpenTK::Mathematics::Equal;
using ::OpenTK::Mathematics::Length;
using ::OpenTK::Mathematics::MathHelper::DegreesToRadians;
using ::OpenTK::Mathematics::Scale;

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

        [[nodiscard]] Enemy10Entity& RequireEnemy(Enemy10Entity* enemy)
        {
            return RequireReference(enemy);
        }

        [[nodiscard]] PlayerEntity& MainPlayer()
        {
            return RequireReference(PlayerEntity::Main());
        }

        [[nodiscard]] std::int32_t WrapInt32(std::uint32_t value) noexcept
        {
            return std::bit_cast<std::int32_t>(value);
        }

        [[nodiscard]] std::int32_t AddInt32(
            std::int32_t left, std::int32_t right) noexcept
        {
            return WrapInt32(
                static_cast<std::uint32_t>(left) + static_cast<std::uint32_t>(right));
        }

        [[nodiscard]] std::int32_t MultiplyInt32(
            std::int32_t left, std::int32_t right) noexcept
        {
            return WrapInt32(
                static_cast<std::uint32_t>(left) * static_cast<std::uint32_t>(right));
        }

        [[nodiscard]] std::int32_t UInt32ToInt32(std::uint32_t value) noexcept
        {
            return std::bit_cast<std::int32_t>(value);
        }

        [[nodiscard]] std::uint16_t GetShotCount(const Enemy10Values& values)
        {
            const std::int32_t range
                = static_cast<std::int32_t>(values.MaxShots)
                + 1
                - static_cast<std::int32_t>(values.MinShots);
            const std::uint32_t random = Rng::GetRandomInt2(range);
            const std::int64_t count
                = static_cast<std::int64_t>(values.MinShots)
                + static_cast<std::int64_t>(random);
            return static_cast<std::uint16_t>(count);
        }

        template <typename T>
        [[nodiscard]] T& VectorAt(std::vector<T>& values, std::int32_t index)
        {
            if (index < 0 || static_cast<std::size_t>(index) >= values.size())
            {
                throw SceneDetail::IndexOutOfRangeException();
            }
            return values[static_cast<std::size_t>(index)];
        }

        template <typename T>
        [[nodiscard]] const T& VectorAt(const std::vector<T>& values, std::int32_t index)
        {
            if (index < 0 || static_cast<std::size_t>(index) >= values.size())
            {
                throw SceneDetail::IndexOutOfRangeException();
            }
            return values[static_cast<std::size_t>(index)];
        }
    }

    const std::array<std::int32_t, 11> Enemy10Entity::_recolors{
        0, 0, 0, 0, 0, 2, 1, 0, 0, 0, 0
    };
}

namespace MphRead::Entities::Enemies
{
    Enemy10Entity::Enemy10Entity(EnemyInstanceEntityData data,
        Formats::Culling::NodeRef nodeRef, Scene* scene)
        : EnemyInstanceEntity(data, nodeRef, scene),
          _spawner(CastSpawner(data.Spawner))
    {
        auto processes = std::make_shared<ManagedArray<std::function<void()>>>(6);
        (*processes)[0] = [this]() { State0(); };
        (*processes)[1] = [this]() { State1(); };
        (*processes)[2] = [this]() { State2(); };
        (*processes)[3] = [this]() { State3(); };
        (*processes)[4] = [this]() { State4(); };
        (*processes)[5] = [this]() { State5(); };
        _stateProcesses = std::move(processes);
    }

    void Enemy10Entity::EnemyInitialize()
    {
        if (_spawner == nullptr)
        {
            throw System::NullReferenceException();
        }

        const std::int32_t version = UInt32ToInt32(
            static_cast<std::uint32_t>(_spawner->Data.Fields.S08().EnemyVersion));
        if (version < 0 || static_cast<std::size_t>(version) >= _recolors.size())
        {
            throw SceneDetail::IndexOutOfRangeException();
        }
        SetRecolor(_recolors[static_cast<std::size_t>(version)]);

        const Vector3 facing = _spawner->FacingVector();
        const Vector3 up = FixParallelVectors(facing, Vector3(0.0F, 1.0F, 0.0F));
        SetTransform(facing, up, static_cast<Vector3>(_spawner->Position));

        _movementType = _spawner->Data.Fields.S08().WarWasp.MovementType;
        Flags |= EnemyFlags::Visible;
        Flags |= EnemyFlags::OnRadar;
        _boundingRadius = 1.0F;
        _hurtVolumeInit = CollisionVolume(Vector3(0.0F, -0.45F, 0.0F), 1.4F);

        const std::int32_t subtype = UInt32ToInt32(
            static_cast<std::uint32_t>(_spawner->Data.Fields.S08().EnemySubtype));
        _values = VectorAt(Metadata::Enemy10Values, subtype);
        _health = _healthMax = _values.HealthMax;
        Metadata::LoadEffectiveness(_values.Effectiveness, BeamEffectiveness);
        _scanId = _values.ScanId;

        _homeVolume = CollisionVolume::Move(
            _spawner->Data.Fields.S08().WarWasp.Volume2, Position);
        _movementVolume = CollisionVolume::Move(
            _spawner->Data.Fields.S08().WarWasp.Volume1, Position);

        const std::vector<std::shared_ptr<WeaponInfo>>& enemyWeapons
            = RequireReference(Weapons::EnemyWeapons);
        const std::shared_ptr<WeaponInfo> weapon = VectorAt(enemyWeapons, version);
        _equipInfo = std::make_shared<EquipInfo>(weapon, _beams);
        _equipInfo->GetAmmo = [this]() { return _ammo; };
        _equipInfo->SetAmmo = [this](std::int32_t newAmmo) { _ammo = newAmmo; };
        _equipInfo->UnchargedDamage(_values.BeamDamage);
        _equipInfo->SplashDamage(_values.SplashDamage);

        _shotCount = GetShotCount(_values);
        _shotTimer = 30 * 2; // todo: FPS stuff

        if (10 >= static_cast<std::int32_t>(Metadata::EnemyModelNames.size()))
        {
            throw SceneDetail::IndexOutOfRangeException();
        }
        SetUpModel(Metadata::EnemyModelNames[10], 1);

        _stepDistance = Fixed::ToFloat(_values.StepDistance1);
        _initialPos = Position;
        if (_movementType == 1)
        {
            _moveIndex = 1;
            _maxMoveIndex = 3;
            _finalMoveIndex = _maxMoveIndex;
            const float xx = _movementVolume.BoxVector3.X * _movementVolume.BoxDot1;
            const float xz = _movementVolume.BoxVector3.X * _movementVolume.BoxDot3;
            const float zx = _movementVolume.BoxVector3.Z * _movementVolume.BoxDot1;
            const float zz = _movementVolume.BoxVector3.Z * _movementVolume.BoxDot3;
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
                static_cast<std::int32_t>(
                    _spawner->Data.Fields.S08().WarWasp.PositionCount) - 1);
            _finalMoveIndex = _maxMoveIndex;
            for (std::int32_t i = 0; i < 16; ++i)
            {
                _movePositions[static_cast<std::size_t>(i)]
                    = _spawner->Data.Fields.S08().WarWasp.MovementVectors[i].ToFloatVector()
                    + static_cast<Vector3>(Position);
            }
        }
        if (_movementType != 0)
        {
            StartMovingTowardPosition();
        }
    }

    void Enemy10Entity::StartMovingToward(Vector3 target, float step)
    {
        const Vector3 travel = target - static_cast<Vector3>(Position);
        _stepDistance = step;
        const float distance = Length(travel);
        _stepCount = AddInt32(ConvertToInt32Net9(distance / _stepDistance), 1);
        if (distance == 0.0F)
        {
            _speed = Vector3::Zero;
        }
        else
        {
            _speed = ::OpenTK::Mathematics::Scale(
                travel, _stepDistance / distance);
            // todo: FPS stuff
            _speed.X /= 2.0F;
            _speed.Y /= 2.0F;
            _speed.Z /= 2.0F;
            _stepCount = MultiplyInt32(_stepCount, 2);
        }
    }

    void Enemy10Entity::StartMovingTowardPosition()
    {
        const std::size_t moveIndex = static_cast<std::size_t>(_moveIndex);
        if (moveIndex >= _movePositions.size())
        {
            throw SceneDetail::IndexOutOfRangeException();
        }
        _moveTarget = _movePositions[moveIndex];
        StartMovingToward(_moveTarget, Fixed::ToFloat(_values.StepDistance1));
        SetTransform(
            _speed.Normalized(), Vector3(0.0F, 1.0F, 0.0F), Position);
    }

    void Enemy10Entity::MoveInCircle()
    {
        if (_movementType == 0)
        {
            _circleAngle += Fixed::ToFloat(_values.CircleIncrement) / 2.0F;
            if (_circleAngle >= 360.0F)
            {
                _circleAngle -= 360.0F;
            }
            const float angle = DegreesToRadians(_circleAngle);
            _speed.X = _initialPos.X + std::sin(angle) * _movementVolume.CylinderRadius;
            _speed.Z = _initialPos.Z + std::cos(angle) * _movementVolume.CylinderRadius;
            SetTransform(
                _speed.Normalized(), Vector3(0.0F, 1.0F, 0.0F), Position);
        }
    }

    void Enemy10Entity::EnemyProcess()
    {
        if (HandleBlockingCollision(Position, _hurtVolume, true) && _state1 == 3)
        {
            _state2 = 5;
            _subId = _state2;
            StartMovingToward(_moveTarget, Fixed::ToFloat(_values.StepDistance3));
            SetTransform(
                _speed.Normalized(), Vector3(0.0F, 1.0F, 0.0F), Position);

            const std::shared_ptr<AnimationInfo> animInfo = _models[0].AnimInfo;
            AnimationInfo& info = RequireReference(animInfo);
            ManagedArray<std::int32_t>& indices = RequireReference(info.Index);
            if (indices[0] == 2)
            {
                _models[0].SetAnimation(1);
            }
        }
        (void)ContactDamagePlayer(_values.ContactDamage, false);
        _soundSource.PlaySfx(SfxId::WASP_IDLE, true);
        CallStateProcess();
    }

    void Enemy10Entity::State0()
    {
        MoveInCircle();
        if (!Equal(static_cast<Vector3>(Position), _moveTarget)
            && _movementType != 0)
        {
            SetTransform(
                (_moveTarget - static_cast<Vector3>(Position)).Normalized(),
                Vector3(0.0F, 1.0F, 0.0F),
                Position);
        }
        (void)CallSubroutine<Enemy10Entity>(Metadata::Enemy10Subroutines, this);
    }

    void Enemy10Entity::State1()
    {
        const Vector3 playerPos = MainPlayer().Position;
        if (!Equal(static_cast<Vector3>(Position), playerPos))
        {
            SetTransform(
                (playerPos - static_cast<Vector3>(Position)).Normalized(),
                Vector3(0.0F, 1.0F, 0.0F),
                Position);
        }
        (void)CallSubroutine<Enemy10Entity>(Metadata::Enemy10Subroutines, this);
    }

    void Enemy10Entity::State2()
    {
        State1();
    }

    void Enemy10Entity::State3()
    {
        if (_shotCount > 0 && _shotTimer == 10 * 2)
        {
            EquipInfo& equip = RequireReference(_equipInfo);
            equip.UnchargedDamage(_values.BeamDamage);
            equip.SplashDamage(_values.SplashDamage);
            equip.HeadshotDamage(_values.BeamDamage);
            const Vector3 spawnPos = AddY(static_cast<Vector3>(Position), -0.5F);
            (void)BeamProjectileEntity::Spawn(
                SharedFrom<EntityBase>(this),
                _equipInfo,
                spawnPos,
                _aimVector,
                BeamSpawnFlags::None,
                NodeRef,
                _scene);
            --_shotCount;
            PlayBeamShotSfx();
        }
        else if (_shotTimer == 15 * 2)
        {
            _models[0].SetAnimation(2, AnimFlags::None | AnimFlags::Reverse);
            const std::shared_ptr<AnimationInfo> animInfo = _models[0].AnimInfo;
            AnimationInfo& info = RequireReference(animInfo);
            ManagedArray<std::int32_t>& frames = RequireReference(info.Frame);
            frames[0] = 15;
        }
        --_shotTimer;
        (void)CallSubroutine<Enemy10Entity>(Metadata::Enemy10Subroutines, this);
    }

    void Enemy10Entity::PlayBeamShotSfx()
    {
        EquipInfo& equip = RequireReference(_equipInfo);
        const std::shared_ptr<WeaponInfo> weapon = equip.Weapon;
        WeaponInfo& weaponRef = RequireReference(weapon);

        const std::shared_ptr<std::vector<std::vector<std::int32_t>>> beamSfx
            = Metadata::BeamSfx();
        std::vector<std::vector<std::int32_t>>& sfxRows = RequireReference(beamSfx);
        const std::int32_t beamIndex = static_cast<std::int32_t>(weaponRef.Beam);
        std::vector<std::int32_t>& sfxRow = VectorAt(sfxRows, beamIndex);
        const std::int32_t sfx = VectorAt(
            sfxRow, static_cast<std::int32_t>(BeamSfx::Shot));
        if (sfx != -1)
        {
            _soundSource.PlaySfx(sfx);
        }
    }

    void Enemy10Entity::State4()
    {
        if (!Equal(static_cast<Vector3>(Position), _moveTarget))
        {
            SetTransform(
                (_moveTarget - static_cast<Vector3>(Position)).Normalized(),
                Vector3(0.0F, 1.0F, 0.0F),
                Position);
        }
        (void)CallSubroutine<Enemy10Entity>(Metadata::Enemy10Subroutines, this);
    }

    void Enemy10Entity::State5()
    {
        (void)CallSubroutine<Enemy10Entity>(Metadata::Enemy10Subroutines, this);
    }

    bool Enemy10Entity::Behavior00()
    {
        const std::shared_ptr<AnimationInfo> animInfo = _models[0].AnimInfo;
        AnimationInfo& info = RequireReference(animInfo);
        ManagedArray<AnimFlags>& flags = RequireReference(info.Flags);
        if ((flags[0] & AnimFlags::Ended) != AnimFlags::None)
        {
            _aimVector = (
                AddY(static_cast<Vector3>(MainPlayer().Position), 0.5F)
                - static_cast<Vector3>(Position)).Normalized();
            _models[0].SetAnimation(2, AnimFlags::NoLoop);
            return true;
        }
        return false;
    }

    bool Enemy10Entity::Behavior01()
    {
        if (_movementType == 0 && (_state1 == 0 || _state1 == 1))
        {
            return false;
        }
        if (_stepCount > 0)
        {
            --_stepCount;
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
                --_moveIndex;
            }

            const std::size_t moveIndex = static_cast<std::size_t>(_moveIndex);
            if (moveIndex >= _movePositions.size())
            {
                throw SceneDetail::IndexOutOfRangeException();
            }
            _moveTarget = _movePositions[moveIndex];
            StartMovingToward(
                _moveTarget,
                _state1 == 4
                    ? Fixed::ToFloat(_values.StepDistance1)
                    : _stepDistance);
            SetTransform(
                _speed.Normalized(), Vector3(0.0F, 1.0F, 0.0F), Position);
        }
        return true;
    }

    bool Enemy10Entity::Behavior02()
    {
        if (_stepCount > 0)
        {
            --_stepCount;
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

    bool Enemy10Entity::Behavior03()
    {
        if (_movementType == 3
            || !_homeVolume.TestPoint(MainPlayer().Position))
        {
            return false;
        }
        _finalMoveIndex = _moveIndex;
        _nextPattern = _pattern = 2;
        _stepDistance = Fixed::ToFloat(_values.StepDistance2);
        return true;
    }

    bool Enemy10Entity::Behavior04()
    {
        if (_shotCount > 0 || _shotTimer > 0)
        {
            return false;
        }
        _shotCount = GetShotCount(_values);
        _shotTimer = 30 * 2;
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

    bool Enemy10Entity::Behavior05()
    {
        if (_shotCount == 0 || _shotTimer > 0)
        {
            return false;
        }
        _shotTimer = 30 * 2;
        _models[0].SetAnimation(0, AnimFlags::NoLoop);
        return true;
    }

    bool Enemy10Entity::Behavior06()
    {
        if (_movementType != 0 && _finalMoveIndex != _moveIndex)
        {
            return false;
        }
        _speed = Vector3::Zero;
        const Vector3 playerPos = MainPlayer().Position;
        Vector3 facing = playerPos - static_cast<Vector3>(Position);
        if (!Equal(static_cast<Vector3>(Position), playerPos))
        {
            facing = facing.Normalized();
        }
        SetTransform(facing, Vector3(0.0F, 1.0F, 0.0F), Position);
        _models[0].SetAnimation(0, AnimFlags::NoLoop);
        return true;
    }

    bool Enemy10Entity::Behavior07()
    {
        if (_homeVolume.TestPoint(Position))
        {
            return false;
        }
        ReachTargetOrReversePattern();
        return true;
    }

    bool Enemy10Entity::Behavior08()
    {
        if (_homeVolume.TestPoint(MainPlayer().Position))
        {
            return false;
        }
        ReversePattern();
        return true;
    }

    bool Enemy10Entity::Behavior09()
    {
        Formats::CollisionResult res{};
        if (!Formats::CollisionDetection::CheckBetweenPoints(
            Position,
            MainPlayer().Position,
            Formats::TestFlags::None,
            _scene,
            res))
        {
            return false;
        }
        ReachTargetOrReversePattern();
        return true;
    }

    void Enemy10Entity::ReachTargetOrReversePattern()
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

    void Enemy10Entity::ReversePattern()
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

    bool Enemy10Entity::Behavior00(Enemy10Entity* enemy)
    {
        return RequireEnemy(enemy).Behavior00();
    }

    bool Enemy10Entity::Behavior01(Enemy10Entity* enemy)
    {
        return RequireEnemy(enemy).Behavior01();
    }

    bool Enemy10Entity::Behavior02(Enemy10Entity* enemy)
    {
        return RequireEnemy(enemy).Behavior02();
    }

    bool Enemy10Entity::Behavior03(Enemy10Entity* enemy)
    {
        return RequireEnemy(enemy).Behavior03();
    }

    bool Enemy10Entity::Behavior04(Enemy10Entity* enemy)
    {
        return RequireEnemy(enemy).Behavior04();
    }

    bool Enemy10Entity::Behavior05(Enemy10Entity* enemy)
    {
        return RequireEnemy(enemy).Behavior05();
    }

    bool Enemy10Entity::Behavior06(Enemy10Entity* enemy)
    {
        return RequireEnemy(enemy).Behavior06();
    }

    bool Enemy10Entity::Behavior07(Enemy10Entity* enemy)
    {
        return RequireEnemy(enemy).Behavior07();
    }

    bool Enemy10Entity::Behavior08(Enemy10Entity* enemy)
    {
        return RequireEnemy(enemy).Behavior08();
    }

    bool Enemy10Entity::Behavior09(Enemy10Entity* enemy)
    {
        return RequireEnemy(enemy).Behavior09();
    }
}
