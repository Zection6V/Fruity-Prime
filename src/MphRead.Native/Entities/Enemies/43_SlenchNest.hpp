#pragma once

#include "../../Formats/Culling.hpp"
#include "../EnemyInstanceEntity.hpp"
#include "../EnemySpawnEntity.hpp"

namespace MphRead::Entities::Enemies
{
    class Enemy43Entity : public EnemyInstanceEntity
    {
    public:
        Enemy43Entity(EnemyInstanceEntityData data,
            Formats::Culling::NodeRef nodeRef, Scene* scene);

        Enemy43Entity(const Enemy43Entity&) = delete;
        Enemy43Entity& operator=(const Enemy43Entity&) = delete;
        Enemy43Entity(Enemy43Entity&&) = delete;
        Enemy43Entity& operator=(Enemy43Entity&&) = delete;

    protected:
        void EnemyInitialize() override;

    private:
        EnemySpawnEntity* const _spawner;
    };
}
