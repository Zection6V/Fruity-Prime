#include "36_Voldrum.hpp"

#include "../../Metadata/Enemies.hpp"
#include "../../Metadata/Weapons.hpp"
#include "../../Scene.hpp"
#include "../../Utility/Rng.hpp"
#include "../BeamProjectileEntity.hpp"
#include "../EnemySpawnEntity.hpp"
#include "../Players/PlayerEntity.hpp"

#include <array>
#include <bit>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

namespace MphRead::Entities::Enemies
{
    namespace
    {
        using OpenTK::Mathematics::Vector3;

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

        [[nodiscard]] Enemy36Entity& RequireEnemy(Enemy36Entity* enemy)
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

        [[nodiscard]] float Length(Vector3 value)
        {
            return std::sqrt(
                value.X * value.X + value.Y * value.Y + value.Z * value.Z);
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

        [[nodiscard]] Vector3 AddX(Vector3 value, float amount) noexcept
        {
            value.X += amount;
            return value;
        }

        [[nodiscard]] float RadiansToDegrees(float radians) noexcept
        {
            return radians * (180.0F / 3.14159265358979323846F);
        }

        [[nodiscard]] std::int32_t UInt32ToInt32(std::uint32_t value) noexcept
        {
            return std::bit_cast<std::int32_t>(value);
        }

        [[nodiscard]] std::uint16_t TimesTwo(std::uint16_t value) noexcept
        {
            return static_cast<std::uint16_t>(
                static_cast<std::uint32_t>(value) * 2U);
        }

        [[nodiscard]] std::uint16_t GetShotCount(const Enemy36Values& values)
        {
            const std::int32_t range
                = static_cast<std::int32_t>(values.MaxShots)
                + 1
                - static_cast<std::int32_t>(values.MinShots);
            const std::uint32_t random = Rng::GetRandomInt2(range);
            return static_cast<std::uint16_t>(
                static_cast<std::uint32_t>(values.MinShots) + random);
        }

        [[nodiscard]] std::shared_ptr<EntityBase> SharedEntity(
            Scene* scene, EntityBase* entity)
        {
            Scene& sceneRef = RequireReference(scene);
            auto enumerator = sceneRef.Entities().GetEnumerator();
            while (enumerator.MoveNext())
            {
                std::shared_ptr<EntityBase> current = enumerator.Current();
                if (!current)
                {
                    throw System::NullReferenceException();
                }
                if (current.get() == entity)
                {
                    return current;
                }
            }
            throw SceneDetail::InvalidOperationException();
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
        [[nodiscard]] const T& VectorAt(
            const std::vector<T>& values, std::int32_t index)
        {
            if (index < 0 || static_cast<std::size_t>(index) >= values.size())
            {
                throw SceneDetail::IndexOutOfRangeException();
            }
            return values[static_cast<std::size_t>(index)];
        }
    }

    const std::array<std::int32_t, 11> Enemy36Entity::_recolors{
        0, 1, 0, 4, 0, 3, 2, 0, 0, 0, 0
    };

    Enemy36Entity::Enemy36Entity(EnemyInstanceEntityData data,
        Formats::Culling::NodeRef nodeRef, Scene* scene)
        : Enemy35Entity(data, nodeRef, scene)
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

    void Enemy36Entity::Setup()
    {
        EnemySpawnEntity& spawner = RequireReference(_spawner);

        const std::int32_t version = UInt32ToInt32(
            static_cast<std::uint32_t>(spawner.Data.Fields.S06().EnemyVersion));
        if (version < 0 || static_cast<std::size_t>(version) >= _recolors.size())
        {
            throw SceneDetail::IndexOutOfRangeException();
        }
        SetRecolor(_recolors[static_cast<std::size_t>(version)]);

        const std::int32_t subtype = UInt32ToInt32(
            static_cast<std::uint32_t>(spawner.Data.Fields.S06().EnemySubtype));
        _values = VectorAt(Metadata::Enemy36Values, subtype);

        Vector3 facing = spawner.Data.Header.FacingVector.ToFloatVector().Normalized();
        Vector3 up = FixParallelVectors(facing, Vector3(0.0F, 1.0F, 0.0F));
        if (Equal(facing, Vector3(0.0F, 1.0F, 0.0F)))
        {
            Vector3 swap = facing;
            facing = up;
            up = swap;
        }
        SetTransform(
            facing, up, spawner.Data.Header.Position.ToFloatVector());
        Flags |= EnemyFlags::Visible;
        Flags |= EnemyFlags::OnRadar;
        _boundingRadius = 0.5F;
        _hurtVolumeInit = CollisionVolume(spawner.Data.Fields.S06().Volume0);
        _homeVolume = CollisionVolume::Move(
            spawner.Data.Fields.S06().Volume1, Position);
        assert(_homeVolume.Type == VolumeType::Cylinder);
        _health = _healthMax = _values.HealthMax;
        Metadata::LoadEffectiveness(_values.Effectiveness, BeamEffectiveness);
        _scanId = _values.ScanId;

        if (version < 0
            || static_cast<std::size_t>(version) >= Weapons::EnemyWeapons.size())
        {
            throw SceneDetail::IndexOutOfRangeException();
        }
        const std::shared_ptr<WeaponInfo> weapon
            = Weapons::EnemyWeapons[static_cast<std::size_t>(version)];
        _equipInfo1 = std::make_shared<EquipInfo>();
        _equipInfo1->SetWeapon(weapon);
        _equipInfo1->SetBeams(RequireReference(_beams));
        _equipInfo2 = std::make_shared<EquipInfo>();
        _equipInfo2->SetWeapon(weapon);
        _equipInfo2->SetBeams(RequireReference(_beams));

        _equipInfo1->SetGetAmmo([this]() { return _ammo1; });
        _equipInfo1->SetSetAmmo(
            [this](std::int32_t newAmmo) { _ammo1 = newAmmo; });
        _equipInfo1->SetGetAmmo([this]() { return _ammo2; });
        _equipInfo1->SetSetAmmo(
            [this](std::int32_t newAmmo) { _ammo2 = newAmmo; });
        _equipInfo1->SetUnchargedDamage(_values.BeamDamage);
        _equipInfo1->SetSplashDamage(_values.SplashDamage);
        _equipInfo2->SetUnchargedDamage(_values.BeamDamage);
        _equipInfo2->SetSplashDamage(_values.SplashDamage);

        const float minFactor = Fixed::ToFloat(_values.MinSpeedFactor);
        const float maxFactor = Fixed::ToFloat(_values.MaxSpeedFactor);
        _minSpeedFactor = minFactor / 2.0F;
        _maxSpeedFactor = maxFactor / 2.0F;
        _speedFactor = minFactor / 2.0F;
        _speedInc = (maxFactor - minFactor)
            * (1.0F / static_cast<float>(_values.SpeedSteps));
        _speedInc /= 2.0F;
        _speedIncAmount = _speedInc;
        _delayTimer = TimesTwo(_values.DelayTime);
        _shotTimer = TimesTwo(_values.ShotTime);
        _shotCount = GetShotCount(_values);
        _aimStepCount = TimesTwo(_values.AimSteps);

        if (spawner.Data.SpawnerHealth == 0
            || (std::fabs(Position.X - _homeVolume.CylinderPosition.X)
                    < 1.0F / 4096.0F
                && std::fabs(Position.Z - _homeVolume.CylinderPosition.Z)
                    < 1.0F / 4096.0F))
        {
            PickRoamTarget();
        }
        else
        {
            UpdateMoveTarget(WithY(_homeVolume.CylinderPosition, Position.Y));
        }
        _state1 = _state2 = 1;
        _subId = _state1;

        if (36 >= static_cast<std::int32_t>(Metadata::EnemyModelNames.size()))
        {
            throw SceneDetail::IndexOutOfRangeException();
        }
        ModelInstance& inst = SetUpModel(Metadata::EnemyModelNames[36], 5);
        (void)inst;
    }

    void Enemy36Entity::EnemyProcess()
    {
        bool sfxGrounded = true;
        if (!_grounded)
        {
            _speed.Y -= Fixed::ToFloat(110) / 4.0F;
        }
        if (_state1 == 2 || _state1 == 3)
        {
            bool discard = false;
            if (!HandleBlockingCollision(
                    Position, _hurtVolume, true, _grounded, discard))
            {
                sfxGrounded = false;
            }
        }
        else if (_state1 != 1 && _state1 != 4)
        {
            if (!HandleCollision())
            {
                sfxGrounded = false;
            }
        }
        if (_state1 != 0 && _state1 != 5)
        {
            (void)ContactDamagePlayer(_values.ContactDamage, true);
        }
        CallStateProcess();
        if (_state1 != 0)
        {
            sfxGrounded = false;
        }
        const float amount
            = 0xFFFF * _speedFactor * 2.0F / (_maxSpeedFactor * 2.0F);
        UpdateRollSfx(amount, sfxGrounded);
    }

    bool Enemy36Entity::HandleCollision()
    {
        return Enemy35Entity::HandleCollision(4, 5);
    }

    void Enemy36Entity::State0()
    {
        UpdateSpeed();
        (void)CallSubroutine<Enemy36Entity>(Metadata::Enemy36Subroutines, this);
    }

    void Enemy36Entity::State1()
    {
        (void)CallSubroutine<Enemy36Entity>(Metadata::Enemy36Subroutines, this);
    }

    void Enemy36Entity::UpdateFacing()
    {
        _speed = Vector3::Zero;
        Vector3 facing = (
            static_cast<Vector3>(MainPlayer().Position)
            - static_cast<Vector3>(Position)).Normalized();
        if (facing.Y > 0.5F)
        {
            facing = WithY(facing, 0.5F).Normalized();
        }
        else if (facing.Y < -0.5F)
        {
            facing = WithY(facing, -0.5F).Normalized();
        }
        SetTransform(facing, UpVector(), Position);
    }

    void Enemy36Entity::State2()
    {
        UpdateFacing();
        (void)CallSubroutine<Enemy36Entity>(Metadata::Enemy36Subroutines, this);
    }

    void Enemy36Entity::State3()
    {
        UpdateFacing();
        if (_shotCount > 0 && _shotTimer > 0)
        {
            --_shotTimer;
        }
        else
        {
            const Vector3 facing = FacingVector();
            const Vector3 spawnPos1 = AddX(
                static_cast<Vector3>(Position), -0.43F);
            const Vector3 spawnPos2 = AddX(
                static_cast<Vector3>(Position), 0.43F);

            EquipInfo& equip1 = RequireReference(_equipInfo1);
            equip1.SetUnchargedDamage(_values.BeamDamage);
            equip1.SetSplashDamage(_values.SplashDamage);
            equip1.SetHeadshotDamage(_values.BeamDamage);
            EquipInfo& equip2 = RequireReference(_equipInfo2);
            equip2.SetUnchargedDamage(_values.BeamDamage);
            equip2.SetSplashDamage(_values.SplashDamage);
            equip2.SetHeadshotDamage(_values.BeamDamage);

            const std::shared_ptr<EntityBase> source = SharedEntity(_scene, this);
            (void)BeamProjectileEntity::Spawn(
                source,
                _equipInfo1,
                spawnPos1,
                facing,
                BeamSpawnFlags::None,
                NodeRef,
                _scene);
            (void)BeamProjectileEntity::Spawn(
                source,
                _equipInfo2,
                spawnPos2,
                facing,
                BeamSpawnFlags::None,
                NodeRef,
                _scene);

            --_shotCount;
            _delayTimer = TimesTwo(_values.DelayTime);
            _shotTimer = TimesTwo(_values.ShotTime);
            _models[0].SetAnimation(0);
            _soundSource.PlaySfx(SfxId::GUARD_BOT_ATTACK2);
        }
        (void)CallSubroutine<Enemy36Entity>(Metadata::Enemy36Subroutines, this);
    }

    void Enemy36Entity::State4()
    {
        (void)CallSubroutine<Enemy36Entity>(Metadata::Enemy36Subroutines, this);
    }

    void Enemy36Entity::State5()
    {
        State0();
    }

    bool Enemy36Entity::Behavior00()
    {
        const bool collided = HandleCollision();
        if (!SeekTargetFacing(_targetVec, UpVector(), _aimSteps, _aimAngleStep)
            || !collided)
        {
            return false;
        }
        _speedInc = _speedIncAmount;
        _speedFactor = _minSpeedFactor;
        _speed = WithY(Scale(FacingVector(), _speedFactor), 0.0F);
        _airborne = false;
        _timeInAir = 0;
        return true;
    }

    bool Enemy36Entity::Behavior01()
    {
        if (_delayTimer > 0)
        {
            --_delayTimer;
            return false;
        }
        _delayTimer = TimesTwo(_values.DelayTime);
        return true;
    }

    bool Enemy36Entity::Behavior02()
    {
        if (_shotCount > 0)
        {
            return false;
        }
        PickRoamTarget();
        _delayTimer = TimesTwo(_values.DelayTime);
        _shotTimer = TimesTwo(_values.ShotTime);
        _shotCount = GetShotCount(_values);
        _models[0].SetAnimation(5);
        return true;
    }

    bool Enemy36Entity::Behavior03()
    {
        const Vector3 between
            = static_cast<Vector3>(Position) - _moveStart;
        if (LengthSquared(between) <= _moveDistSqr
            && (!_airborne || _timeInAir <= 5 * 2))
        {
            return false;
        }
        PickRoamTarget();
        if (_state1 == 5)
        {
            _targetVec = WithY(
                static_cast<Vector3>(MainPlayer().Position)
                    - static_cast<Vector3>(Position),
                0.0F).Normalized();
            const float angle = RadiansToDegrees(
                std::acos(Vector3::Dot(FacingVector(), _targetVec)));
            _aimSteps = _aimStepCount;
            _aimAngleStep = angle / static_cast<float>(_aimSteps);
        }
        _speed = Vector3(
            0.0F, Fixed::ToFloat(_values.JumpSpeed) / 2.0F, 0.0F);
        _timeInAir = 0;
        _airborne = false;
        return true;
    }

    bool Enemy36Entity::Behavior04()
    {
        const std::int32_t slotIndex = MainPlayer().SlotIndex();
        if (slotIndex < 0
            || static_cast<std::size_t>(slotIndex) >= HitPlayers.size())
        {
            throw SceneDetail::IndexOutOfRangeException();
        }
        if (!HitPlayers[static_cast<std::size_t>(slotIndex)])
        {
            return false;
        }

        const Vector3 between
            = MainPlayer().Volume().SpherePosition - static_cast<Vector3>(Position);
        const float mag = Length(between) * 5.0F;
        PlayerEntity& speedTarget = MainPlayer();
        const float speedX = MainPlayer().Speed().X + between.X / mag;
        const float speedY = MainPlayer().Speed().Y;
        const float speedZ = MainPlayer().Speed().Z + between.Z / mag;
        speedTarget.SetSpeed(Vector3(speedX, speedY, speedZ));
        MainPlayer().TakeDamage(
            _values.ContactDamage, DamageFlags::NoDmgInvuln, std::nullopt, this);

        PickRoamTarget();
        if (_state1 == 5)
        {
            _targetVec = WithY(
                static_cast<Vector3>(MainPlayer().Position)
                    - static_cast<Vector3>(Position),
                0.0F).Normalized();
            const float angle = RadiansToDegrees(
                std::acos(Vector3::Dot(FacingVector(), _targetVec)));
            _aimSteps = _aimStepCount;
            _aimAngleStep = angle / static_cast<float>(_aimSteps);
        }
        _speed = Vector3(
            0.0F, Fixed::ToFloat(_values.JumpSpeed) / 2.0F, 0.0F);
        _timeInAir = 0;
        _airborne = false;
        return true;
    }

    bool Enemy36Entity::Behavior05()
    {
        if (MainPlayer().Health() == 0)
        {
            return false;
        }
        const Vector3 between = (
            static_cast<Vector3>(MainPlayer().Position)
            - static_cast<Vector3>(Position)).Normalized();
        if (Vector3::Dot(FacingVector(), between)
            <= Fixed::ToFloat(_values.RangeMaxCosine))
        {
            return false;
        }
        _speed = Vector3::Zero;
        return true;
    }

    bool Enemy36Entity::Behavior00(Enemy36Entity* enemy)
    {
        return RequireEnemy(enemy).Behavior00();
    }

    bool Enemy36Entity::Behavior01(Enemy36Entity* enemy)
    {
        return RequireEnemy(enemy).Behavior01();
    }

    bool Enemy36Entity::Behavior02(Enemy36Entity* enemy)
    {
        return RequireEnemy(enemy).Behavior02();
    }

    bool Enemy36Entity::Behavior03(Enemy36Entity* enemy)
    {
        return RequireEnemy(enemy).Behavior03();
    }

    bool Enemy36Entity::Behavior04(Enemy36Entity* enemy)
    {
        return RequireEnemy(enemy).Behavior04();
    }

    bool Enemy36Entity::Behavior05(Enemy36Entity* enemy)
    {
        return RequireEnemy(enemy).Behavior05();
    }
}
