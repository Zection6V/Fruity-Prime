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
    class Enemy05Entity : public EnemyInstanceEntity
    {
    public:
        Enemy05Entity(EnemyInstanceEntityData data,
            Formats::Culling::NodeRef nodeRef, Scene* scene);

        Enemy05Entity(const Enemy05Entity&) = delete;
        Enemy05Entity& operator=(const Enemy05Entity&) = delete;
        Enemy05Entity(Enemy05Entity&&) = delete;
        Enemy05Entity& operator=(Enemy05Entity&&) = delete;

        [[nodiscard]] static bool Behavior00(Enemy05Entity* enemy);
        [[nodiscard]] static bool Behavior01(Enemy05Entity* enemy);

    protected:
        void EnemyInitialize() override;
        void EnemyProcess() override;

    private:
        EnemySpawnEntity* const _spawner;

        OpenTK::Mathematics::Vector3 _initialPos{};
        OpenTK::Mathematics::Vector3 _field184{};
        OpenTK::Mathematics::Vector3 _field190{};
        float _weaveOffset = 0.0F;
        float _field1B0 = 0.0F;
        float _bobAngle = 0.0F;
        float _bobOffset = 0.0F;
        float _bobSpeed = 0.0F;
        std::uint16_t _field170 = 0;
        float _targetY = 0.0F;
        std::uint16_t _field1A0 = 0;

        void UpdateState();
        void UpdateMovement();

        void State0();
        void State1();

        [[nodiscard]] bool Behavior00();
        [[nodiscard]] bool Behavior01();
    };
}
