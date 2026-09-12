#include "BeamEffectEntity.hpp"

#include "../Program.hpp"
#include "../Scene.hpp"

#include <bit>
#include <cmath>
#include <cstdint>
#include <memory>
#include <new>
#include <utility>

namespace
{
    using OpenTK::Mathematics::Matrix4;
    using OpenTK::Mathematics::Vector3;

    [[nodiscard]] float Length(Vector3 value) noexcept
    {
        return std::sqrt(value.X * value.X + value.Y * value.Y + value.Z * value.Z);
    }

    [[nodiscard]] Vector3 ExtractScale(Matrix4 value) noexcept
    {
        return Vector3(
            Length(Vector3(value.M11, value.M12, value.M13)),
            Length(Vector3(value.M21, value.M22, value.M23)),
            Length(Vector3(value.M31, value.M32, value.M33)));
    }

    [[nodiscard]] Matrix4 ClearScale(Matrix4 value) noexcept
    {
        const Vector3 scale = ExtractScale(value);
        value.M11 /= scale.X;
        value.M12 /= scale.X;
        value.M13 /= scale.X;
        value.M21 /= scale.Y;
        value.M22 /= scale.Y;
        value.M23 /= scale.Y;
        value.M31 /= scale.Z;
        value.M32 /= scale.Z;
        value.M33 /= scale.Z;
        return value;
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

    [[nodiscard]] std::int32_t ManagedFrameLifespan(std::int32_t frameCount) noexcept
    {
        const std::uint32_t value
            = (std::bit_cast<std::uint32_t>(frameCount) - 1U) * 2U;
        return std::bit_cast<std::int32_t>(value);
    }

    [[nodiscard]] std::int32_t ManagedPostDecrement(std::int32_t& value) noexcept
    {
        const std::int32_t previous = value;
        const std::uint32_t decremented = std::bit_cast<std::uint32_t>(value) - 1U;
        value = std::bit_cast<std::int32_t>(decremented);
        return previous;
    }
}

namespace MphRead::Entities
{
    BeamEffectEntityData::BeamEffectEntityData(
        std::int32_t type,
        bool noSplat,
        Matrix4 transform,
        std::shared_ptr<::MphRead::Formats::Collision::EntityCollision> entCol) noexcept
        : Type(type),
          NoSplat(noSplat),
          Transform(transform),
          EntityCollision(std::move(entCol))
    {
    }

    BeamEffectEntityData& BeamEffectEntityData::operator=(
        const BeamEffectEntityData& other) noexcept
    {
        if (this != std::addressof(other))
        {
            this->~BeamEffectEntityData();
            ::new (static_cast<void*>(this)) BeamEffectEntityData(other);
        }
        return *this;
    }

    BeamEffectEntity::BeamEffectEntity(Scene* scene)
        : EntityBase(EntityType::BeamEffect, scene)
    {
    }

    void BeamEffectEntity::Spawn(BeamEffectEntityData data)
    {
        _models = ModelList{};
        ModelInstance* model;
        if (data.Type == 0)
        {
            model = std::addressof(SetUpModel("iceWave", 0, AnimFlags::NoLoop));
        }
        else if (data.Type == 1)
        {
            model = std::addressof(SetUpModel("sniperBeam", 0, AnimFlags::NoLoop));
        }
        else if (data.Type == 2)
        {
            model = std::addressof(SetUpModel("cylBossLaserBurn"));
        }
        else
        {
            throw ProgramException("Invalid beam effect type.");
        }

        _lifespan = 0;
        const std::shared_ptr<MphRead::Model> modelDefinition = model->Model();
        const MphRead::Model& definition = RequireReference(modelDefinition);
        const MphRead::AnimationGroups& groups = RequireReference(definition.AnimationGroups);
        const auto& nodeGroups = RequireReference(groups.Node);
        if (!nodeGroups.empty() && data.Type != 2)
        {
            const MphRead::NodeAnimationGroup& group = RequireReference(nodeGroups[0]);
            _lifespan = ManagedFrameLifespan(group.FrameCount);
        }
        else
        {
            const auto& materialGroups = RequireReference(groups.Material);
            if (!materialGroups.empty())
            {
                const MphRead::MaterialAnimationGroup& group
                    = RequireReference(materialGroups[0]);
                _lifespan = ManagedFrameLifespan(group.FrameCount);
            }
        }

        Transform = data.Transform;
        if (data.Type == 0)
        {
            RequireReference(_scene).SpawnEffect(78, ClearScale(data.Transform));
        }
    }

    void BeamEffectEntity::Reposition(Vector3 offset)
    {
        Position = static_cast<Vector3>(Position) + offset;
    }

    bool BeamEffectEntity::Process()
    {
        if (ManagedPostDecrement(_lifespan) <= 0)
        {
            return false;
        }
        return EntityBase::Process();
    }

    void BeamEffectEntity::Destroy()
    {
        RequireReference(_scene).UnlinkBeamEffect(this);
        EntityBase::Destroy();
    }

    std::shared_ptr<BeamEffectEntity> BeamEffectEntity::Create(
        BeamEffectEntityData data, Scene* scene)
    {
        if (data.Type >= 3)
        {
            std::int32_t effectId = data.Type - 3;
            if (data.NoSplat)
            {
                if (effectId == 1)
                {
                    effectId = 2;
                }
                else if (effectId == 92)
                {
                    effectId = 98;
                }
            }
            RequireReference(scene).SpawnEffect(
                effectId, data.Transform, false, data.EntityCollision);
            return nullptr;
        }
        return RequireReference(scene).InitBeamEffect(data);
    }
}
