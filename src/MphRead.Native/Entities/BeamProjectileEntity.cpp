#include "BeamProjectileEntity.hpp"

#include "../Features.hpp"
#include "../GameState.hpp"
#include "../MemoryArrays.hpp"
#include "../Metadata/Metadata.hpp"
#include "../Metadata/SoundMeta.hpp"
#include "../Metadata/Weapons.hpp"
#include "../Messaging.hpp"
#include "../Mods/Network/NetDamage.hpp"
#include "../Mods/Network/NetHitPrediction.hpp"
#include "../Mods/Network/NetLog.hpp"
#include "../Read.hpp"
#include "../Scene.hpp"
#include "../Utility/Rng.hpp"
#include "BeamEffectEntity.hpp"
#include "DoorEntity.hpp"
#include "EnemyInstanceEntity.hpp"
#include "ForceFieldEntity.hpp"
#include "ItemSpawnEntity.hpp"
#include "PlatformEntity.hpp"
#include "Players/HalfturretEntity.hpp"
#include "Players/PlayerEntity.hpp"

#include <algorithm>
#include <any>
#include <array>
#include <bit>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <limits>
#include <memory>
#include <type_traits>
#include <utility>

namespace
{
    using MphRead::Entities::EntityBase;
    using OpenTK::Mathematics::Matrix4;
    using OpenTK::Mathematics::Vector3;
    using OpenTK::Mathematics::Vector4;

    template <typename TEnum>
    [[nodiscard]] constexpr bool TestFlag(TEnum value, TEnum flag) noexcept
    {
        using U = std::underlying_type_t<TEnum>;
        return (static_cast<U>(value) & static_cast<U>(flag)) != 0;
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

    [[nodiscard]] float LengthSquared(Vector3 v) noexcept
    {
        return v.X * v.X + v.Y * v.Y + v.Z * v.Z;
    }

    [[nodiscard]] float Length(Vector3 v) noexcept
    {
        return std::sqrt(LengthSquared(v));
    }

    [[nodiscard]] bool IsZero(Vector3 v) noexcept
    {
        return v.X == 0.0F && v.Y == 0.0F && v.Z == 0.0F;
    }

    [[nodiscard]] Vector3 Negate(Vector3 v) noexcept
    {
        return Vector3(-v.X, -v.Y, -v.Z);
    }

    [[nodiscard]] Vector3 Scale(Vector3 v, float s) noexcept
    {
        return Vector3(v.X * s, v.Y * s, v.Z * s);
    }

    [[nodiscard]] Vector3 Divide(Vector3 v, float s) noexcept
    {
        return Vector3(v.X / s, v.Y / s, v.Z / s);
    }

    [[nodiscard]] Vector3 ComponentMultiply(Vector3 a, Vector3 b) noexcept
    {
        return Vector3(a.X * b.X, a.Y * b.Y, a.Z * b.Z);
    }

    [[nodiscard]] Vector3 Normalize(Vector3 v)
    {
        const float length = Length(v);
        return Vector3(v.X / length, v.Y / length, v.Z / length);
    }

    [[nodiscard]] Vector3 AddY(Vector3 v, float y) noexcept
    {
        v.Y += y;
        return v;
    }

    [[nodiscard]] Vector3 WithY(Vector3 v, float y) noexcept
    {
        v.Y = y;
        return v;
    }

    [[nodiscard]] Vector4 NegatePlane(Vector3 direction) noexcept
    {
        return Vector4(-direction.X, -direction.Y, -direction.Z, 0.0F);
    }

    [[nodiscard]] Matrix4 Translation(Vector3 p) noexcept
    {
        return Matrix4(
            Vector4(1, 0, 0, 0), Vector4(0, 1, 0, 0),
            Vector4(0, 0, 1, 0), Vector4(p, 1));
    }

    [[nodiscard]] Matrix4 ScaleMatrix(float s) noexcept
    {
        return Matrix4(
            Vector4(s, 0, 0, 0), Vector4(0, s, 0, 0),
            Vector4(0, 0, s, 0), Vector4(0, 0, 0, 1));
    }

    void SetRow3(Matrix4& m, Vector3 v) noexcept
    {
        m.M41 = v.X; m.M42 = v.Y; m.M43 = v.Z;
    }

    void ScaleRow2(Matrix4& m, float s) noexcept
    {
        m.M31 *= s; m.M32 *= s; m.M33 *= s;
    }

    [[nodiscard]] Matrix4 ClearScale(Matrix4 value) noexcept
    {
        const float x = Length(Vector3(value.M11, value.M12, value.M13));
        const float y = Length(Vector3(value.M21, value.M22, value.M23));
        const float z = Length(Vector3(value.M31, value.M32, value.M33));
        value.M11 /= x; value.M12 /= x; value.M13 /= x;
        value.M21 /= y; value.M22 /= y; value.M23 /= y;
        value.M31 /= z; value.M32 /= z; value.M33 /= z;
        return value;
    }

    [[nodiscard]] MphRead::MessageObject BoxInt32(std::int32_t value)
    {
        return std::make_shared<const std::any>(value);
    }

    template <typename T>
    [[nodiscard]] MphRead::MessageObject BoxValue(T value)
    {
        return std::make_shared<const std::any>(std::move(value));
    }

    [[nodiscard]] MphRead::MessageObject BoxEntityOrZero(EntityBase* entity)
    {
        if (entity != nullptr)
        {
            return std::make_shared<const std::any>(entity);
        }
        return BoxInt32(0);
    }

    [[nodiscard]] std::size_t Index(std::int32_t value)
    {
        if (value < 0)
        {
            throw MphRead::Memory::Detail::IndexOutOfRangeException();
        }
        return static_cast<std::size_t>(value);
    }

    template <typename TContainer>
    [[nodiscard]] decltype(auto) ManagedAt(TContainer& container, std::size_t index)
    {
        if (index >= container.size())
        {
            throw MphRead::Memory::Detail::IndexOutOfRangeException();
        }
        return container[index];
    }

    template <typename TContainer>
    [[nodiscard]] decltype(auto) ManagedAt(const TContainer& container, std::size_t index)
    {
        if (index >= container.size())
        {
            throw MphRead::Memory::Detail::IndexOutOfRangeException();
        }
        return container[index];
    }

    [[nodiscard]] std::int32_t ManagedInt32FromUInt64(std::uint64_t value) noexcept
    {
        return std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(value));
    }

    [[nodiscard]] std::int32_t ManagedAdd(std::int32_t left, std::int32_t right) noexcept
    {
        const std::uint32_t value = std::bit_cast<std::uint32_t>(left)
            + std::bit_cast<std::uint32_t>(right);
        return std::bit_cast<std::int32_t>(value);
    }

    [[nodiscard]] std::int32_t ManagedSubtract(std::int32_t left, std::int32_t right) noexcept
    {
        const std::uint32_t value = std::bit_cast<std::uint32_t>(left)
            - std::bit_cast<std::uint32_t>(right);
        return std::bit_cast<std::int32_t>(value);
    }

    template <typename TContainer>
    [[nodiscard]] decltype(auto) ManagedListAt(const TContainer& container, std::int32_t index)
    {
        if (index < 0 || static_cast<std::size_t>(index) >= container.size())
        {
            throw MphRead::Memory::Detail::ArgumentOutOfRangeException();
        }
        return container[static_cast<std::size_t>(index)];
    }

    [[nodiscard]] float DegreesToRadians(float degrees) noexcept
    {
        return degrees * (3.14159265358979323846F / 180.0F);
    }
}

namespace MphRead::Entities
{
    const std::shared_ptr<EquipInfo> BeamProjectileEntity::_ricochetEquip
        = std::make_shared<EquipInfo>();

    const std::array<EntityType, 5> BeamProjectileEntity::_homingTargetTypes{
        EntityType::Player,
        EntityType::Halfturret,
        EntityType::EnemyInstance,
        EntityType::Door,
        EntityType::Platform
    };

    const std::array<std::array<std::uint8_t, 12>, 11> BeamProjectileEntity::_terSplat1P{{
        {{255, 99, 121, 122, 123, 126, 125, 124, 100, 142, 141, 140}},
        {{255, 99, 121, 122, 123, 126, 125, 124, 100, 142, 141, 140}},
        {{255, 255, 255, 255, 255, 255, 255, 255, 255, 142, 141, 140}},
        {{255, 99, 121, 122, 123, 126, 125, 124, 100, 142, 141, 140}},
        {{255, 99, 121, 122, 123, 126, 125, 124, 100, 142, 141, 140}},
        {{255, 99, 121, 122, 123, 126, 125, 124, 100, 142, 141, 140}},
        {{255, 255, 255, 255, 255, 255, 255, 255, 255, 142, 141, 140}},
        {{255, 255, 255, 255, 255, 255, 255, 255, 255, 142, 141, 140}},
        {{255, 255, 255, 255, 255, 255, 255, 255, 255, 142, 141, 140}},
        {{255, 255, 255, 255, 255, 255, 255, 255, 255, 142, 141, 140}},
        {{255, 255, 255, 255, 255, 255, 255, 255, 255, 142, 141, 140}}
    }};

    BeamProjectileEntity::BeamProjectileEntity(Scene* scene)
        : EntityBase(EntityType::BeamProjectile, scene)
    {
    }

#define BEAM_VALUE_PROPERTY(type, name, field) \
    type BeamProjectileEntity::name() const noexcept { return field; } \
    void BeamProjectileEntity::Set##name(type value) noexcept { field = value; }

    BEAM_VALUE_PROPERTY(BeamFlags, Flags, _flags)
    BEAM_VALUE_PROPERTY(BeamType, Beam, _beam)
    BEAM_VALUE_PROPERTY(BeamType, BeamKind, _beamKind)
    BEAM_VALUE_PROPERTY(Vector3, Velocity, _velocity)
    BEAM_VALUE_PROPERTY(Vector3, Acceleration, _acceleration)
    BEAM_VALUE_PROPERTY(Vector3, BackPosition, _backPosition)
    BEAM_VALUE_PROPERTY(Vector3, SpawnPosition, _spawnPosition)
    BEAM_VALUE_PROPERTY(std::int32_t, DrawFuncId, _drawFuncId)
    BEAM_VALUE_PROPERTY(float, Age, _age)
    BEAM_VALUE_PROPERTY(float, Lifespan, _lifespan)
    BEAM_VALUE_PROPERTY(Vector3, Color, _color)
    BEAM_VALUE_PROPERTY(std::uint8_t, CollisionEffect, _collisionEffect)
    BEAM_VALUE_PROPERTY(std::uint8_t, DamageDirType, _damageDirType)
    BEAM_VALUE_PROPERTY(std::uint8_t, SplashDamageType, _splashDamageType)
    BEAM_VALUE_PROPERTY(float, Homing, _homing)
    BEAM_VALUE_PROPERTY(Vector3, Direction, _direction)
    BEAM_VALUE_PROPERTY(Vector3, Right, _right)
    BEAM_VALUE_PROPERTY(Vector3, Up, _up)
    BEAM_VALUE_PROPERTY(float, Damage, _damage)
    BEAM_VALUE_PROPERTY(float, HeadshotDamage, _headshotDamage)
    BEAM_VALUE_PROPERTY(float, SplashDamage, _splashDamage)
    BEAM_VALUE_PROPERTY(float, SplashRadius, _splashRadius)
    BEAM_VALUE_PROPERTY(float, MaxDistance, _maxDistance)
    BEAM_VALUE_PROPERTY(Affliction, Afflictions, _afflictions)
    BEAM_VALUE_PROPERTY(std::int32_t, DamageInterpolation, _damageInterpolation)
    BEAM_VALUE_PROPERTY(std::int32_t, SpeedInterpolation, _speedInterpolation)
    BEAM_VALUE_PROPERTY(float, SpeedDecayTime, _speedDecayTime)
    BEAM_VALUE_PROPERTY(float, Speed, _speed)
    BEAM_VALUE_PROPERTY(float, InitialSpeed, _initialSpeed)
    BEAM_VALUE_PROPERTY(float, FinalSpeed, _finalSpeed)
    BEAM_VALUE_PROPERTY(float, DamageDirMag, _damageDirMag)
    BEAM_VALUE_PROPERTY(float, RicochetLossH, _ricochetLossH)
    BEAM_VALUE_PROPERTY(float, RicochetLossV, _ricochetLossV)
    BEAM_VALUE_PROPERTY(float, CylinderRadius, _cylinderRadius)

#undef BEAM_VALUE_PROPERTY

    std::array<Vector3, 10>& BeamProjectileEntity::PastPositions() noexcept { return _pastPositions; }
    const std::array<Vector3, 10>& BeamProjectileEntity::PastPositions() const noexcept { return _pastPositions; }

    std::shared_ptr<EntityBase> BeamProjectileEntity::Owner() const noexcept { return _owner; }
    void BeamProjectileEntity::SetOwner(std::shared_ptr<EntityBase> value) noexcept { _owner = std::move(value); }
    std::shared_ptr<WeaponInfo> BeamProjectileEntity::RicochetWeapon() const noexcept { return _ricochetWeapon; }
    void BeamProjectileEntity::SetRicochetWeapon(std::shared_ptr<WeaponInfo> value) noexcept { _ricochetWeapon = std::move(value); }
    std::shared_ptr<Effects::EffectEntry> BeamProjectileEntity::Effect() const noexcept { return _effect; }
    void BeamProjectileEntity::SetEffect(std::shared_ptr<Effects::EffectEntry> value) noexcept { _effect = std::move(value); }
    std::shared_ptr<Effects::EffectEntry> BeamProjectileEntity::MuzzleEffect() const noexcept { return _muzzleEffect; }
    void BeamProjectileEntity::SetMuzzleEffect(std::shared_ptr<Effects::EffectEntry> value) noexcept { _muzzleEffect = std::move(value); }
    std::shared_ptr<EntityBase> BeamProjectileEntity::Target() const noexcept { return _target; }
    void BeamProjectileEntity::SetTarget(std::shared_ptr<EntityBase> value) noexcept { _target = std::move(value); }
    std::shared_ptr<EquipInfo> BeamProjectileEntity::Equip() const noexcept { return _equip; }
    void BeamProjectileEntity::SetEquip(std::shared_ptr<EquipInfo> value) noexcept { _equip = std::move(value); }

    void BeamProjectileEntity::Initialize()
    {
        EntityBase::Initialize();
        if (_drawFuncId == 0 || _drawFuncId == 3 || _drawFuncId == 6 || _drawFuncId == 7
            || _drawFuncId == 10 || _drawFuncId == 12)
        {
            _trailModel = Read::GetModelInstance("trail");
        }
        else if (_drawFuncId == 1 || _drawFuncId == 2)
        {
            _trailModel = Read::GetModelInstance("electroTrail");
        }
        else if (_drawFuncId == 9)
        {
            _trailModel = Read::GetModelInstance("arcWelder");
        }
        if (_trailModel)
        {
            Model& model = RequireReference(_trailModel->Model());
            Material& material = RequireReference(ManagedAt(RequireReference(model.Materials), 0));
            _bindingId = RequireReference(_scene).BindGetTexture(
                _trailModel->Model(), material.TextureId, material.PaletteId, 0);
        }
    }

    void BeamProjectileEntity::Reposition(Vector3 offset)
    {
        _spawnPosition = _spawnPosition + offset;
        Position = static_cast<Vector3>(Position) + offset;
        _backPosition = _backPosition + offset;
        for (Vector3& past : _pastPositions)
        {
            past = past + offset;
        }
    }

    bool BeamProjectileEntity::Process()
    {
        if (_lifespan <= 0.0F)
        {
            return false;
        }
        Scene& scene = RequireReference(_scene);
        _lifespan -= scene.FrameTime;
        if (TestFlag(_flags, BeamFlags::Collided))
        {
            return true;
        }
        const bool firstFrame = _age == 0.0F;
        if (TestFlag(_flags, BeamFlags::Continuous) && _age > 0.0F)
        {
            if (TestFlag(_flags, BeamFlags::Continuous) && _owner && _owner->Type == EntityType::Player)
            {
                StopHomingSfx();
                static_cast<PlayerEntity*>(_owner.get())->StopContinuousBeamSfx(_beam);
            }
            return false;
        }
        _age += scene.FrameTime;
        _backPosition = Position;
        if (scene.FrameCount % 2 == 0)
        {
            for (std::int32_t i = 9; i > 0; --i)
            {
                _pastPositions[static_cast<std::size_t>(i)] = _pastPositions[static_cast<std::size_t>(i - 1)];
            }
            _pastPositions[0] = Position;
        }
        if (TestFlag(_flags, BeamFlags::Homing) && TestFlag(_flags, BeamFlags::Continuous))
        {
            if (_target)
            {
                Vector3 targetPos{};
                _target->GetPosition(targetPos);
                Position = targetPos;
            }
            else
            {
                _velocity = Divide(_velocity, 4.0F);
            }
        }
        else
        {
            Position = static_cast<Vector3>(Position) + _velocity;
            _velocity = _velocity + Divide(_acceleration, 2.0F);
            assert(_speedDecayTime >= 0.0F);
            if (_speedDecayTime > 0.0F && _age <= _speedDecayTime)
            {
                const float magnitude = Length(_velocity);
                if (magnitude > 0.0F)
                {
                    _speed = GetInterpolatedValue(
                        _speedInterpolation, _initialSpeed, _finalSpeed, _age / _speedDecayTime);
                    _velocity = Scale(_velocity, _speed / magnitude);
                }
            }
        }
        _soundSource.Update(Position, _beam == BeamType::Missile ? 3 : 2);
        UpdateNodeRefVolume();
        if (_target)
        {
            const MessageQueueReadOnly& queue = scene.MessageQueue();
            for (std::int32_t i = 0; i < queue.Count(); ++i)
            {
                MessageInfo info = queue[i];
                if (info.Message == Message::Destroyed
                    && info.ExecuteFrame == scene.FrameCount
                    && info.Sender == _target.get())
                {
                    _target.reset();
                    break;
                }
            }
        }
        if (!TestFlag(_flags, BeamFlags::Continuous) || firstFrame)
        {
            CheckCollision();
        }
        if (TestFlag(_flags, BeamFlags::Homing) && !TestFlag(_flags, BeamFlags::Continuous) && _target)
        {
            Vector3 targetPos{};
            _target->GetPosition(targetPos);
            Vector3 acceleration = targetPos - static_cast<Vector3>(Position);
            acceleration = !IsZero(acceleration) ? Normalize(acceleration) : Vector3(1, 0, 0);
            acceleration = Scale(acceleration, _speed);
            if (Vector3::Dot(acceleration, _velocity) >= 0.0F)
            {
                acceleration = acceleration - _velocity;
                const float accelMag = Length(acceleration);
                if (accelMag > _homing)
                {
                    acceleration = Scale(acceleration, _homing / accelMag);
                }
                _velocity = _velocity + acceleration;
            }
            else
            {
                _target.reset();
            }
        }
        if (TestFlag(_flags, BeamFlags::Charged)
            && (_beam != BeamType::Missile || TestFlag(_flags, BeamFlags::Homing)))
        {
            const auto table = Metadata::BeamSfx();
            const std::int32_t sfx = ManagedAt(ManagedAt(RequireReference(table), Index(static_cast<std::int32_t>(_beam))),
                Index(static_cast<std::int32_t>(BeamSfx::Homing)));
            if (sfx != -1)
            {
                _soundSource.PlaySfx(sfx, true);
            }
        }
        if (TestFlag(_flags, BeamFlags::HasModel))
        {
            UpdateAnimFrames(_models[0]);
        }
        else if (_effect)
        {
            _effect->Transform(Position, ClearScale(static_cast<Matrix4>(Transform)));
        }
        if (_lifespan <= 0.0F)
        {
            Formats::CollisionResult colRes{};
            colRes.Plane = NegatePlane(_direction);
            colRes.Position = Position;
            SpawnCollisionEffect(colRes, true);
            OnCollision(colRes, nullptr);
            if (!TestFlag(_flags, BeamFlags::Continuous) || !_owner || _owner->Type != EntityType::Player)
            {
                PlayBeamHitSfx();
            }
        }
        return true;
    }

    void BeamProjectileEntity::CheckCollision()
    {
        Scene& scene = RequireReference(_scene);
        Formats::CollisionResult anyRes{};
        EntityBase* colWith = nullptr;
        bool noColEff = false;
        float minDist = 2.0F;

        if (_maxDistance > 0.0F)
        {
            const Vector3 frontTravel = static_cast<Vector3>(Position) - _spawnPosition;
            const float dist = Length(frontTravel);
            if (dist >= _maxDistance)
            {
                const Vector3 backTravel = _backPosition - _spawnPosition;
                const float dot = Vector3::Dot(Normalize(frontTravel), backTravel);
                float pct = 1.0F;
                if (Fixed::ToFloat(Fixed::ToInt(dist)) != Fixed::ToFloat(Fixed::ToInt(dot)))
                {
                    pct = (_maxDistance - dot) / (dist - dot);
                }
                if (pct < 2.0F)
                {
                    minDist = pct;
                    anyRes.Position = Vector3(
                        _backPosition.X + (Position.X - _backPosition.X) * pct,
                        _backPosition.Y + (Position.Y - _backPosition.Y) * pct,
                        _backPosition.Z + (Position.Z - _backPosition.Z) * pct);
                    if (_drawFuncId == 4)
                    {
                        anyRes.Plane = Vector4(0, 1, 0, 0);
                    }
                    else
                    {
                        anyRes.Plane = NegatePlane(_direction);
                    }
                    noColEff = true;
                }
            }
        }

        if (TestFlag(_flags, BeamFlags::SurfaceCollision))
        {
            Formats::CollisionResult colRes{};
            if (Formats::CollisionDetection::CheckBetweenPoints(
                    _backPosition, Position, Formats::TestFlags::Beams, _scene, colRes)
                && colRes.Distance < minDist)
            {
                const float dot = Vector3::Dot(_backPosition, colRes.Plane.Xyz()) - colRes.Plane.W;
                if (dot >= 0.0F)
                {
                    minDist = colRes.Distance;
                    anyRes = colRes;
                }
            }

            auto doors = scene.GetDoorEntities().GetEnumerator();
            while (doors.MoveNext())
            {
                const std::shared_ptr<DoorEntity> doorPtr = doors.Current();
                DoorEntity& door = RequireReference(doorPtr);
                if (TestFlag(door.Flags(), DoorFlags::Open) || door.ConnectorInactive())
                {
                    continue;
                }
                const Vector3 doorFacing = door.FacingVector();
                const Vector3 lockPos = door.LockPosition();
                Vector4 plane(doorFacing, 0.0F);
                if (Vector3::Dot(_backPosition - lockPos, doorFacing) < 0.0F)
                {
                    plane.X *= -1.0F; plane.Y *= -1.0F; plane.Z *= -1.0F; plane.W *= -1.0F;
                }
                const Vector3 wvec = ComponentMultiply(
                    plane.Xyz(), lockPos + Scale(plane.Xyz(), 0.4F));
                plane.W = wvec.X + wvec.Y + wvec.Z;
                if (Formats::CollisionDetection::CheckCylinderIntersectPlane(
                        _backPosition, Position, plane, colRes)
                    && colRes.Distance < minDist)
                {
                    const Vector3 between = colRes.Position - lockPos;
                    if (LengthSquared(between) < door.RadiusSquared())
                    {
                        minDist = colRes.Distance;
                        anyRes = colRes;
                        colWith = doorPtr.get();
                        noColEff = false;
                        anyRes.Field0 = 0;
                        anyRes.Plane = plane;
                        anyRes.Flags = Formats::Collision::CollisionFlags::None;
                    }
                }
            }

            auto forceFields = scene.GetForceFieldEntities().GetEnumerator();
            while (forceFields.MoveNext())
            {
                const std::shared_ptr<ForceFieldEntity> forceFieldPtr = forceFields.Current();
                ForceFieldEntity& forceField = RequireReference(forceFieldPtr);
                if (forceField.Active
                    && Formats::CollisionDetection::CheckCylinderIntersectPlane(
                        _backPosition, Position, forceField.Plane(), colRes)
                    && colRes.Distance < minDist)
                {
                    const Vector3 between = colRes.Position - static_cast<Vector3>(forceField.Position);
                    float dot = Vector3::Dot(between, forceField.FieldUpVector());
                    if (dot <= forceField.Height() && dot >= -forceField.Height())
                    {
                        dot = Vector3::Dot(between, forceField.FieldRightVector());
                        if (dot <= forceField.Width() && dot >= -forceField.Width())
                        {
                            minDist = colRes.Distance;
                            anyRes = colRes;
                            colWith = forceFieldPtr.get();
                            noColEff = false;
                            anyRes.Field0 = 0;
                            anyRes.Plane = forceField.Plane();
                        }
                    }
                }
            }
        }

        assert(_owner != nullptr);
        EntityBase& owner = RequireReference(_owner);
        if (owner.Type != EntityType::EnemyInstance)
        {
            auto enemies = scene.GetEnemyInstanceEntities().GetEnumerator();
            while (enemies.MoveNext())
            {
                const std::shared_ptr<EnemyInstanceEntity> enemyPtr = enemies.Current();
                EnemyInstanceEntity& enemy = RequireReference(enemyPtr);
                Formats::CollisionResult res{};
                CollisionVolume hurtVolume = enemy.HurtVolume();
                if (TestFlag(enemy.Flags(), EnemyFlags::CollideBeam)
                    && Formats::CollisionDetection::CheckCylinderOverlapVolume(
                        &hurtVolume, _backPosition, Position, _cylinderRadius, res))
                {
                    if (_beam == BeamType::OmegaCannon && enemy.EnemyType() == EnemyType::GoreaMeteor)
                    {
                        enemy.TakeDamage(500, this);
                    }
                    else if (res.Distance < minDist)
                    {
                        minDist = res.Distance;
                        anyRes = res;
                        colWith = enemyPtr.get();
                        noColEff = false;
                    }
                }
            }
        }

        bool hitHalfturret = false;
        auto players = scene.GetPlayerEntities().GetEnumerator();
        while (players.MoveNext())
        {
            const std::shared_ptr<PlayerEntity> playerPtr = players.Current();
            PlayerEntity& player = RequireReference(playerPtr);
            if (player.Health() == 0 || TestFlag(player.Flags2(), PlayerFlags2::Spectating))
            {
                continue;
            }
            if (Mods::Network::NetLog::Enabled)
            {
                ManagedAt(Mods::Network::NetDamage::PlayerChecks, Index(player.SlotIndex()))++;
            }
            const bool hasHalfturret = player.Hunter() == Hunter::Weavel
                && TestFlag(player.Flags2(), PlayerFlags2::Halfturret);
            const std::shared_ptr<HalfturretEntity> playerTurret = player.Halfturret();
            if ((_owner.get() == playerPtr.get()
                    || (hasHalfturret && _owner.get() == playerTurret.get()))
                && (!TestFlag(_flags, BeamFlags::SelfDamage) || _age < (1.0F / 30.0F) * 4.0F))
            {
                continue;
            }

            bool hitPlayer = false;
            Formats::CollisionResult playerRes{};
            const float radii = player.Volume().SphereRadius + _cylinderRadius;
            if (player.IsAltForm())
            {
                if (player.Hunter() == Hunter::Kanden)
                {
                    if (Formats::CollisionDetection::CheckCylinderOverlapSphere(
                            _backPosition, Position, ManagedAt(player.KandenSegPos(), 2), 1.6F, playerRes))
                    {
                        if (Formats::CollisionDetection::CheckCylinderOverlapSphere(
                                _backPosition, Position, player.Volume().SpherePosition, radii, playerRes)
                            || Formats::CollisionDetection::CheckCylinderOverlapSphere(
                                _backPosition, Position, ManagedAt(player.KandenSegPos(), 1), radii, playerRes)
                            || Formats::CollisionDetection::CheckCylinderOverlapSphere(
                                _backPosition, Position, ManagedAt(player.KandenSegPos(), 2), radii, playerRes)
                            || Formats::CollisionDetection::CheckCylinderOverlapSphere(
                                _backPosition, Position, ManagedAt(player.KandenSegPos(), 3), radii, playerRes))
                        {
                            hitPlayer = true;
                        }
                    }
                }
                else if (Formats::CollisionDetection::CheckCylinderOverlapSphere(
                    _backPosition, Position, player.Volume().SpherePosition, radii, playerRes))
                {
                    hitPlayer = true;
                }
            }
            else
            {
                const float minY = Fixed::ToFloat(player.Values().MinPickupHeight);
                const Vector3 playerBottom = AddY(player.Position, minY);
                const float dot = Fixed::ToFloat(player.Values().MaxPickupHeight) - minY;
                if (Formats::CollisionDetection::CheckCylindersOverlap(
                    _backPosition, Position, playerBottom, Vector3(0, 1, 0), dot, radii, playerRes))
                {
                    hitPlayer = true;
                }
            }

            if (hitPlayer && playerRes.Distance < minDist)
            {
                Mods::Network::NetDamage::NotePlayerOverlap(_owner.get(), &player);
                if (Mods::Network::NetLog::Enabled)
                {
                    ManagedAt(Mods::Network::NetDamage::PlayerOverlaps, Index(player.SlotIndex()))++;
                    ManagedAt(Mods::Network::NetDamage::PlayerAccepted, Index(player.SlotIndex()))++;
                }
                minDist = playerRes.Distance;
                anyRes = playerRes;
                colWith = playerPtr.get();
                noColEff = false;
                hitHalfturret = false;
            }
            else if (hitPlayer && Mods::Network::NetLog::Enabled)
            {
                ManagedAt(Mods::Network::NetDamage::PlayerOverlaps, Index(player.SlotIndex()))++;
            }

            if (hasHalfturret && _owner.get() != playerTurret.get())
            {
                Formats::CollisionResult turretRes{};
                const float radius = _cylinderRadius + 0.45F;
                HalfturretEntity& turret = RequireReference(playerTurret);
                if (Formats::CollisionDetection::CheckCylinderOverlapSphere(
                        _backPosition, Position, turret.Position, radius, turretRes)
                    && turretRes.Distance < minDist)
                {
                    minDist = turretRes.Distance;
                    anyRes = turretRes;
                    colWith = playerTurret.get();
                    noColEff = false;
                    hitHalfturret = true;
                }
            }
        }

        if (GameState::SinglePlayer)
        {
            auto beams = scene.GetBeamProjectileEntities().GetEnumerator();
            while (beams.MoveNext())
            {
                const std::shared_ptr<BeamProjectileEntity> otherPtr = beams.Current();
                BeamProjectileEntity& other = RequireReference(otherPtr);
                if (other._owner == _owner || TestFlag(other._flags, BeamFlags::Collided)
                    || !TestFlag(other._flags, BeamFlags::Destroyable)
                    || (other._owner && other._owner->Type == EntityType::EnemyInstance
                        && owner.Type == EntityType::EnemyInstance))
                {
                    continue;
                }
                Formats::CollisionResult beamRes{};
                const std::int32_t radiusIndex = static_cast<std::int32_t>(
                    static_cast<std::uint16_t>(other._flags & (BeamFlags::RadiusIndex1 | BeamFlags::RadiusIndex2))) >> 9;
                const float radius = ManagedAt(Metadata::BeamRadiusValues, Index(radiusIndex));
                if (Formats::CollisionDetection::CheckCylinderOverlapSphere(
                        _backPosition, Position, other.Position, radius, beamRes)
                    && beamRes.Distance < minDist)
                {
                    minDist = beamRes.Distance;
                    anyRes = beamRes;
                    colWith = otherPtr.get();
                    noColEff = true;
                }
            }
        }

        if (minDist < 0.0F || minDist > 1.0F)
        {
            return;
        }

        const float amt = Fixed::ToFloat(204);
        Position = Vector3(
            anyRes.Position.X + anyRes.Plane.X * amt,
            anyRes.Position.Y + anyRes.Plane.Y * amt,
            anyRes.Position.Z + anyRes.Plane.Z * amt);
        bool ricochet = true;
        if (_drawFuncId == 12)
        {
            _soundSource.PlaySfx(SfxId::BIGEYE_ATTACK1C, false, true);
        }

        if (colWith != nullptr)
        {
            if (colWith->Type == EntityType::Player || colWith->Type == EntityType::Halfturret)
            {
                PlayerEntity* player;
                if (colWith->Type == EntityType::Player)
                {
                    player = static_cast<PlayerEntity*>(colWith);
                }
                else
                {
                    const auto turretOwner = static_cast<HalfturretEntity*>(colWith)->Owner();
                    player = std::addressof(RequireReference(turretOwner));
                }
                DamageFlags damageFlags = DamageFlags::NoDmgInvuln;
                if (ManagedAt(player->BeamEffectiveness(), Index(static_cast<std::int32_t>(_beam))) == Effectiveness::Zero)
                {
                    if (GameState::SinglePlayer && _owner.get() == PlayerEntity::Main())
                    {
                        Matrix4 transform = GetTransformMatrix(Vector3(1, 0, 0), Vector3(0, 1, 0), player->Position);
                        std::shared_ptr<Effects::EffectEntry> effect = scene.SpawnEffectGetEntry(115, transform);
                        if (effect)
                        {
                            effect->SetReadOnlyField(0, 1.0F);
                            scene.DetachEffectEntry(effect, false);
                        }
                    }
                }
                else
                {
                    if (hitHalfturret)
                    {
                        damageFlags = static_cast<DamageFlags>(
                            static_cast<std::underlying_type_t<DamageFlags>>(damageFlags)
                            | static_cast<std::underlying_type_t<DamageFlags>>(DamageFlags::Halfturret));
                    }
                    const Vector3 damageDir = GetDamageDirection(anyRes.Position, player->Position);
                    float damage = 0.0F;
                    std::uint32_t wholeDamage = 0;
                    bool isHeadshot = false;
                    if (!player->IsAltForm() && _beam != BeamType::ShockCoil
                        && anyRes.Position.Y - player->Position.Y
                            >= Fixed::ToFloat(player->Values().MaxPickupHeight) - 0.3F)
                    {
                        if (_beam == BeamType::Imperialist)
                        {
                            isHeadshot = true;
                        }
                        else
                        {
                            const Vector3 travel = static_cast<Vector3>(Position) - _spawnPosition;
                            isHeadshot = LengthSquared(travel) <= 15.0F * 15.0F;
                        }
                    }
                    if (isHeadshot)
                    {
                        if (_maxDistance > 0.0F)
                        {
                            const float pct = Vector3::Distance(Position, _spawnPosition) / _maxDistance;
                            damage = GetInterpolatedValue(_damageInterpolation, _headshotDamage, 0.0F, pct);
                        }
                        else
                        {
                            damage = _headshotDamage;
                        }
                        if (std::fabs(_damage - _headshotDamage) > 1.0F / 4096.0F)
                        {
                            damageFlags = static_cast<DamageFlags>(
                                static_cast<std::underlying_type_t<DamageFlags>>(damageFlags)
                                | static_cast<std::underlying_type_t<DamageFlags>>(DamageFlags::Headshot));
                        }
                    }
                    else
                    {
                        if (_maxDistance > 0.0F)
                        {
                            const float pct = Vector3::Distance(Position, _spawnPosition) / _maxDistance;
                            damage = GetInterpolatedValue(_damageInterpolation, _damage, 0.0F, pct);
                        }
                        else
                        {
                            damage = _damage;
                        }
                    }
                    wholeDamage = static_cast<std::uint32_t>(std::clamp(
                        damage, 0.0F, static_cast<float>(std::numeric_limits<std::int32_t>::max())));
                    if (wholeDamage != 0)
                    {
                        player->TakeDamage(wholeDamage, damageFlags, damageDir, this);
                    }
                    if (TestFlag(_flags, BeamFlags::LifeDrain) && owner.Type == EntityType::Player)
                    {
                        PlayerEntity* ownerPlayer = static_cast<PlayerEntity*>(_owner.get());
                        if (!ownerPlayer->IsPrimeHunter() && ownerPlayer->TeamIndex() != player->TeamIndex())
                        {
                            const std::int32_t before = ownerPlayer->Health();
                            ownerPlayer->GainHealth(wholeDamage);
                            Mods::Network::NetHitPrediction::NoteDrain(
                                ownerPlayer, ownerPlayer->Health() - before);
                        }
                    }
                    if (!player->IsMainPlayer() || player->IsAltForm() || player->IsMorphing())
                    {
                        SpawnCollisionEffect(anyRes, true);
                    }
                }
                OnCollision(anyRes, colWith);
                PlayBeamHitSfx();
                ricochet = false;
            }
            else if (colWith->Type == EntityType::EnemyInstance)
            {
                EnemyInstanceEntity* enemy = static_cast<EnemyInstanceEntity*>(colWith);
                const std::shared_ptr<EntityBase> enemyOwner = enemy->Owner();
                EnemyInstanceEntity* enemyOwnerInst = enemyOwner && enemyOwner->Type == EntityType::EnemyInstance
                    ? static_cast<EnemyInstanceEntity*>(enemyOwner.get()) : nullptr;
                if (enemy->GetEffectiveness(_beam) == Effectiveness::Zero
                    && (enemy->EnemyType() == EnemyType::FireSpawn
                        || (enemyOwnerInst != nullptr && enemyOwnerInst->EnemyType() == EnemyType::FireSpawn)))
                {
                    const Vector3 facing = Normalize(enemy->Transform.Row2().Xyz());
                    const float w = Vector3::Dot(facing,
                        static_cast<Vector3>(enemy->Position) + Scale(facing, Fixed::ToFloat(0x3800)));
                    anyRes.Plane = Vector4(facing, w);
                    const float dot = Vector3::Dot(Position, facing);
                    anyRes.Position = Vector3(
                        Position.X + facing.X * (dot - w),
                        Position.Y + facing.Y * (dot - w),
                        Position.Z + facing.Z * (dot - w));
                    ProcessRicochet(anyRes);
                }
                else
                {
                    float damage = _damage;
                    if (_maxDistance > 0.0F)
                    {
                        const float pct = Vector3::Distance(Position, _spawnPosition) / _maxDistance;
                        damage = GetInterpolatedValue(_damageInterpolation, _damage, 0.0F, pct);
                    }
                    if (damage > 0.0F && (_beam != BeamType::ShockCoil || scene.FrameCount % 2 == 0))
                    {
                        enemy->TakeDamage(static_cast<std::uint32_t>(damage), this);
                        SpawnCollisionEffect(anyRes, true);
                    }
                    OnCollision(anyRes, colWith);
                    PlayBeamHitSfx();
                }
                ricochet = false;
            }
            else if (colWith->Type == EntityType::Door)
            {
                DoorEntity* door = static_cast<DoorEntity*>(colWith);
                SpawnCollisionEffect(anyRes, true);
                OnCollision(anyRes, colWith);
                PlayBeamHitSfx();
                if (_owner && _owner->Type == EntityType::Player)
                {
                    PlayerEntity* player = static_cast<PlayerEntity*>(_owner.get());
                    if (player->IsMainPlayer() || scene.CameraMode != CameraMode::Player)
                    {
                        if (TestFlag(door->Flags(), DoorFlags::Locked)
                            && !TestFlag(door->Flags(), DoorFlags::ShowLock))
                        {
                            if (door->Data().PaletteId == static_cast<std::int32_t>(_beam))
                            {
                                door->Unlock(true, true);
                            }
                            else if (GameState::SinglePlayer)
                            {
                                scene.SendMessage(Message::ShowWarning, this, nullptr,
                                    BoxInt32(40), BoxInt32(180), 10);
                            }
                        }
                        if (!GameState::InRoomTransition)
                        {
                            door->SetFlags(door->Flags() | DoorFlags::ShotOpen);
                        }
                    }
                }
                ricochet = false;
            }
            else if (colWith->Type == EntityType::ForceField)
            {
                ForceFieldEntity* forceField = static_cast<ForceFieldEntity*>(colWith);
                if (!TestFlag(_flags, BeamFlags::Ricochet))
                {
                    SpawnCollisionEffect(anyRes, true);
                    OnCollision(anyRes, colWith);
                    PlayBeamHitSfx();
                    const auto lock = forceField->Lock();
                    if (lock)
                    {
                        lock->LockHit(this);
                    }
                    ricochet = false;
                }
            }
            else if (colWith->Type == EntityType::BeamProjectile)
            {
                BeamProjectileEntity* other = static_cast<BeamProjectileEntity*>(colWith);
                if (TestFlag(_flags, BeamFlags::ForceEffect))
                {
                    SpawnCollisionEffect(anyRes, true);
                }
                OnCollision(anyRes, colWith);
                PlayBeamHitSfx();
                if (TestFlag(other->_flags, BeamFlags::ForceEffect))
                {
                    other->SpawnCollisionEffect(anyRes, true);
                }
                if (other->_drawFuncId == 12)
                {
                    _soundSource.PlaySfx(SfxId::BIGEYE_ATTACK1C, false, true);
                    ItemType item = ItemType::None;
                    const std::uint32_t rand = Rng::GetRandomInt2(100U);
                    if (scene.AreaId == 0)
                    {
                        if (rand < 5)
                        {
                            item = ItemType::HealthMedium;
                        }
                        else if (rand < 10)
                        {
                            item = ItemType::MissileSmall;
                        }
                    }
                    else
                    {
                        if (rand < 25)
                        {
                            item = ItemType::HealthMedium;
                        }
                        else if (rand < 75)
                        {
                            item = ItemType::UASmall;
                        }
                    }
                    if (item != ItemType::None)
                    {
                        const Formats::Culling::NodeRef nodeRef = scene.GetNodeRefByPosition(other->Position);
                        ItemSpawnEntity::SpawnItemDrop(item, other->Position, nodeRef, 100, _scene);
                    }
                }
                else if (other->_drawFuncId == 11)
                {
                    _soundSource.PlaySfx(SfxId::GOREA_ATTACK3B, false, true);
                }
                else
                {
                    _soundSource.PlaySfx(SfxId::LOB_GUN_HIT, false, true);
                }
                other->OnCollision(anyRes, this);
                ricochet = false;
            }
        }
        else
        {
            bool reflected = TestFlag(anyRes.Flags, Formats::Collision::CollisionFlags::ReflectBeams);
            if (anyRes.EntityCollision)
            {
                scene.SendMessage(Message::BeamCollideWith, this, anyRes.EntityCollision->Entity.get(),
                    BoxValue(anyRes), BoxInt32(0));
                RequireReference(anyRes.EntityCollision->Entity).CheckBeamReflection(reflected);
            }
            if ((!TestFlag(_flags, BeamFlags::Ricochet) && !reflected)
                || _drawFuncId == 8 || anyRes.Terrain() >= Terrain::Acid)
            {
                if (!noColEff || TestFlag(_flags, BeamFlags::ForceEffect))
                {
                    const bool noSplat = anyRes.Terrain() == Terrain::Lava || anyRes.EntityCollision != nullptr;
                    SpawnCollisionEffect(anyRes, noSplat);
                }
                if (_ricochetWeapon)
                {
                    PlayRicochetSfx();
                }
                else if (anyRes.Terrain() <= Terrain::Lava)
                {
                    PlayBeamHitSfx();
                }
                else
                {
                    _soundSource.PlaySfx(SfxId::GENERIC_HIT, false, true);
                }
                OnCollision(anyRes, nullptr);
                ricochet = false;
            }
        }

        if (ricochet)
        {
            ProcessRicochet(anyRes);
        }
        if (_drawFuncId == 8)
        {
            SpawnSniperBeam();
        }
    }

    void BeamProjectileEntity::ProcessRicochet(Formats::CollisionResult colRes)
    {
        const float dot1 = Vector3::Dot(_velocity, colRes.Plane.Xyz());
        _velocity = Vector3(
            (_velocity.X - 2.0F * colRes.Plane.X * dot1) * _ricochetLossH,
            (_velocity.Y - 2.0F * colRes.Plane.Y * dot1) * _ricochetLossV,
            (_velocity.Z - 2.0F * colRes.Plane.Z * dot1) * _ricochetLossH);
        _speed = Length(_velocity);
        const float dot2 = Vector3::Dot(_direction, colRes.Plane.Xyz());
        _direction = Vector3(
            _direction.X - 2.0F * colRes.Plane.X * dot2,
            _direction.Y - 2.0F * colRes.Plane.Y * dot2,
            _direction.Z - 2.0F * colRes.Plane.Z * dot2);
        const float dot3 = Vector3::Dot(colRes.Position, colRes.Plane.Xyz());
        const float factor = 0.01F - (dot3 - colRes.Plane.W);
        _backPosition = Vector3(
            colRes.Position.X + colRes.Plane.X * factor,
            colRes.Position.Y + colRes.Plane.Y * factor,
            colRes.Position.Z + colRes.Plane.Z * factor);
        Position = _backPosition;
        for (std::int32_t i = 9; i > 0; --i)
        {
            _pastPositions[static_cast<std::size_t>(i)] = _pastPositions[static_cast<std::size_t>(i - 1)];
        }
        _pastPositions[0] = Position;
        if (RequireReference(_scene).FrameCount % 2 == 0)
        {
            for (std::int32_t i = 9; i > 0; --i)
            {
                _pastPositions[static_cast<std::size_t>(i)] = _pastPositions[static_cast<std::size_t>(i - 1)];
            }
            _pastPositions[0] = Position;
        }
        PlayRicochetSfx();
    }

    void BeamProjectileEntity::PlayRicochetSfx()
    {
        const bool charged = TestFlag(_flags, BeamFlags::Charged);
        if (charged && _beam == BeamType::Magmaul)
        {
            PlayBeamHitSfx();
            return;
        }
        const auto table = Metadata::BeamSfx();
        const std::int32_t sfx = ManagedAt(ManagedAt(RequireReference(table), Index(static_cast<std::int32_t>(_beam))),
            Index(static_cast<std::int32_t>(BeamSfx::Ricochet)));
        if (sfx != -1)
        {
            float amountA;
            if (_beam == BeamType::Judicator)
            {
                amountA = static_cast<float>(Rng::GetRandomInt1(0xFFFF));
            }
            else
            {
                amountA = 0xFFFF * (_speed * 2.0F) / Fixed::ToFloat(3300);
            }
            _soundSource.PlaySfx(sfx, false, true, -1.0F, false, false, amountA);
        }
    }

    void BeamProjectileEntity::StopHomingSfx()
    {
        const auto table = Metadata::BeamSfx();
        const std::int32_t sfx = ManagedAt(ManagedAt(RequireReference(table), Index(static_cast<std::int32_t>(_beam))),
            Index(static_cast<std::int32_t>(BeamSfx::Homing)));
        if (sfx != -1)
        {
            _soundSource.StopSfx(sfx);
        }
    }

    void BeamProjectileEntity::PlayBeamHitSfx()
    {
        StopHomingSfx();
        const BeamSfx type = TestFlag(_flags, BeamFlags::Charged) ? BeamSfx::ChargeHit : BeamSfx::Hit;
        const auto table = Metadata::BeamSfx();
        const std::int32_t sfx = ManagedAt(ManagedAt(RequireReference(table), Index(static_cast<std::int32_t>(_beam))),
            Index(static_cast<std::int32_t>(type)));
        if (sfx != -1)
        {
            _soundSource.PlaySfx(sfx, false, true);
        }
    }

    void BeamProjectileEntity::OnCollision(Formats::CollisionResult colRes, EntityBase* colWith)
    {
        if (_effect)
        {
            RequireReference(_scene).DetachEffectEntry(_effect, true);
            _effect.reset();
        }
        if (_splashDamage > 0.0F && colRes.Terrain() <= Terrain::Lava)
        {
            assert(_equip != nullptr);
            assert(_owner != nullptr);
            CheckSplashDamage(colWith);
            if (_ricochetWeapon && (colWith == nullptr || colWith->Type != EntityType::Player))
            {
                const Vector3 factor = Scale(_velocity, 7.0F);
                const float dot = Vector3::Dot(colRes.Plane.Xyz(), factor);
                const Vector3 spawnDir = Normalize(Vector3(
                    colRes.Plane.X + factor.X - colRes.Plane.X * 2.0F * dot,
                    colRes.Plane.Y + factor.Y - colRes.Plane.Y * 2.0F * dot,
                    colRes.Plane.Z + factor.Z - colRes.Plane.Z * 2.0F * dot));
                if (RequireReference(_owner).Type == EntityType::Player)
                {
                    PlayerEntity* player = static_cast<PlayerEntity*>(_owner.get());
                    if (player->IsBot() && GameState::SinglePlayer && player->Hunter() == Hunter::Spire)
                    {
                        const std::int32_t encounter
                            = ManagedAt(GameState::EncounterState, Index(player->SlotIndex()));
                        std::uint16_t damage = 3;
                        if (encounter == 2 || (encounter == 0 && player->BotLevel() > 0))
                        {
                            damage = 4;
                        }
                        _ricochetEquip->SetUnchargedDamage(damage);
                        _ricochetEquip->SetMinChargeDamage(damage);
                        _ricochetEquip->SetChargedDamage(damage);
                        _ricochetEquip->SetHeadshotDamage(damage);
                        _ricochetEquip->SetMinChargeHeadshotDamage(damage);
                        _ricochetEquip->SetChargedHeadshotDamage(damage);
                        _ricochetEquip->SetSplashDamage(damage);
                        _ricochetEquip->SetMinChargeSplashDamage(damage);
                        _ricochetEquip->SetChargedSplashDamage(damage);
                        ManagedAt(_ricochetEquip->DmgDirTypes(), 0) = 0;
                        ManagedAt(_ricochetEquip->DmgDirTypes(), 1) = 0;
                    }
                }
                EquipInfo& equip = RequireReference(_equip);
                _ricochetEquip->SetBeams(equip.Beams());
                _ricochetEquip->SetWeapon(_ricochetWeapon);
                BeamSpawnFlags flags = BeamSpawnFlags::None;
                if (TestFlag(_flags, BeamFlags::Charged))
                {
                    flags |= BeamSpawnFlags::Charged;
                }
                static_cast<void>(Spawn(
                    _owner, _ricochetEquip, colRes.Position, spawnDir, flags, NodeRef, _scene));
            }
        }
        if (!TestFlag(_flags, BeamFlags::Continuous))
        {
            _flags |= BeamFlags::Collided;
            _lifespan = 4.0F * (1.0F / 30.0F);
            _velocity = Vector3::Zero;
        }
        if (_owner)
        {
            RequireReference(_scene).SendMessage(Message::Impact, this, _owner.get(),
                BoxEntityOrZero(colWith), BoxInt32(0));
            StopHomingSfx();
        }
    }

    void BeamProjectileEntity::CheckSplashDamage(EntityBase* colWith)
    {
        Scene& scene = RequireReference(_scene);
        auto players = scene.GetPlayerEntities().GetEnumerator();
        while (players.MoveNext())
        {
            const std::shared_ptr<PlayerEntity> playerPtr = players.Current();
            PlayerEntity& player = RequireReference(playerPtr);
            if (playerPtr.get() == colWith)
            {
                continue;
            }

            const auto omegaCannonFlash = [&]()
            {
                if (_beam == BeamType::OmegaCannon && playerPtr.get() == PlayerEntity::Main())
                {
                    scene.SetFade(FadeType::FadeInWhite, 15.0F / 30.0F, false);
                }
            };

            if (player.Health() > 0)
            {
                const std::shared_ptr<HalfturretEntity> turret = player.Halfturret();
                if (!TestFlag(player.Flags2(), PlayerFlags2::Halfturret)
                    || _owner.get() != turret.get())
                {
                    Formats::CollisionResult discard{};
                    const float dist = Vector3::Distance(player.Position, Position);
                    if (dist >= _splashRadius
                        || Formats::CollisionDetection::CheckBetweenPoints(
                            Position, player.Position, Formats::TestFlags::Beams, _scene, discard))
                    {
                        omegaCannonFlash();
                    }
                    else
                    {
                        const Vector3 damageDir = GetDamageDirection(Position, player.Position);
                        const float ratio = dist / _splashRadius;
                        const std::int32_t damage = static_cast<std::int32_t>(
                            GetInterpolatedValue(_splashDamageType, _splashDamage, 0.0F, ratio));
                        player.TakeDamage(damage, DamageFlags::NoDmgInvuln, damageDir, this);
                        if (_owner)
                        {
                            scene.SendMessage(Message::Impact, this, _owner.get(),
                                BoxValue(static_cast<EntityBase*>(&player)), BoxInt32(0));
                            StopHomingSfx();
                        }
                    }
                }
            }
            else
            {
                omegaCannonFlash();
            }
        }

        auto enemies = scene.GetEnemyInstanceEntities().GetEnumerator();
        while (enemies.MoveNext())
        {
            const std::shared_ptr<EnemyInstanceEntity> enemyPtr = enemies.Current();
            EnemyInstanceEntity& enemy = RequireReference(enemyPtr);
            if (enemyPtr.get() == colWith || !TestFlag(enemy.Flags(), EnemyFlags::CollideBeam))
            {
                continue;
            }
            Formats::CollisionResult res{};
            const float dist = Vector3::Distance(enemy.Position, Position);
            if (dist < _splashRadius
                && !Formats::CollisionDetection::CheckBetweenPoints(
                    Position, enemy.Position, Formats::TestFlags::Beams, _scene, res))
            {
                const float damage = GetInterpolatedValue(
                    _splashDamageType, _splashDamage, 0.0F, dist / _splashRadius);
                enemy.TakeDamage(static_cast<std::uint32_t>(damage), this);
                if (_owner)
                {
                    scene.SendMessage(Message::Impact, this, _owner.get(),
                        BoxValue(static_cast<EntityBase*>(&enemy)), BoxInt32(0));
                    StopHomingSfx();
                }
            }
        }
    }

    float BeamProjectileEntity::GetInterpolatedValue(
        std::int32_t type, float value1, float value2, float ratio) const
    {
        if (type == 3)
        {
            return ratio > 1.0F ? value2 : value1;
        }
        ratio = std::clamp(ratio, 0.0F, 1.0F);
        if (type == 0)
        {
            return value1 + (value2 - value1) * ratio;
        }
        if (type == 1)
        {
            return value1 + (value2 - value1)
                * ((std::sin(DegreesToRadians(270.0F - 180.0F * ratio)) + 1.0F) / 2.0F);
        }
        if (type == 2)
        {
            return value1 + (value2 - value1)
                * (std::sin(DegreesToRadians(270.0F - 90.0F * ratio)) + 1.0F);
        }
        return 0.0F;
    }

    void BeamProjectileEntity::GetDrawInfo()
    {
        if (_drawFuncId == 0) Draw00();
        else if (_drawFuncId == 1) Draw01();
        else if (_drawFuncId == 2) Draw02();
        else if (_drawFuncId == 3) Draw03();
        else if (_drawFuncId == 6 || _drawFuncId == 12) Draw06();
        else if (_drawFuncId == 7) Draw07();
        else if (_drawFuncId == 9) Draw09();
        else if (_drawFuncId == 10) Draw10();
        else if (_drawFuncId == 17) Draw17();
    }

    void BeamProjectileEntity::Draw00()
    {
        if (!TestFlag(_flags, BeamFlags::Collided))
        {
            RequireReference(_scene).AddSingleParticle(
                SingleType::Fuzzball, Position, _color, 1.0F, 1.0F / 4.0F);
        }
        DrawTrail1(Fixed::ToFloat(122));
    }

    void BeamProjectileEntity::Draw01()
    {
        DrawTrail1(Fixed::ToFloat(614));
    }

    void BeamProjectileEntity::Draw02()
    {
        DrawTrail2(Fixed::ToFloat(1024), 5);
    }

    void BeamProjectileEntity::Draw03()
    {
        if (!TestFlag(_flags, BeamFlags::Collided))
        {
            EntityBase::GetDrawInfo();
        }
        DrawTrail2(Fixed::ToFloat(204), 5);
    }

    void BeamProjectileEntity::Draw06()
    {
        if (!TestFlag(_flags, BeamFlags::Collided))
        {
            RequireReference(_scene).AddSingleParticle(
                SingleType::Fuzzball, Position, Vector3(1.0F, 1.0F, 1.0F), 1.0F, 1.0F / 4.0F);
        }
        DrawTrail3(Fixed::ToFloat(204));
    }

    void BeamProjectileEntity::Draw07()
    {
        if (!TestFlag(_flags, BeamFlags::Collided))
        {
            RequireReference(_scene).AddSingleParticle(
                SingleType::Fuzzball, Position, Vector3(1.0F, 1.0F, 1.0F), 1.0F, 1.0F / 4.0F);
        }
        DrawTrail2(Fixed::ToFloat(204), 5);
    }

    void BeamProjectileEntity::Draw09()
    {
        if (!TestFlag(_flags, BeamFlags::Collided))
        {
            if (_target)
            {
                DrawTrail4(0.15F, 0.5F, 10);
            }
            else if (_owner.get() == PlayerEntity::Main())
            {
                DrawTrail4(0.025F, 0.35F, 5);
            }
        }
    }

    void BeamProjectileEntity::Draw10()
    {
        DrawTrail2(Fixed::ToFloat(81), 2);
    }

    void BeamProjectileEntity::Draw17()
    {
        if (!TestFlag(_flags, BeamFlags::Collided))
        {
            EntityBase::GetDrawInfo();
        }
    }

    void BeamProjectileEntity::DrawTrail1(float height)
    {
        assert(_trailModel != nullptr);
        Model& model = RequireReference(RequireReference(_trailModel).Model());
        const Recolor& recolor = RequireReference(ManagedAt(RequireReference(model.Recolors), 0));
        const Texture& texture = ManagedAt(RequireReference(recolor.Textures), 0);
        const float uvS = (texture.Width - (1.0F / 16.0F)) / texture.Width;
        const float uvT = (texture.Height - (1.0F / 16.0F)) / texture.Height;
        auto uvsAndVerts = std::make_shared<ManagedArray<Vector3>>(8);
        (*uvsAndVerts)[0] = Vector3::Zero;
        (*uvsAndVerts)[1] = Vector3(
            Position.X - _backPosition.X,
            Position.Y - _backPosition.Y - height,
            Position.Z - _backPosition.Z);
        (*uvsAndVerts)[2] = Vector3(0, uvT, 0);
        (*uvsAndVerts)[3] = Vector3(
            Position.X - _backPosition.X,
            height + Position.Y - _backPosition.Y,
            Position.Z - _backPosition.Z);
        (*uvsAndVerts)[4] = Vector3(uvS, 0, 0);
        (*uvsAndVerts)[5] = Vector3(0, -height, 0);
        (*uvsAndVerts)[6] = Vector3(uvS, uvT, 0);
        (*uvsAndVerts)[7] = Vector3(0, height, 0);
        Material& material = RequireReference(ManagedAt(RequireReference(model.Materials), 0));
        const float alpha = std::clamp(_lifespan * 30.0F * 8.0F, 0.0F, 31.0F) / 31.0F;
        Scene& scene = RequireReference(_scene);
        scene.AddRenderItem(RenderItemType::TrailSingle, alpha, scene.GetNextPolygonId(), _color,
            material.XRepeat, material.YRepeat, material.ScaleS, material.ScaleT,
            Translation(_backPosition), uvsAndVerts, _bindingId);
    }

    void BeamProjectileEntity::DrawTrail2(float height, std::int32_t segments)
    {
        assert(_trailModel != nullptr);
        if (segments < 2)
        {
            return;
        }
        if (segments > static_cast<std::int32_t>(_pastPositions.size() / 2))
        {
            segments = static_cast<std::int32_t>(_pastPositions.size() / 2);
        }
        const std::int32_t count = 4 * segments;
        Model& model = RequireReference(RequireReference(_trailModel).Model());
        const Recolor& recolor = RequireReference(ManagedAt(RequireReference(model.Recolors), 0));
        const Texture& texture = ManagedAt(RequireReference(recolor.Textures), 0);
        const float uvT = (texture.Height - (1.0F / 16.0F)) / texture.Height;
        auto uvsAndVerts = std::make_shared<ManagedArray<Vector3>>(static_cast<std::size_t>(count));
        for (std::int32_t i = 0; i < segments; ++i)
        {
            float uvS = 0.0F;
            if (i > 0)
            {
                uvS = (texture.Width / static_cast<float>(segments - 1) * i - (1.0F / 16.0F))
                    / texture.Width;
            }
            const Vector3 vec = ManagedAt(_pastPositions, static_cast<std::size_t>(i * 2)) - _pastPositions[0];
            (*uvsAndVerts)[static_cast<std::size_t>(4 * i)] = Vector3(uvS, 0, 0);
            (*uvsAndVerts)[static_cast<std::size_t>(4 * i + 1)] = Vector3(vec.X, vec.Y - height, vec.Z);
            (*uvsAndVerts)[static_cast<std::size_t>(4 * i + 2)] = Vector3(uvS, uvT, 0);
            (*uvsAndVerts)[static_cast<std::size_t>(4 * i + 3)] = Vector3(vec.X, vec.Y + height, vec.Z);
        }
        Material& material = RequireReference(ManagedAt(RequireReference(model.Materials), 0));
        const float alpha = std::clamp(_lifespan * 30.0F * 8.0F, 0.0F, 31.0F) / 31.0F;
        Scene& scene = RequireReference(_scene);
        scene.AddRenderItem(RenderItemType::TrailMulti, alpha, scene.GetNextPolygonId(), _color,
            material.XRepeat, material.YRepeat, material.ScaleS, material.ScaleT,
            Translation(_pastPositions[0]), uvsAndVerts, _bindingId, count);
    }

    void BeamProjectileEntity::DrawTrail3(float height)
    {
        assert(_trailModel != nullptr);
        Model& model = RequireReference(RequireReference(_trailModel).Model());
        const Recolor& recolor = RequireReference(ManagedAt(RequireReference(model.Recolors), 0));
        const Texture& texture = ManagedAt(RequireReference(recolor.Textures), 0);
        const float uvS2 = (texture.Width - (1.0F / 16.0F)) / texture.Width;
        const float uvT2 = (texture.Height / 4.0F - (1.0F / 16.0F)) / texture.Height;
        auto uvsAndVerts = std::make_shared<ManagedArray<Vector3>>(8);
        (*uvsAndVerts)[0] = Vector3::Zero;
        (*uvsAndVerts)[1] = Vector3(0, -height, 0);
        (*uvsAndVerts)[2] = Vector3(0, uvT2, 0);
        (*uvsAndVerts)[3] = Vector3(0, height, 0);
        (*uvsAndVerts)[4] = Vector3(uvS2, 0, 0);
        (*uvsAndVerts)[5] = Vector3(
            _pastPositions[8].X - _pastPositions[0].X,
            _pastPositions[8].Y - _pastPositions[0].Y - height,
            _pastPositions[8].Z - _pastPositions[0].Z);
        (*uvsAndVerts)[6] = Vector3(uvS2, uvT2, 0);
        (*uvsAndVerts)[7] = Vector3(
            _pastPositions[8].X - _pastPositions[0].X,
            _pastPositions[8].Y - _pastPositions[0].Y + height,
            _pastPositions[8].Z - _pastPositions[0].Z);
        Material& material = RequireReference(ManagedAt(RequireReference(model.Materials), 0));
        const float alpha = std::clamp(_lifespan * 30.0F * 8.0F, 0.0F, 31.0F) / 31.0F;
        Scene& scene = RequireReference(_scene);
        scene.AddRenderItem(RenderItemType::TrailSingle, alpha, scene.GetNextPolygonId(), _color,
            material.XRepeat, material.YRepeat, material.ScaleS, material.ScaleT,
            Translation(_pastPositions[0]), uvsAndVerts, _bindingId);
    }

    void BeamProjectileEntity::DrawTrail4(float height, float range, std::int32_t segments)
    {
        assert(_trailModel != nullptr);
        if (segments < 2)
        {
            return;
        }
        const std::int32_t count = 4 * segments;
        Scene& scene = RequireReference(_scene);
        const std::int32_t frames = ManagedInt32FromUInt64(scene.LiveFrames) / 2;
        const std::int32_t positionSeed = static_cast<std::int32_t>(Position.X * 4096.0F);
        std::uint32_t rng = std::bit_cast<std::uint32_t>(ManagedAdd(frames, positionSeed));
        const std::int32_t index = frames & 15;
        const float halfRange = range / 2.0F;
        const Vector3 vec = static_cast<Vector3>(Position) - _pastPositions[8];
        Model& model = RequireReference(RequireReference(_trailModel).Model());
        const Recolor& recolor = RequireReference(ManagedAt(RequireReference(model.Recolors), 0));
        const Texture& texture = ManagedAt(RequireReference(recolor.Textures), 0);
        const float uvT = (texture.Height - (1.0F / 16.0F)) / texture.Height;
        auto uvsAndVerts = std::make_shared<ManagedArray<Vector3>>(static_cast<std::size_t>(count));
        for (std::int32_t i = 0; i < segments; ++i)
        {
            float uvS = 0.0F;
            const std::int32_t factor = index + i;
            if (factor > 0)
            {
                uvS = (2.0F * texture.Width / static_cast<float>(segments - 1) * factor
                    - (1.0F / 16.0F)) / texture.Width;
            }
            const float pct = static_cast<float>(i) / static_cast<float>(segments - 1);
            float x = vec.X * pct + _velocity.X / 4.0F * pct * (1.0F - pct);
            float y = vec.Y * pct + _velocity.Y / 4.0F * pct * (1.0F - pct);
            float z = vec.Z * pct + _velocity.Z / 4.0F * pct * (1.0F - pct);
            if (i > 0 && i < segments - 1)
            {
                const std::uint32_t randomRange = static_cast<std::uint32_t>(Fixed::ToInt(range));
                x += Rng::CallRng(rng, randomRange) / 4096.0F - halfRange;
                y += Rng::CallRng(rng, randomRange) / 4096.0F - halfRange;
                z += Rng::CallRng(rng, randomRange) / 4096.0F - halfRange;
            }
            (*uvsAndVerts)[static_cast<std::size_t>(4 * i)] = Vector3(uvS, 0, 0);
            (*uvsAndVerts)[static_cast<std::size_t>(4 * i + 1)] = Vector3(x, y - height, z);
            (*uvsAndVerts)[static_cast<std::size_t>(4 * i + 2)] = Vector3(uvS, uvT, 0);
            (*uvsAndVerts)[static_cast<std::size_t>(4 * i + 3)] = Vector3(x, y + height, z);
        }
        Material& material = RequireReference(ManagedAt(RequireReference(model.Materials), 0));
        scene.AddRenderItem(RenderItemType::TrailMulti, 1.0F, scene.GetNextPolygonId(), _color,
            material.XRepeat, material.YRepeat, material.ScaleS, material.ScaleT,
            Translation(_pastPositions[8]), uvsAndVerts, _bindingId, count);
    }

    Matrix4 BeamProjectileEntity::GetModelTransform(ModelInstance& inst, std::int32_t index)
    {
        if (_drawFuncId == 3)
        {
            Matrix4 transform = GetTransformMatrix(_direction, _up);
            SetRow3(transform, Position);
            return transform;
        }
        if (_drawFuncId == 17)
        {
            Matrix4 transform = Transform;
            const float scale = Vector3::Distance(Position, _backPosition);
            ScaleRow2(transform, scale);
            SetRow3(transform, _backPosition);
            return transform;
        }
        return EntityBase::GetModelTransform(inst, index);
    }

    void BeamProjectileEntity::Destroy()
    {
        _soundSource.StopAllSfx();
        _lifespan = 0.0F;
        if (_effect)
        {
            RequireReference(_scene).DetachEffectEntry(_effect, true);
            _effect.reset();
        }
        if (_muzzleEffect)
        {
            if (TestFlag(_flags, BeamFlags::DestroyMuzzle))
            {
                RequireReference(_scene).UnlinkEffectEntry(_muzzleEffect);
            }
            _muzzleEffect.reset();
        }
        _owner.reset();
        _effect.reset();
        _target.reset();
        _ricochetWeapon.reset();
        _equip.reset();
        _trailModel.reset();
        EntityBase::Destroy();
    }

    std::shared_ptr<BeamProjectileEntity> BeamProjectileEntity::ChooseBeamSlot(
        const std::shared_ptr<EquipInfo>& equip,
        const std::shared_ptr<EntityBase>& owner)
    {
        EquipInfo& equipRef = RequireReference(equip);
        WeaponInfo& weapon = RequireReference(equipRef.Weapon());
        auto& beams = equipRef.Beams();
        if (TestFlag(weapon.Flags(), WeaponFlags::Continuous))
        {
            for (std::size_t i = 0; i < beams.size(); ++i)
            {
                const std::shared_ptr<BeamProjectileEntity>& beam = beams[i];
                BeamProjectileEntity& beamRef = RequireReference(beam);
                if (TestFlag(beamRef._flags, BeamFlags::Continuous)
                    && beamRef._beamKind == weapon.BeamKind()
                    && beamRef._owner == owner
                    && beamRef._lifespan < weapon.UnchargedLifespan())
                {
                    return beam;
                }
            }
        }
        for (std::size_t i = 0; i < beams.size(); ++i)
        {
            const std::shared_ptr<BeamProjectileEntity>& beam = beams[i];
            BeamProjectileEntity& beamRef = RequireReference(beam);
            if (beamRef._lifespan <= 0.0F)
            {
                return beam;
            }
            if (TestFlag(beamRef._flags, BeamFlags::Continuous)
                && beamRef._beamKind == weapon.BeamKind()
                && beamRef._owner == owner
                && beamRef._lifespan < weapon.UnchargedLifespan())
            {
                return beam;
            }
        }
        if (beams.empty())
        {
            throw Memory::Detail::IndexOutOfRangeException();
        }
        return beams.back();
    }

    BeamResultFlags BeamProjectileEntity::Spawn(
        std::shared_ptr<EntityBase> owner,
        std::shared_ptr<EquipInfo> equip,
        Vector3 position,
        Vector3 direction,
        BeamSpawnFlags spawnFlags,
        Formats::Culling::NodeRef nodeRef,
        Scene* scene)
    {
        BeamResultFlags result = BeamResultFlags::Spawned;
        EquipInfo& equipRef = RequireReference(equip);
        const std::shared_ptr<WeaponInfo> weaponPtr = equipRef.Weapon();
        WeaponInfo& weapon = RequireReference(weaponPtr);

        bool charged = false;
        float chargePct = 0.0F;
        if (TestFlag(weapon.Flags(), WeaponFlags::CanCharge))
        {
            if (TestFlag(weapon.Flags(), WeaponFlags::PartialCharge))
            {
                if (equipRef.ChargeLevel() >= weapon.MinCharge() * 2)
                {
                    charged = true;
                    chargePct = (equipRef.ChargeLevel() - weapon.MinCharge() * 2)
                        / static_cast<float>(weapon.FullCharge() * 2 - weapon.MinCharge() * 2);
                }
            }
            else if (equipRef.ChargeLevel() >= weapon.FullCharge() * 2)
            {
                charged = true;
                chargePct = 1.0F;
            }
        }

        const auto getAmount = [chargePct](std::int32_t unchargedAmt,
            std::int32_t minChargeAmt, std::int32_t fullChargeAmt) -> float
        {
            return chargePct <= 0.0F
                ? static_cast<float>(unchargedAmt)
                : static_cast<float>(minChargeAmt)
                    + static_cast<float>(ManagedSubtract(fullChargeAmt, minChargeAmt)) * chargePct;
        };

        std::int32_t cost = static_cast<std::int32_t>(
            getAmount(weapon.AmmoCost(), weapon.MinChargeCost(), weapon.ChargeCost()));
        if (TestFlag(weapon.Flags(), WeaponFlags::Continuous))
        {
            if (RequireReference(scene).FrameCount % 2 == 0)
            {
                const std::uint64_t bits = static_cast<std::uint64_t>(cost & 31);
                cost /= 32;
                if (RequireReference(scene).FrameCount % 2 == 0 && bits != 0
                    && ((bits * (RequireReference(scene).FrameCount / 2)) & 31U) > 32U - bits)
                {
                    ++cost;
                }
            }
            else
            {
                cost = 0;
            }
        }
        const std::int32_t ammo = equipRef.Ammo();
        if (ammo >= 0 && cost > ammo)
        {
            return BeamResultFlags::NoSpawn;
        }
        equipRef.SetAmmo(ManagedSubtract(ammo, cost));

        std::shared_ptr<Effects::EffectEntry> muzzleEffect{};
        if (!TestFlag(spawnFlags, BeamSpawnFlags::NoMuzzle))
        {
            const std::uint8_t effectId = ManagedAt(weapon.MuzzleEffects(), charged ? 1U : 0U);
            if (effectId != 255)
            {
                assert(effectId >= 3);
                const Vector3 effUp = direction;
                const Vector3 effFacing = GetCrossVector(effUp);
                Matrix4 transform = GetTransformMatrix(effFacing, effUp);
                SetRow3(transform, position);
                if (TestFlag(spawnFlags, BeamSpawnFlags::DestroyMuzzle))
                {
                    muzzleEffect = RequireReference(scene).SpawnEffectGetEntry(effectId - 3, transform);
                }
                else
                {
                    RequireReference(scene).SpawnEffect(effectId - 3, transform);
                }
            }
        }

        const std::int32_t projectiles = static_cast<std::int32_t>(
            getAmount(weapon.Projectiles(), weapon.MinChargeProjectiles(), weapon.ChargedProjectiles()));
        if (projectiles <= 0)
        {
            return result;
        }

        const bool instantAoe = (charged && TestFlag(weapon.Flags(), WeaponFlags::AoeCharged))
            || (!charged && TestFlag(weapon.Flags(), WeaponFlags::AoeUncharged));

        BeamFlags flags = BeamFlags::None;
        const float speed = getAmount(
            weapon.UnchargedSpeed(), weapon.MinChargeSpeed(), weapon.ChargedSpeed()) / 4096.0F / 2.0F;
        const float finalSpeed = getAmount(
            weapon.UnchargedFinalSpeed(), weapon.MinChargeFinalSpeed(), weapon.ChargedFinalSpeed())
            / 4096.0F / 2.0F;
        const float speedDecayTime = ManagedAt(weapon.SpeedDecayTimes(), charged ? 1U : 0U) * (1.0F / 30.0F);
        const std::uint16_t speedInterpolation = ManagedAt(weapon.SpeedInterpolations(), charged ? 1U : 0U);
        const float gravity = getAmount(
            weapon.UnchargedGravity(), weapon.MinChargeGravity(), weapon.ChargedGravity()) / 4096.0F;
        const Vector3 acceleration(0.0F, gravity / 2.0F, 0.0F);
        const float homing = getAmount(
            weapon.UnchargedHoming(), weapon.MinChargeHoming(), weapon.ChargedHoming()) / 4096.0F / 2.0F;
        if (homing > 0.0F)
        {
            flags |= BeamFlags::Homing;
        }
        if (charged || TestFlag(spawnFlags, BeamSpawnFlags::Charged))
        {
            flags |= BeamFlags::Charged;
        }
        if ((charged && TestFlag(weapon.Flags(), WeaponFlags::RicochetCharged))
            || (!charged && TestFlag(weapon.Flags(), WeaponFlags::RicochetUncharged)))
        {
            flags |= BeamFlags::Ricochet;
        }
        if ((charged && TestFlag(weapon.Flags(), WeaponFlags::SelfDamageCharged))
            || (!charged && TestFlag(weapon.Flags(), WeaponFlags::SelfDamageUncharged)))
        {
            flags |= BeamFlags::SelfDamage;
        }
        if ((charged && TestFlag(weapon.Flags(), WeaponFlags::ForceEffectCharged))
            || (!charged && TestFlag(weapon.Flags(), WeaponFlags::ForceEffectUncharged)))
        {
            flags |= BeamFlags::ForceEffect;
        }
        if ((charged && TestFlag(weapon.Flags(), WeaponFlags::DestroyableCharged))
            || (!charged && TestFlag(weapon.Flags(), WeaponFlags::DestroyableUncharged)))
        {
            flags |= BeamFlags::Destroyable;
        }
        if ((charged && TestFlag(weapon.Flags(), WeaponFlags::LifeDrainCharged))
            || (!charged && TestFlag(weapon.Flags(), WeaponFlags::LifeDrainUncharged)))
        {
            flags |= BeamFlags::LifeDrain;
        }

        std::uint8_t drawFuncId = ManagedAt(equipRef.DrawFuncIds(), charged ? 1U : 0U);
        if (drawFuncId == 255)
        {
            drawFuncId = ManagedAt(weapon.DrawFuncIds(), charged ? 1U : 0U);
        }
        const std::uint16_t colorValue = ManagedAt(weapon.Colors(), charged ? 1U : 0U);
        const float red = ((colorValue >> 0) & 0x1FU) / 31.0F;
        const float green = ((colorValue >> 5) & 0x1FU) / 31.0F;
        const float blue = ((colorValue >> 10) & 0x1FU) / 31.0F;
        const Vector3 color(red, green, blue);
        const std::uint8_t colEffect = ManagedAt(weapon.CollisionEffects(), charged ? 1U : 0U);
        std::uint8_t dmgDirType = ManagedAt(equipRef.DmgDirTypes(), charged ? 1U : 0U);
        if (dmgDirType == 255)
        {
            dmgDirType = ManagedAt(weapon.DmgDirTypes(), charged ? 1U : 0U);
        }
        const float dmgDirMag = getAmount(
            weapon.UnchargedDmgDirMag(), weapon.MinChargeDmgDirMag(), weapon.ChargedDmgDirMag()) / 4096.0F;
        std::int32_t damage = static_cast<std::int32_t>(getAmount(
            equipRef.UnchargedDamage(), equipRef.MinChargeDamage(), equipRef.ChargedDamage()));
        std::int32_t hsDamage = static_cast<std::int32_t>(getAmount(
            equipRef.HeadshotDamage(), equipRef.MinChargeHeadshotDamage(), equipRef.ChargedHeadshotDamage()));
        std::int32_t splashDmg = static_cast<std::int32_t>(getAmount(
            equipRef.SplashDamage(), equipRef.MinChargeSplashDamage(), equipRef.ChargedSplashDamage()));
        const float splashRadius = getAmount(
            weapon.UnchargedSplashRadius(), weapon.MinChargeSplashRadius(), weapon.ChargedSplashRadius()) / 4096.0F;
        const std::uint8_t splashDmgType = ManagedAt(weapon.SplashDamageTypes(), charged ? 1U : 0U);
        if (TestFlag(spawnFlags, BeamSpawnFlags::DoubleDamage))
        {
            damage *= 2;
            hsDamage *= 2;
            splashDmg *= 2;
        }
        else if (TestFlag(spawnFlags, BeamSpawnFlags::PrimeHunter))
        {
            damage = 150 * damage / 100;
            hsDamage = 150 * hsDamage / 100;
            splashDmg = 150 * splashDmg / 100;
        }
        if (Features::HalfDamageUnscoped && weapon.Beam() == BeamType::Imperialist && !equipRef.Zoomed())
        {
            damage /= 2;
            hsDamage /= 2;
            splashDmg /= 2;
        }
        if (TestFlag(weapon.Flags(), WeaponFlags::Continuous))
        {
            if (RequireReference(scene).FrameCount % 2 == 0)
            {
                const std::uint64_t bits = static_cast<std::uint64_t>(damage & 31);
                damage /= 32;
                if (bits != 0 && ((bits * (RequireReference(scene).FrameCount / 2)) & 31U) >= 32U - bits)
                {
                    ++damage;
                }
            }
            else
            {
                damage = 0;
            }
        }
        if (Cheats::QuadrupleDamage)
        {
            damage *= 4;
            hsDamage *= 4;
            splashDmg *= 4;
        }

        const std::uint16_t damageInterpolation = ManagedAt(weapon.DamageInterpolations(), charged ? 1U : 0U);
        const float maxDist = getAmount(
            weapon.UnchargedDistance(), weapon.MinChargeDistance(), weapon.ChargedDistance()) / 4096.0F;
        const Affliction afflictions = ManagedAt(weapon.Afflictions(), charged ? 1U : 0U);
        const float cylinderRadius = getAmount(
            weapon.UnchargedCylRadius(), weapon.MinChargeCylRadius(), weapon.ChargedCylRadius()) / 4096.0F;
        const float lifespan = getAmount(
            weapon.UnchargedLifespan(), weapon.MinChargeLifespan(), weapon.ChargedLifespan()) * (1.0F / 30.0F);
        if (TestFlag(weapon.Flags(), WeaponFlags::Continuous))
        {
            flags |= BeamFlags::Continuous;
        }
        if (TestFlag(weapon.Flags(), WeaponFlags::SurfaceCollision))
        {
            flags |= BeamFlags::SurfaceCollision;
        }
        const std::uint32_t radiusIndex
            = (static_cast<std::uint32_t>(weapon.Flags()) >> (charged ? 26U : 24U)) & 3U;
        flags = static_cast<BeamFlags>(
            static_cast<std::uint16_t>(flags) | static_cast<std::uint16_t>(radiusIndex << 9U));
        const float ricochetLossH = getAmount(
            weapon.UnchargedRicochetLossH(), weapon.MinChargeRicochetLossH(), weapon.ChargedRicochetLossH()) / 4096.0F;
        const float ricochetLossV = getAmount(
            weapon.UnchargedRicochetLossV(), weapon.MinChargeRicochetLossV(), weapon.ChargedRicochetLossV()) / 4096.0F;
        const std::int32_t maxSpread = static_cast<std::int32_t>(getAmount(
            weapon.UnchargedSpread(), weapon.MinChargeSpread(), weapon.ChargedSpread()));
        const std::shared_ptr<WeaponInfo> ricochetWeapon
            = charged ? weapon.ChargedRicochetWeapon() : weapon.UnchargedRicochetWeapon();

        const Vector3 dirVec = direction;
        Vector3 rightVec{};
        if (dirVec.X != 0.0F || dirVec.Z != 0.0F)
        {
            rightVec = Normalize(Vector3(dirVec.Z, 0.0F, -dirVec.X));
        }
        else
        {
            rightVec = Vector3(1.0F, 0.0F, 0.0F);
        }
        const Vector3 upVec = Normalize(Vector3::Cross(dirVec, rightVec));
        Vector3 velocity = Vector3::Zero;
        if (maxSpread <= 0)
        {
            velocity = Scale(direction, speed);
        }

        for (std::int32_t i = 0; i < projectiles; ++i)
        {
            const std::shared_ptr<BeamProjectileEntity> beam = ChooseBeamSlot(equip, owner);
            BeamProjectileEntity& beamRef = RequireReference(beam);
            if (beamRef._lifespan > 0.0F && !TestFlag(beamRef._flags, BeamFlags::Collided))
            {
                Formats::CollisionResult colRes{};
                colRes.Position = beamRef.Position;
                colRes.Plane = NegatePlane(beamRef._direction);
                beamRef._ricochetWeapon.reset();
                beamRef.OnCollision(colRes, nullptr);
            }
            beamRef.Destroy();
            RequireReference(scene).RemoveEntity(beam);
            beamRef._models = ModelList{};

            if (!charged)
            {
                std::uint16_t smoke = static_cast<std::uint16_t>(
                    static_cast<std::uint32_t>(equipRef.SmokeLevel())
                    + static_cast<std::uint32_t>(weapon.SmokeShotAmount()));
                equipRef.SetSmokeLevel(smoke);
                if (equipRef.SmokeLevel() > weapon.SmokeStart())
                {
                    equipRef.SetSmokeLevel(weapon.SmokeStart());
                }
            }

            beamRef._owner = owner;
            beamRef._beam = weapon.Beam();
            beamRef._beamKind = weapon.BeamKind();
            beamRef._flags = flags;
            beamRef.NodeRef = nodeRef;
            beamRef._age = 0.0F;
            beamRef._initialSpeed = beamRef._speed = speed;
            beamRef._finalSpeed = finalSpeed;
            beamRef._speedDecayTime = speedDecayTime;
            beamRef._speedInterpolation = speedInterpolation;
            beamRef._homing = homing;
            beamRef._drawFuncId = drawFuncId;
            beamRef._color = color;
            beamRef._collisionEffect = colEffect;
            beamRef._damageDirType = dmgDirType;
            beamRef._splashDamageType = splashDmgType;
            beamRef._damageDirMag = dmgDirMag;
            beamRef._spawnPosition = beamRef._backPosition = position;
            beamRef.Position = position;
            for (std::int32_t j = 0; j < 10; ++j)
            {
                beamRef._pastPositions[static_cast<std::size_t>(j)] = position;
            }
            beamRef._direction = dirVec;
            beamRef._right = rightVec;
            beamRef._up = upVec;
            beamRef._damage = static_cast<float>(damage);
            beamRef._headshotDamage = static_cast<float>(hsDamage);
            beamRef._splashDamage = static_cast<float>(splashDmg);
            beamRef._splashRadius = splashRadius;
            beamRef._damageInterpolation = damageInterpolation;
            beamRef._maxDistance = maxDist;
            beamRef._afflictions = afflictions;
            beamRef._cylinderRadius = cylinderRadius;
            beamRef._lifespan = lifespan;
            beamRef._ricochetLossH = ricochetLossH;
            beamRef._ricochetLossV = ricochetLossV;
            beamRef._ricochetWeapon = ricochetWeapon;
            beamRef._equip = equip;

            if (RequireReference(owner).Type == EntityType::Player)
            {
                PlayerEntity* ownerPlayer = static_cast<PlayerEntity*>(owner.get());
                const std::size_t slotIndex = Index(ownerPlayer->SlotIndex());
                std::int32_t& beamDamageMax = ManagedAt(GameState::BeamDamageMax, slotIndex);
                beamDamageMax = ManagedAdd(beamDamageMax, damage);
            }

            if (instantAoe)
            {
                beamRef.SpawnIceWave(weaponPtr, chargePct);
                beamRef._velocity = Vector3::Zero;
                beamRef._acceleration = Vector3::Zero;
                beamRef._flags = BeamFlags::Collided;
                beamRef._lifespan = 0.0F;
                beamRef.Destroy();
                return result;
            }

            if (maxSpread > 0)
            {
                const float angle1 = DegreesToRadians(Rng::GetRandomInt2(
                    static_cast<std::uint32_t>(maxSpread)) / 4096.0F);
                const float angle2 = DegreesToRadians(Rng::GetRandomInt2(0x168000U) / 4096.0F);
                const float sin1 = std::sin(angle1);
                const float cos1 = std::cos(angle1);
                const float sin2 = std::sin(angle2);
                const float cos2 = std::cos(angle2);
                velocity.X = direction.X * cos1 + (beamRef._up.X * cos2 + beamRef._right.X * sin2) * sin1;
                velocity.Y = direction.Y * cos1 + (beamRef._up.Y * cos2 + beamRef._right.Y * sin2) * sin1;
                velocity.Z = direction.Z * cos1 + (beamRef._up.Z * cos2 + beamRef._right.Z * sin2) * sin1;
                velocity = Scale(velocity, beamRef._speed);
            }
            beamRef._velocity = velocity;
            beamRef._acceleration = acceleration;

            if (beamRef._drawFuncId == 3)
            {
                beamRef._flags |= BeamFlags::HasModel;
                beamRef._models.Add(Read::GetModelInstance("iceShard"));
            }
            else if (beamRef._drawFuncId == 17)
            {
                beamRef._flags |= BeamFlags::HasModel;
                const std::shared_ptr<ModelInstance> model = Read::GetModelInstance("energyBeam");
                RequireReference(model).SetAnimation(0);
                beamRef._models.Add(model);
                Matrix4 transform = GetTransformMatrix(beamRef._direction, beamRef._up);
                SetRow3(transform, position);
                beamRef.Transform = transform;
                AnimationInfo& animInfo = RequireReference(RequireReference(model).AnimInfo);
                const std::int32_t frameCount = (*animInfo.FrameCount)[0];
                (*animInfo.Frame)[0] = ManagedInt32FromUInt64(RequireReference(scene).FrameCount) / 2 % frameCount;
            }
            else
            {
                const std::int32_t effectId = ManagedAt(Metadata::BeamDrawEffects, Index(beamRef._drawFuncId));
                if (effectId != 0)
                {
                    const Vector3 effUp = beamRef._direction;
                    const Vector3 effFacing = GetCrossVector(effUp);
                    Matrix4 transform = GetTransformMatrix(effFacing, effUp);
                    SetRow3(transform, beamRef.Position);
                    beamRef._effect = RequireReference(scene).SpawnEffectGetEntry(effectId, transform);
                    if (beamRef._effect)
                    {
                        beamRef._effect->SetElementExtension(true);
                    }
                }
            }

            if (TestFlag(spawnFlags, BeamSpawnFlags::DestroyMuzzle))
            {
                beamRef._muzzleEffect = muzzleEffect;
                beamRef._flags |= BeamFlags::DestroyMuzzle;
            }

            assert(!beamRef._target);
            if (TestFlag(beamRef._flags, BeamFlags::Homing))
            {
                if (CheckHomingTargets(beam, equip, scene))
                {
                    result |= BeamResultFlags::Homing;
                }
                if (beamRef._beam == BeamType::ShockCoil && RequireReference(owner).Type == EntityType::Player)
                {
                    PlayerEntity* ownerPlayer = static_cast<PlayerEntity*>(owner.get());
                    if ((GameState::Multiplayer || !ownerPlayer->IsBot())
                        && ownerPlayer->ShockCoilTarget() == beamRef._target
                        && RequireReference(scene).FrameCount % 2 == 0)
                    {
                        const std::uint16_t timer = ownerPlayer->ShockCoilTimer();
                        if (timer >= 120 * 2)
                        {
                            beamRef._damage += 4.0F;
                            beamRef._splashDamage += 4.0F;
                            beamRef._headshotDamage += 4.0F;
                        }
                        else
                        {
                            const std::uint16_t increment = timer / (30 * 2);
                            beamRef._damage += increment;
                            beamRef._splashDamage += increment;
                            beamRef._headshotDamage += increment;
                        }
                    }
                }
            }

            if (beamRef._beam == BeamType::ShockCoil && RequireReference(owner).Type == EntityType::Player)
            {
                ++Mods::Network::NetDamage::ShockCoilSpawned;
                if (beamRef._target)
                {
                    ++Mods::Network::NetDamage::ShockCoilAcquired;
                }
            }
            beamRef._soundSource.Update(beamRef.Position, 0);
            RequireReference(scene).AddEntity(beam);
        }
        return result;
    }

    bool BeamProjectileEntity::CheckHomingTargets(
        const std::shared_ptr<BeamProjectileEntity>& beam,
        const std::shared_ptr<EquipInfo>& equip,
        Scene* scene)
    {
        bool result = false;
        EquipInfo& equipRef = RequireReference(equip);
        const std::shared_ptr<WeaponInfo> weapon = equipRef.Weapon();
        BeamProjectileEntity& beamRef = RequireReference(beam);
        assert(beamRef._owner != nullptr);
        const float tolerance = Fixed::ToFloat(equipRef.HomingTolerance());
        float curDiv = tolerance;

        for (std::size_t i = 0; i < _homingTargetTypes.size(); ++i)
        {
            const EntityType type = _homingTargetTypes[i];
            if (type == EntityType::EnemyInstance
                && (RequireReference(beamRef._owner).Type == EntityType::EnemyInstance
                    || RequireReference(beamRef._owner).Type == EntityType::Platform))
            {
                continue;
            }

            auto enumerator = RequireReference(scene).Entities().GetEnumerator();
            while (enumerator.MoveNext())
            {
                const std::shared_ptr<EntityBase> entityPtr = enumerator.Current();
                EntityBase& entity = RequireReference(entityPtr);
                if (entity.Type != type || entityPtr == beamRef._owner || !entity.GetTargetable())
                {
                    continue;
                }

                bool tryTarget = false;
                if (type == EntityType::Player)
                {
                    PlayerEntity& player = static_cast<PlayerEntity&>(entity);
                    if (RequireReference(beamRef._owner).Type != EntityType::Player)
                    {
                        tryTarget = true;
                    }
                    else
                    {
                        PlayerEntity& ownerPlayer
                            = static_cast<PlayerEntity&>(RequireReference(beamRef._owner));
                        tryTarget = player.TeamIndex() != ownerPlayer.TeamIndex();
                    }
                }
                else if (type == EntityType::Halfturret)
                {
                    HalfturretEntity& halfturret = static_cast<HalfturretEntity&>(entity);
                    if (RequireReference(beamRef._owner).Type != EntityType::Player)
                    {
                        tryTarget = true;
                    }
                    else
                    {
                        PlayerEntity& ownerPlayer
                            = static_cast<PlayerEntity&>(RequireReference(beamRef._owner));
                        tryTarget = RequireReference(halfturret.Owner()).TeamIndex() != ownerPlayer.TeamIndex();
                    }
                }
                else if (type == EntityType::EnemyInstance)
                {
                    EnemyInstanceEntity& enemy = static_cast<EnemyInstanceEntity&>(entity);
                    const EnemyFlags enemyFlags = enemy.Flags();
                    if (TestFlag(enemyFlags, EnemyFlags::CollideBeam)
                        && (!TestFlag(enemyFlags, EnemyFlags::NoHomingNc)
                            || TestFlag(beamRef._flags, BeamFlags::Continuous))
                        && (!TestFlag(enemyFlags, EnemyFlags::NoHomingCo)
                            || !TestFlag(beamRef._flags, BeamFlags::Continuous)))
                    {
                        tryTarget = true;
                    }
                }
                else if (type == EntityType::Platform)
                {
                    PlatformEntity& platform = static_cast<PlatformEntity&>(entity);
                    tryTarget = TestFlag(platform.Flags(), PlatformFlags::BeamTarget);
                }
                else
                {
                    tryTarget = true;
                }

                if (tryTarget)
                {
                    Vector3 targetPosition{};
                    entity.GetPosition(targetPosition);
                    const Vector3 between = targetPosition - static_cast<Vector3>(beamRef.Position);
                    const float distSqr = Vector3::Dot(between, between);
                    WeaponInfo& weaponRef = RequireReference(weapon);
                    const float range = Fixed::ToFloat(weaponRef.HomingRange());
                    if ((TestFlag(weaponRef.Flags(), WeaponFlags::Continuous)
                            && beamRef._beamKind != BeamType::Platform
                        || distSqr <= range * range)
                        && distSqr > 0.0F)
                    {
                        const float dist = std::sqrt(distSqr);
                        assert(!IsZero(beamRef._velocity));
                        const float dot = Vector3::Dot(between, Normalize(beamRef._velocity));
                        const float div1 = dot / dist;
                        if (div1 >= curDiv)
                        {
                            if (TestFlag(weaponRef.Flags(), WeaponFlags::Continuous))
                            {
                                bool canTarget = false;
                                if (type == EntityType::Player)
                                {
                                    PlayerEntity& player = static_cast<PlayerEntity&>(entity);
                                    if ((!player.IsAltForm() && !player.IsMorphing())
                                        || div1 >= Fixed::ToFloat(4006))
                                    {
                                        canTarget = true;
                                    }
                                }
                                else
                                {
                                    canTarget = true;
                                }
                                if (canTarget)
                                {
                                    const float div2 = std::min(dist / range, 1.0F);
                                    if (div1 >= tolerance + div2 * (Fixed::ToFloat(4094) - tolerance))
                                    {
                                        curDiv = div1;
                                        beamRef._target = entityPtr;
                                        result = true;
                                    }
                                }
                            }
                            else
                            {
                                curDiv = div1;
                                beamRef._target = entityPtr;
                            }
                        }
                    }
                }
            }
        }
        return result;
    }

    void BeamProjectileEntity::SpawnIceWave(
        const std::shared_ptr<WeaponInfo>& weaponPtr, float chargePct)
    {
        WeaponInfo& weapon = RequireReference(weaponPtr);
        float angle = chargePct <= 0.0F
            ? static_cast<float>(weapon.UnchargedSpread())
            : static_cast<float>(weapon.MinChargeSpread())
                + static_cast<float>(ManagedSubtract(
                    weapon.ChargedSpread(), weapon.MinChargeSpread())) * chargePct;
        angle /= 4096.0F;
        assert(angle == 60.0F);
        CheckIceWaveCollision(angle);

        const Vector3 up = _direction;
        Vector3 facing{};
        if (up.X != 0.0F || up.Z != 0.0F)
        {
            const Vector3 temp = Vector3::Cross(Vector3(0, 1, 0), up);
            facing = Normalize(Vector3::Cross(up, temp));
        }
        else
        {
            const Vector3 temp = Vector3::Cross(Vector3(1, 0, 0), up);
            facing = Normalize(Vector3::Cross(up, temp));
        }
        Matrix4 transform = Matrix::Multiply44(ScaleMatrix(_maxDistance), GetTransformMatrix(facing, up));
        SetRow3(transform, Position);
        const std::shared_ptr<BeamEffectEntity> ent = BeamEffectEntity::Create(
            BeamEffectEntityData(0, false, transform), _scene);
        if (ent)
        {
            RequireReference(_scene).AddEntity(ent);
        }
    }

    void BeamProjectileEntity::CheckIceWaveCollision(float angle)
    {
        const float angleCos = std::cos(DegreesToRadians(angle));
        auto enumerator = RequireReference(_scene).GetPlayerEntities().GetEnumerator();
        while (enumerator.MoveNext())
        {
            const std::shared_ptr<PlayerEntity> playerPtr = enumerator.Current();
            PlayerEntity& player = RequireReference(playerPtr);
            if (playerPtr == std::dynamic_pointer_cast<PlayerEntity>(_owner) || player.Health() == 0)
            {
                continue;
            }
            CheckIceWaveCollision(player, player.Position, angleCos, false);
            if (TestFlag(player.Flags2(), PlayerFlags2::Halfturret))
            {
                CheckIceWaveCollision(player, RequireReference(player.Halfturret()).Position, angleCos, true);
            }
        }
    }

    void BeamProjectileEntity::CheckIceWaveCollision(
        PlayerEntity& player, Vector3 position, float angleCos, bool halfturret)
    {
        const Vector3 full = position - static_cast<Vector3>(Position);
        Vector3 between = full;
        const float dot = Vector3::Dot(between, _up);
        between = between + Scale(_up, -dot);
        const float mag = Length(between);
        const float reach = GameState::ShadowFreeze ? mag : Length(full);
        if (reach < _maxDistance && reach > 0.0F)
        {
            const Vector3 toward = GameState::ShadowFreeze ? Divide(between, mag) : Divide(full, reach);
            if (Vector3::Dot(toward, _direction) > angleCos)
            {
                const Vector3 dir = GetDamageDirection(Position, player.Position);
                DamageFlags flags = DamageFlags::NoDmgInvuln;
                if (halfturret)
                {
                    flags = static_cast<DamageFlags>(
                        static_cast<std::underlying_type_t<DamageFlags>>(flags)
                        | static_cast<std::underlying_type_t<DamageFlags>>(DamageFlags::Halfturret));
                }
                player.TakeDamage(static_cast<std::int32_t>(_damage), flags, dir, this);
            }
        }
    }

    Vector3 BeamProjectileEntity::GetDamageDirection(Vector3 beamPos, Vector3 targetPos) const
    {
        if (_damageDirType == 1)
        {
            Vector3 direction(0, 1, 0);
            if (!IsZero(_velocity))
            {
                direction = Normalize(_velocity);
            }
            return Scale(direction, _damageDirMag);
        }
        if (_damageDirType == 2)
        {
            Vector3 direction = targetPos - beamPos;
            if (!IsZero(direction))
            {
                direction = Normalize(direction);
                direction.Y /= 2.0F;
                if (direction.Y < 0.03F)
                {
                    direction.Y = 0.03F;
                }
            }
            else
            {
                direction = Vector3(0, 0.03F, 0);
            }
            return Scale(direction, _damageDirMag);
        }
        if (_damageDirType == 3)
        {
            Vector3 direction = WithY(targetPos - beamPos, 0.0F);
            if (!IsZero(direction))
            {
                direction = Scale(Normalize(direction), _damageDirMag);
            }
            return direction;
        }
        if (_damageDirType == 4)
        {
            return Vector3(0, _damageDirMag, 0);
        }
        return Vector3::Zero;
    }

    void BeamProjectileEntity::SpawnCollisionEffect(Formats::CollisionResult colRes, bool noSplat)
    {
        if (_collisionEffect != 255)
        {
            if (PlayerEntity::PlayerCount > 2 && _collisionEffect == 4)
            {
                noSplat = true;
            }
            Vector3 spawnPos(
                colRes.Position.X + colRes.Plane.X / 8.0F,
                colRes.Position.Y + colRes.Plane.Y / 8.0F,
                colRes.Position.Z + colRes.Plane.Z / 8.0F);
            Vector3 up = _beam == BeamType::Imperialist ? Negate(_direction) : colRes.Plane.Xyz();
            if (colRes.EntityCollision)
            {
                spawnPos = Matrix::Vec3MultMtx4(spawnPos, colRes.EntityCollision->Inverse1);
                up = Matrix::Vec3MultMtx3(up, colRes.EntityCollision->Inverse1);
            }
            const Vector3 facing = GetCrossVector(up);
            Matrix4 transform = GetTransformMatrix(facing, up);
            SetRow3(transform, spawnPos);
            if (!GameState::SinglePlayer || colRes.Terrain() <= Terrain::Lava)
            {
                const std::shared_ptr<BeamEffectEntity> ent = BeamEffectEntity::Create(
                    BeamEffectEntityData(_collisionEffect, noSplat, transform, colRes.EntityCollision), _scene);
                if (ent)
                {
                    if (_splashDamage > 0.0F)
                    {
                        ent->Scale = Vector3(_splashRadius, _splashRadius, _splashRadius);
                    }
                    RequireReference(_scene).AddEntity(ent);
                }
            }
            const auto& splatRow = ManagedListAt(_terSplat1P, static_cast<std::int32_t>(_beamKind));
            const std::uint8_t splatEffect
                = ManagedListAt(splatRow, static_cast<std::int32_t>(colRes.Terrain()));
            if (GameState::SinglePlayer && splatEffect != 255)
            {
                const std::uint8_t adjusted = static_cast<std::uint8_t>(splatEffect + 3);
                const std::shared_ptr<BeamEffectEntity> ent = BeamEffectEntity::Create(
                    BeamEffectEntityData(adjusted, noSplat, transform, colRes.EntityCollision), _scene);
                if (ent)
                {
                    RequireReference(_scene).AddEntity(ent);
                }
            }
        }
    }

    void BeamProjectileEntity::SpawnDamageEffect(Effectiveness effectiveness)
    {
        if (effectiveness != Effectiveness::Normal && effectiveness != Effectiveness::Double)
        {
            return;
        }
        std::int32_t effectId = 0;
        Matrix4 transform = GetTransformMatrix(Vector3(1, 0, 0), Vector3(0, 1, 0));
        SetRow3(transform, Position);
        if (effectiveness == Effectiveness::Double)
        {
            effectId = static_cast<std::int32_t>(_beam) + 20;
        }
        else if (GameState::SinglePlayer)
        {
            effectId = static_cast<std::int32_t>(_beam) + 12;
        }
        else
        {
            effectId = static_cast<std::int32_t>(_beam) + 154;
        }
        if (effectId > 0)
        {
            RequireReference(_scene).SpawnEffect(effectId, transform);
        }
    }

    void BeamProjectileEntity::SpawnSniperBeam()
    {
        const Vector3 spawnPos = _pastPositions[8];
        Vector3 up = static_cast<Vector3>(Position) - spawnPos;
        const float magnitude = Length(up);
        if (magnitude > 0.0F)
        {
            up = Normalize(up);
            const Vector3 facing = GetCrossVector(up);
            Matrix4 transform = GetTransformMatrix(facing, up);
            SetRow3(transform, spawnPos);
            const std::shared_ptr<BeamEffectEntity> ent = BeamEffectEntity::Create(
                BeamEffectEntityData(1, false, transform), _scene);
            if (ent)
            {
                ent->Scale = Vector3(1.0F, magnitude, 1.0F);
                RequireReference(_scene).AddEntity(ent);
            }
        }
    }

    Vector3 BeamProjectileEntity::GetCrossVector(Vector3 up)
    {
        if (up.Z <= Fixed::ToFloat(-3686) || up.Z >= Fixed::ToFloat(3686))
        {
            return Normalize(Vector3::Cross(Vector3(1, 0, 0), up));
        }
        return Normalize(Vector3::Cross(Vector3(0, 0, 1), up));
    }
}
