#include "41_Slench.hpp"

#include "42_SlenchShield.hpp"
#include "44_SlenchSynapse.hpp"
#include "../../Features.hpp"
#include "../../Formats/CollisionDetection.hpp"
#include "../../Formats/Effects.hpp"
#include "../../GameState.hpp"
#include "../../Metadata/Enemies.hpp"
#include "../../Metadata/SoundMeta.hpp"
#include "../../Metadata/Weapons.hpp"
#include "../../Scene.hpp"
#include "../../Utility/Rng.hpp"
#include "../BeamProjectileEntity.hpp"
#include "../DoorEntity.hpp"
#include "../EnemySpawnEntity.hpp"
#include "../Players/PlayerEntity.hpp"

#include <array>
#include <bit>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <stdexcept>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

namespace MphRead::Entities::Enemies
{
    namespace
    {
        using OpenTK::Mathematics::Matrix3;
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

        template <typename TEnum>
        [[nodiscard]] constexpr bool TestFlag(TEnum value, TEnum flag) noexcept
        {
            using Underlying = std::underlying_type_t<TEnum>;
            return (static_cast<Underlying>(value) & static_cast<Underlying>(flag))
                == static_cast<Underlying>(flag);
        }

        [[nodiscard]] EnemySpawnEntity* CastSpawner(EntityBase* spawner) noexcept
        {
            EnemySpawnEntity* typedSpawner = dynamic_cast<EnemySpawnEntity*>(spawner);
            assert(typedSpawner != nullptr);
            return typedSpawner;
        }

        [[nodiscard]] PlayerEntity& MainPlayer()
        {
            return RequireReference(PlayerEntity::Main());
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

        [[nodiscard]] bool Equal(Vector3 left, Vector3 right) noexcept
        {
            return left.X == right.X && left.Y == right.Y && left.Z == right.Z;
        }

        [[nodiscard]] float LengthSquared(Vector3 value) noexcept
        {
            return value.X * value.X + value.Y * value.Y + value.Z * value.Z;
        }

        [[nodiscard]] float Length(Vector3 value)
        {
            return std::sqrt(LengthSquared(value));
        }

        [[nodiscard]] Vector3 ScaleVector(Vector3 value, float factor) noexcept
        {
            return Vector3(
                value.X * factor, value.Y * factor, value.Z * factor);
        }

        [[nodiscard]] Vector3 WithY(Vector3 value, float y) noexcept
        {
            value.Y = y;
            return value;
        }

        [[nodiscard]] Vector3 AddY(Vector3 value, float amount) noexcept
        {
            value.Y += amount;
            return value;
        }

        [[nodiscard]] float DegreesToRadians(float degrees) noexcept
        {
            return degrees * (3.14159265358979323846F / 180.0F);
        }

        [[nodiscard]] float RadiansToDegrees(float radians) noexcept
        {
            return radians * (180.0F / 3.14159265358979323846F);
        }

        [[nodiscard]] Matrix4 CreateFromAxisAngle(Vector3 axis, float angle) noexcept
        {
            axis = axis.Normalized();
            const float cosine = std::cos(angle);
            const float sine = std::sin(angle);
            const float oneMinusCosine = 1.0F - cosine;
            const float x = axis.X;
            const float y = axis.Y;
            const float z = axis.Z;
            return Matrix4(
                Vector4(
                    oneMinusCosine * x * x + cosine,
                    oneMinusCosine * x * y + sine * z,
                    oneMinusCosine * x * z - sine * y,
                    0.0F),
                Vector4(
                    oneMinusCosine * x * y - sine * z,
                    oneMinusCosine * y * y + cosine,
                    oneMinusCosine * y * z + sine * x,
                    0.0F),
                Vector4(
                    oneMinusCosine * x * z + sine * y,
                    oneMinusCosine * y * z - sine * x,
                    oneMinusCosine * z * z + cosine,
                    0.0F),
                Vector4(0.0F, 0.0F, 0.0F, 1.0F));
        }

        [[nodiscard]] Vector3 Vec3MultMtx3(Vector3 value, Matrix4 matrix) noexcept
        {
            return Vector3(
                value.X * matrix.M11 + value.Y * matrix.M21 + value.Z * matrix.M31,
                value.X * matrix.M12 + value.Y * matrix.M22 + value.Z * matrix.M32,
                value.X * matrix.M13 + value.Y * matrix.M23 + value.Z * matrix.M33);
        }

        [[nodiscard]] Vector3 Vec3MultMtx3(Vector3 value, Matrix3 matrix) noexcept
        {
            return Vector3(
                value.X * matrix.M11 + value.Y * matrix.M21 + value.Z * matrix.M31,
                value.X * matrix.M12 + value.Y * matrix.M22 + value.Z * matrix.M32,
                value.X * matrix.M13 + value.Y * matrix.M23 + value.Z * matrix.M33);
        }

        void SetRow3(Matrix4& matrix, Vector3 value) noexcept
        {
            matrix.M41 = value.X;
            matrix.M42 = value.Y;
            matrix.M43 = value.Z;
        }

        [[nodiscard]] std::int32_t AddInt32Unchecked(
            std::int32_t left, std::int32_t right) noexcept
        {
            const std::uint32_t result
                = std::bit_cast<std::uint32_t>(left)
                + std::bit_cast<std::uint32_t>(right);
            return std::bit_cast<std::int32_t>(result);
        }

        [[nodiscard]] std::int32_t SubInt32Unchecked(
            std::int32_t left, std::int32_t right) noexcept
        {
            const std::uint32_t result
                = std::bit_cast<std::uint32_t>(left)
                - std::bit_cast<std::uint32_t>(right);
            return std::bit_cast<std::int32_t>(result);
        }

        [[nodiscard]] std::int32_t MulInt32Unchecked(
            std::int32_t left, std::int32_t right) noexcept
        {
            const std::uint32_t result
                = std::bit_cast<std::uint32_t>(left)
                * std::bit_cast<std::uint32_t>(right);
            return std::bit_cast<std::int32_t>(result);
        }

        [[nodiscard]] std::int32_t DivideInt32(
            std::int32_t dividend, std::int32_t divisor)
        {
            if (divisor == 0)
            {
                throw std::domain_error("Attempted to divide by zero.");
            }
            if (dividend == std::numeric_limits<std::int32_t>::min() && divisor == -1)
            {
                throw std::overflow_error("Arithmetic operation resulted in an overflow.");
            }
            return dividend / divisor;
        }

        [[nodiscard]] bool AnimationEnded(ModelInstance& model)
        {
            AnimationInfo& animInfo = RequireReference(model.AnimInfo);
            ManagedArray<AnimFlags>& flags = RequireReference(animInfo.Flags);
            if (flags.Length() == 0)
            {
                throw SceneDetail::IndexOutOfRangeException();
            }
            return TestFlag(flags[0], AnimFlags::Ended);
        }
    }

    const std::array<float, 10> Enemy41Entity::_recoilLut{
        0.2F, 0.4F, 0.55F, 0.7F, 0.775F,
        0.85F, 0.875F, 0.9F, 0.95F, 1.0F
    };

    const std::array<Movie, 4> Enemy41Entity::_deathMovieIds{
        Movie::SlenchAlinos1Defeat,
        Movie::SlenchArcterra1Defeat,
        Movie::SlenchCA2Defeat,
        Movie::SlenchVDO2Defeat
    };

    Enemy41Entity::Enemy41Entity(EnemyInstanceEntityData data,
        Formats::Culling::NodeRef nodeRef, Scene* scene)
        : EnemyInstanceEntity(data, nodeRef, scene),
          _spawner(CastSpawner(data.Spawner))
    {
    }

    Enemies::SlenchFlags Enemy41Entity::SlenchFlags() const noexcept
    {
        return _slenchFlags;
    }

    std::int32_t Enemy41Entity::Subtype() const noexcept
    {
        return _subtype;
    }

    std::int32_t Enemy41Entity::Phase() const noexcept
    {
        return _phase;
    }

    float Enemy41Entity::ShieldOffset() const noexcept
    {
        return _shieldOffset;
    }

    std::int32_t Enemy41Entity::SynapseIndex() const noexcept
    {
        return _synapseIndex;
    }

    Enemy41Values Enemy41Entity::GetValues()
    {
        const std::int32_t index = MulInt32Unchecked(_subtype, 3);
        return VectorAt(Metadata::Enemy41Values, index);
    }

    Enemy41Values Enemy41Entity::GetPhaseValues()
    {
        const std::int32_t index = AddInt32Unchecked(
            MulInt32Unchecked(_subtype, 3), _phase);
        return VectorAt(Metadata::Enemy41Values, index);
    }

    SlenchState Enemy41Entity::State() const noexcept
    {
        return static_cast<SlenchState>(_state1);
    }

    void Enemy41Entity::EnemyInitialize()
    {
        Scene& scene = RequireReference(_scene);
        _slenchFlags = Enemies::SlenchFlags::Floating;
        if (scene.RoomId() == 82)
        {
            _subtype = 1;
        }
        else if (scene.RoomId() == 64)
        {
            _subtype = 2;
        }
        else if (scene.RoomId() == 76)
        {
            _subtype = 3;
            _slenchFlags = Enemies::SlenchFlags::Rolling;
        }

        EntityBase& dataSpawner = RequireReference(_data.Spawner);
        const Vector3 position = dataSpawner.Position;
        const Vector3 facing = dataSpawner.FacingVector();
        const Vector3 up = dataSpawner.UpVector();
        Matrix4 transform = GetTransformMatrix(facing, up);
        SetRow3(transform, position);
        Transform = transform;

        const Enemy41Values values = GetValues();
        _health = _healthMax = static_cast<std::uint16_t>(
            static_cast<std::uint32_t>(values.Health));
        Flags |= EnemyFlags::Visible;
        Flags |= EnemyFlags::Invincible;
        Flags |= EnemyFlags::CollidePlayer;
        Flags |= EnemyFlags::NoMaxDistance;
        Flags |= EnemyFlags::OnRadar;
        SetHealthbarMessageId(2);
        Metadata::LoadEffectiveness(
            VectorAt(Metadata::SlenchEffectiveness, _subtype), BeamEffectiveness);

        EnemySpawnEntity& spawner = RequireReference(_spawner);
        _hurtVolumeInit = CollisionVolume(spawner.Data.Fields.S00().Volume0);
        _boundingRadius = _hurtVolumeInit.SphereRadius;
        _hurtVolumeInit = CollisionVolume(_hurtVolumeInit.SpherePosition, 2.9F);
        _shieldOffset = _hurtVolumeInit.SphereRadius;
        _model = &SetUpModel("BigEyeBall", 10, AnimFlags::NoLoop);
        _recoilTimer = 1000;

        const std::shared_ptr<WeaponInfo> slenchTear
            = VectorAt(RequireReference(Weapons::BossWeapons), 3);
        _equipInfo = std::make_shared<EquipInfo>(slenchTear, _beams);
        _equipInfo->SetGetAmmo([this]() { return _ammo; });
        _equipInfo->SetSetAmmo(
            [this](std::int32_t newAmmo) { _ammo = newAmmo; });

        if (facing.Y >= -0.5F)
        {
            const Vector3 normalized = WithY(facing, 0.0F).Normalized();
            _targetX = normalized.X;
            _targetZ = normalized.Z;
        }
        else
        {
            const Vector3 normalized = WithY(up, 0.0F).Normalized();
            _targetX = normalized.X;
            _targetZ = normalized.Z;
            _targetAngle = 90.0F;
        }

        UpdateFacing();
        _targetHorizontal = Vector3(_targetX, 0.0F, _targetZ);
        _startPos = position;
        _detachedPosition = ScaleVector(facing, 3.0F) + position;
        _detachedFacing = _detachedPosition + facing;
        ChangeState(SlenchState::Initial);

        std::shared_ptr<EnemyInstanceEntity> shieldEnemy
            = EnemySpawnEntity::SpawnEnemy(
                this, EnemyType::SlenchShield, NodeRef, _scene);
        std::shared_ptr<Enemy42Entity> shield
            = std::dynamic_pointer_cast<Enemy42Entity>(shieldEnemy);
        if (!shield)
        {
            return;
        }
        scene.AddEntity(shield);
        _shield = shield;

        for (std::int32_t i = 0; i < _synapseCount; ++i)
        {
            _synapseIndex = i;
            std::shared_ptr<EnemyInstanceEntity> synapseEnemy
                = EnemySpawnEntity::SpawnEnemy(
                    this, EnemyType::SlenchSynapse, NodeRef, _scene);
            std::shared_ptr<Enemy44Entity> synapse
                = std::dynamic_pointer_cast<Enemy44Entity>(synapseEnemy);
            if (!synapse)
            {
                return;
            }
            scene.AddEntity(synapse);
            ArrayAt(_synapses, i) = std::move(synapse);
        }

        UpdateScanId(values.ScanId1);
    }

    void Enemy41Entity::UpdateFacing()
    {
        Vector3 facing(_targetX, 0.0F, _targetZ);
        const Vector3 axis(_targetZ, 0.0F, -_targetX);
        const Matrix4 rotMtx
            = CreateFromAxisAngle(axis, DegreesToRadians(_targetAngle));
        facing = Vec3MultMtx3(facing, rotMtx).Normalized();
        const Vector3 up = Vector3::Cross(facing, axis).Normalized();
        Matrix4 transform = GetTransformMatrix(facing, up);
        SetRow3(transform, Position);
        Transform = transform;
    }

    void Enemy41Entity::ChangeState(SlenchState state)
    {
        _hitFloor = false;
        _slenchFlags &= ~Enemies::SlenchFlags::TargetingPlayer;
        _slenchFlags &= ~Enemies::SlenchFlags::Vulnerable;
        _slenchFlags &= ~Enemies::SlenchFlags::Wobbling;
        const Enemy41Values phaseValues = GetPhaseValues();

        switch (state)
        {
        case SlenchState::Initial:
        case SlenchState::Slam:
        case SlenchState::SlamReturn:
            CloseEye();
            break;

        case SlenchState::Intro:
            _soundSource.PlaySfx(SfxId::BIGEYE_INTRO_SCR);
            _slenchFlags &= ~Enemies::SlenchFlags::Detached;
            _slenchFlags |= Enemies::SlenchFlags::EyeClosed;
            RequireReference(_model).SetAnimation(13, AnimFlags::NoLoop);
            break;

        case SlenchState::ShieldRaise:
            _shieldEffect2 = RequireReference(_scene).SpawnEffectGetEntry(
                69,
                Vector3(1.0F, 0.0F, 0.0F),
                Vector3(0.0F, 1.0F, 0.0F),
                Position);
            break;

        case SlenchState::Idle:
        {
            CloseEye();
            const std::int32_t min
                = static_cast<std::int32_t>(phaseValues.MinStaticShotTimer) * 2;
            const std::int32_t max
                = static_cast<std::int32_t>(phaseValues.MaxStaticShotTimer) * 2;
            const std::uint32_t random = Rng::GetRandomInt2(
                SubInt32Unchecked(max, min));
            _staticShotTimer = std::bit_cast<std::int32_t>(
                static_cast<std::uint32_t>(min) + random);
            break;
        }

        case SlenchState::ShootTear:
        {
            _soundSource.PlaySfx(SfxId::BIGEYE_ATTACK1A_SCR);
            _slenchFlags &= ~Enemies::SlenchFlags::Detached;
            OpenEye();
            _staticShotTimer = 0;
            _staticShotCooldown = 0;
            _staticShotCounter = 0;
            const Vector3 up = FacingVector();
            Vector3 facing;
            if (up.Z <= -0.9F || up.Z >= 0.9F)
            {
                facing = Vector3::Cross(
                    Vector3(1.0F, 0.0F, 0.0F), up).Normalized();
            }
            else
            {
                facing = Vector3::Cross(
                    Vector3(0.0F, 0.0F, 1.0F), up).Normalized();
            }
            _shotEffect = RequireReference(_scene).SpawnEffectGetEntry(
                81, facing, up, Position);
            if (_shotEffect)
            {
                _shotEffect->SetElementExtension(false);
            }
            break;
        }

        case SlenchState::ShieldLower:
            CloseEye();
            _slamTimer = 0;
            UpdateScanId(GetValues().ScanId2);
            break;

        case SlenchState::Return:
            CloseEye();
            _slenchFlags &= ~Enemies::SlenchFlags::Detached;
            break;

        case SlenchState::Attach:
            _soundSource.PlaySfx(SfxId::BIGEYE_ATTACH_SCR);
            if (_damageEffect)
            {
                RequireReference(_scene).DetachEffectEntry(_damageEffect, false);
                _damageEffect.reset();
            }
            UpdateScanId(GetValues().ScanId1);
            break;

        case SlenchState::Detach:
            _slenchFlags |= Enemies::SlenchFlags::Detached;
            _patternAngle = 0.0F;
            _floatBaseY = 0.0F;
            _roamTimer = MulInt32Unchecked(phaseValues.RoamTime, 2);
            _slamTimer = 0;
            _slenchFlags &= ~Enemies::SlenchFlags::Rolling;
            _slenchFlags &= ~Enemies::SlenchFlags::Floating;
            _slenchFlags &= ~Enemies::SlenchFlags::Bouncy;
            _slenchFlags &= ~Enemies::SlenchFlags::PatternFlip1;
            _slenchFlags &= ~Enemies::SlenchFlags::PatternFlip2;
            if (_subtype == 3)
            {
                CloseEye();
                _slenchFlags |= Enemies::SlenchFlags::Rolling;
                _rollTimer = phaseValues.RollTime;
            }
            else
            {
                OpenEye();
                _slenchFlags |= Enemies::SlenchFlags::Floating;
            }
            break;

        case SlenchState::RollingDone:
            CloseEye();
            _patternAngle = 0.0F;
            _slenchFlags &= ~Enemies::SlenchFlags::Rolling;
            _slenchFlags &= ~Enemies::SlenchFlags::Bouncy;
            _slenchFlags &= ~Enemies::SlenchFlags::PatternFlip1;
            _slenchFlags &= ~Enemies::SlenchFlags::PatternFlip2;
            _slenchFlags |= Enemies::SlenchFlags::Floating;
            _slenchFlags |= Enemies::SlenchFlags::Vulnerable;
            break;

        case SlenchState::Roam:
            _slenchFlags |= Enemies::SlenchFlags::Detached;
            if (_subtype == 3
                && TestFlag(_slenchFlags, Enemies::SlenchFlags::Rolling))
            {
                CloseEye();
            }
            else
            {
                OpenEye();
                _slenchFlags |= Enemies::SlenchFlags::Vulnerable;
            }
            break;

        case SlenchState::SlamReady:
            _soundSource.PlaySfx(SfxId::BIGEYE_ATTACK3_SCR);
            CloseEye();
            _wobbleTimer = 0;
            _stateAfterSlam = _state1;
            break;

        case SlenchState::Dead:
            _deathTimer = 36 * 2;
            CloseEye();
            RequireReference(_scene).SpawnEffect(
                206, FacingVector(), UpVector(), Position);
            break;
        }

        _state2 = static_cast<std::uint8_t>(state);
    }

    void Enemy41Entity::UpdateScanId(std::int32_t scanId)
    {
        _scanId = scanId;
        if (_shield)
        {
            _shield->UpdateScanId(scanId);
        }
    }

    void Enemy41Entity::OpenEye()
    {
        if (TestFlag(_slenchFlags, Enemies::SlenchFlags::EyeClosed))
        {
            _slenchFlags &= ~Enemies::SlenchFlags::EyeClosed;
            _soundSource.PlaySfx(SfxId::BIGEYE_OPEN);
            const std::int32_t animIndex
                = TestFlag(_slenchFlags, Enemies::SlenchFlags::Detached)
                ? 4
                : 10;
            RequireReference(_model).SetAnimation(animIndex, AnimFlags::NoLoop);
        }
    }

    void Enemy41Entity::CloseEye()
    {
        if (!TestFlag(_slenchFlags, Enemies::SlenchFlags::EyeClosed))
        {
            _slenchFlags |= Enemies::SlenchFlags::EyeClosed;
            _soundSource.PlaySfx(SfxId::BIGEYE_CLOSE);
            const std::int32_t animIndex
                = TestFlag(_slenchFlags, Enemies::SlenchFlags::Detached)
                ? 2
                : 8;
            RequireReference(_model).SetAnimation(animIndex, AnimFlags::NoLoop);
        }
    }

    void Enemy41Entity::EnemyProcess()
    {
        const Enemy41Values phaseValues = GetPhaseValues();
        const Vector3 facing = FacingVector();
        const Vector3 up = UpVector();
        Vector3 playerTarget = AddY(
            MainPlayer().Volume().SpherePosition, 0.5F);

        if (TestFlag(_slenchFlags, Enemies::SlenchFlags::Wobbling))
        {
            _wobbleAngle += 20 / 2;
            if (_wobbleAngle >= 360.0F)
            {
                _wobbleAngle -= 360.0F;
            }
            const Vector3 axis = facing;
            const Matrix4 rotMtx = CreateFromAxisAngle(
                axis, DegreesToRadians(_wobbleAngle));
            playerTarget = playerTarget
                + ScaleVector(Vec3MultMtx3(up, rotMtx).Normalized(), 2.0F);
        }

        if (TestFlag(_slenchFlags, Enemies::SlenchFlags::TargetingPlayer))
        {
            if (_shotCooldown > 0)
            {
                _shotCooldown = SubInt32Unchecked(_shotCooldown, 1);
            }
            else
            {
                EquipInfo& equip = RequireReference(_equipInfo);
                equip.SetWeapon(VectorAt(
                    RequireReference(Weapons::BossWeapons),
                    AddInt32Unchecked(4, _subtype)));
                const Vector3 spawnPos
                    = ScaleVector(facing, _shieldOffset) + static_cast<Vector3>(Position);
                const BeamResultFlags result = BeamProjectileEntity::Spawn(
                    SharedEntity(_scene, this),
                    _equipInfo,
                    spawnPos,
                    facing,
                    BeamSpawnFlags::None,
                    Formats::Culling::NodeRef::None,
                    _scene);
                if (result != BeamResultFlags::NoSpawn)
                {
                    SetRecoilTargetVecs();
                    _soundSource.PlaySfx(SfxId::BIGEYE_ATTACK2);
                    const std::shared_ptr<WeaponInfo> weapon = equip.Weapon;
                    _shotCooldown = MulInt32Unchecked(
                        RequireReference(weapon).ShotCooldown, 2);
                }
            }
        }

        const std::int32_t slotIndex = MainPlayer().SlotIndex();
        if (slotIndex < 0
            || static_cast<std::size_t>(slotIndex) >= HitPlayers.size())
        {
            throw SceneDetail::IndexOutOfRangeException();
        }
        if (HitPlayers[static_cast<std::size_t>(slotIndex)])
        {
            MainPlayer().TakeDamage(35, DamageFlags::None, facing, this);
        }

        if (State() == SlenchState::Roam)
        {
            if (TestFlag(_slenchFlags, Enemies::SlenchFlags::Floating))
            {
                const float increment = std::cos(DegreesToRadians(
                    RequireReference(_scene).GlobalElapsedTime() / 675.0F)) * 0.4F;
                const float y = _detachedPosition.Y + increment;
                const Vector3 position = WithY(Position, y);
                const Vector3 target = WithY(
                    Position,
                    y + _shieldOffset * (increment >= 0.0F ? 1.0F : -1.0F));
                Formats::CollisionResult discard{};
                if (!Formats::CollisionDetection::CheckBetweenPoints(
                    position,
                    target,
                    Formats::TestFlags::None,
                    _scene,
                    discard))
                {
                    Position = position;
                }
            }
            else
            {
                DropToFloor(-0.1F);
            }
        }

        ProcessRecoil();

        if (IsStaticState() && AreAllSynapsesDead())
        {
            ChangeState(SlenchState::ShieldLower);
        }

        if (IsStaticState()
            && State() != SlenchState::Initial
            && State() != SlenchState::Intro)
        {
            for (std::int32_t i = 0; i < _synapseCount; ++i)
            {
                Enemy44Entity& synapse = RequireReference(ArrayAt(_synapses, i));
                if (synapse.State() == SynapseState::Initial)
                {
                    synapse.ChangeState(SynapseState::Appear);
                }
                if (synapse.State() == SynapseState::Initial
                    || synapse.State() == SynapseState::Appear)
                {
                    break;
                }
            }
        }

        if (_damageEffect)
        {
            _damageEffect->Transform(Position, Transform);
        }

        if (AreAllSynapsesDead())
        {
            if (_shieldEffect1)
            {
                RequireReference(_scene).DetachEffectEntry(_shieldEffect1, false);
                _shieldEffect1.reset();
                _shieldEffect2 = RequireReference(_scene).SpawnEffectGetEntry(
                    83,
                    Vector3(1.0F, 0.0F, 0.0F),
                    Vector3(0.0F, 1.0F, 0.0F),
                    Position);
            }
        }
        else if (_state1 >= 3 && !_shieldEffect1)
        {
            _shieldEffect1 = RequireReference(_scene).SpawnEffectGetEntry(
                82,
                Vector3(1.0F, 0.0F, 0.0F),
                Vector3(0.0F, 1.0F, 0.0F),
                Position);
            if (_shieldEffect1)
            {
                _shieldEffect1->SetElementExtension(true);
            }
        }

        if (State() == SlenchState::Initial)
        {
            if (LengthSquared(playerTarget - static_cast<Vector3>(Position))
                <= 16.0F * 16.0F)
            {
                ChangeState(SlenchState::Intro);
            }
        }
        else if (State() == SlenchState::Intro)
        {
            if (AnimationEnded(RequireReference(_model)))
            {
                _slenchFlags &= ~Enemies::SlenchFlags::EyeClosed;
                _slenchFlags &= ~Enemies::SlenchFlags::Detached;
                ChangeState(SlenchState::ShieldRaise);
            }
        }
        else if (State() == SlenchState::ShieldRaise)
        {
            if (_shieldEffect2 && _shieldEffect2->IsFinished())
            {
                RequireReference(_scene).DetachEffectEntry(_shieldEffect2, false);
                _shieldEffect2.reset();
                ChangeState(SlenchState::Idle);
            }
        }
        else if (State() == SlenchState::Idle)
        {
            if (_staticShotTimer > 0
                && (_staticShotTimer = SubInt32Unchecked(_staticShotTimer, 1)) > 0)
            {
                (void)RotateToTarget(
                    playerTarget,
                    Fixed::ToFloat(phaseValues.AngleIncrement2) / 2.0F);
            }
            else
            {
                ChangeState(SlenchState::ShootTear);
            }
        }
        else if (State() == SlenchState::ShootTear)
        {
            if (_staticShotTimer < 36 * 2)
            {
                _staticShotTimer = AddInt32Unchecked(_staticShotTimer, 1);
                if (_staticShotTimer < 16 * 2)
                {
                    (void)RotateToTarget(
                        playerTarget,
                        Fixed::ToFloat(phaseValues.AngleIncrement3) / 2.0F);
                }
                else
                {
                    if (_staticShotTimer == 16 * 2)
                    {
                        (void)Rng::GetRandomInt2(360);
                        const float angle = static_cast<float>(
                            Rng::GetRandomInt2(360));
                        const Matrix4 rotMtx = CreateFromAxisAngle(
                            facing, DegreesToRadians(angle));
                        const Vector3 vecA = Vec3MultMtx3(up, rotMtx);
                        const Vector3 vecB
                            = ScaleVector(facing, 4.0F)
                            + static_cast<Vector3>(Position);
                        const float randf = static_cast<float>(
                            Rng::GetRandomInt2(0x2000)) / 4096.0F + 2.0F;
                        _destVec2 = ScaleVector(vecA, randf) + vecB;
                    }
                    (void)RotateToTarget(
                        _destVec2,
                        Fixed::ToFloat(phaseValues.AngleIncrement1) / 2.0F);
                }
            }
            else if (_staticShotCooldown != 0)
            {
                _staticShotCooldown = SubInt32Unchecked(
                    _staticShotCooldown, 1);
                (void)RotateToTarget(
                    playerTarget,
                    Fixed::ToFloat(phaseValues.AngleIncrement3) / 2.0F);
            }
            else if (_staticShotCounter
                < static_cast<std::int32_t>(phaseValues.StaticShotCount))
            {
                _staticShotCounter = AddInt32Unchecked(
                    _staticShotCounter, 1);
                _staticShotCooldown
                    = static_cast<std::int32_t>(phaseValues.StaticShotCooldown) * 2;
                RequireReference(_equipInfo).SetWeapon(
                    VectorAt(RequireReference(Weapons::BossWeapons), 3));
                if (_staticShotCounter > 1)
                {
                    _soundSource.PlaySfx(SfxId::MISSILE);
                }
                const Vector3 spawnPos
                    = ScaleVector(facing, _shieldOffset)
                    + static_cast<Vector3>(Position);
                const BeamResultFlags result = BeamProjectileEntity::Spawn(
                    SharedEntity(_scene, this),
                    _equipInfo,
                    spawnPos,
                    facing,
                    BeamSpawnFlags::None,
                    Formats::Culling::NodeRef::None,
                    _scene);
                if (result != BeamResultFlags::NoSpawn)
                {
                    SetRecoilTargetVecs();
                }
            }

            if (_shotEffect)
            {
                _shotEffect->Transform(up, facing, Position);
                if (_shotEffect->IsFinished())
                {
                    RequireReference(_scene).DetachEffectEntry(
                        _shotEffect, false);
                    _shotEffect.reset();
                }
            }

            if (!_shotEffect
                && _staticShotCounter
                    == static_cast<std::int32_t>(phaseValues.StaticShotCount))
            {
                ChangeState(SlenchState::Idle);
            }
        }
        else if (State() == SlenchState::ShieldLower)
        {
            if (_shieldEffect2)
            {
                _shieldEffect2->Transform(Position, Transform);
                if (_shieldEffect2->IsFinished())
                {
                    RequireReference(_scene).DetachEffectEntry(
                        _shieldEffect2, false);
                    _shieldEffect2.reset();
                    _soundSource.PlaySfx(SfxId::BIGEYE_DETACH);
                }
            }
            else if (_subtype == 3
                || (RotateToTarget(
                        _detachedFacing,
                        Fixed::ToFloat(phaseValues.AngleIncrement1) / 2.0F)
                    && MoveToPosition(
                        _detachedPosition,
                        Fixed::ToFloat(phaseValues.MoveIncrement1) / 2.0F)))
            {
                _dropSpeed = 0.0F;
                _slenchFlags &= ~Enemies::SlenchFlags::Rolling;
                _slenchFlags &= ~Enemies::SlenchFlags::Bouncy;
                _slenchFlags |= Enemies::SlenchFlags::Floating;
                ChangeState(SlenchState::Detach);
            }
        }
        else if (State() == SlenchState::Return)
        {
            if (_damageEffect && AnimationEnded(RequireReference(_model)))
            {
                RequireReference(_model).SetAnimation(0);
            }
            if (CheckPosAgainstCurrent(_detachedPosition))
            {
                ChangeState(SlenchState::Attach);
            }
            else if (RotateToTarget(
                _detachedPosition,
                Fixed::ToFloat(phaseValues.AngleIncrement1) / 2.0F))
            {
                (void)MoveToPosition(
                    _detachedPosition,
                    Fixed::ToFloat(phaseValues.MoveIncrement2) / 2.0F);
            }
        }
        else if (State() == SlenchState::Attach)
        {
            if (RotateToTarget(
                    _detachedFacing,
                    Fixed::ToFloat(phaseValues.AngleIncrement1) / 2.0F)
                && MoveToPosition(
                    _startPos,
                    Fixed::ToFloat(phaseValues.MoveIncrement2) / 2.0F))
            {
                for (std::int32_t i = 0; i < _synapseCount; ++i)
                {
                    RequireReference(ArrayAt(_synapses, i))
                        .ChangeState(SynapseState::Initial);
                }
                ChangeState(SlenchState::ShieldRaise);
            }
        }
        else if (State() == SlenchState::Detach)
        {
            ChangeState(SlenchState::Roam);
        }
        else if (State() == SlenchState::RollingDone)
        {
            const Vector3 pos = WithY(_detachedPosition, _floatBaseY);
            if (CheckPosAgainstCurrent(pos))
            {
                ChangeState(SlenchState::Roam);
            }
            else if (RotateToTarget(
                pos, Fixed::ToFloat(phaseValues.AngleIncrement4) / 2.0F))
            {
                (void)MoveToPosition(
                    pos, Fixed::ToFloat(phaseValues.MoveIncrement3) / 2.0F);
            }
        }

        else if (State() == SlenchState::Roam)
        {
            const std::int32_t phaseHealth = MulInt32Unchecked(
                static_cast<std::int32_t>(_healthMax) / 3,
                SubInt32Unchecked(2, _phase));
            if (_health > phaseHealth)
            {
                if (_roamTimer == 0
                    || (_roamTimer = SubInt32Unchecked(_roamTimer, 1)) != 0)
                {
                    if (_health
                        <= AddInt32Unchecked(
                            phaseHealth,
                            static_cast<std::int32_t>(_healthMax) / 12))
                    {
                        _slenchFlags |= Enemies::SlenchFlags::Wobbling;
                    }

                    if (_subtype == 3)
                    {
                        if (TestFlag(
                            _slenchFlags, Enemies::SlenchFlags::Rolling))
                        {
                            if (_rollTimer == 0
                                || (_rollTimer
                                    = SubInt32Unchecked(_rollTimer, 1)) != 0)
                            {
                                if (_hitFloor)
                                {
                                    _soundSource.PlaySfx(
                                        SfxId::SPIRE_ROLL, true);
                                    if (_floatBaseY == 0.0F)
                                    {
                                        _floatBaseY = (
                                            _detachedPosition.Y + Position.Y)
                                            / 2.0F;
                                    }
                                    _patternAngle += Fixed::ToFloat(
                                        phaseValues.RollingAngleInc) / 2.0F;
                                    if (_patternAngle >= 360.0F)
                                    {
                                        _slenchFlags
                                            ^= Enemies::SlenchFlags::PatternFlip1;
                                        _patternAngle -= 360.0F;
                                    }

                                    Vector3 vecA;
                                    float angle;
                                    if (TestFlag(
                                        _slenchFlags,
                                        Enemies::SlenchFlags::PatternFlip1))
                                    {
                                        vecA = ScaleVector(_targetHorizontal, -1.0F);
                                        angle = 360.0F - _patternAngle;
                                    }
                                    else
                                    {
                                        vecA = _targetHorizontal;
                                        angle = _patternAngle + 180.0F;
                                    }

                                    const float factor = Fixed::ToFloat(
                                        phaseValues.RollingSpeed);
                                    const Vector3 vecB
                                        = ScaleVector(vecA, factor)
                                        + _detachedPosition;
                                    const float radians = DegreesToRadians(angle);
                                    const float sine = std::sin(radians);
                                    const float cosine = std::cos(radians);
                                    const Matrix3 mtx(
                                        cosine, 0.0F, -sine,
                                        0.0F, 1.0F, 0.0F,
                                        sine, 0.0F, cosine);
                                    const Vector3 newPos
                                        = ScaleVector(
                                            Vec3MultMtx3(
                                                _targetHorizontal, mtx),
                                            factor)
                                        + vecB;
                                    Position = WithY(newPos, Position.Y);

                                    _targetAngle += Fixed::ToFloat(
                                        phaseValues.RollingAngleInc) / 2.0F;
                                    if (_targetAngle < 0.0F)
                                    {
                                        _targetAngle += 360.0F;
                                    }

                                    const Vector3 vecC = WithY(
                                        static_cast<Vector3>(Position) - vecB,
                                        0.0F).Normalized();
                                    if (TestFlag(
                                        _slenchFlags,
                                        Enemies::SlenchFlags::PatternFlip1))
                                    {
                                        _targetZ = vecC.X;
                                        _targetX = -vecC.Z;
                                    }
                                    else
                                    {
                                        _targetZ = -vecC.X;
                                        _targetX = vecC.Z;
                                    }
                                }
                                else if (_dropSpeed == 0.0F)
                                {
                                    _hitFloor = true;
                                }
                            }
                            else
                            {
                                _soundSource.StopSfx(SfxId::SPIRE_ROLL);
                                ChangeState(SlenchState::RollingDone);
                            }
                        }
                        else
                        {
                            _patternAngle += Fixed::ToFloat(
                                phaseValues.FloatingAngleInc) / 2.0F;
                            if (_patternAngle >= 360.0F)
                            {
                                _slenchFlags
                                    ^= Enemies::SlenchFlags::PatternFlip1;
                                _patternAngle -= 360.0F;
                            }

                            const float factor
                                = Fixed::ToFloat(phaseValues.FloatingSpeed);
                            float angle;
                            Vector3 vecA;
                            if (TestFlag(
                                _slenchFlags,
                                Enemies::SlenchFlags::PatternFlip1))
                            {
                                angle = 540.0F - _patternAngle;
                                vecA = Vector3(
                                    _detachedPosition.X
                                        + _targetHorizontal.X * factor,
                                    _floatBaseY,
                                    _detachedPosition.Z
                                        + _targetHorizontal.Z * factor);
                            }
                            else
                            {
                                angle = _patternAngle;
                                vecA = Vector3(
                                    _detachedPosition.X
                                        - _targetHorizontal.X * factor,
                                    _floatBaseY,
                                    _detachedPosition.Z
                                        - _targetHorizontal.Z * factor);
                            }

                            const Vector3 vecB = Vector3::Cross(
                                _targetHorizontal,
                                Vector3(0.0F, 1.0F, 0.0F)).Normalized();
                            const Matrix4 rotMtx = CreateFromAxisAngle(
                                vecB, DegreesToRadians(angle));
                            Position = ScaleVector(
                                Vec3MultMtx3(_targetHorizontal, rotMtx),
                                factor) + vecA;

                            if (RotateToTarget(
                                playerTarget,
                                Fixed::ToFloat(
                                    phaseValues.AngleIncrement4) / 2.0F))
                            {
                                _slenchFlags
                                    |= Enemies::SlenchFlags::TargetingPlayer;
                            }
                            else
                            {
                                _slenchFlags
                                    &= ~Enemies::SlenchFlags::TargetingPlayer;
                            }
                            (void)SetUpSlam(playerTarget);
                        }
                    }
                    else
                    {
                        _patternAngle += Fixed::ToFloat(
                            phaseValues.FloatingAngleInc) / 2.0F;
                        if (_patternAngle >= 360.0F)
                        {
                            _slenchFlags
                                ^= Enemies::SlenchFlags::PatternFlip1;
                            _patternAngle -= 360.0F;
                        }

                        Vector3 vecA;
                        float angle = _patternAngle + 180.0F;
                        if (TestFlag(
                            _slenchFlags,
                            Enemies::SlenchFlags::PatternFlip1))
                        {
                            vecA = Vector3::Cross(
                                _targetHorizontal,
                                Vector3(0.0F, 1.0F, 0.0F)).Normalized();
                            angle = 180.0F - _patternAngle;
                        }
                        else
                        {
                            vecA = Vector3::Cross(
                                Vector3(0.0F, 1.0F, 0.0F),
                                _targetHorizontal).Normalized();
                        }

                        vecA = ScaleVector(
                            vecA, Fixed::ToFloat(phaseValues.FloatingSpeed));
                        const Vector3 vecB = vecA + _detachedPosition;
                        const Matrix4 rotMtx = CreateFromAxisAngle(
                            _targetHorizontal, DegreesToRadians(angle));
                        vecA = Vec3MultMtx3(vecA, rotMtx);
                        Position = vecA + vecB;

                        if (_subtype != 0)
                        {
                            const float increment
                                = Fixed::ToFloat(
                                    phaseValues.MoveIncrement3) / 2.0F;
                            if (TestFlag(
                                _slenchFlags,
                                Enemies::SlenchFlags::PatternFlip2))
                            {
                                _floatBaseY -= increment;
                            }
                            else
                            {
                                _floatBaseY += increment;
                            }
                            if (_floatBaseY < 0.0F
                                || _floatBaseY
                                    >= Fixed::ToFloat(phaseValues.RollTime))
                            {
                                _slenchFlags
                                    ^= Enemies::SlenchFlags::PatternFlip2;
                            }
                            Position = static_cast<Vector3>(Position)
                                + ScaleVector(_targetHorizontal, _floatBaseY);
                            if (_subtype == 2)
                            {
                                (void)SetUpSlam(playerTarget);
                            }
                        }

                        if (!RotateToTarget(
                            playerTarget,
                            Fixed::ToFloat(
                                phaseValues.AngleIncrement4) / 2.0F))
                        {
                            _slenchFlags
                                &= ~Enemies::SlenchFlags::TargetingPlayer;
                        }
                        else
                        {
                            const std::uint32_t random
                                = Rng::GetRandomInt2(100);
                            if (random >= 50)
                            {
                                _slenchFlags
                                    &= ~Enemies::SlenchFlags::TargetingPlayer;
                            }
                            else
                            {
                                _slenchFlags
                                    |= Enemies::SlenchFlags::TargetingPlayer;
                            }
                        }
                    }
                }
                else
                {
                    ChangeState(SlenchState::Return);
                }
            }
            else
            {
                std::int32_t effectId = 205;
                if (_phase == 0)
                {
                    effectId = 203;
                }
                else if (_phase == 1)
                {
                    effectId = 204;
                }
                RequireReference(_scene).SpawnEffect(effectId, Transform);
                _damageEffect
                    = RequireReference(_scene).SpawnEffectGetEntry(201, Transform);
                if (_damageEffect)
                {
                    _damageEffect->SetElementExtension(true);
                }
                _phase = AddInt32Unchecked(_phase, 1);
                _soundSource.PlaySfx(SfxId::BIGEYE_DIE_SCR);
                ChangeState(SlenchState::Return);
            }
        }
        else if (State() == SlenchState::SlamReady)
        {
            _destVec1 = playerTarget;
            Position = _destVec2;
            (void)RotateToTarget(
                _destVec1,
                Fixed::ToFloat(phaseValues.AngleIncrement5) / 2.0F);
            const std::int32_t time = MulInt32Unchecked(
                DivideInt32(
                    360,
                    static_cast<std::int32_t>(phaseValues.WobbleRotInc)),
                static_cast<std::int32_t>(phaseValues.WobbleCycles));
            _wobbleTimer = AddInt32Unchecked(_wobbleTimer, 1);
            if (_wobbleTimer >= MulInt32Unchecked(time, 2))
            {
                ChangeState(SlenchState::Slam);
            }
            else
            {
                _wobbleAngle
                    += static_cast<float>(phaseValues.WobbleRotInc) / 2.0F;
                if (_wobbleAngle >= 360.0F)
                {
                    _wobbleAngle -= 360.0F;
                }
                const float maxDist
                    = Fixed::ToFloat(phaseValues.MaxWobbleDist);
                float factor = maxDist
                    - static_cast<float>(_wobbleTimer) * maxDist
                    / static_cast<float>(MulInt32Unchecked(
                        MulInt32Unchecked(time, 2), 2));
                if (factor < 0.01F)
                {
                    factor = 0.01F;
                }
                const Matrix4 rotMtx = CreateFromAxisAngle(
                    facing, DegreesToRadians(_wobbleAngle));
                Position = ScaleVector(Vec3MultMtx3(up, rotMtx), factor)
                    + _destVec2;
            }
        }
        else if (State() == SlenchState::Slam)
        {
            if (MoveToPosition(
                _destVec1,
                Fixed::ToFloat(phaseValues.MoveIncrement4) / 2.0F))
            {
                ChangeState(SlenchState::SlamReturn);
            }
        }
        else if (State() == SlenchState::SlamReturn)
        {
            if (MoveToPosition(
                _destVec2,
                Fixed::ToFloat(phaseValues.MoveIncrement5) / 2.0F))
            {
                ChangeState(static_cast<SlenchState>(_stateAfterSlam));
            }
        }
        else if (State() == SlenchState::Dead)
        {
            if (_deathTimer != 0)
            {
                _deathTimer = SubInt32Unchecked(_deathTimer, 1);
            }
            if (_deathTimer == 0)
            {
                _health = 0;
                Flags &= ~EnemyFlags::Invincible;
            }
        }

        UpdateFacing();
    }

    void Enemy41Entity::DropToFloor(float step)
    {
        const float max = 1.1F * 30.0F;
        const auto acceleration
            = ConstantAcceleration(step, _dropSpeed, -max, max);
        _dropSpeed = std::get<0>(acceleration);
        const float displacement = std::get<1>(acceleration);
        if (!CheckCollision(Vector3(0.0F, 1.0F, 0.0F), displacement))
        {
            Position = AddY(Position, displacement);
        }
        else
        {
            _dropSpeed = -_dropSpeed
                / (TestFlag(_slenchFlags, Enemies::SlenchFlags::Bouncy)
                    ? 1.0F
                    : 2.0F);
            if (_dropSpeed > -0.05F * 30.0F
                && _dropSpeed < 0.05F * 30.0F)
            {
                _dropSpeed = 0.0F;
            }
        }
    }

    bool Enemy41Entity::CheckCollision(Vector3 vec, float dist)
    {
        Formats::CollisionResult res{};
        const Vector3 position = Position;
        const float factor = dist
            + _shieldOffset * (dist >= 0.0F ? 1.0F : -1.0F);
        if (!Equal(vec, Vector3::Zero))
        {
            const Vector3 dest = ScaleVector(vec, factor) + position;
            if (Formats::CollisionDetection::CheckBetweenPoints(
                position,
                dest,
                Formats::TestFlags::None,
                _scene,
                res))
            {
                return true;
            }
        }

        if (vec.X != 0.0F || vec.Z != 0.0F)
        {
            Vector3 dest = ScaleVector(vec, factor)
                + AddY(position, _shieldOffset / -2.0F);
            if (Formats::CollisionDetection::CheckBetweenPoints(
                position,
                dest,
                Formats::TestFlags::None,
                _scene,
                res))
            {
                return true;
            }

            dest = ScaleVector(vec, factor)
                + AddY(position, _shieldOffset * -1.0F);
            if (Formats::CollisionDetection::CheckBetweenPoints(
                position,
                dest,
                Formats::TestFlags::None,
                _scene,
                res))
            {
                return true;
            }

            auto enumerator
                = RequireReference(_scene).GetDoorEntities().GetEnumerator();
            while (enumerator.MoveNext())
            {
                DoorEntity& door = RequireReference(enumerator.Current());
                if (TestFlag(door.Flags(), DoorFlags::Open))
                {
                    continue;
                }

                const Vector3 doorFacing = door.FacingVector();
                Vector4 plane(doorFacing);
                Vector3 between = position - door.LockPosition();
                if (Vector3::Dot(between, doorFacing) < 0.0F)
                {
                    plane.X *= -1.0F;
                    plane.Y *= -1.0F;
                    plane.Z *= -1.0F;
                    plane.W *= -1.0F;
                }

                const Vector3 lockPosition = door.LockPosition();
                plane.W = (plane.X * 0.4F + lockPosition.X) * plane.X
                    + (plane.Y * 0.4F + lockPosition.Y) * plane.Y
                    + (plane.Z * 0.4F + lockPosition.Z) * plane.Z;
                const Vector3 cylTop = position + ScaleVector(vec, dist);
                if (Formats::CollisionDetection::CheckCylinderIntersectPlane(
                        position, cylTop, plane, res)
                    && res.Distance < 2.0F)
                {
                    between = res.Position - door.LockPosition();
                    if (LengthSquared(between) < door.RadiusSquared())
                    {
                        return true;
                    }
                }
            }
        }
        return false;
    }

    void Enemy41Entity::SetRecoilTargetVecs()
    {
        _recoilTimer = 0;
        _destVec2 = Position;
        _destVec1 = ScaleVector(FacingVector(), -1.0F).Normalized();
    }

    void Enemy41Entity::ProcessRecoil()
    {
        if (_recoilTimer >= 10 * 2)
        {
            return;
        }

        float factor = 1.0F;
        if (_recoilTimer <= 4 * 2 + 1)
        {
            factor = ArrayAt(
                _recoilLut, static_cast<std::int32_t>(_recoilTimer));
        }
        else
        {
            const float diff = static_cast<float>(
                9 * 2 + 1 - static_cast<std::int32_t>(_recoilTimer));
            factor = diff / static_cast<float>(5 * 2);
        }
        Position = ScaleVector(_destVec1, factor) + _destVec2;
        _recoilTimer = static_cast<std::uint16_t>(
            static_cast<std::uint32_t>(_recoilTimer) + 1U);
    }

    bool Enemy41Entity::IsStaticState() noexcept
    {
        return _state1 < 5;
    }

    bool Enemy41Entity::AreAllSynapsesDead()
    {
        for (std::int32_t i = 0; i < _synapseCount; ++i)
        {
            if (RequireReference(ArrayAt(_synapses, i)).State()
                != SynapseState::Dead)
            {
                return false;
            }
        }
        return true;
    }

    bool Enemy41Entity::CanSynapsesRespawn()
    {
        if (!IsStaticState())
        {
            return false;
        }
        for (std::int32_t i = 0; i < _synapseCount; ++i)
        {
            Enemy44Entity& synapse = RequireReference(ArrayAt(_synapses, i));
            if (synapse.State() != SynapseState::Dying
                && synapse.State() != SynapseState::Dead)
            {
                return true;
            }
        }
        return false;
    }

    bool Enemy41Entity::SetUpSlam(Vector3 target)
    {
        const Enemy41Values phaseValues = GetPhaseValues();
        const std::int32_t slamDelay = static_cast<std::int32_t>(
            static_cast<std::uint32_t>(phaseValues.SlamDelay) * 2U);
        if (static_cast<std::int32_t>(_slamTimer) < slamDelay)
        {
            _slamTimer = static_cast<std::uint16_t>(
                static_cast<std::uint32_t>(_slamTimer) + 1U);
        }
        else
        {
            const Vector3 between
                = target - static_cast<Vector3>(Position);
            if (Length(between)
                <= Fixed::ToFloat(phaseValues.SlamRange))
            {
                const float radius = _shieldOffset * 0.75F;
                target = target - static_cast<Vector3>(Position);
                const float length = Length(target);
                if (length > radius)
                {
                    _destVec2 = Position;
                    target = target.Normalized();
                    _destVec1 = ScaleVector(target, length - radius)
                        + static_cast<Vector3>(Position);
                    _slamTimer = 0;
                    ChangeState(SlenchState::SlamReady);
                    return true;
                }
            }
        }
        return false;
    }

    bool Enemy41Entity::CheckPosAgainstCurrent(Vector3 pos) noexcept
    {
        return Equal(pos, Position);
    }

    bool Enemy41Entity::MoveToPosition(Vector3 position, float increment)
    {
        Vector3 between = position - static_cast<Vector3>(Position);
        if (LengthSquared(between) > increment * increment)
        {
            between = between.Normalized();
            Position = static_cast<Vector3>(Position)
                + ScaleVector(between, increment);
            return false;
        }
        Position = position;
        return true;
    }

    bool Enemy41Entity::RotateToTarget(Vector3 target, float increment)
    {
        float angle = 0.0F;
        if (Position.Y != target.Y)
        {
            const float x = target.X - Position.X;
            const float z = target.Z - Position.Z;
            const float squareRoot = std::sqrt(x * x + z * z);
            const float y = Position.Y - target.Y;
            const float atan = RadiansToDegrees(std::atan2(y, squareRoot));
            angle = atan + (atan < 0.0F ? 360.0F : 0.0F);
        }

        float targetAngle = _targetAngle;
        if (targetAngle < angle)
        {
            const float diff = angle - targetAngle;
            if (angle - targetAngle >= 180.0F)
            {
                if (360.0F - diff <= increment)
                {
                    targetAngle = angle;
                }
                else
                {
                    targetAngle -= increment;
                }
            }
            else if (diff <= increment)
            {
                targetAngle = angle;
            }
            else
            {
                targetAngle += increment;
            }
        }
        else if (targetAngle > angle)
        {
            const float diff = targetAngle - angle;
            if (targetAngle - angle >= 180.0F)
            {
                if (360.0F - diff <= increment)
                {
                    targetAngle = angle;
                }
                else
                {
                    targetAngle += increment;
                }
            }
            else if (diff <= increment)
            {
                targetAngle = angle;
            }
            else
            {
                targetAngle -= increment;
            }
        }

        if (targetAngle >= 360.0F)
        {
            targetAngle -= 360.0F;
        }
        _targetAngle = targetAngle;
        return RotateToTargetHorizontal(target, increment)
            && targetAngle == angle;
    }

    bool Enemy41Entity::RotateToTargetHorizontal(
        Vector3 target, float increment)
    {
        Vector3 between = target - static_cast<Vector3>(Position);
        if (between.X == 0.0F && between.Z == 0.0F)
        {
            return true;
        }
        between = WithY(between, 0.0F).Normalized();
        Vector3 fields
            = Vector3(_targetX, 0.0F, _targetZ).Normalized();
        const Vector3 between2 = fields - between;
        const float radians = DegreesToRadians(increment);
        float sine = std::sin(radians);
        const float cosine = std::cos(radians);
        if (between2.X * between2.X + between2.Z * between2.Z
            <= (1.0F - cosine) * (1.0F - cosine) + sine * sine)
        {
            _targetX = between.X;
            _targetZ = between.Z;
            return true;
        }

        const Vector3 cross = Vector3::Cross(between, fields);
        if (cross.Y > 0.0F)
        {
            sine *= -1.0F;
        }
        const Matrix3 mtx(
            cosine, 0.0F, -sine,
            0.0F, 1.0F, 0.0F,
            sine, 0.0F, cosine);
        fields = Vec3MultMtx3(fields, mtx);
        fields = fields.Normalized();
        _targetX = fields.X;
        _targetZ = fields.Z;
        return false;
    }

    bool Enemy41Entity::ShieldTakeDamage(EntityBase* source)
    {
        return EnemyTakeDamage(source);
    }

    bool Enemy41Entity::EnemyTakeDamage(EntityBase* source)
    {
        if (_subtype == 3
            && TestFlag(_slenchFlags, Enemies::SlenchFlags::Rolling)
            && source != nullptr
            && source->Type == EntityType::Bomb)
        {
            _rollTimer = SubInt32Unchecked(_rollTimer, 30 * 2);
            if (_rollTimer == 0)
            {
                _rollTimer = 1;
            }
            else if (_rollTimer < 0)
            {
                if (Bugfixes::NoSlenchRollTimerUnderflow())
                {
                    _rollTimer = 1;
                }
                else
                {
                    _rollTimer = AddInt32Unchecked(
                        _rollTimer,
                        static_cast<std::int32_t>(
                            (std::numeric_limits<std::uint16_t>::max() + 1U)
                            * 2U));
                }
            }
        }

        if (TestFlag(
            static_cast<EnemyFlags>(Flags), EnemyFlags::Invincible))
        {
            if (!AreAllSynapsesDead()
                && State() != SlenchState::Initial
                && State() != SlenchState::Intro
                && State() != SlenchState::ShieldRaise
                && source != nullptr
                && source->Type == EntityType::BeamProjectile)
            {
                BeamProjectileEntity* beamPtr
                    = dynamic_cast<BeamProjectileEntity*>(source);
                if (beamPtr == nullptr)
                {
                    throw SceneDetail::InvalidCastException();
                }
                BeamProjectileEntity& beam = *beamPtr;
                const Vector3 up
                    = (static_cast<Vector3>(beam.Position)
                        - static_cast<Vector3>(Position)).Normalized();
                Vector3 facing;
                if (up.Z <= -0.9F || up.Z >= 0.9F)
                {
                    facing = Vector3::Cross(
                        Vector3(1.0F, 0.0F, 0.0F), up).Normalized();
                }
                else
                {
                    facing = Vector3::Cross(
                        Vector3(0.0F, 0.0F, 1.0F), up).Normalized();
                }
                RequireReference(_scene).SpawnEffect(
                    70, facing, up, Position);
                _soundSource.PlaySfx(SfxId::BIGEYE_DEFLECT);
            }
            return true;
        }

        _timeSinceDamage = 0;
        _soundSource.PlaySfx(SfxId::BIGEYE_DAMAGE);
        std::int32_t animIndex = 11;
        if (TestFlag(_slenchFlags, Enemies::SlenchFlags::EyeClosed))
        {
            animIndex = 12;
        }
        else if (TestFlag(
            _slenchFlags, Enemies::SlenchFlags::Detached))
        {
            animIndex = 6;
        }
        RequireReference(_model).SetAnimation(
            animIndex, AnimFlags::NoLoop);

        if (_health == 0)
        {
            _health = 1;
            if (State() != SlenchState::Dead)
            {
                ChangeState(SlenchState::Dead);
                _soundSource.PlaySfx(
                    SfxId::BIGEYE_DIE_SCR,
                    false,
                    true,
                    std::numeric_limits<float>::max(),
                    true);
            }
            if (MainPlayer().Health() > 0
                && GameState::SinglePlayer())
            {
                RequireReference(_scene).StartMovie(
                    ArrayAt(_deathMovieIds, _subtype),
                    FadeType::FadeOutInWhite,
                    40.0F / 30.0F,
                    FadeType::FadeOutInWhite,
                    5.0F / 30.0F);
            }
        }
        return false;
    }
}
