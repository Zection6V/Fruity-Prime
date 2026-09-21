#include "32_GoreaSealSphere2.hpp"

#include "../../Formats/CollisionDetection.hpp"
#include "../../MemoryArrays.hpp"
#include "../../Metadata/Enemies.hpp"
#include "../../Scene.hpp"
#include "../BeamProjectileEntity.hpp"
#include "../Players/PlayerEntity.hpp"

#include <bit>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

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

        template <typename T>
        [[nodiscard]] T& CastReference(EntityBase* value)
        {
            T* cast = dynamic_cast<T*>(value);
            if (cast == nullptr)
            {
                throw SceneDetail::InvalidCastException();
            }
            return *cast;
        }

        template <typename T>
        [[nodiscard]] const T& ManagedListAt(
            const std::vector<T>& values, std::int32_t index)
        {
            if (index < 0 || static_cast<std::size_t>(index) >= values.size())
            {
                throw Memory::Detail::ArgumentOutOfRangeException();
            }
            return values[static_cast<std::size_t>(index)];
        }

        [[nodiscard]] std::int32_t ManagedAdd(
            std::int32_t left, std::int32_t right) noexcept
        {
            const std::uint32_t value = std::bit_cast<std::uint32_t>(left)
                + std::bit_cast<std::uint32_t>(right);
            return std::bit_cast<std::int32_t>(value);
        }

        [[nodiscard]] std::int32_t ManagedSubtract(
            std::int32_t left, std::int32_t right) noexcept
        {
            const std::uint32_t value = std::bit_cast<std::uint32_t>(left)
                - std::bit_cast<std::uint32_t>(right);
            return std::bit_cast<std::int32_t>(value);
        }

        [[nodiscard]] bool TestFlag(Gorea2Flags value, Gorea2Flags flag) noexcept
        {
            return (value & flag) == flag;
        }

        [[nodiscard]] PlayerEntity& MainPlayer()
        {
            return RequireReference(PlayerEntity::Main());
        }
    }

    Enemy32Entity::Enemy32Entity(EnemyInstanceEntityData data,
        Formats::Culling::NodeRef nodeRef, Scene* scene)
        : GoreaEnemyEntityBase(data, nodeRef, scene)
    {
    }

    Node* Enemy32Entity::AttachNode() const noexcept
    {
        return _attachNode.get();
    }

    std::int32_t Enemy32Entity::Damage() const noexcept
    {
        return _damage;
    }

    void Enemy32Entity::SetDamage(std::int32_t value) noexcept
    {
        _damage = value;
    }

    std::int32_t Enemy32Entity::DamageTimer() const noexcept
    {
        return _damageTimer;
    }

    bool Enemy32Entity::Visible() const noexcept
    {
        return _visible;
    }

    bool Enemy32Entity::Targetable() const noexcept
    {
        return _targetable;
    }

    void Enemy32Entity::EnemyInitialize()
    {
        if (Enemy31Entity* owner = dynamic_cast<Enemy31Entity*>(_owner))
        {
            _gorea2 = owner;
            Flags &= ~EnemyFlags::Visible;
            Flags &= ~EnemyFlags::NoHomingNc;
            Flags &= ~EnemyFlags::NoHomingCo;
            Flags |= EnemyFlags::Invincible;
            Flags |= EnemyFlags::CollidePlayer;
            Flags |= EnemyFlags::CollideBeam;
            Flags |= EnemyFlags::NoMaxDistance;
            _state1 = _state2 = 255;
            SetHealthbarMessageId(3);

            ModelInstance& ownerModel
                = RequireReference(ManagedListAt(owner->GetModels(), 0));
            Model& model = RequireReference(ownerModel.Model());
            _attachNode = model.GetNodeByName("ChestBall1");

            const Vector3 facing = owner->FacingVector();
            const Vector3 up = owner->UpVector();
            const Vector3 position = owner->Position;
            SetTransform(facing, up, position);
            Position = static_cast<Vector3>(Position)
                + RequireReference(_attachNode).Position;
            _prevPos = Position;
            _boundingRadius = 1.0F;
            _hurtVolumeInit = CollisionVolume(Vector3::Zero, owner->Scale.X);
            _health = 65535;
            _healthMax = 840;
            Metadata::LoadEffectiveness(
                MphRead::EnemyType::GoreaSealSphere2, BeamEffectiveness);
        }
    }

    void Enemy32Entity::EnemyProcess()
    {
        Enemy31Entity& gorea = RequireReference(_gorea2);
        if (TypeExtensions::TestFlag(
            static_cast<EnemyFlags>(gorea.Flags), EnemyFlags::Visible))
        {
            const Matrix4 transform
                = GetNodeTransform(_gorea2, _attachNode.get());
            Position = transform.Row3().Xyz();
        }
        if (_damageTimer > 0)
        {
            --_damageTimer;
        }
        _targetable = _visible && IsVisible(NodeRef);
    }

    void Enemy32Entity::UpdateVisibility()
    {
        Formats::CollisionResult discard{};
        _visible = !Formats::CollisionDetection::CheckBetweenPoints(
            Position, RequireReference(MainPlayer().CameraInfo()).Position,
            Formats::TestFlags::None, _scene, discard);
    }

    bool Enemy32Entity::EnemyTakeDamage(EntityBase* source)
    {
        bool ignoreDamage = false;
        bool isOmegaCannon = false;
        if (source != nullptr && source->Type == EntityType::BeamProjectile)
        {
            BeamProjectileEntity& beamSource
                = CastReference<BeamProjectileEntity>(source);
            isOmegaCannon = beamSource.BeamKind() == BeamType::OmegaCannon;
        }

        std::int32_t damage
            = 65535 - static_cast<std::int32_t>(_health);
        if ((_damage >= 720
                && (!isOmegaCannon
                    || !TestFlag(RequireReference(_gorea2).GoreaFlags,
                        Gorea2Flags::Bit9)))
            || (source != nullptr && source->Type == EntityType::Bomb))
        {
            damage = 0;
            ignoreDamage = true;
        }

        if (isOmegaCannon)
        {
            RequireReference(_gorea2).GoreaFlags |= Gorea2Flags::Bit9;
        }

        if (TypeExtensions::TestFlag(
                static_cast<EnemyFlags>(Flags), EnemyFlags::Invincible)
            || RequireReference(_gorea2).Func214080C())
        {
            damage = 0;
            ignoreDamage = true;
        }
        else
        {
            _damage = ManagedAdd(_damage, damage);
            RequireReference(_gorea2).UpdatePhase();
        }

        _health = 65535;
        RequireReference(_gorea2).GoreaFlags &= ~Gorea2Flags::LaserActive;
        _soundSource.StopSfx(SfxId::GOREA2_ATTACK1B);

        if (_damage >= 840)
        {
            RequireReference(_gorea2).GoreaFlags |= Gorea2Flags::Bit11;
        }
        else if (damage != 0
            && !TypeExtensions::TestFlag(
                static_cast<EnemyFlags>(Flags), EnemyFlags::Invincible))
        {
            bool spawnEffect = false;
            if (damage >= 120)
            {
                spawnEffect = true;
            }
            else
            {
                const std::uint32_t field244
                    = static_cast<std::uint32_t>(
                        RequireReference(_gorea2).Field244());
                std::int32_t index
                    = static_cast<std::int32_t>(std::countr_zero(field244));
                if (index > 6)
                {
                    index = 6;
                }
                const std::int32_t diff = ManagedSubtract(
                    _damage, 120 * index);
                for (std::int32_t i = 1; i < 3; ++i)
                {
                    if (ManagedSubtract(diff, damage) < i * 40
                        && i * 40 <= diff)
                    {
                        spawnEffect = true;
                        break;
                    }
                }
            }

            if (spawnEffect)
            {
                RequireReference(_gorea2).GoreaFlags |= Gorea2Flags::Bit16;
                SpawnEffect(44, Position);
                RequireReference(_gorea2).GoreaFlags |= Gorea2Flags::Bit13;
                Flags |= EnemyFlags::Invincible;
            }

            _soundSource.PlaySfx(SfxId::GOREA2_DAMAGE1);
            _damageTimer = 10 * 2;

            Enemy31Entity& gorea = RequireReference(_gorea2);
            ModelInstance& ownerModel
                = RequireReference(ManagedListAt(gorea.GetModels(), 0));
            Model& model = RequireReference(ownerModel.Model());
            Material& material
                = RequireReference(model.GetMaterialByName("ChestCore"));
            material.Diffuse = ColorRgb(31, 0, 0);
        }

        return ignoreDamage;
    }

    void Enemy32Entity::SetDead()
    {
        _scanId = 0;
        Flags &= ~EnemyFlags::CollidePlayer;
        Flags &= ~EnemyFlags::CollideBeam;
        Flags |= EnemyFlags::Invincible;
        Flags |= EnemyFlags::NoHomingNc;
        Flags |= EnemyFlags::NoHomingCo;
    }
}
