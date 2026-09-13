#include "25_GoreaHead.hpp"

#include "../../Formats/Effects.hpp"
#include "../../Scene.hpp"

#include <cstdint>
#include <memory>

namespace MphRead::Entities::Enemies
{
    namespace
    {
        using OpenTK::Mathematics::Matrix4;
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

        [[nodiscard]] Vector3 Scale(Vector3 value, float scale) noexcept
        {
            return Vector3(
                value.X * scale,
                value.Y * scale,
                value.Z * scale);
        }
    }

    Enemy25Entity::Enemy25Entity(EnemyInstanceEntityData data,
        Formats::Culling::NodeRef nodeRef, Scene* scene)
        : GoreaEnemyEntityBase(data, nodeRef, scene)
    {
    }

    void Enemy25Entity::EnemyInitialize()
    {
        if (Enemy24Entity* owner = dynamic_cast<Enemy24Entity*>(_owner))
        {
            _gorea1A = owner;
            InitializeCommon(owner->Spawner());
            Flags |= EnemyFlags::Invincible;
            Flags &= ~EnemyFlags::Visible;
            _state1 = _state2 = 255;

            const auto& models = _owner->GetModels();
            if (models.empty())
            {
                throw SceneDetail::IndexOutOfRangeException();
            }
            ModelInstance& ownerModel = RequireReference(models[0]);
            Model& model = RequireReference(ownerModel.Model());
            _attachNode = model.GetNodeByName("Head");

            Position = static_cast<Vector3>(Position)
                + RequireReference(_attachNode).Position;
            _prevPos = Position;
            SetTransform(
                owner->FacingVector(), owner->UpVector(), Position);
            _hurtVolumeInit = CollisionVolume(
                Vector3::Zero, Fixed::ToFloat(1314));
        }
    }

    void Enemy25Entity::EnemyProcess()
    {
        Matrix4 transform = GetNodeTransform(_gorea1A, _attachNode);
        Position = transform.Row3().Xyz();

        if (_flashEffect)
        {
            if (_flashEffect->IsFinished())
            {
                RemoveFlashEffect();
            }
            else
            {
                Enemy24Entity& gorea = RequireReference(_gorea1A);
                Vector3 position = static_cast<Vector3>(Position)
                    + Scale(gorea.FacingVector(), Fixed::ToFloat(2949));
                position = position
                    + Scale(gorea.UpVector(), Fixed::ToFloat(-939));
                _flashEffect->Transform(
                    gorea.FacingVector(), gorea.UpVector(), position);
            }
        }
    }

    void Enemy25Entity::RemoveFlashEffect()
    {
        if (_flashEffect)
        {
            RequireReference(_scene).DetachEffectEntry(
                _flashEffect, false);
            _flashEffect.reset();
        }
    }

    void Enemy25Entity::RespawnFlashEffect()
    {
        RemoveFlashEffect();
        Enemy24Entity& gorea = RequireReference(_gorea1A);
        Vector3 spawnPos = static_cast<Vector3>(Position)
            + Scale(gorea.FacingVector(), Fixed::ToFloat(2949));
        spawnPos = spawnPos
            + Scale(gorea.UpVector(), Fixed::ToFloat(-939));
        _flashEffect = SpawnEffectGetEntry(104, spawnPos, false);
    }

    bool Enemy25Entity::EnemyTakeDamage(EntityBase* source)
    {
        (void)source;
        Damage = 65535 - static_cast<std::int32_t>(_health);
        _health = 65535;
        return true;
    }
}
