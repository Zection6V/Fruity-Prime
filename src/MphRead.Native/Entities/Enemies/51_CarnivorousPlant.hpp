#pragma once

#include "../../Formats/Culling.hpp"
#include "../EnemyInstanceEntity.hpp"

namespace MphRead::Entities
{
    class EnemySpawnEntity;
}

namespace MphRead::Entities::Enemies
{
    class Enemy51Entity : public EnemyInstanceEntity
    {
    public:
        Enemy51Entity(EnemyInstanceEntityData data,
            Formats::Culling::NodeRef nodeRef, Scene* scene);

        Enemy51Entity(const Enemy51Entity&) = delete;
        Enemy51Entity& operator=(const Enemy51Entity&) = delete;
        Enemy51Entity(Enemy51Entity&&) = delete;
        Enemy51Entity& operator=(Enemy51Entity&&) = delete;

    protected:
        void EnemyInitialize() override;
        void EnemyProcess() override;

    private:
        EnemySpawnEntity* const _spawner;
    };
}
