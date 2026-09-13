#pragma once

#include "../../Formats/Culling.hpp"
#include "../EnemyInstanceEntity.hpp"

#include <cstdint>

namespace MphRead::Entities
{
    class EnemySpawnEntity;
}

namespace MphRead::Entities::Enemies
{
    class Enemy01Entity : public EnemyInstanceEntity
    {
    public:
        Enemy01Entity(EnemyInstanceEntityData data,
            Formats::Culling::NodeRef nodeRef, Scene* scene);

        Enemy01Entity(const Enemy01Entity&) = delete;
        Enemy01Entity& operator=(const Enemy01Entity&) = delete;
        Enemy01Entity(Enemy01Entity&&) = delete;
        Enemy01Entity& operator=(Enemy01Entity&&) = delete;

    protected:
        void EnemyInitialize() override;
        void EnemyProcess() override;

    private:
        EnemySpawnEntity* const _spawner;
        OpenTK::Mathematics::Vector3 _direction{};
        float _angleInc = 0.0F;
        float _curAngle = 0.0F;
        float _maxAngle = 0.0F;
        float _angleCos = 0.0F;
        OpenTK::Mathematics::Vector3 _intendedDir{};
        OpenTK::Mathematics::Vector3 _field1A0{};
        OpenTK::Mathematics::Vector3 _field1AC{};
        std::int32_t _volumeCheckDelay = 0;
        bool _seekingVolume = false;
        CollisionVolume _homeVolume{};
    };
}
