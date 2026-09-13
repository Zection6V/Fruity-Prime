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
    class Enemy04Entity : public EnemyInstanceEntity
    {
    public:
        Enemy04Entity(EnemyInstanceEntityData data,
            Formats::Culling::NodeRef nodeRef, Scene* scene);

        Enemy04Entity(const Enemy04Entity&) = delete;
        Enemy04Entity& operator=(const Enemy04Entity&) = delete;
        Enemy04Entity(Enemy04Entity&&) = delete;
        Enemy04Entity& operator=(Enemy04Entity&&) = delete;

        [[nodiscard]] static bool Behavior00(Enemy04Entity* enemy);
        [[nodiscard]] static bool Behavior01(Enemy04Entity* enemy);

    protected:
        void EnemyInitialize() override;
        void EnemyProcess() override;

    private:
        EnemySpawnEntity* const _spawner;
        OpenTK::Mathematics::Vector3 _initialPos{};
        OpenTK::Mathematics::Vector3 _field188{};
        OpenTK::Mathematics::Vector3 _field194{};
        float _weaveOffset = 0.0F;
        float _bobAngle = 0.0F;
        float _bobOffset = 0.0F;
        float _bobSpeed = 0.0F;
        float _weaveAngle = 0.0F;
        std::uint16_t _field170 = 0;

        void UpdateState();
        void UpdateMovement();

        void State0();
        void State1();

        [[nodiscard]] bool Behavior00();
        [[nodiscard]] bool Behavior01();
    };
}
