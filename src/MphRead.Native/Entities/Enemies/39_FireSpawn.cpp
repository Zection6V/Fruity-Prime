#include "39_FireSpawn.hpp"

#include "50_HitZone.hpp"
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
#include <limits>
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

        template <typename T>
        [[nodiscard]] const T& RequireReference(const std::shared_ptr<const T>& value)
        {
            if (!value)
            {
                throw System::NullReferenceException();
            }
            return *value;
        }

        template <typename T, std::size_t N>
        [[nodiscard]] T& ArrayAt(std::array<T, N>& values, std::int32_t index)
        {
            if (index < 0 || static_cast<std::size_t>(index) >= N)
            {
                throw SceneDetail::IndexOutOfRangeException();
            }
            return values[static_cast<std::size_t>(index)];
        }

        template <typename T, std::size_t N>
        [[nodiscard]] const T& ArrayAt(
            const std::array<T, N>& values, std::int32_t index)
        {
            if (index < 0 || static_cast<std::size_t>(index) >= N)
            {
                throw SceneDetail::IndexOutOfRangeException();
            }
            return values[static_cast<std::size_t>(index)];
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

        [[nodiscard]] EnemySpawnEntity* CastSpawner(EntityBase* spawner) noexcept
        {
            EnemySpawnEntity* typedSpawner = dynamic_cast<EnemySpawnEntity*>(spawner);
            assert(typedSpawner != nullptr);
            return typedSpawner;
        }

        [[nodiscard]] Enemy39Entity& RequireEnemy(Enemy39Entity* enemy)
        {
            return RequireReference(enemy);
        }

        [[nodiscard]] PlayerEntity& MainPlayer()
        {
            return RequireReference(PlayerEntity::Main());
        }

        [[nodiscard]] std::int32_t UInt32ToInt32Unchecked(
            std::uint32_t value) noexcept
        {
            return std::bit_cast<std::int32_t>(value);
        }

        [[nodiscard]] std::int32_t AddInt32Unchecked(
            std::int32_t left, std::int32_t right) noexcept
        {
            const std::uint32_t bits
                = std::bit_cast<std::uint32_t>(left)
                + std::bit_cast<std::uint32_t>(right);
            return std::bit_cast<std::int32_t>(bits);
        }

        [[nodiscard]] std::int32_t SubtractInt32Unchecked(
            std::int32_t left, std::int32_t right) noexcept
        {
            const std::uint32_t bits
                = std::bit_cast<std::uint32_t>(left)
                - std::bit_cast<std::uint32_t>(right);
            return std::bit_cast<std::int32_t>(bits);
        }

        [[nodiscard]] std::int32_t MultiplyInt32Unchecked(
            std::int32_t left, std::int32_t right) noexcept
        {
            const std::uint32_t bits
                = std::bit_cast<std::uint32_t>(left)
                * std::bit_cast<std::uint32_t>(right);
            return std::bit_cast<std::int32_t>(bits);
        }

        [[nodiscard]] constexpr Vector3 WithY(Vector3 value, float y) noexcept
        {
            value.Y = y;
            return value;
        }

        [[nodiscard]] constexpr Vector3 AddY(Vector3 value, float y) noexcept
        {
            value.Y += y;
            return value;
        }

        void SetTranslation(Matrix4& transform, Vector3 position) noexcept
        {
            transform.M41 = position.X;
            transform.M42 = position.Y;
            transform.M43 = position.Z;
        }

        [[nodiscard]] Matrix4 ClearScale(Matrix4 transform)
        {
            const Vector3 row0
                = Vector3(transform.M11, transform.M12, transform.M13).Normalized();
            const Vector3 row1
                = Vector3(transform.M21, transform.M22, transform.M23).Normalized();
            const Vector3 row2
                = Vector3(transform.M31, transform.M32, transform.M33).Normalized();

            transform.M11 = row0.X;
            transform.M12 = row0.Y;
            transform.M13 = row0.Z;
            transform.M21 = row1.X;
            transform.M22 = row1.Y;
            transform.M23 = row1.Z;
            transform.M31 = row2.X;
            transform.M32 = row2.Y;
            transform.M33 = row2.Z;
            return transform;
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

        [[nodiscard]] float DegreesToRadians(float degrees) noexcept
        {
            return degrees * (3.14159265358979323846F / 180.0F);
        }

        [[nodiscard]] float RoundToEven(float value) noexcept
        {
            if (!std::isfinite(value))
            {
                return value;
            }
            const float lower = std::floor(value);
            const float fraction = value - lower;
            if (fraction < 0.5F)
            {
                return lower;
            }
            if (fraction > 0.5F)
            {
                return lower + 1.0F;
            }
            return std::fmod(lower, 2.0F) == 0.0F ? lower : lower + 1.0F;
        }

        [[nodiscard]] std::uint32_t RoundRadiusToUInt32(float radius) noexcept
        {
            const float rounded = RoundToEven(radius * 4096.0F);
            if (std::isnan(rounded) || rounded <= 0.0F)
            {
                return 0U;
            }
            if (static_cast<double>(rounded)
                >= static_cast<double>(std::numeric_limits<std::uint32_t>::max()))
            {
                return std::numeric_limits<std::uint32_t>::max();
            }
            return static_cast<std::uint32_t>(rounded);
        }

        [[nodiscard]] std::uint32_t RandomAttackCount(
            const Enemy39Values& values)
        {
            const std::int32_t range
                = static_cast<std::int32_t>(values.AttackCountMax)
                + 1
                - static_cast<std::int32_t>(values.AttackCountMin);
            return static_cast<std::uint32_t>(values.AttackCountMin)
                + Rng::GetRandomInt2(static_cast<std::uint32_t>(range));
        }

        [[nodiscard]] std::uint32_t RandomDiveTimer(
            const Enemy39Values& values)
        {
            const std::int32_t range
                = static_cast<std::int32_t>(values.DiveTimerMax)
                + 1
                - static_cast<std::int32_t>(values.DiveTimerMin);
            return static_cast<std::uint32_t>(values.DiveTimerMin)
                + Rng::GetRandomInt2(static_cast<std::uint32_t>(range));
        }

    }

    const std::array<std::int32_t, 11> Enemy39Entity::_recolors{
        0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1
    };

    Enemy39Entity::Enemy39Entity(EnemyInstanceEntityData data,
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

    Enemy39Values Enemy39Entity::Values() const noexcept
    {
        return _values;
    }

    void Enemy39Entity::EnemyInitialize()
    {
        EnemySpawnEntity& spawner = RequireReference(_spawner);

        const std::int32_t version
            = UInt32ToInt32Unchecked(spawner.Data.Fields.S06().EnemyVersion);
        SetRecolor(ArrayAt(_recolors, version));

        Flags |= EnemyFlags::Visible;
        Flags |= EnemyFlags::Invincible;
        Flags |= EnemyFlags::OnRadar;
        Flags &= ~EnemyFlags::CollidePlayer;

        const Vector3 position = RequireReference(_data.Spawner).Position;
        Matrix4 transform = GetTransformMatrix(
            (static_cast<Vector3>(MainPlayer().Position) - position).Normalized(),
            Vector3(0.0F, 1.0F, 0.0F));
        SetTranslation(transform, position);
        Transform = transform;

        _boundingRadius = 1.0F;
        _hurtVolumeInit = CollisionVolume(
            Vector3(0.0F, Fixed::ToFloat(13516), Fixed::ToFloat(11059)),
            0.5F);

        ModelInstance& inst = SetUpModel(
            "LavaDemon", 1, AnimFlags::Paused);

        const std::int32_t subtype
            = UInt32ToInt32Unchecked(spawner.Data.Fields.S06().EnemySubtype);
        _values = VectorAt(Metadata::Enemy39Values, subtype);
        _health = _healthMax = _values.HealthMax;

        AnimationInfo& initAnim = RequireReference(inst.AnimInfo);
        _animFrameCount = RequireReference(initAnim.FrameCount)[0];

        _activeVolume = CollisionVolume::Move(
            spawner.Data.Fields.S06().Volume2, Position);
        _locationVolume = CollisionVolume::Move(
            spawner.Data.Fields.S06().Volume1, Position);

        Metadata::LoadEffectiveness(_values.Effectiveness, BeamEffectiveness);
        _scanId = _values.ScanId;

        const std::shared_ptr<WeaponInfo> weapon
            = VectorAt(RequireReference(Weapons::EnemyWeapons), version);

        _equipInfo[0] = std::make_shared<EquipInfo>(weapon, _beams);
        _equipInfo[1] = std::make_shared<EquipInfo>(weapon, _beams);

        _equipInfo[0]->SetGetAmmo([this]() { return _ammo0; });
        _equipInfo[0]->SetSetAmmo(
            [this](std::int32_t newAmmo) { _ammo0 = newAmmo; });
        _equipInfo[1]->SetGetAmmo([this]() { return _ammo1; });
        _equipInfo[1]->SetSetAmmo(
            [this](std::int32_t newAmmo) { _ammo1 = newAmmo; });

        _equipInfo[0]->UnchargedDamage(_values.BeamDamage);
        _equipInfo[0]->SplashDamage(_values.SplashDamage);
        _equipInfo[1]->UnchargedDamage(_values.BeamDamage);
        _equipInfo[1]->SplashDamage(_values.SplashDamage);

        _attackDelay = static_cast<std::int32_t>(_values.AttackDelay) * 2;
        _attackCount = RandomAttackCount(_values);
        SetHealthbarMessageId(4);

        if (spawner.Data.Fields.S06().EnemySubtype == 1U)
        {
            Model& model = RequireReference(inst.Model());
            const std::vector<std::shared_ptr<Material>>& materials
                = RequireReference(model.Materials);
            RequireReference(VectorAt(materials, 0)).Ambient
                = ColorRgb(18, 27, 31);
            SetHealthbarMessageId(5);
        }

        inst.SetAnimation(3, AnimFlags::Paused);
        _wristNodeL = RequireReference(inst.Model()).GetNodeByName("Wrist_L");
        _wristNodeR = RequireReference(inst.Model()).GetNodeByName("Wrist_R");

        std::shared_ptr<EnemyInstanceEntity> enemy
            = EnemySpawnEntity::SpawnEnemy(
                this, MphRead::EnemyType::HitZone, NodeRef, _scene);
        _hitZone = std::dynamic_pointer_cast<Enemy50Entity>(enemy);
        if (_hitZone)
        {
            Scene& scene = RequireReference(_scene);
            scene.AddEntity(_hitZone);
            _hitZone->Transform = ClearScale(static_cast<Matrix4>(Transform));
            Metadata::LoadEffectiveness(
                _values.Effectiveness, _hitZone->BeamEffectiveness);
            _hitZone->Flags |= EnemyFlags::Invincible;
            _hitZone->Flags &= ~EnemyFlags::CollidePlayer;
            _hitZone->Flags &= ~EnemyFlags::CollideBeam;

            const Vector3 cylPos(
                0.0F, Fixed::ToFloat(-2867), 0.0F);
            const CollisionVolume hurtVolume(
                Vector3(0.0F, 1.0F, 0.0F),
                cylPos,
                Fixed::ToFloat(10649),
                Fixed::ToFloat(13844));
            _hitZone->SetUp(1, hurtVolume, 1.0F);
        }
    }

    void Enemy39Entity::EnemyProcess()
    {
        (void)ContactDamagePlayer(_values.ContactDamage, true);
        if (_state1 == 4)
        {
            assert(_wristNodeL != nullptr);
            assert(_wristNodeR != nullptr);
            const Node& wristNodeL = RequireReference(_wristNodeL);
            const Node& wristNodeR = RequireReference(_wristNodeR);
            _wristPos[0] = wristNodeL.Animation.Row3().Xyz();
            _wristPos[1] = wristNodeR.Animation.Row3().Xyz();
        }
        CallStateProcess();
    }

    bool Enemy39Entity::EnemyTakeDamage(EntityBase* source)
    {
        (void)source;
        if (_health == 0)
        {
            _soundSource.PlaySfx(
                SfxId::LAVA_DEMON_DIE_SCR,
                false,
                true);

            if (_effectEntry)
            {
                RequireReference(_scene).UnlinkEffectEntry(_effectEntry);
                _effectEntry.reset();
            }

            if (_hitZone)
            {
                _hitZone->SetHealth(0);
                _hitZone.reset();
            }
        }
        return false;
    }

    void Enemy39Entity::State0()
    {
        const Vector3 facing = WithY(
            (static_cast<Vector3>(MainPlayer().Position)
                - static_cast<Vector3>(Position)).Normalized(),
            0.0F);
        Matrix4 transform = GetTransformMatrix(
            facing, Vector3(0.0F, 1.0F, 0.0F));
        SetTranslation(transform, Position);
        Transform = transform;
        (void)CallSubroutine<Enemy39Entity>(
            Metadata::Enemy39Subroutines, this);
    }

    void Enemy39Entity::State1()
    {
        (void)CallSubroutine<Enemy39Entity>(
            Metadata::Enemy39Subroutines, this);
    }

    void Enemy39Entity::State2()
    {
        if (_tangibilityTimer == 5 * 2)
        {
            assert(_hitZone != nullptr);
            Enemy50Entity& hitZone = RequireReference(_hitZone);
            hitZone.Flags |= EnemyFlags::CollidePlayer;
            hitZone.Flags |= EnemyFlags::CollideBeam;
            hitZone.HitPlayers[0] = true;
            Flags |= EnemyFlags::CollidePlayer;
            Flags |= EnemyFlags::CollideBeam;
            HitPlayers[0] = true;
        }
        if (_tangibilityTimer <= 5 * 2)
        {
            _tangibilityTimer = AddInt32Unchecked(_tangibilityTimer, 1);
        }
        State0();
    }

    void Enemy39Entity::State3()
    {
        State0();
    }

    void Enemy39Entity::State4()
    {
        State0();

        if (_attackCount > 0
            && _animFrameCount > 0
            && RequireReference(_scene).FrameCount() != 0
            && RequireReference(_scene).FrameCount() % 2 == 0)
        {
            ModelInstance& model = _models[0];
            AnimationInfo& anim = RequireReference(model.AnimInfo);
            ManagedArray<AnimFlags>& flags = RequireReference(anim.Flags);

            if (TypeExtensions::TestFlag(flags[0], AnimFlags::Ended))
            {
                _soundSource.PlaySfx(SfxId::LAVA_DEMON_ATTACK_SCR);

                ManagedArray<std::int32_t>& indices
                    = RequireReference(anim.Index);
                if (indices[0] == 1)
                {
                    model.SetAnimation(0, AnimFlags::NoLoop);
                    _animFrameCount
                        = RequireReference(anim.FrameCount)[0];
                    _wristId = 0;
                }
                else if (indices[0] == 0)
                {
                    model.SetAnimation(1, AnimFlags::NoLoop);
                    _animFrameCount
                        = RequireReference(anim.FrameCount)[0];
                    _wristId = 1;
                }
            }

            if (_animFrameCount == 53)
            {
                CreateEffect();
            }
            else if (_animFrameCount == 25)
            {
                --_attackCount;
                _attackDelay
                    = static_cast<std::int32_t>(_values.AttackDelay) * 2;

                Vector3 dir = AddY(
                    static_cast<Vector3>(MainPlayer().Position), 0.5F)
                    - ArrayAt(_wristPos, _wristId);
                dir = dir.Normalized();

                std::shared_ptr<EquipInfo> equipInfo
                    = ArrayAt(_equipInfo, _wristId);
                EquipInfo& equip = RequireReference(equipInfo);
                equip.UnchargedDamage(_values.BeamDamage);
                equip.SplashDamage(_values.SplashDamage);
                equip.HeadshotDamage(_values.BeamDamage);

                (void)BeamProjectileEntity::Spawn(
                    SharedFrom<EntityBase>(this),
                    equipInfo,
                    ArrayAt(_wristPos, _wristId),
                    dir,
                    BeamSpawnFlags::None,
                    NodeRef,
                    _scene);

                if (_effectEntry)
                {
                    RequireReference(_scene).DetachEffectEntry(
                        _effectEntry, true);
                    _effectEntry.reset();
                }
            }
            else if (_animFrameCount >= 26 && _animFrameCount <= 52)
            {
                if (_effectEntry)
                {
                    const Vector3 wristPos
                        = ArrayAt(_wristPos, _wristId);
                    const Matrix4 transform
                        = ClearScale(static_cast<Matrix4>(Transform));
                    _effectEntry->Transform(wristPos, transform);
                }
            }

            _animFrameCount
                = SubtractInt32Unchecked(_animFrameCount, 1);
        }
    }

    void Enemy39Entity::CreateEffect()
    {
        Matrix4 transform = GetTransformMatrix(
            Vector3(1.0F, 0.0F, 0.0F),
            Vector3(0.0F, 1.0F, 0.0F));
        SetTranslation(transform, ArrayAt(_wristPos, _wristId));

        const std::int32_t effectId
            = RequireReference(_spawner).Data.Fields.S06().EnemySubtype == 1U
            ? 96
            : 94;
        _effectEntry
            = RequireReference(_scene).SpawnEffectGetEntry(effectId, transform);
        if (_effectEntry)
        {
            _effectEntry->SetElementExtension(true);
        }
    }

    void Enemy39Entity::State5()
    {
        if (_tangibilityTimer == 18 * 2)
        {
            assert(_hitZone != nullptr);
            Enemy50Entity& hitZone = RequireReference(_hitZone);
            hitZone.Flags &= ~EnemyFlags::CollidePlayer;
            hitZone.Flags &= ~EnemyFlags::CollideBeam;
            hitZone.ClearHitPlayers();
            Flags &= ~EnemyFlags::CollidePlayer;
            Flags &= ~EnemyFlags::CollideBeam;
            ClearHitPlayers();
        }
        if (_tangibilityTimer <= 18 * 2)
        {
            _tangibilityTimer = AddInt32Unchecked(_tangibilityTimer, 1);
        }
        State0();
    }

    bool Enemy39Entity::Behavior0()
    {
        if (!_activeVolume.TestPoint(MainPlayer().Position))
        {
            return false;
        }

        ChooseSurfaceLocation();
        _diveTimer = RandomDiveTimer(_values);
        _diveTimer *= 2U;
        return true;
    }

    void Enemy39Entity::ChooseSurfaceLocation()
    {
        float distance = 0.0F;

        if (_locationVolume.Type == VolumeType::Cylinder)
        {
            const std::uint32_t radius
                = RoundRadiusToUInt32(_locationVolume.CylinderRadius);
            distance = static_cast<float>(Rng::GetRandomInt2(radius))
                / 4096.0F;
        }
        else if (_locationVolume.Type == VolumeType::Sphere)
        {
            const std::uint32_t radius
                = RoundRadiusToUInt32(_locationVolume.SphereRadius);
            distance = static_cast<float>(Rng::GetRandomInt2(radius))
                / 4096.0F;
        }

        Vector3 vec(distance, 0.0F, 0.0F);
        _surfaceDirection
            = MultiplyInt32Unchecked(_surfaceDirection, -1);

        const float angle
            = static_cast<float>(Rng::GetRandomInt2(0xB4000))
            / 4096.0F;
        const Matrix4 rotation = CreateRotationY(DegreesToRadians(
            angle * static_cast<float>(_surfaceDirection)));
        vec = Matrix::Vec3MultMtx3(vec, rotation);

        Vector3 position = Vector3::Zero;
        if (_locationVolume.Type == VolumeType::Cylinder)
        {
            position = Vector3(
                _locationVolume.CylinderPosition.X + vec.X,
                Position.Y,
                _locationVolume.CylinderPosition.Z + vec.Z);
        }
        else if (_locationVolume.Type == VolumeType::Sphere)
        {
            position = Vector3(
                _locationVolume.SpherePosition.X + vec.X,
                Position.Y,
                _locationVolume.SpherePosition.Z + vec.Z);
        }
        Position = position;
    }

    bool Enemy39Entity::Behavior1()
    {
        if (_attackDelay > 0)
        {
            _attackDelay = SubtractInt32Unchecked(_attackDelay, 1);
            return false;
        }

        _models[0].SetAnimation(1, AnimFlags::NoLoop);
        _attackDelay = static_cast<std::int32_t>(_values.AttackDelay) * 2;
        _soundSource.PlaySfx(SfxId::LAVA_DEMON_ATTACK_SCR);
        return true;
    }

    bool Enemy39Entity::Behavior2()
    {
        AnimationInfo& anim = RequireReference(_models[0].AnimInfo);
        if (!TypeExtensions::TestFlag(
                RequireReference(anim.Flags)[0], AnimFlags::Ended))
        {
            return false;
        }

        Matrix4 transform = GetTransformMatrix(
            Vector3(1.0F, 0.0F, 0.0F),
            Vector3(0.0F, 1.0F, 0.0F));
        SetTranslation(transform, Position);

        const std::int32_t effectId
            = RequireReference(_spawner).Data.Fields.S06().EnemySubtype == 1U
            ? 133
            : 93;
        RequireReference(_scene).SpawnEffect(effectId, transform);
        _models[0].SetAnimation(3, AnimFlags::Paused);
        _tangibilityTimer = 0;
        return true;
    }

    bool Enemy39Entity::Behavior3()
    {
        AnimationInfo& anim = RequireReference(_models[0].AnimInfo);
        if (!TypeExtensions::TestFlag(
                RequireReference(anim.Flags)[0], AnimFlags::Ended))
        {
            return false;
        }

        Flags |= EnemyFlags::CollidePlayer;
        Flags |= EnemyFlags::CollideBeam;
        Flags &= ~EnemyFlags::Invincible;
        _tangibilityTimer = 0;
        return true;
    }

    bool Enemy39Entity::Behavior4()
    {
        if (_diveTimer > 0)
        {
            --_diveTimer;
            return false;
        }

        Matrix4 transform = GetTransformMatrix(
            Vector3(1.0F, 0.0F, 0.0F),
            Vector3(0.0F, 1.0F, 0.0F));
        SetTranslation(transform, Position);

        const std::int32_t effectId
            = RequireReference(_spawner).Data.Fields.S06().EnemySubtype == 1U
            ? 132
            : 95;
        RequireReference(_scene).SpawnEffect(effectId, transform);
        _models[0].SetAnimation(3, AnimFlags::NoLoop);
        _soundSource.PlaySfx(SfxId::LAVA_DEMON_APPEAR_SCR);
        return true;
    }

    bool Enemy39Entity::Behavior5()
    {
        if (_attackCount > 0)
        {
            return false;
        }
        return StartSubmerge();
    }

    bool Enemy39Entity::Behavior6()
    {
        if (_activeVolume.TestPoint(MainPlayer().Position))
        {
            return false;
        }
        return StartSubmerge();
    }

    bool Enemy39Entity::StartSubmerge()
    {
        AnimationInfo& anim = RequireReference(_models[0].AnimInfo);
        if (!TypeExtensions::TestFlag(
                RequireReference(anim.Flags)[0], AnimFlags::Ended))
        {
            return false;
        }

        _animFrameCount = RequireReference(anim.FrameCount)[0];
        _soundSource.PlaySfx(SfxId::LAVA_DEMON_DISAPPEAR_SCR);
        _models[0].SetAnimation(2, AnimFlags::NoLoop);
        _attackCount = RandomAttackCount(_values);
        _wristId = 1;
        _tangibilityTimer = 0;
        Flags |= EnemyFlags::Invincible;
        return true;
    }

    void Enemy39Entity::Destroy()
    {
        if (_effectEntry)
        {
            RequireReference(_scene).UnlinkEffectEntry(_effectEntry);
            _effectEntry.reset();
        }
        EnemyInstanceEntity::Destroy();
    }

    bool Enemy39Entity::Behavior0(Enemy39Entity* enemy)
    {
        return RequireEnemy(enemy).Behavior0();
    }

    bool Enemy39Entity::Behavior1(Enemy39Entity* enemy)
    {
        return RequireEnemy(enemy).Behavior1();
    }

    bool Enemy39Entity::Behavior2(Enemy39Entity* enemy)
    {
        return RequireEnemy(enemy).Behavior2();
    }

    bool Enemy39Entity::Behavior3(Enemy39Entity* enemy)
    {
        return RequireEnemy(enemy).Behavior3();
    }

    bool Enemy39Entity::Behavior4(Enemy39Entity* enemy)
    {
        return RequireEnemy(enemy).Behavior4();
    }

    bool Enemy39Entity::Behavior5(Enemy39Entity* enemy)
    {
        return RequireEnemy(enemy).Behavior5();
    }

    bool Enemy39Entity::Behavior6(Enemy39Entity* enemy)
    {
        return RequireEnemy(enemy).Behavior6();
    }
}
