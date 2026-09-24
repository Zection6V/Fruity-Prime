#include "33_GoreaMeteor.hpp"

#include "../../Formats/CollisionDetection.hpp"
#include "../../Metadata/Enemies.hpp"
#include "../../Read.hpp"
#include "../../Scene.hpp"
#include "../../Utility/Rng.hpp"
#include "../ItemInstanceEntity.hpp"
#include "../Players/PlayerEntity.hpp"
#include "../../NativeRuntime/System/Managed.hpp"
#include "../../Formats/Types.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <limits>
#include <optional>
#include <utility>
#include <vector>

using ::MphRead::NativeRuntime::RequireReference;
using ::OpenTK::Mathematics::Length;
using ::OpenTK::Mathematics::LengthSquared;
using ::OpenTK::Mathematics::MathHelper::DegreesToRadians;

namespace MphRead::Entities::Enemies
{
    namespace
    {
        using OpenTK::Mathematics::Matrix4;
        using OpenTK::Mathematics::Vector3;
        using OpenTK::Mathematics::Vector4;

        [[nodiscard]] Enemy33Entity& RequireEnemy(Enemy33Entity* enemy)
        {
            return RequireReference(enemy);
        }

        [[nodiscard]] Vector3 ScaleVector(Vector3 value, float scale) noexcept
        {
            return Vector3(value.X * scale, value.Y * scale, value.Z * scale);
        }

        [[nodiscard]] Vector3 DivideVector(Vector3 value, float divisor) noexcept
        {
            return Vector3(value.X / divisor, value.Y / divisor, value.Z / divisor);
        }

        [[nodiscard]] Vector3 WithY(Vector3 value, float y) noexcept
        {
            value.Y = y;
            return value;
        }

        [[nodiscard]] Matrix4 CreateFromAxisAngle(Vector3 axis, float angle) noexcept
        {
            axis = axis.Normalized();
            const float axisX = axis.X;
            const float axisY = axis.Y;
            const float axisZ = axis.Z;

            const float cosine = std::cos(-angle);
            const float sine = std::sin(-angle);
            const float t = 1.0F - cosine;

            const float tXX = t * axisX * axisX;
            const float tXY = t * axisX * axisY;
            const float tXZ = t * axisX * axisZ;
            const float tYY = t * axisY * axisY;
            const float tYZ = t * axisY * axisZ;
            const float tZZ = t * axisZ * axisZ;

            const float sinX = sine * axisX;
            const float sinY = sine * axisY;
            const float sinZ = sine * axisZ;

            return Matrix4(
                Vector4(tXX + cosine, tXY - sinZ, tXZ + sinY, 0.0F),
                Vector4(tXY + sinZ, tYY + cosine, tYZ - sinX, 0.0F),
                Vector4(tXZ - sinY, tYZ + sinX, tZZ + cosine, 0.0F),
                Vector4(0.0F, 0.0F, 0.0F, 1.0F));
        }

        [[nodiscard]] std::int32_t ManagedAdd(
            std::int32_t left, std::int32_t right) noexcept
        {
            const std::uint32_t value = std::bit_cast<std::uint32_t>(left)
                + std::bit_cast<std::uint32_t>(right);
            return std::bit_cast<std::int32_t>(value);
        }

        [[nodiscard]] std::int32_t FloatToInt32(float value) noexcept
        {
            if (!std::isfinite(value)
                || value >= 2147483648.0F
                || value < -2147483648.0F)
            {
                return std::numeric_limits<std::int32_t>::min();
            }
            return static_cast<std::int32_t>(value);
        }

        [[nodiscard]] bool HitPlayerAt(
            const std::array<bool, 8>& hitPlayers, std::int32_t index)
        {
            if (index < 0
                || static_cast<std::size_t>(index) >= hitPlayers.size())
            {
                throw SceneDetail::IndexOutOfRangeException();
            }
            return hitPlayers[static_cast<std::size_t>(index)];
        }
    }

    Enemy33Entity::Enemy33Entity(EnemyInstanceEntityData data,
        Formats::Culling::NodeRef nodeRef, Scene* scene)
        : GoreaEnemyEntityBase(data, nodeRef, scene)
    {
        auto processes = std::make_shared<ManagedArray<std::function<void()>>>(4);
        (*processes)[0] = [this]() { State00(); };
        (*processes)[1] = [this]() { State01(); };
        (*processes)[2] = [this]() { State02(); };
        (*processes)[3] = [this]() { State03(); };
        _stateProcesses = std::move(processes);
    }

    void Enemy33Entity::EnemyInitialize()
    {
        if (Enemy31Entity* owner = dynamic_cast<Enemy31Entity*>(_owner))
        {
            Flags |= EnemyFlags::Visible;
            Flags |= EnemyFlags::NoHomingNc;
            Flags &= ~EnemyFlags::Invincible;
            Flags |= EnemyFlags::CollidePlayer;
            Flags |= EnemyFlags::CollideBeam;
            Flags |= EnemyFlags::NoMaxDistance;
            Flags |= EnemyFlags::OnRadar;

            std::shared_ptr<ModelInstance> model
                = Read::GetModelInstance("goreaMeteor");
            _model = model.get();
            _models.Add(std::move(model));

            SetTransform(owner->FacingVector(), owner->UpVector(), owner->Position);
            const Vector3 position = Position;
            _prevPos = position;
            _basePos = position;
            _boundingRadius = 1.0F;
            _hurtVolumeInit = CollisionVolume(Vector3::Zero, 1.0F);
            _healthMax = 8;
            _health = _healthMax;
            _effectUp = Vector3(0.0F, 1.0F, 0.0F);
            _effectFacing = Vector3(0.0F, 0.0F, 1.0F);
            _effect = SpawnEffectGetEntry(
                79, Position, _effectFacing, _effectUp, true);
            _field1A0 = 2.0F;
            _field1A4 = 0.125F;
            _field1AC = 0.0F;
            _field1B0 = 1.0F;
            _field1B4 = 15;
            _field1B6 = 390 * 2;
            _field1B8 = 150 * 2;
            _field1BC = static_cast<float>(12 / 2);
            _itemChance1 = 40;
            _itemChance2 = 0;
            _itemChance3 = 0;
            _itemChance4 = 60;
            _field1C4 = static_cast<std::uint8_t>(5 * 2);
            _target = PlayerEntity::Main().get();
        }
    }

    void Enemy33Entity::InitializePosition(Vector3 position)
    {
        Position = position;
        _prevPos = position;
        _basePos = position;
    }

    void Enemy33Entity::EnemyProcess()
    {
        if (!TypeExtensions::TestFlag(
            static_cast<EnemyFlags>(Flags), EnemyFlags::Visible))
        {
            return;
        }
        if (_effect)
        {
            _effect->Transform(_effectFacing, _effectUp, Position);
        }
        UpdateRotation();
        CallStateProcess();
        UpdateSpeed();
        CheckCollision();
        CheckHitPlayer();
        UpdatePosition();
        if (_health > 0)
        {
            _soundSource.PlaySfx(SfxId::GOREA2_ATTACK2_LOOP2, true);
        }
    }

    void Enemy33Entity::UpdateRotation()
    {
        _field1BE += _field1BC;
        if (_field1BE >= 360.0F)
        {
            _field1BE -= 360.0F;
        }
        const Vector3 axis = Vector3::Cross(_effectUp, _effectFacing);
        const Matrix4 mtx = CreateFromAxisAngle(
            axis, DegreesToRadians(_field1BE));
        const Vector3 up = Matrix::Vec3MultMtx3(_effectUp, mtx);
        const Vector3 facing = Matrix::Vec3MultMtx3(_effectFacing, mtx);
        SetTransform(facing, up, Position);
    }

    void Enemy33Entity::UpdateSpeed()
    {
        if (_target == nullptr)
        {
            return;
        }
        Vector3 speed = static_cast<Vector3>(_target->Position)
            - static_cast<Vector3>(Position);
        if (LengthSquared(speed) > 1.0F / 128.0F)
        {
            speed = speed.Normalized();
            _effectFacing = speed;
            _effectUp = Enemy31Entity::Func21418EC(
                _effectFacing, _effectUp);
            speed = ScaleVector(speed, _field1A4);
        }
        _speed = DivideVector(speed, 2.0F);
    }

    void Enemy33Entity::CheckCollision()
    {
        if (_health == 0)
        {
            return;
        }
        const Vector3 travel
            = _prevPos - static_cast<Vector3>(Position);
        Formats::CollisionResult discard{};
        if (LengthSquared(travel) > 1.0F / 128.0F
            && Formats::CollisionDetection::CheckBetweenPoints(
                _prevPos, Position, Formats::TestFlags::Beams,
                _scene, discard))
        {
            CheckExplosionDamage();
            _soundSource.PlaySfx(
                SfxId::GOREA2_ATTACK2_DIE_SCR,
                false, false, -1.0F, true);
            SpawnEffect(178, Position, _effectUp, _effectFacing);
        }
    }

    void Enemy33Entity::CheckHitPlayer()
    {
        if (_health != 0 && _target != nullptr
            && HitPlayerAt(HitPlayers, _target->SlotIndex()))
        {
            Explode(176);
        }
    }

    void Enemy33Entity::UpdatePosition()
    {
        _basePos = _basePos + _speed;
        if (_shakeTimer > 0)
        {
            const float x
                = (static_cast<std::int32_t>(Rng::GetRandomInt2(512))
                    - 256) / 4096.0F;
            const float y
                = (static_cast<std::int32_t>(Rng::GetRandomInt2(512))
                    - 256) / 4096.0F;
            const float z
                = (static_cast<std::int32_t>(Rng::GetRandomInt2(512))
                    - 256) / 4096.0F;
            Position = Vector3(
                _basePos.X + x, _basePos.Y + y, _basePos.Z + z);
            --_shakeTimer;
        }
    }

    bool Enemy33Entity::EnemyTakeDamage(EntityBase* source)
    {
        (void)source;
        if (_health > 0)
        {
            _shakeTimer = 30 * 2;
            SpawnEffect(176, Position);
            Func2140E44();
            _soundSource.PlaySfx(SfxId::GOREA2_ATTACK2_HIT);
        }
        else
        {
            SpawnItemDrop();
            Explode(177);
        }
        return false;
    }

    void Enemy33Entity::Func2140E44()
    {
        _timeSinceDamage = 0;
        _flag = true;
    }

    void Enemy33Entity::SpawnItemDrop()
    {
        bool spawn = true;
        ItemType itemType = ItemType::None;
        const std::int32_t chance1 = _itemChance1;
        const std::int32_t chance2 = ManagedAdd(chance1, _itemChance2);
        const std::int32_t chance3 = ManagedAdd(chance2, _itemChance3);
        const std::int32_t chance4 = ManagedAdd(chance3, _itemChance4);
        const std::uint32_t rand = Rng::GetRandomInt2(chance4);
        if (static_cast<std::int64_t>(rand) >= static_cast<std::int64_t>(chance3))
        {
            spawn = false;
        }
        else if (static_cast<std::int64_t>(rand) >= static_cast<std::int64_t>(chance2))
        {
            itemType = ItemType::UASmall;
        }
        else if (static_cast<std::int64_t>(rand) >= static_cast<std::int64_t>(chance1))
        {
            itemType = ItemType::MissileSmall;
        }
        else
        {
            itemType = ItemType::HealthSmall;
        }
        if (spawn)
        {
            const std::int32_t despawnTime = 300 * 2;
            auto item = std::make_shared<ItemInstanceEntity>(
                ItemInstanceEntityData(Position, itemType, despawnTime),
                NodeRef, _scene);
            RequireReference(_scene).AddEntity(std::move(item));
        }
    }

    void Enemy33Entity::Explode(std::int32_t effectId)
    {
        _soundSource.PlaySfx(
            SfxId::GOREA2_ATTACK2_DIE_SCR,
            false, false, -1.0F, true);
        CheckExplosionDamage();
        SpawnEffect(effectId, Position);
        _soundSource.StopAllSfx();
        _health = 0;
        Flags &= ~EnemyFlags::Visible;
        Flags &= ~EnemyFlags::CollidePlayer;
        Flags &= ~EnemyFlags::CollideBeam;
        Flags |= EnemyFlags::Invincible;
        Position = WithY(Position, 524288.0F);
        _speed = Vector3::Zero;
        if (_effect)
        {
            RequireReference(_scene).UnlinkEffectEntry(_effect);
            _effect.reset();
        }
    }

    void Enemy33Entity::CheckExplosionDamage()
    {
        if (_target == nullptr)
        {
            return;
        }
        Vector3 toTarget = static_cast<Vector3>(_target->Position)
            - static_cast<Vector3>(Position);
        const float distance = Length(toTarget);
        if (distance >= _field1A0)
        {
            return;
        }
        Formats::CollisionResult result{};
        const Vector3 limitMin(
            Position.X - _field1AC,
            Position.Y - _field1AC,
            Position.Z - _field1AC);
        const Vector3 limitMax(
            Position.X + _field1AC,
            Position.Y + _field1AC,
            Position.Z + _field1AC);
        const std::vector<std::shared_ptr<Formats::CollisionCandidate>>& candidates
            = Formats::CollisionDetection::GetCandidatesForLimits(
                std::nullopt, Vector3::Zero, 0.0F,
                limitMin, limitMax, false, _scene);
        if (Formats::CollisionDetection::CheckBetweenPoints(
            &candidates, _prevPos, _target->Position,
            Formats::TestFlags::Beams, _scene, result))
        {
            return;
        }
        std::int32_t damage = _field1B4;
        float dirMag = _field1B0;
        PlayerEntity& mainPlayer = RequireReference(PlayerEntity::Main().get());
        if (!HitPlayerAt(HitPlayers, mainPlayer.SlotIndex()))
        {
            const float factor
                = std::clamp(distance / _field1A0, 0.0F, 1.0F);
            damage = FloatToInt32(
                static_cast<float>(damage)
                - static_cast<float>(damage) * factor);
            dirMag = static_cast<float>(
                FloatToInt32(dirMag - dirMag * factor));
        }
        if (distance > 1.0F / 128.0F)
        {
            toTarget = toTarget.Normalized();
        }
        else
        {
            toTarget = Vector3(0.0F, 1.0F, 0.0F);
        }
        toTarget = ScaleVector(toTarget, dirMag);
        _target->TakeDamage(
            damage, DamageFlags::NoDmgInvuln, toTarget, this);
    }

    bool Enemy33Entity::EnemyGetDrawInfo()
    {
        if (RequireReference(_scene).ProcessFrame() && _flag)
        {
            _timeSinceDamage = 4 * 2;
        }
        DrawGeneric();
        return true;
    }

    void Enemy33Entity::State00()
    {
        (void)CallSubroutine(Metadata::Enemy33Subroutines, this);
    }

    void Enemy33Entity::State01()
    {
        (void)CallSubroutine(Metadata::Enemy33Subroutines, this);
    }

    void Enemy33Entity::State02()
    {
        if (_field1C4 > 0)
        {
            if (_field1C5 != 0)
            {
                --_field1C5;
            }
            if (_field1C5 == 0)
            {
                _field1C5 = _field1C4;
                _flag = !_flag;
                if (_flag)
                {
                    Func2140E44();
                }
            }
        }
        (void)CallSubroutine(Metadata::Enemy33Subroutines, this);
    }

    void Enemy33Entity::State03()
    {
        (void)CallSubroutine(Metadata::Enemy33Subroutines, this);
    }

    bool Enemy33Entity::Behavior00()
    {
        if (_field1B8 > 0)
        {
            --_field1B8;
        }
        if (_field1B8 == 0)
        {
            Explode(178);
            return true;
        }
        return false;
    }

    bool Enemy33Entity::Behavior01()
    {
        return true;
    }

    bool Enemy33Entity::Behavior02()
    {
        if (_field1B6 > 0)
        {
            --_field1B6;
        }
        if (_field1B6 == 0)
        {
            Explode(178);
            return true;
        }
        return false;
    }

    bool Enemy33Entity::Behavior03()
    {
        if (_target == nullptr)
        {
            return false;
        }
        Formats::CollisionResult discard{};
        return Formats::CollisionDetection::CheckSphereOverlapVolume(
            &_target->Volume(), Position, _field1A0, discard);
    }

    bool Enemy33Entity::Behavior00(Enemy33Entity* enemy)
    {
        return RequireEnemy(enemy).Behavior00();
    }

    bool Enemy33Entity::Behavior01(Enemy33Entity* enemy)
    {
        return RequireEnemy(enemy).Behavior01();
    }

    bool Enemy33Entity::Behavior02(Enemy33Entity* enemy)
    {
        return RequireEnemy(enemy).Behavior02();
    }

    bool Enemy33Entity::Behavior03(Enemy33Entity* enemy)
    {
        return RequireEnemy(enemy).Behavior03();
    }
}
