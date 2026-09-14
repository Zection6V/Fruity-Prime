#pragma once

#include "../../Formats/Culling.hpp"
#include "../EnemyInstanceEntity.hpp"

#include <cstdint>

namespace MphRead::Entities::Enemies
{
    class Enemy50Entity : public EnemyInstanceEntity
    {
    public:
        Enemy50Entity(EnemyInstanceEntityData data,
            Formats::Culling::NodeRef nodeRef, Scene* scene);

        Enemy50Entity(const Enemy50Entity&) = delete;
        Enemy50Entity& operator=(const Enemy50Entity&) = delete;
        Enemy50Entity(Enemy50Entity&&) = delete;
        Enemy50Entity& operator=(Enemy50Entity&&) = delete;

        void SetUp(std::uint16_t health, CollisionVolume hurtVolume, float boundingRadius);
        void HandleMessage(MessageInfo info) override;

    protected:
        void EnemyProcess() override;
        [[nodiscard]] bool EnemyTakeDamage(EntityBase* source) override;

    private:
        EnemyInstanceEntity* const _enemyOwner;
    };
}
