#include "23_PsychoBit.hpp"

#include "../../Metadata/Enemies.hpp"
#include "../../Metadata/Weapons.hpp"
#include "../../Scene.hpp"
#include "../../Utility/Rng.hpp"
#include "../BeamProjectileEntity.hpp"
#include "../CamSeq/CameraSequence.hpp"
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
#include <utility>
#include <vector>

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

        template <typename T>
        [[nodiscard]] T& RequireReference(const std::shared_ptr<T>& value)
        {
            if (!value)
            {
                throw System::NullReferenceException();
            }
            return *value;
        }

        [[nodiscard]] Enemy23Entity& RequireEnemy(Enemy23Entity* enemy)
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

        [[nodiscard]] Vector3 ScaleVector(Vector3 value, float scale) noexcept
        {
            return Vector3(value.X * scale, value.Y * scale, value.Z * scale);
        }

        [[nodiscard]] Vector3 AddY(Vector3 value, float amount) noexcept
        {
            value.Y += amount;
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

        [[nodiscard]] std::int32_t UInt32ToInt32(std::uint32_t value) noexcept
        {
            return std::bit_cast<std::int32_t>(value);
        }

        [[nodiscard]] std::uint16_t TimesTwo(std::uint16_t value) noexcept
        {
            return static_cast<std::uint16_t>(
                static_cast<std::uint32_t>(value) * 2U);
        }

        [[nodiscard]] std::uint16_t GetShotCount(const Enemy23Values& values)
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

    const std::array<std::int32_t, 11> Enemy23Entity::_recolors{
        0, 1, 0, 4, 0, 3, 2, 0, 0, 0, 0
    };

    Enemy23Entity::Enemy23Entity(EnemyInstanceEntityData data,
        Formats::Culling::NodeRef nodeRef, Scene* scene)
        : EnemyInstanceEntity(data, nodeRef, scene),
          _spawner(CastSpawner(data.Spawner))
    {
        auto processes = std::make_shared<ManagedArray<std::function<void()>>>(11);
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
        _stateProcesses = std::move(processes);
    }

    void Enemy23Entity::EnemyInitialize()
    {
        EnemySpawnEntity& spawner = RequireReference(_spawner);

        const std::int32_t version = UInt32ToInt32(spawner.Data.Fields.S06().EnemyVersion);
        if (version < 0 || static_cast<std::size_t>(version) >= _recolors.size())
        {
            throw SceneDetail::IndexOutOfRangeException();
        }
        SetRecolor(_recolors[static_cast<std::size_t>(version)]);

        const Vector3 facing = spawner.FacingVector();
        const Vector3 up = FixParallelVectors(facing, Vector3(0.0F, 1.0F, 0.0F));
        SetTransform(facing, up, static_cast<Vector3>(spawner.Position));

        if (23 >= static_cast<std::int32_t>(Metadata::EnemyModelNames.size()))
        {
            throw SceneDetail::IndexOutOfRangeException();
        }
        SetUpModel(Metadata::EnemyModelNames[23], 3);
        Flags |= EnemyFlags::Visible;
        Flags |= EnemyFlags::OnRadar;
        _boundingRadius = 1.0F;
        _hurtVolumeInit = CollisionVolume(spawner.Data.Fields.S06().Volume0);

        const std::int32_t subtype = UInt32ToInt32(spawner.Data.Fields.S06().EnemySubtype);
        _values = VectorAt(Metadata::Enemy23Values, subtype);
        _health = _healthMax = _values.HealthMax;
        Metadata::LoadEffectiveness(_values.Effectiveness, BeamEffectiveness);
        _scanId = _values.ScanId;
        _curFacing = facing;
        _homeVolume = CollisionVolume::Move(
            spawner.Data.Fields.S06().Volume1, Position);
        _nearVolume = CollisionVolume(Vector3::Zero, 1.0F);
        _rangeVolume = CollisionVolume::Move(
            spawner.Data.Fields.S06().Volume3, Position);

        const Weapons::WeaponList& enemyWeapons
            = RequireReference(Weapons::EnemyWeapons);
        if (version < 0
            || static_cast<std::size_t>(version) >= enemyWeapons.size())
        {
            throw SceneDetail::IndexOutOfRangeException();
        }
        const std::shared_ptr<WeaponInfo> weapon
            = enemyWeapons[static_cast<std::size_t>(version)];
        _equipInfo = std::make_shared<EquipInfo>(weapon, _beams);
        _equipInfo->SetGetAmmo([this]() { return _ammo; });
        _equipInfo->SetSetAmmo([this](std::int32_t newAmmo) { _ammo = newAmmo; });
        _equipInfo->UnchargedDamage(_values.BeamDamage);
        _equipInfo->SplashDamage(_values.SplashDamage);

        _delayTimer = TimesTwo(_values.DelayTime);
        _shotTimer = TimesTwo(_values.ShotTime);
        _speedFactor = Fixed::ToFloat(_values.MinSpeedFactor1) / 2.0F;
        _state1 = _state2 = 9;

        assert(_homeVolume.Type != VolumeType::Sphere);
        bool atHomePoint = false;
        Vector3 homePoint = Vector3::Zero;
        if (spawner.Data.SpawnerHealth == 0)
        {
            atHomePoint = true;
        }
        else if (_homeVolume.Type == VolumeType::Cylinder)
        {
            assert(Equal(_homeVolume.CylinderVector, Vector3(0.0F, 1.0F, 0.0F)));
            homePoint = Vector3(
                _homeVolume.CylinderPosition.X,
                _homeVolume.CylinderPosition.Y + _homeVolume.CylinderDot / 2.0F,
                _homeVolume.CylinderPosition.Z);
            if (std::fabs(Position.X - homePoint.X) < 1.0F / 4096.0F
                && std::fabs(Position.Y - homePoint.Y) < 1.0F / 4096.0F
                && std::fabs(Position.Z - homePoint.Z) < 1.0F / 4096.0F)
            {
                atHomePoint = true;
            }
        }
        else if (_homeVolume.Type == VolumeType::Box)
        {
            homePoint = Vector3(
                _homeVolume.BoxPosition.X
                    + _homeVolume.BoxVector1.X * (_homeVolume.BoxDot1 / 2.0F),
                _homeVolume.BoxPosition.Y
                    + _homeVolume.BoxVector2.Y * (_homeVolume.BoxDot2 / 2.0F),
                _homeVolume.BoxPosition.Z
                    + _homeVolume.BoxVector3.Z * (_homeVolume.BoxDot3 / 2.0F));
            if (std::fabs(Position.X - homePoint.X) < 1.0F / 4096.0F
                && std::fabs(Position.Y - homePoint.Y) < 1.0F / 4096.0F
                && std::fabs(Position.Z - homePoint.Z) < 1.0F / 4096.0F)
            {
                atHomePoint = true;
            }
        }

        if (atHomePoint)
        {
            PickRoamTarget();
        }
        else
        {
            UpdateMoveTarget(homePoint);
            _speedFactor = Fixed::ToFloat(_values.MinSpeedFactor2) / 2.0F;
        }
        _increaseSpeed = true;
        _shotCount = GetShotCount(_values);
        _aimVec = (static_cast<Vector3>(MainPlayer().Position)
            - static_cast<Vector3>(Position)).Normalized();
        _subId = _state1;
    }

    void Enemy23Entity::EnemyProcess()
    {
        if (_effect)
        {
            const Vector3 facing = FacingVector();
            _eyePos = static_cast<Vector3>(Position) - facing;
            _effect->Transform(
                facing, Vector3(0.0F, 1.0F, 0.0F), _eyePos);
        }
        (void)ContactDamagePlayer(_values.ContactDamage, true);
        _nearVolume = CollisionVolume::Move(
            CollisionVolume(Vector3::Zero, 1.0F), Position);
        _soundSource.PlaySfx(SfxId::PSYCHOBIT_FLY, true);
        CallStateProcess();
    }

    bool Enemy23Entity::EnemyTakeDamage(EntityBase* source)
    {
        (void)source;
        if (_health > 0)
        {
            if (_state1 == 3)
            {
                _damaged = true;
            }
            else if (_state1 != 9 && _state1 != 10)
            {
                _state2 = 3;
                _subId = _state2;
                if (_state1 == 2)
                {
                    _soundSource.StopSfx(SfxId::PSYCHOBIT_CHARGE);
                }
                if (!_effect)
                {
                    SpawnEffect();
                }
                _speed = Vector3::Zero;
                _speedFactor = 0.0F;
                _speedInc = 0.0F;
                _delayTimer = TimesTwo(_values.DelayTime);
                _shotTimer = TimesTwo(_values.ShotTime);
                _shotCount = GetShotCount(_values);
                _aimVec = (static_cast<Vector3>(MainPlayer().Position)
                    - static_cast<Vector3>(Position)).Normalized();
                _models[0].SetAnimation(0, AnimFlags::NoLoop);
                SetTransform(
                    _aimVec, Vector3(0.0F, 1.0F, 0.0F), Position);
                _curFacing = _aimVec;
            }
        }
        else if (_effect)
        {
            RequireReference(_scene).UnlinkEffectEntry(_effect);
            _effect.reset();
        }
        return false;
    }

    void Enemy23Entity::SpawnEffect()
    {
        const Vector3 facing = FacingVector();
        _eyePos = static_cast<Vector3>(Position) - facing;
        _effect = RequireReference(_scene).SpawnEffectGetEntry(
            240,
            Vector3(1.0F, 0.0F, 0.0F),
            Vector3(0.0F, 1.0F, 0.0F),
            _eyePos);
        if (_effect)
        {
            _effect->SetElementExtension(true);
        }
    }

    void Enemy23Entity::UpdateMoveTarget(Vector3 targetPoint)
    {
        const Vector3 facing = FacingVector();
        _moveTarget = targetPoint;
        _moveStart = Position;
        _targetVec = _moveTarget - static_cast<Vector3>(Position);
        _moveDistSqr = LengthSquared(_targetVec);
        _moveDistSqrHalf = _moveDistSqr / 2.0F;
        _increaseSpeed = true;
        _targetVec = _targetVec.Normalized();
        const float angle = RadiansToDegrees(
            std::acos(Vector3::Dot(facing, _targetVec)));
        _aimSteps = TimesTwo(_values.AimSteps);
        _aimAngleStep = angle / static_cast<float>(_aimSteps);
        _crossVec = Vector3::Cross(facing, _targetVec).Normalized();
    }

    void Enemy23Entity::PickRoamTarget()
    {
        Vector3 moveTarget;
        if (_homeVolume.Type == VolumeType::Cylinder)
        {
            const float dist = Fixed::ToFloat(Rng::GetRandomInt2(
                Fixed::ToInt(_homeVolume.CylinderRadius)));
            Vector3 vec(dist, 0.0F, 0.0F);
            _roamAngleSign *= -1.0F;
            const float angle = Fixed::ToFloat(
                Rng::GetRandomInt2(0xB4000)) * _roamAngleSign;
            const Matrix4 rotY = CreateRotationY(DegreesToRadians(angle));
            vec = Matrix::Vec3MultMtx3(vec, rotY);
            vec.Y = Fixed::ToFloat(Rng::GetRandomInt2(
                Fixed::ToInt(_homeVolume.CylinderDot)));
            moveTarget = _homeVolume.CylinderPosition + vec;
        }
        else
        {
            assert(_homeVolume.Type == VolumeType::Box);
            const float distX = Fixed::ToFloat(Rng::GetRandomInt2(
                Fixed::ToInt(_homeVolume.BoxDot1)));
            const float distY = Fixed::ToFloat(Rng::GetRandomInt2(
                Fixed::ToInt(_homeVolume.BoxDot2)));
            const float distZ = Fixed::ToFloat(Rng::GetRandomInt2(
                Fixed::ToInt(_homeVolume.BoxDot3)));
            const Vector3 vec(
                _homeVolume.BoxVector1.X * distX,
                _homeVolume.BoxVector2.Y * distY,
                _homeVolume.BoxVector3.Z * distZ);
            moveTarget = _homeVolume.BoxPosition + vec;
        }
        UpdateMoveTarget(moveTarget);
        _speed = Vector3::Zero;
    }

    void Enemy23Entity::UpdateSpeed(float min, float max)
    {
        if (_increaseSpeed)
        {
            const Vector3 between = _moveTarget - static_cast<Vector3>(Position);
            if (LengthSquared(between) < _moveDistSqrHalf)
            {
                _increaseSpeed = false;
            }
        }
        if (_increaseSpeed)
        {
            _speedFactor += _speedInc;
            if (_speedFactor > max / 2.0F)
            {
                _speedFactor = max / 2.0F;
            }
        }
        else
        {
            _speedFactor -= _speedInc;
            if (_speedFactor < min / 2.0F)
            {
                _speedFactor = min / 2.0F;
            }
        }
        _speed = ScaleVector(FacingVector(), _speedFactor);
    }

    void Enemy23Entity::State00()
    {
        UpdateSpeed(
            Fixed::ToFloat(_values.MinSpeedFactor1),
            Fixed::ToFloat(_values.MaxSpeedFactor1));
        (void)CallSubroutine<Enemy23Entity>(Metadata::Enemy23Subroutines, this);
    }

    void Enemy23Entity::State01()
    {
        (void)CallSubroutine<Enemy23Entity>(Metadata::Enemy23Subroutines, this);
    }

    void Enemy23Entity::UpdateFacing()
    {
        const Vector3 between = (
            static_cast<Vector3>(MainPlayer().Position)
            - static_cast<Vector3>(Position)).Normalized();
        if (Vector3::Dot(_curFacing, between)
            > Fixed::ToFloat(_values.RangeMaxCosine))
        {
            SetTransform(
                between, Vector3(0.0F, 1.0F, 0.0F), Position);
        }
    }

    void Enemy23Entity::State02()
    {
        UpdateFacing();
        (void)CallSubroutine<Enemy23Entity>(Metadata::Enemy23Subroutines, this);
    }

    void Enemy23Entity::State03()
    {
        UpdateFacing();
        if (_shotCount > 0 && _shotTimer > 0)
        {
            --_shotTimer;
        }
        else
        {
            const Vector3 targetPos = AddY(
                static_cast<Vector3>(MainPlayer().Position), 0.5F);
            _aimVec = (targetPos - static_cast<Vector3>(Position)).Normalized();
            const Vector3 spawnPos
                = static_cast<Vector3>(Position) + ScaleVector(_aimVec, 0.5F);
            EquipInfo& equip = RequireReference(_equipInfo);
            equip.UnchargedDamage(_values.BeamDamage);
            equip.SplashDamage(_values.SplashDamage);
            equip.HeadshotDamage(_values.BeamDamage);
            (void)BeamProjectileEntity::Spawn(
                SharedEntity(_scene, this),
                _equipInfo,
                spawnPos,
                _aimVec,
                BeamSpawnFlags::None,
                NodeRef,
                _scene);
            --_shotCount;
            _delayTimer = TimesTwo(_values.DelayTime);
            _shotTimer = TimesTwo(_values.ShotTime);
            _soundSource.PlaySfx(SfxId::PSYCHOBIT_BEAM);
            _models[0].SetAnimation(0, AnimFlags::NoLoop);
        }
        (void)CallSubroutine<Enemy23Entity>(Metadata::Enemy23Subroutines, this);
    }

    void Enemy23Entity::State04()
    {
        UpdateSpeed(
            Fixed::ToFloat(_values.MinSpeedFactor1),
            Fixed::ToFloat(_values.MaxSpeedFactor1));
        if (CallSubroutine<Enemy23Entity>(
                Metadata::Enemy23Subroutines, this)
            && _state2 == 2)
        {
            _speed = Vector3::Zero;
            _curFacing = FacingVector();
            _models[0].SetAnimation(1);
            _soundSource.PlaySfx(
                SfxId::PSYCHOBIT_CHARGE,
                false, false, -1.0F, false, true);
        }
    }

    void Enemy23Entity::State05()
    {
        (void)CallSubroutine<Enemy23Entity>(Metadata::Enemy23Subroutines, this);
    }

    void Enemy23Entity::State06()
    {
        (void)CallSubroutine<Enemy23Entity>(Metadata::Enemy23Subroutines, this);
    }

    void Enemy23Entity::State07()
    {
        UpdateSpeed(
            Fixed::ToFloat(_values.MinSpeedFactor2),
            Fixed::ToFloat(_values.MaxSpeedFactor2));
        (void)CallSubroutine<Enemy23Entity>(Metadata::Enemy23Subroutines, this);
    }

    void Enemy23Entity::State08()
    {
        (void)CallSubroutine<Enemy23Entity>(Metadata::Enemy23Subroutines, this);
    }

    void Enemy23Entity::State09()
    {
        (void)CallSubroutine<Enemy23Entity>(Metadata::Enemy23Subroutines, this);
    }

    void Enemy23Entity::State10()
    {
        _speed = ScaleVector(FacingVector(), _speedFactor);
        (void)CallSubroutine<Enemy23Entity>(Metadata::Enemy23Subroutines, this);
    }

    bool Enemy23Entity::Behavior00()
    {
        Vector3 facing = FacingVector();
        const bool result = SeekTargetVector(
            _targetVec, facing, _crossVec, _aimSteps, _aimAngleStep);
        const Vector3 up = FixParallelVectors(
            facing, Vector3(0.0F, 1.0F, 0.0F));
        SetTransform(facing, up, Position);
        if (!result)
        {
            return false;
        }
        const float minFactor = Fixed::ToFloat(_values.MinSpeedFactor2);
        const float maxFactor = Fixed::ToFloat(_values.MaxSpeedFactor2);
        _speedFactor = minFactor / 2.0F;
        _speedInc = (maxFactor - minFactor)
            * (1.0F / static_cast<float>(_values.SpeedSteps));
        _speedInc /= 2.0F;
        _speed = ScaleVector(facing, _speedFactor);
        _curFacing = facing;
        _models[0].SetAnimation(2);
        return true;
    }

    void Enemy23Entity::CheckReachedTarget()
    {
        const Vector3 nextPos = static_cast<Vector3>(Position) + _speed;
        if (LengthSquared(_moveStart - nextPos) > _moveDistSqr)
        {
            _speed = nextPos - static_cast<Vector3>(Position);
            _reachedTarget = true;
        }
    }

    bool Enemy23Entity::Behavior01()
    {
        if (_reachedTarget && _reachTargetHackTimer == 0)
        {
            PickRoamTarget();
            _delayTimer = TimesTwo(_values.DelayTime);
            _shotTimer = TimesTwo(_values.ShotTime);
            _reachedTarget = false;
            return true;
        }
        if (_reachTargetHackTimer > 0)
        {
            --_reachTargetHackTimer;
        }
        CheckReachedTarget();
        return false;
    }

    bool Enemy23Entity::Behavior02()
    {
        return Behavior01();
    }

    bool Enemy23Entity::Behavior03()
    {
        Vector3 facing = FacingVector();
        const bool result = SeekTargetVector(
            _targetVec, facing, _crossVec, _aimSteps, _aimAngleStep);
        const Vector3 up = FixParallelVectors(
            facing, Vector3(0.0F, 1.0F, 0.0F));
        SetTransform(facing, up, Position);
        if (!result)
        {
            return false;
        }
        _speedInc = 0.0F;
        _speedFactor = 0.0F;
        _curFacing = facing;
        _models[0].SetAnimation(1);
        _soundSource.PlaySfx(
            SfxId::PSYCHOBIT_CHARGE,
            false, false, -1.0F, false, true);
        SpawnEffect();
        return true;
    }

    bool Enemy23Entity::Behavior04()
    {
        return Behavior00();
    }

    bool Enemy23Entity::Behavior05()
    {
        Vector3 facing = FacingVector();
        const bool result = SeekTargetVector(
            _targetVec, facing, _crossVec, _aimSteps, _aimAngleStep);
        const Vector3 up = FixParallelVectors(
            facing, Vector3(0.0F, 1.0F, 0.0F));
        SetTransform(facing, up, Position);
        if (!result)
        {
            return false;
        }
        const float minFactor = Fixed::ToFloat(_values.MinSpeedFactor1);
        const float maxFactor = Fixed::ToFloat(_values.MaxSpeedFactor1);
        _speedFactor = minFactor / 2.0F;
        _speedInc = (maxFactor - minFactor)
            * (1.0F / static_cast<float>(_values.SpeedSteps));
        _speedInc /= 2.0F;
        _speed = ScaleVector(facing, _speedFactor);
        _curFacing = facing;
        return true;
    }

    bool Enemy23Entity::Behavior06()
    {
        if (_reachedTarget && _reachTargetHackTimer == 0)
        {
            _reachTargetHackTimer = 1;
            const Vector3 facing = FacingVector();
            _targetVec = (
                static_cast<Vector3>(MainPlayer().Position)
                - static_cast<Vector3>(Position)).Normalized();
            const float angle = RadiansToDegrees(
                std::acos(Vector3::Dot(facing, _targetVec)));
            _aimSteps = TimesTwo(_values.AimSteps);
            _aimAngleStep = angle / static_cast<float>(_aimSteps);
            _crossVec = Vector3::Cross(facing, _targetVec).Normalized();
            _speed = Vector3::Zero;
            _delayTimer = TimesTwo(_values.DelayTime);
            _shotTimer = TimesTwo(_values.ShotTime);
            return true;
        }
        if (_reachTargetHackTimer > 0)
        {
            --_reachTargetHackTimer;
        }
        CheckReachedTarget();
        return false;
    }

    bool Enemy23Entity::Behavior07()
    {
        Formats::CameraSequence* current = Formats::CameraSequence::Current();
        if (current != nullptr && current->BlockInput())
        {
            _camSeqDelayTimer = 40 * 2;
            return false;
        }
        if (_camSeqDelayTimer > 0)
        {
            --_camSeqDelayTimer;
            return false;
        }
        if (MainPlayer().Health() > 0)
        {
            const Vector3 between = (
                static_cast<Vector3>(MainPlayer().Position)
                - static_cast<Vector3>(Position)).Normalized();
            if (Vector3::Dot(_curFacing, between)
                    > Fixed::ToFloat(_values.RangeMaxCosine)
                && _rangeVolume.TestPoint(MainPlayer().Position))
            {
                _speed = Vector3::Zero;
                _models[0].SetAnimation(1);
                _soundSource.PlaySfx(
                    SfxId::PSYCHOBIT_CHARGE,
                    false, false, -1.0F, false, true);
                SpawnEffect();
                return true;
            }
        }
        return false;
    }

    bool Enemy23Entity::Behavior08()
    {
        if (_delayTimer > 0)
        {
            --_delayTimer;
            return false;
        }
        _soundSource.StopSfx(SfxId::PSYCHOBIT_CHARGE);
        _models[0].SetAnimation(0, AnimFlags::NoLoop);
        _aimVec = (
            static_cast<Vector3>(MainPlayer().Position)
            + static_cast<Vector3>(Position)).Normalized();
        return true;
    }

    void Enemy23Entity::MoveAway()
    {
        PickRoamTarget();
        _delayTimer = TimesTwo(_values.DelayTime);
        _shotTimer = TimesTwo(_values.ShotTime);
        _models[0].SetAnimation(3);
        if (_effect)
        {
            RequireReference(_scene).UnlinkEffectEntry(_effect);
            _effect.reset();
        }
        if (_state1 == 2)
        {
            _soundSource.StopSfx(SfxId::PSYCHOBIT_CHARGE);
        }
    }

    bool Enemy23Entity::Behavior09()
    {
        const Vector3 between = (
            static_cast<Vector3>(MainPlayer().Position)
            - static_cast<Vector3>(Position)).Normalized();
        if (Vector3::Dot(_curFacing, between)
            >= Fixed::ToFloat(_values.RangeMaxCosine))
        {
            return false;
        }
        MoveAway();
        return true;
    }

    bool Enemy23Entity::Behavior10()
    {
        if (!_nearVolume.TestPoint(MainPlayer().Position))
        {
            return false;
        }
        MoveAway();
        return true;
    }

    bool Enemy23Entity::Behavior11()
    {
        if (_damaged || _shotCount > 0)
        {
            return false;
        }
        PickRoamTarget();
        _delayTimer = TimesTwo(_values.DelayTime);
        _shotTimer = TimesTwo(_values.ShotTime);
        _shotCount = GetShotCount(_values);
        if (_effect)
        {
            RequireReference(_scene).UnlinkEffectEntry(_effect);
            _effect.reset();
        }
        return true;
    }

    bool Enemy23Entity::Behavior12()
    {
        if (!_damaged || _shotCount > 0)
        {
            return false;
        }
        PickRoamTarget();
        _delayTimer = TimesTwo(_values.DelayTime);
        _shotTimer = TimesTwo(_values.ShotTime);
        _shotCount = GetShotCount(_values);
        _damaged = false;
        if (_effect)
        {
            RequireReference(_scene).UnlinkEffectEntry(_effect);
            _effect.reset();
        }
        return true;
    }

    void Enemy23Entity::Destroy()
    {
        if (_effect)
        {
            RequireReference(_scene).UnlinkEffectEntry(_effect);
            _effect.reset();
        }
        EnemyInstanceEntity::Destroy();
    }

    bool Enemy23Entity::Behavior00(Enemy23Entity* enemy)
    {
        return RequireEnemy(enemy).Behavior00();
    }

    bool Enemy23Entity::Behavior01(Enemy23Entity* enemy)
    {
        return RequireEnemy(enemy).Behavior01();
    }

    bool Enemy23Entity::Behavior02(Enemy23Entity* enemy)
    {
        return RequireEnemy(enemy).Behavior02();
    }

    bool Enemy23Entity::Behavior03(Enemy23Entity* enemy)
    {
        return RequireEnemy(enemy).Behavior03();
    }

    bool Enemy23Entity::Behavior04(Enemy23Entity* enemy)
    {
        return RequireEnemy(enemy).Behavior04();
    }

    bool Enemy23Entity::Behavior05(Enemy23Entity* enemy)
    {
        return RequireEnemy(enemy).Behavior05();
    }

    bool Enemy23Entity::Behavior06(Enemy23Entity* enemy)
    {
        return RequireEnemy(enemy).Behavior06();
    }

    bool Enemy23Entity::Behavior07(Enemy23Entity* enemy)
    {
        return RequireEnemy(enemy).Behavior07();
    }

    bool Enemy23Entity::Behavior08(Enemy23Entity* enemy)
    {
        return RequireEnemy(enemy).Behavior08();
    }

    bool Enemy23Entity::Behavior09(Enemy23Entity* enemy)
    {
        return RequireEnemy(enemy).Behavior09();
    }

    bool Enemy23Entity::Behavior10(Enemy23Entity* enemy)
    {
        return RequireEnemy(enemy).Behavior10();
    }

    bool Enemy23Entity::Behavior11(Enemy23Entity* enemy)
    {
        return RequireEnemy(enemy).Behavior11();
    }

    bool Enemy23Entity::Behavior12(Enemy23Entity* enemy)
    {
        return RequireEnemy(enemy).Behavior12();
    }
}
