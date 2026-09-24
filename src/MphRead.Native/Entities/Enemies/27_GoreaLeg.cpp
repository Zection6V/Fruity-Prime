#include "27_GoreaLeg.hpp"

#include "../../MemoryArrays.hpp"
#include "../../Scene.hpp"
#include "../Players/PlayerEntity.hpp"
#include "../../NativeRuntime/System/Managed.hpp"
#include "../../Formats/Types.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

using ::MphRead::NativeRuntime::RequireReference;
using ::OpenTK::Mathematics::LengthSquared;

namespace MphRead::Entities::Enemies
{
    namespace
    {
        using OpenTK::Mathematics::Matrix4;
        using OpenTK::Mathematics::Vector3;

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

        template <typename T, std::size_t N>
        [[nodiscard]] T& ManagedArrayAt(
            std::array<T, N>& values, std::int32_t index)
        {
            if (index < 0 || static_cast<std::size_t>(index) >= N)
            {
                throw SceneDetail::IndexOutOfRangeException();
            }
            return values[static_cast<std::size_t>(index)];
        }

        [[nodiscard]] Vector3 ScaleVector(Vector3 value, float scale) noexcept
        {
            return Vector3(
                value.X * scale,
                value.Y * scale,
                value.Z * scale);
        }

        [[nodiscard]] PlayerEntity& MainPlayer()
        {
            return RequireReference(PlayerEntity::Main());
        }
    }

    Enemy27Entity::Enemy27Entity(EnemyInstanceEntityData data,
        Formats::Culling::NodeRef nodeRef, Scene* scene)
        : GoreaEnemyEntityBase(data, nodeRef, scene)
    {
    }

    void Enemy27Entity::EnemyInitialize()
    {
        if (Enemy24Entity* owner = dynamic_cast<Enemy24Entity*>(_owner))
        {
            _gorea1A = owner;
            InitializeCommon(owner->Spawner());
            Flags &= ~EnemyFlags::Visible;
            Flags |= EnemyFlags::Invincible;
            _state1 = _state2 = 255;
            _prevPos = Position;

            const Vector3 facing = owner->FacingVector();
            const Vector3 up = owner->UpVector();
            const Vector3 position = Position;
            SetTransform(facing, up, position);

            _hurtVolumeInit = CollisionVolume(
                Vector3(0.0F, 1.0F, 0.0F),
                Vector3::Zero,
                Fixed::ToFloat(736),
                Fixed::ToFloat(9700));
            _health = 65535;
            _healthMax = 120;
            SetKneeNode(owner);
        }
    }

    void Enemy27Entity::SetKneeNode(GoreaEnemyEntityBase* parent)
    {
        std::string nodeName;
        if (Index == 2)
        {
            nodeName = "BK_Knee";
        }
        else if (Index == 1)
        {
            nodeName = "R_Knee";
        }
        else
        {
            nodeName = "L_Knee";
        }

        GoreaEnemyEntityBase& parentRef = RequireReference(parent);
        ModelInstance& ownerModel
            = RequireReference(ManagedListAt(parentRef.GetModels(), 0));
        Model& model = RequireReference(ownerModel.Model());
        _kneeNode = model.GetNodeByName(nodeName);

        const Matrix4 transform = GetNodeTransform(parent, _kneeNode.get());
        Position = transform.Row3().Xyz();
    }

    void Enemy27Entity::EnemyProcess()
    {
        const Matrix4 transform
            = GetNodeTransform(_gorea1A, _kneeNode.get());
        Position = transform.Row3().Xyz();

        Vector3 cylinderVec = transform.Row0().Xyz().Normalized();
        if (Index != 1)
        {
            cylinderVec = ScaleVector(cylinderVec, -1.0F);
        }

        const Vector3 cylinderPos
            = ScaleVector(cylinderVec, Fixed::ToFloat(-9700));
        _hurtVolumeInit = CollisionVolume(
            cylinderVec,
            cylinderPos,
            _hurtVolumeInit.CylinderRadius,
            _hurtVolumeInit.CylinderDot);

        CheckPlayerCollision(0.25F, 10);
    }

    void Enemy27Entity::CheckPlayerCollision(float factor, std::int32_t damage)
    {
        if (!ManagedArrayAt(HitPlayers, MainPlayer().SlotIndex()))
        {
            return;
        }

        Vector3 between = TypeExtensions::WithY(
            static_cast<Vector3>(MainPlayer().Position)
                - static_cast<Vector3>(Position),
            0.0F);
        between = LengthSquared(between) > 1.0F / 128.0F
            ? between.Normalized()
            : FacingVector();

        PlayerEntity& speedPlayer = MainPlayer();
        const Vector3 speed = speedPlayer.Speed();
        const Vector3 speedDelta = ScaleVector(between, factor);
        speedPlayer.SetSpeed(speed + speedDelta);

        MainPlayer().TakeDamage(
            damage, DamageFlags::None, std::nullopt, this);
    }

    bool Enemy27Entity::EnemyTakeDamage(EntityBase* source)
    {
        (void)source;
        _health = 65535;
        return false;
    }
}
