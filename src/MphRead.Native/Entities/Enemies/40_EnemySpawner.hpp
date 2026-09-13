#pragma once

#include "../../Formats/Culling.hpp"
#include "../EnemyInstanceEntity.hpp"

#include <cstdint>
#include <memory>

namespace MphRead
{
    namespace Formats::Collision
    {
        class EntityCollision;
    }
}

namespace MphRead::Entities
{
    class EnemySpawnEntity;
}

namespace MphRead::Entities::Enemies
{
    class Enemy40Entity : public EnemyInstanceEntity
    {
    public:
        enum class SpawnerModelType : std::int32_t
        {
            Spawner,
            Nest
        };

        Enemy40Entity(EnemyInstanceEntityData data,
            Formats::Culling::NodeRef nodeRef, Scene* scene);

        Enemy40Entity(const Enemy40Entity&) = delete;
        Enemy40Entity& operator=(const Enemy40Entity&) = delete;
        Enemy40Entity(Enemy40Entity&&) = delete;
        Enemy40Entity& operator=(Enemy40Entity&&) = delete;

        [[nodiscard]] SpawnerModelType ModelType() const noexcept;

    protected:
        void EnemyInitialize() override;
        void EnemyProcess() override;
        [[nodiscard]] bool EnemyTakeDamage(EntityBase* source) override;

    private:
        EnemySpawnEntity* const _spawner;
        std::uint8_t _animTimer = 0;
        std::shared_ptr<Formats::Collision::EntityCollision> _parentEntCol{};
        OpenTK::Mathematics::Matrix4 _invTransform{
            OpenTK::Mathematics::Vector4(1.0F, 0.0F, 0.0F, 0.0F),
            OpenTK::Mathematics::Vector4(0.0F, 1.0F, 0.0F, 0.0F),
            OpenTK::Mathematics::Vector4(0.0F, 0.0F, 1.0F, 0.0F),
            OpenTK::Mathematics::Vector4(0.0F, 0.0F, 0.0F, 1.0F)};
        SpawnerModelType _modelType = SpawnerModelType::Spawner;
    };
}
