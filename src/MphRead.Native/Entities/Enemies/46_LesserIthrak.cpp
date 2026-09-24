#include "46_LesserIthrak.hpp"

#include "50_HitZone.hpp"
#include "../../Formats/CollisionDetection.hpp"
#include "../../Metadata/Enemies.hpp"
#include "../../Metadata/SoundMeta.hpp"
#include "../../Scene.hpp"
#include "../../Utility/Rng.hpp"
#include "../EnemySpawnEntity.hpp"
#include "../Players/PlayerEntity.hpp"
#include "../../NativeRuntime/System/Managed.hpp"
#include "../../Formats/Types.hpp"

#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <utility>

using ::MphRead::NativeRuntime::RequireReference;
using ::OpenTK::Mathematics::AddY;
using ::OpenTK::Mathematics::CreateRotationY;
using ::OpenTK::Mathematics::Divide;
using ::OpenTK::Mathematics::Length;
using ::OpenTK::Mathematics::LengthSquared;
using ::OpenTK::Mathematics::MathHelper::DegreesToRadians;
using ::OpenTK::Mathematics::MathHelper::RadiansToDegrees;
using ::OpenTK::Mathematics::ScaleVector;
using ::OpenTK::Mathematics::WithY;

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

        [[nodiscard]] Enemy46Entity& RequireEnemy(Enemy46Entity* enemy)
        {
            return RequireReference(enemy);
        }

        [[nodiscard]] PlayerEntity& MainPlayer()
        {
            return RequireReference(PlayerEntity::Main());
        }

    }

    Enemy46Entity::Enemy46Entity(EnemyInstanceEntityData data,
        Formats::Culling::NodeRef nodeRef, Scene* scene)
        : EnemyInstanceEntity(data, nodeRef, scene),
          _spawner(CastSpawner(data.Spawner))
    {
        auto processes = std::make_shared<ManagedArray<std::function<void()>>>(20);
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
        (*processes)[17] = [this]() { State17(); };
        (*processes)[18] = [this]() { State18(); };
        (*processes)[19] = [this]() { State19(); };
        _stateProcesses = std::move(processes);
    }

    void Enemy46Entity::EnemyInitialize()
    {
        EnemySpawnEntity& spawner = RequireReference(_spawner);
        Setup(
            spawner.Data.Header.Position.ToFloatVector(),
            spawner.Data.Header.FacingVector.ToFloatVector(),
            0x5555,
            spawner.Data.Fields.S00().Volume0,
            spawner.Data.Fields.S00().Volume2,
            spawner.Data.Fields.S00().Volume1,
            spawner.Data.Fields.S00().Volume3);
    }

    void Enemy46Entity::Setup(
        Vector3 position,
        Vector3 facing,
        std::int32_t effectiveness,
        RawCollisionVolume hurtVolume,
        RawCollisionVolume volume1,
        RawCollisionVolume volume2,
        RawCollisionVolume volume3)
    {
        SetTransform(facing.Normalized(), Vector3(0.0F, 1.0F, 0.0F), position);
        _health = _healthMax = 85;
        Flags |= EnemyFlags::Visible;
        Flags |= EnemyFlags::OnRadar;
        _boundingRadius = 0.5F;
        _hurtVolumeInit = CollisionVolume(hurtVolume);
        _homeVolume = CollisionVolume::Move(volume1, Position);
        _rangeVolume = CollisionVolume::Move(volume2, Position);
        _warnVolume = CollisionVolume::Move(volume3, Position);

        const std::int32_t modelIndex = static_cast<std::int32_t>(EnemyType());
        if (modelIndex < 0
            || static_cast<std::size_t>(modelIndex) >= Metadata::EnemyModelNames.size())
        {
            throw SceneDetail::IndexOutOfRangeException();
        }
        ModelInstance& inst = SetUpModel(
            Metadata::EnemyModelNames[static_cast<std::size_t>(modelIndex)]);
        inst.SetAnimation(
            5, 0, SetFlags::Texture | SetFlags::Material | SetFlags::Node);
        inst.SetAnimation(15, 1, SetFlags::Texcoord);

        Model& model = RequireReference(inst.Model());
        const auto& materials = RequireReference(model.Materials);
        for (std::int32_t i = 0;
            i < static_cast<std::int32_t>(materials.size()); ++i)
        {
            Material& material = RequireReference(
                materials[static_cast<std::size_t>(i)]);
            if (material.Name == "Mouth_tga")
            {
                _mouthMaterial = &material;
                break;
            }
        }

        _delayTimer = 30 * 2;
        _moveStart = Position;
        _moveTimer = 600 * 2;
        Metadata::LoadEffectiveness(effectiveness, BeamEffectiveness);
        SpawnHitZone();
    }

    bool Enemy46Entity::AnimEnded() const
    {
        const ModelInstance& model = _models[0];
        AnimationInfo& animInfo = RequireReference(model.AnimInfo);
        ManagedArray<AnimFlags>& flags = RequireReference(animInfo.Flags);
        if (flags.Length() == 0)
        {
            throw SceneDetail::IndexOutOfRangeException();
        }
        return TypeExtensions::TestFlag(flags[0], AnimFlags::Ended);
    }

    void Enemy46Entity::SetNodeAnim(std::int32_t id, AnimFlags flags)
    {
        _models[0].SetAnimation(
            id, 0, SetFlags::Texture | SetFlags::Material | SetFlags::Node, flags);
    }

    void Enemy46Entity::SpawnHitZone()
    {
        std::shared_ptr<EnemyInstanceEntity> enemy = EnemySpawnEntity::SpawnEnemy(
            this, MphRead::EnemyType::HitZone, NodeRef, _scene);
        _hitZone = std::dynamic_pointer_cast<Enemy50Entity>(enemy);
        if (_hitZone)
        {
            RequireReference(_scene).AddEntity(_hitZone);
            _hitZone->Transform = GetTransformMatrix(
                FacingVector(), Vector3(0.0F, 1.0F, 0.0F), Position);
            Metadata::LoadEffectiveness(0xFFFF, _hitZone->BeamEffectiveness);
            _hitZone->HitPlayers[0] = true;
            const CollisionVolume hurtVolume(
                Vector3(0.0F, 0.93F, -0.8F), 0.6F);
            _hitZone->SetUp(_health, hurtVolume, 1.0F);
        }
    }

    void Enemy46Entity::EnemyProcess()
    {
        if (_state1 == 9)
        {
            if (LengthSquared(_speed) <= Fixed::ToFloat(50) / 2.0F)
            {
                _speed = Vector3::Zero;
            }
            else
            {
                _speed.X -= _acceleration.X / 2.0F;
                _speed.Z -= _acceleration.Z / 2.0F;
            }
        }
        else if (_state1 == 3 || _state1 == 4 || _state1 == 5
            || _state1 == 6 || _state1 == 8)
        {
            const Vector3 facing = WithY(
                static_cast<Vector3>(MainPlayer().Position)
                    - static_cast<Vector3>(Position),
                0.0F).Normalized();
            SetTransform(
                facing, Vector3(0.0F, 1.0F, 0.0F), Position);
        }

        if (_state1 != 0 && _state1 != 1 && _state1 != 2)
        {
            if (_state1 != 7 && _state1 != 19)
            {
                (void)HandleBlockingCollision(
                    AddY(Position, 0.5F),
                    _hurtVolume,
                    true,
                    _groundCol,
                    _wallCol);
            }
            if (_state1 != 7 && _state1 != 8)
            {
                (void)ContactDamagePlayer(15, true);
            }
        }

        CallStateProcess();

        if (_state1 != 0 && _state1 != 1 && _state1 != 2 && !_groundCol)
        {
            _speed.Y -= Fixed::ToFloat(100) / 4.0F;
        }
    }

    bool Enemy46Entity::EnemyTakeDamage(EntityBase* source)
    {
        (void)source;
        if (_health == 0 && _hitZone)
        {
            _hitZone->SetHealth(0);
            _hitZone.reset();
        }
        return false;
    }

    void Enemy46Entity::PickMoveTarget(CollisionVolume volume)
    {
        _moveStart = Position;
        Vector3 moveTarget;
        if (volume.Type == VolumeType::Cylinder
            || volume.Type == VolumeType::Sphere)
        {
            const float radius = volume.Type == VolumeType::Cylinder
                ? volume.CylinderRadius
                : volume.SphereRadius;
            const Vector3 pos = volume.Type == VolumeType::Cylinder
                ? volume.CylinderPosition
                : volume.SpherePosition;
            const float dist = Fixed::ToFloat(
                Rng::GetRandomInt2(Fixed::ToInt(radius)));
            Vector3 vec(dist, 0.0F, 0.0F);
            _dropAngleSign *= -1.0F;
            const float randAngle = Fixed::ToFloat(
                Rng::GetRandomInt2(0xB4000)) * _dropAngleSign;
            const Matrix4 rotY = CreateRotationY(DegreesToRadians(randAngle));
            vec = Matrix::Vec3MultMtx3(vec, rotY);
            moveTarget = Vector3(
                pos.X + vec.X,
                Position.Y - 20.0F,
                pos.Z + vec.Z);
        }
        else
        {
            assert(volume.Type == VolumeType::Box);
            const float distX = Fixed::ToFloat(
                Rng::GetRandomInt2(Fixed::ToInt(volume.BoxDot1)));
            const float distZ = Fixed::ToFloat(
                Rng::GetRandomInt2(Fixed::ToInt(volume.BoxDot3)));
            moveTarget = Vector3(
                volume.BoxVector1.X * distX
                    + volume.BoxVector3.X * distZ
                    + volume.BoxPosition.X,
                Position.Y - 20.0F,
                volume.BoxVector1.Z * distX
                    + volume.BoxVector3.Z * distZ
                    + volume.BoxPosition.Z);
        }

        Formats::CollisionResult result{};
        (void)Formats::CollisionDetection::CheckBetweenPoints(
            Position,
            moveTarget,
            Formats::TestFlags::None,
            _scene,
            result);
        _moveTarget = WithY(moveTarget, result.Position.Y);
        _moveDistSqr = LengthSquared(
            WithY(_moveTarget - static_cast<Vector3>(Position), 0.0F));
    }

    bool Enemy46Entity::HandleCollision(Vector3 testPos)
    {
        _groundCol = false;
        testPos = AddY(testPos, 0.5F);
        ManagedArray<Formats::CollisionResult> results(30);
        const std::int32_t count = Formats::CollisionDetection::CheckInRadius(
            testPos,
            _boundingRadius,
            30,
            false,
            Formats::TestFlags::None,
            _scene,
            &results);
        if (count == 0)
        {
            return false;
        }

        for (std::int32_t i = 0; i < count; ++i)
        {
            const Formats::CollisionResult result
                = results[static_cast<std::size_t>(i)];
            float v12;
            if (result.Field0 != 0)
            {
                v12 = _boundingRadius - result.Field14;
            }
            else
            {
                v12 = _boundingRadius + result.Plane.W
                    - Vector3::Dot(testPos, result.Plane.Xyz());
            }
            if (v12 > 0.0F)
            {
                Position = static_cast<Vector3>(Position)
                    + ScaleVector(result.Plane.Xyz(), v12);
                if (result.Plane.Y >= 0.1F || result.Plane.Y <= -0.1F)
                {
                    _groundCol = true;
                }
                else
                {
                    _wallCol = true;
                }
                const float dot = Vector3::Dot(_speed, result.Plane.Xyz());
                if (dot < 0.0F)
                {
                    _speed = _speed + ScaleVector(result.Plane.Xyz(), -dot);
                }
            }
        }
        return true;
    }

    void Enemy46Entity::CallSubroutine()
    {
        (void)EnemyInstanceEntity::CallSubroutine<Enemy46Entity>(
            Metadata::Enemy46Subroutines, this);
    }

    void Enemy46Entity::State00()
    {
        CallSubroutine();
    }

    void Enemy46Entity::State01()
    {
        CallSubroutine();
    }

    void Enemy46Entity::State02()
    {
        CallSubroutine();
    }

    void Enemy46Entity::State03()
    {
        const Vector3 facing = FacingVector();
        _speed.X = facing.X * 0.15F / 2.0F;
        _speed.Z = facing.Z * 0.15F / 2.0F;
        _soundSource.PlaySfx(SfxId::HANGING_TERROR_WALK, true);
        CallSubroutine();
    }

    void Enemy46Entity::State04()
    {
        CallSubroutine();
    }

    void Enemy46Entity::State05()
    {
        _speed.X = 0.0F;
        _speed.Z = 0.0F;
        CallSubroutine();
    }

    void Enemy46Entity::State06()
    {
        _speed.X = 0.0F;
        _speed.Z = 0.0F;
        CallSubroutine();
    }

    void Enemy46Entity::State07()
    {
        CallSubroutine();
    }

    void Enemy46Entity::State08()
    {
        _speed.X = 0.0F;
        _speed.Z = 0.0F;
        if (_delayTimer == 15 * 2)
        {
            SetNodeAnim(2, AnimFlags::NoLoop);
        }
        else if (_delayTimer == 0)
        {
            MainPlayer().TakeDamage(
                10,
                DamageFlags::NoDmgInvuln,
                ScaleVector(_speed, 2.0F),
                this);
            _delayTimer = 30 * 2;
        }
        if (_delayTimer > 0)
        {
            --_delayTimer;
        }
        CallSubroutine();
    }

    void Enemy46Entity::State09()
    {
        CallSubroutine();
    }

    void Enemy46Entity::State10()
    {
        _speed.X = 0.0F;
        _speed.Z = 0.0F;
        CallSubroutine();
    }

    void Enemy46Entity::State11()
    {
        _speed.X = 0.0F;
        _speed.Z = 0.0F;
        CallSubroutine();
    }

    void Enemy46Entity::State12()
    {
        _speed.X = 0.0F;
        _speed.Z = 0.0F;
        CallSubroutine();
    }

    void Enemy46Entity::State13()
    {
        _speed.X = 0.0F;
        _speed.Z = 0.0F;
        CallSubroutine();
    }

    void Enemy46Entity::State14()
    {
        _soundSource.PlaySfx(SfxId::HANGING_TERROR_WALK, true);
        CallSubroutine();
    }

    void Enemy46Entity::State15()
    {
        _speed.X = 0.0F;
        _speed.Z = 0.0F;
        CallSubroutine();
    }

    void Enemy46Entity::State16()
    {
        _speed.X = 0.0F;
        _speed.Z = 0.0F;
        CallSubroutine();
    }

    void Enemy46Entity::State17()
    {
        _speed.X = 0.0F;
        _speed.Z = 0.0F;
        CallSubroutine();
    }

    void Enemy46Entity::State18()
    {
        _speed.X = 0.0F;
        _speed.Z = 0.0F;
        CallSubroutine();
    }

    void Enemy46Entity::State19()
    {
        CallSubroutine();
    }

    bool Enemy46Entity::Behavior00()
    {
        if (!AnimEnded())
        {
            return false;
        }
        SetNodeAnim(16);
        PickMoveTarget(_homeVolume);
        _targetVec = WithY(
            _moveTarget - static_cast<Vector3>(Position), 0.0F).Normalized();
        const float angle = RadiansToDegrees(
            std::acos(Vector3::Dot(FacingVector(), _targetVec)));
        _stepCount = 10 * 2;
        _aimAngleStep = angle / static_cast<float>(_stepCount);
        return true;
    }

    bool Enemy46Entity::Behavior01()
    {
        if (_stepCount > 0)
        {
            --_stepCount;
            return false;
        }
        SetNodeAnim(13);
        const Vector3 facing = WithY(
            static_cast<Vector3>(MainPlayer().Position)
                - static_cast<Vector3>(Position),
            0.0F).Normalized();
        SetTransform(
            facing, Vector3(0.0F, 1.0F, 0.0F), Position);
        _speed = Vector3(facing.X * 0.15F, 0.0F, facing.Z * 0.15F);
        _speed = Divide(_speed, 2.0F);
        _stepCount = 3 * 2;
        return true;
    }

    bool Enemy46Entity::Behavior02()
    {
        if (!_warnVolume.TestPoint(MainPlayer().Position))
        {
            return false;
        }
        SetNodeAnim(6);
        _soundSource.PlaySfx(SfxId::HANGING_TERROR_WARN);
        return true;
    }

    bool Enemy46Entity::Behavior03()
    {
        if (!SeekTargetFacing(
                _targetVec,
                Vector3(0.0F, 1.0F, 0.0F),
                _stepCount,
                _aimAngleStep))
        {
            return false;
        }
        SetNodeAnim(13);
        const Vector3 facing = FacingVector();
        _speed = Vector3(facing.X * 0.15F, 0.0F, facing.Z * 0.15F);
        _speed = Divide(_speed, 2.0F);
        return true;
    }

    bool Enemy46Entity::Behavior04()
    {
        if (!SeekTargetFacing(
                _targetVec,
                Vector3(0.0F, 1.0F, 0.0F),
                _stepCount,
                _aimAngleStep))
        {
            return false;
        }
        SetNodeAnim(13);
        const Vector3 facing = FacingVector();
        _speed = Vector3(facing.X * 0.15F, 0.0F, facing.Z * 0.15F);
        _speed = Divide(_speed, 2.0F);
        _stepCount = 50 * 2;
        return true;
    }

    bool Enemy46Entity::Behavior05()
    {
        if (_stepCount > 0)
        {
            --_stepCount;
            return false;
        }
        if (_state1 == 2)
        {
            if (HandleCollision(Position))
            {
                SetNodeAnim(14);
                _moveTarget.Y = Position.Y;
                _speed.X = 0.0F;
                _speed.Z = 0.0F;
                _stepCount = 60 * 2;
                _soundSource.PlaySfx(SfxId::HANGING_TERROR_SCREAM_SCR);
                return true;
            }
        }
        else if (_state1 == 7)
        {
            const Vector3 facing = FacingVector();
            const Vector3 destPos
                = static_cast<Vector3>(Position) + ScaleVector(facing, 2.0F);
            if (HandleCollision(Position) || HandleCollision(destPos))
            {
                SetNodeAnim(10, AnimFlags::NoLoop);
                _speed = Vector3(facing.X * 0.18F, 0.0F, facing.Z * 0.18F);
                _speed = Divide(_speed, 2.0F);
                _acceleration = Divide(_speed, _accelSteps);
                return true;
            }
        }
        return false;
    }

    bool Enemy46Entity::Behavior06()
    {
        if (!AnimEnded())
        {
            return false;
        }
        SetNodeAnim(16);
        _targetVec = WithY(
            static_cast<Vector3>(MainPlayer().Position)
                - static_cast<Vector3>(Position),
            0.0F).Normalized();
        const float angle = RadiansToDegrees(
            std::acos(Vector3::Dot(FacingVector(), _targetVec)));
        _stepCount = 10 * 2;
        _aimAngleStep = angle / static_cast<float>(_stepCount);
        return true;
    }

    bool Enemy46Entity::Behavior07()
    {
        if (!HandleCollision(Position))
        {
            return false;
        }
        SetNodeAnim(13);
        const Vector3 facing = FacingVector();
        _speed = Vector3(facing.X * 0.15F, 0.0F, facing.Z * 0.15F);
        _speed = Divide(_speed, 2.0F);
        return true;
    }

    bool Enemy46Entity::Behavior08()
    {
        if (!SeekTargetFacing(
                _targetVec,
                Vector3(0.0F, 1.0F, 0.0F),
                _stepCount,
                _aimAngleStep))
        {
            return false;
        }
        SetNodeAnim(14, AnimFlags::NoLoop);
        return true;
    }

    bool Enemy46Entity::Behavior09()
    {
        if (!AnimEnded())
        {
            return false;
        }
        SetNodeAnim(4, AnimFlags::NoLoop);
        _wallCol = false;
        return true;
    }

    bool Enemy46Entity::Behavior10()
    {
        if (!SeekTargetFacing(
                _targetVec,
                Vector3(0.0F, 1.0F, 0.0F),
                _stepCount,
                _aimAngleStep))
        {
            return false;
        }
        SetNodeAnim(14, AnimFlags::NoLoop);
        _soundSource.StopSfx(SfxId::HANGING_TERROR_WALK);
        _soundSource.PlaySfx(SfxId::HANGING_TERROR_SCREAM_SCR);
        _stepCount = 50 * 2;
        return true;
    }

    bool Enemy46Entity::Behavior11()
    {
        PlayerEntity& mainPlayer = MainPlayer();
        CollisionVolume playerCol = mainPlayer.Volume();
        assert(playerCol.Type == VolumeType::Sphere);
        playerCol.SpherePosition.Y += Fixed::ToFloat(1000);
        playerCol.SphereRadius += Fixed::ToFloat(1000);
        Formats::CollisionResult result{};
        if (!Formats::CollisionDetection::CheckVolumesOverlap(
                &playerCol, &_hurtVolume, result))
        {
            return false;
        }
        mainPlayer.HandleCollision(result);
        _speed.X = 0.0F;
        _speed.Z = 0.0F;
        SetNodeAnim(9, AnimFlags::NoLoop);
        mainPlayer.TakeDamage(
            15, DamageFlags::None, ScaleVector(_speed, 2.0F), this);
        return true;
    }

    bool Enemy46Entity::Behavior12()
    {
        if (!AnimEnded())
        {
            return false;
        }
        _stepCount = 50 * 2;
        const Vector3 facing = FacingVector();
        _speed = Vector3(facing.X * 0.15F, 0.0F, facing.Z * 0.15F);
        _speed = Divide(_speed, 2.0F);
        SetNodeAnim(13);
        return true;
    }

    bool Enemy46Entity::Behavior13()
    {
        const Vector3 playerPos = MainPlayer().Position;
        if (!_rangeVolume.TestPoint(playerPos)
            || !_rangeVolume.TestPoint(Position))
        {
            return false;
        }
        SetNodeAnim(16);
        _targetVec = WithY(
            playerPos - static_cast<Vector3>(Position), 0.0F).Normalized();
        const float angle = RadiansToDegrees(
            std::acos(Vector3::Dot(FacingVector(), _targetVec)));
        _stepCount = 10 * 2;
        _aimAngleStep = angle / static_cast<float>(_stepCount);
        _speed = Vector3::Zero;
        _soundSource.StopSfx(SfxId::HANGING_TERROR_WALK);
        return true;
    }

    bool Enemy46Entity::Behavior14()
    {
        if (_warnVolume.TestPoint(MainPlayer().Position))
        {
            return false;
        }
        SetNodeAnim(5);
        return true;
    }

    bool Enemy46Entity::Behavior15()
    {
        if (!_rangeVolume.TestPoint(MainPlayer().Position))
        {
            return false;
        }
        PickMoveTarget(_homeVolume);
        const Vector3 travel
            = _moveTarget - static_cast<Vector3>(Position);
        const float distance = Length(travel);
        _stepCount = static_cast<std::uint16_t>(
            (distance / _stepDistance) + 1.0F);
        _speed = ScaleVector(travel, _stepDistance / distance);
        SetNodeAnim(12, AnimFlags::NoLoop);
        _soundSource.PlaySfx(SfxId::HANGING_TERROR_DROP);
        return true;
    }

    void Enemy46Entity::StartRecoil()
    {
        const float factor = Fixed::ToFloat(500);
        const Vector3 facing = FacingVector();
        Vector3 vec(
            -facing.X * factor,
            0.0F,
            -facing.Z * factor);
        _recoilAngleSign *= -1.0F;
        const float randAngle = (
            Fixed::ToFloat(Rng::GetRandomInt2(0x1E000)) + 30.0F)
            * _recoilAngleSign;
        const Matrix4 rotY = CreateRotationY(DegreesToRadians(randAngle));
        vec = Matrix::Vec3MultMtx3(vec, rotY);
        _speed = Vector3(vec.X, Fixed::ToFloat(1000), vec.Z);
        _speed = Divide(_speed, 2.0F);
    }

    bool Enemy46Entity::Behavior16()
    {
        if (LengthSquared(
                static_cast<Vector3>(Position)
                    - static_cast<Vector3>(MainPlayer().Position))
            >= 1.5F * 1.5F)
        {
            return false;
        }
        const Vector3 destPos
            = static_cast<Vector3>(Position)
            + ScaleVector(FacingVector(), -2.0F);
        Formats::CollisionResult discard{};
        if (Formats::CollisionDetection::CheckBetweenPoints(
                Position,
                destPos,
                Formats::TestFlags::None,
                _scene,
                discard))
        {
            return false;
        }
        StartRecoil();
        SetNodeAnim(7, AnimFlags::NoLoop);
        _soundSource.StopSfx(SfxId::HANGING_TERROR_WALK);
        return true;
    }

    bool Enemy46Entity::Behavior17()
    {
        if (_stepCount > 0)
        {
            --_stepCount;
            return false;
        }
        _speed = ScaleVector(FacingVector(), Fixed::ToFloat(1800));
        _speed.Y = Fixed::ToFloat(600);
        _speed = Divide(_speed, 2.0F);
        _stepCount = 7 * 2;
        return true;
    }

    bool Enemy46Entity::Behavior18()
    {
        const Vector3 between
            = static_cast<Vector3>(Position)
            - static_cast<Vector3>(MainPlayer().Position);
        const float distSqr = LengthSquared(between);
        if (distSqr <= 1.5F * 1.5F || distSqr >= 2.0F * 2.0F)
        {
            return false;
        }
        _stepCount = 0;
        _speed = Vector3::Zero;
        _delayTimer = 15 * 2;
        _soundSource.StopSfx(SfxId::HANGING_TERROR_WALK);
        return true;
    }

    bool Enemy46Entity::Behavior19()
    {
        if (_reachingTarget)
        {
            SetNodeAnim(16);
            PickMoveTarget(_homeVolume);
            _soundSource.StopSfx(SfxId::HANGING_TERROR_WALK);
            _speed = Vector3::Zero;
            _targetVec = WithY(
                _moveTarget - static_cast<Vector3>(Position), 0.0F).Normalized();
            const float angle = RadiansToDegrees(
                std::acos(Vector3::Dot(FacingVector(), _targetVec)));
            _stepCount = 10 * 2;
            _aimAngleStep = angle / static_cast<float>(_stepCount);
            _reachingTarget = false;
            return true;
        }

        const Vector3 nextPos
            = static_cast<Vector3>(Position) + _speed;
        const Vector3 travel = _moveStart - nextPos;
        if (LengthSquared(travel) > _moveDistSqr)
        {
            _speed = nextPos - static_cast<Vector3>(Position);
            _reachingTarget = true;
        }
        return false;
    }

    bool Enemy46Entity::Behavior20()
    {
        if (_moveTimer > 0)
        {
            --_moveTimer;
            return false;
        }
        SetNodeAnim(16);
        _soundSource.StopSfx(SfxId::HANGING_TERROR_WALK);
        _speed = Vector3::Zero;
        _targetVec = WithY(
            static_cast<Vector3>(MainPlayer().Position)
                - static_cast<Vector3>(Position),
            0.0F).Normalized();
        const float angle = RadiansToDegrees(
            std::acos(Vector3::Dot(FacingVector(), _targetVec)));
        _stepCount = 10 * 2;
        _aimAngleStep = angle / static_cast<float>(_stepCount);
        _moveTimer = 600 * 2;
        return true;
    }

    bool Enemy46Entity::Behavior21()
    {
        if (_delayTimer > 0
            || LengthSquared(
                static_cast<Vector3>(Position)
                    - static_cast<Vector3>(MainPlayer().Position))
                <= 2.0F * 2.0F)
        {
            return false;
        }
        SetNodeAnim(13);
        const Vector3 facing = WithY(
            static_cast<Vector3>(MainPlayer().Position)
                - static_cast<Vector3>(Position),
            0.0F).Normalized();
        SetTransform(
            facing, Vector3(0.0F, 1.0F, 0.0F), Position);
        _speed = Vector3(facing.X * 0.15F, 0.0F, facing.Z * 0.15F);
        _speed = Divide(_speed, 2.0F);
        _delayTimer = 30 * 2;
        return true;
    }

    bool Enemy46Entity::Behavior22()
    {
        if (_delayTimer < 15 * 2
            || Rng::GetRandomInt2(0x64000) >= 2048 / 2)
        {
            return false;
        }
        StartRecoil();
        SetNodeAnim(7, AnimFlags::NoLoop);
        return true;
    }

    bool Enemy46Entity::Behavior23()
    {
        if (_stepCount > 0)
        {
            --_stepCount;
            return false;
        }
        SetNodeAnim(8);
        _stepCount = 12 * 2;
        return true;
    }

    bool Enemy46Entity::Behavior24()
    {
        const Vector3 between
            = static_cast<Vector3>(Position)
            - static_cast<Vector3>(MainPlayer().Position);
        const float distSqr = LengthSquared(between);
        if (distSqr <= 3.5F * 3.5F || distSqr >= 5.0F * 5.0F)
        {
            return false;
        }
        SetNodeAnim(1);
        _speed = Vector3::Zero;
        _stepCount = 38 * 2;
        _soundSource.StopSfx(SfxId::HANGING_TERROR_WALK);
        _soundSource.PlaySfx(SfxId::HANGING_TERROR_ATTACK1_SCR);
        return true;
    }

    bool Enemy46Entity::Behavior25()
    {
        if (_rangeVolume.TestPoint(Position)
            && _rangeVolume.TestPoint(MainPlayer().Position))
        {
            return false;
        }
        SetNodeAnim(14, AnimFlags::NoLoop);
        _soundSource.StopSfx(SfxId::HANGING_TERROR_WALK);
        _soundSource.PlaySfx(SfxId::HANGING_TERROR_SCREAM_SCR);
        _speed = Vector3::Zero;
        return true;
    }

    void Enemy46Entity::UpdateMouthMaterial()
    {
        RequireReference(_mouthMaterial).Diffuse = ColorRgb(31, 31, 31);
    }

    bool Enemy46Entity::EnemyGetDrawInfo()
    {
        UpdateMouthMaterial();
        return false;
    }

    bool Enemy46Entity::Behavior00(Enemy46Entity* enemy)
    {
        return RequireEnemy(enemy).Behavior00();
    }

    bool Enemy46Entity::Behavior01(Enemy46Entity* enemy)
    {
        return RequireEnemy(enemy).Behavior01();
    }

    bool Enemy46Entity::Behavior02(Enemy46Entity* enemy)
    {
        return RequireEnemy(enemy).Behavior02();
    }

    bool Enemy46Entity::Behavior03(Enemy46Entity* enemy)
    {
        return RequireEnemy(enemy).Behavior03();
    }

    bool Enemy46Entity::Behavior04(Enemy46Entity* enemy)
    {
        return RequireEnemy(enemy).Behavior04();
    }

    bool Enemy46Entity::Behavior05(Enemy46Entity* enemy)
    {
        return RequireEnemy(enemy).Behavior05();
    }

    bool Enemy46Entity::Behavior06(Enemy46Entity* enemy)
    {
        return RequireEnemy(enemy).Behavior06();
    }

    bool Enemy46Entity::Behavior07(Enemy46Entity* enemy)
    {
        return RequireEnemy(enemy).Behavior07();
    }

    bool Enemy46Entity::Behavior08(Enemy46Entity* enemy)
    {
        return RequireEnemy(enemy).Behavior08();
    }

    bool Enemy46Entity::Behavior09(Enemy46Entity* enemy)
    {
        return RequireEnemy(enemy).Behavior09();
    }

    bool Enemy46Entity::Behavior10(Enemy46Entity* enemy)
    {
        return RequireEnemy(enemy).Behavior10();
    }

    bool Enemy46Entity::Behavior11(Enemy46Entity* enemy)
    {
        return RequireEnemy(enemy).Behavior11();
    }

    bool Enemy46Entity::Behavior12(Enemy46Entity* enemy)
    {
        return RequireEnemy(enemy).Behavior12();
    }

    bool Enemy46Entity::Behavior13(Enemy46Entity* enemy)
    {
        return RequireEnemy(enemy).Behavior13();
    }

    bool Enemy46Entity::Behavior14(Enemy46Entity* enemy)
    {
        return RequireEnemy(enemy).Behavior14();
    }

    bool Enemy46Entity::Behavior15(Enemy46Entity* enemy)
    {
        return RequireEnemy(enemy).Behavior15();
    }

    bool Enemy46Entity::Behavior16(Enemy46Entity* enemy)
    {
        return RequireEnemy(enemy).Behavior16();
    }

    bool Enemy46Entity::Behavior17(Enemy46Entity* enemy)
    {
        return RequireEnemy(enemy).Behavior17();
    }

    bool Enemy46Entity::Behavior18(Enemy46Entity* enemy)
    {
        return RequireEnemy(enemy).Behavior18();
    }

    bool Enemy46Entity::Behavior19(Enemy46Entity* enemy)
    {
        return RequireEnemy(enemy).Behavior19();
    }

    bool Enemy46Entity::Behavior20(Enemy46Entity* enemy)
    {
        return RequireEnemy(enemy).Behavior20();
    }

    bool Enemy46Entity::Behavior21(Enemy46Entity* enemy)
    {
        return RequireEnemy(enemy).Behavior21();
    }

    bool Enemy46Entity::Behavior22(Enemy46Entity* enemy)
    {
        return RequireEnemy(enemy).Behavior22();
    }

    bool Enemy46Entity::Behavior23(Enemy46Entity* enemy)
    {
        return RequireEnemy(enemy).Behavior23();
    }

    bool Enemy46Entity::Behavior24(Enemy46Entity* enemy)
    {
        return RequireEnemy(enemy).Behavior24();
    }

    bool Enemy46Entity::Behavior25(Enemy46Entity* enemy)
    {
        return RequireEnemy(enemy).Behavior25();
    }
}
