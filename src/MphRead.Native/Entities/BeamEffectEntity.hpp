#pragma once

#include "EntityBase.hpp"

#include <cstdint>
#include <memory>

namespace MphRead::Formats::Collision
{
    class EntityCollision;
}

namespace MphRead::Entities
{
    struct BeamEffectEntityData final
    {
        const std::int32_t Type = 0;
        const bool NoSplat = false;
        const ::OpenTK::Mathematics::Matrix4 Transform{};
        const std::shared_ptr<::MphRead::Formats::Collision::EntityCollision> EntityCollision{};

        BeamEffectEntityData() noexcept = default;
        BeamEffectEntityData(
            std::int32_t type,
            bool noSplat,
            ::OpenTK::Mathematics::Matrix4 transform,
            std::shared_ptr<::MphRead::Formats::Collision::EntityCollision> entCol = nullptr) noexcept;

        BeamEffectEntityData(const BeamEffectEntityData&) noexcept = default;
        BeamEffectEntityData& operator=(const BeamEffectEntityData& other) noexcept;
    };

    class BeamEffectEntity : public EntityBase
    {
    public:
        explicit BeamEffectEntity(Scene* scene);

        BeamEffectEntity(const BeamEffectEntity&) = delete;
        BeamEffectEntity& operator=(const BeamEffectEntity&) = delete;
        BeamEffectEntity(BeamEffectEntity&&) = delete;
        BeamEffectEntity& operator=(BeamEffectEntity&&) = delete;

        void Spawn(BeamEffectEntityData data);
        void Reposition(::OpenTK::Mathematics::Vector3 offset);
        [[nodiscard]] bool Process() override;
        void Destroy() override;

        [[nodiscard]] static std::shared_ptr<BeamEffectEntity> Create(
            BeamEffectEntityData data, Scene* scene);

    private:
        std::int32_t _lifespan = 0;
    };
}
