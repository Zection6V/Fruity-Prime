#include "20_CretaphidEye.hpp"

#include "19_Cretaphid.hpp"
#include "../../Formats/Collision.hpp"
#include "../../Formats/CollisionDetection.hpp"
#include "../../Metadata/Enemies.hpp"
#include "../../Metadata/Weapons.hpp"
#include "../../Scene.hpp"
#include "../../Utility/Rng.hpp"
#include "../BeamEffectEntity.hpp"
#include "../BeamProjectileEntity.hpp"
#include "../ItemSpawnEntity.hpp"
#include "../Players/PlayerEntity.hpp"
#include "../../NativeRuntime/System/Managed.hpp"
#include "../../Formats/Types.hpp"

#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <type_traits>
#include <vector>

using ::MphRead::NativeRuntime::RequireReference;
using ::OpenTK::Mathematics::AddY;
using ::OpenTK::Mathematics::CreateRotationX;
using ::OpenTK::Mathematics::CreateScale;
using ::OpenTK::Mathematics::CreateTranslation;
using ::OpenTK::Mathematics::Divide;
using ::OpenTK::Mathematics::IdentityMatrix;
using ::OpenTK::Mathematics::MathHelper::DegreesToRadians;
using ::OpenTK::Mathematics::Multiply;

namespace MphRead::Entities::Enemies
{
    namespace
    {
        using OpenTK::Mathematics::Matrix4;
        using OpenTK::Mathematics::Vector3;
        using OpenTK::Mathematics::Vector4;

        Enemy19Entity* CastCretaphid(EntityBase* spawner) noexcept
        {
            Enemy19Entity* owner = dynamic_cast<Enemy19Entity*>(spawner);
            assert(owner != nullptr);
            return owner;
        }

        [[nodiscard]] PlayerEntity& MainPlayer()
        {
            return RequireReference(PlayerEntity::Main());
        }

        template <typename T>
        [[nodiscard]] T& ManagedAt(
            const std::shared_ptr<ManagedArray<T>>& values, std::int32_t index)
        {
            ManagedArray<T>& array = RequireReference(values);
            if (index < 0 || static_cast<std::size_t>(index) >= array.Length())
            {
                throw SceneDetail::IndexOutOfRangeException();
            }
            return array[static_cast<std::size_t>(index)];
        }

        template <typename T>
        [[nodiscard]] const T& ManagedAt(
            const std::shared_ptr<const ManagedArray<T>>& values, std::int32_t index)
        {
            const ManagedArray<T>& array = RequireReference(values);
            if (index < 0 || static_cast<std::size_t>(index) >= array.Length())
            {
                throw SceneDetail::IndexOutOfRangeException();
            }
            return array[static_cast<std::size_t>(index)];
        }

        [[nodiscard]] bool AnimationEnded(ModelInstance& model)
        {
            const std::shared_ptr<AnimationInfo> animInfo = model.AnimInfo;
            AnimationInfo& info = RequireReference(animInfo);
            ManagedArray<AnimFlags>& flags = RequireReference(info.Flags);
            if (flags.Length() == 0)
            {
                throw SceneDetail::IndexOutOfRangeException();
            }
            return (flags[0] & AnimFlags::Ended) != AnimFlags::None;
        }
    }

    Enemy20Entity::Enemy20Entity(EnemyInstanceEntityData data,
        Formats::Culling::NodeRef nodeRef, Scene* scene)
        : EnemyInstanceEntity(data, nodeRef, scene),
          _cretaphid(CastCretaphid(data.Spawner))
    {
    }

    std::int32_t Enemy20Entity::SegmentIndex() const noexcept
    {
        return _segmentIndex;
    }

    void Enemy20Entity::SetUp(std::shared_ptr<Node> attachNode,
        std::int32_t scanId, std::uint32_t effectiveness,
        std::uint16_t health, Vector3 position, float radius)
    {
        SetHealthbarMessageId(1);
        if (EyeIndex > 6)
        {
            _segmentIndex = 2;
        }
        else if (EyeIndex > 2)
        {
            _segmentIndex = 1;
        }
        else
        {
            _segmentIndex = 0;
        }
        _beamCollisionPos = Vector3::Zero;
        _stateTimer = 0;
        BeamType = 2;
        EyeActive = true;
        _attachNode = attachNode;
        _scanId = scanId;
        Metadata::LoadEffectiveness(effectiveness, BeamEffectiveness);
        _state1 = _state2 = 255;
        _health = _healthMax = health;
        Flags |= EnemyFlags::Visible;
        Flags |= EnemyFlags::Invincible;
        Flags |= EnemyFlags::NoMaxDistance;

        Node& node = RequireReference(_attachNode);
        Matrix4 transform = GetTransformMatrix(
            node.Transform.Row2().Xyz(), node.Transform.Row1().Xyz());
        const Vector3 translation = node.Transform.Row3().Xyz() + position;
        transform.M41 = translation.X;
        transform.M42 = translation.Y;
        transform.M43 = translation.Z;
        Transform = transform;
        _hurtVolumeInit = CollisionVolume(Vector3::Zero, 0.5F);
        _boundingRadius = radius;
        SetUpModel("CylinderBossEye");
    }

    void Enemy20Entity::UpdateState(std::uint8_t newState)
    {
        if (_state1 == 7)
        {
            return;
        }
        if (newState != _state1)
        {
            if (newState == 0)
            {
                _models[0].SetAnimation(
                    3, AnimFlags::NoLoop | AnimFlags::Reverse);
                Flags |= EnemyFlags::Invincible;
                Enemy19Entity& cretaphid = RequireReference(_cretaphid);
                if (cretaphid.PhaseIndex() == 0)
                {
                    _stateTimer = static_cast<std::int32_t>(ManagedAt(
                        cretaphid.Values().Phase0EyeStateTimer0, EyeIndex)) * 2;
                }
                else if (cretaphid.PhaseIndex() == 1)
                {
                    _stateTimer = static_cast<std::int32_t>(ManagedAt(
                        cretaphid.Values().Phase1EyeStateTimer0, EyeIndex)) * 2;
                }
                else if (cretaphid.PhaseIndex() == 2)
                {
                    _stateTimer = static_cast<std::int32_t>(ManagedAt(
                        cretaphid.Values().Phase2EyeStateTimer0, EyeIndex)) * 2;
                }
            }
            else if (newState == 1)
            {
                _models[0].SetAnimation(0, AnimFlags::NoLoop);
                Flags &= ~EnemyFlags::Invincible;
                Enemy19Entity& cretaphid = RequireReference(_cretaphid);
                if (cretaphid.PhaseIndex() == 0)
                {
                    _stateTimer = static_cast<std::int32_t>(ManagedAt(
                        cretaphid.Values().Phase0EyeStateTimer1, EyeIndex)) * 2;
                }
                else if (cretaphid.PhaseIndex() == 1)
                {
                    _stateTimer = static_cast<std::int32_t>(ManagedAt(
                        cretaphid.Values().Phase1EyeStateTimer1, EyeIndex)) * 2;
                }
                else if (cretaphid.PhaseIndex() == 2)
                {
                    _stateTimer = static_cast<std::int32_t>(ManagedAt(
                        cretaphid.Values().Phase2EyeStateTimer1, EyeIndex)) * 2;
                }
            }
            else if (newState == 2)
            {
                _models[0].SetAnimation(0, AnimFlags::NoLoop);
                Flags &= ~EnemyFlags::Invincible;
            }
            else if (newState == 3)
            {
                _models[0].SetAnimation(
                    3, AnimFlags::NoLoop | AnimFlags::Reverse | AnimFlags::Paused);
                Flags |= EnemyFlags::Invincible;
                BeamColliding = false;
                Enemy19Entity& cretaphid = RequireReference(_cretaphid);
                if (cretaphid.PhaseIndex() == 0)
                {
                    _stateTimer = static_cast<std::int32_t>(ManagedAt(
                        cretaphid.Values().Phase0EyeStateTimer2, EyeIndex)) * 2;
                }
                else if (cretaphid.PhaseIndex() == 1)
                {
                    _stateTimer = static_cast<std::int32_t>(ManagedAt(
                        cretaphid.Values().Phase1EyeStateTimer2, EyeIndex)) * 2;
                }
                else if (cretaphid.PhaseIndex() == 2)
                {
                    _stateTimer = static_cast<std::int32_t>(ManagedAt(
                        cretaphid.Values().Phase2EyeStateTimer2, EyeIndex)) * 2;
                }
            }
            else if (newState == 4)
            {
                _models[0].SetAnimation(0, AnimFlags::NoLoop);
                Flags &= ~EnemyFlags::Invincible;
                Enemy19Entity& cretaphid = RequireReference(_cretaphid);
                if (cretaphid.PhaseIndex() == 0)
                {
                    _stateTimer = static_cast<std::int32_t>(ManagedAt(
                        cretaphid.Values().Phase0EyeStateTimer3, EyeIndex)) * 2;
                }
                else if (cretaphid.PhaseIndex() == 1)
                {
                    _stateTimer = static_cast<std::int32_t>(ManagedAt(
                        cretaphid.Values().Phase1EyeStateTimer3, EyeIndex)) * 2;
                }
                else if (cretaphid.PhaseIndex() == 2)
                {
                    _stateTimer = static_cast<std::int32_t>(ManagedAt(
                        cretaphid.Values().Phase2EyeStateTimer3, EyeIndex)) * 2;
                }
            }
            else if (newState == 5)
            {
                _models[0].SetAnimation(
                    3, AnimFlags::NoLoop | AnimFlags::Reverse | AnimFlags::Paused);
                Flags |= EnemyFlags::Invincible;
                Enemy19Entity& cretaphid = RequireReference(_cretaphid);
                if (cretaphid.PhaseIndex() == 0)
                {
                    _stateTimer = static_cast<std::int32_t>(ManagedAt(
                        cretaphid.Values().Phase0EyeStateTimer0, EyeIndex)) * 2;
                }
                else if (cretaphid.PhaseIndex() == 1)
                {
                    _stateTimer = static_cast<std::int32_t>(ManagedAt(
                        cretaphid.Values().Phase1EyeStateTimer0, EyeIndex)) * 2;
                }
                else if (cretaphid.PhaseIndex() == 2)
                {
                    _stateTimer = static_cast<std::int32_t>(ManagedAt(
                        cretaphid.Values().Phase2EyeStateTimer0, EyeIndex)) * 2;
                }
            }
            else if (newState == 6)
            {
                _soundSource.PlayEnvironmentSfx(5);
                _models[0].SetAnimation(
                    3, AnimFlags::NoLoop | AnimFlags::Reverse | AnimFlags::Paused);
                Flags |= EnemyFlags::Invincible;
                BeamColliding = false;
                Enemy19Entity& cretaphid = RequireReference(_cretaphid);
                if (cretaphid.PhaseIndex() == 0)
                {
                    _stateTimer = static_cast<std::int32_t>(ManagedAt(
                        cretaphid.Values().Phase0EyeStateTimer2, EyeIndex)) * 2;
                }
                else if (cretaphid.PhaseIndex() == 1)
                {
                    _stateTimer = static_cast<std::int32_t>(ManagedAt(
                        cretaphid.Values().Phase1EyeStateTimer2, EyeIndex)) * 2;
                }
                else if (cretaphid.PhaseIndex() == 2)
                {
                    _stateTimer = static_cast<std::int32_t>(ManagedAt(
                        cretaphid.Values().Phase2EyeStateTimer2, EyeIndex)) * 2;
                }
            }
            else if (newState == 9)
            {
                _models[0].SetAnimation(5, AnimFlags::NoLoop);
                Flags |= EnemyFlags::Invincible;
            }
        }
        _state1 = _state2 = newState;
    }

    void Enemy20Entity::EnemyProcess()
    {
        if (!EyeActive)
        {
            return;
        }

        auto progressState = [this](std::uint8_t state)
        {
            if (_stateTimer > 0)
            {
                --_stateTimer;
            }
            else
            {
                UpdateState(state);
            }
        };

        if (_state1 == 0)
        {
            progressState(1);
        }
        else if (_state1 == 1)
        {
            progressState(3);
        }
        else if (_state1 == 3)
        {
            if (BeamType == 2)
            {
                RequireReference(_cretaphid).SoundSource().PlayEnvironmentSfx(5);
            }
            else if (BeamType <= 1)
            {
                if (BeamSpawnTimer > 0)
                {
                    --BeamSpawnTimer;
                }
                else if (BeamSpawnCount > 0)
                {
                    BeamSpawnCount = static_cast<std::uint16_t>(
                        static_cast<std::uint32_t>(BeamSpawnCount) - 1U);
                    BeamSpawnTimer = BeamSpawnCooldown;
                    SpawnBeam();
                }
            }
            if (_stateTimer > 0)
            {
                --_stateTimer;
            }
            else
            {
                BeamSpawnTimer = BeamSpawnCooldown;
                RequireReference(_cretaphid).Sub213619C(this);
                UpdateState(4);
            }
        }
        else if (_state1 == 4)
        {
            progressState(0);
        }
        else if (_state1 == 5)
        {
            progressState(6);
        }
        else if (_state1 == 6)
        {
            if (BeamType <= 1)
            {
                if (BeamSpawnTimer > 0)
                {
                    --BeamSpawnTimer;
                }
                else if (BeamSpawnCount > 0)
                {
                    BeamSpawnCount = static_cast<std::uint16_t>(
                        static_cast<std::uint32_t>(BeamSpawnCount) - 1U);
                    BeamSpawnTimer = BeamSpawnCooldown;
                    SpawnBeam();
                }
            }
            if (_stateTimer > 0)
            {
                --_stateTimer;
                RequireReference(_cretaphid).SoundSource().PlayEnvironmentSfx(5);
            }
            else
            {
                BeamSpawnTimer = BeamSpawnCooldown;
                RequireReference(_cretaphid).Sub213619C(this);
                UpdateState(5);
            }
        }
        else if (_state1 == 7)
        {
            if (AnimationEnded(_models[0]))
            {
                _health = 0;
            }
        }
        else if (_state1 == 9)
        {
            if (AnimationEnded(_models[0]))
            {
                RequireReference(_cretaphid).Sub2135F54();
            }
        }
        UpdateTransforms();
        CheckBeamCollision();
    }

    void Enemy20Entity::SpawnBeam()
    {
        assert(BeamType == 0 || BeamType == 1);
        Enemy19Entity& cretaphid = RequireReference(_cretaphid);
        const std::uint16_t damage = ManagedAt(
            cretaphid.Values().EyeBeamDamage, _segmentIndex);

        auto& equipInfos = cretaphid.EquipInfo;
        if (static_cast<std::size_t>(BeamType) >= equipInfos.size())
        {
            throw SceneDetail::IndexOutOfRangeException();
        }
        const std::shared_ptr<EquipInfo> equipInfo
            = equipInfos[static_cast<std::size_t>(BeamType)];
        EquipInfo& equip = RequireReference(equipInfo);
        equip.UnchargedDamage(damage);
        equip.SplashDamage(damage);
        equip.HeadshotDamage(damage);

        const Vector3 facing = FacingVector();
        PlayerEntity& player = MainPlayer();
        const Vector3 spawnDirInitial
            = (AddY(static_cast<Vector3>(player.Position), 0.5F)
                - static_cast<Vector3>(Position)).Normalized();
        Vector3 spawnDir = spawnDirInitial;
        if (Vector3::Dot(facing, spawnDir) < -1.0F)
        {
            spawnDir = facing;
        }
        (void)BeamProjectileEntity::Spawn(
            SharedFrom<EntityBase>(this), equipInfo,
            Position, spawnDir, BeamSpawnFlags::None, NodeRef, _scene);
    }

    void Enemy20Entity::CheckBeamCollision()
    {
        if ((_state1 != 3 && _state1 != 6) || BeamType != 2)
        {
            return;
        }
        PlayerEntity& player = MainPlayer();
        const Vector3 beamCylTop
            = static_cast<Vector3>(Position) + _beamTransform.Row2().Xyz();
        const float radii = player.Volume().SphereRadius
            + Fixed::ToFloat(RequireReference(_cretaphid).Values().CollisionRadius);
        Formats::CollisionResult discard{};
        bool collided = false;
        if (player.IsAltForm())
        {
            collided = Formats::CollisionDetection::CheckCylinderOverlapSphere(
                Position, beamCylTop, player.Volume().SpherePosition,
                radii, discard);
        }
        else
        {
            const Vector3 twoBottom
                = AddY(player.Volume().SpherePosition, -0.5F);
            collided = Formats::CollisionDetection::CheckCylindersOverlap(
                Position, beamCylTop, twoBottom,
                Vector3(0.0F, 1.0F, 0.0F), 2.0F, radii, discard);
        }
        if (collided)
        {
            const std::uint16_t damage = ManagedAt(
                RequireReference(_cretaphid).Values().EyeContactDamage,
                _segmentIndex);
            player.TakeDamage(
                damage, DamageFlags::None, Vector3::Zero, this);
        }
    }

    bool Enemy20Entity::EnemyTakeDamage(EntityBase* source)
    {
        (void)source;
        if (_health == 0)
        {
            _health = 1;
            _state1 = _state2 = 7;
            Flags |= EnemyFlags::Invincible;
            _models[0].SetAnimation(2, AnimFlags::NoLoop);

            ItemType itemType = ItemType::None;
            Enemy19Entity& cretaphid = RequireReference(_cretaphid);
            const Enemy19Values values = cretaphid.Values();
            const std::int32_t chanceTotal
                = static_cast<std::int32_t>(values.ItemChanceHealth)
                + static_cast<std::int32_t>(values.ItemChanceMissile)
                + static_cast<std::int32_t>(values.ItemChanceUa)
                + static_cast<std::int32_t>(values.ItemChanceNone);
            std::uint32_t rand = Rng::GetRandomInt2(chanceTotal);
            if (rand < values.ItemChanceHealth)
            {
                itemType = ItemType::HealthMedium;
            }
            else
            {
                rand -= values.ItemChanceHealth;
                if (rand < values.ItemChanceMissile)
                {
                    itemType = ItemType::MissileSmall;
                }
                else
                {
                    rand -= values.ItemChanceMissile;
                    if (rand < values.ItemChanceUa)
                    {
                        itemType = ItemType::UASmall;
                    }
                }
            }
            if (itemType != ItemType::None)
            {
                (void)ItemSpawnEntity::SpawnItem(
                    itemType, Position, NodeRef, 300 * 2, _scene);
            }
        }
        return false;
    }

    void Enemy20Entity::UpdateTransforms()
    {
        if ((_state1 != 3 && _state1 != 6) || BeamType != 2)
        {
            if (_state1 != 3)
            {
                RequireReference(_cretaphid).UpdateTransforms(false);
                Position = RequireReference(_attachNode).Animation.Row3().Xyz()
                    + static_cast<Vector3>(RequireReference(_cretaphid).Position);
            }
            return;
        }

        RequireReference(_cretaphid).UpdateTransforms(false);
        Matrix4 transform = CreateScale(1.0F, 1.0F, 20.0F);

        if (_segmentIndex < 0
            || static_cast<std::size_t>(_segmentIndex)
                >= RequireReference(_cretaphid).Segments.size())
        {
            throw SceneDetail::IndexOutOfRangeException();
        }
        const float angle = DegreesToRadians(RequireReference(
            RequireReference(_cretaphid).Segments[
                static_cast<std::size_t>(_segmentIndex)]).BeamAngle);
        transform = Multiply(transform, CreateRotationX(angle));
        transform = Multiply(transform, RequireReference(_attachNode).Animation);

        const Vector3 ownerPosition = RequireReference(_cretaphid).Position;
        transform.M41 += ownerPosition.X;
        transform.M42 += ownerPosition.Y;
        transform.M43 += ownerPosition.Z;
        Position = transform.Row3().Xyz();

        const Vector3 pointTwo
            = static_cast<Vector3>(Position) + transform.Row2().Xyz();
        Formats::CollisionResult result{};
        if (Formats::CollisionDetection::CheckBetweenPoints(
            Position, pointTwo, Formats::TestFlags::None, _scene, result))
        {
            _beamCollisionPos = result.Position;
            if (SpawnBurn)
            {
                Vector3 spawnPos = result.Position
                    + Divide(result.Plane.Xyz(), 8.0F);
                Vector3 spawnUp = result.Plane.Xyz();
                if (result.EntityCollision)
                {
                    spawnPos = Matrix::Vec3MultMtx4(
                        spawnPos, result.EntityCollision->Inverse1);
                    spawnUp = Matrix::Vec3MultMtx3(
                        spawnUp, result.EntityCollision->Inverse1);
                }
                const Vector3 spawnFacing = GetCrossVector(spawnUp);
                const Matrix4 spawnTransform
                    = GetTransformMatrix(spawnFacing, spawnUp, spawnPos);
                const std::shared_ptr<BeamEffectEntity> ent
                    = BeamEffectEntity::Create(
                        BeamEffectEntityData(
                            2, false, spawnTransform, result.EntityCollision),
                        _scene);
                if (ent)
                {
                    RequireReference(_scene).AddEntity(ent);
                }
            }
            BeamColliding = true;
        }
        else
        {
            BeamColliding = false;
        }
        SpawnBurn = false;
        _beamTransform = transform;
        RequireReference(_cretaphid).ResetTransforms();
    }

    Vector3 Enemy20Entity::GetCrossVector(Vector3 up)
    {
        if (up.Z <= -0.9F || up.Z >= 0.9F)
        {
            return Vector3::Cross(
                Vector3(1.0F, 0.0F, 0.0F), up).Normalized();
        }
        return Vector3::Cross(
            Vector3(0.0F, 0.0F, 1.0F), up).Normalized();
    }

    bool Enemy20Entity::EnemyGetDrawInfo()
    {
        if ((_state1 == 3 || _state1 == 6) && BeamType == 2)
        {
            ModelInstance& beamModel = RequireReference(
                RequireReference(_cretaphid).BeamModel());
            RequireReference(beamModel.Model()).AnimateNodes(
                0, false, _beamTransform,
                Vector3(1.0F, 1.0F, 1.0F), beamModel.AnimInfo);
            RequireReference(beamModel.Model()).UpdateMatrixStack();
            UpdateMaterials(beamModel, 0);
            GetDrawItems(beamModel, 0);
            if (BeamColliding)
            {
                ModelInstance& beamColModel = RequireReference(
                    RequireReference(_cretaphid).BeamColModel());
                const Matrix4 translation = CreateTranslation(_beamCollisionPos);
                RequireReference(beamColModel.Model()).AnimateNodes(
                    0, false, translation,
                    Vector3(1.0F, 1.0F, 1.0F), beamColModel.AnimInfo);
                RequireReference(beamColModel.Model()).UpdateMatrixStack();
                UpdateMaterials(beamColModel, 0);
                GetDrawItems(beamColModel, 0);
            }
        }

        ModelInstance& model = _models[0];
        if (_timeSinceDamage < 5 * 2)
        {
            for (std::int32_t i = 0;
                i < static_cast<std::int32_t>(RequireReference(
                    RequireReference(model.Model()).Materials).size()); ++i)
            {
                Material& material = RequireReference(RequireReference(
                    RequireReference(model.Model()).Materials)[
                        static_cast<std::size_t>(i)]);
                material.Diffuse = ColorRgb(31, 0, 0);
                material.Lighting = 1;
            }
        }
        RequireReference(model.Model()).AnimateNodes(
            0, false, RequireReference(_attachNode).Animation,
            Vector3(1.0F, 1.0F, 1.0F), model.AnimInfo);
        RequireReference(model.Model()).UpdateMatrixStack();
        UpdateMaterials(model, 0);
        GetDrawItems(model, 0);
        if (_timeSinceDamage < 5 * 2)
        {
            for (std::int32_t i = 0;
                i < static_cast<std::int32_t>(RequireReference(
                    RequireReference(model.Model()).Materials).size()); ++i)
            {
                RequireReference(RequireReference(
                    RequireReference(model.Model()).Materials)[
                        static_cast<std::size_t>(i)]).Lighting = 0;
            }
        }
        return true;
    }
}
