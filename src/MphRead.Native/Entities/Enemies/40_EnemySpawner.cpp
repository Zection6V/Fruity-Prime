#include "40_EnemySpawner.hpp"

#include "../../Formats/Collision.hpp"
#include "../../Scene.hpp"
#include "../EnemySpawnEntity.hpp"
#include "../../NativeRuntime/System/Managed.hpp"
#include "../../Formats/Types.hpp"

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <type_traits>

using ::MphRead::NativeRuntime::RequireReference;
using ::MphRead::TestFlag;

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

        [[nodiscard]] Matrix4 Multiply(Matrix4 left, Matrix4 right) noexcept
        {
            return Matrix4(
                Vector4(
                    left.M11 * right.M11 + left.M12 * right.M21 + left.M13 * right.M31 + left.M14 * right.M41,
                    left.M11 * right.M12 + left.M12 * right.M22 + left.M13 * right.M32 + left.M14 * right.M42,
                    left.M11 * right.M13 + left.M12 * right.M23 + left.M13 * right.M33 + left.M14 * right.M43,
                    left.M11 * right.M14 + left.M12 * right.M24 + left.M13 * right.M34 + left.M14 * right.M44),
                Vector4(
                    left.M21 * right.M11 + left.M22 * right.M21 + left.M23 * right.M31 + left.M24 * right.M41,
                    left.M21 * right.M12 + left.M22 * right.M22 + left.M23 * right.M32 + left.M24 * right.M42,
                    left.M21 * right.M13 + left.M22 * right.M23 + left.M23 * right.M33 + left.M24 * right.M43,
                    left.M21 * right.M14 + left.M22 * right.M24 + left.M23 * right.M34 + left.M24 * right.M44),
                Vector4(
                    left.M31 * right.M11 + left.M32 * right.M21 + left.M33 * right.M31 + left.M34 * right.M41,
                    left.M31 * right.M12 + left.M32 * right.M22 + left.M33 * right.M32 + left.M34 * right.M42,
                    left.M31 * right.M13 + left.M32 * right.M23 + left.M33 * right.M33 + left.M34 * right.M43,
                    left.M31 * right.M14 + left.M32 * right.M24 + left.M33 * right.M34 + left.M34 * right.M44),
                Vector4(
                    left.M41 * right.M11 + left.M42 * right.M21 + left.M43 * right.M31 + left.M44 * right.M41,
                    left.M41 * right.M12 + left.M42 * right.M22 + left.M43 * right.M32 + left.M44 * right.M42,
                    left.M41 * right.M13 + left.M42 * right.M23 + left.M43 * right.M33 + left.M44 * right.M43,
                    left.M41 * right.M14 + left.M42 * right.M24 + left.M43 * right.M34 + left.M44 * right.M44));
        }

        [[nodiscard]] AnimFlags AnimationFlagsAt(ModelInstance& model, std::int32_t index)
        {
            AnimationInfo& animInfo = RequireReference(model.AnimInfo);
            ManagedArray<AnimFlags>& flags = RequireReference(animInfo.Flags);
            if (index < 0 || static_cast<std::size_t>(index) >= flags.Length())
            {
                throw SceneDetail::IndexOutOfRangeException();
            }
            return flags[static_cast<std::size_t>(index)];
        }

        [[nodiscard]] std::int32_t AnimationIndexAt(ModelInstance& model, std::int32_t index)
        {
            AnimationInfo& animInfo = RequireReference(model.AnimInfo);
            ManagedArray<std::int32_t>& indices = RequireReference(animInfo.Index);
            if (index < 0 || static_cast<std::size_t>(index) >= indices.Length())
            {
                throw SceneDetail::IndexOutOfRangeException();
            }
            return indices[static_cast<std::size_t>(index)];
        }
    }

    Enemy40Entity::Enemy40Entity(EnemyInstanceEntityData data,
        Formats::Culling::NodeRef nodeRef, Scene* scene)
        : EnemyInstanceEntity(data, nodeRef, scene),
          _spawner(CastSpawner(data.Spawner))
    {
    }

    Enemy40Entity::SpawnerModelType Enemy40Entity::ModelType() const noexcept
    {
        return _modelType;
    }

    void Enemy40Entity::EnemyInitialize()
    {
        Transform = RequireReference(_data.Spawner).Transform;

        EnemySpawnEntity* spawner = dynamic_cast<EnemySpawnEntity*>(_data.Spawner);
        if (spawner != nullptr && spawner->ParentEntCol)
        {
            _parentEntCol = spawner->ParentEntCol;
            _invTransform = Multiply(_transform, spawner->ParentEntCol->Inverse2);
        }

        _boundingRadius = Fixed::ToFloat(3072);
        _hurtVolumeInit = CollisionVolume(Vector3::Zero, _boundingRadius);
        _hurtVolume = CollisionVolume::Transform(_hurtVolumeInit, Transform);

        EnemySpawnEntity& ownSpawner = RequireReference(_spawner);
        _healthMax = _health = ownSpawner.Data.SpawnerHealth;
        Flags |= EnemyFlags::Visible;
        if (!_parentEntCol)
        {
            Flags |= EnemyFlags::Static;
        }

        std::string model = "EnemySpawner";
        if (ownSpawner.Data.EnemyType == EnemyType::WarWasp
            || ownSpawner.Data.EnemyType == EnemyType::BarbedWarWasp)
        {
            model = "PlantCarnivarous_Pod";
            _modelType = SpawnerModelType::Nest;
        }

        ModelInstance& inst = SetUpModel(model);
        if (ownSpawner.Data.EnemyType != EnemyType::WarWasp
            && ownSpawner.Data.EnemyType != EnemyType::BarbedWarWasp)
        {
            if (TestFlag(ownSpawner.Flags, SpawnerFlags::Active))
            {
                inst.SetAnimation(1);
            }
            else
            {
                inst.SetAnimation(2, AnimFlags::Paused | AnimFlags::Ended);
            }
        }
        else
        {
            inst.SetAnimation(0);
        }
        ownSpawner.Flags |= SpawnerFlags::HasModel;
    }

    void Enemy40Entity::EnemyProcess()
    {
        if (_parentEntCol)
        {
            Transform = Multiply(_invTransform, _parentEntCol->Transform);
        }

        EnemySpawnEntity& spawner = RequireReference(_spawner);
        if (TestFlag(spawner.Flags, SpawnerFlags::Active)
            && !TestFlag(spawner.Flags, SpawnerFlags::Suspended))
        {
            Flags &= ~EnemyFlags::Invincible;
        }
        else
        {
            Flags |= EnemyFlags::Invincible;
        }

        if (TestFlag(spawner.Flags, SpawnerFlags::PlayAnimation))
        {
            spawner.Flags &= ~SpawnerFlags::PlayAnimation;
            if (_animTimer == 0)
            {
                _soundSource.PlaySfx(SfxId::ENEMY_SPAWNER_SPAWN);
            }
            _animTimer = 15 * 2;
            if (_modelType == SpawnerModelType::Spawner)
            {
                _models[0].SetAnimation(2, AnimFlags::NoLoop);
            }
        }
        else if (_modelType == SpawnerModelType::Spawner)
        {
            if (TestFlag(static_cast<EnemyFlags>(Flags), EnemyFlags::Invincible))
            {
                _models[0].SetAnimation(2, AnimFlags::Ended | AnimFlags::Paused);
            }
            else if (TestFlag(AnimationFlagsAt(_models[0], 0), AnimFlags::Ended)
                && AnimationIndexAt(_models[0], 0) != 1)
            {
                if (AnimationIndexAt(_models[0], 0) != 0)
                {
                    _models[0].SetAnimation(1);
                }
                else
                {
                    _health = 0;
                }
            }
        }

        if (_animTimer > 0)
        {
            _animTimer--;
        }
    }

    bool Enemy40Entity::EnemyTakeDamage(EntityBase* source)
    {
        (void)source;
        if (_health == 0 && _modelType == SpawnerModelType::Spawner)
        {
            if (AnimationIndexAt(_models[0], 0) != 0)
            {
                _models[0].SetAnimation(0, AnimFlags::NoLoop);
            }
            _health = 1;
        }
        return false;
    }
}
