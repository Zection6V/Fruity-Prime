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
    class Enemy06Entity : public EnemyInstanceEntity
    {
    public:
        Enemy06Entity(EnemyInstanceEntityData data,
            Formats::Culling::NodeRef nodeRef, Scene* scene);

        Enemy06Entity(const Enemy06Entity&) = delete;
        Enemy06Entity& operator=(const Enemy06Entity&) = delete;
        Enemy06Entity(Enemy06Entity&&) = delete;
        Enemy06Entity& operator=(Enemy06Entity&&) = delete;

        [[nodiscard]] static bool Behavior00(Enemy06Entity* enemy);
        [[nodiscard]] static bool Behavior01(Enemy06Entity* enemy);
        [[nodiscard]] static bool Behavior02(Enemy06Entity* enemy);
        [[nodiscard]] static bool Behavior03(Enemy06Entity* enemy);

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
        std::uint16_t _field172 = 0;
        float _targetY = 0.0F;
        std::uint16_t _field1A0 = 0;

        void UpdateState();
        void UpdateMovement();

        void State0();
        void State1();
        void State2();
        void State3();
        void State4();

        [[nodiscard]] bool Behavior00();
        [[nodiscard]] bool Behavior01();
        [[nodiscard]] bool Behavior02();
        [[nodiscard]] bool Behavior03();
    };
}
